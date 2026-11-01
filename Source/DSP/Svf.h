#pragma once
// SWEAX NEXT — DSP/Svf.h
// TPT (topology-preserving transform) state-variable filtre — Zavalishin / A. Simper formülasyonu.
// Dinamik EQ bantları ve sidechain filtreleri için: katsayılar (fc, Q) sabitken kazanç G örnek başına
// rampalanır; katsayı yeniden hesabı yok → zipper yok (mimari §4 "kazançtan ayrıştırılmış topoloji").
//  bell(x, G) = x + (G − 1)·k·bp   (fc'de tam G, uzakta 1; k = 1/Q)
#include <cmath>
#include <algorithm>

namespace sweax
{

class Svf
{
public:
    struct Out { float lp, bp, hp; };

    void setCoeffs (double sampleRate, double fcHz, double q) noexcept
    {
        const double fc = std::clamp (fcHz, 10.0, sampleRate * 0.49);
        g = std::tan (3.14159265358979323846 * fc / sampleRate);
        k = 1.0 / std::clamp (q, 0.05, 40.0);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() noexcept { ic1eq = ic2eq = 0.0; }

    inline Out process (float v0) noexcept
    {
        const double v3 = (double) v0 - ic2eq;
        const double v1 = a1 * ic1eq + a2 * v3;
        const double v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0 * v1 - ic1eq;
        ic2eq = 2.0 * v2 - ic2eq;
        Out o; o.lp = (float) v2; o.bp = (float) v1; o.hp = (float) ((double) v0 - k * v1 - v2);
        return o;
    }
    // fc'de birim kazançlı bant geçiren (k·bp)
    inline float bandpassNormalized (float v0) noexcept { return (float) (k * process (v0).bp); }
    // bell: fc'de G, uzakta 1
    inline float bell (float v0, float gLin) noexcept { return v0 + (gLin - 1.0f) * bandpassNormalized (v0); }
    void sanitize() noexcept { if (! (std::isfinite (ic1eq) && std::isfinite (ic2eq))) reset(); }
    double kValue() const noexcept { return k; }

private:
    double g { 0.1 }, k { 1.0 }, a1 { 1.0 }, a2 { 0.0 }, a3 { 0.0 };
    double ic1eq { 0.0 }, ic2eq { 0.0 };
};

} // namespace sweax
