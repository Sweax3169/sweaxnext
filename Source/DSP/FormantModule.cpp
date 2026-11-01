// SWEAX NEXT — DSP/FormantModule.cpp
#include "FormantModule.h"
#include "../Chain/VocalChain.h"

namespace sweax
{

namespace
{
    constexpr float LN10_20 = 0.11512925f;      // ln(10)/20
    constexpr float PI_F = 3.14159265f;
}

void FormantModule::prepare (double sampleRate, int maxBlock, int numChannels)
{
    // motorun kelepçesiyle AYNI sr: ikisi farklı N seçerse kepstrum dizileri taşar (bkz. SpektralMotor.h)
    const double s = SpektralMotor::gecerliSr (sampleRate);
    prepareBase (s, maxBlock, numChannels, SpektralMotor::boyutSec (s) - 1);
    motor.prepare (sr, chans);
    const int NMax = SpektralMotor::boyutSec (sr);
    kRe.assign ((size_t) NMax, 0.0f);
    kIm.assign ((size_t) NMax, 0.0f);
    const size_t B = (size_t) motor.maxBins();
    logGen.assign (B, 0.0f);
    zarf.assign (B, 0.0f);
    lnG.assign (B, 0.0f);
    gucYum.assign (B, 0.0f);
    lnGYum.assign (B, 0.0f);
    pending = cur = FormantParams {};
    setQuality (mod);
    resetWet();
}

void FormantModule::setQuality (Quality q) noexcept
{
    mod = q;
    motor.setQuality (q);          // boyut değişimi ERTELENİR (tıksız); bin başına durum sonN ile sıfırlanır
    setDryDelay (latencySamples (q));
}

void FormantModule::resetWet()
{
    motor.reset();
    durumSifirla();
    bosta = true;                                   // ilk processWet hedeflerden başlar (rampasız)
}

void FormantModule::durumSifirla() noexcept
{
    const float a = std::pow (2.0f, std::clamp (pending.shiftSt, -6.0f, 6.0f) / 12.0f);
    alfa = std::isfinite (a) ? a : 1.0f;             // açılışta hedefte başla (kayma rampası yok)
    mixCur = std::isfinite (pending.mix) ? std::clamp (pending.mix, 0.0f, 1.0f) : 1.0f;
    seviyeDb = 0.0f;
    lnGEnb = 0.0f;
    kesim = (float) (LIFTER_MS * 0.001 * sr);
    sonN = -1;                                       // ilk çerçeve yumuşatma durumunu yeniden tohumlar
    yumSayac = 0; sonOtumlu = false;
    std::fill (lnGYum.begin(), lnGYum.end(), 0.0f);
    std::fill (gucYum.begin(), gucYum.end(), 0.0f);
}

void FormantModule::processWet (float* const* ch, int numCh, int n) noexcept
{
    cur = pending;
    if (! std::isfinite (cur.shiftSt)) cur.shiftSt = 0.0f;
    if (! std::isfinite (cur.mix)) cur.mix = 1.0f;
    cur.shiftSt = std::clamp (cur.shiftSt, -6.0f, 6.0f);
    cur.mix = std::clamp (cur.mix, 0.0f, 1.0f);
    if (bosta) { durumSifirla(); bosta = false; }
    // hedef nötr ve TÜM yumuşatılmış durum sönmüş → analiz FFT'si bile gereksiz (birim yol, gecikme hizalı)
    const bool notr = (cur.shiftSt == 0.0f && notrDurum()) || (cur.mix == 0.0f && mixCur == 0.0f);
    motor.isle (ch, ch, numCh, n, notr ? nullptr : static_cast<SpektralIsleyici*> (this));
}

void FormantModule::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    motor.isle (ch, nullptr, numCh, n, nullptr);
    bosta = true;
}

// logGen[0, B) → zarf[0, B)  (true envelope, perdeye uyarlı lifter)
void FormantModule::zarfHesapla (int N, int B, float hedefKesim, double hopSn, int yinelemeSay) noexcept
{
    float* re = kRe.data();
    float* im = kIm.data();
    const float s = 1.0f / (float) N;
    const float a = (float) std::exp (-hopSn / (KAYMA_MS * 0.001));
    kesim = hedefKesim < kesim ? hedefKesim : hedefKesim + a * (kesim - hedefKesim);   // düşüş anında (sızıntı yok), artış yumuşak
    if (! std::isfinite (kesim)) kesim = (float) (LIFTER_MS * 0.001 * sr);
    const float n1 = 0.75f * kesim, n2 = 1.25f * kesim;
    for (int yineleme = 0; yineleme <= yinelemeSay; ++yineleme)
    {
        re[0] = logGen[0];
        for (int b = 1; b < B - 1; ++b) { re[b] = logGen[(size_t) b]; re[N - b] = logGen[(size_t) b]; }
        re[N / 2] = logGen[(size_t) (B - 1)];
        std::fill (im, im + N, 0.0f);
        motor.fft().ileri (re, im, N);                                  // gerçek simetrik → gerçek kepstrum (×N)
        for (int q = 0; q <= N / 2; ++q)
        {
            const float fq = (float) q;
            const float l = fq <= n1 ? 1.0f : (fq < n2 ? 0.5f + 0.5f * std::cos (PI_F * (fq - n1) / (n2 - n1)) : 0.0f);
            re[q] *= l;
            if (q > 0 && q < N / 2) re[N - q] *= l;
        }
        std::fill (im, im + N, 0.0f);
        motor.fft().ileri (re, im, N);                                  // simetrik → ters dönüşümle aynı (×N)
        for (int b = 0; b < B; ++b) zarf[(size_t) b] = re[b] * s;
        if (yineleme < yinelemeSay)                                     // true envelope: tepelerin üstüne yaslan
            for (int b = 0; b < B; ++b) logGen[(size_t) b] = std::max (logGen[(size_t) b], zarf[(size_t) b]);
    }
}

bool FormantModule::cerceveIsle (const SpektralCerceve& c, float* kazanc) noexcept
{
    const int B = c.bins, N = c.N;
    const double hopSn = (double) c.hop / c.sr;
    // kayma ve mix yumuşatma (30 ms, hop katsayısı)
    const float aKayma = (float) std::exp (-hopSn / (KAYMA_MS * 0.001));
    const float alfaHedef = std::pow (2.0f, cur.shiftSt / 12.0f);
    alfa = alfaHedef + aKayma * (alfa - alfaHedef);
    if (std::fabs (alfa - alfaHedef) < 1.0e-5f || ! std::isfinite (alfa)) alfa = alfaHedef;
    mixCur = cur.mix + aKayma * (mixCur - cur.mix);
    if (std::fabs (mixCur - cur.mix) < 1.0e-4f || ! std::isfinite (mixCur)) mixCur = cur.mix;
    const float aSeviye = (float) std::exp (-hopSn / (SEVIYE_MS * 0.001));

    if (mixCur <= 0.0f) { lnGEnb = 0.0f; seviyeDb = 0.0f; return false; }
    // nötr: kayma oturmuş VE seviye telafisi + yumuşatılmış kazanç eğrisi sönmüş (yarıda kesilirse basamak olur)
    if (alfa == 1.0f && std::fabs (seviyeDb) < 0.005f && lnGEnb < 6.0e-4f)
    { seviyeDb = 0.0f; lnGEnb = 0.0f; return false; }

    // motorun boyutu değiştiyse bin başına durum anlamını yitirir → yeniden tohumla
    const bool tohum = (N != sonN);
    if (tohum)
    {
        sonN = N;
        std::fill (lnGYum.begin(), lnGYum.end(), 0.0f);
        lnGEnb = 0.0f; seviyeDb = 0.0f; yumSayac = 0;
        kesim = (float) std::min (LIFTER_MS * 0.001 * sr, 0.25 * (double) N);
    }

    // ZARF YUMUŞATICI — zaman sabiti ÖTÜMLÜLÜĞE bağlı (bkz. başlık): ötümlüde 25 ms (ünlü geçişleri izlensin),
    // ötümsüzde 120 ms (periodogram gürültüsü kazanç eğrisine sızmasın; hız buralarda zaten gereksiz).
    // Durum değişiminde BÜYÜYEN PENCERE ısınması (a = sayac/(sayac+1)) → basamak/tık yok, hedefe yumuşak oturur.
    const bool otumlu = c.T0 > 0.0f;
    if (otumlu != sonOtumlu) { sonOtumlu = otumlu; yumSayac = 0; }
    const float aHedef = (float) std::exp (-hopSn / ((otumlu ? ZARF_MS : ZARF_UZUN_MS) * 0.001));
    const float aZarf = tohum ? 0.0f : std::min (aHedef, (float) yumSayac / (float) (yumSayac + 1));
    if (yumSayac < 1 << 20) ++yumSayac;

    // 1) güç spektrumunu ZAMANDA yumuşat: kepstral zarf tahmini periodogram gürültüsünü doğrudan çıkışa taşıyordu
    const float* P = c.guc;
    float tepe = 0.0f;
    for (int b = 0; b < B; ++b)
    {
        float v = P[(size_t) b] + aZarf * (gucYum[(size_t) b] - P[(size_t) b]);
        if (! (v >= 0.0f && v < 1.0e30f)) v = 0.0f;                     // NaN/Inf sigortası
        gucYum[(size_t) b] = v;
        tepe = std::max (tepe, v);
    }

    // 2) hedef ln kazanç eğrisi (sessizlikte 0 → eğri yumuşakça sönsün, basamak yok)
    if (c.rmsDb < SESSIZ_DBFS || ! (tepe > 1.0e-30f && tepe < 1.0e30f))
    {
        std::fill (lnG.begin(), lnG.begin() + B, 0.0f);
    }
    else
    {
        const float taban = tepe * std::pow (10.0f, LOG_TABAN_DB / 10.0f);
        for (int b = 0; b < B; ++b) logGen[(size_t) b] = 0.5f * std::log (std::max (gucYum[(size_t) b], taban));
        // perdeye uyarlı lifter — T0 motorun ORTAK algılayıcısından (tek çerçeve penceresine bağlı değil)
        const float hi = (float) std::min (LIFTER_MAX_MS * 0.001 * sr, 0.25 * (double) N);
        const float lo = std::min ((float) (LIFTER_MIN_MS * 0.001 * sr), hi);
        const float hedefKesim = otumlu
            ? std::clamp (KESIM_ORANI * c.T0, lo, hi)
            : (float) std::min (LIFTER_MS * 0.001 * sr, 0.25 * (double) N);
        // true-envelope yinelemeleri YALNIZ ötümlüde: harmonik TEPELERE yaslanmak içindir. Ötümsüzde tepe yoktur,
        // max() yalnız periodogram gürültüsünün üst kuyruğunu kovalar → kazanç eğrisi zamanda titrer (ölçüldü).
        zarfHesapla (N, B, hedefKesim, hopSn, otumlu ? ZARF_YINELEME : 0);

        const float kelepce = KELEPCE_DB * LN10_20;
        const float tersAlfa = 1.0f / alfa;
        for (int b = 0; b < B; ++b)
        {
            const float x = (float) b * tersAlfa;
            float Ep;
            const int i0 = (int) x;
            if (i0 >= B - 1) Ep = zarf[(size_t) (B - 1)];
            else { const float fr = x - (float) i0; Ep = zarf[(size_t) i0] + fr * (zarf[(size_t) (i0 + 1)] - zarf[(size_t) i0]); }
            lnG[(size_t) b] = std::clamp (Ep - zarf[(size_t) b], -kelepce, kelepce);
        }
    }

    // 3) frekansta 3-bin üçgen → ZAMANDA bin başına ZARF_MS tek kutup → enerji oranı
    double pay = 0.0, payda = 0.0;
    float enb = 0.0f;
    float onceki = lnG[0];
    for (int b = 0; b < B; ++b)
    {
        const float h = lnG[(size_t) b];
        const float sonraki = b + 1 < B ? lnG[(size_t) (b + 1)] : h;
        const float ls = 0.25f * onceki + 0.5f * h + 0.25f * sonraki;
        onceki = h;
        float y = ls + aZarf * (lnGYum[(size_t) b] - ls);
        if (! std::isfinite (y)) y = 0.0f;
        lnGYum[(size_t) b] = y;
        enb = std::max (enb, std::fabs (y));
        const double p = (double) gucYum[(size_t) b];
        pay += p * std::exp (2.0 * (double) y);
        payda += p;
    }
    lnGEnb = enb;

    // 4) seviye koruma + mix (spektral alanda: G_eff = 1 + mix·(G − 1), faz aynı → comb yok)
    const float oranDb = payda > 1.0e-30
        ? std::clamp ((float) (10.0 * std::log10 (std::max (pay, 1.0e-30) / payda)), -SEVIYE_SINIR_DB, SEVIYE_SINIR_DB)
        : 0.0f;
    seviyeDb = oranDb + aSeviye * (seviyeDb - oranDb);
    if (! std::isfinite (seviyeDb)) seviyeDb = 0.0f;
    const float telafi = -seviyeDb * LN10_20;
    for (int b = 0; b < B; ++b)
    {
        const float G = std::exp (lnGYum[(size_t) b] + telafi);
        kazanc[b] = 1.0f + mixCur * (G - 1.0f);
    }
    return true;
}

} // namespace sweax
