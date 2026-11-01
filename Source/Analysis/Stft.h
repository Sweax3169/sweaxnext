#pragma once
// SWEAX NEXT — Analysis/Stft.h
// Kısa zamanlı Fourier dönüşümü (worker tarafı; RT'de kullanılmaz ama tahsissiz çalışır).
//  • Hann penceresi, isteğe bağlı sıfır doldurma (zeroPadFactor: 1/2/4 — LF rezonans için §3.2).
//  • GÜÇ SPEKTRUMU PARSEVAL-TUTARLIDIR: Σ_k P[k] ≈ pencereli sinyalin ortalama karesi (mean-square).
//    Böylece bant enerjileri zaman-alanı RMS/loudness ile aynı birimdedir (dB = 10·log10).
//  • LogBands: 1/12 oktav bant eşleyici (20 Hz … 0.45·fs), bin aralıkları prepare'da hesaplanır.
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <functional>
#include <cmath>

namespace sweax
{

class Stft
{
public:
    // sizeSamples: pencere uzunluğu (2'nin kuvveti), hop: atlama, zeroPadFactor: FFT = size·factor
    void prepare (double sampleRate, int sizeSamples, int hopSamples, int zeroPadFactor = 1);
    void reset();

    int  windowSize() const noexcept { return N; }
    int  fftSize() const noexcept { return Nfft; }
    int  numBins() const noexcept { return Nfft / 2 + 1; }
    int  hop() const noexcept { return hopN; }
    double binHz() const noexcept { return sr / (double) Nfft; }
    double sampleRate() const noexcept { return sr; }

    // Örnekleri besler; her hop dolduğunda onFrame(power, numBins, frameIndex) çağrılır.
    // power: mean-square birimli tek taraflı güç spektrumu (bu çağrıdan sonra geçersiz olur).
    void push (const float* x, int n, const std::function<void (const float* power, int bins, long frame)>& onFrame);

    // Süreden pencere boyu: nextPow2(saniye·fs) — mimari §3.2 (≈85 ms uzun / ≈21 ms kısa)
    static int sizeForDuration (double seconds, double sampleRate) noexcept
    {
        int n = 1;
        while (n < seconds * sampleRate) n <<= 1;
        return n;
    }

private:
    void computeFrame();
    double sr { 48000.0 };
    int N { 4096 }, Nfft { 4096 }, hopN { 1024 };
    std::vector<float> ring;                 // son N örnek
    int ringPos { 0 }, sinceHop { 0 };
    long frames { 0 };
    bool primed { false };
    std::vector<float> window, fftData, power;
    double windowPowerSum { 1.0 };
    std::unique_ptr<juce::dsp::FFT> fft;
    std::function<void (const float*, int, long)> cb;
};

// 1/12 oktav bant eşleyici
class LogBands
{
public:
    void prepare (double sampleRate, int fftSize, int bandsPerOctave = 12, double fLow = 20.0, double fHighFraction = 0.45);
    int  numBands() const noexcept { return (int) lo.size(); }
    double centerHz (int band) const noexcept { return centers[(size_t) band]; }
    double lowHz (int band) const noexcept { return edges[(size_t) band]; }
    double highHz (int band) const noexcept { return edges[(size_t) band + 1]; }
    // power[] (numBins) → out[] (numBands), mean-square birimi; boş bant = en yakın bin
    void apply (const float* power, float* out) const noexcept;
    int  bandForHz (double hz) const noexcept;
private:
    std::vector<int> lo, hi;          // bin aralığı [lo, hi)
    std::vector<double> centers, edges;
};

} // namespace sweax
