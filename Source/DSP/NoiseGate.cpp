// SWEAX NEXT — DSP/NoiseGate.cpp
#include "NoiseGate.h"
#include "../Chain/VocalChain.h"   // Quality
#include <cmath>

namespace sweax
{

namespace
{
    inline float coefFromMs (double sr, double ms) noexcept { return (float) std::exp (-1.0 / (std::max (0.01, ms) * 0.001 * sr)); }
    inline float dbToLin (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
}

void NoiseGate::prepare (double sampleRate, int maxBlock, int numChannels)
{
    const int worst = (int) std::lround (LOOKAHEAD_MS * 0.001 * sampleRate);
    prepareBase (sampleRate, maxBlock, numChannels, worst);
    for (auto& d : audioDelay) d.prepare (worst, 0);
    paramsDirty = true;
    setQuality (Quality::Yuksek);
    resetWet();
}

int NoiseGate::latencySamples (Quality mode) const noexcept
{
    return mode == Quality::Canli ? 0 : (int) std::lround (LOOKAHEAD_MS * 0.001 * sr);
}

void NoiseGate::setQuality (Quality mode) noexcept
{
    activeDelay = latencySamples (mode);
    for (auto& d : audioDelay) d.setDelay (activeDelay);
    setDryDelay (activeDelay);
}

void NoiseGate::updateParams() noexcept
{
    cur = pending;
    envRise = coefFromMs (sr, 0.3);
    envFall = coefFromMs (sr, 8.0);
    openLin = dbToLin (cur.threshDb);
    closeLin = dbToLin (cur.threshDb - std::max (0.0f, cur.hystDb));
    gClosed = dbToLin (-std::max (0.0f, cur.rangeDb));
    atkCoef = coefFromMs (sr, std::max (0.1f, cur.attackMs));
    relCoef = coefFromMs (sr, std::max (5.0f, cur.releaseMs));
    holdN = (int) std::lround (std::max (0.0f, cur.holdMs) * 0.001 * sr);
    paramsDirty = false;
}

void NoiseGate::processWet (float* const* ch, int numCh, int n) noexcept
{
    if (paramsDirty) updateParams();
    const int c = std::min (numCh, chans);
    for (int i = 0; i < n; ++i)
    {
        // dedektör: GECİKMEMİŞ giriş (lookahead = dedektör önde okur)
        float pk = 0.0f;
        for (int k = 0; k < c; ++k) { const float a = std::fabs (ch[k][i]); if (a > pk) pk = a; }
        if (! (pk < 1.0e6f)) pk = 0.0f;                       // NaN/Inf → 0 (kapı kapanır, patlamaz)
        const float coef = pk > env ? envRise : envFall;
        env = pk + coef * (env - pk);

        // histerezisli durum
        if (env >= openLin) { open = true; holdLeft = holdN; }
        else if (env < closeLin)
        {
            if (holdLeft > 0) --holdLeft;
            else open = false;
        }
        // (aralıkta: durumunu korur)

        const float gT = open ? 1.0f : gClosed;
        const float gc = gT > g ? atkCoef : relCoef;
        g = gT + gc * (g - gT);

        // ses yolu: gecikmiş örneğe kazanç
        for (int k = 0; k < c; ++k)
        {
            const float d = audioDelay[(size_t) k].processSample (ch[k][i]);
            ch[k][i] = d * g;
        }
    }
    grDbOut = g > 1.0e-6f ? 20.0f * std::log10 (g) : -120.0f;
}

void NoiseGate::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    const int c = std::min (numCh, chans);
    for (int k = 0; k < c; ++k)
        for (int i = 0; i < n; ++i) audioDelay[(size_t) k].processSample (ch[k][i]);
    grDbOut = 0.0f;
}

void NoiseGate::resetWet()
{
    for (auto& d : audioDelay) d.reset();
    env = 0.0f; g = 1.0f; open = true; holdLeft = 0; grDbOut = 0.0f;
}

} // namespace sweax
