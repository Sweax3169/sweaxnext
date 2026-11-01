// SWEAX NEXT — Tests/Integration/PlanApplyTest.cpp
// Yayıncı: plan → APVTS + SESSION tek undo adımı; GERİ AL her şeyi döndürür; kilitli modüle dokunulmaz;
// kullanılmayan EQ bantları kapatılır; tabanlar plan değerine eşitlenir; state kaydet/yükle plan düğümünü korur.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "PluginProcessor.h"
#include "Analysis/Report.h"
#include "Decision/Planner.h"
#include "Decision/PlanPublisher.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
    using namespace sweax;
    using namespace sweax::decision;

    AnalysisReport rapor()
    {
        AnalysisReport r;
        r.capturedSec = 30.0; r.voicedSec = 20.0; r.sentences = 10;
        r.voicedLufs = -24.0f; r.noiseFloorDb = -60.0f; r.snrDb = 36.0f; r.crestMedianDb = 9.0f;
        r.f0P5Hz = 100.0f; r.f0MedianHz = 150.0f; r.f0P95Hz = 240.0f;
        r.sibilantCenterHz = 6800.0f; r.sibilantPerMinute = 14.0f; r.sibilantEvents = 6;
        r.confidence.level = r.confidence.noise = r.confidence.tonal = r.confidence.dynamics = r.confidence.sibilance = 1.0f;
        return r;
    }
    ProblemDetection d (Problem t, float p, float s, float c, float hz = 0, float bw = 0, float val = 0, int n = 0)
    {
        ProblemDetection x; x.type = t; x.probability = p; x.severity = s; x.confidence = c; x.centerHz = hz; x.bandwidthOct = bw; x.valueDb = val; x.count = n; return x;
    }
}

SWX_TEST (plan_uygula_geri_al_kilit)
{
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 256);
    proc.prepareToPlay (48000.0, 256);
    proc.analysisEngine().stopThread();
    auto& cache = proc.paramCache();
    auto& sess = proc.session();
    sess.setRole (state::Role::Lead);

    auto r = rapor();
    std::vector<ProblemDetection> dets = {
        d (Problem::Mud, 0.9f, 0.8f, 1.0f, 330.0f, 1.2f, 7.0f),
        d (Problem::Sibilance, 0.9f, 0.7f, 1.0f, 6800.0f, 1.0f, 15.0f, 6),
        d (Problem::MacroDynamics, 0.9f, 0.6f, 1.0f, 0, 0, 5.0f, 10),
    };
    PlannerInput in; in.report = &r; in.detections = &dets; in.role = state::Role::Lead; in.planId = 3;
    auto plan = Planner::plan (in);

    // önce elle bir bant 5'i aç (bayat bant): plan onu KAPATMALI
    cache.param (params::eqBant (4, 0))->setValueNotifyingHost (1.0f);
    SWX_BEKLE (cache.flag (params::eqBant (4, 0)));
    const float eskiTrim = cache.raw (params::InputTrim);

    const int yazilan = PlanPublisher::apply (plan, cache, sess, proc.undoManager());
    SWX_BEKLE (yazilan > 10);
    SWX_BEKLE (cache.flag (params::EqOn));
    SWX_BEKLE (cache.flag (params::DsOn));
    SWX_BEKLE (cache.flag (params::LevOn));
    SWX_BEKLE (! cache.flag (params::eqBant (4, 0)));        // bayat bant kapandı
    SWX_BEKLE_YAKIN (cache.raw (params::InputTrim), 6.0f, 0.05f);   // −24 → −18
    SWX_BEKLE_EQ ((int) sess.getPlanId(), 3);
    SWX_BEKLE (sess.getScreen() == state::Screen::MixReady);
    // taban = plan değeri
    SWX_BEKLE_YAKIN (sess.getBaseline (params::DsRange, -1.0f), cache.raw (params::DsRange), 1e-4f);
    // plan düğümü: EQ modülü gerekçeli
    auto eqNode = PlanPublisher::moduleNode (sess, state::modul::EQ);
    SWX_BEKLE (eqNode.isValid());
    SWX_BEKLE (eqNode.getNumChildren() >= 1);

    // ---- GERİ AL: tek adım her şeyi döndürür ----
    SWX_BEKLE (proc.undoManager().canUndo());
    proc.undoManager().undo();
    SWX_BEKLE (! cache.flag (params::EqOn));
    SWX_BEKLE (! cache.flag (params::DsOn));
    SWX_BEKLE (! cache.flag (params::LevOn));
    SWX_BEKLE (cache.flag (params::eqBant (4, 0)));          // bayat bant geri geldi (kullanıcının haliydi)
    SWX_BEKLE_YAKIN (cache.raw (params::InputTrim), eskiTrim, 1e-4f);
    SWX_BEKLE_EQ ((int) sess.getPlanId(), 0);
    proc.undoManager().redo();
    SWX_BEKLE (cache.flag (params::EqOn));
    SWX_BEKLE_EQ ((int) sess.getPlanId(), 3);

    // ---- kilit: de-esser kilitliyken yeni plan ona dokunmaz ----
    sess.setLocked (state::modul::DEESS, true);
    const float dsRangeOnce = cache.raw (params::DsRange);
    auto dets2 = dets; dets2[1].severity = 0.2f;             // farklı sibilans → farklı range üretirdi
    PlannerInput in2; in2.report = &r; in2.detections = &dets2; in2.role = state::Role::Lead; in2.planId = 4;
    in2.setLocked (state::modul::DEESS, sess.isLocked (state::modul::DEESS));
    auto plan2 = Planner::plan (in2);
    PlanPublisher::apply (plan2, cache, sess, proc.undoManager());
    SWX_BEKLE_YAKIN (cache.raw (params::DsRange), dsRangeOnce, 1e-4f);   // kilitli: değişmedi
    SWX_BEKLE (cache.flag (params::DsOn));                                // açık kaldı
    SWX_BEKLE_EQ ((int) sess.getPlanId(), 4);

    // ---- state round-trip: plan düğümü ve planId korunur ----
    juce::MemoryBlock mb; proc.getStateInformation (mb);
    SweaxProcessor p2;
    p2.setPlayConfigDetails (2, 2, 48000.0, 256);
    p2.prepareToPlay (48000.0, 256);
    p2.analysisEngine().stopThread();
    p2.setStateInformation (mb.getData(), (int) mb.getSize());
    SWX_BEKLE_EQ ((int) p2.session().getPlanId(), 4);
    SWX_BEKLE (p2.session().getScreen() == state::Screen::MixReady);
    SWX_BEKLE_YAKIN (p2.paramCache().raw (params::DsRange), cache.raw (params::DsRange), 1e-4f);
    SWX_BEKLE (PlanPublisher::moduleNode (p2.session(), state::modul::EQ).isValid());
    SWX_BEKLE (p2.session().isLocked (state::modul::DEESS));
    p2.releaseResources();
    proc.releaseResources();
}

SWX_TEST (plan_makrolar_ve_yine_de_ac)
{
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 256);
    proc.prepareToPlay (48000.0, 256);
    proc.analysisEngine().stopThread();
    auto& cache = proc.paramCache();
    auto& sess = proc.session();

    auto r = rapor();
    std::vector<ProblemDetection> dets = { d (Problem::Mud, 0.9f, 0.8f, 1.0f, 330.0f, 1.2f, 7.0f) };
    PlannerInput in; in.report = &r; in.detections = &dets; in.planId = 1;
    PlanPublisher::apply (Planner::plan (in), cache, sess, proc.undoManager());

    // makrolar 0.5'e ve tabanlar 0.5: effective == raw
    SWX_BEKLE_YAKIN (cache.raw (params::MacroTemizlik), 0.5f, 1e-6f);
    // mud KESİM bandını bul: statik kazancı < −1 olan ilk açık bant (taban plosive bandı statik 0 dB'dir)
    int bant = -1;
    for (int b = 0; b < params::EQ_BANT_SAYISI; ++b)
        if (cache.flag (params::eqBant (b, 0)) && cache.raw (params::eqBant (b, 3)) < -1.0f) { bant = b; break; }
    SWX_BEKLE (bant >= 0);
    const auto gid = params::eqBant (bant, 3);
    const float g0 = cache.raw (gid);
    SWX_BEKLE (g0 < -1.0f);
    SWX_BEKLE_YAKIN (cache.effective (gid), g0, 1e-5f);
    // TEMİZLİK makrosu 1.0 → kesim ölçeği 2× (parametre aralığına klempli; guardrail PLANA uygulanır, makro kullanıcı alanı)
    cache.param (params::MacroTemizlik)->setValueNotifyingHost (1.0f);
    SWX_BEKLE_YAKIN (cache.effective (gid), std::max (-18.0f, g0 * 2.0f), 1e-4f);
    cache.param (params::MacroTemizlik)->setValueNotifyingHost (0.0f);
    SWX_BEKLE_YAKIN (cache.effective (gid), 0.0f, 1e-5f);
    // AI DEĞERİNE DÖN (makro) → 0.5
    PlanPublisher::returnMacroToAi (params::Makro::Temizlik, cache, sess, proc.undoManager());
    SWX_BEKLE_YAKIN (cache.raw (params::MacroTemizlik), 0.5f, 1e-6f);
    // parametreyi elle boz → TÜM AI DEĞERLERİNE DÖN → taban değerine döner
    cache.param (gid)->setValueNotifyingHost (cache.toNormalized (gid, 2.0f));
    PlanPublisher::returnAllToAi (cache, sess, proc.undoManager());
    SWX_BEKLE_YAKIN (cache.raw (gid), g0, 1e-4f);

    // YİNE DE AÇ: de-esser planda cerrahi işlem yapmıyor (kanıt yok → birinci aşama 0 dB, yalnız son aşama sigortası)
    // → nötr aç + kilit + UserRequest; nötr son aşama sigortasını da kapatır (kullanıcı modülü tamamen devraldı)
    SWX_BEKLE (cache.raw (params::DsRange) < 0.05f);
    PlanPublisher::forceEnableNeutral (state::modul::DEESS, cache, sess, proc.undoManager());
    SWX_BEKLE (cache.flag (params::DsOn));
    SWX_BEKLE_YAKIN (cache.raw (params::DsRange), 0.0f, 1e-6f);           // nötr: işitilebilir değişim yok
    SWX_BEKLE (! cache.flag (params::Ds2On));
    SWX_BEKLE (sess.isLocked (state::modul::DEESS));
    auto dsNode = PlanPublisher::moduleNode (sess, state::modul::DEESS);
    bool userReq = false;
    for (int i = 0; i < dsNode.getNumChildren(); ++i)
        if ((int) dsNode.getChild (i).getProperty ("rule", -1) == (int) ReasonRule::UserRequest) userReq = true;
    SWX_BEKLE (userReq);
    // tek undo adımı geri alır
    proc.undoManager().undo();
    // M3.5: "yine de aç" nötrü son aşama sigortasını da kapatır; undo planın hâline döner (birinci aşama 0 dB + sigorta açık)
    SWX_BEKLE (cache.raw (params::DsRange) < 0.05f);
    SWX_BEKLE (cache.flag (params::Ds2On));
    SWX_BEKLE (! sess.isLocked (state::modul::DEESS));
    proc.releaseResources();
}

SWX_TEST (yine_de_ac_eq_sat_notr)
{
    // Denetim regresyonu: EQ icin YINE DE AC bayat bantlari geri aciyordu; SAT icin drive0+mix1 hala bukuyordu.
    using namespace sweax;
    using namespace sweax::decision;
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 256);
    proc.prepareToPlay (48000.0, 256);
    proc.analysisEngine().stopThread();
    auto& c = proc.paramCache();
    // bayat bant birak
    c.param (params::EqB3On)->setValueNotifyingHost (1.0f);
    c.param (params::EqB3Gain)->setValueNotifyingHost (c.toNormalized (params::EqB3Gain, -6.0f));
    PlanPublisher::forceEnableNeutral (state::modul::EQ, c, proc.session(), proc.undoManager());
    SWX_BEKLE (c.flag (params::EqOn));
    for (int b = 0; b < params::EQ_BANT_SAYISI; ++b) SWX_BEKLE (! c.flag (params::eqBant (b, 0)));
    SWX_BEKLE (! c.flag (params::EqHpfOn));
    PlanPublisher::forceEnableNeutral (state::modul::SAT, c, proc.session(), proc.undoManager());
    SWX_BEKLE (c.flag (params::SatOn));
    SWX_BEKLE (c.raw (params::SatMix) < 0.001f);          // mix 0 = birebir
    proc.releaseResources();
}
