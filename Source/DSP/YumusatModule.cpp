// SWEAX NEXT — DSP/YumusatModule.cpp
#include "YumusatModule.h"
#include "../Chain/VocalChain.h"

namespace sweax
{

namespace
{
    constexpr float LN10_20 = 0.11512925f;      // ln(10)/20
    constexpr int EGITIM_MIN = 2;               // her yanda en az eğitim bini
}

void YumusatModule::prepare (double sampleRate, int maxBlock, int numChannels)
{
    // motorun kelepçesiyle AYNI sr: ikisi farklı N seçerse gecikme raporu ve dizi boyları uyuşmaz
    const double s = SpektralMotor::gecerliSr (sampleRate);
    prepareBase (s, maxBlock, numChannels, SpektralMotor::boyutSec (s) - 1);
    motor.prepare (sr, chans);
    const size_t B = (size_t) motor.maxBins();
    algi.assign (B, 0.0f);
    genis.assign (B, 0.0f);
    onek.assign (B + 1, 0.0);
    kirp.assign (B, 0.0f);
    ref.assign (B, 0.0f);
    hedef.assign (B, 0.0f);
    azaltma.assign (B, 0.0f);
    bantW.assign (B, 0.0f);
    pAlt.assign (B, 0);
    pUst.assign (B, 0);
    kuyruk.assign (B, 0);
    sinirN = bantN = sinirG = -1;
    genisG = 1; korumaBin = 1 + KORUMA;
    sinirSharp = bantLo = bantHi = -1.0f;
    pending = cur = YumusatParams {};
    bosta = false;
    setQuality (mod);
    resetWet();
}

void YumusatModule::setQuality (Quality q) noexcept
{
    mod = q;
    motor.setQuality (q);          // boyut değişimi ERTELENİR (tıksız); bin başına durum sinirN ile sıfırlanır
    setDryDelay (latencySamples (q));
}

void YumusatModule::resetWet()
{
    motor.reset();
    durumSifirla();
}

void YumusatModule::durumSifirla() noexcept
{
    std::fill (algi.begin(), algi.end(), 0.0f);
    std::fill (azaltma.begin(), azaltma.end(), 0.0f);
    std::fill (hedef.begin(), hedef.end(), 0.0f);
    azaltmaVar = false;
    analizVardi = false;
    sonAzaltmaDb = 0.0f;
}

void YumusatModule::processWet (float* const* ch, int numCh, int n) noexcept
{
    cur = pending;
    cur.depth = std::clamp (cur.depth, 0.0f, 1.0f);
    cur.sharp = std::clamp (cur.sharp, 0.0f, 1.0f);
    cur.speed = std::clamp (cur.speed, 0.0f, 1.0f);
    cur.loHz = std::clamp (cur.loHz, 20.0f, 24000.0f);
    cur.hiHz = std::clamp (cur.hiHz, 20.0f, 96000.0f);
    bosta = false;
    // depth 0 ve bekleyen azaltma yok → analiz FFT'si bile gereksiz: birim yol (gecikme hizalı, tam şeffaf)
    const bool gerekli = cur.depth > 0.0f || azaltmaVar;
    if (! gerekli) sonAzaltmaDb = 0.0f;
    else if (! analizVardi) std::fill (algi.begin(), algi.end(), 0.0f);    // birim yoldan dönüş: bayat algı taşınmaz
    analizVardi = gerekli;
    motor.isle (ch, ch, numCh, n, gerekli ? static_cast<SpektralIsleyici*> (this) : nullptr);
}

void YumusatModule::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    motor.isle (ch, nullptr, numCh, n, nullptr);          // halkalar ısınık/hizalı kalır
    if (! bosta) { durumSifirla(); bosta = true; }        // yeniden açılış bayat azaltma taşımaz
}

void YumusatModule::pencereSinirlari (int N, int B) noexcept
{
    sinirN = N; sinirSharp = cur.sharp; sinirG = genisG;
    korumaBin = genisG + KORUMA;
    const double oktav = std::pow (1.0 / 6.0, (double) cur.sharp);          // 1 → 1/6 oktav (geometrik)
    const double fAlt = std::pow (2.0, -0.5 * oktav), fUst = std::pow (2.0, 0.5 * oktav);
    for (int b = 0; b < B; ++b)
    {
        int lo = (int) std::ceil ((double) b * fAlt - 1.0e-9);
        int hi = (int) std::floor ((double) b * fUst + 1.0e-9);
        lo = std::min (lo, b - korumaBin - EGITIM_MIN);                      // koruma dışında eğitim bini şart
        hi = std::max (hi, b + korumaBin + EGITIM_MIN);
        pAlt[(size_t) b] = std::clamp (lo, 1, B - 1);
        pUst[(size_t) b] = std::clamp (hi, 1, B - 1);
    }
}

// x[0,B) → y[0,B): y[b] = maks x[max(0,b−g) .. min(B−1,b+g)]. Monoton azalan indeks kuyruğu → O(B), tahsissiz.
void YumusatModule::maksGenislet (const float* x, float* y, int B, int g) noexcept
{
    int* d = kuyruk.data();
    int bas = 0, son = 0, j = 0;
    for (int b = 0; b < B; ++b)
    {
        const int hi = std::min (B - 1, b + g);
        while (j <= hi)
        {
            while (son > bas && x[d[son - 1]] <= x[j]) --son;
            d[son++] = j;
            ++j;
        }
        const int lo = b - g;
        while (d[bas] < lo) ++bas;
        y[b] = x[d[bas]];
    }
}

void YumusatModule::bantAgirliklari (int N, int B) noexcept
{
    bantN = N; bantLo = cur.loHz; bantHi = cur.hiHz;
    const double lo = std::min (cur.loHz, cur.hiHz);
    const double hi = std::min ((double) std::max (cur.loHz, cur.hiHz), 0.5 * sr);
    const double PI_D = 3.14159265358979323846;
    for (int b = 0; b < B; ++b)
    {
        const double f = (double) b * sr / (double) N;
        double w = 0.0;
        if (b > 0)
        {
            if (f < lo)
            {
                const double okt = std::log2 (lo / f);
                w = okt < 0.5 ? 0.5 + 0.5 * std::cos (PI_D * okt / 0.5) : 0.0;
            }
            else if (f > hi)
            {
                const double okt = std::log2 (f / hi);
                w = okt < 0.5 ? 0.5 + 0.5 * std::cos (PI_D * okt / 0.5) : 0.0;
            }
            else w = 1.0;
        }
        bantW[(size_t) b] = (float) w;
    }
}

bool YumusatModule::cerceveIsle (const SpektralCerceve& c, float* kazanc) noexcept
{
    const int B = c.bins, N = c.N;
    if (N != sinirN)                                  // motorun boyutu değişti → bin başına durum anlamını yitirdi
    {
        std::fill (algi.begin(), algi.end(), 0.0f);
        std::fill (azaltma.begin(), azaltma.end(), 0.0f);
        std::fill (hedef.begin(), hedef.end(), 0.0f);
        azaltmaVar = false;
    }
    // perdeye uyarlı genişletme yarıçapı: harmonik aralığının yarısı (ötümsüz → 1). Histerezis: sınırda titremesin.
    {
        const float gHedef = c.T0 > 0.0f
            ? std::clamp (GENIS_ORAN * (float) N / c.T0, 1.0f, (float) GENIS_MAKS) : 1.0f;
        if (std::fabs (gHedef - (float) genisG) > 0.6f)
            genisG = std::clamp ((int) std::lround (gHedef), 1, GENIS_MAKS);
    }
    if (N != sinirN || cur.sharp != sinirSharp || genisG != sinirG) pencereSinirlari (N, B);
    if (N != bantN || cur.loHz != bantLo || cur.hiHz != bantHi) bantAgirliklari (N, B);

    const double hopSn = (double) c.hop / c.sr;
    const float aAlgi = (float) std::exp (-hopSn / (ALGI_MS * 0.001));
    const float aAtk = (float) std::exp (-1.0);                                        // τ = 1 hop
    const double birakSn = 0.250 * std::pow (0.040 / 0.250, (double) cur.speed);      // 250 → 40 ms
    const float aBirak = (float) std::exp (-hopSn / birakSn);
    const float depth = cur.depth;
    const float aralik = 3.0f + 9.0f * depth;
    const float* P = c.guc;

    // 1) algı: zamanda yumuşatılmış güç + perdeye uyarlı maks genişletme
    for (int b = 0; b < B; ++b)
    {
        float v = P[(size_t) b] + aAlgi * (algi[(size_t) b] - P[(size_t) b]);
        if (! (v >= 1.0e-30f && v < 1.0e30f)) v = 0.0f;                               // NaN/Inf sigortası + denormal tabanı
        algi[(size_t) b] = v;
    }
    // ±genisG maks genişletme: harmonikler arası vadiler komşu TEPEYLE dolar → `genis` tarak-düzleştirilmiş zarf
    maksGenislet (algi.data(), genis.data(), B, genisG);
    float tepe = 0.0f;
    for (int b = 0; b < B; ++b) tepe = std::max (tepe, genis[(size_t) b]);

    // 2) hedef azaltma
    const bool sessiz = c.rmsDb < SESSIZ_DBFS || tepe <= 1.0e-20f;
    if (sessiz || depth <= 0.0f)
    {
        std::fill (hedef.begin(), hedef.begin() + B, 0.0f);
    }
    else
    {
        // koruma bantlı (CFAR) kırpılmış ortalama: eğitim = pencere − [b−korumaBin, b+korumaBin]
        // (korumaBin = genişletme yarıçapı + 2 → binin KENDİ genişletilmiş tepesi referansa girmez)
        auto ortalama = [&] (const float* x, float* cikis)
        {
            onek[0] = 0.0;
            for (int b = 0; b < B; ++b) onek[(size_t) (b + 1)] = onek[(size_t) b] + (double) x[b];
            for (int b = 1; b < B; ++b)
            {
                const int lo = pAlt[(size_t) b], hi = pUst[(size_t) b];
                const int gLo = std::max (lo, b - korumaBin), gHi = std::min (hi, b + korumaBin);
                double s = onek[(size_t) (hi + 1)] - onek[(size_t) lo];
                int say = hi - lo + 1;
                if (gHi >= gLo) { s -= onek[(size_t) (gHi + 1)] - onek[(size_t) gLo]; say -= gHi - gLo + 1; }
                cikis[b] = say > 0 ? (float) std::max (0.0, s / (double) say) : x[b];
            }
            cikis[0] = x[0];
        };
        ortalama (genis.data(), ref.data());
        for (int b = 0; b < B; ++b) kirp[(size_t) b] = std::min (genis[(size_t) b], ref[(size_t) b]);
        ortalama (kirp.data(), ref.data());

        const float kapi = tepe * std::pow (10.0f, -BIN_KAPI_DB / 10.0f);
        hedef[0] = 0.0f;
        for (int b = 1; b < B; ++b)
        {
            const float w = bantW[(size_t) b];
            const float g = genis[(size_t) b];
            float h = 0.0f;
            if (w > 0.0f && g >= kapi && g > 1.0e-20f)
            {
                const float e = 10.0f * std::log10 (g / std::max (ref[(size_t) b], 1.0e-30f)) - ESIK_DB;
                if (e > 0.0f) h = -std::min (aralik, depth * e) * w;
            }
            hedef[(size_t) b] = h;
        }
    }

    // 3) frekansta 3-bin üçgen → zamanda atak/bırakma → doğrusal kazanç
    bool var = false;
    double pay = 0.0, payda = 0.0;
    float onceki = hedef[0];
    for (int b = 0; b < B; ++b)
    {
        const float h = hedef[(size_t) b];
        const float sonraki = b + 1 < B ? hedef[(size_t) (b + 1)] : h;
        const float t = 0.25f * onceki + 0.5f * h + 0.25f * sonraki;
        onceki = h;
        float g = azaltma[(size_t) b];
        g = t < g ? t + aAtk * (g - t) : t + aBirak * (g - t);
        if (g > -1.0e-4f || ! std::isfinite (g)) g = 0.0f;
        azaltma[(size_t) b] = g;
        float G = 1.0f;
        if (g < 0.0f) { G = std::exp (g * LN10_20); var = true; }
        kazanc[b] = G;
        const double wP = (double) bantW[(size_t) b] * (double) P[(size_t) b];
        pay += wP * (double) G * (double) G;
        payda += wP;
    }
    azaltmaVar = var;
    sonAzaltmaDb = payda > 1.0e-20 ? (float) std::min (0.0, 10.0 * std::log10 (std::max (pay, 1.0e-30) / payda)) : 0.0f;
    return var;
}

} // namespace sweax
