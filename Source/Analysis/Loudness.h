#pragma once
// SWEAX NEXT — Analysis/Loudness.h
// ITU-R BS.1770-4 / EBU R128 loudness ölçümü — GERÇEK, sahte değil (eski eklentinin "-0.691 + 10log10(rms²)"
// hilesinin yerine). Mimari §3.2 / §4.1:
//  • K-ağırlık HOST örnekleme hızında yeniden tasarlanır: analog prototip sabitleri (shelf 1681.97 Hz +3.9998 dB
//    Q 0.7072; HPF 38.135 Hz Q 0.5003) RBJ formülleriyle bilinear+prewarp → 48 kHz'de ITU tablo katsayılarını
//    birebir verir, 44.1/96/192 kHz'de doğru kalır (sabit 48k katsayısı 96k'da belirgin sapardı).
//  • Momentary 400 ms / short-term 3 s: 100 ms hop ile %75 örtüşen bloklar.
//  • Integrated: mutlak kapı −70 LKFS, ardından göreli kapı (ortalama − 10 LU).
//  • True-peak: 4× polifaz Kaiser-sinc ara değer (ITU Annex 2 ruhunda; fs/4'te düz), kanal başına.
//  • RT güvenli: tüm tahsis prepare()'da; blok geçmişi sabit kapasiteli halka (2 saat @ 10 blok/s).
#include "../Core/Biquad.h"
#include <vector>
#include <array>
#include <cmath>

namespace sweax
{

class KWeighting
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();
    // yerinde: buffer[ch][i]
    void process (float* const* channels, int numChannels, int numSamples) noexcept;
    const BiquadCoeffs& shelfCoeffs() const noexcept { return shelf; }
    const BiquadCoeffs& hpfCoeffs() const noexcept { return hpf; }
    void sanitize() noexcept { for (auto& f : s1) f.sanitize(); for (auto& f : s2) f.sanitize(); }
private:
    BiquadCoeffs shelf, hpf;
    std::array<Biquad, 2> s1, s2;   // kanal başına iki kademe
};

class TruePeakMeter
{
public:
    static constexpr int OVERSAMPLE = 4;
    static constexpr int TAPS = 16;             // faz başına (4× alanında 64 tap Kaiser-sinc)
    void prepare (int numChannels);
    void reset();
    void process (const float* const* channels, int numChannels, int numSamples) noexcept;
    float maxTruePeakLinear() const noexcept { return maxTp; }
    float maxTruePeakDb() const noexcept { return 20.0f * std::log10 (std::max (maxTp, 1.0e-9f)); }
    float maxSamplePeakDb() const noexcept { return 20.0f * std::log10 (std::max (maxSp, 1.0e-9f)); }
    void resetPeaks() noexcept { maxTp = 0.0f; maxSp = 0.0f; }
private:
    std::array<std::array<float, TAPS>, OVERSAMPLE> fir {};        // fir[faz][tap]
    std::array<std::array<float, TAPS>, 2> hist {};                 // kanal başına geçmiş (halka)
    std::array<int, 2> pos {};
    float maxTp { 0.0f }, maxSp { 0.0f };
};

class LoudnessMeter
{
public:
    static constexpr double HOP_SEC = 0.1;
    static constexpr int MOMENTARY_HOPS = 4;    // 400 ms
    static constexpr int SHORT_HOPS = 30;       // 3 s
    static constexpr int MAX_BLOCKS = 72000;    // 2 saat @ 10/s — integrated için blok geçmişi
    static constexpr float ABS_GATE_LUFS = -70.0f;
    static constexpr float REL_GATE_LU = -10.0f;

    void prepare (double sampleRate, int numChannels, int maxBlock);
    void reset();
    // Ham (ağırlıksız) ses girer; içeride K-ağırlık uygulanır (kopya tampon üzerinde).
    void process (const float* const* channels, int numChannels, int numSamples) noexcept;

    float momentaryLufs() const noexcept { return momentary; }
    float shortTermLufs() const noexcept { return shortTerm; }
    float maxMomentaryLufs() const noexcept { return maxMomentary; }
    float maxShortTermLufs() const noexcept { return maxShortTerm; }
    // Gate'li integrated — talep üzerine hesaplanır (UI/worker); RT'de ÇAĞIRMA (O(n) blok geçmişi).
    float integratedLufs() const noexcept;
    float truePeakDb() const noexcept { return tp.maxTruePeakDb(); }
    float samplePeakDb() const noexcept { return tp.maxSamplePeakDb(); }
    int   blocksCollected() const noexcept { return (int) std::min<size_t> (blockCount, MAX_BLOCKS); }

    static float msToLufs (double meanSquare) noexcept
    {
        return (float) (-0.691 + 10.0 * std::log10 (std::max (meanSquare, 1.0e-20)));
    }

private:
    void hopCompleted() noexcept;

    double sr { 48000.0 };
    int channels { 2 }, hopSamples { 4800 }, hopFill { 0 };
    std::array<double, 2> hopSum {};                 // kanal başına Σx² (K-ağırlıklı) — mevcut hop
    std::vector<double> hopHistory;                  // son SHORT_HOPS hop'un kanal-ağırlıklı Σx²'si (halka)
    int hopHistPos { 0 }, hopHistCount { 0 };
    std::vector<float> blockLoudness;                // 400 ms blok loudness'ları (halka, MAX_BLOCKS)
    size_t blockCount { 0 };
    float momentary { -100.0f }, shortTerm { -100.0f }, maxMomentary { -100.0f }, maxShortTerm { -100.0f };
    KWeighting kw;
    TruePeakMeter tp;
    std::vector<float> scratch;                      // K-ağırlık kopyası (prepare'da maxBlock×ch)
    std::array<float*, 2> scratchPtr {};
};

} // namespace sweax
