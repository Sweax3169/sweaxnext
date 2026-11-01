#pragma once
// SWEAX NEXT — UI/SeviyeMetre.h
// Dikey tepe (peak) metresi: processor'dan callback ile dB okur (ses yazar, UI okur).
// Yeşil → altın → kırmızı gradyan segmentleri, tepe-tutma çizgisi, altta etiket.
// Kendi 30 Hz timer'ı; editör kapanınca durur (bileşen yıkımı).
#include "SweaxLnF.h"
#include <functional>

namespace sweax::ui
{

class SeviyeMetre : public juce::Component, private juce::Timer
{
public:
    explicit SeviyeMetre (std::function<float()> dbOku, juce::String etiketMetni)
        : oku (std::move (dbOku)), etiket (std::move (etiketMetni))
    {
        setOpaque (false);
        startTimerHz (30);
    }
    ~SeviyeMetre() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        using namespace palet;
        auto r = getLocalBounds().toFloat();
        auto yaziAlani = r.removeFromBottom (14.0f);
        r = r.reduced (2.0f, 0.0f);

        g.setColour (juce::Colour (0xff0e1117));
        g.fillRoundedRectangle (r, 4.0f);

        const float db = juce::jlimit (ALT_DB, 0.0f, sonDb);
        const float oran = (db - ALT_DB) / (0.0f - ALT_DB);
        if (oran > 0.001f)
        {
            auto dolu = r.withTop (r.getBottom() - r.getHeight() * oran).reduced (1.5f, 0.0f);
            juce::ColourGradient grad (YESIL, r.getX(), r.getBottom(),
                                       KIRMIZI, r.getX(), r.getY(), false);
            grad.addColour (0.72, YESIL);
            grad.addColour (0.86, ALTIN);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (dolu, 3.0f);
        }
        // tepe-tutma çizgisi
        if (tepeDb > ALT_DB)
        {
            const float tOran = (juce::jlimit (ALT_DB, 0.0f, tepeDb) - ALT_DB) / (0.0f - ALT_DB);
            const float y = r.getBottom() - r.getHeight() * tOran;
            g.setColour (tepeDb > -0.3f ? KIRMIZI : METIN.withAlpha (0.8f));
            g.fillRect (r.getX() + 1.0f, y - 1.0f, r.getWidth() - 2.0f, 2.0f);
        }
        // −6 / −18 kılavuzları
        g.setColour (CIZGI.withAlpha (0.6f));
        for (float k : { -6.0f, -18.0f })
        {
            const float y = r.getBottom() - r.getHeight() * ((k - ALT_DB) / (0.0f - ALT_DB));
            g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        }
        g.setColour (CIZGI);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        g.setColour (SOLGUN);
        g.setFont (juce::Font (9.5f, juce::Font::bold));
        g.drawText (etiket, yaziAlani.toNearestInt(), juce::Justification::centred);
    }

private:
    static constexpr float ALT_DB = -60.0f;

    void timerCallback() override
    {
        const float v = oku ? oku() : -100.0f;
        sonDb = std::isfinite (v) ? v : -100.0f;                    // sigorta: NaN metreyi kilitlemesin
        if (sonDb >= tepeDb) { tepeDb = sonDb; tepeYas = 0; }
        else if (++tepeYas > 45) tepeDb += (sonDb - tepeDb) * 0.08f; // ~1.5 sn tut, sonra süzül
        repaint();
    }

    std::function<float()> oku;
    juce::String etiket;
    float sonDb { -100.0f }, tepeDb { -100.0f };
    int tepeYas { 0 };
};

} // namespace sweax::ui
