#pragma once
// SWEAX NEXT — UI/ModulSeridi.h
// UI v2 sekme içeriği: her modül bir ŞERİT — başlık çubuğu (ad + AÇIK pill'i + canlı GR) ve altında
// APVTS bağlantılı KNOB/COMBO/TOGGLE hücrelerinin akışı (satır dolunca sarar). DetaylarPaneli'nin
// (satır-slider listesi) yerine geçer: aynı parametre kümesi, eski SWEAX'in knob-grid diliyle.
// Kural değişmez: hücreler hiçbir değeri kendisi tutmaz; her şey Attachment üzerinden APVTS'tedir.
// Grup ayırıcı (örn. EQ bantları) tam satırlık ince başlıktır.
#include "../PluginProcessor.h"
#include "SweaxLnF.h"
#include "Metin.h"

namespace sweax::ui
{

class ModulSeridi : public juce::Component
{
public:
    struct Oge          // Param(id) veya Grup(başlık)
    {
        params::Id id { params::Bypass };
        juce::String grup;                      // doluysa grup ayırıcı
        static Oge P (params::Id i) { Oge o; o.id = i; return o; }
        static Oge G (juce::String b) { Oge o; o.grup = std::move (b); return o; }
    };

    // onId: başlıktaki AÇIK pill'i (params::SAYI ⇒ pill yok, ör. GENEL şeridi)
    ModulSeridi (SweaxProcessor& p, juce::String baslikMetni, params::Id onId, const std::vector<Oge>& ogeler)
        : proc (p), baslik (std::move (baslikMetni))
    {
        if (onId != params::SAYI)
        {
            acikPill = std::make_unique<juce::ToggleButton> (juce::String());
            addAndMakeVisible (*acikPill);
            const auto& t = params::TABLO[(size_t) onId];
            acikBag = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                proc.apvts(), juce::String (t.id.data(), t.id.size()), *acikPill);
        }
        for (const auto& o : ogeler)
        {
            auto h = std::make_unique<Hucre>();
            if (o.grup.isNotEmpty())
            {
                h->grup = std::make_unique<juce::Label>();
                h->grup->setText (o.grup, juce::dontSendNotification);
                h->grup->setFont (juce::Font (11.0f, juce::Font::bold));
                h->grup->setColour (juce::Label::textColourId, palet::ALTIN.withAlpha (0.75f));
                addAndMakeVisible (*h->grup);
                hucreler.push_back (std::move (h));
                continue;
            }
            const auto& t = params::TABLO[(size_t) o.id];
            const juce::String idStr (t.id.data(), t.id.size());
            const juce::String tabloAd (t.ad.data(), t.ad.size());
            h->etiket = std::make_unique<juce::Label>();
            h->etiket->setText (metin::paramKisaAd (idStr, tabloAd), juce::dontSendNotification);
            h->etiket->setFont (juce::Font (10.0f, juce::Font::bold));
            h->etiket->setColour (juce::Label::textColourId, palet::SOLGUN);
            h->etiket->setJustificationType (juce::Justification::centred);
            h->etiket->setMinimumHorizontalScale (0.7f);
            const auto ipucu = metin::paramAciklama (idStr);
            addAndMakeVisible (*h->etiket);
            auto& apvts = proc.apvts();
            if (t.tip == params::Tip::Bool)
            {
                h->toggle = std::make_unique<juce::ToggleButton> (juce::String());
                if (ipucu.isNotEmpty()) h->toggle->setTooltip (ipucu);
                addAndMakeVisible (*h->toggle);
                h->bBag = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, idStr, *h->toggle);
            }
            else if (t.tip == params::Tip::Choice)
            {
                h->combo = std::make_unique<juce::ComboBox>();
                juce::StringArray secenekler;
                secenekler.addTokens (juce::String (t.secenekler.data(), t.secenekler.size()), "|", "");
                h->combo->addItemList (secenekler, 1);
                if (ipucu.isNotEmpty()) h->combo->setTooltip (ipucu);
                addAndMakeVisible (*h->combo);
                h->cBag = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, idStr, *h->combo);
            }
            else
            {
                h->knob = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                          juce::Slider::TextBoxBelow);
                h->knob->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 68, 14);
                h->knob->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
                if (ipucu.isNotEmpty()) h->knob->setTooltip (ipucu);
                addAndMakeVisible (*h->knob);
                // değer metni parametrenin kendi stringFromValue'sundan gelir (ParamCache::createLayout)
                h->sBag = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, idStr, *h->knob);
            }
            hucreler.push_back (std::move (h));
        }
    }

    void canliGrGuncelle (float grDb)
    {
        if (std::abs (grDb - sonGr) < 0.05f) return;
        sonGr = grDb;
        repaint (0, 0, getWidth(), BASLIK_H);
    }

    // tek satırda dizilseydi gereken genişlik (grup ayırıcısı olan şeritler tam satır ister)
    int tercihGenislik() const
    {
        int w = IC_PAY * 2;
        for (const auto& h : hucreler)
        {
            if (h->grup != nullptr) return 1 << 20;                 // gruplu şerit (EQ) tam satır
            w += h->combo != nullptr ? 118 : (h->toggle != nullptr ? 64 : 74);
        }
        return juce::jmax (w, 190);                                 // denetim: tek knöblü şeritte başlık + GR sıkışıyordu
    }

    // verilen genişlik için yerleşimi yap ve toplam yüksekliği döndür (parent önce bunu çağırır)
    int duzenle (int w)
    {
        const int knobW = 74, comboW = 118, toggleW = 64, satirH = 96, grupH = 20;
        int x = IC_PAY, y = BASLIK_H + 6;
        int satirdaki = 0;
        auto yeniSatir = [&] { x = IC_PAY; y += satirH; satirdaki = 0; };
        for (auto& h : hucreler)
        {
            if (h->grup != nullptr)
            {
                if (satirdaki > 0) yeniSatir();
                h->grup->setBounds (IC_PAY, y, w - IC_PAY * 2, grupH - 4);
                y += grupH;
                continue;
            }
            const int hw = h->combo != nullptr ? comboW : (h->toggle != nullptr ? toggleW : knobW);
            if (x + hw > w - IC_PAY && satirdaki > 0) yeniSatir();
            auto hucre = juce::Rectangle<int> (x, y, hw, satirH - 6);
            h->etiket->setBounds (hucre.removeFromTop (13));
            if (h->knob != nullptr) h->knob->setBounds (hucre);
            else if (h->combo != nullptr) h->combo->setBounds (hucre.removeFromTop (24).reduced (4, 0));
            else if (h->toggle != nullptr) h->toggle->setBounds (hucre.removeFromTop (26).withTrimmedLeft (hw / 2 - 18));
            x += hw; ++satirdaki;
        }
        if (satirdaki > 0) y += satirH;
        y += 4;
        setSize (w, y);
        return y;
    }

    void paint (juce::Graphics& g) override
    {
        using namespace palet;
        auto r = getLocalBounds().toFloat();
        g.setColour (PANEL.withAlpha (0.55f));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (CIZGI.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

        auto bas = getLocalBounds().removeFromTop (BASLIK_H).reduced (IC_PAY, 0);
        if (acikPill != nullptr) bas.removeFromLeft (42);                     // pill başlığın solunda
        g.setColour (ALTIN);
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText (baslik, bas, juce::Justification::centredLeft);
        if (sonGr < -0.05f)
        {
            g.setColour (TURKUAZ);
            g.setFont (juce::Font (11.5f, juce::Font::bold));
            g.drawText ("GR " + juce::String (-sonGr, 1) + " dB", bas, juce::Justification::centredRight);
        }
    }

    void resized() override
    {
        if (acikPill != nullptr)
            acikPill->setBounds (IC_PAY, 4, 40, BASLIK_H - 8);
    }

private:
    static constexpr int BASLIK_H = 26, IC_PAY = 10;

    struct Hucre
    {
        std::unique_ptr<juce::Label> grup, etiket;
        std::unique_ptr<juce::Slider> knob;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sBag;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bBag;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cBag;
    };

    SweaxProcessor& proc;
    juce::String baslik;
    std::unique_ptr<juce::ToggleButton> acikPill;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> acikBag;
    std::vector<std::unique_ptr<Hucre>> hucreler;
    float sonGr { 0.0f };
};

// ---- Sekme sayfası: şeritlerin dikey akışı (kaydırmalı) ----
class SeritSayfasi : public juce::Component
{
public:
    SeritSayfasi()
    {
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&icerik, false);
        viewport.setScrollBarsShown (true, false);
    }

    ModulSeridi& seritEkle (SweaxProcessor& p, juce::String baslik, params::Id onId,
                            const std::vector<ModulSeridi::Oge>& ogeler)
    {
        auto s = std::make_unique<ModulSeridi> (p, std::move (baslik), onId, ogeler);
        icerik.addAndMakeVisible (*s);
        seritler.push_back (std::move (s));
        return *seritler.back();
    }

    void resized() override
    {
        viewport.setBounds (getLocalBounds());
        yerlestir (viewport.getMaximumVisibleWidth());
        // denetim (M3.2): ilk yerleşim kaydırma çubuğunu bilemez — içerik uzayıp çubuk çıktıysa
        // görünür genişlik daralır; bir kez daha yerleştir (sağ 8 px'in altında knöb kalmıyordu... kalıyordu).
        if (viewport.getMaximumVisibleWidth() != icerik.getWidth())
            yerlestir (viewport.getMaximumVisibleWidth());
    }

private:
    void yerlestir (const int w)
    {
        // dar şeritler yan yana akar (açgözlü satır doldurma); satır yüksekliği = en uzun şerit
        int y = 0;
        size_t i = 0;
        while (i < seritler.size())
        {
            // satıra girecek şeritleri seç
            size_t j = i; int toplamTercih = 0;
            while (j < seritler.size())
            {
                const int t = juce::jmin (seritler[j]->tercihGenislik(), w);
                if (j > i && toplamTercih + 8 + t > w) break;
                toplamTercih += (j > i ? 8 : 0) + t;
                ++j;
            }
            const int adet = (int) (j - i);
            // genişlik paylaşımı: tercih oranında, kalan boşluk eşit dağıtılır
            int x = 0, satirH = 0;
            const int fazla = juce::jmax (0, w - toplamTercih);
            for (size_t k = i; k < j; ++k)
            {
                const int t = juce::jmin (seritler[k]->tercihGenislik(), w);
                const int sw = t + fazla / adet;
                const int h = seritler[k]->duzenle (juce::jmin (sw, w - x));
                seritler[k]->setTopLeftPosition (x, y);
                satirH = juce::jmax (satirH, h);
                x += seritler[k]->getWidth() + 8;
            }
            y += satirH + 8;
            i = j;
        }
        icerik.setSize (w, juce::jmax (y, 1));
    }

public:
    std::vector<std::unique_ptr<ModulSeridi>>& tumSeritler() { return seritler; }

private:
    juce::Viewport viewport;
    juce::Component icerik;
    std::vector<std::unique_ptr<ModulSeridi>> seritler;
};

} // namespace sweax::ui
