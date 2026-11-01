#pragma once
// SWEAX NEXT — DSP/DeEsser.h
// De-esser (mimari §4.1): split-band (Linkwitz-Riley 4. derece) veya geniş bant.
//  • Dedektör: bant seviyesi (RMS 2 ms) + bant/geniş-bant oranı + GÜRÜLTÜLÜLÜK KAPISI: bant sinyalinin normalize
//    otokorelasyon tepesi (gecikmeler fs/12k … fs/2k, 8 ms pencere) > 0.3 ise HARMONİK (parlak 'i') → GR yok.
//    Sibilans gürültü benzeridir (tepe < 0.3). Eşik = bant seviyesi mutlak dB (planlayıcı sesli bant istatistiğinden koyar).
//  • Kazanç: over = bant − eşik, soft knee 3 dB, range ile sınırlı (maks 12; planlayıcı 8'i geçmez, kanıt ≥ 0.8 hariç).
//    Attack/release dB alanında (%63). Lookahead: LATENCY SABİT = f(mod): CANLI 0, YÜKSEK/ULTRA 3 ms; parametre
//    (0–3 ms) dedektör önde okuma miktarıdır (kazanç gecikme hattı D − L).
//  • Split: GR yalnız üst banda; alt bant + üst bant toplamı LR4 ile düz. Wide: GR tüm sinyale.
//  • Stereo link: mono toplam dedektör (link) / kanal başına (link kapalı).
//  • GÖRELİ MOD (M3.4 denetim T3 — zincir sonu ikinci aşama): over = (s bandı 2 ms) − GÖVDE − göreli eşik; GÖVDE =
//    300 Hz-3 kHz gücünün dB tepe izleyicisi (atak 20 ms / bırakma 1.5 s) ≈ vokalin o anki "genel yüksekliği".
//    Kompresyon/GLOW/limiter gövdeyi bastırırken 5 ms s tepeleri ataktan geçiyordu → mutlak eşik bunu izleyemez;
//    yerel 150 ms zarf da (ilk deneme) s anındaki sönmüş gövdeyi ölçüp algıyı kaçırıyordu (korpusla ölçüldü).
//    Korpus: s olayları bu ölçüde −4…−1 dB, sesli kareler ≈ −35 (p95 −10…−20).
#include "Module.h"
#include "Svf.h"
#include <array>
#include <vector>

namespace sweax
{

struct DeEssParams
{
    bool  on { false };
    int   mode { 0 };                // 0 Split, 1 Wide
    float hz { 6500.0f }, threshDb { -24.0f }, rangeDb { 0.0f };
    float lookaheadMs { 1.0f }, attackMs { 1.0f }, releaseMs { 60.0f };
    bool  link { true };
    bool  goreli { false };          // true: eşik gövdeye GÖRELİ (goreliEsikDb), mutlak threshDb kullanılmaz
    float goreliEsikDb { -4.0f };
};

class DeEsser : public ModuleBase
{
public:
    static constexpr double LOOKAHEAD_MAX_MS = 3.0;

    void prepare (double sampleRate, int maxBlock, int numChannels);
    int latencySamples (Quality mode) const noexcept;
    void setQuality (Quality mode) noexcept;
    float reportedGainDb() const noexcept { return 0.0f; }
    float gainReductionDb() const noexcept { return grOut; }
    bool  harmonicGateActive() const noexcept { return gateHarmonic; }
    float autocorrPeak() const noexcept { return acPeak; }

    void setParams (const DeEssParams& p) noexcept { pending = p; paramsDirty = true; }

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    struct Detector
    {
        float bandEnv { 0.0f }, wideEnv { 0.0f }, grDb { 0.0f };
    };
    void updateCoeffs() noexcept;
    float computeGr (Detector& d, float band, float wide, float govde) noexcept;

    DeEssParams pending, cur;
    bool paramsDirty { false };
    int activeDelay { 0 };
    // crossover (LR4 = 2× Butterworth 2. derece), kanal başına
    std::array<std::array<Biquad, 2>, MAX_CH> lp, hp;
    std::array<Svf, MAX_CH> detBand;                  // wide mod dedektör bandı (ve oran için)
    std::array<DelayLine, MAX_CH> lowDelay, wideDelay; // ses yolu gecikmesi (split: alt+üst; wide: tüm)
    std::array<DelayLine, MAX_CH> highDelay;
    std::array<DelayLine, MAX_CH> gainDelay;
    std::array<Detector, MAX_CH> det;
    float envCoef { 0.0f }, wideCoef { 0.0f }, atkCoef { 0.0f }, relCoef { 0.0f };
    float lastHz { -1.0f };
    // HARMONİKLİK KAPISI (ölçülerek düzeltildi): bant sinyalinin kendi otokorelasyonu bant-sınırlı GÜRÜLTÜDE de yüksektir
    // (merkez frekans gecikmesinde r≈0.5) → yanlış kapı. Doğru ipucu: bant ZARFININ F0 periyodunda periyodikliği —
    // harmonik demet (parlak ünlü, /z/) F0'da genlik modülasyonludur, sibilant gürültü değildir.
    // ÖRNEKLEME HIZINDAN BAĞIMSIZ (inceleme: sabit 8× desimasyonla maliyet ~sr² büyüyor, 192 kHz'de 64'lük blok
    // bütçesinin %265'i tek patlamada): zarf ≈6 kHz'e desimasyon (dec = sr/6000), halka 32 ms (≈192), gecikmeler
    // F0 70–1000 Hz (6…86, adım 2), değerlendirme her 5 ms'de bir; iç döngüler modulosuz (doğrusallaştırılmış kopya).
    // Histerezis 0.25/0.35 + 2 ardışık karar.
    float envLp { 0.0f }, envLpCoef { 0.0f }; int decCounter { 0 }, decim { 8 };
    static constexpr int ENV_MAX = 512;
    std::array<float, ENV_MAX> envRing {}; int envPos { 0 }, envLen { 192 }, acCounter { 0 }, acInterval { 240 }, lagLo { 6 }, lagHi { 86 };
    float acPeak { 0.0f }; bool gateHarmonic { false }; int gateOn { 0 }, gateOff { 0 };
    float grOut { 0.0f };
    // göreli mod gövde dedektörü: link açıkken [0] mono ortalamadan; link kapalıyken kanal başına (denetim dsp-rt-4:
    // tek kanallı stereo kayıtta mono ortalama gövdeyi 6 dB düşük gösterip fazladan kısıyordu)
    std::array<Biquad, MAX_CH> govdeHp, govdeLp;
    std::array<float, MAX_CH> govdeEnv {}, govdeTepe {}, govdeDb {};
    float govdeCoef { 0.0f }, govdeAtk { 0.0f }, govdeRel { 0.0f };
    inline void govdeIsle (int k, float x) noexcept
    {
        if (! (x > -1.0e6f && x < 1.0e6f)) x = 0.0f;
        const auto kk = (size_t) k;
        const float b = govdeLp[kk].processSample (govdeHp[kk].processSample (x));
        govdeEnv[kk] = b * b + govdeCoef * (govdeEnv[kk] - b * b);
        if (! std::isfinite (govdeEnv[kk])) govdeEnv[kk] = 0.0f;
        const float db = std::max (-70.0f, 10.0f * std::log10 (govdeEnv[kk] + 1.0e-12f));
        const float c = db > govdeTepe[kk] ? govdeAtk : govdeRel;
        govdeTepe[kk] = db + c * (govdeTepe[kk] - db);
        if (! std::isfinite (govdeTepe[kk])) govdeTepe[kk] = -70.0f;
        govdeDb[kk] = govdeTepe[kk];
    }
    void govdeSanitize() noexcept { for (int k = 0; k < MAX_CH; ++k) { govdeHp[(size_t) k].sanitize(); govdeLp[(size_t) k].sanitize(); } }
};

} // namespace sweax
