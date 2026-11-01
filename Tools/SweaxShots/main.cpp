// SWEAX NEXT — Tools/SweaxShots/main.cpp
// Editörü EKRAN DIŞINDA çizer: pencere açmaz, masaüstüne dokunmaz. Gerçek SweaxProcessor + SweaxEditor kurulur,
// demo vokal blok blok işlenir, timer'lar gerçek zaman hızında pompalanır ve editör paintEntireComponent ile bir
// juce::Image'a çizilir. Tıklamalar düğmenin onClick'i çağrılarak yapılır; imleç yalnız kareye çizilen bir işarettir.
//   • sabit PNG'ler  : her ekran, her sekme, birkaç stilin ZİNCİR sayfası (--olcek ile retina çözünürlük)
//   • senaryolar     : ham BGRA kareler ffmpeg'e borulanır → kayıpsız .mkv (GIF/MP4/WebM sonradan türetilir)
// Kullanım: SweaxShots demo.wav cikis_klasoru [--olcek 2] [--fps 25] [--ffmpeg ffmpeg]
// Turda dinleme bölümü 4× hızlıdır (README bunu söyler); geri kalanı gerçek zamanlı.
// Analiz motorunun iş parçacığı durdurulur, analiz processPendingSync ile eşzamanlı koşar (kareler tekrarlanabilir).
#define _CRT_SECURE_NO_WARNINGS
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UI/Metin.h"
#include "UI/SweaxLnF.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>
#include <functional>

using namespace sweax;

namespace
{
    constexpr double SR = 48000.0;
    constexpr int BLOK = 480;                               // 10 ms
    constexpr int GEN = 1180, YUK = 780;

    juce::Component* bul (juce::Component& kok, const std::function<bool (juce::Component&)>& kosul)
    {
        for (auto* c : kok.getChildren())
        {
            if (kosul (*c)) return c;
            if (auto* r = bul (*c, kosul)) return r;
        }
        return nullptr;
    }

    juce::TextButton* dugme (juce::Component& kok, const juce::String& yazi)
    {
        return dynamic_cast<juce::TextButton*> (bul (kok, [&] (juce::Component& c)
        {
            auto* b = dynamic_cast<juce::TextButton*> (&c);
            return b != nullptr && b->isVisible() && b->getButtonText() == yazi;
        }));
    }

    juce::TextButton* birincilDugme (juce::Component& kok)
    {
        return dynamic_cast<juce::TextButton*> (bul (kok, [] (juce::Component& c)
        {
            return dynamic_cast<juce::TextButton*> (&c) != nullptr && (bool) c.getProperties()["primary"];
        }));
    }

    void pngYaz (const juce::Image& img, const juce::File& f)
    {
        f.deleteFile();
        juce::FileOutputStream os (f);
        if (os.openedOk()) { juce::PNGImageFormat png; png.writeImageToStream (img, os); }
        std::printf ("  %s\n", f.getFileName().toRawUTF8());
    }

    // ok imleci (beyaz gövde, koyu kontur) + tıklama halkası
    void imlecCiz (juce::Graphics& g, juce::Point<float> p, float tikYas)
    {
        if (tikYas >= 0.0f && tikYas < 0.45f)
        {
            const float k = tikYas / 0.45f;
            const float r = 8.0f + 26.0f * k;
            g.setColour (ui::palet::ALTIN.withAlpha (0.85f * (1.0f - k)));
            g.drawEllipse (p.x - r, p.y - r, 2 * r, 2 * r, 2.5f);
        }
        juce::Path ok;
        ok.startNewSubPath (0, 0);
        ok.lineTo (0, 19); ok.lineTo (4.6f, 14.6f); ok.lineTo (8.0f, 22.0f); ok.lineTo (11.2f, 20.6f);
        ok.lineTo (7.9f, 13.4f); ok.lineTo (14.0f, 13.4f); ok.closeSubPath();
        ok.applyTransform (juce::AffineTransform::translation (p));
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillPath (ok, juce::AffineTransform::translation (1.2f, 1.8f));
        g.setColour (juce::Colours::white);
        g.fillPath (ok);
        g.setColour (juce::Colour (0xff101418));
        g.strokePath (ok, juce::PathStrokeType (1.3f));
    }

    struct Cekim
    {
        SweaxProcessor& proc;
        SweaxEditor& ed;
        std::vector<float> ses;
        int64_t konum { 0 };
        juce::AudioBuffer<float> buf { 2, BLOK };
        juce::MidiBuffer midi;
        double bas { juce::Time::getMillisecondCounterHiRes() };
        int64_t kareNo { 0 };
        int fps { 25 };
        int hiz { 1 };                                      // kare başına işlenen ses = hız × gerçek zaman

        // imleç
        juce::Point<float> imlec { 1020.0f, 690.0f }, hedefBas, hedef;
        double hareketBas { -1.0 }, hareketSure { 0.0 }, sonTik { -10.0 };
        bool imlecGorunur { false };
        std::FILE* boru { nullptr };

        double zaman() const { return (double) kareNo / fps; }

        void sesIsle (int n)
        {
            while (n > 0)
            {
                const int m = std::min (n, BLOK);
                for (int i = 0; i < m; ++i)
                {
                    const float v = ses[(size_t) ((konum + i) % (int64_t) ses.size())];
                    buf.setSample (0, i, v);
                    buf.setSample (1, i, v);
                }
                buf.setSize (2, m, true, false, true);
                proc.processBlock (buf, midi);
                buf.setSize (2, BLOK, true, false, true);
                konum += m;
                n -= m;
            }
            proc.analysisEngine().processPendingSync();
        }

        // bir kare: dt kadar ses + gerçek zaman hızında timer pompası
        void ilerle()
        {
            sesIsle ((int) std::lround (SR * hiz / fps));
            ++kareNo;
            const double hedefMs = bas + 1000.0 * (double) kareNo / fps;
            const double simdi = juce::Time::getMillisecondCounterHiRes();
            if (hedefMs > simdi) juce::Thread::sleep ((int) (hedefMs - simdi));
            juce::Timer::callPendingTimersSynchronously();
        }

        juce::Image ciz (float olcek, bool imlecli)
        {
            juce::Image img (juce::Image::ARGB, (int) std::lround (GEN * olcek), (int) std::lround (YUK * olcek), true);
            juce::Graphics g (img);
            g.addTransform (juce::AffineTransform::scale (olcek));
            ed.paintEntireComponent (g, true);
            if (imlecli && imlecGorunur)
            {
                if (hareketBas >= 0.0)
                {
                    const double k = juce::jlimit (0.0, 1.0, (zaman() - hareketBas) / hareketSure);
                    const double e = k < 0.5 ? 4 * k * k * k : 1 - std::pow (-2 * k + 2, 3) / 2;   // easeInOutCubic
                    imlec = hedefBas + (hedef - hedefBas) * (float) e;
                }
                imlecCiz (g, imlec, (float) (zaman() - sonTik));
            }
            return img;
        }

        void kareYaz()
        {
            if (boru == nullptr) return;
            auto img = ciz (1.0f, true);
            juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
            for (int y = 0; y < bd.height; ++y)
                std::fwrite (bd.getLinePointer (y), 1, (size_t) bd.width * 4, boru);
        }

        // süre boyunca kare üret
        void bekle (double sn)
        {
            const int n = (int) std::lround (sn * fps);
            for (int i = 0; i < n; ++i) { ilerle(); kareYaz(); }
        }

        juce::Point<float> merkez (juce::Component& c)
        {
            return ed.getLocalArea (&c, c.getLocalBounds()).getCentre().toFloat() + juce::Point<float> (4.0f, 6.0f);
        }

        void imleciGotur (juce::Component& c, double sure)
        {
            imlecGorunur = true;
            hedefBas = imlec; hedef = merkez (c);
            hareketBas = zaman(); hareketSure = sure;
            bekle (sure);
            imlec = hedef; hareketBas = -1.0;
        }

        void imleciGoturNokta (juce::Point<float> p, double sure)
        {
            imlecGorunur = true;
            hedefBas = imlec; hedef = p;
            hareketBas = zaman(); hareketSure = sure;
            bekle (sure);
            imlec = hedef; hareketBas = -1.0;
        }

        void tikla (juce::Button& b)
        {
            sonTik = zaman();
            if (b.onClick) b.onClick();
        }

        bool videoBasla (const juce::File& f, const juce::String& ffmpeg)
        {
            f.deleteFile();
            const juce::String komut = "\"" + ffmpeg + "\" -loglevel error -y -f rawvideo -pix_fmt bgra -s "
                                     + juce::String (GEN) + "x" + juce::String (YUK) + " -r " + juce::String (fps)
                                     + " -i - -c:v ffv1 -pix_fmt bgra \"" + f.getFullPathName() + "\"";
            // _popen komutu cmd /c'ye verir; cmd tırnakla başlayan satırda ilk ve son tırnağı yutar → bir kat daha sar
            boru = _popen (("\"" + komut + "\"").toRawUTF8(), "wb");
            std::printf ("video: %s\n", f.getFileName().toRawUTF8());
            return boru != nullptr;
        }

        void videoBitir()
        {
            if (boru != nullptr) { _pclose (boru); boru = nullptr; }
            imlecGorunur = false;
        }

        // kullanıcı yolu: stil kutusunda seçim → kutunun onChange'i oturuma yazar ve yeniden planlar
        void stilSec (int indeks)
        {
            auto* kutu = dynamic_cast<juce::ComboBox*> (bul (ed, [] (juce::Component& k)
            {
                auto* cb = dynamic_cast<juce::ComboBox*> (&k);
                return cb != nullptr && cb->getNumItems() == 16;
            }));
            if (kutu != nullptr) kutu->setSelectedId (indeks + 1, juce::sendNotificationSync);
        }

        // plan yayınlanana kadar (MixReady + yeni planId) kareleri sürdür
        void planBekle (juce::uint32 eskiPlan, double azami)
        {
            const double t0 = zaman();
            while (zaman() - t0 < azami)
            {
                ilerle(); kareYaz();
                if (proc.session().getScreen() == state::Screen::MixReady && proc.session().getPlanId() != eskiPlan
                    && ! proc.planPendingNow())
                    return;
            }
            std::printf ("UYARI: plan %0.1f sn icinde yayinlanmadi\n", azami);
        }
    };

    std::vector<float> wavOku (const juce::File& f)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        if (r == nullptr) return {};
        if (std::abs (r->sampleRate - SR) > 1.0) std::printf ("UYARI: demo %0.0f Hz, 48000 bekleniyordu\n", r->sampleRate);
        juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
        r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
        std::vector<float> m ((size_t) b.getNumSamples(), 0.0f);
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i) m[(size_t) i] += b.getSample (c, i) / (float) b.getNumChannels();
        return m;
    }
}

int main (int argc, char** argv)
{
    if (argc == 2 && juce::String (argv[1]) == "--bilgi")
    {
        // README'deki gecikme tablosu: kalite modu × örnekleme hızı → host'a bildirilen gecikme
        juce::ScopedJuceInitialiser_GUI init;
        const char* mod[3] = { "Canli", "Yuksek", "Ultra" };
        for (double sr : { 44100.0, 48000.0, 96000.0 })
            for (int q = 0; q < 3; ++q)
            {
                SweaxProcessor p;
                auto* prm = p.paramCache().param (params::QualityMode);
                prm->setValueNotifyingHost (p.paramCache().toNormalized (params::QualityMode, (float) q));
                p.setPlayConfigDetails (2, 2, sr, 512);
                p.prepareToPlay (sr, 512);
                std::printf ("%-6s %6.0f Hz  %5d ornek  %6.1f ms\n", mod[q], sr, p.getLatencySamples(),
                             1000.0 * p.getLatencySamples() / sr);
                p.releaseResources();
            }
        return 0;
    }
    if (argc < 3)
    {
        std::printf ("SweaxShots demo.wav cikis_klasoru [--olcek 2] [--fps 25] [--ffmpeg ffmpeg]\n"
                     "SweaxShots --bilgi    (kalite moduna gore gecikme)\n");
        return 2;
    }
    float olcek = 2.0f; int fps = 25; juce::String ffmpeg = "ffmpeg";
    for (int i = 3; i + 1 < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--olcek") olcek = juce::String (argv[++i]).getFloatValue();
        else if (a == "--fps") fps = juce::String (argv[++i]).getIntValue();
        else if (a == "--ffmpeg") ffmpeg = argv[++i];
    }
    juce::ScopedJuceInitialiser_GUI init;
    ui::SpektrumEkrani::ekranDisiCiz = true;                // masaüstünde değiliz: spektrum yine de hesaplansın
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const juce::File demo = cwd.getChildFile (juce::String (argv[1]));
    const juce::File dir = cwd.getChildFile (juce::String (argv[2]));
    dir.createDirectory();

    auto ses = wavOku (demo);
    if (ses.empty()) { std::printf ("HATA: demo okunamadi\n"); return 1; }

    SweaxProcessor proc;
    proc.setPlayConfigDetails (2, 2, SR, BLOK);
    proc.prepareToPlay (SR, BLOK);
    proc.analysisEngine().stopThread();

    std::unique_ptr<juce::AudioProcessorEditor> edBase (proc.createEditor());
    auto* ed = dynamic_cast<SweaxEditor*> (edBase.get());
    if (ed == nullptr) return 1;
    ed->setSize (GEN, YUK);

    Cekim c { proc, *ed, std::move (ses) };
    c.fps = fps;

    // ---------------- 1) TUR: rol → dinle → mix hazır → A/B → sekmeler ----------------
    c.videoBasla (dir.getChildFile ("tur.mkv"), ffmpeg);
    c.bekle (0.6);
    pngYaz (c.ciz (olcek, false), dir.getChildFile ("ekran-rol.png"));
    if (auto* b = dugme (*ed, metin::ROL_ANA)) { c.imleciGotur (*b, 0.9); c.tikla (*b); }
    c.bekle (0.5);
    pngYaz (c.ciz (olcek, false), dir.getChildFile ("ekran-hazir.png"));
    c.bekle (0.4);
    if (auto* b = birincilDugme (*ed)) { c.imleciGotur (*b, 0.8); c.tikla (*b); }
    c.imleciGoturNokta ({ 1060.0f, 720.0f }, 0.8);
    // dinleme 4× hızlı: 12 sn'lik videoda demo kaydın tamamı (~50 sn) duyulur, güven gerçek değerine çıkar
    c.hiz = 4;
    c.bekle (5.0);
    pngYaz (c.ciz (olcek, false), dir.getChildFile ("ekran-dinliyor.png"));
    c.bekle (7.0);
    c.hiz = 1;
    const auto plan0 = proc.session().getPlanId();
    if (auto* b = birincilDugme (*ed)) { c.imleciGotur (*b, 0.8); c.tikla (*b); }
    c.planBekle (plan0, 20.0);
    c.imleciGoturNokta ({ 640.0f, 300.0f }, 0.9);
    c.bekle (2.6);
    if (auto* b = dugme (*ed, metin::AB_ORIJINAL)) { c.imleciGotur (*b, 0.8); c.tikla (*b); }
    c.bekle (2.2);
    if (auto* b = dugme (*ed, metin::AB_ISLENMIS)) { c.imleciGotur (*b, 0.6); c.tikla (*b); }
    c.bekle (2.2);
    for (const auto* sekme : { &metin::TAB_DINAMIK, &metin::TAB_FX, &metin::TAB_ZINCIR })
    {
        if (auto* b = dugme (*ed, *sekme)) { c.imleciGotur (*b, 0.7); c.tikla (*b); }
        c.bekle (1.9);
    }
    c.videoBitir();

    // ---------------- 2) sabitler: her sekme (Temiz stil, analiz edilmiş demo) ----------------
    {
        const char* sekmeAd[SweaxEditor::SekmeSayisi] = { "zincir", "eq", "dinamik", "tune", "efektler", "cikis" };
        for (int s = 0; s < SweaxEditor::SekmeSayisi; ++s)
        {
            ed->sekmeSec (s);
            c.bekle (0.5);
            pngYaz (c.ciz (olcek, false), dir.getChildFile (juce::String ("sekme-") + sekmeAd[s] + ".png"));
        }
        ed->sekmeSec (SweaxEditor::SekmeZincir);
    }

    // ---------------- 3) STİLLER: aynı vokal, stil değişince plan + zincir sırası değişir ----------------
    c.videoBasla (dir.getChildFile ("stiller.mkv"), ffmpeg);
    c.bekle (1.0);
    const std::pair<int, const char*> stiller[] = { { 2, "trap" }, { 4, "killoki" }, { 8, "motive" }, { 11, "ceza" },
                                                    { 0, "dogal" }, { 1, "temiz" } };
    for (const auto& [indeks, ad] : stiller)
    {
        const auto onceki = proc.session().getPlanId();
        c.stilSec (indeks);
        c.planBekle (onceki, 15.0);
        c.bekle (2.4);
        pngYaz (c.ciz (olcek, false), dir.getChildFile (juce::String ("stil-") + ad + ".png"));
    }
    c.videoBitir();

    edBase.reset();
    proc.releaseResources();
    std::printf ("TAMAM\n");
    return 0;
}
