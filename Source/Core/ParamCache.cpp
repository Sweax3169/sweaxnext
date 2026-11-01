// SWEAX NEXT — Core/ParamCache.cpp
#include "ParamCache.h"
#include <algorithm>

namespace sweax
{
using namespace params;

static juce::String s (std::string_view v) { return juce::String (v.data(), v.size()); }

juce::AudioProcessorValueTreeState::ParameterLayout ParamCache::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout L;
    for (const auto& t : TABLO)
    {
        const juce::ParameterID pid { s (t.id), PARAM_SURUM };
        switch (t.tip)
        {
            case Tip::Float:
            {
                const float adim = (t.birim == "st" || t.id == "swx.dirt.rate" || t.id.substr (0, 8) == "swx.pos.") ? 1.0f : 0.0f;   // DSP tamsayıya yuvarlıyor / sıra konumu
                juce::NormalisableRange<float> r (t.min, t.max, adim, t.skew);
                // insanca değer metni (host otomasyon şeridi + UI knob kutusu): "329 Hz", "-5.1 dB", "%65"…
                const juce::String birim = s (t.birim);
                const float mx = t.max;
                auto attr = juce::AudioParameterFloatAttributes()
                    .withLabel (birim)
                    .withAutomatable (t.otomasyon)
                    .withStringFromValueFunction ([birim, mx] (float v, int)
                    {
                        if (birim == "Hz")
                            return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                                 : v < 100.0f   ? juce::String (v, 1) + " Hz"
                                                : juce::String (juce::roundToInt (v)) + " Hz";   // String(v,0) tam hassasiyet basar
                        if (birim == "st")
                        {
                            const int st = juce::roundToInt (v);
                            return (st > 0 ? juce::String ("+") : juce::String()) + juce::String (st) + " st";
                        }
                        if (birim == "dB")  return juce::String (v, 1) + " dB";
                        if (birim == "ms")  return v < 10.0f ? juce::String (v, 1) + " ms"
                                                             : juce::String (juce::roundToInt (v)) + " ms";
                        if (birim == ":1")  return juce::String (v, 1) + ":1";
                        if (birim.isEmpty())
                        {
                            if (mx <= 1.01f) return "%" + juce::String ((int) std::lround (v * 100.0f));
                            return juce::String (v, 2);                       // Q, ses sayısı vb.
                        }
                        return juce::String (v, 1) + " " + birim;             // LUFS vb.
                    })
                    .withValueFromStringFunction ([birim, mx] (const juce::String& txt)
                    {
                        auto tmp = txt.trim();
                        const bool yuzde = tmp.containsChar ('%');
                        tmp = tmp.removeCharacters ("%");
                        float v = tmp.getFloatValue();
                        if (tmp.containsIgnoreCase ("kHz")) v *= 1000.0f;
                        if (yuzde || (birim.isEmpty() && mx <= 1.01f && v > 1.5f)) v /= 100.0f;
                        return v;
                    });
                L.add (std::make_unique<juce::AudioParameterFloat> (pid, s (t.ad), r, t.def, attr));
                break;
            }
            case Tip::Bool:
            {
                auto attr = juce::AudioParameterBoolAttributes().withAutomatable (t.otomasyon);
                L.add (std::make_unique<juce::AudioParameterBool> (pid, s (t.ad), t.def > 0.5f, attr));
                break;
            }
            case Tip::Choice:
            {
                juce::StringArray secenekler;
                secenekler.addTokens (s (t.secenekler), "|", "");
                auto attr = juce::AudioParameterChoiceAttributes().withAutomatable (t.otomasyon);
                L.add (std::make_unique<juce::AudioParameterChoice> (pid, s (t.ad), secenekler, (int) t.def, attr));
                break;
            }
        }
    }
    return L;
}

void ParamCache::attach (juce::AudioProcessorValueTreeState& apvts)
{
    for (size_t i = 0; i < TABLO.size(); ++i)
    {
        const auto id = s (TABLO[i].id);
        ptr[i] = apvts.getRawParameterValue (id);
        rp[i]  = apvts.getParameter (id);
        jassert (ptr[i] != nullptr && rp[i] != nullptr); // tabloyla düzen ayrıştıysa burada patlar
        baseline[i].store (TABLO[i].def, std::memory_order_relaxed);
    }
    for (auto& m : macroBase) m.store (0.5f, std::memory_order_relaxed);
    attached = true;
}

params::Id ParamCache::macroParam (params::Makro m) noexcept
{
    switch (m)
    {
        case Makro::Temizlik:  return MacroTemizlik;
        case Makro::Denge:     return MacroDenge;
        case Makro::Onde:      return MacroOnde;
        case Makro::Parlaklik: return MacroParlaklik;
        case Makro::Alan:      return MacroAlan;
        case Makro::Genislik:  return MacroGenislik;
        case Makro::Cikis:     return MacroCikis;
        default:               return Bypass; // Yok → anlamsız; çağıran kontrol eder
    }
}

float ParamCache::effective (params::Id id) const noexcept
{
    const auto& t = TABLO[(size_t) id];
    const float v = raw (id);
    if (t.makro == Makro::Yok) return v;
    const float makro = raw (macroParam (t.makro));
    const float ai = std::max (macroBase[(size_t) t.makro].load (std::memory_order_relaxed), 1.0e-3f);
    const float scale = std::clamp (makro / ai, 0.0f, 2.0f);
    return std::clamp (t.notr + (v - t.notr) * scale, t.min, t.max);
}

float ParamCache::toNormalized (params::Id id, float ham) const noexcept
{
    auto* p = rp[(size_t) id];
    return p ? p->getNormalisableRange().convertTo0to1 (std::clamp (ham, TABLO[(size_t) id].min, TABLO[(size_t) id].max)) : 0.0f;
}

float ParamCache::fromNormalized (params::Id id, float n) const noexcept
{
    auto* p = rp[(size_t) id];
    return p ? p->getNormalisableRange().convertFrom0to1 (std::clamp (n, 0.0f, 1.0f)) : TABLO[(size_t) id].def;
}

} // namespace sweax
