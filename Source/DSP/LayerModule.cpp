// SWEAX NEXT — DSP/LayerModule.cpp
#include "LayerModule.h"

namespace sweax
{

void LayerModule::prepare (double sampleRate, int maxBlock, int numChannels)
{
    prepareBase (sampleRate, maxBlock, numChannels, 0);
    setDryDelay (0);
    wNom = (float) std::max (64.0, WINDOW_NOM_MS * 0.001 * sampleRate);
    wMax = (float) std::max (96.0, WINDOW_MAX_MS * 0.001 * sampleRate);
    ringLen = (int) wMax + 16;
    ring.assign ((size_t) ringLen, 0.0f);
    // analiz: ~16 kHz desimasyonlu kopya (PitchCorrector reçetesi; maliyet sr'dan bağımsız)
    anaK = std::max (1, (int) std::lround (sampleRate / 16000.0));
    const double anaSr = sampleRate / (double) anaK;
    tracker.prepare (anaSr, 70.0, 800.0, 0.15f);
    anaHop = std::max (64, (int) std::lround (0.016 * anaSr));
    anaRingN = tracker.frameSize() + anaHop + 8;
    anaRing.assign ((size_t) anaRingN, 0.0f);
    scratch.assign ((size_t) tracker.frameSize(), 0.0f);
    seviye.prepare (sampleRate, 20.0);
    for (auto& s : ses) { s.kazanc.prepare (sampleRate, 15.0); s.panL.prepare (sampleRate, 15.0); s.panR.prepare (sampleRate, 15.0); }
    paramsDirty = true;
    resetWet();
}

void LayerModule::resetWet()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    wp = 0;
    std::fill (anaRing.begin(), anaRing.end(), 0.0f);
    anaWp = 0; anaAcc = 0.0f; anaAccN = 0; sinceAnalyze = 0;
    tracker.reset();
    f0 = 0.0f;
    wEff = wHedef = wNom;
    for (auto& s : ses)
    {
        s.faz = 0.25; s.hp.reset(); s.lp.reset(); s.aktif = false;
        s.kazanc.setCurrentAndTarget (0.0f); s.panL.setCurrentAndTarget (0.7071f); s.panR.setCurrentAndTarget (0.7071f);
    }
    seviye.setCurrentAndTarget (0.0f);
    paramsDirty = true;                                   // denetim: reset sonrası katman kalıcı sessiz kalıyordu
}

void LayerModule::updateParams() noexcept
{
    cur = pending;
    const int a = (int) std::lround (std::clamp (cur.semiA, -12.0f, 12.0f));
    const int b = (int) std::lround (std::clamp (cur.semiB, -12.0f, 12.0f));
    const int semis[2] = { a, b };
    const bool ikisi = a != 0 && b != 0;
    const float w = std::clamp (cur.width, 0.0f, 1.0f);
    const double lpHz = std::clamp ((double) cur.toneHz, 1500.0, std::min (12000.0, sr * 0.45));
    for (int v = 0; v < 2; ++v)
    {
        auto& s = ses[(size_t) v];
        const bool yeniAktif = semis[v] != 0;
        const bool tamSessiz = s.kazanc.getCurrent() <= 0.0f && ! s.kazanc.isSmoothing();
        if (yeniAktif && ! s.aktif && tamSessiz) { s.faz = 0.25; s.hp.reset(); s.lp.reset(); }
        s.aktif = yeniAktif;
        if (yeniAktif) s.oran = 1.0 - std::pow (2.0, (double) semis[v] / 12.0);   // kapanırken son oran KORUNUR
        s.kazanc.setTarget (yeniAktif ? 1.0f : 0.0f);
        const float konum = ikisi ? (v == 0 ? -w : w) : 0.0f;       // −1 sol .. +1 sağ
        const float aci = (konum + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        s.panL.setTarget (std::cos (aci));
        s.panR.setTarget (std::sin (aci));
        s.hp.setCoeffs (design::highpass (sr, 90.0, 0.7071));      // eleştirmen bulgusu: 70 Hz'de oktav-altı plosive LF tıkı
        s.lp.setCoeffs (design::lowpass (sr, lpHz, 0.7071));
    }
    gritK = 1.0f + 5.0f * std::clamp (cur.grit, 0.0f, 1.0f);
    gritInv = 1.0f / gritK;
    const float lvl = std::pow (10.0f, std::clamp (cur.levelDb, -36.0f, 0.0f) / 20.0f);
    seviye.setTarget (lvl);                                        // ses aç/kapa kazançları ses başına yumuşar
    paramsDirty = false;
}

void LayerModule::analizEt() noexcept
{
    const int need = tracker.frameSize();
    int rp = anaWp - need; while (rp < 0) rp += anaRingN;
    for (int i = 0; i < need; ++i)
    {
        scratch[(size_t) i] = anaRing[(size_t) rp];
        if (++rp >= anaRingN) rp = 0;
    }
    const auto res = tracker.analyze (scratch.data(), need);
    if (! (res.voiced && res.hz > 70.0f && res.hz < 800.0f)) return;       // tonal değil: son pencere korunur
    f0 = res.hz;
    const float T = (float) sr / f0;
    // N ÇİFT, W = N·T; histerezis: mevcut N ile W nominalin [0.7, 1.45] katındaysa N değişmez (N sıçraması = kayma)
    const int nMevcut = std::max (2, 2 * (int) std::lround (0.5f * wHedef / T));
    float wMevcut = (float) nMevcut * T;
    int N = nMevcut;
    if (wMevcut < 0.7f * wNom || wMevcut > 1.45f * wNom || wMevcut > wMax)
        N = std::max (2, 2 * (int) std::lround (0.5f * wNom / T));
    float w = (float) N * T;
    while (w > wMax && N > 2) { N -= 2; w = (float) N * T; }
    wHedef = std::clamp (w, 64.0f, wMax);
}

void LayerModule::besle (float in) noexcept
{
    ring[(size_t) wp] = in;
    wp = (wp + 1) % ringLen;
    anaAcc += in;
    if (++anaAccN >= anaK)
    {
        anaRing[(size_t) anaWp] = anaAcc / (float) anaK;
        anaWp = (anaWp + 1) % anaRingN;
        anaAcc = 0.0f; anaAccN = 0;
        if (++sinceAnalyze >= anaHop) { sinceAnalyze = 0; analizEt(); }
    }
}

void LayerModule::processWet (float* const* ch, int numCh, int n) noexcept
{
    if (paramsDirty) updateParams();
    const int c = std::min (numCh, chans);
    const bool gritVar = gritK > 1.001f;
    for (int i = 0; i < n; ++i)
    {
        float in = ch[0][i];
        if (c > 1) in = 0.5f * (in + ch[1][i]);
        if (! (in > -1.0e6f && in < 1.0e6f)) in = 0.0f;
        besle (in);
        // pencere yumuşak kayar (τ ≈ 40 ms): hızlı kayma perde sapması yapar, yavaş kayma nota geçişinde kısa iptal
        wEff += (float) (1.0 / (0.040 * sr)) * (wHedef - wEff);
        const double W = (double) wEff;
        const double C = 0.5 * (double) wMax + 2.0;                       // sabit merkez gecikme
        const float lvl = seviye.next();
        float L = 0.0f, R = 0.0f, M = 0.0f;
        for (auto& s : ses)
        {
            const float gSes = s.kazanc.next();
            const float pL = s.panL.next(), pR = s.panR.next();
            if (gSes <= 0.0f && ! s.aktif) continue;
            s.faz += s.oran / W;
            while (s.faz >= 1.0) s.faz -= 1.0;
            while (s.faz < 0.0) s.faz += 1.0;
            const double p1 = s.faz;
            double p2 = p1 + 0.5; if (p2 >= 1.0) p2 -= 1.0;
            const double s1 = std::sin (juce::MathConstants<double>::pi * p1);
            const float g1 = (float) (s1 * s1);                               // sin²: g1 + g2 = 1
            const float g2 = 1.0f - g1;
            float y = g1 * halkaOku (C + (p1 - 0.5) * W) + g2 * halkaOku (C + (p2 - 0.5) * W);
            y = s.lp.processSample (s.hp.processSample (y));
            if (gritVar) y = std::tanh (gritK * y) * gritInv;
            y *= gSes;
            L += y * pL;
            R += y * pR;
            M += y;                                                       // denetim: mono'da pan kazançları toplanmaz
        }
        if (c > 1) { ch[0][i] += lvl * L; ch[1][i] += lvl * R; }
        else ch[0][i] += lvl * M;
    }
    for (auto& s : ses) { s.hp.sanitize(); s.lp.sanitize(); }
}

void LayerModule::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    const int c = std::min (numCh, chans);
    for (int i = 0; i < n; ++i)
    {
        float in = ch[0][i];
        if (c > 1) in = 0.5f * (in + ch[1][i]);
        if (! (in > -1.0e6f && in < 1.0e6f)) in = 0.0f;
        besle (in);
        wEff += (float) (1.0 / (0.040 * sr)) * (wHedef - wEff);
    }
}

} // namespace sweax
