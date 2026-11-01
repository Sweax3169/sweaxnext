#pragma once
// SWEAX NEXT — DSP/FormantModule.h
// "FORMANT" (M3.5): PERDEDEN BAĞIMSIZ formant kaydırma — Soundtoys Little AlterBoy / Pitcher 'gender' tarzı.
// Araştırma: Şehinşah inceltilmiş burun tonu, Reçkol 'gender erkeğe' kalınlaştırma + AlterBoy, Motive WAVES'te
// 'pitch aynı formant aşağı'. Ortak STFT motoru (SpektralMotor) üstünde bir ZAMANLA DEĞİŞEN EQ'dur:
//  • Faz DOKUNULMAZ; yalnız bin başına gerçek kazanç → perde, zamanlama ve geçişler AYNEN kalır (otokorelasyon
//    F0 testle ölçülür).
//  • Zarf: MID log-genlikten "true envelope" (Röbel-Rodet): gerçek kepstrum → lifter → E; A ← max(A, E) ile 2 ek
//    yineleme (zarf harmonik TEPELERİNE yaslanır, tepe-vadi ortalamasına değil). Log tabanı çerçeve tepesinden −80 dB.
//  • PERDEYE UYARLI LİFTER: T0 artık MOTORUN ORTAK perde algılayıcısından gelir (ondalanmış uzun geçmişli NCCF,
//    bkz. SpektralMotor.h) — tek çerçeveden DEĞİL. Eski tek-çerçeve kepstral/otokorelasyon yöntemi 21 ms
//    pencereye ~2.3 periyottan az sığan perdelerde çöküyordu: ÖLÇÜLDÜ (48k, +3 st, hedef F1 oranı ×1.19)
//    F0 110 Hz ×1.210 ama 105 ×1.057, 100 ×1.019, 90 ×1.030, 85 ×1.035 — yani hedef malzemenin (Türkçe erkek
//    rap vokali, F0 85–120 Hz) tam ortası çalışmıyordu. Ortak algılayıcıyla F0 ≥ 65 Hz'de çalışır: SONRA
//    (48k ve 96k, +3 st) 85 ×1.178 | 90 ×1.204 | 100 ×1.236 | 110 ×1.208 | 120 ×1.200 — denetim_1 testi.
//    AŞAĞI yön sistematik olarak zayıf (−3 st, hedef ×0.841): 85 ×0.922 | 90 ×0.865 | 100 ×0.894 | 120 ×0.878.
//    Sebep zarf ÇÖZÜNÜRLÜĞÜ: 0.35·T0 kesimi 90 Hz genişlikli F1'i ~240 Hz'e yumuşatır, tepe konumu komşu F2'ye
//    (1200 Hz) doğru yanlıdır ve bu yanlılık AŞAĞI kaydırmaya KARŞI çalışır. KESIM_ORANI 0.30 ve 0.25 DENENDİ:
//    aşağı yön düzelmedi (85 Hz'de ×0.929 / ×0.935) ama yukarı yön F0 120'de ×1.077 / ×1.048'e çöktü ve
//    formant_j düştü → 0.35 korunuyor.
//    Ötümlü → kesim = 0.35·T0 ∈ [0.5 ms, min(5 ms, N/4)], Hann kenarı [0.75, 1.25] × kesim (≤ 0.4375·T0: perde
//    quefrency'si ve harmonik örnekleme sınırı T0/2 HER ZAMAN dışarıda). Ötümsüz/bulunamadı → 2.0 ms.
//    Kesim düşüşü anında, artışı 30 ms yumuşak.
//    ÖLÇÜLDÜ (F0 dürtü → F1 700/F2 1200/F3 2600 rezonatör; harmonik örnekli tepe oranı, hedef ×1.19 / ×0.84):
//      sabit 2 ms düz kepstrum : +3 st F1 ×1.036 F2 ×1.017  (formant YERİNDE kalıyor: 2 ms lifter ~500 Hz
//                                 yumuşatır, 90 Hz genişlikli gerçek tepe kalıntısı baskın)
//      sabit 2 ms + TE 2       : +3 st ×1.043 / ×1.012
//      uyarlı 0.35·T0 + TE 2   : +3 st ×1.202 / ×1.186 | −3 st ×0.879 / ×0.816   (F0 = 120 Hz)
//    Harmonik kazanç eğrisi düzgünlüğü (yumuşak eğimli dürtü dizisi, ±4 st, 2. fark maks): K 0.35 → F0 400/250/
//    180/120 Hz'de 0.26/0.80/0.40/0.11 dB; K 0.5 (T0/2) → F0 400'de 4.07 dB (perde dalgalanması zarfa sızıyor).
//  • ZAMAN YUMUŞATMASI (M3.5 denetim düzeltmesi): kepstral zarf tahmini çerçeveden çerçeveye doğal olarak oynar
//    (gürültü periodogramı üstel dağılımlıdır) ve eskiden bu doğrudan çıkışa geçiyordu. ÖLÇÜLDÜ (48k, beyaz
//    gürültü, 1/3 oktav bant kazancının ZAMAN std'si; aynı motor + zamanla değişmeyen eğim işleyicisi TABANI
//    0.00–0.01 dB): FORMANT +3 st → 500 Hz 0.85 | 1k 1.54 | 2k 1.58 | 4k 1.25 | 8k 0.93 | 14k 0.74 dB.
//    SONRA: 0.12–0.39 dB (YUMUŞAT'ın aynı ölçümdeki 0.09–0.27'siyle aynı mertebe). denetim_5 testi.
//    Düzeltme iki kademeli, ikisi de hop'a göre normalize (sr/N bağımsız): (1) güç spektrumu bin başına tek
//    kutupla yumuşatılır ve zarf BUNDAN çıkarılır; (2) ln kazanç eğrisi bin başına aynı katsayıyla yumuşatılır.
//    ZAMAN SABİTİ ÖTÜMLÜLÜĞE BAĞLI — çünkü STFT her N örnekte (48k'da 21.3 ms) BİR bağımsız spektrum üretir:
//    tek kutup ancak τ ≫ N/sr olunca kazanç sağlar (ölçüldü: 20 ms → 0.75 dB, 40 → 0.52, 70 → 0.37, 110 → 0.29).
//    Tek bir uzun sabit ünlü geçişlerini de geciktirirdi. Kusur zaten YALNIZ gürültü benzeri malzemede duyulur
//    (durağan ünlüde 1/3 oktav bant std'si 0.01–0.21 dB), hız da yalnız orada gereksizdir → ötümlü 25 ms,
//    ötümsüz 120 ms. Ötümlülük değişiminde BÜYÜYEN PENCERE ısınması (a = sayaç/(sayaç+1)) kullanılır: sabit
//    atlaması basamak/tık üretmez. True-envelope yinelemeleri de YALNIZ ötümlüde çalışır (ötümsüzde max()
//    yalnız periodogramın üst kuyruğunu kovalar).
//  • α = 2^(shift/12); E'(k) = E(k/α) doğrusal ara değer (Nyquist üstü → son bin değeri); ln G = E' − E, ±18 dB
//    kelepçe, 3-bin (0.25/0.5/0.25) yumuşatma.
//  • Seviye koruma: çerçeve MID enerji oranı Σ P·G² / Σ P (dB, ±6 dB sınırlı) 50 ms tek kutupla yumuşatılır ve
//    TERSİ uygulanır → formant kaydırma ses seviyesini oynatmaz (±3 st'te RMS değişimi testle ölçülür).
//  • shift değişimi α üzerinde 30 ms tek kutup (hop'a göre katsayı); mix aynı hızda. Mix SPEKTRAL alanda
//    uygulanır: G_eff = 1 + mix·(G − 1) ≡ gecikme hizalı kuru·(1−mix) + ıslak·mix (doğrusal, faz aynı → comb yok,
//    ayrı kuru hat gerekmez). Açılış/reset sonrası ilk blok hedeflerden rampasız başlar (enable crossfade örter).
//  • Birim: α = 1 (ve seviye telafisi + yumuşatılmış kazanç eğrisi sönmüş) VEYA mix = 0 → motor birim yola geçer.
//    Çerçeve RMS < −80 dBFS → birim (gürültü tabanında zarf tahmini yapılmaz).
//  • Gecikme = motorun (CANLI N/2−1, YÜKSEK/ULTRA N−1).
//  • CANLI MODDA KAYDIRMA ZAYIF — ÖLÇÜLDÜ, entegrasyonda dikkate alınmalı (48k, +3 st, hedef F1 ×1.189):
//      F0  85: YÜKSEK ×1.178 / CANLI ×1.053 | F0 100: ×1.236 / ×1.043 | F0 120: ×1.200 / ×1.056 | F0 150: ×1.085 /
//      ×1.067 — yani CANLI istenen 3 yarım tonun ancak ~0.8'ini veriyor.
//    Sebep PENCERE BOYUTUDUR, perde algılama DEĞİL (T0 artık ortak algılayıcıdan doğru geliyor): N/2 = 512 @48k →
//    bin 93.75 Hz, sqrt-Hann ana lobu ~280 Hz. 90 Hz genişlikli gerçek F1 rezonansı analizde zaten ~280 Hz'e
//    yayılıyor; kepstral zarf bunu daha da yumuşatıyor, dolayısıyla kazanç eğrisi G çıkıştaki KESKİN gerçek
//    tepeyi yerinden oynatamıyor (aynı mekanizma "sabit 2 ms düz kepstrum → ×1.036" satırında da görülüyor).
//    KESİM TAVANI DENENDİ: N/4 → 0.35·N (F0 100'de lifter 2.67 → 3.50 ms) kaydırmayı ×1.041'den ×1.043'e taşıdı,
//    yani tavan bağlayıcı değil — geri alındı. CANLI'da FORMANT'ı kullanmak isteyen zincir ya etkiyi az kabul
//    etmeli ya da modülü CANLI'da devre dışı bırakmalı.
#include "Module.h"
#include "SpektralMotor.h"

namespace sweax
{

struct FormantParams
{
    bool  on { false };
    float shiftSt { 0.0f };         // −6..+6 yarım ton (formant zarfı; perde DEĞİL)
    float mix { 1.0f };             // 0..1 gecikme hizalı kuru/ıslak
};

class FormantModule : public ModuleBase, private SpektralIsleyici
{
public:
    static constexpr double LIFTER_MS = 2.0;              // ötümsüz/varsayılan kesim
    static constexpr double LIFTER_MIN_MS = 0.5, LIFTER_MAX_MS = 5.0;
    static constexpr float KESIM_ORANI = 0.35f;              // ötümlü kesim = 0.35·T0 (Hann kenarı ≤ 0.4375·T0)
    static constexpr int ZARF_YINELEME = 2;               // true-envelope ek yineleme
    static constexpr float KELEPCE_DB = 18.0f;
    static constexpr float SEVIYE_SINIR_DB = 6.0f;
    static constexpr double SEVIYE_MS = 50.0, KAYMA_MS = 30.0;
    static constexpr double ZARF_MS = 25.0, ZARF_UZUN_MS = 120.0;   // zarf yumuşatması: ötümlü / ötümsüz
    static constexpr float SESSIZ_DBFS = -80.0f;
    static constexpr float LOG_TABAN_DB = -80.0f;

    void prepare (double sampleRate, int maxBlock, int numChannels);
    int latencySamples (Quality q) const noexcept { return SpektralMotor::gecikmeSec (sr, q); }
    void setQuality (Quality q) noexcept;                        // RT güvenli (tahsis yok); kuru gecikmeyi de ayarlar
    void setParams (const FormantParams& p) noexcept { pending = p; }

    // ölçer/teşhis
    float currentShiftSt() const noexcept { return 12.0f * std::log2 (std::max (alfa, 1.0e-3f)); }
    float lastLevelCompDb() const noexcept { return -seviyeDb; }
    float lastLifterMs() const noexcept { return (float) (1000.0 * (double) kesim / sr); }   // uyarlı kesim
    float lastF0Hz() const noexcept { const float t = motor.sonT0(); return t > 0.0f ? (float) (sr / t) : 0.0f; }
    bool lastVoiced() const noexcept { return sonOtumlu; }

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    bool cerceveIsle (const SpektralCerceve& c, float* kazanc) noexcept override;
    void durumSifirla() noexcept;
    void zarfHesapla (int N, int B, float hedefKesim, double hopSn, int yineleme) noexcept;
    bool notrDurum() const noexcept { return alfa == 1.0f && seviyeDb == 0.0f && lnGEnb == 0.0f; }

    FormantParams pending, cur;
    SpektralMotor motor;
    Quality mod { static_cast<Quality> (1) };
    bool bosta { true }, sonOtumlu { false };
    int sonN { -1 };                         // motorun boyutu değişince bin başına durum sıfırlanır
    int yumSayac { 0 };                      // zarf yumuşatıcı ısınma sayacı (büyüyen pencere → basamaksız geçiş)
    float alfa { 1.0f }, mixCur { 1.0f }, seviyeDb { 0.0f }, lnGEnb { 0.0f };
    float kesim { 96.0f };                   // lifter kesimi (örnek), yumuşatılmış
    std::vector<float> kRe, kIm;             // kepstrum çalışma dizileri (NMax)
    std::vector<float> logGen, zarf, lnG;    // maxBins
    std::vector<float> gucYum, lnGYum;       // maxBins — zamanda yumuşatılmış güç ve ln kazanç
};

} // namespace sweax
