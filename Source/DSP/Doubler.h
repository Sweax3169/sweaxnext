#pragma once
// SWEAX NEXT — DSP/Doubler.h
// Vokal kalınlaştırıcı (M2): mikro-perde kaydırmalı 2/4 ses kopyası, L/R yayılım.
//  • Detune: çift-tap testere pitch-shifter (Eventide mikro-pitch yaklaşımı) — sabit oran r = 2^(±cent/1200),
//    pencere ~40 ms, iki tap üçgen crossfade → LFO vibratosu YOK, gerçek sabit detune.
//  • Her ses: sabit yayılım gecikmesi (spread ± ofset) + HP/LP bant sınırlama (vokalin altını/üstünü itmez).
//  • 2 ses: −cent → L, +cent → R. 4 ses: ±cent sert, ±cent/2 %50 pan.
//  • Kuru sinyal DOKUNULMAZ (latency 0); wet = mix · sesler. Islak yol girişi kanal ortalaması (vokal monodur).
//  • idleWet girişle ısınır: açılış anında dolu ses (gecikme ~20-40 ms zaten kısa).
#include "Module.h"

namespace sweax
{

struct DoublerParams
{
    bool  on { false };
    float mix { 0.35f };             // 0..1
    float spreadMs { 18.0f };        // 5..40 temel yayılım
    float detuneCents { 9.0f };      // 0..25
    float hpHz { 180.0f }, lpHz { 9000.0f };
    int   voices { 0 };              // 0 → 2 ses, 1 → 4 ses
};

class Doubler : public ModuleBase
{
public:
    static constexpr int MAX_VOICES = 4;
    static constexpr double WINDOW_MS = 40.0;      // pitch-shifter crossfade penceresi

    void prepare (double sampleRate, int maxBlock, int numChannels);
    int latencySamples (Quality) const noexcept { return 0; }
    void setQuality (Quality) noexcept {}
    void setParams (const DoublerParams& p) noexcept { pending = p; paramsDirty = true; }

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    struct Voice
    {
        double phase { 0.0 };        // 0..1 pencere fazı (testere)
        double rate { 0.0 };         // faz artışı/örnek = (1−r)/pencere
        float baseDelay { 0.0f };    // yayılım (örnek)
        float panL { 1.0f }, panR { 0.0f };
        Biquad hp, lp;
    };
    void updateParams() noexcept;
    void pushRing (float x) noexcept
    {
        ring[(size_t) wp] = x;
        wp = (wp + 1) % ringLen;
    }
    float readRing (double delaySamples) const noexcept
    {
        double rp = (double) wp - 1.0 - delaySamples;
        while (rp < 0.0) rp += (double) ringLen;
        const int i0 = (int) rp;
        const double fr = rp - i0;
        const int i1 = (i0 + 1) % ringLen;
        return (float) ((1.0 - fr) * ring[(size_t) i0] + fr * ring[(size_t) i1]);
    }

    DoublerParams pending, cur;
    bool paramsDirty { true };
    std::vector<float> ring;
    int ringLen { 1 }, wp { 0 };
    int windowN { 1 };
    std::array<Voice, MAX_VOICES> voice;
    int activeVoices { 2 };
    LinearSmoother mixS;
};

} // namespace sweax
