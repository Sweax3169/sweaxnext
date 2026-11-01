#pragma once
// SWEAX NEXT — DSP/LayerModule.h
// "KATMAN" (M3.4): pitch-shift PARALEL katman sesleri — lead'e EKLENİR.
// Diskografi araştırması: Uzi "back vokal/katman kültürü, oktav-altı + 3'lü/5'li harmoni, −6…−12 dB";
// rage "harmoni katmanları +5/+7/−12 semiton −8/−10 dB, double'lar daha karanlık ve saturated".
//  • Ses A ve B: yarım ton aralığı (−12…+12, 0 = kapalı). Çift-tap testere kaydırıcı, sin² crossfade.
//  • PERİYODA KİLİTLİ PENCERE (ölçülen hata düzeltmesi): sabit 50 ms pencerede iki tap arası (W/2) giriş
//    periyodunun YARIM-TAMSAYI katına denk gelince taplar ters fazda birbirini İPTAL ediyordu (220 Hz → 110 Hz
//    çıkışı −200 dB; 440 Hz'de tesadüfen fazda). Çözüm PitchCorrector'daki ölçülmüş kural: W = N·T, N ÇİFT
//    (W/2 tam periyot → taplar fazda; sarma sıçraması W tam periyot → faz sürekli). F0: 16 kHz desimasyonlu YIN.
//    Tonal olmayan (ünsüz/gürültü) bölümde son pencere korunur — gürültü eş-fazlı iptal yapmaz.
//  • Her ses: HP 90 Hz (oktav-altı çamur ve plosive LF tıkı yapmasın) + LP ton + isteğe bağlı grit (tanh, birim küçük-sinyal).
//  • Pan: iki ses açıksa A sol / B sağ (genişlik), tek ses açıksa ORTADA; ses aç/kapa ve pan 15 ms yumuşak.
//  • SABİT MERKEZ GECİKME (denetim): d = C + (faz−0.5)·W — baskın tapın gecikme hızı tam 1−r; W kayması yalnız
//    sönük tapa biner (eski d = faz·W her nota geçişinde katman perdesini yarım tonlarca saptırıyordu).
//  • Girdi mono toplam (vokal monodur); kuru dokunulmaz, latency 0; idleWet halkayı + takibi besler.
#include "Module.h"
#include "../Analysis/Pitch.h"

namespace sweax
{

struct LayerParams
{
    bool  on { false };
    float semiA { -12.0f };          // −12..+12 (tamsayıya yuvarlanır; 0 = kapalı)
    float semiB { 0.0f };
    float levelDb { -10.0f };        // −36..0
    float width { 0.6f };            // 0..1
    float toneHz { 6000.0f };        // 1500..12000
    float grit { 0.0f };             // 0..1
};

class LayerModule : public ModuleBase
{
public:
    static constexpr double WINDOW_NOM_MS = 40.0;   // hedef pencere (N·T bu değere en yakın çift N)
    static constexpr double WINDOW_MAX_MS = 60.0;

    void prepare (double sampleRate, int maxBlock, int numChannels);
    int latencySamples (Quality) const noexcept { return 0; }
    void setQuality (Quality) noexcept {}
    void setParams (const LayerParams& p) noexcept { pending = p; paramsDirty = true; }

    // test erişimi
    float currentWindowSamples() const noexcept { return wEff; }
    float trackedHz() const noexcept { return f0; }

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    struct Ses
    {
        bool aktif { false };
        double faz { 0.0 }, oran { 0.0 };            // oran: gecikme artışı/örnek = 1 − r (kapanırken son oran korunur)
        LinearSmoother kazanc, panL, panR;           // denetim: aç/kapa ve pan sıçraması tık yapıyordu
        Biquad hp, lp;
    };
    void updateParams() noexcept;
    void besle (float in) noexcept;                   // halka + desimasyonlu analiz kopyası
    void analizEt() noexcept;
    float halkaOku (double gecikme) const noexcept
    {
        double rp = (double) wp - 1.0 - gecikme;
        while (rp < 0.0) rp += (double) ringLen;
        const int i0 = (int) rp;
        const double fr = rp - i0;
        const int i1 = (i0 + 1) % ringLen;
        return (float) ((1.0 - fr) * ring[(size_t) i0] + fr * ring[(size_t) i1]);
    }

    LayerParams pending, cur;
    bool paramsDirty { true };
    std::vector<float> ring;
    int ringLen { 1 }, wp { 0 };
    float wNom { 1920.0f }, wMax { 2880.0f }, wEff { 1920.0f }, wHedef { 1920.0f };
    std::array<Ses, 2> ses;
    float gritK { 0.0f }, gritInv { 1.0f };
    LinearSmoother seviye;

    // perde takibi (PitchCorrector ile aynı reçete: ~16 kHz desimasyon, ~16 ms hop)
    PitchTracker tracker;
    std::vector<float> anaRing, scratch;
    int anaRingN { 1 }, anaWp { 0 }, anaK { 3 }, anaHop { 256 };
    float anaAcc { 0.0f };
    int anaAccN { 0 }, sinceAnalyze { 0 };
    float f0 { 0.0f };
};

} // namespace sweax
