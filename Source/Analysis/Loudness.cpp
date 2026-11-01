// SWEAX NEXT — Analysis/Loudness.cpp
#include "Loudness.h"
#include <algorithm>
#include <numeric>

namespace sweax
{

// ---------------- KWeighting ----------------
// BS.1770 ön-filtre analog sabitleri (48 kHz tablo katsayılarını üreten değerler; libebur128/pyloudnorm ile aynı)
namespace
{
    constexpr double K_SHELF_HZ = 1681.974450955533;
    constexpr double K_SHELF_DB = 3.999843853973347;
    constexpr double K_SHELF_Q  = 0.7071752369554196;
    constexpr double K_HPF_HZ   = 38.13547087602444;
    constexpr double K_HPF_Q    = 0.5003270373238773;
}

void KWeighting::prepare (double sampleRate, int numChannels)
{
    (void) numChannels;
    // ITU-R BS.1770-4 ön-filtre: analog prototip + bilinear (K = tan(π·f0/fs)). Bu, RBJ raf formülü DEĞİL —
    // ITU rafının kendine özgü Vb = Vh^0.49967 ara terimi vardır (libebur128 ile aynı); 48 kHz'de Tablo 1'i verir.
    {
        const double K  = std::tan (design::kPi * K_SHELF_HZ / sampleRate);
        const double Vh = std::pow (10.0, K_SHELF_DB / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / K_SHELF_Q + K * K;
        shelf.b0 = (Vh + Vb * K / K_SHELF_Q + K * K) / a0;
        shelf.b1 = 2.0 * (K * K - Vh) / a0;
        shelf.b2 = (Vh - Vb * K / K_SHELF_Q + K * K) / a0;
        shelf.a1 = 2.0 * (K * K - 1.0) / a0;
        shelf.a2 = (1.0 - K / K_SHELF_Q + K * K) / a0;
    }
    {
        const double K  = std::tan (design::kPi * K_HPF_HZ / sampleRate);
        const double a0 = 1.0 + K / K_HPF_Q + K * K;
        hpf.b0 = 1.0; hpf.b1 = -2.0; hpf.b2 = 1.0;          // ITU Tablo 2: pay {1, −2, 1} normalize edilmez
        hpf.a1 = 2.0 * (K * K - 1.0) / a0;
        hpf.a2 = (1.0 - K / K_HPF_Q + K * K) / a0;
    }
    for (auto& f : s1) f.setCoeffs (shelf);
    for (auto& f : s2) f.setCoeffs (hpf);
    reset();
}

void KWeighting::reset()
{
    for (auto& f : s1) f.reset();
    for (auto& f : s2) f.reset();
}

void KWeighting::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    const int ch = std::min (numChannels, 2);
    for (int c = 0; c < ch; ++c)
    {
        s1[(size_t) c].process (channels[c], numSamples);
        s2[(size_t) c].process (channels[c], numSamples);
    }
}

// ---------------- TruePeakMeter ----------------
namespace
{
    // I0 Bessel (Kaiser penceresi için) — seri açılımı, hazırlıkta bir kez
    double besselI0 (double x) noexcept
    {
        double sum = 1.0, term = 1.0; const double y = x * x * 0.25;
        for (int k = 1; k < 60; ++k) { term *= y / ((double) k * (double) k); sum += term; if (term < 1.0e-14 * sum) break; }
        return sum;
    }
}

void TruePeakMeter::prepare (int numChannels)
{
    (void) numChannels;
    // 4× polifaz ara değer: her faz p (1..3) örnek n_c + p/4 noktasını tahmin eder (n_c = TAPS/2 − 1, geçmişin ortası).
    // Çekirdek: sinc(kesim 0.5 devir/örnek) × Kaiser(β≈5.65 → ~60 dB yan lob). 4× alanında 64 tap ≈ geçiş bandı
    // 0.097·4fs → geçiş kenarı ≈ 0.39·fs (44.1k'da 17 kHz); fs/4'te düzlük < 0.01 dB (test: fs/4 sinüs → 0 dBTP).
    constexpr double beta = 5.65;
    const int L = TAPS;
    const double center = (double) (L / 2 - 1);
    const double halfWidth = (double) L * 0.5;
    const double i0b = besselI0 (beta);
    for (int p = 0; p < OVERSAMPLE; ++p)
    {
        const double tau = center + (double) p / OVERSAMPLE;
        double sum = 0.0;
        for (int k = 0; k < L; ++k)
        {
            const double t = (double) k - tau;                                   // örnek cinsinden uzaklık
            const double sinc = (std::fabs (t) < 1.0e-9) ? 1.0 : std::sin (design::kPi * t) / (design::kPi * t);
            const double r = t / halfWidth;
            const double w = (std::fabs (r) >= 1.0) ? 0.0 : besselI0 (beta * std::sqrt (1.0 - r * r)) / i0b;
            fir[(size_t) p][(size_t) k] = (float) (sinc * w);
            sum += sinc * w;
        }
        for (int k = 0; k < L; ++k) fir[(size_t) p][(size_t) k] = (float) (fir[(size_t) p][(size_t) k] / sum);   // DC = 1
    }
    reset();
}

void TruePeakMeter::reset()
{
    for (auto& h : hist) h.fill (0.0f);
    pos.fill (0);
    resetPeaks();
}

void TruePeakMeter::process (const float* const* channels, int numChannels, int numSamples) noexcept
{
    const int ch = std::min (numChannels, 2);
    for (int c = 0; c < ch; ++c)
    {
        auto& h = hist[(size_t) c];
        int& wp = pos[(size_t) c];
        const float* x = channels[c];
        for (int i = 0; i < numSamples; ++i)
        {
            const float v = x[i];
            maxSp = std::max (maxSp, std::fabs (v));
            h[(size_t) wp] = v;
            wp = (wp + 1) % TAPS;
            // faz 0 = örneğin kendisi (geçmişin ortası); 1..3 ara değerler
            for (int p = 1; p < OVERSAMPLE; ++p)
            {
                float acc = 0.0f;
                const auto& f = fir[(size_t) p];
                for (int k = 0; k < TAPS; ++k)
                    acc += f[(size_t) k] * h[(size_t) ((wp + k) % TAPS)];
                maxTp = std::max (maxTp, std::fabs (acc));
            }
            maxTp = std::max (maxTp, std::fabs (v));
        }
    }
}

// ---------------- LoudnessMeter ----------------
void LoudnessMeter::prepare (double sampleRate, int numChannels, int maxBlock)
{
    sr = sampleRate;
    channels = std::clamp (numChannels, 1, 2);
    hopSamples = std::max (1, (int) std::lround (sr * HOP_SEC));
    hopHistory.assign ((size_t) SHORT_HOPS, 0.0);
    blockLoudness.assign ((size_t) MAX_BLOCKS, -200.0f);
    scratch.assign ((size_t) std::max (1, maxBlock) * 2, 0.0f);
    scratchPtr[0] = scratch.data();
    scratchPtr[1] = scratch.data() + std::max (1, maxBlock);
    kw.prepare (sr, channels);
    tp.prepare (channels);
    reset();
}

void LoudnessMeter::reset()
{
    hopFill = 0; hopSum.fill (0.0);
    std::fill (hopHistory.begin(), hopHistory.end(), 0.0);
    hopHistPos = 0; hopHistCount = 0; blockCount = 0;
    momentary = shortTerm = maxMomentary = maxShortTerm = -100.0f;
    kw.reset(); tp.reset();
}

void LoudnessMeter::process (const float* const* channels_, int numChannels, int numSamples) noexcept
{
    if (numChannels <= 0 || numSamples <= 0) return;
    const int ch = std::min (std::min (numChannels, channels), 2);
    const int maxBlock = (int) (scratch.size() / 2);
    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (numSamples - done, maxBlock);
        // true-peak ham sinyalden
        const float* src[2] = { channels_[0] + done, ch > 1 ? channels_[1] + done : channels_[0] + done };
        tp.process (src, ch, n);
        // K-ağırlık kopya üzerinde
        for (int c = 0; c < ch; ++c) std::copy (src[c], src[c] + n, scratchPtr[(size_t) c]);
        float* kp[2] = { scratchPtr[0], scratchPtr[1] };
        kw.process (kp, ch, n);
        // hop toplamları
        int i = 0;
        while (i < n)
        {
            const int take = std::min (n - i, hopSamples - hopFill);
            for (int c = 0; c < ch; ++c)
            {
                double s2 = 0.0;
                const float* p = kp[c] + i;
                for (int k = 0; k < take; ++k) s2 += (double) p[k] * (double) p[k];
                hopSum[(size_t) c] += s2;
            }
            hopFill += take; i += take;
            if (hopFill >= hopSamples) hopCompleted();
        }
        done += n;
    }
}

void LoudnessMeter::hopCompleted() noexcept
{
    // kanal ağırlıkları L/R = 1.0 (BS.1770 G_L = G_R = 1); mono giriş tek kanal sayılır
    double weighted = 0.0;
    for (int c = 0; c < channels; ++c) weighted += hopSum[(size_t) c];
    hopSum.fill (0.0); hopFill = 0;
    hopHistory[(size_t) hopHistPos] = weighted / (double) hopSamples;   // hop ortalama karesi (kanal toplamı)
    hopHistPos = (hopHistPos + 1) % SHORT_HOPS;
    hopHistCount = std::min (hopHistCount + 1, SHORT_HOPS);

    auto ortalama = [this] (int hops) -> double
    {
        const int k = std::min (hops, hopHistCount);
        double s = 0.0;
        for (int j = 1; j <= k; ++j) s += hopHistory[(size_t) ((hopHistPos - j + SHORT_HOPS) % SHORT_HOPS)];
        return s / (double) std::max (1, k);
    };
    if (hopHistCount >= MOMENTARY_HOPS)
    {
        momentary = msToLufs (ortalama (MOMENTARY_HOPS));
        maxMomentary = std::max (maxMomentary, momentary);
        blockLoudness[blockCount % (size_t) MAX_BLOCKS] = momentary;   // 400 ms blok, 100 ms hop
        ++blockCount;
    }
    if (hopHistCount >= SHORT_HOPS)
    {
        shortTerm = msToLufs (ortalama (SHORT_HOPS));
        maxShortTerm = std::max (maxShortTerm, shortTerm);
    }
}

float LoudnessMeter::integratedLufs() const noexcept
{
    const size_t n = std::min<size_t> (blockCount, (size_t) MAX_BLOCKS);
    if (n == 0) return -100.0f;
    // BS.1770-4: mutlak kapı → ortalama → göreli kapı (−10 LU) → ortalama (kapılı bloklar, ms alanında)
    auto lufsToMs = [] (float l) { return std::pow (10.0, ((double) l + 0.691) / 10.0); };
    double sum = 0.0; size_t cnt = 0;
    for (size_t i = 0; i < n; ++i)
        if (blockLoudness[i] > ABS_GATE_LUFS) { sum += lufsToMs (blockLoudness[i]); ++cnt; }
    if (cnt == 0) return -100.0f;
    const float relGate = msToLufs (sum / (double) cnt) + REL_GATE_LU;
    double sum2 = 0.0; size_t cnt2 = 0;
    for (size_t i = 0; i < n; ++i)
        if (blockLoudness[i] > ABS_GATE_LUFS && blockLoudness[i] > relGate) { sum2 += lufsToMs (blockLoudness[i]); ++cnt2; }
    if (cnt2 == 0) return -100.0f;
    return msToLufs (sum2 / (double) cnt2);
}

} // namespace sweax
