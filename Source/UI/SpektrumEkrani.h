#pragma once
// SWEAX NEXT — UI/SpektrumEkrani.h
// Ana ekran spektrum + EQ eğrisi (M2.1, eski SWEAX görsel dili):
//  • Giriş (solgun dolgu) ve çıkış (turkuaz çizgi+dolgu) spektrumu — RT FIFO'dan UI iş parçacığında hesap.
//  • Hesap: 72 log-aralıklı noktada Hann pencereli Goertzel (4096 örnek) — FFT bağımlılığı yok, log eksende
//    bin toplama sanatı yok; nokta başına doğrudan genlik. Üstel yumuşatma (hızlı atak / yavaş bırakma).
//  • EQ yanıt eğrisi (altın): ParamCache'teki HPF + 6 bandın statik yanıtı, CorrectiveEQ ile AYNI design::
//    fonksiyonlarından (BiquadCoeffs::magnitudeAt) — çizilen şey duyulanın kendisidir. Dinamik bantlar merkezde
//    elmas işaretiyle gösterilir (aralık oku).
//  • 30 Hz kendi timer'ı; editör kapalıyken FIFO yazılmaz (processor editorAttached).
#include "../PluginProcessor.h"
#include "SweaxLnF.h"

namespace sweax::ui
{

class SpektrumEkrani : public juce::Component, private juce::Timer
{
public:
    static constexpr int NOKTA = 72;
    // Tools/SweaxShots editörü masaüstüne koymadan çizer; isShowing() orada hep false olduğu için hesap zorlanır
    static inline bool ekranDisiCiz = false;
    static constexpr int PENCERE = 4096;

    explicit SpektrumEkrani (SweaxProcessor& p) : proc (p)
    {
        setOpaque (true);
        girisHalka.assign (PENCERE, 0.0f);
        cikisHalka.assign (PENCERE, 0.0f);
        for (int i = 0; i < NOKTA; ++i)
        {
            girisDb[(size_t) i] = cikisDb[(size_t) i] = -100.0f;
            frekans[(size_t) i] = 30.0 * std::pow (20000.0 / 30.0, (double) i / (NOKTA - 1));
        }
        // denetim: yeniden açılışta FIFO'daki bayat içerik boşaltılır
        { float tmp[1024]; while (proc.spectrumInFifo().pop (tmp, 1024) > 0) {} while (proc.spectrumOutFifo().pop (tmp, 1024) > 0) {} }
        startTimerHz (30);
    }
    ~SpektrumEkrani() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        using namespace palet;
        auto r = getLocalBounds().toFloat();
        g.fillAll (ZEMIN);                                  // opak bileşen: köşeler de boyanır (denetim)
        g.setColour (ZEMIN.darker (0.2f));
        g.fillRoundedRectangle (r, 8.0f);

        const float x0 = 6.0f, x1 = r.getWidth() - 6.0f;
        const float y0 = 6.0f, y1 = r.getHeight() - 6.0f;
        // ---- ızgara (eski SWEAX reçetesi: 60→16k etiketli dikey çizgiler + dB yatayları) ----
        static const double gridHz[] = { 60.0, 100.0, 200.0, 400.0, 800.0, 1500.0, 3000.0, 6000.0, 10000.0, 16000.0 };
        static const char* gridAd[]  = { "60", "100", "200", "400", "800", "1.5k", "3k", "6k", "10k", "16k" };
        g.setColour (CIZGI.withAlpha (0.4f));
        for (double f : gridHz)
            g.drawVerticalLine ((int) xIcin (f, x0, x1), y0, y1);
        for (float db : { -60.0f, -40.0f, -20.0f })
        {
            const float y = ySpekIcin (db, y0, y1);
            g.setColour (CIZGI.withAlpha (0.3f));
            g.drawHorizontalLine ((int) y, x0, x1);
        }
        g.setFont (juce::Font (9.5f));
        g.setColour (SOLGUN.withAlpha (0.85f));
        for (size_t i = 0; i < std::size (gridHz); ++i)
            g.drawText (gridAd[i], (int) xIcin (gridHz[i], x0, x1) - 16, (int) y1 - 12, 32, 11, juce::Justification::centred);
        g.setColour (SOLGUN.withAlpha (0.6f));
        for (float db : { -20.0f, -40.0f, -60.0f })
            g.drawText (juce::String ((int) db), (int) x0 + 2, (int) ySpekIcin (db, y0, y1) - 11, 28, 10, juce::Justification::left);

        // ---- spektrumlar ----
        auto yol = [&] (const std::array<float, NOKTA>& v)
        {
            juce::Path pth;
            pth.startNewSubPath (x0, y1);
            for (int i = 0; i < NOKTA; ++i)
                pth.lineTo (xIcin (frekans[(size_t) i], x0, x1), ySpekIcin (v[(size_t) i], y0, y1));
            pth.lineTo (x1, y1);
            pth.closeSubPath();
            return pth;
        };
        {
            auto pg = yol (girisDb);
            g.setColour (SOLGUN.withAlpha (0.16f));
            g.fillPath (pg);
        }
        {
            auto pc = yol (cikisDb);
            g.setColour (TURKUAZ.withAlpha (0.22f));
            g.fillPath (pc);
            juce::Path cizgi;
            for (int i = 0; i < NOKTA; ++i)
            {
                const float x = xIcin (frekans[(size_t) i], x0, x1), y = ySpekIcin (cikisDb[(size_t) i], y0, y1);
                if (i == 0) cizgi.startNewSubPath (x, y); else cizgi.lineTo (x, y);
            }
            g.setColour (TURKUAZ.withAlpha (0.9f));
            g.strokePath (cizgi, juce::PathStrokeType (1.6f));
        }

        // ---- EQ eğrisi (statik yanıt; ±15 dB ekseni) ----
        {
            auto& c = proc.paramCache();
            std::vector<BiquadCoeffs> ks;
            if (c.flag (params::EqOn))
            {
                if (c.flag (params::EqHpfOn))
                {
                    const int stages = c.choice (params::EqHpfSlope) == 1 ? 2 : 1;
                    for (int st = 0; st < stages; ++st)
                        ks.push_back (design::highpass (proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0,
                                                        std::clamp ((double) c.raw (params::EqHpfHz), 20.0, 400.0), 0.7071));
                }
                for (int b = 0; b < params::EQ_BANT_SAYISI; ++b)
                {
                    if (! c.flag (params::eqBant (b, 0))) continue;
                    const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
                    const double hz = c.raw (params::eqBant (b, 2)), q = c.raw (params::eqBant (b, 4));
                    const double gain = c.raw (params::eqBant (b, 3));
                    switch (c.choice (params::eqBant (b, 1)))
                    {
                        case 1: ks.push_back (design::lowShelf (fs, hz, gain, q)); break;
                        case 2: ks.push_back (design::highShelf (fs, hz, gain, q)); break;
                        case 3: ks.push_back (design::notch (fs, hz, q)); break;
                        default: ks.push_back (design::bellMatched (fs, hz, gain, q)); break;
                    }
                }
            }
            if (! ks.empty())
            {
                const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
                juce::Path e;
                for (int i = 0; i < NOKTA; ++i)
                {
                    double mag = 1.0;
                    for (const auto& k : ks) mag *= k.magnitudeAt (frekans[(size_t) i], fs);
                    const float db = (float) (20.0 * std::log10 (std::max (mag, 1.0e-6)));
                    const float x = xIcin (frekans[(size_t) i], x0, x1);
                    const float y = yEqIcin (db, y0, y1);
                    if (i == 0) e.startNewSubPath (x, y); else e.lineTo (x, y);
                }
                g.setColour (ALTIN.withAlpha (0.95f));
                g.strokePath (e, juce::PathStrokeType (2.0f));
                // dinamik bant işaretleri (elmas + aşağı çizgi = aralık)
                for (int b = 0; b < params::EQ_BANT_SAYISI; ++b)
                {
                    if (! c.flag (params::eqBant (b, 0)) || ! c.flag (params::eqBant (b, 5))) continue;
                    const float x = xIcin ((double) c.raw (params::eqBant (b, 2)), x0, x1);
                    const float y = yEqIcin (0.0f, y0, y1);
                    const float rng = c.raw (params::eqBant (b, 7));
                    g.setColour (ALTIN.withAlpha (0.8f));
                    juce::Path elmas;
                    elmas.addQuadrilateral (x, y - 5.0f, x + 4.0f, y, x, y + 5.0f, x - 4.0f, y);
                    g.fillPath (elmas);
                    g.drawLine (x, y, x, yEqIcin (rng, y0, y1), 1.2f);
                }
            }
        }
    }

private:
    void timerCallback() override
    {
        // FIFO'ları tüket → halka tamponlar (görünmezken de: bayat birikme olmasın)
        cek (proc.spectrumInFifo(), girisHalka, girisPos);
        cek (proc.spectrumOutFifo(), cikisHalka, cikisPos);
        if (! isShowing() && ! ekranDisiCiz) return;        // denetim (M3.2): gizliyken 590k MAC/sn Goertzel boşa yanıyordu
        hesapla (girisHalka, girisPos, girisDb);
        hesapla (cikisHalka, cikisPos, cikisDb);
        repaint();
    }

    void cek (SpscFifo<float>& f, std::vector<float>& halka, int& pos)
    {
        float tmp[1024];
        for (;;)
        {
            const size_t n = f.pop (tmp, 1024);
            if (n == 0) break;
            for (size_t i = 0; i < n; ++i)
            {
                const float v = tmp[i];
                halka[(size_t) pos] = (v > -1.0e6f && v < 1.0e6f) ? v : 0.0f;   // denetim: NaN eğriyi kilitliyordu
                pos = (pos + 1) % PENCERE;
            }
        }
    }

    void hesapla (const std::vector<float>& halka, int pos, std::array<float, NOKTA>& hedef)
    {
        // pencereyi kronolojik sıraya kopyala + Hann
        const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
        std::array<float, PENCERE> w;
        for (int i = 0; i < PENCERE; ++i)
        {
            const float hann = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (PENCERE - 1));
            w[(size_t) i] = halka[(size_t) ((pos + i) % PENCERE)] * hann;
        }
        for (int k = 0; k < NOKTA; ++k)
        {
            // Goertzel
            const double om = 2.0 * juce::MathConstants<double>::pi * frekans[(size_t) k] / fs;
            const double cw = 2.0 * std::cos (om);
            double s0 = 0.0, s1 = 0.0, s2 = 0.0;
            for (int i = 0; i < PENCERE; ++i)
            {
                s0 = (double) w[(size_t) i] + cw * s1 - s2;
                s2 = s1; s1 = s0;
            }
            const double guc = s1 * s1 + s2 * s2 - cw * s1 * s2;
            const float db = (float) (10.0 * std::log10 (std::max (guc, 1.0e-12)) - 20.0 * std::log10 ((double) PENCERE * 0.25));
            float& v = hedef[(size_t) k];
            if (! std::isfinite (db)) continue;                 // sigorta
            const float coef = db > v ? 0.45f : 0.12f;         // hızlı atak, yavaş bırakma
            v = v + coef * (db - v);
            if (! std::isfinite (v) || v < -100.0f) v = -100.0f;
        }
    }

    float xIcin (double hz, float x0, float x1) const
    {
        const double t = std::log (hz / 30.0) / std::log (20000.0 / 30.0);
        return x0 + (float) std::clamp (t, 0.0, 1.0) * (x1 - x0);
    }
    float ySpekIcin (float db, float y0, float y1) const
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db + 80.0f) / 80.0f);   // −80..0 dB
        return y1 - t * (y1 - y0);
    }
    float yEqIcin (float db, float y0, float y1) const
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db + 15.0f) / 30.0f);   // ±15 dB
        return y1 - t * (y1 - y0);
    }

    SweaxProcessor& proc;
    std::vector<float> girisHalka, cikisHalka;
    int girisPos { 0 }, cikisPos { 0 };
    std::array<double, NOKTA> frekans {};
    std::array<float, NOKTA> girisDb {}, cikisDb {};
};

} // namespace sweax::ui
