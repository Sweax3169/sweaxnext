#pragma once
// SWEAX NEXT — DSP/HalfBand.h
// 2× oversampling yarım-bant FIR (25 tap, Kaiser β=9, kesim fs/4, DC birim). Gecikme 12 örnek (2× hızda),
// yani 1× tabanda sabit 12 örnek latency. Saturator ve DirtModule ortak kullanır (OutputStage ile aynı tasarım).
#include <array>
#include <cmath>

namespace sweax
{

struct HalfBand25
{
    static constexpr int LEN = 25, DELAY = 12;
    std::array<float, LEN> h {};
    std::array<float, LEN> z {};
    int zp { 0 };

    static double besselI0 (double x) noexcept
    {
        double sum = 1.0, term = 1.0; const double y = x * x * 0.25;
        for (int k = 1; k < 60; ++k) { term *= y / ((double) k * (double) k); sum += term; if (term < 1.0e-14 * sum) break; }
        return sum;
    }

    void design() noexcept
    {
        constexpr double PI = 3.141592653589793, beta = 9.0;
        const double i0b = besselI0 (beta);
        double sum = 0.0;
        for (int k = 0; k < LEN; ++k)
        {
            const double t = (double) k - (double) DELAY;
            const double sinc = (std::fabs (t) < 1e-9) ? 0.5 : std::sin (PI * t * 0.5) / (PI * t);
            const double r = t / (double) (DELAY + 1);
            const double w = (std::fabs (r) >= 1.0) ? 0.0 : besselI0 (beta * std::sqrt (1.0 - r * r)) / i0b;
            h[(size_t) k] = (float) (sinc * w);
            sum += h[(size_t) k];
        }
        for (auto& v : h) v = (float) (v / sum);
        reset();
    }
    void reset() noexcept { z.fill (0.0f); zp = 0; }
    inline float tick (float x) noexcept
    {
        z[(size_t) zp] = x;
        float acc = 0.0f;
        int idx = zp;
        for (int k = 0; k < LEN; ++k) { acc += h[(size_t) k] * z[(size_t) idx]; if (--idx < 0) idx = LEN - 1; }
        zp = (zp + 1) % LEN;
        return acc;
    }
};

} // namespace sweax
