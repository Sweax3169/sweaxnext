// SWEAX NEXT — DSP/Doubler.cpp
#include "Doubler.h"
#include "../Chain/VocalChain.h"
#include <cmath>

namespace sweax
{

namespace
{
    constexpr double PI = 3.141592653589793;
    BiquadCoeffs onePoleHp (double sr, double fc) noexcept
    {
        // RBJ 2. derece Butterworth HP (Q=0.707)
        const double w0 = 2.0 * PI * fc / sr, cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * 0.70710678);
        const double a0 = 1.0 + alpha;
        BiquadCoeffs c;
        c.b0 = (1.0 + cw) * 0.5 / a0; c.b1 = -(1.0 + cw) / a0; c.b2 = (1.0 + cw) * 0.5 / a0;
        c.a1 = -2.0 * cw / a0; c.a2 = (1.0 - alpha) / a0;
        return c;
    }
    BiquadCoeffs onePoleLp (double sr, double fc) noexcept
    {
        const double w0 = 2.0 * PI * fc / sr, cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * 0.70710678);
        const double a0 = 1.0 + alpha;
        BiquadCoeffs c;
        c.b0 = (1.0 - cw) * 0.5 / a0; c.b1 = (1.0 - cw) / a0; c.b2 = (1.0 - cw) * 0.5 / a0;
        c.a1 = -2.0 * cw / a0; c.a2 = (1.0 - alpha) / a0;
        return c;
    }
}

void Doubler::prepare (double sampleRate, int maxBlock, int numChannels)
{
    prepareBase (sampleRate, maxBlock, numChannels, 0);
    setDryDelay (0);
    windowN = std::max (16, (int) std::lround (WINDOW_MS * 0.001 * sampleRate));
    // halka: pencere + azami yayılım (40 ms) + pencere payı
    ringLen = windowN * 2 + (int) std::lround (0.050 * sampleRate) + 8;
    ring.assign ((size_t) ringLen, 0.0f);
    wp = 0;
    mixS.prepare (sampleRate, 15.0);
    mixS.setCurrentAndTarget (0.0f);
    paramsDirty = true;
    resetWet();
}

void Doubler::updateParams() noexcept
{
    cur = pending;
    activeVoices = cur.voices > 0 ? 4 : 2;
    const float spread = std::clamp (cur.spreadMs, 5.0f, 40.0f) * 0.001f * (float) sr;
    // sesler: [−c hardL, +c hardR, −c/2 %50L, +c/2 %50R]
    const float cents[MAX_VOICES] = { -cur.detuneCents, cur.detuneCents, -0.5f * cur.detuneCents, 0.5f * cur.detuneCents };
    const float panLs[MAX_VOICES] = { 1.0f, 0.0f, 0.75f, 0.25f };
    const float dly[MAX_VOICES]  = { spread, spread * 0.8f, spread * 1.25f, spread * 0.6f };
    for (int v = 0; v < MAX_VOICES; ++v)
    {
        auto& vc = voice[(size_t) v];
        const double r = std::pow (2.0, (double) cents[v] / 1200.0);
        vc.rate = (1.0 - r) / (double) windowN;          // faz/örnek; pozitif faz artışı = gecikme artar (pes)
        vc.baseDelay = dly[v];
        vc.panL = panLs[v]; vc.panR = 1.0f - panLs[v];
        vc.hp.setCoeffs (onePoleHp (sr, std::clamp (cur.hpHz, 40.0f, 1000.0f)));
        vc.lp.setCoeffs (onePoleLp (sr, std::clamp (cur.lpHz, 1500.0f, 18000.0f)));
    }
    mixS.setTarget (std::clamp (cur.mix, 0.0f, 1.0f) * 0.5f);   // ses toplamı; 0.5 → tek ses ~ −6 dB
    paramsDirty = false;
}

void Doubler::processWet (float* const* ch, int numCh, int n) noexcept
{
    if (paramsDirty) updateParams();
    const int c = std::min (numCh, chans);
    for (int i = 0; i < n; ++i)
    {
        float in = ch[0][i];
        if (c > 1) in = 0.5f * (in + ch[1][i]);
        if (! (in > -1.0e6f && in < 1.0e6f)) in = 0.0f;
        pushRing (in);
        const float m = mixS.next();
        float wetL = 0.0f, wetR = 0.0f;
        for (int v = 0; v < activeVoices; ++v)
        {
            auto& vc = voice[(size_t) v];
            // çift-tap testere: tap1 fazı p, tap2 fazı p+0.5 (mod 1); üçgen crossfade
            vc.phase += vc.rate;
            while (vc.phase >= 1.0) vc.phase -= 1.0;
            while (vc.phase < 0.0) vc.phase += 1.0;
            const double p1 = vc.phase;
            double p2 = vc.phase + 0.5; if (p2 >= 1.0) p2 -= 1.0;
            const double d1 = (double) vc.baseDelay + p1 * (double) windowN;
            const double d2 = (double) vc.baseDelay + p2 * (double) windowN;
            const float g1 = (float) (1.0 - std::fabs (2.0 * p1 - 1.0));   // üçgen (p=0.5 → 1)
            const float g2 = 1.0f - g1;
            float s = g1 * readRing (d1) + g2 * readRing (d2);
            s = vc.lp.processSample (vc.hp.processSample (s));
            wetL += s * vc.panL;
            wetR += s * vc.panR;
        }
        ch[0][i] += m * wetL;
        if (c > 1) ch[1][i] += m * wetR; else ch[0][i] += m * wetR;
    }
}

void Doubler::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    // ısın: halka girişle dolu kalır → açılışta anında dolu ses
    const int c = std::min (numCh, chans);
    for (int i = 0; i < n; ++i)
    {
        float in = ch[0][i];
        if (c > 1) in = 0.5f * (in + ch[1][i]);
        if (! (in > -1.0e6f && in < 1.0e6f)) in = 0.0f;    // denetim: halka NaN'ı açılışta biquad'lara taşıyordu
        pushRing (in);
    }
}

void Doubler::resetWet()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    wp = 0;
    for (auto& v : voice) { v.phase = 0.0; v.hp.reset(); v.lp.reset(); }
}

} // namespace sweax
