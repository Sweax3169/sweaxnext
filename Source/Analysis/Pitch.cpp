// SWEAX NEXT — Analysis/Pitch.cpp
#include "Pitch.h"
#include <algorithm>

namespace sweax
{

void PitchTracker::prepare (double sampleRate, double minHz, double maxHz, float threshold)
{
    sr = sampleRate;
    thr = threshold;
    maxLag = (int) std::ceil (sr / minHz);
    minLag = std::max (2, (int) std::floor (sr / maxHz));
    W = maxLag;                                   // entegrasyon penceresi = en uzun periyot → çerçeve 2× periyot
    d.assign ((size_t) maxLag + 2, 0.0);
    cmnd.assign ((size_t) maxLag + 2, 0.0);
    reset();
}

// d(τ) = Σ_{j<W} (x[j] − x[j+τ])²  (τ = 0..maxLag) ; CMND: d'(τ) = d(τ)·τ / Σ_{1..τ} d
PitchTracker::Result PitchTracker::analyze (const float* x, int n) noexcept
{
    Result r;
    if (n < W + maxLag) return r;
    // enerji kapısı: sessizlikte perde yok
    double e = 0.0; for (int j = 0; j < W; ++j) e += (double) x[j] * x[j];
    if (e / W < 1.0e-8) { prevHz = 0.0f; stable = 0; return r; }      // < −80 dBFS RMS

    d[0] = 0.0; cmnd[0] = 1.0;
    double running = 0.0;
    for (int tau = 1; tau <= maxLag; ++tau)
    {
        double s = 0.0;
        const float* a = x; const float* b = x + tau;
        for (int j = 0; j < W; ++j) { const double df = (double) a[j] - (double) b[j]; s += df * df; }
        d[(size_t) tau] = s;
        running += s;
        cmnd[(size_t) tau] = (running > 0.0) ? s * tau / running : 1.0;
    }
    // adım 4: ilk eşik-altı çukur (yerel minimuma kadar in)
    int tau = -1;
    for (int t = minLag; t <= maxLag; ++t)
    {
        if (cmnd[(size_t) t] < thr)
        {
            while (t + 1 <= maxLag && cmnd[(size_t) t + 1] < cmnd[(size_t) t]) ++t;
            tau = t; break;
        }
    }
    // eşik altı yoksa: global minimum (voiced sayılmaz ama periodicity raporlanır)
    int best = minLag;
    for (int t = minLag + 1; t <= maxLag; ++t) if (cmnd[(size_t) t] < cmnd[(size_t) best]) best = t;
    if (tau < 0) tau = best;

    // süreklilik: önceki kararlı perdenin lag'ında da derin çukur varsa ve aday 2×/0.5× ise onu tercih et
    if (prevHz > 0.0f && stable >= 2)
    {
        const int pl = (int) std::lround (sr / prevHz);
        if (pl >= minLag && pl <= maxLag)
        {
            int lp = pl;                                     // yerel minimum arama (±2)
            for (int t = std::max (minLag, pl - 2); t <= std::min (maxLag, pl + 2); ++t) if (cmnd[(size_t) t] < cmnd[(size_t) lp]) lp = t;
            const double ratio = (double) tau / (double) lp;
            const bool octave = (std::fabs (ratio - 2.0) < 0.06) || (std::fabs (ratio - 0.5) < 0.015);
            if (octave && cmnd[(size_t) lp] < thr + 0.08) tau = lp;
        }
    }

    const double lag = refineLag (tau);
    r.periodicity = (float) std::clamp (1.0 - cmnd[(size_t) tau], 0.0, 1.0);
    r.voiced = cmnd[(size_t) tau] < thr;
    r.hz = r.voiced ? (float) (sr / lag) : 0.0f;
    if (r.voiced)
    {
        if (prevHz > 0.0f && std::fabs (cents (r.hz, prevHz)) < 100.0f) ++stable; else stable = 0;
        prevHz = r.hz;
    }
    else { prevHz = 0.0f; stable = 0; }
    return r;
}

// parabolik tepe: komşu tamsayı gecikmelerin d değerleri (gürültü terimi hepsinde özdeş → sapmasız)
double PitchTracker::refineLag (int tau) const noexcept
{
    if (tau - 1 < 1 || tau + 1 > maxLag) return (double) tau;
    const double a = d[(size_t) tau - 1], b = d[(size_t) tau], c = d[(size_t) tau + 1];
    const double den = a - 2.0 * b + c;
    if (den > 1e-18)
    {
        const double delta = 0.5 * (a - c) / den;
        if (std::fabs (delta) < 1.0) return tau + delta;
    }
    return (double) tau;
}

} // namespace sweax
