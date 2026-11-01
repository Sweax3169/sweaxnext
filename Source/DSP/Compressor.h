#pragma once
// SWEAX NEXT — DSP/Compressor.h
// Feed-forward log-alanı kompresör (mimari §4.1):
//  • Dedektör: peak (0.5 ms ön yumuşatma) / RMS (10 ms) / hibrit (dB ortalaması); sidechain HPF; stereo link (mono toplam).
//  • Kazanç hesaplayıcı: soft knee 0–24 dB, oran 1–20.
//  • Zarf: attack/release dB alanında (%63 = 1τ), 5 ms HOLD (guardrail: 100 Hz'de THD < %1), program-bağımlı release
//    (derin/sürekli GR → release ×1.0, hafif GR → ×0.5).
//  • Lookahead: LATENCY SABİT = f(mod): CANLI 0, YÜKSEK/ULTRA 5 ms. Lookahead parametresi (0–5 ms) dedektörün ses
//    yoluna göre ne kadar önde okunacağını belirler (kazanç gecikme hattı D − L) — latency'yi DEĞİŞTİRMEZ.
//    Guardrail: attack < 1 ms yalnız lookahead ≥ 1 ms ile (CANLI'da attack ≥ 1 ms'e klemplenir); release ≥ 30 ms.
//  • Makeup, mix (kuru yol aynı gecikme). Character: Clean / Smooth (knee +6, attack ≥ 5 ms) / Punch (knee −3, attack
//    ≥ 0.3 ms, hold 10 ms) / Opto (release ×2 program-bağımlı, RMS) — yalnız belgelenmiş çarpanlar, sihir yok.
#include "Module.h"
#include <array>

namespace sweax
{

struct CompParams
{
    bool  on { false };
    float threshDb { -18.0f }, ratio { 2.0f }, kneeDb { 6.0f };
    float attackMs { 10.0f }, releaseMs { 120.0f }, lookaheadMs { 0.0f };
    float makeupDb { 0.0f }, mix { 1.0f };
    int   detector { 2 };            // 0 Peak, 1 RMS, 2 Hybrid
    float scHpfHz { 80.0f };
    int   character { 0 };           // 0 Clean, 1 Smooth, 2 Punch, 3 Opto
};

class Compressor : public ModuleBase
{
public:
    static constexpr double LOOKAHEAD_MAX_MS = 5.0;

    void prepare (double sampleRate, int maxBlock, int numChannels);
    int latencySamples (Quality mode) const noexcept;
    void setQuality (Quality mode) noexcept;                 // RT dışı (mod değişimi): aktif gecikme
    float reportedGainDb() const noexcept { return curMakeupDb; }
    float gainReductionDb() const noexcept { return grDbOut; } // ≤ 0, ölçer

    void setParams (const CompParams& p) noexcept { pending = p; paramsDirty = true; }

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    CompParams pending, cur;
    bool paramsDirty { false };
    int  activeDelay { 0 };          // ses yolu gecikmesi (örnek) = latency (mod)
    std::array<DelayLine, MAX_CH> audioDelay;
    DelayLine gainDelay;             // dB kazanç gecikmesi: D − L
    Biquad scHpf; float scHpfLast { -1.0f };
    // dedektör
    float peakEnv { 0.0f }, rmsEnv { 0.0f }, peakCoef { 0.0f }, rmsCoef { 0.0f };
    // zarf
    float grDb { 0.0f }, grDbOut { 0.0f };
    float atkCoef { 0.0f }, relCoef { 0.0f }, relCoefFast { 0.0f };
    int holdSamples { 0 }, holdLeft { 0 };
    float curMakeupDb { 0.0f };
    DbGainSmoother makeup; LinearSmoother mixS;
};

} // namespace sweax
