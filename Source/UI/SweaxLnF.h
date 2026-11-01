#pragma once
// SWEAX NEXT — UI/SweaxLnF.h
// Görsel dil v2 (UI yeniden inşası): eski SWEAX MIX&MASTERING'in ölçülen reçetesi temel alındı —
// koyu gradyan zemin + altın/turkuaz aksan, KALIN ark'lı döner knob (değer halkası altın→turkuaz
// gradyanı, iç disk dikey gradyan, yuvarlak uçlu ibre), "primary" altın gradyan buton (glow'lu),
// "tab" alt-çizgili sekme butonu, hap (pill) toggle. Tüm renkler tek yerde (PALET).
#include <juce_gui_basics/juce_gui_basics.h>

namespace sweax::ui
{

namespace palet
{
    inline const juce::Colour ZEMIN   { 0xff0b0e14 };   // eski reçete: siyaha yakın, hafif mavi ton
    inline const juce::Colour ZEMIN2  { 0xff141821 };   // zemin gradyanının üst ucu
    inline const juce::Colour PANEL   { 0xff161b24 };
    inline const juce::Colour PANEL2  { 0xff20242e };
    inline const juce::Colour CIZGI   { 0xff2a3140 };
    inline const juce::Colour METIN   { 0xfff0ebe1 };   // sıcak kırık beyaz (eski reçete)
    inline const juce::Colour SOLGUN  { 0xff8a93a3 };
    inline const juce::Colour ALTIN   { 0xffe2b558 };
    inline const juce::Colour TURKUAZ { 0xff4ab9c4 };
    inline const juce::Colour YESIL   { 0xff4fc38a };
    inline const juce::Colour KIRMIZI { 0xffe0574a };
    inline const juce::Colour KOYU_YAZI { 0xff0c0d11 }; // altın zemin üstündeki yazı
}

class SweaxLnF : public juce::LookAndFeel_V4
{
public:
    SweaxLnF()
    {
        using namespace palet;
        setColour (juce::ResizableWindow::backgroundColourId, ZEMIN);
        setColour (juce::Label::textColourId, METIN);
        setColour (juce::TextButton::buttonColourId, PANEL);
        setColour (juce::TextButton::buttonOnColourId, ALTIN.withAlpha (0.22f));
        setColour (juce::TextButton::textColourOffId, METIN);
        setColour (juce::TextButton::textColourOnId, ALTIN);
        setColour (juce::ComboBox::backgroundColourId, PANEL);
        setColour (juce::ComboBox::textColourId, METIN);
        setColour (juce::ComboBox::outlineColourId, CIZGI);
        setColour (juce::ComboBox::arrowColourId, ALTIN);
        setColour (juce::PopupMenu::backgroundColourId, PANEL);
        setColour (juce::PopupMenu::textColourId, METIN);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, ALTIN.withAlpha (0.2f));
        setColour (juce::PopupMenu::highlightedTextColourId, ALTIN);
        setColour (juce::Slider::backgroundColourId, PANEL2);
        setColour (juce::Slider::trackColourId, ALTIN.withAlpha (0.7f));
        setColour (juce::Slider::thumbColourId, ALTIN);
        setColour (juce::Slider::textBoxTextColourId, SOLGUN.brighter (0.3f));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ToggleButton::textColourId, METIN);
        setColour (juce::ToggleButton::tickColourId, ALTIN);
        setColour (juce::ToggleButton::tickDisabledColourId, palet::CIZGI);
        setColour (juce::ScrollBar::thumbColourId, palet::CIZGI);
        setColour (juce::TooltipWindow::backgroundColourId, PANEL2);
        setColour (juce::TooltipWindow::textColourId, METIN);
        setColour (juce::TooltipWindow::outlineColourId, ALTIN.withAlpha (0.35f));
    }

    // ---- KNOB: eski reçetenin zenginleştirilmiş hali ----
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace palet;
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float yariCap = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
        const auto merkez = r.getCentre();
        const float aci = startAngle + pos * (endAngle - startAngle);
        const float arkKalinlik = juce::jmax (3.0f, yariCap * 0.14f);
        const float arkYariCap = yariCap - arkKalinlik * 0.5f;

        // pist
        juce::Path pist;
        pist.addCentredArc (merkez.x, merkez.y, arkYariCap, arkYariCap, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff1d212b));
        g.strokePath (pist, juce::PathStrokeType (arkKalinlik, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // değer arkı: altın → turkuaz (değer yükseldikçe)
        juce::Path deger;
        deger.addCentredArc (merkez.x, merkez.y, arkYariCap, arkYariCap, 0.0f, startAngle, aci, true);
        const auto renk = ALTIN.interpolatedWith (TURKUAZ, juce::jlimit (0.0f, 1.0f, pos * 0.6f));
        g.setColour (s.isEnabled() ? renk : juce::Colour (0xff3a414c));
        g.strokePath (deger, juce::PathStrokeType (arkKalinlik, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // iç disk (dikey gradyan) + halka + ibre
        const float gCap = arkYariCap - arkKalinlik * 1.15f;
        if (gCap > 4.0f)
        {
            auto disk = juce::Rectangle<float> (merkez.x - gCap, merkez.y - gCap, gCap * 2.0f, gCap * 2.0f);
            juce::ColourGradient ig (juce::Colour (0xff1d2028), disk.getCentreX(), disk.getY(),
                                     juce::Colour (0xff0e1015), disk.getCentreX(), disk.getBottom(), false);
            g.setGradientFill (ig);
            g.fillEllipse (disk);
            g.setColour (juce::Colour (0xff2a2e38));
            g.drawEllipse (disk.reduced (0.5f), 1.0f);

            juce::Path p;
            const float ibreBoy = gCap * 0.82f;
            const float ibreKalin = juce::jmax (2.0f, arkKalinlik * 0.55f);
            p.addRoundedRectangle (-ibreKalin * 0.5f, -gCap * 0.92f, ibreKalin, ibreBoy, ibreKalin * 0.5f);
            p.applyTransform (juce::AffineTransform::rotation (aci).translated (merkez.x, merkez.y));
            g.setColour (s.isEnabled() ? METIN : SOLGUN);
            g.fillPath (p);
        }
        // parlak uç noktası (değer arkının ucunda)
        if (s.isEnabled())
        {
            const float ux = merkez.x + arkYariCap * std::sin (aci);
            const float uy = merkez.y - arkYariCap * std::cos (aci);
            g.setColour (renk.brighter (0.4f));
            g.fillEllipse (ux - 2.2f, uy - 2.2f, 4.4f, 4.4f);
        }
    }

    // ---- Butonlar: "primary" (altın gradyan), "tab" (alt çizgi), normal (panel) ----
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                               bool over, bool down) override
    {
        using namespace palet;
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const float kose = juce::jmin (10.0f, r.getHeight() * 0.3f);
        const bool primary = b.getProperties()["primary"];
        const bool tab     = b.getProperties()["tab"];
        const bool aktif   = b.getToggleState();

        if (primary)
        {
            juce::ColourGradient grad (ALTIN, r.getX(), r.getY(), ALTIN.darker (0.18f), r.getX(), r.getBottom(), false);
            if (down) grad.multiplyOpacity (0.85f);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, kose);
            g.setColour (ALTIN.withAlpha (over ? 0.4f : 0.22f));
            g.drawRoundedRectangle (r.expanded (1.5f), kose + 1.5f, 1.4f);       // glow
        }
        else if (tab)
        {
            if (aktif)
            {
                g.setColour (ALTIN.withAlpha (0.14f));
                g.fillRoundedRectangle (r, kose);
                g.setColour (ALTIN);
                g.fillRoundedRectangle (r.getX() + 6.0f, r.getBottom() - 2.5f, r.getWidth() - 12.0f, 2.5f, 1.2f);
            }
            else if (over)
            {
                g.setColour (PANEL2);
                g.fillRoundedRectangle (r, kose);
            }
        }
        else
        {
            auto c = bg;
            if (aktif) c = ALTIN.withAlpha (0.2f);
            if (down) c = c.darker (0.2f);
            else if (over) c = c.brighter (0.12f);
            g.setColour (c);
            g.fillRoundedRectangle (r, kose);
            g.setColour (aktif ? ALTIN.withAlpha (0.9f) : CIZGI);
            g.drawRoundedRectangle (r, kose, aktif ? 1.4f : 1.0f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool) override
    {
        using namespace palet;
        const bool primary = b.getProperties()["primary"];
        const bool tab     = b.getProperties()["tab"];
        juce::Colour renk;
        if (primary)  renk = KOYU_YAZI;
        else if (tab) renk = b.getToggleState() ? ALTIN : METIN.withAlpha (0.72f);
        else          renk = b.getToggleState() ? ALTIN : (over ? METIN.brighter (0.2f) : METIN);
        if (! b.isEnabled()) renk = SOLGUN.withAlpha (0.6f);
        g.setColour (renk);
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 2), juce::Justification::centred, 1);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& c) override
    {
        using namespace palet;
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        juce::Path ok;
        const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
        ok.addTriangle (ax - 4.0f, ay - 2.5f, ax + 4.0f, ay - 2.5f, ax, ay + 3.5f);
        g.setColour (c.isEnabled() ? ALTIN : SOLGUN);
        g.fillPath (ok);
    }

    // ---- Pill toggle (eski reçete) ----
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        using namespace palet;
        auto r = b.getLocalBounds().toFloat().reduced (2.0f);
        const float pillH = juce::jmin (18.0f, r.getHeight() * 0.85f);
        const float pillW = pillH * 1.9f;
        auto pill = juce::Rectangle<float> (r.getX(), r.getCentreY() - pillH * 0.5f, pillW, pillH);
        const bool acik = b.getToggleState();

        g.setColour (acik ? (b.isEnabled() ? ALTIN : ALTIN.withAlpha (0.4f)) : juce::Colour (0xff262a34));
        g.fillRoundedRectangle (pill, pillH * 0.5f);
        const float top = pillH - 4.0f;
        const float tx = acik ? pill.getRight() - top - 2.0f : pill.getX() + 2.0f;
        g.setColour (acik ? KOYU_YAZI : METIN.withAlpha (0.85f));
        g.fillEllipse (tx, pill.getCentreY() - top * 0.5f, top, top);

        if (b.getButtonText().isNotEmpty())
        {
            g.setColour (over ? ALTIN : METIN.withAlpha (b.isEnabled() ? 1.0f : 0.5f));
            g.setFont (juce::Font (11.5f, juce::Font::bold));
            g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (pillW + 7.0f).toNearestInt(),
                              juce::Justification::centredLeft, 1);
        }
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (12.5f); }
    juce::Font getPopupMenuFont() override { return juce::Font (13.5f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override
    { return juce::Font (juce::jmin (13.5f, (float) h * 0.42f), juce::Font::bold); }
};

} // namespace sweax::ui
