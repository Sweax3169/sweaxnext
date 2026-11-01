#pragma once
// SWEAX NEXT — DSP/Saturator.h
// Karakter/doygunluk modülü (M2 §4): rap vokal "önde ve dolu" hissi için tüp-tarzı yumuşak doygunluk.
//  • Eğri (M2.2 denetim düzeltmesi): y = (tanh(d·x + b) − tanh(b)) / (d·sech²(b)), b = 0.2 — tek + çift harmonik;
//    HER YERDE MONOTON (ölçüldü: eski β·x² biçimi büyük negatif tepede polariteyi katlıyordu); küçük-sinyal kazancı 1.
//  • 2× oversample (25-tap yarım-bant Kaiser-sinc, OutputStage ile aynı tasarım).
//  • Çift harmonik asimetrisinin DC'si çıkışta tek kutuplu DC engelleyiciyle (≈10 Hz) alınır.
//  • Tone: RBJ high-shelf @ 1.8 kHz ±6 dB (basit tını eğimi).
//  • Mix: paralel doygunluk (NY tarzı) — kuru kopya OS gecikmesiyle hizalanır, faz-tutarlı.
//  • Latency: 12 örnek (2×12 tap gecikme @2× hız), tüm modlarda sabit = f(sr, mod) sözleşmesine uygun.
// ALIAS BÜTÇESİ (M3.5 denetimi, bulgu 5 — ölçüldü, 1 s sinüs, −6 dBFS, harmonik-DIŞI en kötü bileşen, dBc):
//   2× OS her karakterde AYNI (latency = f(sr, mod) sözleşmesi karaktere göre gecikme değiştirmeye izin vermez);
//   üretilen harmonik derecesi karaktere göre değişir → alias tabanı da değişir. Ölçüt ve kilit test
//   (fxkarakter_alias_butcesi): 1 kHz, drive {3, 6, 9, 12}, her karakter → tüm alias bileşenleri ≤ −60 dBc.
//   Ölçülen en kötüler @1 kHz: TÜP −122.5 | BANT −142.5 | KIRIK −90.0 dBc → ölçüt ≥ 30 dB payla sağlanıyor.
//   Yüksek temel frekansta pay düşer (dürüst tablo, TÜM bant en kötü):
//     44.1k / 3 kHz: drive 6 → TÜP −90.6 | BANT −104.5 | KIRIK −70.2;  drive 12 → −58.3 | −73.1 | −66.1
//     48k / 5 kHz DUYULUR BANT (≤16 k): drive 6 → −134.7 | −147.5 | −79.6;  drive 12 → −88.5 | −100.5 | −69.3
//   En kötü bileşenler 17–21 kHz'de (25-tap yarım-bandın geçiş bandı — M2'den beri var); 3 kHz+drive 12 sınır
//   durumunda taban artık TÜP'ün kendisi (−58.3, M2 birebir yolu), Kırık değil.
// KARAKTER (M3.5 FX KARAKTER) — 2× OS, DC engelleyici, tone, mix ve latency HER karakterde aynı:
//  • 0 TÜP (varsayılan): yukarıdaki eğri + SERT MOD (drive > 8 dB) — M2/M3.3 çıkışı BİT BİT korunur (FNV-1a).
//  • 1 BANT (tape): SİMETRİK tanh(d·x)/d → yalnız tek harmonikler (H2 yapısal olarak yok, H3 baskın).
//    Ön vurgu / geri vurgu: 2× hızda RBJ high-shelf 3 kHz +3 dB → eğri → aynı shelf −3 dB (RBJ shelf'te A↔1/A
//    birebir ters filtre) → küçük sinyalde düz, büyük sinyalde tiz ÖNCE doyar (bant kaydı gibi hafif HF sıkıştırma).
//  • 2 KIRIK (crunch — Decapitator 'Punish' / Devil-Loc tarzı): +3 dB iç sürüş, ASİMETRİK kırpma
//    y = u / (1 + (u/c)²)^½, c+ = 0.5, c− = 0.9 → çift + tek harmonik belirgin. f'(0) = 1 (küçük sinyal birim),
//    türev (1 + (u/c)²)^(−3/2) > 0 → HER YERDE MONOTON; double hassasiyette (tavanda float yuvarlaması 1 ulp'lik
//    geri adım yapabiliyordu). SERT MOD yalnız Tüp'e aittir; Bant/Kırık 8 dB üstünde de aynı eğri.
//    DİZ ÜSSÜ 4 → 2 (M3.5 denetimi, bulgu 5): ⁴√ biçimi neredeyse SERT bir diz yapıyordu, harmonik serisi çok
//    yavaş sönüyor ve 2× OS örtmüyordu (44.1k/3 kHz drive 12'de −45.4 dBc ölçülmüştü). Yumuşatılmış dizle +
//    seviye eşitleme (aşağıda) sonrası aynı koşul −66.1 dBc; 1 kHz ölçütü tabloda. Crunch imzası korunur:
//    200 Hz −12 dBFS drive 6'da THD %7.3 (Tüp %4.8), H2 −26.0 dBc, H3 −25.5 dBc.
//  • SEVİYE EŞİTLEME (M3.5 denetimi, bulgu 6): −18 dBFS 1 kHz referansta karakterler Tüp'e ±0.5 dB eşit
//    (test bağlar; ölçülen: Bant −0.0…+0.2 dB — telafisiz zaten içeride; Kırık telafisiz −1.9 dB'ye açılıyordu,
//    k·f(x/k) telafisiyle −0.2…−0.4 dB). Telafi egriHesapla'da sabit nokta yinelemesiyle çözülür; küçük-sinyal
//    kazancı tam 1 kalır (yalnız tavan yükselir), monotonluk değişmez, Tüp'e dokunulmaz.
//  • Karakter değişimi 20 ms şekillendirici karışımıyla tıksız (DirtModule geçiş fikri); geçiş 2× OS İÇİNDE,
//    aynı yarım-bant filtrelerden. Bant'a geçişte vurgu filtreleri sıfırdan (ağırlığı 0'dan açılır).
//    ÜÇ ŞEKİLLENDİRİCİ ÜZERİNDE AĞIRLIK VEKTÖRÜ (M3.5 denetimi): her değişimde rampa O ANKİ ağırlıklardan
//    hedefe gider. Eskiden tek bir (eski, yeni) çifti + sayaç vardı ve 20 ms dolmadan gelen İKİNCİ değişim
//    ağırlığı 0'a döndürüp yarım kalan karışımı ATIYORDU → tek örnekte basamak. Ölçüldü (220 Hz 0.40, drive
//    6 dB, blok 32/64): 0→2 sonra +64 örnekte 2→1 maks adım 0.08726, kararlı 0.01181 (×7.39); düzeltme
//    sonrası ×1.00. Kapalıyken / prepare veya reset sonrası İLK örneğe kadar karakter anında uygulanır.
#include "Module.h"
#include "HalfBand.h"

namespace sweax
{

struct SatParams
{
    bool  on { false };
    float driveDb { 3.0f };          // 0..12
    float toneDb { 0.0f };           // −6..+6 (1.8 kHz shelf)
    float mix { 1.0f };              // 0..1
    int   character { 0 };           // 0 Tüp, 1 Bant (tape), 2 Kırık (crunch)
};

class Saturator : public ModuleBase
{
public:
    static constexpr int HB_LEN = HalfBand25::LEN, HB_DELAY = HalfBand25::DELAY;
    static constexpr int LATENCY = HB_DELAY;             // 24 @2× = 12 taban örneği
    static constexpr int KARAKTER_SAYISI = 3;
    static constexpr double GECIS_MS = 20.0;

    void prepare (double sampleRate, int maxBlock, int numChannels);
    int latencySamples (Quality) const noexcept { return LATENCY; }
    void setQuality (Quality) noexcept {}
    void setParams (const SatParams& p) noexcept { pending = p; paramsDirty = true; }

    // statik transfer eğrisi (test/UI): RT yolundaki eğrinin AYNISI (Bant'ın doğrusal vurgu filtreleri hariç)
    static float transfer (int character, float driveDb, float x) noexcept;
    int activeCharacter() const noexcept { return karakter; }
    bool characterTransitioning() const noexcept { return gecisKalan > 0; }

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    using HalfBand = HalfBand25;                         // ortak tasarım (DSP/HalfBand.h)
    static constexpr float BIAS = 0.2f;                  // çift harmonik önyargısı (monoton — denetim düzeltmesi)
    // sürüşe bağlı eğri katsayıları (updateParams / transfer ortak)
    struct Egri
    {
        float drive { 1.0f }, invDrive { 1.0f }, biasT { 0.19738f }, invSmall { 1.0f };
        float sert { 0.0f }, sertH { 1.0f }, sertInvH { 1.0f };   // SERT MOD (M3.3): 8 dB üstü sürüşte ikinci tanh kademesi
        double kDrive { 1.0 }, kInvDrive { 1.0 };                 // Kırık iç sürüşü (+3 dB)
        double kSev { 1.0 }, kSevInv { 1.0 };                     // Kırık seviye eşitleme: y = k·f(x/k) (denetim, bulgu 6)
    };
    static Egri egriHesapla (float driveDb) noexcept;
    static float egriTup (const Egri& e, float s) noexcept;
    static float egriBant (const Egri& e, float s) noexcept;
    static float egriKirik (const Egri& e, float s) noexcept;
    float sekilTek (int kar, int k, float s) noexcept;
    void updateParams() noexcept;

    SatParams pending, cur;
    bool paramsDirty { true };
    // "durum boş" — YALNIZ prepare/reset sonrası, İLK örnek işlenene kadar true. (Denetim: eskiden bu bayrak
    // paramsDirty kapısına bağlıydı; arayan her blok setParams çağırmazsa reset'ten çok SONRA da true kalıyor ve
    // o andaki karakter değişimi 20 ms karışımı ATLIYORDU — ölçüldü: maks örnek adımı 0.01181 → 0.09593, ×8.1.)
    bool bosDurum { true };
    Egri e;
    std::array<HalfBand, MAX_CH> up, dn;
    std::array<DelayLine, MAX_CH> dryAlign;              // mix için kuru hizalama (LATENCY)
    std::array<Biquad, MAX_CH> tone;
    std::array<Biquad, MAX_CH> bantOn, bantArka;         // Bant ön/geri vurgu (2× hızda)
    std::array<float, MAX_CH> dcState {};
    float dcCoef { 0.999f };
    LinearSmoother mixS;
    // karakter geçişi: üç şekillendirici üzerinde AĞIRLIK VEKTÖRÜ (kararlıda tek bileşen 1, toplam hep 1).
    // Her değişimde rampa O ANKİ ağırlıklardan (karBas) hedefe gider → yarım kalan geçiş ATILMAZ.
    int karakter { 0 };                                  // hedef karakter
    std::array<float, (size_t) KARAKTER_SAYISI> karW { { 1.0f, 0.0f, 0.0f } };
    std::array<float, (size_t) KARAKTER_SAYISI> karBas { { 1.0f, 0.0f, 0.0f } };
    int gecisKalan { 0 }, gecisN { 960 };
};

} // namespace sweax
