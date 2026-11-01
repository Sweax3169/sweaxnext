#pragma once
// SWEAX NEXT — DSP/SpektralMotor.h
// Ortak WOLA STFT motoru (M3.5 "spektral" akışı): YUMUŞAT (dinamik rezonans bastırma) ve FORMANT (perdeden bağımsız
// formant kaydırma) bu motorun üstüne kurulur. Motor bir MODÜL DEĞİLDİR — modül onu üye olarak taşır.
//  • Pencere: periyodik sqrt-Hann analiz + aynı sentez, hop = N/4. Σ w_a·w_s (N/4 kaydırmalı) = 2 TAM (kosinüs
//    terimleri birbirini götürür) → ölçek 0.5; değiştirilmemiş binlerde yeniden kurulum yuvarlama hatası düzeyinde
//    (testte beyaz gürültü ve sinüste gecikme hizalı fark ≤ −80 dB ölçülür).
//  • N = sr·0.02133'e en yakın 2'nin kuvveti (44.1/48k → 1024, 88.2/96k → 2048, 176.4/192k → 4096, 22.05k → 512);
//    CANLI → N/2, YÜKSEK/ULTRA → N. prepare en büyük N için tahsis eder; setQuality tahsissiz geçiş.
//  • Gecikme = N − 1 (TAM): çerçeve, son örneği girer girmez işlenir ve o örneğin çıkışı çerçeveden SONRA okunur;
//    tamamlanmış OLA örnekleri [t−N+1, t−N+H] aralığıdır → sabit N − 1 (dürtü testiyle birebir doğrulanır).
//  • FFT: kendi yinelemeli karmaşık FFT'miz (HizliFFT). juce::dsp::FFT (bu platformda FFTFallback) ÖLÇÜLDÜ:
//    operator new yapmıyor ama her çağrıda SpinLock alıyor ve N·8+16 baytı ses iş parçacığı yığınına alloca
//    ediyor (192k'da 32 KB — bazı hostların ses iş parçacığı yığını 64 KB). Kendi FFT'miz kilitsiz, yığınsız,
//    ayrı re/im dizileri; JUCE ile N = 2..8192 eşdeğer (göreli hata ≤ 2e-5) ve ölçüldü: N=1024 ileri+geri
//    9.3 µs vs juce::dsp::FFT real-only 30.1 µs (SpektralTest: spektral_fft_juce_esdegerlik).
//  • Stereo: kanal başına bağımsız analiz/sentez; kazanç eğrisi MID (L+R)/2 spektrumundan BİR KEZ hesaplanır ve
//    iki kanala aynı uygulanır (imaj kaymaz). Mono çalışır.
//  • Kanca: sanal SpektralIsleyici::cerceveIsle (std::function YOK) — mid güç spektrumu girer, bin başına doğrusal
//    kazanç çıkar. false dönerse (birim) motor sentez FFT'sini ATLAR ve çerçeveyi zaman alanında birim katkıyla
//    ekler (0.5·Hann·x) — kapalı/nötr modül ucuz ve OLA akümülatörü dolu/hizalı kalır.
//  • Boşta (isleyici = nullptr, çıkış = nullptr): halkalar girişle beslenir → yeniden açılışta 10 ms enable
//    crossfade'i bayat/boş ses görmez.
//  • NaN/Inf/|x| ≥ 1e6 giriş örneği 0 kabul edilir (AirExciter standardı); sonlu olmayan kazanç çerçevesi birim
//    sayılır. sampleRate ≤ 0 / sonlu değil → gecerliSr() 48 kHz'e kelepçeler; MODÜLLER DE aynı kelepçeyi
//    kullanmak ZORUNDA (denetim: modül kelepçesiz sr ile tahsis edip motor 48k'ya kelepçeleyince FormantModule
//    kRe dizisi 256, motor N'i 1024 oldu → perdeKesimi re[N−b] ile yığını bozdu, STATUS_HEAP_CORRUPTION).
//
// ORTAK PERDE ALGILAYICI (M3.5 denetim düzeltmesi — kritik bulgu)
//  • Eskiden FORMANT perdeyi TEK ÇERÇEVENİN güç spektrumundan (Wiener-Khinchin dairesel otokorelasyon) çıkarıyordu.
//    N = 21.3 ms olduğu için bu ancak ~2.3 periyot pencereye sığınca çalışıyor: ÖLÇÜLDÜ (sentetik ünlü, +3 st,
//    hedef F1 oranı ×1.19) — 48k'da F0 110 Hz ×1.210 ama 105 Hz ×1.057, 100 Hz ×1.019, 85 Hz ×1.035; 44.1k'da
//    kırılma 100 Hz'de. Kırılma noktası her sr'de T0/N ≈ 0.43 (= 2.3 periyot) → hedef malzeme olan 85–120 Hz
//    Türkçe erkek rap vokali BANDININ TAM ORTASI çalışmıyordu. Kök neden fiziksel: 21 ms pencere 100 Hz
//    harmoniklerini (2.1 bin aralık, sqrt-Hann ana lobu 3 bin) AYIRAMAZ; algılayıcı F1 rezonansının çınlamasına
//    (700 Hz → 2 periyot = 138 örnek) kilitleniyordu.
//  • Çözüm: perde artık ÇERÇEVEDEN BAĞIMSIZ, uzun geçmişli ve motorda ORTAK (iki modül de aynı T0'ı kullanır).
//    Mid akışı 4. derece Butterworth (1.2 kHz) ile alçak geçirilir, D = round(sr/8k) ile ondalanır (48k → 6,
//    96k → 12, 192k → 24, 44.1k → 6, 22.05k → 3) ve srD ≈ 8 kHz'de 512 örneklik (64 ms) halkada tutulur. Her
//    çerçevede normalize edilmiş otokorelasyon (NCCF): referans pencere EN YENİ W = 512 − τmaks örnek, gecikmeler
//    τ ∈ [srD/700, srD/65] → F0 65–700 Hz. 48k'da τmaks = 124, W = 388 örnek = 48.5 ms (65 Hz'de 3.2 periyot).
//  • Sekizli hatası: ρ ≥ 0.85·maks olan EN KÜÇÜK gecikmeli yerel tepe (2·T0 seçilmez) + parabolik ara değer.
//    Ötümlülük eşiği ρ ≥ 0.5. Ötümsüz → T0 = 0 (modüller kendi varsayılanına düşer).
//  • Ondalama alçak geçireni İKİ İŞE YARAR: (1) aliasing'i keser, (2) saf yüksek tonların (ör. testteki 3.5 kHz
//    rezonans sinüsü, ıslıklı sesler) sahte "perde" üretmesini engeller — sinüs her periyodunun katında ρ = 1
//    verir ve arama aralığına düşen bir kat her zaman vardır. 1.2 kHz kesim 3.5 kHz'i 37 dB bastırır (2 kHz kesim
//    yalnız 19.5 dB bastırıyordu: pembe gürültü + 3.5 kHz testinde sahte kilitlenme riski ölçüldü).
//  • Korelasyon HOP'A DEĞİL ZAMANA bağlı çalışır: en fazla 10 ms'de bir (perde zaten yavaş değişir). Hop başına
//    çalıştırmak ÖLÇÜLDÜ: CANLI'da hop yarıya indiği için çerçeve hızı ikiye katlanıyor ve FORMANT %2.27 gerçek
//    zamana çıkıyordu (bütçe %2). 10 ms'lik sabit hızla: YUMUŞAT %0.94 (CANLI %0.89), FORMANT %1.33 (CANLI %1.33).
//  • Maliyet: 48k'da 114 gecikme × 388 örnek ≈ 44 kMAC/güncelleme, 100 güncelleme/s → 4.4 MMAC/s. İç döngü 4 ayrı
//    toplayıcı kullanır (tek double zincirinde FMA gecikmesi darboğaz). isleyici == nullptr iken (boşta/birim yol)
//    halka BESLENİR ama korelasyon ATLANIR.
//
// MOD GEÇİŞİ (setQuality) — TIKSIZ (M3.5 denetim düzeltmesi)
//  • ESKİ DAVRANIŞ ÖLÇÜLDÜ (48k, 3.5 kHz sinüs, blok 256): boyut değişince reset() halkaları anında sıfırlıyordu →
//    YÜKSEK→CANLI 9.33 ms, CANLI→YÜKSEK 20.00 ms TAM dijital sessizlik (−261 dBFS), örnekten örneğe sıçrama
//    0.4424 (girişin kendi maks sıçraması 0.2270 — yani iki katı: duyulabilir TIK) ve 12–22 kHz bandı
//    −54.3 dB'den +17.4 dB'ye fırlıyordu (+71.7 dB splatter).
//  • YENİ: setQuality boyutu ANINDA değiştirmez, ERTELER. isle() içinde sönümleme (3 ms) → boyut değişimi +
//    reset → hazırlık (N_yeni örnek, sessiz) → yükselme (5 ms) durum makinesi çalışır; sönüm/yükselme şekli
//    SMOOTHSTEP (uçlarda türev 0 — doğrusal rampa köşeleri 12–22 kHz'de −33.7 dB tıkırtı bırakıyordu).
//    SONRA ÖLÇÜLDÜ (aynı kurulum): sıçrama 0.064–0.091 — GİRİŞİN kendi maks sıçraması 0.2270'in ALTINDA ve mod
//    sabitken ölçülen 0.0779 tabanıyla aynı mertebede; 12–22 kHz splatter −35.4…−53.0 dB (taban −59.1);
//    kararlı seviyenin 20 dB altında kalan süre CANLI'ya 12.0 ms, YÜKSEK'e 22.7 ms — ama TAMAMEN rampalı.
//    (Süre N'e bağlıdır: OLA akümülatörünün yeniden dolması için N örnek şart.) Test: denetim_2_mod_gecisi_tiksiz.
//  • Neden crossfade DEĞİL: mod değişince GECİKME de değişiyor (N−1 ↔ N/2−1, 48k'da 512 örnek). İki çıkış
//    birbirinin 512 örnek KAYMIŞ hâli; üst üste bindirmek comb/faz süpürmesi üretir. Zaman çizgisi zaten
//    ekleniyor/atlanıyor → temiz ek yeri ancak SESSİZLİKTEN geçer.
//  • Motor "soğukken" (prepare/reset sonrası, hiç örnek işlenmemişken) setQuality ANINDA uygulanır: prepare'ın
//    hemen ardından mod kuran testler ve preset yüklemeleri rampasız/gecikmesiz doğru boyutta başlar.
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>
#include "../Core/Biquad.h"

namespace sweax
{

enum class Quality : int;   // Chain/VocalChain.h

// ---------------------------------------------------------------------------------------------------------------
// Yinelemeli karmaşık FFT (radix-2, yerinde). Tablolar yalnız hazirla()'da; ileri/geri tahsissiz ve kilitsiz.
//  ileri: X_k = Σ x_n e^{−i2πkn/N}      geri: x_n = (1/N) Σ X_k e^{+i2πkn/N}
// Seviye tabloları N'den bağımsızdır (yarım uzunluk h için cos/sin(πj/h)) → en büyük N'nin tablosu tüm küçük
// boyutlara hizmet eder; bit-ters tablo kaydırılarak kullanılır.
class HizliFFT
{
public:
    void hazirla (int maxN);
    void ileri (float* re, float* im, int n) const noexcept { donustur (re, im, n, sinIleri.data()); }
    void geri (float* re, float* im, int n) const noexcept;
    int maxBoyut() const noexcept { return maxN; }

private:
    void donustur (float* re, float* im, int n, const float* sinTablo) const noexcept;
    std::vector<int> ters;
    std::vector<float> cosT, sinIleri, sinGeri;
    int maxN { 0 }, maxLog { 0 };
};

// ---------------------------------------------------------------------------------------------------------------
struct SpektralCerceve
{
    const float* guc { nullptr };   // MID güç spektrumu [0, bins): sinüs TEPE genliği² ölçeğinde (0 dBFS sinüs ≈ 1.0)
    int bins { 0 };                 // N/2 + 1
    int N { 0 }, hop { 0 };
    double sr { 48000.0 };
    float rmsDb { -200.0f };        // çerçevenin (pencere ağırlıklı) mid RMS seviyesi, dBFS
    float T0 { 0.0f };              // ORTAK perde periyodu (TAM ÖRNEK, ara değerli); 0 = ötümsüz/bulunamadı
};

class SpektralIsleyici
{
public:
    virtual ~SpektralIsleyici() = default;
    // kazanc[0, bins) girişte 1.0; modül bin başına DOĞRUSAL kazanç yazar (negatif frekanslara motor aynalar).
    // Dönüş false → çerçeve birim (kazanç dizisi yok sayılır, sentez FFT'si atlanır). RT: tahsis/kilit yok.
    virtual bool cerceveIsle (const SpektralCerceve& c, float* kazanc) noexcept = 0;
};

// ---------------------------------------------------------------------------------------------------------------
class SpektralMotor
{
public:
    static constexpr double PENCERE_SANIYE = 0.02133;
    static constexpr int MAX_KANAL = 2;
    // ortak perde algılayıcı
    static constexpr int PERDE_HALKA = 768;             // ondalanmış geçmiş (≈ 64 ms @12 kHz)
    static constexpr double PERDE_SR = 12000.0;         // hedef ondalanmış hız
    static constexpr double PERDE_LP_HZ = 2000.0;       // ondalama alçak geçireni (4. derece Butterworth)
    static constexpr double PERDE_MIN_HZ = 65.0, PERDE_MAX_HZ = 700.0;
    static constexpr float PERDE_ESIK = 0.5f;           // NCCF ötümlülük eşiği
    static constexpr double PERDE_GUNCELLEME_MS = 10.0; // korelasyon en fazla bu sıklıkta (hop'tan BAĞIMSIZ)
    // mod geçişi
    static constexpr double SONUM_MS = 3.0, YUKSEL_MS = 5.0;

    // sampleRate kelepçesi — motor ve MODÜLLER aynı değeri kullanmak zorunda (bkz. başlık)
    static double gecerliSr (double sampleRate) noexcept
    {
        return (sampleRate > 0.0 && sampleRate < 1.0e9 && sampleRate == sampleRate) ? sampleRate : 48000.0;
    }
    // YÜKSEK/ULTRA çerçeve boyutu (sr'ye en yakın 2'nin kuvveti, [256, 8192])
    static int boyutSec (double sampleRate) noexcept;
    static int boyutSec (double sampleRate, Quality q) noexcept;
    static int gecikmeSec (double sampleRate, Quality q) noexcept { return boyutSec (sampleRate, q) - 1; }

    void prepare (double sampleRate, int numChannels);          // tahsis (en büyük N)
    void setQuality (Quality q) noexcept;                       // RT güvenli: boyut değişimi ERTELENİR (tıksız)
    void reset() noexcept;

    int latencySamples (Quality q) const noexcept { return gecikmeSec (sr, q); }
    int boyut() const noexcept { return N; }
    int hop() const noexcept { return H; }
    int bins() const noexcept { return N / 2 + 1; }
    int maxBins() const noexcept { return NMax / 2 + 1; }
    double sampleRate() const noexcept { return sr; }
    const HizliFFT& fft() const noexcept { return donusum; }    // modüllerin yardımcı dönüşümleri (kepstrum) için
    bool gecisteMi() const noexcept { return gecisDurum != 0; } // mod geçişi sürüyor (teşhis/test)
    float sonT0() const noexcept { return T0; }                 // teşhis/test

    // giriş → çıkış (out == in yerinde olabilir; out == nullptr → yalnız besle). isleyici == nullptr → birim.
    void isle (const float* const* in, float* const* out, int numCh, int n, SpektralIsleyici* isleyici) noexcept;

private:
    void cerceve (int numCh, SpektralIsleyici* isleyici) noexcept;
    void boyutUygula (int yeniN) noexcept;
    void halkaSifirla() noexcept;
    void gecisIlerlet() noexcept;
    // mod geçişi sönüm/yükselme şekli: smoothstep (uçlarda türev 0). Doğrusal rampa ÖLÇÜLDÜ: 3.5 kHz sinüste
    // 12–22 kHz bandı −33.7 dB (sabit mod tabanı −59.1 dB) — köşeler tıkırdıyordu.
    static float rampa (float x) noexcept
    { const float t = std::clamp (x, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }
    void perdeBesle (const float* mid, int n) noexcept;
    void perdeHesapla() noexcept;
    // aktif pencere/OLA dizileri: HAM İŞARETÇİ YOK (denetim: kopyalanan motor kaynağın tamponunu okuyordu —
    // ölçüldü, kopya kaynak yok edildikten sonra tam ölçekli çöp üretti). Seçim kucukMod bayrağıyla.
    const float* pencereD() const noexcept { return (kucukMod ? pencereK : pencereB).data(); }
    const float* olaD() const noexcept { return (kucukMod ? olaK : olaB).data(); }

    double sr { 48000.0 };
    int NMax { 1024 }, N { 1024 }, H { 256 };
    int chans { 2 }, sayac { 0 };
    bool kucukMod { false };
    HizliFFT donusum;
    std::vector<float> pencereB, pencereK;          // sqrt-Hann (NMax / NMax/2)
    std::vector<float> olaB, olaK;                  // 0.5·Hann (birim çerçeve katkısı)
    float normGucB { 1.0f }, normGucK { 1.0f };     // (2/Σw)²
    float normGuc { 1.0f };
    std::vector<float> girisBuf, akum, cikisBuf;    // MAX_KANAL × NMax (cikis: × NMax/4)
    std::vector<float> re, im;                      // MAX_KANAL × NMax
    std::vector<float> guc, kazanc;                 // NMax/2 + 1
    std::vector<float> midParca;                    // NMax/4 (perde ön ucu)
    // mod geçişi (ertelenmiş boyut değişimi + sessizlik rampası)
    int bekleyenN { 0 }, gecisDurum { 0 }, gecisSayac { 0 }, sonumN { 1 }, yukselN { 1 };
    float sustur { 1.0f };
    bool sicak { false };
    // ortak perde algılayıcı
    Biquad lpA, lpB;
    int desim { 6 }, desimSayac { 0 }, halkaYaz { 0 };
    double srD { 8000.0 };
    int tauMin { 11 }, tauMax { 124 }, pW { 388 };
    int perdeAralik { 1 }, perdeSayac { 0 };        // korelasyonu hop'tan bağımsız sabit hızda çalıştır
    std::vector<float> perdeHalka;                  // PERDE_HALKA
    std::vector<float> perdeBuf;                    // PERDE_HALKA (doğrusallaştırılmış, en yenisi sonda)
    std::vector<float> perdeRho;                    // PERDE_HALKA (gecikme başına NCCF; tepe arama tekrar okur)
    std::vector<double> perdeOnek;                  // PERDE_HALKA + 1 (kare önek toplamı)
    float T0 { 0.0f };
};

} // namespace sweax
