#pragma once
// SWEAX NEXT — DSP/YumusatModule.h
// "YUMUŞAT" (M3.5): DİNAMİK REZONANS BASTIRMA — oeksound soothe2 / Waves Silk Vocal tarzı. Araştırma: Kava zincirinde
// Silk Vocal, Reçkol V2'de soothe2 ×2 (biri de-ess), Uzi niet041 presetinde soothe2, Motive taklitinde soothe2.
// Ortak STFT motoru (SpektralMotor) üstünde, çerçeve başına MID güç spektrumundan bin başına kazanç:
//  • ALGI SPEKTRUMU: güç, bin başına 10 ms tek kutupla zamanda yumuşatılır + PERDEYE UYARLI MAKS GENİŞLETME
//    (aşağıda). Ham periodogramın dB std'si ≈ 5.6 dB (üstel dağılım) — gürültü binleri de "rezonans" sayılır;
//    zaman yumuşatması bunu bastırır.
//  • PERDEYE UYARLI GENİŞLETME (M3.5 denetim düzeltmesi — yüksek şiddetli bulgu). Genişletme yarıçapı eskiden
//    SABİT ±1 bindi; harmonik aralığı ~5 bini geçince (F0 ≳ 230 Hz @48k/N=1024) CFAR eğitim binleri harmonikler
//    arası VADİLERE düşüyor, referans çöküyor ve TEMİZ harmonik içerik rezonans sanılıp bastırılıyordu.
//    ÖLÇÜLDÜ (48k, depth 0.7, aynı 3-rezonatör zarfı iki kaynağa: dürtü dizisi = harmonik, beyaz gürültü =
//    harmoniksiz ama ÖZDEŞ zarf; 2–12 kHz bant değişimi; girişte HİÇ rezonans yok):
//      ESKİ sharp 0.5: F0 150 harm −0.93 / gür −1.82 | F0 300 −2.95 / −1.82 | F0 400 −7.85 / −1.82 dB
//      ESKİ sharp 1.0: F0 300 −6.16 / −1.23 | F0 400 −8.51 / −1.23 dB          (azalma F0 ile tekdüze artıyor
//                      → etki spektral biçimden değil HARMONİK YAPIDAN geliyordu)
//    Düzeltme: genişletme yarıçapı g = yuvarla(0.5 · harmonik aralığı) bin, harmonik aralığı = N/T0 (T0 motorun
//    ORTAK perde algılayıcısından; ötümsüz → g = 1, eski davranış). Böylece maks-genişletme vadileri komşu
//    harmoniğin TEPESİYLE doldurur: `genis` tarak-düzleştirilmiş bir ZARF olur, referans onunla eşitlenir ve
//    fazlalık ≈ 0 çıkar. Koruma bandı da g + 2 bine genişler (binin kendi genişletilmiş tepesi eğitime girmesin),
//    pencere gerekirse buna göre açılır. Fiziksel sınır: bir rezonans harmonik aralığından dar ÇÖZÜLEMEZ — yüksek
//    F0'da `sharp` doygunlaşır. HEDEF MALZEMEDE DEĞİŞİKLİK YOK: Türkçe erkek rap vokali F0 85–120 Hz → aralık
//    1.8–2.6 bin → g = 1, yani aşağıdaki ablasyon/kalibrasyon ölçümleri aynen geçerli.
//    Yan etki (istenen): yarıçap BİN değil HZ cinsinden tanımlı olduğundan CANLI (N/2) ile YÜKSEK (N) aynı
//    bastırmayı verir. Eskiden F0 400 Hz'de fark 7.53 dB'ydi (Kalite anahtarı TON değiştiriyordu).
//  • REFERANS EĞRİ: log-frekans penceresinde KORUMA BANTLI (CFAR) KIRPILMIŞ güç ortalaması: binin kendisi ve ±(g+2)
//    komşusu (genişletilmiş tepe) ortalamaya girmez, 2 geçiş (ortalama → her bin kendi ortalamasına kırpılır →
//    yeniden ortalama). Pencere: sharp=0 → 1 oktav, sharp=1 → 1/6 oktav (geometrik ara); her yanda en az 2 eğitim
//    bini. Önek toplamları double (float önek farkı alçak-seviye HF binlerinde sayısal çöp üretir) → O(bins);
//    sınırlar yalnız sharp/N/g değişince yeniden hesaplanır (tahsissiz). Maks genişletme monoton kuyrukla O(bins).
//    Güç alanı (dB ortalaması değil): dB ortalaması tepe-vadi arasına oturur → normal harmonikler de "fazlalık"
//    sayılıp ses donuklaşır.
//    ABLASYON ÖLÇÜMÜ (48k, depth 0.7; pembe gürültü + yerel seviyenin +20 dB üstü 3.5 kHz sinüs | beyaz gürültü).
//    Ablasyon satırları ESKİ sabit ±2 binlik koruma bandıyla ölçüldü; "tam" satırı GÜNCEL koddur:
//      tam (güncel, ±(g+2))   : sinüs −9.25 dB (sharp 0.5) / −9.24 dB (sharp 1) | gürültü toplam −0.39 dB
//      tam (eski, sabit ±2)   : −9.23 / −8.49 dB                                | −0.39 dB
//      koruma bantsız         : −7.73 / −3.53 dB (dar pencerede tepe kendi referansını şişiriyor) | −0.35 dB
//      genişletmesiz          : −7.27 / −7.29 dB                                                 | −0.50 dB
//      zaman yumuşatmasız     : −9.21 / −8.47 dB                                                 | −0.65 dB
//      hiçbiri (ham)          : −6.94 / −5.25 dB                                                 | −0.85 dB
//    sharp=1'deki −8.49 → −9.24 iyileşmesi koruma bandının ±2'den ±3'e (g=1 iken) çıkmasındandır: sqrt-Hann ana
//    lobu (±1 bin) + genişletme (±1 bin) = ±2 bin, yani ESKİ ±2'lik bant tepenin eteğini hâlâ eğitime sokup
//    referansı şişiriyordu. Yeni bant, ötümsüz malzemede de doğru olanıdır.
//  • FAZLALIK e = algı_dB − ref_dB − 2 dB; odak bandı [loHz, hiHz], kenarlarda yarım oktav yükseltilmiş-kosinüs.
//  • HEDEF r = −min(3 + 9·depth, depth·e) dB × bant ağırlığı — ASLA yükseltme. Kapılar: çerçeve RMS < −70 dBFS
//    (sessizlik/oda gürültüsü pompalanmaz) ve algı tepe bininden 60 dB aşağıdaki binler işlenmez.
//  • r frekansta 3-bin üçgen (0.25/0.5/0.25) yumuşatılır (müzikal gürültü yok); zamanda bin başına atak τ = 1 hop,
//    bırakma speed=1 → 40 ms, speed=0 → 250 ms (geometrik; hop süresine göre katsayı → sr/N bağımsız).
//  • Ölçer: lastReductionDb() = odak bandındaki GERÇEK enerji değişimi 10·log10(Σ w·P·G² / Σ w·P) ≤ 0.
//  • Gecikme = motorun (CANLI N/2−1, YÜKSEK/ULTRA N−1); tüm kazançlar 0 dB iken motor birim yola geçer (ucuz).
#include "Module.h"
#include "SpektralMotor.h"

namespace sweax
{

struct YumusatParams
{
    bool  on { false };
    float depth { 0.35f };          // 0..1  (eğim = depth, aralık = 3 + 9·depth dB)
    float sharp { 0.5f };           // 0..1  (referans penceresi 1 → 1/6 oktav)
    float loHz { 2000.0f };         // 300..8000
    float hiHz { 12000.0f };        // 2000..20000
    float speed { 0.5f };           // 0..1  (bırakma 250 → 40 ms)
};

class YumusatModule : public ModuleBase, private SpektralIsleyici
{
public:
    static constexpr float ESIK_DB = 2.0f;
    static constexpr float SESSIZ_DBFS = -70.0f;
    static constexpr float BIN_KAPI_DB = 60.0f;
    static constexpr double ALGI_MS = 10.0;
    static constexpr int KORUMA = 2;                // koruma bandının genişletme yarıçapına EKLENEN payı
    static constexpr float GENIS_ORAN = 0.5f;       // genişletme yarıçapı = 0.5 × harmonik aralığı (bin)
    static constexpr int GENIS_MAKS = 16;

    void prepare (double sampleRate, int maxBlock, int numChannels);
    int latencySamples (Quality q) const noexcept { return SpektralMotor::gecikmeSec (sr, q); }
    void setQuality (Quality q) noexcept;                        // RT güvenli (tahsis yok); kuru gecikmeyi de ayarlar
    void setParams (const YumusatParams& p) noexcept { pending = p; }

    float lastReductionDb() const noexcept { return sonAzaltmaDb; }   // ≤ 0, odak bandı enerji değişimi
    // teşhis/test
    float lastF0Hz() const noexcept { const float t = motor.sonT0(); return t > 0.0f ? (float) (sr / t) : 0.0f; }
    int lastExpandBins() const noexcept { return genisG; }            // perdeye uyarlı genişletme yarıçapı

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    bool cerceveIsle (const SpektralCerceve& c, float* kazanc) noexcept override;
    void durumSifirla() noexcept;
    void pencereSinirlari (int N, int B) noexcept;
    void bantAgirliklari (int N, int B) noexcept;
    void maksGenislet (const float* x, float* y, int B, int g) noexcept;   // ±g kayan pencere maks, O(B)

    YumusatParams pending, cur;
    SpektralMotor motor;
    Quality mod { static_cast<Quality> (1) };      // son istenen mod (prepare yeniden uygular); 1 = Yuksek
    bool bosta { false }, azaltmaVar { false }, analizVardi { false };
    // bin başına durum (maxBins)
    std::vector<float> algi;                 // zamanda yumuşatılmış güç
    std::vector<float> genis;                // 3-bin maks genişletilmiş algı
    std::vector<double> onek;                // önek toplamı (bins + 1)
    std::vector<float> kirp;                 // kırpılmış algı
    std::vector<float> ref;                  // referans güç
    std::vector<float> hedef;                // hedef azaltma dB (≤ 0)
    std::vector<float> azaltma;              // yumuşatılmış azaltma dB (≤ 0)
    std::vector<float> bantW;                // odak bandı ağırlığı
    std::vector<int> pAlt, pUst;             // log-frekans pencere sınırları (dahil)
    std::vector<int> kuyruk;                 // maks genişletme için monoton indeks kuyruğu (maxBins)
    int sinirN { -1 }, bantN { -1 }, sinirG { -1 };
    int genisG { 1 }, korumaBin { 3 };       // perdeye uyarlı genişletme yarıçapı ve koruma bandı (bin)
    float sinirSharp { -1.0f }, bantLo { -1.0f }, bantHi { -1.0f };
    float sonAzaltmaDb { 0.0f };
};

} // namespace sweax
