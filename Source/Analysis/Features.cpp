// SWEAX NEXT — Analysis/Features.cpp
#include "Features.h"
#include <algorithm>
#include <cmath>

namespace sweax
{

namespace
{
    inline float toDb (double ms) noexcept { return (float) (10.0 * std::log10 (std::max (ms, 1.0e-12))); }
}

void FeatureExtractor::prepare (double sampleRate, int windowSize, int hopSize, int zeroPadFactor)
{
    sr = sampleRate;
    stft.prepare (sr, windowSize, hopSize, zeroPadFactor);
    lb.prepare (sr, stft.fftSize(), 12, 20.0, 0.45);
    bandMs.assign ((size_t) lb.numBands(), 0.0f);
    prevMag.assign ((size_t) stft.numBins(), 0.0f);
    mag.assign ((size_t) stft.numBins(), 0.0f);
    binHzTable.resize ((size_t) stft.numBins());
    for (int k = 0; k < stft.numBins(); ++k) binHzTable[(size_t) k] = k * stft.binHz();
    bandLog2.resize ((size_t) lb.numBands());
    for (int b = 0; b < lb.numBands(); ++b) bandLog2[(size_t) b] = std::log2 (lb.centerHz (b));
    binLo100 = std::max (1, (int) std::lround (100.0 / stft.binHz()));
    binHi10k = std::min (stft.numBins() - 1, (int) std::lround (10000.0 / stft.binHz()));
    bin4k = std::min (stft.numBins() - 1, (int) std::lround (4000.0 / stft.binHz()));
    // 4. derece Butterworth 120 Hz: Q = 0.5412, 1.3066
    lf1.setCoeffs (design::lowpass (sr, 120.0, 0.54119610));
    lf2.setCoeffs (design::lowpass (sr, 120.0, 1.30656296));
    tdRing.assign ((size_t) stft.hop(), 0.0f);
    lfRing.assign ((size_t) stft.hop(), 0.0f);
    reset();
}

void FeatureExtractor::reset()
{
    stft.reset();
    lf1.reset(); lf2.reset();
    std::fill (prevMag.begin(), prevMag.end(), 0.0f);
    std::fill (tdRing.begin(), tdRing.end(), 0.0f);
    std::fill (lfRing.begin(), lfRing.end(), 0.0f);
    ringPos = 0; frameIndex = 0; timePos = 0.0; havePrev = false;
}

void FeatureExtractor::push (const float* x, int n, const std::function<void (const FrameFeatures&)>& onFrame)
{
    // zaman alanı halkaları örnek örnek; STFT her hop dolduğunda kareyi verir ve halkalar (tam son hop) okunur.
    const int H = stft.hop();
    for (int i = 0; i < n; ++i)
    {
        const float v = x[i];
        tdRing[(size_t) ringPos] = v;
        lfRing[(size_t) ringPos] = lf2.processSample (lf1.processSample (v));
        ringPos = (ringPos + 1) % H;
        timePos += 1.0 / sr;
        stft.push (&v, 1, [&] (const float* power, int bins, long)
        {
            FrameFeatures f;
            f.frame = frameIndex++;
            f.timeSec = timePos;
            double sumSq = 0.0; float peak = 0.0f, lfPeak = 0.0f;
            for (int k = 0; k < H; ++k)
            {
                const float s = tdRing[(size_t) k]; sumSq += (double) s * s; peak = std::max (peak, std::fabs (s));
                lfPeak = std::max (lfPeak, std::fabs (lfRing[(size_t) k]));
            }
            const double ms = sumSq / H;
            f.rms = (float) std::sqrt (ms);
            f.peakAbs = peak;
            f.energyDb = toDb (ms);
            f.crestDb = (f.rms > 1.0e-7f) ? 20.0f * std::log10 (std::max (peak, 1.0e-7f) / f.rms) : 0.0f;
            f.lfEnvDb = 20.0f * std::log10 (std::max (lfPeak, 1.0e-6f));
            computeSpectral (power, bins, f);
            onFrame (f);
        });
    }
}

void FeatureExtractor::computeSpectral (const float* power, int bins, FrameFeatures& f)
{
    // bantlar
    lb.apply (power, bandMs.data());
    f.numBands = std::min (lb.numBands(), FrameFeatures::MAX_BANDS);
    for (int b = 0; b < f.numBands; ++b) f.bandsDb[b] = toDb (bandMs[(size_t) b]);

    // toplam / centroid / spread / rolloff / HF oranı (bin 1'den itibaren; DC hariç)
    double total = 0.0, wsum = 0.0, hf = 0.0;
    for (int k = 1; k < bins; ++k) { const double p = power[k]; total += p; wsum += p * binHzTable[(size_t) k]; if (k >= bin4k) hf += p; }
    f.spectralEnergyDb = toDb (total);
    if (total > 1.0e-14)
    {
        f.centroidHz = (float) (wsum / total);
        double v = 0.0;
        for (int k = 1; k < bins; ++k) { const double d = binHzTable[(size_t) k] - f.centroidHz; v += power[k] * d * d; }
        f.spreadHz = (float) std::sqrt (v / total);
        double acc = 0.0; f.rolloffHz = (float) binHzTable[(size_t) bins - 1];
        for (int k = 1; k < bins; ++k) { acc += power[k]; if (acc >= 0.85 * total) { f.rolloffHz = (float) binHzTable[(size_t) k]; break; } }
        f.hfRatio = (float) (hf / total);
    }
    else { f.centroidHz = 0.0f; f.spreadHz = 0.0f; f.rolloffHz = 0.0f; f.hfRatio = 0.0f; }

    // düzlük (100 Hz – 10 kHz): geometrik / aritmetik ortalama
    {
        double logSum = 0.0, lin = 0.0; int cnt = 0;
        for (int k = binLo100; k <= binHi10k; ++k) { const double p = std::max ((double) power[k], 1.0e-14); logSum += std::log (p); lin += p; ++cnt; }
        f.flatness = cnt > 0 ? (float) (std::exp (logSum / cnt) / (lin / cnt)) : 0.0f;
    }

    // eğim: bant dB vs log2(f), 200 Hz – 10 kHz (dB/oktav), yalnız anlamlı enerjili bantlar (en yüksek −60 dB)
    {
        double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0; int cnt = 0;
        float mx = -200.0f; for (int b = 0; b < f.numBands; ++b) mx = std::max (mx, f.bandsDb[b]);
        for (int b = 0; b < f.numBands; ++b)
        {
            const double fc = lb.centerHz (b);
            if (fc < 200.0 || fc > 10000.0 || f.bandsDb[b] < mx - 60.0f) continue;
            const double xl = bandLog2[(size_t) b], y = f.bandsDb[b];
            sx += xl; sy += y; sxx += xl * xl; sxy += xl * y; ++cnt;
        }
        const double den = cnt * sxx - sx * sx;
        f.slopeDbPerOct = (cnt >= 3 && den > 1.0e-9) ? (float) ((cnt * sxy - sx * sy) / den) : 0.0f;
    }

    // akı: yarım-dalga doğrultulmuş genlik artışı, mevcut enerjiyle normalize (0..1 civarı)
    {
        double inc = 0.0, cur = 0.0;
        for (int k = 1; k < bins; ++k)
        {
            const float m = std::sqrt (std::max (power[k], 0.0f));
            mag[(size_t) k] = m;
            const float dm = m - prevMag[(size_t) k];
            if (havePrev && dm > 0.0f) inc += (double) dm * dm;
            cur += (double) m * m;
        }
        f.flux = (havePrev && cur > 1.0e-14) ? (float) std::min (1.0, inc / cur) : 0.0f;
        std::swap (mag, prevMag);
        havePrev = true;
    }
}

} // namespace sweax
