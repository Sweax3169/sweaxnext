// SWEAX NEXT — Analysis/Stft.cpp
#include "Stft.h"
#include <algorithm>

namespace sweax
{

void Stft::prepare (double sampleRate, int sizeSamples, int hopSamples, int zeroPadFactor)
{
    sr = sampleRate;
    N = std::max (16, sizeSamples);
    Nfft = N * std::clamp (zeroPadFactor, 1, 8);
    hopN = std::clamp (hopSamples, 1, N);
    int order = 0; while ((1 << order) < Nfft) ++order;
    Nfft = 1 << order;
    fft = std::make_unique<juce::dsp::FFT> (order);
    ring.assign ((size_t) N, 0.0f);
    fftData.assign ((size_t) Nfft * 2, 0.0f);
    power.assign ((size_t) Nfft / 2 + 1, 0.0f);
    window.assign ((size_t) N, 0.0f);
    windowPowerSum = 0.0;
    for (int i = 0; i < N; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * (double) i / (double) N); // periyodik Hann
        window[(size_t) i] = (float) w;
        windowPowerSum += w * w;
    }
    reset();
}

void Stft::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    ringPos = 0; sinceHop = 0; frames = 0; primed = false;
}

void Stft::push (const float* x, int n, const std::function<void (const float*, int, long)>& onFrame)
{
    for (int i = 0; i < n; ++i)
    {
        ring[(size_t) ringPos] = x[i];
        ringPos = (ringPos + 1) % N;
        if (ringPos == 0) primed = true;
        if (++sinceHop >= hopN && primed)
        {
            sinceHop = 0;
            computeFrame();
            onFrame (power.data(), numBins(), frames++);
        }
    }
}

void Stft::computeFrame()
{
    // pencereli kare: halkadaki en eski örnek ringPos'ta başlar
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    for (int i = 0; i < N; ++i)
        fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % N)] * window[(size_t) i];
    fft->performRealOnlyForwardTransform (fftData.data(), true);
    // JUCE gerçek FFT çıktısı: [re0, im0, re1, im1, ...] (Nfft/2+1 karmaşık)
    // Parseval tutarlılığı: Σ_k(tüm Nfft) |X_k|² = Nfft·Σ|x·w|² → P[k] = 2|X|²/(Nfft·Σw²) (k=1..Nfft/2−1), uçlarda 1.
    // (sıfır doldurmada Nfft ≠ N; enerji daha çok bin'e yayılır, toplam korunur)
    const double norm = 1.0 / ((double) Nfft * windowPowerSum);
    const int bins = Nfft / 2 + 1;
    for (int k = 0; k < bins; ++k)
    {
        const double re = fftData[(size_t) (2 * k)], im = fftData[(size_t) (2 * k + 1)];
        const double p = (re * re + im * im) * norm;
        power[(size_t) k] = (float) ((k == 0 || k == Nfft / 2) ? p : 2.0 * p);
    }
}

// ---------------- LogBands ----------------
void LogBands::prepare (double sampleRate, int fftSize, int bandsPerOctave, double fLow, double fHighFraction)
{
    lo.clear(); hi.clear(); centers.clear(); edges.clear();
    const double binHz = sampleRate / (double) fftSize;
    const int bins = fftSize / 2 + 1;
    const double fHigh = sampleRate * fHighFraction;
    const double ratio = std::pow (2.0, 1.0 / (double) bandsPerOctave);
    double f = fLow;
    edges.push_back (f);
    while (f * ratio <= fHigh)
    {
        const double fn = f * ratio;
        int a = (int) std::floor (f / binHz), b = (int) std::floor (fn / binHz);
        a = std::clamp (a, 0, bins - 1); b = std::clamp (b, a + 1, bins);
        lo.push_back (a); hi.push_back (b);
        centers.push_back (std::sqrt (f * fn));
        edges.push_back (fn);
        f = fn;
    }
}

void LogBands::apply (const float* power, float* out) const noexcept
{
    for (size_t b = 0; b < lo.size(); ++b)
    {
        double s = 0.0;
        for (int k = lo[b]; k < hi[b]; ++k) s += power[k];
        out[b] = (float) s;
    }
}

int LogBands::bandForHz (double hz) const noexcept
{
    for (size_t b = 0; b < lo.size(); ++b)
        if (hz < edges[b + 1]) return (int) b;
    return (int) lo.size() - 1;
}

} // namespace sweax
