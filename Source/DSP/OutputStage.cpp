// SWEAX NEXT — DSP/OutputStage.cpp
#include "OutputStage.h"
#include "../Chain/VocalChain.h"
#include <cmath>
#include <algorithm>

namespace sweax
{

namespace
{
    constexpr double PI = 3.14159265358979323846;
    double besselI0 (double x) noexcept
    {
        double sum = 1.0, term = 1.0; const double y = x * x * 0.25;
        for (int k = 1; k < 60; ++k) { term *= y / ((double) k * (double) k); sum += term; if (term < 1.0e-14 * sum) break; }
        return sum;
    }
    inline float coefFromMs (double sr, double ms) noexcept { return (float) std::exp (-1.0 / (std::max (0.01, ms) * 0.001 * sr)); }
}

// yarım-bant: kesim 0.25 (giriş hızının), merkez 0.5; çift ofset tapları sıfır — Kaiser pencereli sinc
void OutputStage::HalfBand::design (double beta) noexcept
{
    const double i0b = besselI0 (beta);
    double sum = 0.0;
    for (int k = 0; k < HB_LEN; ++k)
    {
        const double t = (double) k - (double) HB_DELAY;
        const double sinc = (std::fabs (t) < 1e-9) ? 0.5 : std::sin (PI * t * 0.5) / (PI * t);
        const double r = t / (double) (HB_DELAY + 1);
        const double w = (std::fabs (r) >= 1.0) ? 0.0 : besselI0 (beta * std::sqrt (1.0 - r * r)) / i0b;
        h[(size_t) k] = (float) (sinc * w);
        sum += h[(size_t) k];
    }
    for (auto& v : h) v = (float) (v / sum);               // DC birim
    reset();
}

void OutputStage::prepare (double sampleRate, int maxBlock, int numChannels)
{
    lookN = std::max (8, (int) std::lround (LOOKAHEAD_MS * 0.001 * sampleRate));
    const int worst = std::max (latencySamples (Quality::Canli), latencySamples (Quality::Yuksek));
    prepareBase (sampleRate, maxBlock, numChannels, worst);
    for (auto& d : audioDelay) d.prepare (lookN - 1 + (TAPS / 2 - 1), lookN - 1 + (TAPS / 2 - 1));
    for (auto& d : osDelay) d.prepare (OS * lookN, OS * lookN);
    setDryDelay (latencySamples (Quality::Canli));
    // CANLI TP tahmin FIR'ı (Loudness ile aynı tasarım)
    {
        constexpr double beta = 5.65;
        const double center = (double) (TAPS / 2 - 1), halfWidth = (double) TAPS * 0.5, i0b = besselI0 (beta);
        for (int p = 0; p < OVERSAMPLE; ++p)
        {
            const double tau = center + (double) p / OVERSAMPLE; double sum = 0.0;
            for (int k = 0; k < TAPS; ++k)
            {
                const double t = (double) k - tau;
                const double sinc = (std::fabs (t) < 1.0e-9) ? 1.0 : std::sin (PI * t) / (PI * t);
                const double r = t / halfWidth;
                const double w = (std::fabs (r) >= 1.0) ? 0.0 : besselI0 (beta * std::sqrt (1.0 - r * r)) / i0b;
                fir[(size_t) p][(size_t) k] = (float) (sinc * w); sum += sinc * w;
            }
            for (int k = 0; k < TAPS; ++k) fir[(size_t) p][(size_t) k] = (float) (fir[(size_t) p][(size_t) k] / sum);
        }
    }
    for (auto& f : up1) f.design (7.0);
    for (auto& f : up2) f.design (7.0);
    for (auto& f : dn2) f.design (7.0);
    for (auto& f : dn1) f.design (7.0);
    os2.assign ((size_t) maxBlock * 2, 0.0f);
    os4a.assign ((size_t) maxBlock * 4, 0.0f);
    os4b.assign ((size_t) maxBlock * 4, 0.0f);
    gain4.assign ((size_t) maxBlock * 4, 1.0f);
    const int maxL = OS * lookN;
    reqRing.assign ((size_t) maxL, 1.0f); prefMin.assign ((size_t) maxL, 1.0f); sufMin.assign ((size_t) maxL, 1.0f);
    avgRing.assign ((size_t) maxL, 1.0f);
    holdN = (int) std::lround (0.010 * sr);                // taban hızda örnek sayısı; OS yolunda ×OS
    osActive = false; L = lookN;
    pending = cur = OutputParams {};
    paramsDirty = true;
    resetWet();
}

int OutputStage::latencySamples (Quality mode) const noexcept
{
    return mode == Quality::Canli ? lookN - 1 + (TAPS / 2 - 1) : lookN + 2 * (HB_DELAY / 2 + HB_DELAY / OS);
}

void OutputStage::setQuality (Quality mode) noexcept
{
    osActive = mode != Quality::Canli;
    L = osActive ? OS * lookN : lookN;
    setDryDelay (latencySamples (mode));
    resetWet();
    paramsDirty = true;                                   // relCoef/hold OS hızına göre yeniden
}

void OutputStage::resetWet()
{
    for (auto& d : audioDelay) d.reset();
    for (auto& d : osDelay) d.reset();
    for (auto& h : hist) h.fill (0.0f);
    hpos.fill (0);
    for (auto& f : up1) f.reset();
    for (auto& f : up2) f.reset();
    for (auto& f : dn2) f.reset();
    for (auto& f : dn1) f.reset();
    std::fill (reqRing.begin(), reqRing.end(), 1.0f);
    std::fill (prefMin.begin(), prefMin.end(), 1.0f);
    std::fill (sufMin.begin(), sufMin.end(), 1.0f);
    std::fill (avgRing.begin(), avgRing.end(), 1.0f);
    ringPos = 0; avgSum = (double) L; gSmoothDb = 0.0f; holdLeft = 0; grOut = 0.0f;
}

float OutputStage::truePeakEstimate (int c, float x) noexcept
{
    auto& h = hist[(size_t) c]; int& wp = hpos[(size_t) c];
    h[(size_t) wp] = x; wp = (wp + 1) % TAPS;
    float pk = std::fabs (h[(size_t) ((wp + TAPS / 2 - 1) % TAPS)]);
    for (int p = 1; p < OVERSAMPLE; ++p)
    {
        float acc = 0.0f; const auto& f = fir[(size_t) p];
        for (int k = 0; k < TAPS; ++k) acc += f[(size_t) k] * h[(size_t) ((wp + k) % TAPS)];
        pk = std::max (pk, std::fabs (acc));
    }
    return pk;
}

// tek örnek çekirdek: gerekli kazanç → kayan min (van Herk) → L-ort. → release/hold → uygulanacak kazanç (lineer)
inline float OutputStage::coreGain (float req) noexcept
{
    reqRing[(size_t) ringPos] = req;
    prefMin[(size_t) ringPos] = (ringPos == 0) ? req : std::min (prefMin[(size_t) ringPos - 1], req);
    if (ringPos == L - 1)
    {
        sufMin[(size_t) (L - 1)] = reqRing[(size_t) (L - 1)];
        for (int j = L - 2; j >= 0; --j) sufMin[(size_t) j] = std::min (sufMin[(size_t) j + 1], reqRing[(size_t) j]);
    }
    const float slidingMin = (ringPos == L - 1) ? prefMin[(size_t) ringPos]
                                                : std::min (prefMin[(size_t) ringPos], sufMin[(size_t) ringPos + 1]);
    ringPos = (ringPos + 1) % L;
    avgSum += (double) slidingMin - (double) avgRing[(size_t) ringPos];
    avgRing[(size_t) ringPos] = slidingMin;
    const float gAttack = (float) (avgSum / L);
    float gAttackDb = 20.0f * std::log10 (std::max (gAttack, 1.0e-5f));
    gAttackDb = std::max (gAttackDb, -40.0f);
    if (gAttackDb <= gSmoothDb + 1.0e-4f) { gSmoothDb = std::min (gSmoothDb, gAttackDb); holdLeft = holdN * (osActive ? OS : 1); }
    else if (holdLeft > 0) --holdLeft;
    else gSmoothDb = gAttackDb + relCoef * (gSmoothDb - gAttackDb);
    const float gDb = std::min (gSmoothDb, gAttackDb);
    grOut = gDb;
    return std::pow (10.0f, gDb * 0.05f);
}

void OutputStage::processBase (float* const* ch, int numCh, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        float pk = 0.0f;
        for (int k = 0; k < numCh; ++k) pk = std::max (pk, truePeakEstimate (k, ch[k][i]));
        if (! std::isfinite (pk)) { for (auto& h : hist) h.fill (0.0f); pk = 0.0f; }
        const float req = pk > ceilLin ? std::max (ceilLin / pk, 0.03162f) : 1.0f;
        const float g = coreGain (req);
        for (int k = 0; k < numCh; ++k)
            ch[k][i] = audioDelay[(size_t) k].processSample (ch[k][i]) * g;
    }
}

void OutputStage::processOs (float* const* ch, int numCh, int n) noexcept
{
    const int n4 = n * OS;
    // 1) her kanalı 4×'e çıkar (zero-stuff + HB, ×2 kazanç kademede) → os4b'ye kanal kanal; tepe birleşik
    std::fill (gain4.begin(), gain4.begin() + n4, 0.0f);   // önce tepe toplama tamponu olarak kullan
    for (int k = 0; k < numCh; ++k)
    {
        // 1× → 2× (girişte NaN/Inf 0'a indirgenir: durum zehirlenmesi yok; son savunma processor'da)
        for (int i = 0; i < n; ++i)
        {
            float x = ch[k][i];
            if (! std::isfinite (x)) x = 0.0f;
            os2[(size_t) (2 * i)]     = 2.0f * up1[(size_t) k].tick (x);
            os2[(size_t) (2 * i + 1)] = 2.0f * up1[(size_t) k].tick (0.0f);
        }
        // 2× → 4×
        float* dst = (k == 0) ? os4a.data() : os4b.data();
        for (int i = 0; i < 2 * n; ++i)
        {
            dst[2 * i]     = 2.0f * up2[(size_t) k].tick (os2[(size_t) i]);
            dst[2 * i + 1] = 2.0f * up2[(size_t) k].tick (0.0f);
        }
        for (int i = 0; i < n4; ++i)
            gain4[(size_t) i] = std::max (gain4[(size_t) i], std::fabs (dst[i]));
    }
    // 2) OS hızında kazanç dizisi (stereo link) — 4×'te örnek tepeleri ≈ gerçek tepeler (ISP hatası < 0.05 dB)
    for (int i = 0; i < n4; ++i)
    {
        const float pk = gain4[(size_t) i];
        const float req = pk > ceilLin ? std::max (ceilLin / pk, 0.03162f) : 1.0f;
        gain4[(size_t) i] = coreGain (req);
    }
    // 3) gecikme + kazanç + aşağı örnekleme (HB @4× → her 2. → HB @2× → her 2.)
    for (int k = 0; k < numCh; ++k)
    {
        float* src = (k == 0) ? os4a.data() : os4b.data();
        for (int i = 0; i < n4; ++i)
        {
            const float d = osDelay[(size_t) k].processSample (src[i]);
            src[i] = d * gain4[(size_t) i];
        }
        for (int i = 0; i < 2 * n; ++i)
        {
            const float a = dn2[(size_t) k].tick (src[2 * i]);
            dn2[(size_t) k].tick (src[2 * i + 1]);
            os2[(size_t) i] = a;
        }
        for (int i = 0; i < n; ++i)
        {
            const float a = dn1[(size_t) k].tick (os2[(size_t) (2 * i)]);
            dn1[(size_t) k].tick (os2[(size_t) (2 * i + 1)]);
            ch[k][i] = a;
        }
    }
}

void OutputStage::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    // gecikme hatları + filtre durumları beslenir (kazanç 1)
    if (! osActive)
    {
        for (int i = 0; i < n; ++i)
            for (int k = 0; k < numCh; ++k)
            {
                audioDelay[(size_t) k].processSample (ch[k][i]);
                auto& h = hist[(size_t) k]; int& wp = hpos[(size_t) k];
                h[(size_t) wp] = ch[k][i]; wp = (wp + 1) % TAPS;
            }
        return;
    }
    for (int k = 0; k < numCh; ++k)
        for (int i = 0; i < n; ++i)
        {
            float x = ch[k][i];
            if (! std::isfinite (x)) x = 0.0f;
            const float u2[2] = { 2.0f * up1[(size_t) k].tick (x), 2.0f * up1[(size_t) k].tick (0.0f) };
            for (int j = 0; j < 2; ++j)
            {
                const float c1 = 2.0f * up2[(size_t) k].tick (u2[j]);
                const float c2 = 2.0f * up2[(size_t) k].tick (0.0f);
                const float e1 = dn2[(size_t) k].tick (osDelay[(size_t) k].processSample (c1));
                dn2[(size_t) k].tick (osDelay[(size_t) k].processSample (c2));
                dn1[(size_t) k].tick (e1);
            }
        }
}

void OutputStage::processWet (float* const* ch, int numCh, int n) noexcept
{
    if (paramsDirty)
    {
        cur = pending; paramsDirty = false;
        ceilLin = std::pow (10.0f, std::clamp (cur.ceilingDbTp, -6.0f, 0.0f) * 0.05f);
        relCoef = coefFromMs (osActive ? sr * OS : sr, std::max (50.0f, cur.releaseMs));
    }
    if (osActive) processOs (ch, numCh, n);
    else processBase (ch, numCh, n);
    if (! std::isfinite (gSmoothDb)) { gSmoothDb = 0.0f; avgSum = (double) L; std::fill (avgRing.begin(), avgRing.end(), 1.0f); }
}

} // namespace sweax
