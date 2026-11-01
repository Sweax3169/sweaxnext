#pragma once
// SWEAX NEXT — Core/ParamCache.h
// Parametre tablosundan (ParamIds.h) APVTS düzenini üretir ve ses iş parçacığı için ATOMİK
// önbellek kurar. processBlock'ta sıfır String, sıfır hash araması, sıfır tahsis (eski eklentinin
// blok başına ~150 String tahsisi burada yapısal olarak imkânsızdır).
//
// ETKİN DEĞER (§2.2): makro-etiketli parametre için
//   scale     = clamp( makroDegeri / max(aiMakro, 1e-3), 0, 2 )
//   effective = clamp( notr + (deger - notr) * scale, min, max )
// Böylece AI'nin bulmadığı düzeltmeyi (deger == notr) hiçbir makro icat edemez.
#include "ParamIds.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

namespace sweax
{

class ParamCache
{
public:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Kurulumda bir kez (message thread): işaretçileri önbelleğe al. Tüm parametreler bulunmalı.
    void attach (juce::AudioProcessorValueTreeState& apvts);
    bool isAttached() const noexcept { return attached; }

    // ---- SES İŞ PARÇACIĞI (tahsissiz) ----
    inline float raw (params::Id id) const noexcept
    {
        return ptr[(size_t) id]->load (std::memory_order_relaxed);
    }
    inline bool flag (params::Id id) const noexcept { return raw (id) > 0.5f; }
    inline int  choice (params::Id id) const noexcept { return (int) (raw (id) + 0.5f); }
    float effective (params::Id id) const noexcept;   // makro birleşimi uygulanmış ham değer

    // ---- MESSAGE THREAD ----
    juce::RangedAudioParameter* param (params::Id id) const noexcept { return rp[(size_t) id]; }
    void setBaseline (params::Id id, float hamDeger) noexcept { baseline[(size_t) id].store (hamDeger, std::memory_order_relaxed); }
    float getBaseline (params::Id id) const noexcept { return baseline[(size_t) id].load (std::memory_order_relaxed); }
    void setMacroBaseline (params::Makro m, float v) noexcept { macroBase[(size_t) m].store (v, std::memory_order_relaxed); }
    float getMacroBaseline (params::Makro m) const noexcept { return macroBase[(size_t) m].load (std::memory_order_relaxed); }

    // Ham değer → normalize (0..1) ve tersi (message thread; plan uygulaması / UI)
    float toNormalized (params::Id id, float ham) const noexcept;
    float fromNormalized (params::Id id, float n) const noexcept;

    // Bir makronun APVTS parametre kimliği
    static params::Id macroParam (params::Makro m) noexcept;

private:
    std::array<std::atomic<float>*, (size_t) params::SAYI> ptr {};
    std::array<juce::RangedAudioParameter*, (size_t) params::SAYI> rp {};
    std::array<std::atomic<float>, (size_t) params::SAYI> baseline {};
    std::array<std::atomic<float>, 8> macroBase {};     // Makro enum boyu (Yok dahil)
    bool attached { false };
};

} // namespace sweax
