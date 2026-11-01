#pragma once
// SWEAX NEXT — DSP/PlateReverb.h
// Vokal plate reverb (M2): Dattorro (1997, "Effect Design Part 1") şekil-8 tank topolojisi — endüstri klasiği,
// vokalde kanıtlanmış yoğun/pürüzsüz plate. FDN denemesi değil, yayın tap tablosuyla birebir.
//  • Giriş: pre-delay → HP (vokal alt gövdesi reverbe girmez) → bant LP → 4 seri difüzör allpass.
//  • Tank: iki yarım; her yarım: modüle allpass (±8 örnek, ~1 Hz — metaliklik kırılır) → gecikme → sönüm LP →
//    decay → allpass → gecikme → decay → çapraza. Tüm uzunluklar 29761 Hz referansından sr'a ölçeklenir.
//  • Decay: T60'tan İLKELİ eşleme: decay = 10^(−3·loopSec/(4·T60)) (döngü başına 4 decay uygulanır).
//  • Çıkış: yayının L/R tap toplamları (7'şer tap) → width (M/S) → ducking (vokal varken kısılır) → mix (wet ekleme).
//  • Latency 0 (yalnız kuyruk). idleWet kuyruğu doğal söndürür (hayalet kuyruk yok).
// ODA TİPİ (M3.5 FX KARAKTER) — aynı Dattorro tankı, tip başına ölçek + difüzyon + modülasyon + erken yansıma:
//  • 0 PLATE (varsayılan): M2 davranışı BİT BİT korunur (FNV-1a özet testi: 48k/44.1k/96k, 3 parametre seti).
//  • 1 ODA (Room): tank/difüzör ×0.42, 7+7 tap erken yansıma 5–45 ms (L/R farklı, −6…−9 dB), sığ modülasyon.
//  • 2 SALON (Hall): ×1.12, Plate düzeyinde difüzyon, modülasyon daha derin/yavaş (×1.4 sapma, 0.6 Hz), geç
//    yoğunlaşan başlangıç. Araştırma: Killoki VintageVerb 'Bright Hall' 2.36 s, low cut 370 / high cut 8 k —
//    HP ve damping kullanıcı parametresi, tip yalnız mekânı verir. (M3.5 denetimi, bulgu 7: eski ×1.55 +
//    0.55/0.45 difüzyon kuyruğu dört tipin en seyreği yapıyordu — Abel–Huang yoğunluk @300 ms 0.48, Plate 0.79 —
//    ve 0.5 s altı T60 ulaşılamazdı. Yeni dengede yoğunluk 300–400 ms ort. 0.88 ≥ Plate 0.79, test bağlar.)
//  • 3 CHAMBER: ×0.70, yüksek difüzyon (0.80/0.70, tank 0.72/0.55), 7+7 tap 10–60 ms, parlak (sönüm fc ×1.3).
//    Araştırma: Motive "chamber/room karakterli, altları çok kesik, parlak" kuyruk. (Denetim, bulgu 10: eski
//    "bant LP neredeyse açık" farkı — katsayı 0.02 / 0.05 — 48k'da fc ≈ 30 k / 23 k demekti: duyulmaz ve sr'a
//    normalize değil → kaldırıldı, parlaklık yalnız sönüm fc çarpanından gelir.)
//  • SEVİYE EŞLEMESİ (denetim, bulgu 10): tankKazanc tüm ıslak yola (ER DAHİL) uygulanır. Ölçülen (48k, dürtü,
//    decay 1.5, damp 12k): tepe (slap) Plate'e göre Oda −0.2 / Salon −0.0 / Chamber +0.6 dB (test sınırı ±1);
//    kuyruk RMS 200–400 ms Oda −0.5 / Salon +0.0 / Chamber −0.7 dB.
//  • DECAY EŞLEMESİ (M3.5 denetimi, bulgu 4 — üç parça, ölçüm: Schroeder geri entegrasyon −5…−25 dB):
//    (1) KISA UÇ: tank allpass'ları decayG'den bağımsız ~0.5 s çınlıyor ve kısa T60'ı tabanlıyordu → 0.6 s
//        altında tank difüzyon kazançları decay'le kısılır (tüm tipler; 0.6 s üstü çarpan TAM 1). Ek olarak
//        alt klemp Plate 0.15 → 0.01 / diğerleri 0.02 → 0.005 (Plate'te 0.15'e ancak decay < 0.66 s'de
//        iniliyordu → decay ≥ 0.66 s bit-birebir) ve tip başına ölçülü kisaDuzelt hedef kısaltması.
//    (2) SÖNÜM DÖNGÜ KAYBI (yalnız tip 1–3): damp LP'si döngü başına 2 kez geçer; eşleme artık bunu döngü
//        uzunluğuyla ölçeklenen temsili frekansta D = |H| ile telafi eder (g⁴·D² hedefe eşitlenir). Plate'e
//        uygulanmaz (bit-birebir): damp 3 kHz decay 3 s'de Plate ×0.76'ya kısalır — M2'den beri bilinen sınır.
//    (3) ÖLÇÜLEN TABLO (48k, damp 12k; test sınırı hedefin ±%25'i, 4 tip × {0.3, 0.8, 1.5, 3.0} s):
//      hedef 0.3 s : Plate ×0.94 | Oda ×0.94 | Salon ×1.04 | Chamber ×0.80   (eski: ×2.33 / ×1.08 / ×2.04 / ×1.26)
//      hedef 0.8 s : ×1.06 | ×0.98 | ×1.09 | ×1.07
//      hedef 1.5 s : ×0.96 | ×0.92 | ×1.01 | ×0.97
//      hedef 3.0 s : ×0.89 | ×0.89 | ×0.94 | ×0.90
//    damp 3000 (tip 1–3, düzeltme (2) ile): 0.8 s ×0.89–0.97, 1.5 s ×0.92–0.94, 3.0 s ×0.97–1.09 (eski −%27'ye
//    dek kısalıyordu). 44.1k/96k aynı sınırlar içinde (96k'da tüm tipler hafif kısa, en kötü ×0.77).
//  • Tip değişimi RT GÜVENLİ ve tıksız: ıslak yol 30 ms söner → tank/difüzörler temizlenir + mantıksal uzunluklar
//    değişir (prepare en büyük ölçek için TAHSİS etti; burada yalnız fill) → 30 ms açılır. Kapalıyken / prepare
//    veya reset sonrası İLK örneğe kadar geçiş anında yapılır (duyulmuyor, tank boş).
//    İPTAL (M3.5 denetimi): sönüş sürerken kullanıcı ESKİ tipe dönerse tank SİLİNMEZ — geçiş iptal edilir, çıkış
//    aynı rampayla geri açılır (durum 3). Eskiden sönüş koşulsuz tamamlanıp tipUygula çağrılıyordu: Plate→Oda→
//    (16 ms sonra) Plate'te ıslak RMS T+100 ms'de −5.5 dB çöküyordu; düzeltme sonrası ölçülen fark 0.0 dB.
//    GERÇEK tip değişiminde kuyruk KESİNTİSİ tasarım gereği kalır (tek tank, tahsissiz): ıslak RMS ~40 ms
//    boyunca −16 dB'ye iner, ~140 ms'de geri gelir. TIK YOK (maks örnek adımı kararlı durumun ×1.09'u).
#include "Module.h"

namespace sweax
{

struct ReverbParams
{
    bool  on { false };
    float mix { 0.14f };
    float preDelayMs { 45.0f };
    float decaySec { 1.4f };
    float dampHz { 7000.0f };
    float hpHz { 220.0f };
    float width { 1.0f };
    float duckDb { 6.0f };
    int   type { 0 };                  // 0 Plate, 1 Oda (Room), 2 Salon (Hall), 3 Chamber
};

class PlateReverb : public ModuleBase
{
public:
    static constexpr int TIP_SAYISI = 4;
    static constexpr double TIP_GECIS_MS = 30.0;         // sön + aç (her biri)

    void prepare (double sampleRate, int maxBlock, int numChannels);
    int latencySamples (Quality) const noexcept { return 0; }
    void setQuality (Quality) noexcept {}
    void setParams (const ReverbParams& p) noexcept { pending = p; paramsDirty = true; }

    // test/UI: şu an tankta çalan tip ve geçiş sürüyor mu
    int activeType() const noexcept { return aktifTip; }
    bool typeTransitioning() const noexcept { return tipDurum != 0; }

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    struct Ring
    {
        std::vector<float> b; int len { 1 }, wp { 0 };
        // tahsis yalnız prepare'da (kapasite); len = mantıksal uzunluk (tip değişiminde tahsissiz değişir)
        void init (int n) { len = std::max (2, n); b.assign ((size_t) len, 0.0f); wp = 0; }
        void setLen (int n) noexcept { len = std::clamp (n, 2, (int) b.size()); reset(); }
        void reset() noexcept { std::fill (b.begin(), b.end(), 0.0f); wp = 0; }
        inline void push (float x) noexcept { b[(size_t) wp] = x; wp = (wp + 1) % len; }
        inline float tap (int d) const noexcept { int i = wp - 1 - d; while (i < 0) i += len; return b[(size_t) i]; }
        inline float tapFrac (double d) const noexcept
        {
            double rp = (double) wp - 1.0 - d; while (rp < 0.0) rp += (double) len;
            const int i0 = (int) rp; const double fr = rp - i0; const int i1 = (i0 + 1) % len;
            return (float) ((1.0 - fr) * b[(size_t) i0] + fr * b[(size_t) i1]);
        }
    };
    // sabit-katsayılı allpass: y = −g·x + tap; push(x + g·y)
    struct Ap
    {
        Ring r; int d { 1 }; float g { 0.5f };
        void allocate (int maxDelay) { r.init (maxDelay + 2); }
        void set (int delay, float gain) noexcept { d = delay; g = gain; r.setLen (delay + 2); }
        void reset() noexcept { r.reset(); }
        inline float tick (float x) noexcept
        {
            const float t = r.tap (d);
            const float y = t - g * x;
            r.push (x + g * y);
            return y;
        }
    };
    // bir tipin tüm tam-sayı uzunlukları (prepare kapasite hesabı + tip uygulama ortak)
    struct Uzunluk
    {
        int dif[4] {}, modD[2] {}, modPay { 0 }, exc { 0 }, d1[2] {}, d3[2] {}, ap2[2] {};
        int cikis[14] {};
        int er[2][7] {};
    };
    void uzunlukHesapla (int tip, Uzunluk& u) const noexcept;
    void tipUygula (int tip) noexcept;                    // RT güvenli: tahsis yok, halkaları temizler
    void turevHesapla() noexcept;                         // decayG + dampCoef (cur + aktif tip)
    void updateParams() noexcept;
    void tickTank (float in, float& outL, float& outR) noexcept;

    ReverbParams pending, cur;
    bool paramsDirty { true };
    // "tank boş" — YALNIZ prepare/reset sonrası, İLK örnek işlenene kadar true. (Denetim: eskiden bu bayrak
    // paramsDirty kapısına bağlıydı; arayan her blok setParams çağırmazsa reset'ten çok SONRA, tank yeniden
    // DOLDUKTAN sonra bile true kalıyor ve o andaki tip değişimi 30 ms sön/aç rampasını ATLIYORDU —
    // ölçüldü: ıslak maks örnek adımı 0.00847 → 0.22742, ×26.8.)
    bool bosDurum { true };
    double scale { 1.0 };

    Ring pre;                     // pre-delay
    int preN { 0 };
    Biquad inHp;                  // giriş HP
    float bwState { 0.0f };       // giriş bant LP (tek kutup)
    float bantK { 0.05f };        // bant LP katsayısı (Plate 0.05 = bandwidth 0.95)
    float tankK { 1.0f };         // tip kuyruk seviye eşlemesi (Plate 1, uygulanmaz)
    Ap dif1, dif2, dif3, dif4;    // giriş difüzörleri
    // tank (L/R yarımlar)
    Ring modRingL, modRingR; int modDL { 1 }, modDR { 1 };
    float tankG { 0.70f };        // modüle allpass g (decay diffusion 1)
    int excN { 1 };               // modülasyon sapması (örnek; tepe-tepe 2·excN)
    double lfo { 0.0 }, lfoInc { 0.0 };
    Ring d1L, d3L, d1R, d3R;      // tank gecikmeleri (Dattorro 4453/3720, 4217/3163)
    Ap ap2L, ap2R;                // tank allpass (1800 / 2656)
    int n1L { 1 }, n3L { 1 }, n1R { 1 }, n3R { 1 };
    int cikisTap[14] {};          // çıkış tap konumları (tip ölçekli)
    float dampL { 0.0f }, dampR { 0.0f }, dampCoef { 0.3f };
    float decayG { 0.5f };
    float fbL { 0.0f }, fbR { 0.0f };
    // erken yansımalar (Oda / Chamber) — tanka paralel
    Ring erHat;
    int erSayi { 0 };
    int erTapL[7] {}, erTapR[7] {};
    float erGL[7] {}, erGR[7] {};
    // tip geçişi: 0 kararlı, 1 çıkış sönüyor, 2 yeni tanka giriş rampası, 3 İPTAL (tank korunur, çıkış geri açılır)
    int aktifTip { 0 }, hedefTip { 0 }, tipDurum { 0 };
    float tipFade { 1.0f }, girisFade { 1.0f }, tipAdim { 0.001f };   // çıkış sönüşü / boş tanka giriş rampası
    // ducking + mix
    float duckEnv { 0.0f }, duckRise { 0.0f }, duckFall { 0.0f }, duckAmt { 6.0f };
    float widthAmt { 1.0f };
    LinearSmoother mixS;
};

} // namespace sweax
