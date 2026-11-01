// SWEAX NEXT — DSP/PitchCorrector.cpp
#include "PitchCorrector.h"
#include "../Chain/VocalChain.h"
#include <cmath>

namespace sweax
{

namespace
{
    inline float coefFromMs (double sr, double ms) noexcept { return (float) std::exp (-1.0 / (std::max (0.5, ms) * 0.001 * sr)); }
    inline float temiz (float x) noexcept { return (x > -1.0e6f && x < 1.0e6f) ? x : 0.0f; }

    constexpr int MAJOR[7] = { 0, 2, 4, 5, 7, 9, 11 };
    constexpr int MINOR[7] = { 0, 2, 3, 5, 7, 8, 10 };

    float snapCents (float hz, int key, int scale) noexcept
    {
        const double midi = 69.0 + 12.0 * std::log2 ((double) hz / 440.0);
        if (scale == 0)
        {
            const double sn = std::round (midi);
            return (float) (100.0 * (sn - midi));
        }
        const int* tbl = scale == 1 ? MAJOR : MINOR;
        double best = 1.0e9;
        for (int oct = -1; oct <= 1; ++oct)
            for (int i = 0; i < 7; ++i)
            {
                const double base = std::floor (midi / 12.0) * 12.0 + (double) oct * 12.0 + (double) key + (double) tbl[i];
                const double d = base - midi;
                if (std::fabs (d) < std::fabs (best)) best = d;
            }
        return (float) (100.0 * best);
    }
}

void PitchCorrector::prepare (double sampleRate, int maxBlock, int numChannels)
{
    const double k = sampleRate / 48000.0;
    baseN = std::max (16, (int) std::lround (48.0 * k));
    wNomN = std::max (128, (int) std::lround (1440.0 * k));   // 30 ms: N=2 çift-periyot 66 Hz'e kadar sığar
    latN  = baseN + wNomN / 2;
    prepareBase (sampleRate, maxBlock, numChannels, latN);
    setDryDelay (latN);
    // analiz: ~16 kHz desimasyonlu kopya — maliyet sr'dan bağımsız (denetim: 192k'da W·maxLag patlıyordu)
    anaK = std::max (1, (int) std::lround (sampleRate / 16000.0));
    anaSr = sampleRate / (double) anaK;
    tracker.prepare (anaSr, 60.0, 800.0, 0.15f);
    anaHop = std::max (64, (int) std::lround (0.016 * anaSr));            // ~16 ms
    anaRingN = tracker.frameSize() + anaHop + 8;
    anaRing.assign ((size_t) anaRingN, 0.0f);
    scratch.assign ((size_t) tracker.frameSize(), 0.0f);
    ringN = baseN + 2 * wNomN + maxBlock + 8;
    ringL.assign ((size_t) ringN, 0.0f);
    ringR.assign ((size_t) ringN, 0.0f);
    nStep = 1.0f / std::max (1.0f, (float) (0.015 * sampleRate));          // 15 ms nötr rampası
    paramsDirty = true;
    resetWet();
}

void PitchCorrector::updateParams() noexcept
{
    cur = pending;
    atkCoef = coefFromMs (sr, cur.speedMs);
    relCoef = coefFromMs (sr, 40.0);
    paramsDirty = false;
}

void PitchCorrector::analyzePitch() noexcept
{
    const int need = tracker.frameSize();
    int rp = anaWp - need; while (rp < 0) rp += anaRingN;
    for (int i = 0; i < need; ++i)
    {
        scratch[(size_t) i] = anaRing[(size_t) rp];
        if (++rp >= anaRingN) rp = 0;
    }
    const auto res = tracker.analyze (scratch.data(), need);
    voiced = res.voiced && res.hz > 60.0f && res.hz < 800.0f;
    f0 = res.hz;
    if (voiced && cur.strength > 0.001f)
        targetCents = cur.strength * snapCents (f0, cur.key, cur.scale);
    else
        targetCents = 0.0f;
    targetCents = std::clamp (targetCents, -150.0f, 150.0f);   // oktav hatası koruması
}

void PitchCorrector::processWet (float* const* ch, int numCh, int n) noexcept
{
    if (paramsDirty) updateParams();
    const int c = std::min (numCh, chans);
    for (int i = 0; i < n; ++i)
    {
        const float l = temiz (ch[0][i]);
        const float r = c > 1 ? temiz (ch[1][i]) : l;
        pushSample (l, r);

        // cent yumuşatma (atak = speed; bırakma = 40 ms)
        const float coefC = std::fabs (targetCents) > std::fabs (offsetCents) ? atkCoef : relCoef;
        offsetCents = targetCents + coefC * (offsetCents - targetCents);

        // pencere: kaynak periyodunun TAM SAYI ÇİFT katı (ölçüldü: kesirli pencere ek-yeri faz kayması;
        // N tekken temel crossfade ortasında π fazla söner) — N=1 yalnız <66 Hz kaçış yolu
        if (voiced && f0 > 60.0f)
        {
            const float T = (float) sr / f0;
            int N = 2 * std::clamp ((int) std::lround (0.25f * (float) wNomN / T), 1, 4);
            float wTgt = (float) N * T;
            if (wTgt > (float) wNomN) { N = 2; wTgt = 2.0f * T; }
            if (wTgt > (float) wNomN) { N = 1; wTgt = T; }
            wTgt = std::clamp (wTgt, 64.0f, (float) wNomN);
            wEff += 0.002f * (wTgt - wEff);
        }
        const double ratio = std::pow (2.0, (double) offsetCents / 1200.0);

        // NÖTR karışım rampası (M2.2 denetim düzeltmesi): fazı 0.5'e sürmek tap gecikmesini ~3 örnek/örnek
        // kaydırıp her düzeltme bırakışında cıvıltı yapıyordu — bunun yerine iki-tap ↔ tek-tap(latN) 15 ms
        // karışım; tam nötrde faz 0.5'e oturur (tek tap konumu) → yeniden giriş dikişsiz (g1 = 1).
        const bool notr = std::fabs (offsetCents) < 0.5f;
        nBlend = notr ? std::min (1.0f, nBlend + nStep) : std::max (0.0f, nBlend - nStep);
        if (nBlend >= 1.0f)
            phase = 0.5;
        else
        {
            phase += (1.0 - ratio) / (double) wEff;
            while (phase >= 1.0) phase -= 1.0;
            while (phase < 0.0) phase += 1.0;
        }
        const double p1 = phase;
        double p2 = phase + 0.5; if (p2 >= 1.0) p2 -= 1.0;
        const double center = (double) latN - 0.5 * (double) wEff;
        const double d1 = std::max (1.0, center + p1 * (double) wEff);
        const double d2 = std::max (1.0, center + p2 * (double) wEff);
        const float g1 = (float) (1.0 - std::fabs (2.0 * p1 - 1.0));
        const float g2 = 1.0f - g1;

        float outL = g1 * readTap (ringL, d1) + g2 * readTap (ringL, d2);
        float outR = c > 1 ? g1 * readTap (ringR, d1) + g2 * readTap (ringR, d2) : outL;
        if (nBlend > 0.0f)
        {
            const float sL = readTap (ringL, (double) latN);
            outL += (sL - outL) * nBlend;
            if (c > 1) { const float sR = readTap (ringR, (double) latN); outR += (sR - outR) * nBlend; }
            else outR = outL;
        }
        ch[0][i] = outL;
        if (c > 1) ch[1][i] = outR;
    }
}

void PitchCorrector::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    const int c = std::min (numCh, chans);
    for (int i = 0; i < n; ++i)
    {
        const float l = temiz (ch[0][i]);                       // denetim: sanitize'siz push NaN'ı halkaya taşıyordu
        const float r = c > 1 ? temiz (ch[1][i]) : l;
        pushSample (l, r);
    }
    offsetCents = 0.0f; targetCents = 0.0f; phase = 0.5; nBlend = 1.0f;
}

void PitchCorrector::resetWet()
{
    std::fill (ringL.begin(), ringL.end(), 0.0f);
    std::fill (ringR.begin(), ringR.end(), 0.0f);
    std::fill (anaRing.begin(), anaRing.end(), 0.0f);
    wp = 0; anaWp = 0; anaAcc = 0.0f; anaAccN = 0; sinceAnalyze = 0;
    tracker.reset();
    f0 = 0.0f; voiced = false;
    offsetCents = targetCents = 0.0f;
    phase = 0.5; wEff = (float) wNomN; nBlend = 1.0f;
}

} // namespace sweax
