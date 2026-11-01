// SWEAX NEXT — DSP/SpektralMotor.cpp
#include "SpektralMotor.h"
#include "../Chain/VocalChain.h"

namespace sweax
{

namespace
{
    constexpr double PI_D = 3.14159265358979323846;
    inline int log2Tam (int n) noexcept { int l = 0; while ((1 << l) < n) ++l; return l; }
}

// ================================================================================================= HizliFFT ====
void HizliFFT::hazirla (int n)
{
    maxN = std::max (2, n);
    maxLog = log2Tam (maxN);
    maxN = 1 << maxLog;
    ters.assign ((size_t) maxN, 0);
    for (int i = 0; i < maxN; ++i)
    {
        int r = 0;
        for (int b = 0; b < maxLog; ++b) if (i & (1 << b)) r |= 1 << (maxLog - 1 - b);
        ters[(size_t) i] = r;
    }
    // seviye tabloları: yarım uzunluk h için [h−1, 2h−1) → cos(πj/h), ±sin(πj/h)
    cosT.assign ((size_t) maxN, 1.0f);
    sinIleri.assign ((size_t) maxN, 0.0f);
    sinGeri.assign ((size_t) maxN, 0.0f);
    for (int h = 1; h < maxN; h <<= 1)
        for (int j = 0; j < h; ++j)
        {
            const double a = PI_D * (double) j / (double) h;
            cosT[(size_t) (h - 1 + j)] = (float) std::cos (a);
            sinIleri[(size_t) (h - 1 + j)] = (float) -std::sin (a);
            sinGeri[(size_t) (h - 1 + j)] = (float) std::sin (a);
        }
}

void HizliFFT::geri (float* r, float* i, int n) const noexcept
{
    donustur (r, i, n, sinGeri.data());
    const float s = 1.0f / (float) n;
    for (int k = 0; k < n; ++k) { r[k] *= s; i[k] *= s; }
}

void HizliFFT::donustur (float* r, float* i, int n, const float* sinTablo) const noexcept
{
    if (n < 2 || n > maxN) return;
    const int kay = maxLog - log2Tam (n);
    for (int a = 0; a < n; ++a)
    {
        const int b = ters[(size_t) a] >> kay;
        if (a < b) { std::swap (r[a], r[b]); std::swap (i[a], i[b]); }
    }
    // h = 1: w = 1
    for (int a = 0; a < n; a += 2)
    {
        const float r0 = r[a], r1 = r[a + 1], i0 = i[a], i1 = i[a + 1];
        r[a] = r0 + r1; r[a + 1] = r0 - r1;
        i[a] = i0 + i1; i[a + 1] = i0 - i1;
    }
    for (int h = 2; h < n; h <<= 1)
    {
        const float* c = cosT.data() + (h - 1);
        const float* s = sinTablo + (h - 1);
        for (int a = 0; a < n; a += 2 * h)
        {
            float* ar = r + a; float* ai = i + a;
            float* br = r + a + h; float* bi = i + a + h;
            for (int j = 0; j < h; ++j)
            {
                const float tr = br[j] * c[j] - bi[j] * s[j];
                const float ti = br[j] * s[j] + bi[j] * c[j];
                br[j] = ar[j] - tr; bi[j] = ai[j] - ti;
                ar[j] += tr;        ai[j] += ti;
            }
        }
    }
}

// ============================================================================================= SpektralMotor ====
int SpektralMotor::boyutSec (double sampleRate) noexcept
{
    const double hedef = std::max (1.0, gecerliSr (sampleRate) * PENCERE_SANIYE);
    const int l = (int) std::lround (std::log2 (hedef));
    return 1 << std::clamp (l, 8, 13);
}

int SpektralMotor::boyutSec (double sampleRate, Quality q) noexcept
{
    const int n = boyutSec (sampleRate);
    return q == Quality::Canli ? n / 2 : n;
}

void SpektralMotor::prepare (double sampleRate, int numChannels)
{
    sr = gecerliSr (sampleRate);
    chans = std::clamp (numChannels, 1, MAX_KANAL);
    NMax = boyutSec (sr);
    donusum.hazirla (NMax);
    auto pencereKur = [] (int n, std::vector<float>& w, std::vector<float>& ola, float& norm)
    {
        w.assign ((size_t) n, 0.0f);
        ola.assign ((size_t) n, 0.0f);
        double toplam = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double hann = 0.5 - 0.5 * std::cos (2.0 * PI_D * (double) i / (double) n);   // periyodik
            w[(size_t) i] = (float) std::sqrt (hann);
            ola[(size_t) i] = (float) (0.5 * hann);                                              // w_a·w_s·0.5
            toplam += std::sqrt (hann);
        }
        norm = (float) ((2.0 / toplam) * (2.0 / toplam));
    };
    pencereKur (NMax, pencereB, olaB, normGucB);
    pencereKur (NMax / 2, pencereK, olaK, normGucK);
    girisBuf.assign ((size_t) (MAX_KANAL * NMax), 0.0f);
    akum.assign ((size_t) (MAX_KANAL * NMax), 0.0f);
    cikisBuf.assign ((size_t) (MAX_KANAL * NMax / 4), 0.0f);
    re.assign ((size_t) (MAX_KANAL * NMax), 0.0f);
    im.assign ((size_t) (MAX_KANAL * NMax), 0.0f);
    guc.assign ((size_t) (NMax / 2 + 1), 0.0f);
    kazanc.assign ((size_t) (NMax / 2 + 1), 1.0f);
    midParca.assign ((size_t) (NMax / 4), 0.0f);
    sonumN = std::max (1, (int) std::lround (SONUM_MS * 0.001 * sr));
    yukselN = std::max (1, (int) std::lround (YUKSEL_MS * 0.001 * sr));
    // ortak perde algılayıcı
    desim = std::clamp ((int) std::lround (sr / PERDE_SR), 1, 64);
    srD = sr / (double) desim;
    const auto lp = design::lowpass (sr, std::min (PERDE_LP_HZ, 0.40 * srD), 0.5412);   // 4. derece Butterworth
    const auto lp2 = design::lowpass (sr, std::min (PERDE_LP_HZ, 0.40 * srD), 1.3066);
    lpA.setCoeffs (lp); lpB.setCoeffs (lp2);
    tauMax = std::min ((int) std::ceil (srD / PERDE_MIN_HZ), PERDE_HALKA - 128);
    tauMin = std::clamp ((int) std::floor (srD / PERDE_MAX_HZ), 4, tauMax - 8);
    pW = PERDE_HALKA - tauMax;
    perdeAralik = std::max (1, (int) std::lround (PERDE_GUNCELLEME_MS * 0.001 * sr));
    perdeSayac = perdeAralik;                                  // ilk çerçevede hesapla
    perdeHalka.assign ((size_t) PERDE_HALKA, 0.0f);
    perdeBuf.assign ((size_t) PERDE_HALKA, 0.0f);
    perdeRho.assign ((size_t) PERDE_HALKA, 0.0f);
    perdeOnek.assign ((size_t) PERDE_HALKA + 1, 0.0);
    kucukMod = false;
    bekleyenN = 0; gecisDurum = 0; sustur = 1.0f; sicak = false;
    boyutUygula (NMax);
}

void SpektralMotor::boyutUygula (int yeniN) noexcept
{
    N = yeniN;
    H = N / 4;
    kucukMod = (N != NMax);
    normGuc = kucukMod ? normGucK : normGucB;
    halkaSifirla();
}

void SpektralMotor::halkaSifirla() noexcept
{
    std::fill (girisBuf.begin(), girisBuf.end(), 0.0f);
    std::fill (akum.begin(), akum.end(), 0.0f);
    std::fill (cikisBuf.begin(), cikisBuf.end(), 0.0f);
    std::fill (perdeHalka.begin(), perdeHalka.end(), 0.0f);
    lpA.reset(); lpB.reset();
    sayac = 0; desimSayac = 0; halkaYaz = 0; T0 = 0.0f; perdeSayac = perdeAralik;
}

// Boyut değişimi ERTELENİR: sönümleme → değiştir → hazırlık → yükselme (bkz. başlık "MOD GEÇİŞİ").
// Motor soğuksa (hiç örnek işlenmedi) anında uygulanır: prepare + setQuality dizisi doğru boyutta başlar.
void SpektralMotor::setQuality (Quality q) noexcept
{
    const int yeniN = boyutSec (sr, q);
    if (! sicak)
    {
        bekleyenN = 0; gecisDurum = 0; gecisSayac = 0; sustur = 1.0f;
        if (yeniN != N) boyutUygula (yeniN);
        return;
    }
    if (yeniN == N && gecisDurum == 0) return;                  // değişiklik yok
    if (yeniN == N && bekleyenN == yeniN) return;               // aynı hedefe zaten gidiyoruz
    bekleyenN = yeniN;
    if (gecisDurum == 0) { gecisDurum = 1; gecisSayac = sonumN; }
    else if (gecisDurum >= 2 && yeniN != N)                     // hazırlık/yükselme sırasında yeni istek
    { gecisDurum = 1; gecisSayac = std::max (1, (int) std::lround (sustur * (float) sonumN)); }
}

void SpektralMotor::gecisIlerlet() noexcept
{
    if (gecisDurum == 1)                                        // sönümleme bitti
    {
        sustur = 0.0f;
        const int hedefN = bekleyenN;
        bekleyenN = 0;
        if (hedefN == 0 || hedefN == N)                         // kullanıcı geri döndü: halkalar sağlam, doğrudan aç
        { gecisDurum = 3; gecisSayac = yukselN; return; }
        boyutUygula (hedefN);
        gecisDurum = 2; gecisSayac = N;                         // hazırlık: OLA dolana kadar sessiz
    }
    else if (gecisDurum == 2) { gecisDurum = 3; gecisSayac = yukselN; }
    else { gecisDurum = 0; gecisSayac = 0; sustur = 1.0f; }
}

void SpektralMotor::reset() noexcept
{
    if (bekleyenN != 0 && bekleyenN != N) boyutUygula (bekleyenN);   // bekleyen mod değişimi: sessizce uygulanır
    else halkaSifirla();
    bekleyenN = 0; gecisDurum = 0; gecisSayac = 0; sustur = 1.0f; sicak = false;
}

void SpektralMotor::isle (const float* const* in, float* const* out, int numCh, int n, SpektralIsleyici* isleyici) noexcept
{
    const int c = std::clamp (numCh, 1, chans);
    sicak = true;
    int i = 0;
    while (i < n)
    {
        int m = std::min (n - i, H - sayac);                   // çerçeve tetiğine kadar (dahil) örnek sayısı
        if (gecisDurum != 0) m = std::min (m, gecisSayac);     // geçiş faz sınırını aşma
        // 1) girişi yaz (çıkış yerinde olabilir → önce tüm parçayı kopyala)
        for (int k = 0; k < c; ++k)
        {
            float* g = girisBuf.data() + (size_t) (k * NMax) + (size_t) (N - H + sayac);
            const float* x = in[k] + i;
            for (int j = 0; j < m; ++j)
            {
                const float v = x[j];
                g[j] = (v > -1.0e6f && v < 1.0e6f) ? v : 0.0f;
            }
        }
        // 1b) mid akışı → perde ön ucu (ondalanmış halka her zaman ısınık kalır)
        {
            float* mp = midParca.data();
            const float* g0 = girisBuf.data() + (size_t) (N - H + sayac);
            if (c >= 2)
            {
                const float* g1 = girisBuf.data() + (size_t) NMax + (size_t) (N - H + sayac);
                for (int j = 0; j < m; ++j) mp[j] = 0.5f * (g0[j] + g1[j]);
            }
            else std::memcpy (mp, g0, sizeof (float) * (size_t) m);
            perdeBesle (mp, m);
        }
        // 2) çıkış: tetik öncesi örnekler mevcut çıkış tamponundan [sayac+1 ...]
        const float adim = gecisDurum == 1 ? -1.0f / (float) sonumN
                         : gecisDurum == 3 ?  1.0f / (float) yukselN : 0.0f;
        const bool tetik = sayac + m == H;
        const int onceki = tetik ? m - 1 : m;
        if (out != nullptr)
        {
            if (gecisDurum == 0)
            {
                for (int k = 0; k < c; ++k)
                {
                    const float* o = cikisBuf.data() + (size_t) (k * (NMax / 4)) + (size_t) (sayac + 1);
                    std::memcpy (out[k] + i, o, sizeof (float) * (size_t) onceki);
                }
            }
            else
                for (int j = 0; j < onceki; ++j)
                {
                    const float g = rampa (sustur + adim * (float) j);
                    for (int k = 0; k < c; ++k)
                        out[k][i + j] = cikisBuf[(size_t) (k * (NMax / 4) + sayac + 1 + j)] * g;
                }
        }
        if (tetik)
        {
            cerceve (c, isleyici);
            if (out != nullptr)
            {
                const float g = gecisDurum == 0 ? 1.0f : rampa (sustur + adim * (float) (m - 1));
                for (int k = 0; k < c; ++k) out[k][i + m - 1] = cikisBuf[(size_t) (k * (NMax / 4))] * g;
            }
            sayac = 0;
        }
        else sayac += m;
        // 3) geçiş durum makinesi (boyut değişimi sayac/halka sıfırlamayı etkilediği için EN SON)
        if (gecisDurum != 0)
        {
            sustur = std::clamp (sustur + adim * (float) m, 0.0f, 1.0f);
            gecisSayac -= m;
            if (gecisSayac <= 0) gecisIlerlet();
        }
        i += m;
    }
}

// ------------------------------------------------------------------------------- ortak perde algılayıcı ------
void SpektralMotor::perdeBesle (const float* mid, int n) noexcept
{
    for (int j = 0; j < n; ++j)
    {
        const float y = lpB.processSample (lpA.processSample (mid[j]));
        if (++desimSayac >= desim)
        {
            desimSayac = 0;
            perdeHalka[(size_t) halkaYaz] = std::isfinite (y) ? y : 0.0f;
            if (++halkaYaz >= PERDE_HALKA) halkaYaz = 0;
        }
    }
    lpA.sanitize(); lpB.sanitize();
}

// NCCF: referans = EN YENİ pW örnek; τ ∈ [tauMin, tauMax] geriye bakar. Sekizli koruması + parabolik ara değer.
void SpektralMotor::perdeHesapla() noexcept
{
    const int L = PERDE_HALKA;
    for (int j = 0; j < L; ++j) perdeBuf[(size_t) j] = perdeHalka[(size_t) ((halkaYaz + j) % L)];   // en yenisi sonda
    perdeOnek[0] = 0.0;
    for (int j = 0; j < L; ++j)
        perdeOnek[(size_t) (j + 1)] = perdeOnek[(size_t) j] + (double) perdeBuf[(size_t) j] * perdeBuf[(size_t) j];
    const int r0 = L - pW;                                     // referans pencere başlangıcı
    const double eRef = perdeOnek[(size_t) L] - perdeOnek[(size_t) r0];
    if (! (eRef > 1.0e-12)) { T0 = 0.0f; return; }             // sessizlik
    float* rho = perdeRho.data();                              // her gecikme İÇİN BİR KEZ (tepe arama tekrar okur)
    float enBuyuk = -1.0f;
    int enBuyukT = tauMin;
    for (int t = tauMin; t <= tauMax; ++t)
    {
        const int s = r0 - t;
        const float* a = perdeBuf.data() + r0;
        const float* b = perdeBuf.data() + s;
        // 4 ayrı toplayıcı: tek zincirde FMA gecikmesi (4 çevrim) darboğaz olur — ölçüldü, çerçeve başına 4× fark
        double n0 = 0.0, n1 = 0.0, n2 = 0.0, n3 = 0.0;
        int j = 0;
        for (; j + 4 <= pW; j += 4)
        {
            n0 += (double) a[j] * b[j];         n1 += (double) a[j + 1] * b[j + 1];
            n2 += (double) a[j + 2] * b[j + 2]; n3 += (double) a[j + 3] * b[j + 3];
        }
        for (; j < pW; ++j) n0 += (double) a[j] * b[j];
        const double num = (n0 + n1) + (n2 + n3);
        const double eL = perdeOnek[(size_t) (s + pW)] - perdeOnek[(size_t) s];
        const float v = (float) (num / std::sqrt (std::max (eRef * eL, 1.0e-24)));
        rho[t] = std::isfinite (v) ? v : 0.0f;
        if (rho[t] > enBuyuk) { enBuyuk = rho[t]; enBuyukT = t; }
    }
    if (! (enBuyuk >= PERDE_ESIK)) { T0 = 0.0f; return; }
    // ≥ 0.85·maks olan EN KÜÇÜK gecikmeli yerel tepe → 2·T0 / 3·T0 katları seçilmez
    int T = enBuyukT;
    for (int t = tauMin + 1; t < tauMax; ++t)
        if (rho[t] >= 0.85f * enBuyuk && rho[t] >= rho[t - 1] && rho[t] >= rho[t + 1]) { T = t; break; }
    float d = 0.0f;
    if (T > tauMin && T < tauMax)
    {
        const float payda = rho[T - 1] - 2.0f * rho[T] + rho[T + 1];
        if (std::fabs (payda) > 1.0e-9f) d = std::clamp (0.5f * (rho[T - 1] - rho[T + 1]) / payda, -0.5f, 0.5f);
    }
    const float t0 = ((float) T + d) * (float) desim;
    T0 = std::isfinite (t0) && t0 > 0.0f ? t0 : 0.0f;
}

void SpektralMotor::cerceve (int numCh, SpektralIsleyici* isleyici) noexcept
{
    const int B = N / 2 + 1;
    bool fftYolu = false;
    if (isleyici != nullptr)
    {
        // perde korelasyonu hop'a değil ZAMANA bağlı çalışır: CANLI'da hop yarıya iner ama perde 10 ms'de bir
        // güncellenmeye devam eder (perde zaten yavaş değişir; hop başına çalıştırmak CANLI'da %1.3 CPU yiyordu)
        perdeSayac += H;
        if (perdeSayac >= perdeAralik) { perdeSayac = 0; perdeHesapla(); }
        const float* pencere = pencereD();
        for (int k = 0; k < numCh; ++k)
        {
            const float* g = girisBuf.data() + (size_t) (k * NMax);
            float* r = re.data() + (size_t) (k * NMax);
            float* q = im.data() + (size_t) (k * NMax);
            for (int j = 0; j < N; ++j) { r[j] = g[j] * pencere[j]; q[j] = 0.0f; }
            donusum.ileri (r, q, N);
        }
        // MID güç spektrumu (sinüs tepe genliği² ölçeği) + Parseval ile çerçeve RMS'i
        const float* rL = re.data();
        const float* iL = im.data();
        double tam = 0.0;
        if (numCh >= 2)
        {
            const float* rR = re.data() + (size_t) NMax;
            const float* iR = im.data() + (size_t) NMax;
            for (int b = 0; b < B; ++b)
            {
                const float mr = 0.5f * (rL[b] + rR[b]), mi = 0.5f * (iL[b] + iR[b]);
                const float p = mr * mr + mi * mi;
                guc[(size_t) b] = p * normGuc;
                tam += (b == 0 || b == N / 2) ? (double) p : 2.0 * (double) p;
            }
        }
        else
        {
            for (int b = 0; b < B; ++b)
            {
                const float p = rL[b] * rL[b] + iL[b] * iL[b];
                guc[(size_t) b] = p * normGuc;
                tam += (b == 0 || b == N / 2) ? (double) p : 2.0 * (double) p;
            }
        }
        // Σ_n (w·x)² = tam / N ; Σ w² = N/2 → RMS² = tam / (N²/2)
        const double rms2 = tam / (0.5 * (double) N * (double) N);
        SpektralCerceve cer;
        cer.guc = guc.data(); cer.bins = B; cer.N = N; cer.hop = H; cer.sr = sr;
        cer.rmsDb = (float) (10.0 * std::log10 (rms2 + 1.0e-30));
        cer.T0 = T0;
        std::fill (kazanc.begin(), kazanc.begin() + B, 1.0f);
        fftYolu = isleyici->cerceveIsle (cer, kazanc.data());
        if (fftYolu)
        {
            float kontrol = 0.0f;
            for (int b = 0; b < B; ++b) kontrol += kazanc[(size_t) b];
            if (! std::isfinite (kontrol)) fftYolu = false;      // sigorta: zehirli kazanç → birim çerçeve
        }
    }

    const float* pencere = pencereD();
    const float* olaBirim = olaD();
    for (int k = 0; k < numCh; ++k)
    {
        float* a = akum.data() + (size_t) (k * NMax);
        if (fftYolu)
        {
            float* r = re.data() + (size_t) (k * NMax);
            float* q = im.data() + (size_t) (k * NMax);
            const float* G = kazanc.data();
            for (int b = 0; b < B; ++b) { r[b] *= G[b]; q[b] *= G[b]; }
            for (int b = B; b < N; ++b) { const float g = G[N - b]; r[b] *= g; q[b] *= g; }
            donusum.geri (r, q, N);
            for (int j = 0; j < N; ++j) a[j] += r[j] * pencere[j] * 0.5f;
        }
        else
        {
            const float* g = girisBuf.data() + (size_t) (k * NMax);
            for (int j = 0; j < N; ++j) a[j] += g[j] * olaBirim[j];
        }
        // tamamlanan H örnek → çıkış; akümülatör ve giriş H kaydırılır
        std::memcpy (cikisBuf.data() + (size_t) (k * (NMax / 4)), a, sizeof (float) * (size_t) H);
        std::memmove (a, a + H, sizeof (float) * (size_t) (N - H));
        std::fill (a + (N - H), a + N, 0.0f);
        float* g = girisBuf.data() + (size_t) (k * NMax);
        std::memmove (g, g + H, sizeof (float) * (size_t) (N - H));
    }
}

} // namespace sweax
