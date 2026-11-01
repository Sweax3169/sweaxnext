// SWEAX NEXT — Tests/Integration/EditorUiTest.cpp
// UI v2 duman testi: plan yayınlanmış processor ile editör MixReady'de açılır (konsol düzeni: spektrum +
// metreler + makrolar + sekmeler), HER sekme seçilip boyanır (ZİNCİR kartları, EQ/DİNAMİK/TUNE/EFEKTLER/
// ÇIKIŞ knob şeritleri ≈100 APVTS bağlantısı), editör kapanır — sızıntı/çökme yok.
// SWX_UI_PNG=<taban-yol> verilirse her sekme <taban>_<sekme>.png olarak diske yazılır (görsel inceleme).
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Analysis/Report.h"
#include "Decision/Planner.h"
#include "Decision/PlanPublisher.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdlib>

namespace
{
    // mesaj dongusu pompasi (JUCE_MODAL_LOOPS_PERMITTED kapali): gercek sure bekle + vadesi gelen timerlari isle
    void pompala (int ms)
    {
        juce::Thread::sleep (ms);
        juce::Timer::callPendingTimersSynchronously();
    }
}

SWX_TEST (editor_mixready_kartlar_ve_detaylar)
{
    using namespace sweax;
    using namespace sweax::decision;
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 256);
    proc.prepareToPlay (48000.0, 256);
    proc.analysisEngine().stopThread();
    proc.session().setRole (state::Role::Lead);

    AnalysisReport r;
    r.capturedSec = 30.0; r.voicedSec = 20.0; r.sentences = 10;
    r.voicedLufs = -24.0f; r.noiseFloorDb = -60.0f; r.snrDb = 36.0f; r.crestMedianDb = 9.0f;
    r.f0P5Hz = 100.0f; r.f0MedianHz = 150.0f; r.f0P95Hz = 240.0f;
    r.sibilantCenterHz = 6800.0f; r.sibilantPerMinute = 14.0f; r.sibilantEvents = 6;
    r.confidence.level = r.confidence.noise = r.confidence.tonal = r.confidence.dynamics = r.confidence.sibilance = 1.0f;
    ProblemDetection mud; mud.type = Problem::Mud; mud.probability = 0.9f; mud.severity = 0.8f; mud.confidence = 1.0f;
    mud.centerHz = 330.0f; mud.bandwidthOct = 1.2f; mud.valueDb = 7.0f;
    ProblemDetection sib; sib.type = Problem::Sibilance; sib.probability = 0.9f; sib.severity = 0.7f; sib.confidence = 1.0f;
    sib.centerHz = 6800.0f; sib.bandwidthOct = 1.0f; sib.valueDb = 15.0f; sib.count = 6;
    std::vector<ProblemDetection> dets = { mud, sib };
    PlannerInput in; in.report = &r; in.detections = &dets; in.role = state::Role::Lead; in.planId = 5;
    PlanPublisher::apply (Planner::plan (in), proc.paramCache(), proc.session(), proc.undoManager());
    SWX_BEKLE (proc.session().getScreen() == state::Screen::MixReady);

    {
        auto* edBase = proc.createEditor();
        SWX_BEKLE (edBase != nullptr);
        auto* ed = dynamic_cast<SweaxEditor*> (edBase);
        SWX_BEKLE (ed != nullptr);
        // denetim U-9: kurucu 1180x780 verdigi icin ayni boyut NO-OP idi — kucuk/buyuk yerlesimler de sinanir;
        // mesaj dongusu pompalanir ki timerCallback (GR esleme, plan tazeleme) gercekten kossun.
        ed->setSize (980, 660);
        pompala (80);
        ed->setSize (1400, 900);
        pompala (80);
        ed->setSize (1180, 780);
        pompala (60);

        const char* sekmeAd[SweaxEditor::SekmeSayisi] = { "zincir", "eq", "dinamik", "tune", "fx", "cikis" };
        const char* tabanYol = std::getenv ("SWX_UI_PNG");
        for (int s = 0; s < SweaxEditor::SekmeSayisi; ++s)
        {
            ed->sekmeSec (s);
            juce::Image img (juce::Image::ARGB, 1180, 780, true);
            juce::Graphics g (img);
            ed->paintEntireComponent (g, true);              // boyalama yolu: kartlar/şeritler render edilir
            if (tabanYol != nullptr)
            {
                juce::File f { juce::String (juce::CharPointer_UTF8 (tabanYol)) + "_" + sekmeAd[s] + ".png" };
                f.deleteFile();
                juce::FileOutputStream os (f);
                if (os.openedOk()) { juce::PNGImageFormat png; png.writeImageToStream (img, os); }
            }
        }
        // makro varsayılanı APVTS'te (şerit/knob bağlantıları değeri bozmadı)
        SWX_BEKLE (proc.paramCache().raw (params::MacroTemizlik) == 0.5f);
        // ekran gecisleri: Ready / Listening / RoleSelect yerlesimleri de boyanir (timer gecisi gorur)
        for (auto ekran : { state::Screen::Ready, state::Screen::Listening, state::Screen::RoleSelect, state::Screen::MixReady })
        {
            proc.session().setScreen (ekran);
            pompala (80);
            juce::Image img2 (juce::Image::ARGB, 1180, 780, true);
            juce::Graphics g2 (img2);
            ed->paintEntireComponent (g2, true);
        }
        delete ed;
    }
    proc.releaseResources();
}
