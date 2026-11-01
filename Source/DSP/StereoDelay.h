#pragma once
// SWEAX NEXT — DSP/StereoDelay.h
// Vokal delay (M2): slap veya tempo-senkron ping-pong; bant sınırlı geri besleme; vokal varken DUCKING.
//  • Yapı: mono giriş → L hattı; L→R, R→L çapraz geri besleme (ping-pong). HP/LP döngü İÇİNDE (her tekrar koyulaşır).
//  • Senkron: Slap = ms parametresi; 1/8, 1/8., 1/4 host BPM'inden (chain her blok BPM iletir; host yoksa 120).
//  • Ducking: kuru vokal zarfı (5 ms atak / 250 ms bırakma) −50…−30 dB aralığında 0→1; wet −duck·oran dB kısılır →
//    vokal susunca yankı çiçeklenir (klasik vokal delay otomasyonu, elle yazmaya gerek yok).
//  • Kuru yol dokunulmaz; latency 0. idleWet kuyrukları SIFIRLA besler (yeniden açılışta hayalet yankı olmaz).
// M3.5 FX KARAKTER:
//  • sync 4 = 1/16 (60/bpm/4) SONA eklendi — 0..3 anlamı değişmez. Araştırma: CHORBA 1/8–1/16 feedback'li delay.
//  • width (1 = M2 ping-pong, BİT BİT aynı; 0 = yankılar ORTADA mono, L==R; arası karışım). Araştırma: Motive
//    WAVES ping-pong (width 1) ↔ Killoki kelime sonu tekrarları ortada (width 0). Döngü (çapraz geri besleme)
//    DEĞİŞMEZ, yalnız ÇIKIŞ toplanır. width 20 ms rampalı.
//    MONO ÇIKIŞ SINIRI (M3.5 denetimi, bulgu 8a — BİLİNEN ve KABUL EDİLEN): tek kanallı çıkışta width kolu
//    etkisizdir. Bu tasarım gereğidir: width bir STEREO GÖRÜNTÜ kontrolüdür, seviye kontrolü değil; mono
//    çıkışta görüntü yoktur ve M2'deki gibi lastL + lastR toplanır → eko seviyesi width'ten bağımsız, her
//    width değerinde AYNI kalır (width 0'ın stereo'daki mono toplamıyla enerjice eşdeğer). Test bağlar.
//  • ENERJİ TELAFİSİ — sabit 0.7071 pan yasası YETMİYOR; denetimde İKİ ayrı kusur ölçüldü (48k, 1/16 @120 bpm,
//    mix 1, ıslak yol izole = çıkış − giriş):
//      (1) ara değerlerde ÇUKUR: ilk yankı enerjisi w 0.75 → −0.51 dB, w 0.50 → −0.69 dB, w 0.25 → −0.51 dB.
//          Analitik: E(w)/E(1) = (1 − 0.2929·t)² + 0.5·t², t = 1−w → en düşük t = 0.5'te 0.8536 = −0.688 dB.
//          0.7071 sabit güç YALNIZ uçlarda (t = 0 ve t = 1) doğru; arası doğrusal karışım olduğu için çöküyordu.
//      (2) çapraz geri besleme SÜRDÜRÜLEN tonda L ile R'yi KORELE ediyor → ortada toplama enerjiyi YÜKSELTİYOR:
//          800 Hz'de fb 0.28 (VARSAYILAN) +1.78 dB, fb 0.60 +2.71 dB, fb 0.85 +2.95 dB; 777 Hz'de +1.56/+2.40/
//          +2.62 dB. (Geniş bant gürültüde ≤0.08 dB — rap/konuşmada görünmüyordu, uzatılan hecede duyuluyordu.)
//    Çözüm: KOŞAN ENERJİ ORANI (tek kutuplu, 50 ms) — genişlik çıkışı, ping-pong referansının (lastL² + lastR²)
//    enerjisine normalize edilir: g = √(E_ref / E_çıkış), [0.5, 2] klempli. Korelasyonu ayrıca ölçmeye gerek yok,
//    oran onu zaten içeriyor: ilişkisiz yankıda g = 1 (0.7071 zaten doğru), tam korele tonda g → 0.7071 (+3 dB'yi
//    siler), w = 0.5'te g → 1.082 (çukuru siler). Tahsissiz, örnek-özyineli → blok boyutundan bağımsız; w ≥ 1
//    dalına HİÇ girilmez (M2 birebir korunur).
#include "Module.h"

namespace sweax
{

struct DelayParams
{
    bool  on { false };
    float mix { 0.16f };
    int   sync { 0 };                // 0 Slap(ms), 1 1/8, 2 1/8., 3 1/4, 4 1/16
    float timeMs { 110.0f };
    float feedback { 0.28f };        // 0..0.85
    float hpHz { 250.0f }, lpHz { 6500.0f };
    float duckDb { 8.0f };
    double bpm { 120.0 };
    float width { 1.0f };            // 0 ortada mono … 1 ping-pong (M2)
};

class StereoDelay : public ModuleBase
{
public:
    static constexpr double MAX_DELAY_MS = 1500.0;

    void prepare (double sampleRate, int maxBlock, int numChannels);
    int latencySamples (Quality) const noexcept { return 0; }
    void setQuality (Quality) noexcept {}
    void setParams (const DelayParams& p) noexcept { pending = p; paramsDirty = true; }

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    void updateParams() noexcept;

    DelayParams pending, cur;
    bool paramsDirty { true };
    DelayLine lineL, lineR;
    Biquad hpL, lpL, hpR, lpR;
    float fb { 0.28f };
    // ducking
    float duckEnv { 0.0f }, duckRise { 0.0f }, duckFall { 0.0f };
    float duckAmt { 8.0f };
    float lastL { 0.0f }, lastR { 0.0f };
    LinearSmoother mixS;
    LinearSmoother widthS;            // 20 ms; ilk kurulumda rampasız
    bool ilkParam { true };
    // genişlik enerji telafisi: koşan enerjiler (50 ms tek kutup) — yalnız w < 1 dalında
    float enerjiRef { 0.0f }, enerjiCik { 0.0f }, enerjiA { 0.0f };
    // süre değişimi (denetim düzeltmesi): rampa yerine 10 ms sön → ZIPLA → 10 ms aç (tam-sayı basamak çıtırtısı yok)
    int curDelayN { -1 }, tgtDelayN { 0 };
    float jFade { 1.0f }, jStep { 0.005f };
    int jState { 0 };                 // 0 kararlı, 1 sönüyor, 2 açılıyor
};

} // namespace sweax
