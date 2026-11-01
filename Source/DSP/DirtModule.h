#pragma once
// SWEAX NEXT — DSP/DirtModule.h
// "KİR" (M3.4): sanatçı kimliği için PARALEL kir veri yolu — lead'e EKLENİR, lead'i bozmaz.
// Diskografi araştırması: trap metal (Killoki) "paralel scream bus: overdrive, 300 Hz/5 kHz bant, lead'in
// −12…−8 dB altında"; lo-fi melodik (Ati242) "300 Hz–3 kHz telefon / hafif bitcrush paralel katman −12 dB".
//  • Modlar: 0 SCREAM — HP 300 → 2× OS asimetrik tanh overdrive → LP ton (boğazdan, yağlı grit)
//            1 FUZZ   — HP 150 → 2× OS asimetrik sert kırpma → LP ton (vızıltılı, duvara çakılmış)
//            2 LOFI   — HP 250 → bitcrush (bit) + sample-hold (oran, 48k'ya normalize) → LP ton (hışırtı)
//  • SÜRÜŞ SEVİYEDEN BAĞIMSIZ: bant sürüşten önce −18 dBFS RMS'e normalize edilir. Ön kazanç zarfı hızlı atak
//    (5 ms) / yavaş bırakma (150 ms) ve YALNIZ sesli bloklarda güncellenir (denetim: sessizlikte +36 dB tavanda
//    donup her cümle girişinde 20-28 dB fazla sürüyordu).
//  • SEVİYE: statik kalibrasyon (mod×sürüş için −18 dBFS referans sinüsün şekillendirici RMS'i) + yavaş otomatik
//    kalıntı eşlemesi (referans bandı veri yoluyla AYNI LP'den geçer) + anlık +6 dB tavan (denetim: cümle sonrası
//    150 ms zarf belleği satüre oda gürültüsünü 0.2-0.8 s şişiriyordu). Reset/prepare sonrası kazanç 0'dan açılır
//    ve zarf ısınma bias'ı düzeltilir (denetim: reset anında 150 ms'lik +19 dB patlama).
//  • Mod/sürüş değişimi 25 ms eski↔yeni şekillendirici karışımıyla tıksız (denetim: anında sıçrama tık yapıyordu).
//  • dirtLevel −36 dB = KAPALI (veri yolu tam susar; YİNE DE AÇ gerçekten nötr).
//  • Asimetrik şekillendirici DC üretir → aşağı örneklemeden sonra 35 Hz 2. derece HP (denetim: Killoki çıkışında
//    cümle zarfını izleyen DC basamağı, <20 Hz +12 dB). Zarf/tavan kontrolü host bloğundan BAĞIMSIZ sabit 64 örnek
//    ızgarasında (denetim: 1-17 örnek bloklarda kir yolu 0.5-1.4 dB kısık kalıyordu).
//  • Latency sabit 12 örnek (yarım-bant) — mod/parametreden bağımsız. Kuru yol hizalı; idleWet tüm hatları besler.
#include "Module.h"
#include "HalfBand.h"

namespace sweax
{

struct DirtParams
{
    bool  on { false };
    int   mode { 0 };               // 0 Scream, 1 Fuzz, 2 Lofi
    float driveDb { 18.0f };        // 0..36
    float levelDb { -12.0f };       // −36 (kapalı) .. 0 (bant eşlemesi sonrası)
    float toneHz { 5000.0f };       // 2000..12000 LP
    float bits { 10.0f };           // 4..16 (Lofi)
    float rate { 2.0f };            // 1..8 sample-hold (Lofi)
};

class DirtModule : public ModuleBase
{
public:
    static constexpr int LATENCY = HalfBand25::DELAY;
    static constexpr float SEVIYE_KAPALI_DB = -36.0f;

    void prepare (double sampleRate, int maxBlock, int numChannels);
    int latencySamples (Quality) const noexcept { return LATENCY; }
    void setQuality (Quality) noexcept {}
    void setParams (const DirtParams& p) noexcept { pending = p; paramsDirty = true; }

    float lastAutoGainDb() const noexcept { return sonOtoDb; }

    // şekillendirici (test ve kalibrasyon için açık): mod 0/1 sürülen, 2 kimlik
    static float sekil (int mode, float drive, float s) noexcept;

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    void updateParams (bool anlik) noexcept;   // anlik: çıkış yazılmıyor/ilk kurulum → seviye rampasız
    void isle (float* const* ch, int numCh, int n, bool yaz) noexcept;
    void kontrolGuncelle (int c) noexcept;
    static float kalibrasyon (int mode, float drive) noexcept;

    DirtParams pending, cur;
    bool paramsDirty { true }, ilkParam { true };
    std::array<HalfBand25, MAX_CH> up, dn;
    std::array<DelayLine, MAX_CH> dryAlign;
    std::array<Biquad, MAX_CH> busHp, busLp, refLp, dcHp;
    std::array<float, MAX_CH> tut {};
    std::array<int, MAX_CH> tutSay {};
    float drive { 1.0f }, kalib { 1.0f };
    // tıksız mod/sürüş geçişi
    int eskiMod { 0 };
    float eskiDrive { 1.0f }, eskiKalib { 1.0f };
    int gecisKalan { 0 }, gecisN { 1200 };
    // zarflar
    float envIn { 0.0f }, envOut { 0.0f }, envOn { 0.0f }, envCoef { 0.0f };
    float isinma { 1.0f };
    float sonOtoDb { 0.0f };
    // sabit kontrol ızgarası (çağrılar arası taşınır)
    float izgaraIn { 0.0f }, izgaraOut { 0.0f }, izgaraG0 { 0.0f };
    int izgaraSay { 0 }, izgaraKanal { 1 };
    LinearSmoother otoKazanc, seviye, onKazanc;
};

} // namespace sweax
