#pragma once
// SWEAX NEXT — DSP/WidthModule.h
// M/S genişlik (M2): Side ölçekleme — saf orta/yan matris, faz oyunu yok → mono-uyumlu.
// amount 1 = birim; <1 daraltır (0 = mono), >1 yan içeriği artırır (doubler/reverb'in ürettiği).
// Kuru mono sinyalde Side=0 → hiçbir şey yapmaz (dürüst davranış). Latency 0.
#include "Module.h"

namespace sweax
{

struct WidthParams
{
    bool  on { false };
    float amount { 1.0f };           // 0..2
};

class WidthModule : public ModuleBase
{
public:
    void prepare (double sampleRate, int maxBlock, int numChannels)
    {
        prepareBase (sampleRate, maxBlock, numChannels, 0);
        setDryDelay (0);
        amtS.prepare (sampleRate, 20.0);
        amtS.setCurrentAndTarget (1.0f);
    }
    int latencySamples (Quality) const noexcept { return 0; }
    void setQuality (Quality) noexcept {}
    void setParams (const WidthParams& p) noexcept { amtS.setTarget (std::clamp (p.amount, 0.0f, 2.0f)); }

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override
    {
        if (numCh < 2) { amtS.skip (n); return; }
        for (int i = 0; i < n; ++i)
        {
            const float w = amtS.next();
            const float mid = 0.5f * (ch[0][i] + ch[1][i]);
            const float side = 0.5f * (ch[0][i] - ch[1][i]) * w;
            ch[0][i] = mid + side;
            ch[1][i] = mid - side;
        }
    }
    void idleWet (const float* const*, int, int n) noexcept override { amtS.skip (n); }
    void resetWet() override {}

private:
    LinearSmoother amtS;
};

} // namespace sweax
