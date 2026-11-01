#pragma once
// SWEAX NEXT — UI/ModulKarti.h
// MixReady zincir kartı (mimari §8): modül adı + durum (AÇIK / GEREKMEDİ / KİLİTLİ) + değer özeti + "Neden?"
// (plan düğümündeki Reason KODLARINDAN Metin.h ile render) + KİLİTLE + (kapalıysa) YİNE DE AÇ.
// Kart hiçbir değeri kendi tutmaz: her refresh() SESSION + APVTS'den okur (§8 kuralı).
#include "../PluginProcessor.h"
#include "../Decision/PlanPublisher.h"
#include "Metin.h"

namespace sweax::ui
{

class ModulKarti : public juce::Component
{
public:
    ModulKarti (SweaxProcessor& p, const char* moduleId) : proc (p), modul (moduleId)
    {
        addAndMakeVisible (kilit);
        addAndMakeVisible (yineDeAc);
        kilit.onClick = [this]
        {
            auto& u = proc.undoManager();
            u.beginNewTransaction ("Kilit");
            proc.session().setLocked (modul, ! proc.session().isLocked (modul), &u);
            refresh();
        };
        yineDeAc.onClick = [this]
        {
            decision::PlanPublisher::forceEnableNeutral (modul.toRawUTF8(), proc.paramCache(), proc.session(), proc.undoManager());
            refresh();
        };
        refresh();
    }

    static bool moduleOn (SweaxProcessor& p, const juce::String& m)
    {
        auto& c = p.paramCache();
        if (m == state::modul::EQ) return c.flag (params::EqOn);
        if (m == state::modul::LEVELER) return c.flag (params::LevOn);
        if (m == state::modul::COMP_A) return c.flag (params::CaOn);
        if (m == state::modul::COMP_B) return c.flag (params::CbOn);
        if (m == state::modul::DEESS) return c.flag (params::DsOn);
        if (m == state::modul::GATE) return c.flag (params::GateOn);
        if (m == state::modul::SAT) return c.flag (params::SatOn);
        if (m == state::modul::DOUBLER) return c.flag (params::DblOn);
        if (m == state::modul::DELAY) return c.flag (params::DlyOn);
        if (m == state::modul::REVERB) return c.flag (params::RevOn);
        if (m == state::modul::WIDTH) return c.flag (params::WidOn);
        if (m == state::modul::TUNE) return c.flag (params::TuneOn);
        if (m == state::modul::AIR) return c.flag (params::AirOn);
        if (m == state::modul::GLOW) return c.flag (params::GlowOn);
        if (m == state::modul::DIRT) return c.flag (params::DirtOn);
        if (m == state::modul::LAYER) return c.flag (params::LayerOn);
        if (m == state::modul::YUMUSAT) return c.flag (params::SooOn);
        if (m == state::modul::FORMANT) return c.flag (params::FrmOn);
        if (m == state::modul::TONEQ) return c.flag (params::TeqOn);
        return true;                                     // OUTPUT hep açık
    }

    juce::String degerOzeti() const
    {
        auto& c = proc.paramCache();
        auto hz = [] (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String ((int) std::lround (f)) + " Hz"; };
        if (modul == state::modul::EQ)
        {
            int bant = 0; for (int b = 0; b < params::EQ_BANT_SAYISI; ++b) if (c.flag (params::eqBant (b, 0))) ++bant;
            juce::String s;
            if (c.flag (params::EqHpfOn)) { s += "HPF "; s += hz (c.raw (params::EqHpfHz)); }
            if (bant > 0) { if (s.isNotEmpty()) s += juce::String (juce::CharPointer_UTF8 ("  ·  ")); s += juce::String (bant) + juce::String (juce::CharPointer_UTF8 (" bant")); }
            return s;
        }
        auto U = [] (const char* t) { return juce::String (juce::CharPointer_UTF8 (t)); };
        if (modul == state::modul::LEVELER)
            return U ("hedef ") + juce::String (c.raw (params::LevTarget), 0) + U (" LUFS  ·  ±") + juce::String (c.raw (params::LevMaxUp), 0) + " dB";
        if (modul == state::modul::COMP_A)
            return juce::String (c.raw (params::CaRatio), 1) + U (":1  ·  eşik ") + juce::String (c.raw (params::CaThresh), 0) + " dB";
        if (modul == state::modul::COMP_B)
            return juce::String (c.raw (params::CbRatio), 1) + ":1";
        if (modul == state::modul::DEESS)
            return hz (c.raw (params::DsHz)) + U ("  ·  ") + juce::String (c.raw (params::DsRange), 1) + " dB";
        if (modul == state::modul::GATE)
            return U ("eşik ") + juce::String (c.raw (params::GateThresh), 0) + U (" dB  ·  ") + juce::String (c.raw (params::GateRange), 0) + U (" dB bastırma");
        if (modul == state::modul::SAT)
            return U ("sürüş ") + juce::String (c.raw (params::SatDrive), 1) + " dB";
        if (modul == state::modul::DOUBLER)
            return U ("karışım %") + juce::String ((int) std::lround (c.raw (params::DblMix) * 100.0f));
        if (modul == state::modul::DELAY)
            return U ("karışım %") + juce::String ((int) std::lround (c.raw (params::DlyMix) * 100.0f));
        if (modul == state::modul::REVERB)
            return U ("karışım %") + juce::String ((int) std::lround (c.raw (params::RevMix) * 100.0f)) + U ("  ·  ") + juce::String (c.raw (params::RevDecay), 1) + " sn";
        if (modul == state::modul::WIDTH)
            return U ("miktar ") + juce::String (c.raw (params::WidAmount), 2);
        if (modul == state::modul::TUNE)
            return U ("güç %") + juce::String ((int) std::lround (c.raw (params::TuneStrength) * 100.0f))
                 + U ("  ·  ") + metin::gamAdi (c.choice (params::TuneKey), c.choice (params::TuneScale));
        if (modul == state::modul::DIRT)
        {
            static const char* modAd[3] = { "scream", "fuzz", "lo-fi" };
            return juce::String (modAd[juce::jlimit (0, 2, c.choice (params::DirtMode))])
                 + U ("  ·  ") + juce::String (c.raw (params::DirtLevel), 1) + " dB";
        }
        if (modul == state::modul::LAYER)
        {
            auto aralik = [] (float v) { const int s = (int) std::lround (v); return s == 0 ? juce::String ("-") : (s > 0 ? "+" + juce::String (s) : juce::String (s)); };
            return aralik (c.raw (params::LayerA)) + " / " + aralik (c.raw (params::LayerB))
                 + U ("  ·  ") + juce::String (c.raw (params::LayerLevel), 1) + " dB";
        }
        if (modul == state::modul::YUMUSAT)
            return U ("derinlik %") + juce::String ((int) std::lround (c.raw (params::SooDepth) * 100.0f))
                 + U ("  ·  ") + hz (c.raw (params::SooLo)) + U ("–") + hz (c.raw (params::SooHi));
        if (modul == state::modul::FORMANT)
        {
            const float k = c.raw (params::FrmShift);
            return (k >= 0.0f ? juce::String ("+") : juce::String()) + juce::String (k, 1) + U (" yarım ton  ·  karışım %")
                 + juce::String ((int) std::lround (c.raw (params::FrmMix) * 100.0f));
        }
        if (modul == state::modul::TONEQ)
        {
            juce::String t;
            for (int b = 0; b < params::TEQ_BANT_SAYISI; ++b)
                if (c.flag (params::teqBant (b, 0)))
                {
                    const float g = c.raw (params::teqBant (b, 3));
                    if (t.isNotEmpty()) t += U ("  ·  ");
                    t += hz (c.raw (params::teqBant (b, 2))) + " " + (g >= 0.0f ? juce::String ("+") : juce::String()) + juce::String (g, 1);
                }
            return t;
        }
        if (modul == state::modul::GLOW)
            return U ("derinlik %") + juce::String ((int) std::lround (c.raw (params::GlowDepth) * 100.0f))
                 + U ("  ·  zaman ") + juce::String (c.raw (params::GlowTime), 1) + "x";
        if (modul == state::modul::AIR)
            return U ("mid %") + juce::String ((int) std::lround (c.raw (params::AirMid) * 100.0f))
                 + U ("  ·  high %") + juce::String ((int) std::lround (c.raw (params::AirHigh) * 100.0f));
        return U ("tavan ") + juce::String (c.raw (params::OutCeiling), 1) + " dBTP";
    }

    void refresh()
    {
        acik = moduleOn (proc, modul);
        kilitli = proc.session().isLocked (modul);
        deger = acik ? degerOzeti() : juce::String();
        neden.clear();
        auto mn = decision::PlanPublisher::moduleNode (proc.session(), modul.toRawUTF8());
        if (mn.isValid())
            for (int i = 0; i < mn.getNumChildren() && neden.size() < 2; ++i)
            {
                auto rn = mn.getChild (i);
                const auto s = metin::nedenMetni ((int) rn.getProperty ("rule", -1),
                                                  (float) (double) rn.getProperty ("v0", 0.0), (float) (double) rn.getProperty ("v1", 0.0),
                                                  (float) (double) rn.getProperty ("v2", 0.0), (float) (double) rn.getProperty ("v3", 0.0));
                if (s.isNotEmpty()) neden.add (s);
            }
        kilit.setButtonText (kilitli ? metin::KILIDI_AC : metin::KILITLE);
        yineDeAc.setVisible (! acik && modul != state::modul::OUTPUT);
        yineDeAc.setButtonText (metin::YINE_DE_AC);
        if (getWidth() > 0) resized();   // denetim (M3.2): görünürlük değişti ama yükseklik aynıysa buton (0,0)'da kalıyordu
        const int yeniH = yukseklik();
        if (yeniH != sonH) { sonH = yeniH; if (yukseklikDegisti) yukseklikDegisti(); }
        repaint();
    }

    static int istenenYukseklik (int nedenSatir) { return 58 + nedenSatir * 15; }
    int yukseklik() const { return istenenYukseklik (neden.size()); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (juce::Colour (0xff1b2029));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour ((acik ? juce::Colour (0xff4fc38a) : juce::Colour (0xff8a93a3)).withAlpha (0.9f));
        g.fillRoundedRectangle (r.removeFromLeft (4.0f), 2.0f);
        auto b = getLocalBounds().reduced (14, 8);
        g.setColour (juce::Colour (0xffe8ebf0));
        g.setFont (juce::Font (15.0f, juce::Font::bold));
        g.drawText ((siraNo > 0 ? juce::String (siraNo) + juce::String (juce::CharPointer_UTF8 ("  ·  ")) : juce::String()) + metin::modulAdi (modul),
                    b.removeFromTop (18), juce::Justification::centredLeft);
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.setColour (acik ? juce::Colour (0xff4fc38a) : juce::Colour (0xff8a93a3));
        juce::String durum = acik ? metin::ACIK : metin::GEREKMEDI;
        if (acik && canliGr < -0.2f) durum = juce::String (canliGr, 1) + " dB   " + durum;
        if (kilitli) durum += juce::String (juce::CharPointer_UTF8 ("  ·  ")) + metin::KILITLI;
        auto ust = getLocalBounds().reduced (14, 8).removeFromTop (18);
        g.drawText (durum, ust.removeFromRight (150), juce::Justification::centredRight);
        g.setFont (juce::Font (12.5f));
        g.setColour (juce::Colour (0xffe8ebf0).withAlpha (0.85f));
        if (deger.isNotEmpty()) g.drawText (deger, b.removeFromTop (16), juce::Justification::centredLeft);
        g.setFont (juce::Font (11.5f));
        g.setColour (juce::Colour (0xff8a93a3));
        for (const auto& n : neden)
            g.drawText (juce::String (juce::CharPointer_UTF8 ("• ")) + n, b.removeFromTop (15), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 6);
        auto alt = r.removeFromBottom (20);
        kilit.setBounds (alt.removeFromRight (86).reduced (2, 0));
        if (yineDeAc.isVisible()) yineDeAc.setBounds (alt.removeFromRight (96).reduced (2, 0));
    }

    juce::String moduleId() const { return modul; }
    // M3.5: kartın preset zincirindeki işlem sırası (1 = ilk; 0 = gösterme)
    void setSiraNo (int n) { if (n != siraNo) { siraNo = n; repaint(); } }
    std::function<void()> yukseklikDegisti;                 // ebeveyn yerleşimi tazeler (denetim)

    // canlı GR (editör timer'ı besler; yalnız anlamlı değişimde repaint)
    void canliGrGuncelle (float db)
    {
        if (std::fabs (db - canliGr) < 0.15f) return;
        canliGr = db;
        repaint();
    }

private:
    SweaxProcessor& proc;
    juce::String modul;
    int siraNo { 0 };
    juce::TextButton kilit, yineDeAc;
    bool acik { false }, kilitli { false };
    juce::String deger;
    juce::StringArray neden;
    float canliGr { 0.0f };
    int sonH { -1 };
};

} // namespace sweax::ui
