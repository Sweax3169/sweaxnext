#pragma once
// SWEAX NEXT — DSP/GlowModule.h
// "GLOW" (M3.2): OTT tarzı 3 bant YUKARI+AŞAĞI kompresyon — "anında iyi ses" katmanı.
// Araştırma raporu tabanlı (Ableton OTT preset değerleri + Vital compressor.cpp formülasyonu + Soundgoodizer
// "sabit agresif zincir, tek knob = paralel karışım" dersi):
//  • Bölme: 4. derece Linkwitz-Riley @ 180 Hz / 2.4 kHz (vokal için; OTT'nin 88 Hz'i full-mix içindir),
//    alçak dala f2 allpass'ı — üç bandın toplamı DÜZ (ölçülür: nötr koridorda ±1 dB).
//  • Her bantta çift eşik: üstünde dev oranla AŞAĞI (8/8/12:1 — high'ta ∞ YOK, ess sigortası),
//    altında ılımlı oranla YUKARI (3/4/3:1, soft knee 6 dB, kaldırma tavanı +10 dB).
//  • SESSİZLİK TABANI: bant −55 dB altında yukarı kaldırma sıfıra süzülür (−50..−55 lineer) —
//    OTT'nin bilinen "nefes/oda pompalaması" hastalığının yerleşik çaresi (zincir gate'i de öndedir).
//  • Zarf: bant başına log-alan RMS (~15 ms) + OTT A/R süreleri (45/280, 22/280, 14/130 ms) × TIME çarpanı;
//    stereo TAM LINK (kanal ortalaması tek sidechain — imaj kaymaz).
//  • DEPTH = kuru/ıslak paralel karışım (Soundgoodizer/OTT modeli; %15-30 tatlı bölge) + bant makyajları
//    (+1.5/+1.0/+2.0 dB × depth) + çıkışta kaba eş-ses telafisi (−1.5 dB × depth) — "yüksek = iyi" yanılgısı yok.
//  • Latency 0 (IIR); idleWet filtre+zarf besler; NaN örnek kapısı + zarf sigortası.
#include "Module.h"
#include "Svf.h"

namespace sweax
{

struct GlowParams
{
    bool  on { false };
    float depth { 0.18f };          // 0..1 paralel karışım
    float timeMul { 1.0f };         // 0.33..3 zarf çarpanı
};

class GlowModule : public ModuleBase
{
public:
    static constexpr float XOVER_LO = 180.0f, XOVER_HI = 2400.0f;
    static constexpr int BANT = 3;

    void prepare (double sampleRate, int maxBlock, int numChannels);
    int latencySamples (Quality) const noexcept { return 0; }
    void setQuality (Quality) noexcept {}
    void setParams (const GlowParams& p) noexcept { pending = p; }

    // test erişimi: son bant kazançları (dB)
    float lastGainDb (int bant) const noexcept { return sonKazancDb[(size_t) juce::jlimit (0, BANT - 1, bant)]; }

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    // 3 yollu LR4 bölücü (kanal başına): split1 @lo (lp/hp), split2 @hi (rest üzerinde), alçak dala AP4@hi
    struct Bolucu
    {
        Svf lo1, lo2, hi1a, hi1b;          // split1: lp kademeleri (lo1→lo2 lp), hp kademeleri (hi1a→hi1b hp)
        Svf mid1, mid2, yuk1, yuk2;        // split2 (rest): lp→mid, hp→high
        Svf ap1, ap2;                      // alçak dal faz hizası (AP2×2 = LR4 allpass @hi)
        void hazirla (double sr) noexcept
        {
            for (Svf* f : { &lo1, &lo2, &hi1a, &hi1b }) f->setCoeffs (sr, XOVER_LO, 0.7071);
            for (Svf* f : { &mid1, &mid2, &yuk1, &yuk2, &ap1, &ap2 }) f->setCoeffs (sr, std::min ((double) XOVER_HI, sr * 0.35), 0.7071);
            sifirla();
        }
        void sifirla() noexcept { for (Svf* f : { &lo1, &lo2, &hi1a, &hi1b, &mid1, &mid2, &yuk1, &yuk2, &ap1, &ap2 }) f->reset(); }
        void temizle() noexcept { for (Svf* f : { &lo1, &lo2, &hi1a, &hi1b, &mid1, &mid2, &yuk1, &yuk2, &ap1, &ap2 }) f->sanitize(); }
        inline void bol (float x, float& low, float& mid, float& high) noexcept
        {
            // LR4 = 2× Butterworth2 kaskadı; AP2(f) = x − 2k·bp (TPT)
            const float lp1 = lo1.process (x).lp;
            const float lp = lo2.process (lp1).lp;
            const float hp1 = hi1a.process (x).hp;
            const float rest = hi1b.process (hp1).hp;
            const float mlp1 = mid1.process (rest).lp;
            mid = mid2.process (mlp1).lp;
            const float mhp1 = yuk1.process (rest).hp;
            high = yuk2.process (mhp1).hp;
            const auto a1 = ap1.process (lp);
            const float apx1 = lp - 2.0f * (float) ap1.kValue() * a1.bp;
            const auto a2 = ap2.process (apx1);
            low = apx1 - 2.0f * (float) ap2.kValue() * a2.bp;
        }
    };
    void zarflariGuncelle (int numCh) noexcept;

    GlowParams pending, cur;
    std::array<Bolucu, MAX_CH> bolucu;
    // link'li bant zarfları (kare-ortalama, lineer güç) + kazanç yumuşatıcıları
    std::array<float, BANT> env {};                 // güç (kanal ortalaması)
    std::array<float, BANT> atk {}, rel {};         // katsayılar (timeMul uygulanmış)
    std::array<float, BANT> sonKazancDb {};
    std::array<LinearSmoother, BANT> kazanc;        // lineer bant kazancı (10 ms)
    float rmsCoef { 0.0f };
    float sonTimeMul { -1.0f };
    std::vector<float> bantBuf;                     // BANT × MAX_CH × maxBlock
    double srSaklanan { 48000.0 };
};

} // namespace sweax
