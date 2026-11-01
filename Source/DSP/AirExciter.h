#pragma once
// SWEAX NEXT — DSP/AirExciter.h
// "Hava" modülü (M3): Slate Fresh Air'in belgelenmiş mimarisinden (Dolby-A encode mirası) esinlenme —
// iki ÜST ÜSTE binen yüksek-geçiren bant (MID ≈ 3 kHz+, HIGH ≈ 9 kHz+), her bant PROGRAM-BAĞIMLI
// yukarı-kaldırma ile kuru sinyale PARALEL eklenir:
//  • bant zarfı tam-bant zarfına oranlanır (lineer oran, log yok); bant ZAYIFSA kaldırma tam,
//    bant zaten güçlüyse kaldırma sıfıra iner (statik shelf DEĞİL — parlak kayıtta ess azdırmaz);
//  • kaldırma bloğu başına hesaplanır, 15 ms yumuşatıcıyla sürülür (zipper yok);
//  • ekleme additive'dir: out = x + g·bant (bant kuru içinde zaten var; g = 10^(lift/20) − 1);
//  • IIR (Butterworth 2. derece HP ×2) — latency 0, kalite modundan bağımsız;
//  • idleWet: filtreler + zarflar kapalıyken de beslenir (yeniden açılış bayat/patlamalı olmaz).
#include "Module.h"

namespace sweax
{

struct AirParams
{
    bool  on { false };
    float mid { 0.25f };            // 0..1 (≈ +0..6 dB program-bağımlı)
    float high { 0.20f };           // 0..1 (≈ +0..9 dB program-bağımlı)
};

class AirExciter : public ModuleBase
{
public:
    static constexpr float MID_HZ = 3000.0f, HIGH_HZ = 9000.0f;
    static constexpr float MID_MAX_DB = 6.0f, HIGH_MAX_DB = 9.0f;

    void prepare (double sampleRate, int maxBlock, int numChannels);
    int latencySamples (Quality) const noexcept { return 0; }
    void setQuality (Quality) noexcept {}
    void setParams (const AirParams& p) noexcept { pending = p; }

    // test/inceleme erişimi: son hesaplanan kaldırmalar (dB)
    float lastMidLiftDb() const noexcept { return midLift.getCurrent() > 0.0f ? 20.0f * std::log10 (1.0f + midLift.getCurrent()) : 0.0f; }
    float lastHighLiftDb() const noexcept { return highLift.getCurrent() > 0.0f ? 20.0f * std::log10 (1.0f + highLift.getCurrent()) : 0.0f; }

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    void besle (const float* const* ch, int numCh, int n, float* const* bandMidOut, float* const* bandHighOut) noexcept;
    void kaldirmayiGuncelle (int numCh) noexcept;

    AirParams pending, cur;
    std::array<Biquad, MAX_CH> hpMid, hpHigh;                  // Butterworth 2. derece HP (bant başına tek biquad)
    std::array<float, MAX_CH> envFull {}, envMid {}, envHigh {};
    float atkCoef { 0.0f }, relCoef { 0.0f };
    LinearSmoother midLift, highLift;                          // lineer ek kazanç (g = 10^(lift/20) − 1)
    std::vector<float> bandBuf;                                // MAX_CH × maxBlock × 2 (mid, high)
};

} // namespace sweax
