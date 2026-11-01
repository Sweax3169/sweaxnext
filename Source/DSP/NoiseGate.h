#pragma once
// SWEAX NEXT — DSP/NoiseGate.h
// Gürültü kapısı / aşağı-genişletici (M2 §4): cümle aralarındaki oda sesi / kulaklık taşması / mekanik gürültüyü
// vokalin ALTINA iter. Tam kapatma yerine sınırlı RANGE (varsayılan 15 dB) → doğal nefes bırakır, pompalamaz.
//  • Dedektör: kanal-maks doğrultma + hızlı yükselme (~0.3 ms) / orta düşme (~8 ms) tek kutup; stereo link.
//  • Histerezis: açılma eşiği = thresh, kapanma eşiği = thresh − hyst (varsayılan 4 dB) → cıvıltı yok.
//  • Durum makinesi: Open ↔ (hold) ↔ Release; kazanç LINEER alanda tek kutup (attack açar, release kapatır).
//  • Lookahead: CANLI 0, YÜKSEK/ULTRA 2 ms — ses gecikir, dedektör ÖNDEN okur → ünsüz başlangıçları kırpılmaz.
//    Latency = f(mod) SABİT; parametre/enable değiştiremez (ModuleBase sözleşmesi).
//  • Eşik plan tarafından ölçülen taban gürültüsünden türetilir (Planner: floor+10 dB) — sabit tarif değil.
#include "Module.h"

namespace sweax
{

struct GateParams
{
    bool  on { false };
    float threshDb { -55.0f };
    float rangeDb { 15.0f };          // kapalı durum zayıflatması (0..40)
    float attackMs { 1.5f };          // açılma
    float releaseMs { 120.0f };       // kapanma
    float holdMs { 80.0f };           // eşiğin altına düşünce bekleme
    float hystDb { 4.0f };            // kapanma eşiği = thresh − hyst
};

class NoiseGate : public ModuleBase
{
public:
    static constexpr double LOOKAHEAD_MS = 2.0;

    void prepare (double sampleRate, int maxBlock, int numChannels);
    int latencySamples (Quality mode) const noexcept;
    void setQuality (Quality mode) noexcept;
    void setParams (const GateParams& p) noexcept { pending = p; paramsDirty = true; }
    float gainReductionDb() const noexcept { return grDbOut; }   // ≤ 0 (ölçer)

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    void updateParams() noexcept;

    GateParams pending, cur;
    bool paramsDirty { true };
    int activeDelay { 0 };                       // ses yolu gecikmesi = latency(mod)
    std::array<DelayLine, MAX_CH> audioDelay;

    // dedektör (lineer zarf)
    float env { 0.0f };
    float envRise { 0.0f }, envFall { 0.0f };    // tek kutup katsayıları
    // eşikler / kazanç hedefleri (lineer, param değişince güncellenir)
    float openLin { 0.0f }, closeLin { 0.0f }, gClosed { 0.0f };
    float atkCoef { 0.0f }, relCoef { 0.0f };
    int   holdN { 0 };
    // durum
    float g { 1.0f };                            // anlık kazanç (lineer)
    bool  open { true };
    int   holdLeft { 0 };
    float grDbOut { 0.0f };
};

} // namespace sweax
