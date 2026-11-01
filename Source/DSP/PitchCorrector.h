#pragma once
// SWEAX NEXT — DSP/PitchCorrector.h
// Perde düzeltme (M2 "TUNE"): gerçek zamanlı YIN takibi + çift-tap testere yeniden örnekleme (zaman alanı →
// formantlar doğal korunur; küçük düzeltmelerde şeffaf, hızlı ayarda "hard-tune" karakteri).
//  • Takip (M2.2 denetim düzeltmesi): YIN, ~16 kHz'e DESİMASYONLU kopya üzerinde koşar → maliyet örnekleme
//    hızından BAĞIMSIZ (192 kHz'de W·maxLag patlaması ölçüldü: ~20M MAC/çağrı → desimasyonla ~140k). Boxcar-k
//    desimasyon; rap F0 < 500 Hz için bol bant. Analiz her ~16 ms'de bir.
//  • Hedef: kromatik veya seçili gam (majör/minör + kök) en yakın nota; strength hedefe gidilen ORANI ölçekler
//    (kuru/ıslak karışım DEĞİL — zaman kayması yüzünden karışım comb yapar; endüstri de böyle yapar).
//  • Hız (speedMs): cent alanında tek kutup — 1–5 ms sert kilit (T-Pain), 20–40 ms doğal.
//  • Kaydırıcı: faz testeresi + üçgen crossfade'li iki tap; pencere N·T (N ÇİFT — ölçüldü: kesirli pencere ek-yeri
//    faz kayması, tek N temel sönümü yapar).
//  • NÖTR GEÇİŞ (M2.2 denetim düzeltmesi): fazı sürmek yerine 15 ms'lik karışım rampası iki-tap ↔ tek-tap(latN);
//    tam nötrde faz 0.5'e oturur → düzeltme bırakışında cıvıltı/kayma YOK, nötr durumda comb YOK.
//  • Sessiz/tonal olmayan (rap konuşması): oran 1'e yumuşar → şeffaf. Stereo: aynı yörünge iki kanala.
//  • Latency: sabit = base + W_nom/2 (sr ölçekli, mod bağımsız); enable/parametre değiştiremez.
#include "Module.h"
#include "../Analysis/Pitch.h"

namespace sweax
{

struct TuneParams
{
    bool  on { false };
    float strength { 0.8f };         // 0..1
    float speedMs { 20.0f };         // 1..400
    int   key { 0 };                 // 0=C … 11=B
    int   scale { 0 };               // 0 kromatik, 1 majör, 2 minör
};

class PitchCorrector : public ModuleBase
{
public:
    void prepare (double sampleRate, int maxBlock, int numChannels);
    int latencySamples (Quality) const noexcept { return latN; }
    void setQuality (Quality) noexcept {}
    void setParams (const TuneParams& p) noexcept { pending = p; paramsDirty = true; }
    float currentOffsetCents() const noexcept { return offsetCents; }   // ölçer/teşhis

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    void updateParams() noexcept;
    inline void pushSample (float l, float r) noexcept
    {
        ringL[(size_t) wp] = l; ringR[(size_t) wp] = r;
        wp = (wp + 1) % ringN;
        // desimasyon biriktirici (analiz kopyası)
        anaAcc += 0.5f * (l + r);
        if (++anaAccN >= anaK)
        {
            anaRing[(size_t) anaWp] = anaAcc / (float) anaK;
            anaWp = (anaWp + 1) % anaRingN;
            anaAcc = 0.0f; anaAccN = 0;
            if (++sinceAnalyze >= anaHop) { sinceAnalyze = 0; analyzePitch(); }
        }
    }
    float readTap (const std::vector<float>& ring, double d) const noexcept
    {
        double rp = (double) wp - 1.0 - d; while (rp < 0.0) rp += (double) ringN;
        const int i0 = (int) rp; const double fr = rp - i0; const int i1 = (i0 + 1) % ringN;
        return (float) ((1.0 - fr) * ring[(size_t) i0] + fr * ring[(size_t) i1]);
    }
    void analyzePitch() noexcept;

    TuneParams pending, cur;
    bool paramsDirty { true };

    // ses halkaları (stereo görüntü korunur; yörünge ortak)
    std::vector<float> ringL, ringR;
    int ringN { 1 }, wp { 0 };
    int baseN { 48 }, wNomN { 1440 }, latN { 768 };

    // analiz (desimasyonlu)
    PitchTracker tracker;
    std::vector<float> anaRing, scratch;
    int anaRingN { 1 }, anaWp { 0 };
    int anaK { 3 };                  // desimasyon çarpanı (sr/16k)
    double anaSr { 16000.0 };
    float anaAcc { 0.0f };
    int anaAccN { 0 }, sinceAnalyze { 0 }, anaHop { 256 };
    float f0 { 0.0f };
    bool voiced { false };

    // düzeltme durumu
    float offsetCents { 0.0f };
    float targetCents { 0.0f };
    float atkCoef { 0.0f }, relCoef { 0.0f };
    double phase { 0.5 };            // 0..1 testere
    float wEff { 1440.0f };
    float nBlend { 1.0f };           // 1 = tam nötr (tek tap latN), 0 = iki-tap aktif
    float nStep { 0.001f };          // 15 ms rampa
};

} // namespace sweax
