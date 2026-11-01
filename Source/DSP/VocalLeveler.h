#pragma once
// SWEAX NEXT — DSP/VocalLeveler.h
// Cümle seviyeleyici (mimari §4.1): cümle zarfı = K-ağırlıklı momentary (400 ms) + 1.5 s tepe-tutma;
//  • kazanç = hedef − cümle loudness'ı, [−maxDown, +maxUp] içinde; maxUp ≤ SNR − 20 dB (gürültüyü yükseltme);
//  • VAD-lite HOLD: momentary < gürültü tabanı + 10 dB → kazanç donar; > 1 s sessizlikte 3 dB/s ile birime;
//  • eğim ≤ 20 dB/s yukarı, ≤ 60 dB/s aşağı (speed 0.5 nominal; 0 → ×0.25, 1 → ×2);
//  • lookahead: LATENCY = f(mod): CANLI 0, YÜKSEK/ULTRA 150 ms (ses gecikir, kazanç gecikmesiz sinyalden hesaplanır →
//    cümle başlangıcı öngörülür); kazanç 20 ms alt bloklarda güncellenir, örnek başına doğrusal ara değer.
//  • Gürültü tabanı: momentary'nin 4 s minimum-istatistiği (+2 dB). K-filtre Analysis/Loudness.h'den (RT güvenli).
#include "Module.h"
#include "../Analysis/Loudness.h"
#include <array>
#include <vector>

namespace sweax
{

struct LevelerParams
{
    bool  on { false };
    float targetLufs { -18.0f }, maxUpDb { 6.0f }, maxDownDb { 6.0f }, speed { 0.5f };
};

class VocalLeveler : public ModuleBase
{
public:
    static constexpr double LOOKAHEAD_MS = 150.0;
    static constexpr double SUB_MS = 20.0;       // kazanç güncelleme aralığı
    static constexpr int MOM_SUBS = 20;          // 400 ms
    static constexpr int HOLD_SUBS = 75;         // 1.5 s tepe-tutma
    static constexpr int MIN_SUBS = 200;         // 4 s gürültü tabanı

    void prepare (double sampleRate, int maxBlock, int numChannels);
    int latencySamples (Quality mode) const noexcept;
    void setQuality (Quality mode) noexcept;
    float reportedGainDb() const noexcept { return curGainDb; }
    float momentaryLufs() const noexcept { return momentary; }
    float phraseLufs() const noexcept { return phrase; }
    float noiseFloorLufs() const noexcept { return floorLufs; }
    bool  holding() const noexcept { return hold; }

    void setParams (const LevelerParams& p) noexcept { pending = p; paramsDirty = true; }

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    void subBlockUpdate() noexcept;              // her 20 ms: momentary, cümle zarfı, taban, hedef kazanç
    LevelerParams pending, cur;
    bool paramsDirty { false };
    int activeDelay { 0 };
    std::array<DelayLine, MAX_CH> audioDelay;
    KWeighting kw; std::vector<float> kbuf;
    int subLen { 960 }, subFill { 0 }; double subAcc { 0.0 };
    std::array<double, MOM_SUBS> momRing {}; int momPos { 0 }; int momCount { 0 };
    std::array<float, HOLD_SUBS> holdRing {}; int holdPos { 0 };
    std::array<float, MIN_SUBS> minRing {}; int minPos { 0 }; int minCount { 0 };
    float momentary { -100.0f }, phrase { -100.0f }, floorLufs { -100.0f };
    bool haveFloor { false };
    bool hold { false }; int silentSubs { 0 }, inactiveSubs { 100 };
    float targetGainDb { 0.0f }, curGainDb { 0.0f }, gainStepDb { 0.0f };  // örnek başına ara değer
    std::array<float, 16> upRing {}; int upPos { 0 }, upDelaySubs { 0 };  // yükseliş gecikmesi (alt blok sayısı = D/20 ms)
    float rampFromDb { 0.0f }, rampToDb { 0.0f }; int rampPos { 0 };
};

} // namespace sweax
