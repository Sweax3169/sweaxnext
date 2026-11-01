#pragma once
// SWEAX NEXT — DSP/CorrectiveEQ.h
// Düzeltici EQ (mimari §4.1): HPF (12/24 dB Butterworth) + 6 bant {bell, low shelf, high shelf, notch}.
//  • Statik bantlar: bell = Orfanidis Nyquist-eşleşmeli (peq.m ile doğrulandı), shelf/notch RBJ (bilinear+prewarp).
//    Parametreler tek-kutup (30 ms) yumuşatılır; katsayılar yumuşatma sürerken 64 örneklik alt bloklarda yeniden
//    hesaplanır (blok boyundan bağımsız pürüzsüzlük). Eşik üstü sıçramada (|Δgain| > 6 dB, |Δlog2 f| > 1/3 okt,
//    Q oranı > 2) çift-filtre ısıtma + crossfade (CrossfadingFilter).
//  • BANT/HPF AÇ-KAPA TIKSIZ (inceleme düzeltmesi): kapama = kimlik katsayılarına crossfade (bant, geçiş bitene dek
//    işlemeye devam eder); açma = bell/shelf kazanç 0'dan rampa, notch/HPF kimlikten hedefe crossfade.
//  • Dinamik bantlar: TPT-SVF bell + sidechain aynı bant RMS zarfı (dB), soft knee 3 dB, range sınırlı, attack/release
//    %63; statik kazanç payı 30 ms yumuşatılır, fc/Q blok başına tek-kutup yumuşatılarak uygulanır; zarf sanitize edilir.
//  • PATLAMA KORUMASI (M3.4, sahip: "harfler patlamasın"): HPF'ten ÖNCE çalışır. Tetik: 0.5·fc altı LF zarfı (1 ms)
//    300 Hz–3 kHz gövde zarfını (25 ms) +6 dB aşınca; müdahale: fc köşeli 2. derece
//    dinamik low-shelf (Simper SVF, kazanç örnek başına — zipper yok), atak 1 ms / bırakma 50 ms. Denetim: eski
//    dinamik bell HPF köşesinin ALTINDA kalıp patlamanın 80-200 Hz gövdesine hiç dokunmuyordu (19 render ölçüldü).
//    ÖNDEN-BAKIŞ 5 ms (YÜKSEK/ULTRA): patlama başındaki tık orta bandı yan zincirin LF alçak geçireninden önce
//    yükseltiyor, koruma ~10 ms geç kalıp enerjinin çoğunu kaçırıyordu (simülasyon: 0 ms −1.2 dB, 5 ms −9.1 dB).
//  • Latency: CANLI 0, YÜKSEK/ULTRA 5 ms (sabit; kapalıyken kuru yol ve idleWet aynı gecikmeyi taşır).
//  • HPF asla "reçete" değildir: yalnız plan açar (Room/Proximity/LF gürültü/Plosive kanıtıyla, §4.1).
#include "Module.h"
#include "Svf.h"
#include "../Core/Biquad.h"
#include "../Core/ParamIds.h"
#include <array>

namespace sweax
{

struct EqBandParams
{
    bool  on { false };
    int   type { 0 };                 // 0 Bell, 1 Low Shelf, 2 High Shelf, 3 Notch
    float hz { 1000.0f }, gainDb { 0.0f }, q { 1.0f };
    bool  dyn { false };
    float dynThreshDb { -24.0f }, dynRangeDb { 0.0f }, dynAttackMs { 5.0f }, dynReleaseMs { 120.0f };
};

struct EqParams
{
    bool on { false };
    bool hpfOn { false };
    float hpfHz { 60.0f };
    int hpfSlope { 0 };               // 0: 12 dB, 1: 24 dB
    std::array<EqBandParams, 6> band;
    bool  plosOn { false };
    float plosHz { 150.0f };          // shelf köşesi (≈1.3·F0_p05)
    float plosRangeDb { -6.0f };      // en derin bastırma (≤ 0)
};

class CorrectiveEQ : public ModuleBase
{
public:
    static constexpr int BANDS = 6;

    static constexpr double PLOS_LOOKAHEAD_MS = 5.0;

    // onBakis = false: patlama koruması önden bakışı YOK → gecikme her modda 0 (M3.5 TON EQ örneği; koruma kullanılmaz)
    void prepare (double sampleRate, int maxBlock, int numChannels, bool onBakis = true);
    int latencySamples (Quality mode) const noexcept;
    void setQuality (Quality mode) noexcept;
    float reportedGainDb() const noexcept { return 0.0f; }

    // RT: parametreler blok başında verilir (ParamCache'ten okunmuş POD)
    void setParams (const EqParams& p) noexcept { pending = p; paramsDirty = true; }
    // ölçüm/UI: bant başına anlık dinamik kazanç azaltımı (dB, ≤ 0) — yalnız gösterge (yırtılmaya toleranslı)
    float bandGainReductionDb (int band) const noexcept { return bandState[(size_t) band].dynGrDb; }
    float plosiveGainReductionDb() const noexcept { return plos.grDb; }

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    struct BandState
    {
        // statik
        CrossfadingFilter<1> filt;
        OnePoleSmoother hzS, gainS, qS;
        float lastHz { 0.0f }, lastGain { 0.0f }, lastQ { 0.0f }; int lastType { -1 }; bool wasOn { false };
        bool active { false };         // işleme gerekli (açık veya kapanış geçişi sürüyor)
        // dinamik
        std::array<Svf, MAX_CH> svf;
        std::array<Svf, MAX_CH> scSvf;
        double env { 0.0 };            // sidechain RMS zarfı (lineer kare, tek kutup)
        float grDb { 0.0f };           // yumuşatılmış GR (dB, ≤ 0)
        float dynGrDb { 0.0f };
        float atkCoef { 0.0f }, relCoef { 0.0f }, envCoef { 0.0f };
        float svfHz { 0.0f }, svfQ { 0.0f };   // uygulanan (yumuşatılmış) fc/Q
        bool  wasDyn { false };
        int   dynFade { 0 };                    // dinamik bant kapanış sönümü (örnek): GR + statik pay 0'a iner
    };
    void updateStatic (int b, const EqBandParams& p) noexcept;
    void updateHpf (const EqParams& p) noexcept;
    static BiquadCoeffs designBand (double sr, int type, double hz, double gainDb, double q) noexcept;
    static BiquadCoeffs identity() noexcept { BiquadCoeffs c; c.b0 = 1.0; c.b1 = c.b2 = c.a1 = c.a2 = 0.0; return c; }
    double hpfStageQ (int stages, int stage) const noexcept { return stages == 1 ? 0.70710678 : (stage == 0 ? 0.54119610 : 1.30656296); }

    struct PlosiveGuard
    {
        Svf scLp, scHp, scLp2;
        double eLf { 0.0 }, eMid { 0.0 };
        float grDb { 0.0f };
        std::array<double, MAX_CH> ic1 {}, ic2 {};
        double g0 { 0.1 };
        float lastHz { -1.0f };
        float cLf { 0.0f }, cMid { 0.0f }, cAtk { 0.0f }, cRel { 0.0f };
    };
    void plosiveGuard (float* const* ch, int numCh, int n) noexcept;
    PlosiveGuard plos;
    int laMax { 0 }, laAktif { 0 };
    std::array<DelayLine, MAX_CH> plosGecikme;

    EqParams pending, cur;
    bool paramsDirty { false };
    std::array<BandState, BANDS> bandState;
    CrossfadingFilter<2> hpf;
    OnePoleSmoother hpfHzS; float lastHpfHz { 0.0f }; int lastHpfSlope { -1 }; bool hpfWasOn { false }; bool hpfActive { false };
};

} // namespace sweax
