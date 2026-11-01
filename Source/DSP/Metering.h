#pragma once
// SWEAX NEXT — DSP/Metering.h
// Ölçüm (mimari §4.1): tepe, true-peak, RMS, BS.1770 momentary/short-term (host hızında K-filtre), korelasyon.
//  • Ses iş parçacığı yazar (atomic<float>, relaxed), UI okur. Tahsis yalnız prepare()'da.
//  • Tepe değerleri tutmalı (UI okuduğunda sıfırlanır: readAndResetPeak) ve düşüşlü anlık.
//  • GR geçmişi: "latest-N ring" (ses yazar, UI yırtılmaya toleranslı okur) — modüllerin GR'lerini zincir yazar.
#include "../Analysis/Loudness.h"
#include <atomic>
#include <array>
#include <vector>
#include <cmath>

namespace sweax
{

class Metering
{
public:
    static constexpr int GR_RING = 256;

    void prepare (double sampleRate, int numChannels, int maxBlock)
    {
        sr = sampleRate; chans = std::clamp (numChannels, 1, 2);
        loud.prepare (sr, chans, std::max (1, maxBlock));
        tp.prepare (chans);
        reset();
    }
    void reset()
    {
        loud.reset(); tp.reset();
        peakDb.store (-100.0f); peakHoldDb.store (-100.0f); rmsDb.store (-100.0f); truePeakDb.store (-100.0f);
        momentary.store (-100.0f); shortTerm.store (-100.0f); correlation.store (1.0f);
        for (auto& g : grRing) g.store (0.0f);
        grPos.store (0);
    }

    // RT: işlenmiş çıkış (ölçüm K-filtre kopyası LoudnessMeter içinde)
    void process (const float* const* ch, int numCh, int n) noexcept
    {
        const int c = std::min (numCh, chans);
        float pk = 0.0f; double s0 = 0.0, s1 = 0.0, s01 = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const float a = ch[0][i]; pk = std::max (pk, std::fabs (a)); s0 += (double) a * a;
            if (c > 1) { const float b = ch[1][i]; pk = std::max (pk, std::fabs (b)); s1 += (double) b * b; s01 += (double) a * b; }
        }
        const float pkDb = 20.0f * std::log10 (std::max (pk, 1.0e-6f));
        const float decay = (float) std::pow (0.5, (double) n / (sr * 0.3));
        peakDb.store (std::max (pkDb, peakDb.load (std::memory_order_relaxed) * decay + (1.0f - decay) * -100.0f), std::memory_order_relaxed);
        peakHoldDb.store (std::max (pkDb, peakHoldDb.load (std::memory_order_relaxed)), std::memory_order_relaxed);
        const double ms = c > 1 ? (s0 + s1) / (2.0 * n) : s0 / n;
        rmsDb.store (10.0f * std::log10 ((float) ms + 1.0e-12f), std::memory_order_relaxed);
        if (c > 1) correlation.store ((s0 > 1e-12 && s1 > 1e-12) ? (float) (s01 / std::sqrt (s0 * s1)) : 1.0f, std::memory_order_relaxed);
        loud.process (ch, c, n);
        tp.process (ch, c, n);
        momentary.store (loud.momentaryLufs(), std::memory_order_relaxed);
        shortTerm.store (loud.shortTermLufs(), std::memory_order_relaxed);
        truePeakDb.store (std::max (tp.maxTruePeakDb(), truePeakDb.load (std::memory_order_relaxed)), std::memory_order_relaxed);
    }
    // RT: modül GR'leri (dB, ≤ 0) — blok başına bir örnek
    void pushGainReduction (float eqDb, float levDb, float compADb, float dsDb, float compBDb, float limDb,
                            float gateDb = 0.0f) noexcept
    {
        const int p = grPos.load (std::memory_order_relaxed);
        grRing[(size_t) p].store (eqDb + compADb + dsDb + compBDb + limDb + levDb + gateDb, std::memory_order_relaxed);
        grEq.store (eqDb, std::memory_order_relaxed); grLev.store (levDb, std::memory_order_relaxed); grCompA.store (compADb, std::memory_order_relaxed);
        grDs.store (dsDb, std::memory_order_relaxed); grCompB.store (compBDb, std::memory_order_relaxed); grLim.store (limDb, std::memory_order_relaxed);
        grGate.store (gateDb, std::memory_order_relaxed);
        grPos.store ((p + 1) % GR_RING, std::memory_order_release);
    }

    // UI
    float peak() const noexcept { return peakDb.load (std::memory_order_relaxed); }
    float readAndResetPeakHold() noexcept { return peakHoldDb.exchange (-100.0f, std::memory_order_relaxed); }
    float readAndResetTruePeak() noexcept { const float v = truePeakDb.exchange (-100.0f, std::memory_order_relaxed); tp.resetPeaks(); return v; }
    float rms() const noexcept { return rmsDb.load (std::memory_order_relaxed); }
    float momentaryLufs() const noexcept { return momentary.load (std::memory_order_relaxed); }
    float shortTermLufs() const noexcept { return shortTerm.load (std::memory_order_relaxed); }
    float stereoCorrelation() const noexcept { return correlation.load (std::memory_order_relaxed); }
    float gr (int which) const noexcept   // 0 eq,1 lev,2 compA,3 ds,4 compB,5 lim,6 gate
    {
        switch (which) { case 0: return grEq.load(); case 1: return grLev.load(); case 2: return grCompA.load(); case 3: return grDs.load(); case 4: return grCompB.load(); case 6: return grGate.load(); default: return grLim.load(); }
    }
    int grHistory (float* out, int maxCount) const noexcept    // en yeni son: kronolojik sıra
    {
        const int cnt = std::min (maxCount, GR_RING);
        const int p = grPos.load (std::memory_order_acquire);
        for (int i = 0; i < cnt; ++i) out[i] = grRing[(size_t) ((p - cnt + i + GR_RING) % GR_RING)].load (std::memory_order_relaxed);
        return cnt;
    }

private:
    double sr { 48000.0 }; int chans { 2 };
    LoudnessMeter loud; TruePeakMeter tp;
    std::atomic<float> peakDb { -100.0f }, peakHoldDb { -100.0f }, rmsDb { -100.0f }, truePeakDb { -100.0f };
    std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f }, correlation { 1.0f };
    std::atomic<float> grEq { 0.0f }, grLev { 0.0f }, grCompA { 0.0f }, grDs { 0.0f }, grCompB { 0.0f }, grLim { 0.0f }, grGate { 0.0f };
    std::array<std::atomic<float>, GR_RING> grRing {};
    std::atomic<int> grPos { 0 };
};

} // namespace sweax
