// SWEAX NEXT — DSP/StereoDelay.cpp
#include "StereoDelay.h"
#include "../Chain/VocalChain.h"
#include <cmath>

namespace sweax
{

namespace
{
    constexpr double PI = 3.141592653589793;
    BiquadCoeffs butterHp (double sr, double fc) noexcept
    {
        const double w0 = 2.0 * PI * std::clamp (fc, 20.0, 0.45 * sr) / sr, cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * 0.70710678), a0 = 1.0 + alpha;
        BiquadCoeffs c;
        c.b0 = (1.0 + cw) * 0.5 / a0; c.b1 = -(1.0 + cw) / a0; c.b2 = (1.0 + cw) * 0.5 / a0;
        c.a1 = -2.0 * cw / a0; c.a2 = (1.0 - alpha) / a0;
        return c;
    }
    BiquadCoeffs butterLp (double sr, double fc) noexcept
    {
        const double w0 = 2.0 * PI * std::clamp (fc, 200.0, 0.45 * sr) / sr, cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * 0.70710678), a0 = 1.0 + alpha;
        BiquadCoeffs c;
        c.b0 = (1.0 - cw) * 0.5 / a0; c.b1 = (1.0 - cw) / a0; c.b2 = (1.0 - cw) * 0.5 / a0;
        c.a1 = -2.0 * cw / a0; c.a2 = (1.0 - alpha) / a0;
        return c;
    }
    inline float coefFromMs (double sr, double ms) noexcept { return (float) std::exp (-1.0 / (std::max (0.01, ms) * 0.001 * sr)); }
}

void StereoDelay::prepare (double sampleRate, int maxBlock, int numChannels)
{
    prepareBase (sampleRate, maxBlock, numChannels, 0);
    setDryDelay (0);
    const int maxN = (int) std::lround (MAX_DELAY_MS * 0.001 * sampleRate) + 4;
    lineL.prepare (maxN, 0); lineR.prepare (maxN, 0);
    duckRise = coefFromMs (sampleRate, 5.0);
    duckFall = coefFromMs (sampleRate, 250.0);
    mixS.prepare (sampleRate, 15.0);
    mixS.setCurrentAndTarget (0.0f);
    widthS.prepare (sampleRate, 20.0);
    widthS.setCurrentAndTarget (1.0f);
    enerjiA = 1.0f - coefFromMs (sampleRate, 50.0);    // koşan enerji: ~50 ms zaman sabiti
    ilkParam = true;
    jStep = 1.0f / std::max (1.0f, (float) (0.010 * sampleRate));
    curDelayN = -1; jFade = 1.0f; jState = 0;
    paramsDirty = true;
    resetWet();
}

void StereoDelay::updateParams() noexcept
{
    cur = pending;
    double ms = cur.timeMs;
    const double beat = 60000.0 / std::clamp (cur.bpm, 40.0, 240.0);   // 1/4 nota ms
    if (cur.sync == 1) ms = beat * 0.5;
    else if (cur.sync == 2) ms = beat * 0.75;
    else if (cur.sync == 3) ms = beat;
    else if (cur.sync == 4) ms = beat * 0.25;                          // 1/16 (M3.5)
    ms = std::clamp (ms, 40.0, MAX_DELAY_MS - 10.0);
    tgtDelayN = (int) std::lround (ms * 0.001 * sr);
    if (curDelayN < 0) { curDelayN = tgtDelayN; lineL.setDelay (curDelayN); lineR.setDelay (curDelayN); }
    fb = std::clamp (cur.feedback, 0.0f, 0.85f);
    hpL.setCoeffs (butterHp (sr, cur.hpHz)); hpR.setCoeffs (butterHp (sr, cur.hpHz));
    lpL.setCoeffs (butterLp (sr, cur.lpHz)); lpR.setCoeffs (butterLp (sr, cur.lpHz));
    duckAmt = std::clamp (cur.duckDb, 0.0f, 18.0f);
    mixS.setTarget (std::clamp (cur.mix, 0.0f, 1.0f));
    const float w = std::isfinite (cur.width) ? std::clamp (cur.width, 0.0f, 1.0f) : 1.0f;
    if (ilkParam) { widthS.setCurrentAndTarget (w); ilkParam = false; }
    else widthS.setTarget (w);
    paramsDirty = false;
}

void StereoDelay::processWet (float* const* ch, int numCh, int n) noexcept
{
    if (paramsDirty) updateParams();
    const int c = std::min (numCh, chans);
    for (int i = 0; i < n; ++i)
    {
        // süre değişimi durum makinesi: sön → tek zıplama → aç
        if (jState == 0 && tgtDelayN != curDelayN) jState = 1;
        if (jState == 1)
        {
            jFade -= jStep;
            if (jFade <= 0.0f)
            {
                jFade = 0.0f; curDelayN = tgtDelayN;
                lineL.setDelay (curDelayN); lineR.setDelay (curDelayN);
                jState = 2;
            }
        }
        else if (jState == 2)
        {
            jFade += jStep;
            if (jFade >= 1.0f) { jFade = 1.0f; jState = 0; }
        }
        float in = ch[0][i];
        if (c > 1) in = 0.5f * (in + ch[1][i]);
        if (! (in > -1.0e6f && in < 1.0e6f)) in = 0.0f;

        // ducking zarfı (kuru vokal)
        const float a = std::fabs (in);
        duckEnv = a + (a > duckEnv ? duckRise : duckFall) * (duckEnv - a);
        const float envDb = duckEnv > 1.0e-6f ? 20.0f * std::log10 (duckEnv) : -120.0f;
        const float tt = std::clamp ((envDb + 50.0f) / 20.0f, 0.0f, 1.0f);        // −50…−30 dB → 0…1
        const float duckGain = std::pow (10.0f, -duckAmt * tt * 0.05f);

        // ping-pong: giriş L hattına; çapraz geri besleme; filtreler döngü içinde
        const float outL = lineL.processSample (in + fb * lastR);
        const float outR = lineR.processSample (fb * lastL);
        lastL = lpL.processSample (hpL.processSample (outL));
        lastR = lpR.processSample (hpR.processSample (outR));

        const float m = mixS.next() * duckGain * jFade;
        const float w = widthS.next();
        if (c < 2 || w >= 1.0f)
        {
            // M2 ping-pong (width 1) / mono çıkış — birebir
            ch[0][i] += m * lastL;
            if (c > 1) ch[1][i] += m * lastR; else ch[0][i] += m * lastR;
        }
        else
        {
            // ortaya toplama: M = 0.7071·(L+R), arası doğrusal karışım
            const float mid = 0.70710678f * (lastL + lastR);
            const float oL = w <= 0.0f ? mid : mid + w * (lastL - mid);
            const float oR = w <= 0.0f ? mid : mid + w * (lastR - mid);
            // ENERJİ TELAFİSİ (başlıkta ölçümler): ham genişlik çıkışını ping-pong referansının enerjisine
            // normalize et — ara değer çukurunu ve korele geri beslemedeki +3 dB'yi TEK mekanizma kapatır.
            enerjiRef += enerjiA * (lastL * lastL + lastR * lastR - enerjiRef);
            enerjiCik += enerjiA * (oL * oL + oR * oR - enerjiCik);
            const float g = std::clamp (std::sqrt ((enerjiRef + 1.0e-12f) / (enerjiCik + 1.0e-12f)), 0.5f, 2.0f);
            ch[0][i] += m * oL * g;
            ch[1][i] += m * oR * g;
        }
    }
    // sigorta: koşan enerjiler sonlu kalsın (bozuk giriş hatlara sızarsa telafi kazancı NaN olmasın)
    if (! (std::isfinite (enerjiRef) && std::isfinite (enerjiCik))) enerjiRef = enerjiCik = 0.0f;
}

void StereoDelay::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    juce::ignoreUnused (ch, numCh);
    // kuyruklar doğal söner; yeniden açılış hayalet yankı çalmaz
    for (int i = 0; i < n; ++i)
    {
        const float outL = lineL.processSample (fb * lastR);
        const float outR = lineR.processSample (fb * lastL);
        lastL = lpL.processSample (hpL.processSample (outL));
        lastR = lpR.processSample (hpR.processSample (outR));
    }
}

void StereoDelay::resetWet()
{
    lineL.reset(); lineR.reset();
    hpL.reset(); lpL.reset(); hpR.reset(); lpR.reset();
    duckEnv = 0.0f; lastL = lastR = 0.0f;
    enerjiRef = enerjiCik = 0.0f;                      // telafi sessizlikten g = 1 ile açılır
    widthS.setCurrentAndTarget (widthS.getTarget());   // yarım kalan width rampası reset'te tamamlanır
}

} // namespace sweax
