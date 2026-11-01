// SWEAX NEXT — DSP/Compressor.cpp
#include "Compressor.h"
#include "../Chain/VocalChain.h"
#include <cmath>

namespace sweax
{

namespace
{
    inline float coefFromMs (double sr, double ms) noexcept { return (float) std::exp (-1.0 / (std::max (0.01, ms) * 0.001 * sr)); }
    inline float dbOf (float lin) noexcept { return 20.0f * std::log10 (std::max (lin, 1.0e-7f)); }
}

void Compressor::prepare (double sampleRate, int maxBlock, int numChannels)
{
    const int worst = (int) std::ceil (LOOKAHEAD_MAX_MS * 0.001 * sampleRate);
    prepareBase (sampleRate, maxBlock, numChannels, worst);
    for (auto& d : audioDelay) d.prepare (worst, 0);
    gainDelay.prepare (worst, 0);
    activeDelay = 0;
    makeup.prepare (sr, 20.0); makeup.setCurrentAndTargetDb (0.0f);
    mixS.prepare (sr, 20.0); mixS.setCurrentAndTarget (1.0f);
    scHpfLast = -1.0f;
    pending = cur = CompParams {};
    paramsDirty = true;
    resetWet();
}

int Compressor::latencySamples (Quality mode) const noexcept
{
    return mode == Quality::Canli ? 0 : (int) std::lround (LOOKAHEAD_MAX_MS * 0.001 * sr);
}

void Compressor::setQuality (Quality mode) noexcept
{
    activeDelay = latencySamples (mode);
    for (auto& d : audioDelay) d.setDelay (activeDelay);
    setDryDelay (activeDelay);
    paramsDirty = true;                 // kazanç gecikmesi yeniden hesaplanır
}

// kapalıyken gecikme hatları girişle beslenir (yeniden açılış bayat ses çalmaz); dedektör/zarf dinlenir
void Compressor::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        for (int k = 0; k < numCh; ++k) audioDelay[(size_t) k].processSample (ch[k][i]);
        gainDelay.processSample (0.0f);
    }
    grDb = grDbOut = 0.0f; peakEnv = rmsEnv = 0.0f;
}

void Compressor::resetWet()
{
    for (auto& d : audioDelay) d.reset();
    gainDelay.reset();
    scHpf.reset();
    peakEnv = rmsEnv = 0.0f; grDb = grDbOut = 0.0f; holdLeft = 0;
}

void Compressor::processWet (float* const* ch, int numCh, int n) noexcept
{
    if (paramsDirty)
    {
        cur = pending; paramsDirty = false;
        // character çarpanları (belgelenmiş)
        float knee = cur.kneeDb, atk = cur.attackMs, rel = cur.releaseMs; double holdMs = 5.0; int det = cur.detector; float relMul = 1.0f;
        switch (cur.character)
        {
            case 1: knee = std::min (24.0f, knee + 6.0f); atk = std::max (atk, 5.0f); break;               // Smooth
            case 2: knee = std::max (0.0f, knee - 3.0f); atk = std::max (atk, 0.3f); holdMs = 10.0; break; // Punch
            case 3: det = 1; relMul = 2.0f; break;                                                          // Opto
            default: break;
        }
        // guardrails
        const int lookSamples = std::clamp ((int) std::lround (cur.lookaheadMs * 0.001 * sr), 0, activeDelay);
        if (lookSamples < (int) std::lround (0.001 * sr)) atk = std::max (atk, 1.0f);        // attack < 1 ms yalnız lookahead ≥ 1 ms
        rel = std::max (rel, 30.0f) * relMul;
        cur.kneeDb = knee; cur.attackMs = atk; cur.releaseMs = rel; cur.detector = det;
        atkCoef = coefFromMs (sr, atk); relCoef = coefFromMs (sr, rel); relCoefFast = coefFromMs (sr, rel * 0.5);
        holdSamples = (int) std::lround (holdMs * 0.001 * sr);
        peakCoef = coefFromMs (sr, 0.5); rmsCoef = coefFromMs (sr, 10.0);
        gainDelay.setDelay (std::max (0, activeDelay - lookSamples));
        if (cur.scHpfHz != scHpfLast) { scHpf.setCoeffs (design::highpass (sr, std::clamp ((double) cur.scHpfHz, 20.0, 500.0), 0.7071)); scHpfLast = cur.scHpfHz; }
        makeup.setTargetDb (cur.on ? cur.makeupDb : 0.0f);
        mixS.setTarget (std::clamp (cur.mix, 0.0f, 1.0f));
    }
    const float thr = cur.threshDb, knee = cur.kneeDb, ratio = std::max (1.0f, cur.ratio);
    const float slope = 1.0f - 1.0f / ratio;
    for (int i = 0; i < n; ++i)
    {
        // sidechain (gecikmesiz giriş): mono toplam → HPF
        float sc = 0.0f;
        for (int k = 0; k < numCh; ++k) sc += ch[k][i];
        sc = scHpf.processSample (sc / (float) numCh);
        // dedektör
        const float a = std::fabs (sc);
        peakEnv = a + peakCoef * (peakEnv - a);
        if (a > peakEnv) peakEnv = a;
        rmsEnv = a * a + rmsCoef * (rmsEnv - a * a);
        float lvl;
        if (cur.detector == 0) lvl = dbOf (peakEnv);
        else if (cur.detector == 1) lvl = 10.0f * std::log10 (std::max (rmsEnv, 1.0e-14f));
        else lvl = 0.5f * (dbOf (peakEnv) + 10.0f * std::log10 (std::max (rmsEnv, 1.0e-14f)));
        // kazanç hesaplayıcı (soft knee)
        const float over = lvl - thr;
        float grTarget;
        if (knee <= 0.0f || over <= -knee * 0.5f) grTarget = over <= 0.0f ? 0.0f : -slope * over;
        else if (over >= knee * 0.5f) grTarget = -slope * over;
        else { const float t = over + knee * 0.5f; grTarget = -slope * t * t / (2.0f * knee); }
        // zarf: attack (derinleşme) / hold / release (program-bağımlı)
        if (grTarget < grDb) { grDb = grTarget + atkCoef * (grDb - grTarget); holdLeft = holdSamples; }
        else if (holdLeft > 0) --holdLeft;
        else
        {
            const float c = (-grDb < 3.0f) ? relCoefFast : relCoef;      // hafif GR → hızlı toparlanma
            grDb = grTarget + c * (grDb - grTarget);
        }
        // lookahead: kazanç, ses yolundan (D − L) kadar gecikir
        const float gDb = gainDelay.processSample (grDb);
        const float g = std::pow (10.0f, gDb * 0.05f) * makeup.nextLinear();
        const float m = mixS.next();
        for (int k = 0; k < numCh; ++k)
        {
            const float d = audioDelay[(size_t) k].processSample (ch[k][i]);
            ch[k][i] = d * g * m + d * (1.0f - m);
        }
        grDbOut = gDb;
    }
    curMakeupDb = makeup.getCurrentDb();
    scHpf.sanitize();
    if (! std::isfinite (peakEnv) || ! std::isfinite (rmsEnv) || ! std::isfinite (grDb)) { peakEnv = rmsEnv = 0.0f; grDb = 0.0f; }
}

} // namespace sweax
