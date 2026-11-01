#pragma once
// SWEAX NEXT — Analysis/Features.h
// Kare başına özellik vektörü (mimari §3.2): uzun pencere STFT (≈85 ms) üzerinden spektral özellikler +
// aynı hop üzerinde zaman-alanı özellikleri. Spektrogram SAKLANMAZ; her kare hesaplanır, tüketici toplar.
//  • bandsDb: 1/12 oktav bant enerjileri (dB, mean-square), LogBands ile.
//  • centroid/spread/rolloff(85 %)/flatness(100 Hz–10 kHz)/slope(log-log regresyon 200 Hz–10 kHz, dB/okt)/
//    flux (yarım-dalga doğrultulmuş genlik farkı, normalize)/hfRatio (≥4 kHz / toplam).
//  • LF geçici zarfı ZAMAN ALANINDA: 4. derece Butterworth 120 Hz LPF (iki biquad), |y| tepe → dB (§3.2: STFT bin'i
//    plosive için çok yavaş/kaba). lfEnvDb = hop içindeki en yüksek zarf.
//  • Tahsis yalnız prepare()'da; worker iş parçacığı.
#include "Stft.h"
#include "../Core/Biquad.h"
#include <functional>
#include <vector>

namespace sweax
{

struct FrameFeatures
{
    static constexpr int MAX_BANDS = 128;
    long   frame { 0 };
    double timeSec { 0.0 };            // karenin bitiş zamanı (akış içi, saniye)
    // zaman alanı (hop üzerinde)
    float rms { 0.0f }, peakAbs { 0.0f }, crestDb { 0.0f }, energyDb { -120.0f };
    float lfEnvDb { -120.0f };
    // spektral (pencere üzerinde)
    int   numBands { 0 };
    float bandsDb[MAX_BANDS] {};
    float centroidHz { 0.0f }, spreadHz { 0.0f }, rolloffHz { 0.0f }, flatness { 0.0f };
    float slopeDbPerOct { 0.0f }, flux { 0.0f }, hfRatio { 0.0f };
    float spectralEnergyDb { -120.0f };  // Σ P (pencere, mean-square) dB
};

class FeatureExtractor
{
public:
    void prepare (double sampleRate, int windowSize, int hopSize, int zeroPadFactor = 1);
    void reset();
    void push (const float* x, int n, const std::function<void (const FrameFeatures&)>& onFrame);

    const LogBands& bands() const noexcept { return lb; }
    int hop() const noexcept { return stft.hop(); }
    int windowSize() const noexcept { return stft.windowSize(); }
    double sampleRate() const noexcept { return sr; }

private:
    void computeSpectral (const float* power, int bins, FrameFeatures& f);
    double sr { 48000.0 };
    Stft stft;
    LogBands lb;
    std::vector<float> bandMs, prevMag, mag;
    std::vector<double> binHzTable, bandLog2;
    Biquad lf1, lf2;
    // zaman-alanı istatistikleri TAM OLARAK son hop üzerinden (halka): parça sınırı / yeniden doldurma sonucu değiştirmez
    std::vector<float> tdRing, lfRing; int ringPos { 0 };
    long frameIndex { 0 };
    double timePos { 0.0 };
    int binLo100 { 1 }, binHi10k { 1 }, bin4k { 1 };
    bool havePrev { false };
};

} // namespace sweax
