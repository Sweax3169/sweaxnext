// SWEAX NEXT — DSP/CorrectiveEQ.cpp
#include "CorrectiveEQ.h"
#include "../Chain/VocalChain.h"
#include <cmath>

namespace sweax
{

namespace
{
    constexpr float KNEE_DB = 3.0f;
    constexpr int SUB = 64;            // yumuşatma sürerken katsayı güncelleme alt bloğu (blok boyundan bağımsız)
    inline float coefFromMs (double sr, float ms) noexcept { return (float) std::exp (-1.0 / (std::max (0.01, (double) ms) * 0.001 * sr)); }
}

int CorrectiveEQ::latencySamples (Quality mode) const noexcept
{
    return mode == Quality::Canli ? 0 : laMax;
}

void CorrectiveEQ::setQuality (Quality mode) noexcept
{
    laAktif = latencySamples (mode);
    for (auto& d : plosGecikme) d.setDelay (laAktif);
    setDryDelay (laAktif);
}

void CorrectiveEQ::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    for (int k = 0; k < std::min (numCh, (int) MAX_CH); ++k)
        for (int i = 0; i < n; ++i) plosGecikme[(size_t) k].processSample (ch[k][i]);
}

void CorrectiveEQ::prepare (double sampleRate, int maxBlock, int numChannels, bool onBakis)
{
    laMax = onBakis ? std::max (0, (int) std::lround (PLOS_LOOKAHEAD_MS * 0.001 * sampleRate)) : 0;
    prepareBase (sampleRate, maxBlock, numChannels, laMax);
    for (auto& d : plosGecikme) d.prepare (laMax, laMax);
    laAktif = laMax;
    setDryDelay (laAktif);
    for (auto& b : bandState)
    {
        b.filt.prepare (sr, chans);
        b.hzS.prepare (sr, 30.0); b.gainS.prepare (sr, 30.0); b.qS.prepare (sr, 30.0);
        b.lastHz = 0.0f; b.lastGain = 0.0f; b.lastQ = 0.0f; b.lastType = -1; b.wasOn = false; b.active = false; b.wasDyn = false;
        b.env = 0.0; b.grDb = 0.0f; b.dynGrDb = 0.0f; b.svfHz = 0.0f; b.svfQ = 0.0f;
    }
    hpf.prepare (sr, chans);
    hpfHzS.prepare (sr, 30.0);
    plos.cLf = coefFromMs (sr, 1.0f); plos.cMid = coefFromMs (sr, 25.0f);        // gerçek vokal ızgarası: 25 ms seçici
    plos.cAtk = coefFromMs (sr, 1.0f); plos.cRel = coefFromMs (sr, 50.0f);
    plos.lastHz = -1.0f;
    lastHpfHz = 0.0f; lastHpfSlope = -1; hpfWasOn = false; hpfActive = false;
    pending = cur = EqParams {};
    paramsDirty = true;
}

void CorrectiveEQ::resetWet()
{
    for (auto& b : bandState) { b.filt.reset(); for (auto& s : b.svf) s.reset(); for (auto& s : b.scSvf) s.reset(); b.env = 0.0; b.grDb = 0.0f; b.dynGrDb = 0.0f; }
    hpf.reset();
    plos.scLp.reset(); plos.scHp.reset(); plos.scLp2.reset();
    plos.eLf = plos.eMid = 0.0; plos.grDb = 0.0f; plos.ic1.fill (0.0); plos.ic2.fill (0.0);
    for (auto& d : plosGecikme) d.reset();
}

void CorrectiveEQ::plosiveGuard (float* const* ch, int numCh, int n) noexcept
{
    auto& P = plos;
    const bool acik = cur.plosOn;
    if (! acik && P.grDb > -0.001f)                                        // kapalı ve sönmüş: yalnız sabit gecikme
    {
        P.grDb = 0.0f;
        for (int k = 0; k < numCh; ++k)
            for (int i = 0; i < n; ++i) ch[k][i] = plosGecikme[(size_t) k].processSample (ch[k][i]);
        return;
    }
    const float hz = std::clamp (cur.plosHz, 40.0f, 400.0f);
    constexpr double KQ = 1.41421356237;                                  // Q 0.7071
    if (std::fabs (hz - P.lastHz) > 0.01f)
    {
        P.lastHz = hz;
        P.g0 = std::tan (3.14159265358979323846 * std::min ((double) hz, 0.45 * sr) / sr);
        P.scLp.setCoeffs (sr, 0.5 * hz, 0.7071);
        P.scHp.setCoeffs (sr, 300.0, 0.7071);
        P.scLp2.setCoeffs (sr, std::min (3000.0, 0.45 * sr), 0.7071);
    }
    const float range = acik ? std::fabs (std::min (0.0f, cur.plosRangeDb)) : 0.0f;
    // A = 1 (kazanç 0) katsayıları: durum ısınık kalır, çıkış birebir
    const double gI = P.g0, a1I = 1.0 / (1.0 + gI * (gI + KQ)), a2I = gI * a1I, a3I = gI * a2I;
    for (int i = 0; i < n; ++i)
    {
        float sc = 0.0f;
        for (int k = 0; k < numCh; ++k) sc += ch[k][i];
        sc /= (float) std::max (1, numCh);
        if (! (sc > -1.0e6f && sc < 1.0e6f)) sc = 0.0f;
        const double lf = P.scLp.process (sc).lp;
        const double md = P.scLp2.process (P.scHp.process (sc).hp).lp;
        P.eLf = lf * lf + P.cLf * (P.eLf - lf * lf);
        P.eMid = md * md + P.cMid * (P.eMid - md * md);
        float hedef = 0.0f;
        const float lfDb = 10.0f * std::log10 ((float) P.eLf + 1.0e-12f);
        if (lfDb > -50.0f && range > 0.0f)
        {
            const float midDb = 10.0f * std::log10 ((float) P.eMid + 1.0e-12f);
            const float over = (lfDb - midDb) - 6.0f;
            float gr;
            if (over <= -KNEE_DB * 0.5f) gr = 0.0f;
            else if (over >= KNEE_DB * 0.5f) gr = over;
            else { const float t = over + KNEE_DB * 0.5f; gr = t * t / (2.0f * KNEE_DB); }
            hedef = -std::min (range, gr);
        }
        const float c = hedef < P.grDb ? P.cAtk : P.cRel;
        P.grDb = hedef + c * (P.grDb - hedef);
        double a1 = a1I, a2 = a2I, a3 = a3I, m1 = 0.0, m2 = 0.0;
        if (P.grDb < -0.001f)
        {
            const double A = std::pow (10.0, (double) P.grDb / 40.0);
            const double g = P.g0 / std::sqrt (A);
            a1 = 1.0 / (1.0 + g * (g + KQ)); a2 = g * a1; a3 = g * a2;
            m1 = KQ * (A - 1.0); m2 = A * A - 1.0;
        }
        for (int k = 0; k < numCh; ++k)
        {
            const double v0 = plosGecikme[(size_t) k].processSample (ch[k][i]);   // önden-bakış: ses yolu gecikmeli
            const double v3 = v0 - P.ic2[(size_t) k];
            const double v1 = a1 * P.ic1[(size_t) k] + a2 * v3;
            const double v2 = P.ic2[(size_t) k] + a2 * P.ic1[(size_t) k] + a3 * v3;
            P.ic1[(size_t) k] = 2.0 * v1 - P.ic1[(size_t) k];
            P.ic2[(size_t) k] = 2.0 * v2 - P.ic2[(size_t) k];
            ch[k][i] = (float) (v0 + m1 * v1 + m2 * v2);
        }
    }
    P.scLp.sanitize(); P.scHp.sanitize(); P.scLp2.sanitize();
    bool bozuk = ! (std::isfinite (P.eLf) && std::isfinite (P.eMid) && std::isfinite (P.grDb));
    for (int k = 0; k < MAX_CH; ++k) bozuk = bozuk || ! (std::isfinite (P.ic1[(size_t) k]) && std::isfinite (P.ic2[(size_t) k]));
    if (bozuk) { P.eLf = P.eMid = 0.0; P.grDb = 0.0f; P.ic1.fill (0.0); P.ic2.fill (0.0); }
}

BiquadCoeffs CorrectiveEQ::designBand (double fs, int type, double hz, double gainDb, double q) noexcept
{
    const double f = std::clamp (hz, 20.0, fs * 0.45);
    const double qq = std::clamp (q, 0.1, 16.0);
    switch (type)
    {
        case 1:  return design::lowShelf (fs, f, gainDb, qq);
        case 2:  return design::highShelf (fs, f, gainDb, qq);
        case 3:  return design::notch (fs, f, qq);
        default: return design::bellMatched (fs, f, gainDb, qq);
    }
}

// statik bant: parametre yumuşatma + alt blok katsayı; büyük sıçramada / aç-kapada crossfade
void CorrectiveEQ::updateStatic (int bi, const EqBandParams& p) noexcept
{
    auto& b = bandState[(size_t) bi];
    const bool turnOn = ! b.wasOn && p.on;
    const bool turnOff = b.wasOn && ! p.on;
    b.wasOn = p.on;
    if (turnOff)
    {
        // kapama: kimliğe crossfade (bant, geçiş bitene dek işler) — anahtar tık yapardı (inceleme).
        // Dinamik bant statik filtreyi kullanmaz: onun kapanışı GR/kazancı 0'a indiren kısa sönümdür.
        if (b.wasDyn) b.dynFade = (int) std::lround (0.05 * sr);
        else { b.filt.beginJump (std::max (20.0f, b.lastHz)); b.filt.setPendingCoeffs (0, identity()); }
        return;
    }
    if (turnOn || b.lastType != p.type)
    {
        b.hzS.setCurrentAndTarget (p.hz); b.qS.setCurrentAndTarget (p.q);
        b.active = true;
        if (p.type == 3)
        {
            // notch'un "kazancı" yok: kimlikten hedefe crossfade
            b.filt.setCoeffsDirect (0, identity());
            b.filt.beginJump (p.hz);
            b.filt.setPendingCoeffs (0, designBand (sr, p.type, p.hz, 0.0, p.q));
            b.gainS.setCurrentAndTarget (0.0f);
        }
        else
        {
            // bell/shelf: kazanç 0 (= kimlik) → hedefe rampa
            if (turnOn) b.gainS.setCurrentAndTarget (0.0f);
            b.gainS.setTarget (p.gainDb);
            b.filt.setCoeffsDirect (0, designBand (sr, p.type, b.hzS.getCurrent(), b.gainS.getCurrent(), b.qS.getCurrent()));
        }
        b.lastType = p.type; b.lastHz = p.hz; b.lastGain = p.gainDb; b.lastQ = p.q;
        return;
    }
    if (! p.on) return;
    const bool bigJump = std::fabs (p.gainDb - b.lastGain) > 6.0f
                      || std::fabs (std::log2 (std::max (p.hz, 1.0f) / std::max (b.lastHz, 1.0f))) > 1.0f / 3.0f
                      || std::max (p.q, b.lastQ) / std::max (0.05f, std::min (p.q, b.lastQ)) > 2.0f;
    if (bigJump)
    {
        b.filt.beginJump (p.hz);
        b.filt.setPendingCoeffs (0, designBand (sr, p.type, p.hz, p.gainDb, p.q));
        b.hzS.setCurrentAndTarget (p.hz); b.gainS.setCurrentAndTarget (p.gainDb); b.qS.setCurrentAndTarget (p.q);
    }
    else
    {
        b.hzS.setTarget (p.hz); b.gainS.setTarget (p.gainDb); b.qS.setTarget (p.q);
    }
    b.lastHz = p.hz; b.lastGain = p.gainDb; b.lastQ = p.q;
}

void CorrectiveEQ::updateHpf (const EqParams& p) noexcept
{
    const bool turnOn = ! hpfWasOn && p.hpfOn;
    const bool turnOff = hpfWasOn && ! p.hpfOn;
    hpfWasOn = p.hpfOn;
    const int stages = p.hpfSlope == 1 ? 2 : 1;
    auto make = [&] (double hz, int stage) { return design::highpass (sr, std::clamp (hz, 20.0, 400.0), hpfStageQ (stages, stage)); };
    if (turnOff)
    {
        hpf.beginJump (std::max (20.0f, lastHpfHz));
        for (int s = 0; s < 2; ++s) hpf.setPendingCoeffs (s, identity());
        return;
    }
    if (! p.hpfOn) return;
    if (turnOn || lastHpfSlope != p.hpfSlope)
    {
        hpf.setStages (stages);
        hpfHzS.setCurrentAndTarget (p.hpfHz);
        hpfActive = true;
        // açma: kimlikten hedefe crossfade (anında devreye girme 60–120 Hz'de tık yapardı — inceleme)
        for (int s = 0; s < stages; ++s) hpf.setCoeffsDirect (s, identity());
        hpf.beginJump (p.hpfHz);
        for (int s = 0; s < stages; ++s) hpf.setPendingCoeffs (s, make (p.hpfHz, s));
        lastHpfHz = p.hpfHz; lastHpfSlope = p.hpfSlope;
        return;
    }
    const bool bigJump = std::fabs (std::log2 (std::max (p.hpfHz, 1.0f) / std::max (lastHpfHz, 1.0f))) > 1.0f / 3.0f;
    if (bigJump)
    {
        hpf.beginJump (p.hpfHz);
        for (int s = 0; s < stages; ++s) hpf.setPendingCoeffs (s, make (p.hpfHz, s));
        hpfHzS.setCurrentAndTarget (p.hpfHz);
    }
    else hpfHzS.setTarget (p.hpfHz);
    lastHpfHz = p.hpfHz;
}

void CorrectiveEQ::processWet (float* const* ch, int numCh, int n) noexcept
{
    // NOT: cur.on burada KASITLI olarak kontrol edilmez — modül aç/kapa ModuleBase crossfade'iyle harmanlanır;
    // ıslak yol geçiş boyunca filtrelemeye devam eder (erken dönüş, crossfade ortasında ani sıçrama yapıyordu).
    if (paramsDirty) { cur = pending; paramsDirty = false; updateHpf (cur); for (int b = 0; b < BANDS; ++b) updateStatic (b, cur.band[(size_t) b]); }

    // ---- patlama koruması (HPF ÖNCESİ: tetik ham LF patlamasını görür) ----
    plosiveGuard (ch, numCh, n);

    // ---- HPF ----
    if (hpfActive)
    {
        const int stages = cur.hpfSlope == 1 ? 2 : 1;
        int i = 0;
        while (i < n)
        {
            const int m = std::min (SUB, n - i);
            if (cur.hpfOn && hpfHzS.isSmoothing())
            {
                hpfHzS.skip (m);
                const double hz = hpfHzS.getCurrent();
                for (int s = 0; s < stages; ++s) hpf.setCoeffsDirect (s, design::highpass (sr, std::clamp (hz, 20.0, 400.0), hpfStageQ (stages, s)));
            }
            for (int j = 0; j < m; ++j)
            {
                for (int k = 0; k < numCh; ++k) ch[k][i + j] = hpf.processSample (k, ch[k][i + j]);
                hpf.advance();
            }
            i += m;
        }
        hpf.sanitize();
        if (! cur.hpfOn && ! hpf.inTransition()) hpfActive = false;      // kimliğe geçiş bitti
    }

    // ---- bantlar ----
    for (int bi = 0; bi < BANDS; ++bi)
    {
        const auto& p = cur.band[(size_t) bi];
        auto& b = bandState[(size_t) bi];
        const bool dynNow = p.on && p.dyn && p.type == 0;
        if (dynNow || b.dynFade > 0)
        {
            // dinamik bell: fc/Q hedefe tek-kutup yumuşatılır (blok başına adım; ani setCoeffs zipper yapardı)
            if (dynNow && ! b.wasDyn) { b.svfHz = p.hz; b.svfQ = p.q; for (int k = 0; k < numCh; ++k) { b.svf[(size_t) k].setCoeffs (sr, p.hz, p.q); b.scSvf[(size_t) k].setCoeffs (sr, p.hz, p.q); } }
            if (dynNow) b.wasDyn = true;
            const float aBlk = 1.0f - (float) std::exp (- (double) n / (0.03 * sr));
            if (dynNow && (std::fabs (b.svfHz - p.hz) > 0.01f * p.hz || std::fabs (b.svfQ - p.q) > 0.01f * p.q))
            {
                b.svfHz += aBlk * (p.hz - b.svfHz); b.svfQ += aBlk * (p.q - b.svfQ);
                for (int k = 0; k < numCh; ++k) { b.svf[(size_t) k].setCoeffs (sr, b.svfHz, b.svfQ); b.scSvf[(size_t) k].setCoeffs (sr, b.svfHz, b.svfQ); }
            }
            b.atkCoef = coefFromMs (sr, p.dynAttackMs); b.relCoef = coefFromMs (sr, p.dynReleaseMs);
            b.envCoef = coefFromMs (sr, 2.0f);                      // 2 ms RMS ön yumuşatma
            b.gainS.setTarget (dynNow ? p.gainDb : 0.0f);           // statik pay 30 ms rampalı (ani uygulama zipper — inceleme)
            const float range = dynNow ? std::fabs (p.dynRangeDb) : 0.0f;   // sönümde GR release ile 0'a iner
            const float thr = p.dynThreshDb;
            for (int i = 0; i < n; ++i)
            {
                float sc = 0.0f;
                for (int k = 0; k < numCh; ++k) sc += b.scSvf[(size_t) k].bandpassNormalized (ch[k][i]);
                sc /= (float) numCh;
                b.env = (double) sc * sc + b.envCoef * (b.env - (double) sc * sc);
                const float lvl = 10.0f * std::log10 ((float) b.env + 1.0e-12f);
                const float over = lvl - thr;
                float gr;
                if (over <= -KNEE_DB * 0.5f) gr = 0.0f;
                else if (over >= KNEE_DB * 0.5f) gr = over;
                else { const float t = over + KNEE_DB * 0.5f; gr = t * t / (2.0f * KNEE_DB); }
                gr = -std::min (range, gr);
                const float c = gr < b.grDb ? b.atkCoef : b.relCoef;
                b.grDb = gr + c * (b.grDb - gr);
                const float g = std::pow (10.0f, (b.grDb + b.gainS.next()) * 0.05f);
                for (int k = 0; k < numCh; ++k) ch[k][i] = b.svf[(size_t) k].bell (ch[k][i], g);
            }
            b.dynGrDb = b.grDb;
            for (auto& s : b.svf) s.sanitize();
            for (auto& s : b.scSvf) s.sanitize();
            if (! (std::isfinite ((float) b.env) && std::isfinite (b.grDb))) { b.env = 0.0; b.grDb = 0.0f; }   // zehirlenme kilidi (inceleme)
            if (! dynNow) { b.dynFade -= n; if (b.dynFade <= 0 && ! b.gainS.isSmoothing()) { b.dynFade = 0; b.wasDyn = false; } }
        }
        else
        {
            b.wasDyn = false;
            if (! b.active) { b.dynGrDb = 0.0f; continue; }
            int i = 0;
            while (i < n)
            {
                const int m = std::min (SUB, n - i);
                if (p.on && (b.hzS.isSmoothing() || b.gainS.isSmoothing() || b.qS.isSmoothing()))
                {
                    b.hzS.skip (m); b.gainS.skip (m); b.qS.skip (m);
                    b.filt.setCoeffsDirect (0, designBand (sr, p.type, b.hzS.getCurrent(), b.gainS.getCurrent(), b.qS.getCurrent()));
                }
                for (int j = 0; j < m; ++j)
                {
                    for (int k = 0; k < numCh; ++k) ch[k][i + j] = b.filt.processSample (k, ch[k][i + j]);
                    b.filt.advance();
                }
                i += m;
            }
            b.filt.sanitize();
            b.dynGrDb = 0.0f;
            if (! p.on && ! b.filt.inTransition()) b.active = false;     // kimliğe geçiş bitti
        }
    }
}

} // namespace sweax
