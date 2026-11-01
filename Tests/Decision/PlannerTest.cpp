// SWEAX NEXT — Tests/Decision/PlannerTest.cpp
// Planlayıcı ilkeleri kod olarak test edilir: kanıt yoksa işlem yok (her modülde gerekçe); guardrails (statik ≤6,
// dinamik ≤9, formant ≤3 dinamik, Q ≤8 yalnız rezonans, HPF ≤ 0.75·F0_p05, de-ess ≤8/12, rol GR tavanı);
// düşük güven derinliği AZALTIR; determinizm (aynı girdi → birebir aynı plan); kuantalama.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "Analysis/Report.h"
#include "Decision/Planner.h"
#include "Decision/Receteler.h"
#include <cmath>

namespace
{
    using namespace sweax;
    using namespace sweax::decision;

    AnalysisReport tamRapor()
    {
        AnalysisReport r;
        r.capturedSec = 30.0; r.voicedSec = 20.0; r.roomToneSec = 5.0; r.sentences = 10;
        r.voicedLufs = -22.4f; r.integratedLufs = -24.0f; r.noiseFloorDb = -60.0f; r.snrDb = 38.0f;
        r.crestMedianDb = 9.0f; r.microStdMedianDb = 2.0f; r.sentenceIqrDb = 3.0f; r.sentenceP95P05Db = 4.0f;
        r.f0P5Hz = 95.0f; r.f0MedianHz = 140.0f; r.f0P95Hz = 220.0f;
        r.sibilantCenterHz = 7000.0f; r.sibilantPerMinute = 12.0f; r.sibilantEvents = 8;
        r.confidence.level = r.confidence.noise = r.confidence.tonal = r.confidence.dynamics = r.confidence.sibilance = 1.0f;
        return r;
    }
    ProblemDetection d (Problem t, float p, float s, float c, float hz = 0, float bw = 0, float val = 0, int n = 0)
    {
        ProblemDetection x; x.type = t; x.probability = p; x.severity = s; x.confidence = c; x.centerHz = hz; x.bandwidthOct = bw; x.valueDb = val; x.count = n; return x;
    }
    float paramOf (const ModulePlan& m, params::Id id, float yoksa = -999.0f)
    {
        for (const auto& p : m.params) if (p.id == id) return p.raw;
        return yoksa;
    }
}

SWX_TEST (planner_kanit_yoksa_cerrahi_islem_yok)
{
    // M2 sözleşmesi (sahip onaylı): kanıt yoksa CERRAHİ işlem yok (kesim bandı / de-esser yok) ama
    // rap vokal TABANI kurulur: HPF (F0'a bağlı) + comp A/B + leveler + sat + limiter, hepsi gerekçeli.
    auto r = tamRapor();
    std::vector<ProblemDetection> dets;                      // hiç sorun yok
    PlannerInput in; in.report = &r; in.detections = &dets; in.role = state::Role::Lead; in.planId = 7; in.style = 1;
    const auto plan = Planner::plan (in);
    SWX_BEKLE_EQ ((int) plan.modules.size(), 20);            // ... + glow + dirt + layer (M3.4) + yumuşat + formant + ton EQ (M3.5)
    for (const auto& m : plan.modules)
        SWX_BEKLE (! m.reasons.empty());                     // her modülün gerekçesi var (açık YA DA kapalı)
    // taban zinciri açık
    SWX_BEKLE (plan.module (state::modul::COMP_A)->enabled);
    SWX_BEKLE (plan.module (state::modul::COMP_B)->enabled);
    SWX_BEKLE (plan.module (state::modul::LEVELER)->enabled);
    SWX_BEKLE (plan.module (state::modul::SAT)->enabled);
    SWX_BEKLE (plan.module (state::modul::OUTPUT)->enabled);
    // de-esser: sibilans kanıtı YOK → BİRİNCİ aşama 0 dB (cerrahi işlem yok), yalnız SON AŞAMA sigortası açık
    // (M3.5: gövdeye göreli; zincir s'yi öne çıkarmadıysa hiçbir şey yapmaz — sahibin "s patlamaz" şartı)
    {
        const auto* dsm = plan.module (state::modul::DEESS);
        SWX_BEKLE (dsm->enabled);
        SWX_BEKLE_YAKIN (paramOf (*dsm, params::DsRange), 0.0f, 0.01f);
        SWX_BEKLE_YAKIN (paramOf (*dsm, params::Ds2On), 1.0f, 0.01f);
        SWX_BEKLE (paramOf (*dsm, params::Ds2Range) >= 1.0f);
    }
    // EQ: taban HPF var; kesim bandı YOK (yalnız stil presence/air boost'ları olabilir, kesim ≤0 yok)
    {
        const auto* eq = plan.module (state::modul::EQ);
        SWX_BEKLE (eq->enabled);
        SWX_BEKLE (paramOf (*eq, params::EqHpfOn) > 0.5f);
        const float fc = paramOf (*eq, params::EqHpfHz);
        SWX_BEKLE (fc > 40.0f && fc <= 0.75f * r.f0P5Hz);
        for (int b = 0; b < params::EQ_BANT_SAYISI; ++b)
        {
            const float g = paramOf (*eq, params::eqBant (b, 3), 0.0f);
            SWX_BEKLE (g >= 0.0f);                           // kanıt yokken kesim yok
        }
    }
    // comp tabanı gerekçesi CompBaseline ve GR rol tavanı içinde
    {
        const auto* ca = plan.module (state::modul::COMP_A);
        bool taban = false;
        for (const auto& rs : ca->reasons) if (rs.rule == ReasonRule::CompBaseline) { taban = true; SWX_BEKLE (rs.vals[1] <= Planner::roleGrCeiling (state::Role::Lead) + 0.01f); }
        SWX_BEKLE (taban);
    }
    // trim: −22.4 → −18: +4.4
    SWX_BEKLE_YAKIN (plan.inputTrimDb, 4.4f, 0.05f);
    SWX_BEKLE_EQ ((int) plan.planId, 7);
}

SWX_TEST (planner_guardrails)
{
    auto r = tamRapor();
    std::vector<ProblemDetection> dets = {
        d (Problem::Mud, 1.0f, 1.0f, 1.0f, 320.0f, 1.3f, 9.0f),                 // statik kesim tavan 6
        d (Problem::Resonance, 0.9f, 0.9f, 1.0f, 2800.0f, 0.15f, 12.0f, 1),     // formant bölgesi → dinamik ≤3
        d (Problem::Resonance, 0.8f, 0.8f, 1.0f, 1100.0f, 0.17f, 10.0f, 1),     // normal rezonans → statik, Q ≤8
        d (Problem::Harsh, 0.9f, 1.0f, 1.0f, 3200.0f, 1.3f, 6.0f),              // formant bölgesi → range ≤3
        d (Problem::LfNoise, 0.9f, 0.9f, 1.0f, 60.0f, 2.0f, 12.0f),             // HPF ≤ 0.75·F0p5 = 71.25
        d (Problem::Sibilance, 1.0f, 1.0f, 1.0f, 7000.0f, 1.0f, 20.0f, 8),      // de-ess ≤12 (kanıt tam)
        d (Problem::MacroDynamics, 1.0f, 0.8f, 1.0f, 0, 0, 6.0f, 10),
        d (Problem::MicroDynamics, 1.0f, 1.0f, 1.0f, 0, 0, 4.0f),
    };
    PlannerInput in; in.report = &r; in.detections = &dets; in.role = state::Role::Lead;
    const auto plan = Planner::plan (in);
    const auto* eq = plan.module (state::modul::EQ);
    SWX_BEKLE (eq != nullptr && eq->enabled);
    // HPF guardrail
    const float hpf = paramOf (*eq, params::EqHpfHz);
    SWX_BEKLE (hpf > 30.0f && hpf <= 0.75f * r.f0P5Hz + 0.01f);
    // bantları tara: statik kesim ≤6; dinamik range ≤9; formant bölgesindeki bantlar yalnız dinamik ve ≤3; Q ≤8
    bool formantStatikVar = false; int bantSay = 0;
    for (int b = 0; b < params::EQ_BANT_SAYISI; ++b)
    {
        if (paramOf (*eq, params::eqBant (b, 0), 0.0f) < 0.5f) continue;
        ++bantSay;
        const float hz = paramOf (*eq, params::eqBant (b, 2));
        const float g = paramOf (*eq, params::eqBant (b, 3), 0.0f);
        const float q = paramOf (*eq, params::eqBant (b, 4), 1.0f);
        const bool dyn = paramOf (*eq, params::eqBant (b, 5), 0.0f) > 0.5f;
        SWX_BEKLE (g >= -6.01f && g <= 3.01f);
        SWX_BEKLE (q <= 8.01f);
        if (dyn) SWX_BEKLE (paramOf (*eq, params::eqBant (b, 7), 0.0f) >= -9.01f);
        if (Planner::inFormantZone (hz))
        {
            if (! dyn && g < -0.01f) formantStatikVar = true;
            if (dyn) SWX_BEKLE (paramOf (*eq, params::eqBant (b, 7), 0.0f) >= -3.01f);
        }
    }
    SWX_BEKLE (bantSay >= 3);
    SWX_BEKLE (! formantStatikVar);                          // formant bölgesinde statik kesim YASAK
    // de-esser
    const auto* ds = plan.module (state::modul::DEESS);
    SWX_BEKLE (ds != nullptr && ds->enabled);
    SWX_BEKLE (paramOf (*ds, params::DsRange) <= 12.01f);
    // comp: rol GR tavanı gerekçede
    const auto* ca = plan.module (state::modul::COMP_A);
    SWX_BEKLE (ca != nullptr && ca->enabled);
    SWX_BEKLE (ca->reasons[0].vals[0] <= Planner::roleGrCeiling (state::Role::Lead) + 0.01f);
    // leveler
    SWX_BEKLE (plan.module (state::modul::LEVELER)->enabled);
    SWX_BEKLE (paramOf (*plan.module (state::modul::LEVELER), params::LevMaxUp) <= 8.01f);
    // aşırı sıkışmışsa comp atlanır
    {
        auto dets2 = dets;
        dets2.push_back (d (Problem::OverCompressed, 0.9f, 0.8f, 1.0f, 0, 0, 5.0f));
        PlannerInput in2 = in; in2.detections = &dets2;
        const auto p2 = Planner::plan (in2);
        const auto* ca2 = p2.module (state::modul::COMP_A);
        SWX_BEKLE (! ca2->enabled);
        SWX_BEKLE (ca2->reasons[0].rule == ReasonRule::CompSkipOverCompressed);
    }
    // düşük güven derinliği azaltır
    {
        auto r2 = tamRapor(); r2.confidence.tonal = 0.4f;    // enough() false
        PlannerInput in3 = in; in3.report = &r2;
        const auto p3 = Planner::plan (in3);
        const auto* eq3 = p3.module (state::modul::EQ);
        float gLow = 0.0f, gFull = 0.0f;
        for (int b = 0; b < params::EQ_BANT_SAYISI; ++b)
        {
            if (paramOf (*eq3, params::eqBant (b, 0), 0.0f) > 0.5f && std::fabs (paramOf (*eq3, params::eqBant (b, 2)) - 320.0f) < 40.0f) gLow = paramOf (*eq3, params::eqBant (b, 3), 0.0f);
            if (paramOf (*eq, params::eqBant (b, 0), 0.0f) > 0.5f && std::fabs (paramOf (*eq, params::eqBant (b, 2)) - 320.0f) < 40.0f) gFull = paramOf (*eq, params::eqBant (b, 3), 0.0f);
        }
        SWX_BEKLE (gLow > gFull);                            // daha az kesim (daha küçük mutlak)
    }
}

SWX_TEST (planner_determinizm_ve_kuantalama)
{
    auto r = tamRapor();
    std::vector<ProblemDetection> dets = {
        d (Problem::Mud, 0.77f, 0.63f, 0.91f, 333.3333f, 1.234f, 5.678f),
        d (Problem::Sibilance, 0.9f, 0.55f, 0.95f, 6789.0f, 1.0f, 12.3f, 5),
        d (Problem::MicroDynamics, 0.8f, 0.6f, 0.9f, 0, 0, 2.5f),
    };
    PlannerInput in; in.report = &r; in.detections = &dets;
    const auto a = Planner::plan (in);
    const auto b = Planner::plan (in);
    SWX_BEKLE_EQ (a.modules.size(), b.modules.size());
    for (size_t m = 0; m < a.modules.size(); ++m)
    {
        SWX_BEKLE_EQ (a.modules[m].enabled, b.modules[m].enabled);
        SWX_BEKLE_EQ (a.modules[m].params.size(), b.modules[m].params.size());
        for (size_t p = 0; p < a.modules[m].params.size(); ++p)
        {
            SWX_BEKLE_EQ ((int) a.modules[m].params[p].id, (int) b.modules[m].params[p].id);
            SWX_BEKLE (a.modules[m].params[p].raw == b.modules[m].params[p].raw);   // bit düzeyi (determinizm)
        }
    }
    // kuantalama: frekanslar 1/24 okt ızgarada, kazançlar 0.1 dB
    const auto* eq = a.module (state::modul::EQ);
    for (int bi = 0; bi < params::EQ_BANT_SAYISI; ++bi)
    {
        if (paramOf (*eq, params::eqBant (bi, 0), 0.0f) < 0.5f) continue;
        const float hz = paramOf (*eq, params::eqBant (bi, 2));
        const double k = 24.0 * std::log2 (hz / 20.0);
        SWX_BEKLE_YAKIN (k, std::round (k), 1e-3);
        const float g = paramOf (*eq, params::eqBant (bi, 3), 0.0f);
        SWX_BEKLE_YAKIN (g * 10.0f, std::round (g * 10.0f), 1e-3f);
    }
}

SWX_TEST (planner_master_rolu)
{
    // MASTER (M2.2): tam karisimda vokal araclari KAPALI (gate/tune/fx/compA/leveler), tutkal+hafif sat+width acik,
    // EQ hamleleri <=2 dB kelepceli, giris trimi ENTEGRE loudness'a gore, cikis stil hedef yayin seviyesine.
    auto r = tamRapor();
    r.integratedLufs = -20.0f;
    std::vector<ProblemDetection> dets = { d (Problem::Mud, 1.0f, 1.0f, 1.0f, 350.0f, 1.3f, 9.0f) };
    PlannerInput in; in.report = &r; in.detections = &dets; in.role = state::Role::Master; in.planId = 3; in.style = 2;
    const auto plan = Planner::plan (in);
    SWX_BEKLE (! plan.module (state::modul::GATE)->enabled);
    SWX_BEKLE (! plan.module (state::modul::TUNE)->enabled);
    SWX_BEKLE (! plan.module (state::modul::DOUBLER)->enabled);
    SWX_BEKLE (! plan.module (state::modul::DELAY)->enabled);
    SWX_BEKLE (! plan.module (state::modul::REVERB)->enabled);
    SWX_BEKLE (! plan.module (state::modul::COMP_A)->enabled);
    SWX_BEKLE (! plan.module (state::modul::LEVELER)->enabled);
    SWX_BEKLE (! plan.module (state::modul::DEESS)->enabled);
    SWX_BEKLE (plan.module (state::modul::COMP_B)->enabled);
    SWX_BEKLE (plan.module (state::modul::WIDTH)->enabled);
    SWX_BEKLE (plan.module (state::modul::SAT)->enabled);
    SWX_BEKLE_YAKIN (plan.inputTrimDb, 2.0f, 0.05f);
    {
        const auto* eq = plan.module (state::modul::EQ);
        SWX_BEKLE (eq->enabled);
        for (int b = 0; b < params::EQ_BANT_SAYISI; ++b)
        {
            const float g = paramOf (*eq, params::eqBant (b, 3), 0.0f);
            SWX_BEKLE (g >= -2.01f && g <= 1.51f);
        }
    }
    {
        const auto* o = plan.module (state::modul::OUTPUT);
        SWX_BEKLE_YAKIN (paramOf (*o, params::OutputTrim), 8.5f, 0.05f);   // Trap hedef -9.5 - (-18)
    }
}

SWX_TEST (planner_sanatci_stilleri)
{
    // M3.5 REÇETE→PLAN TESİSATI (sahip: "her biri için özel sıralama, özel kalite"): 12 sanatçı reçetesinin
    // (Decision/Receteler.h, indeks 4-15) plana BİREBİR aktarıldığı tablodan otomatik doğrulanır — reçete
    // değerleri güncellenince bu test kendini uyarlar; tesisat (alan → parametre eşlemesi) kilitlenir.
    auto r = tamRapor();
    std::vector<ProblemDetection> dets;
    auto planla = [&] (int stil)
    {
        PlannerInput in; in.report = &r; in.detections = &dets; in.role = state::Role::Lead; in.planId = 11; in.style = stil;
        return Planner::plan (in);
    };
    for (int stil = 4; stil < RECETE_SAYISI; ++stil)
    {
        const auto& rec = recete (stil);
        const auto plan = planla (stil);
        // sıra reçeteden
        SWX_BEKLE (plan.sira == rec.sira);
        // TUNE
        if (rec.tuneStr > 0.01f)
        {
            SWX_BEKLE_YAKIN (paramOf (*plan.module (state::modul::TUNE), params::TuneStrength), rec.tuneStr, 0.011f);
            SWX_BEKLE_YAKIN (paramOf (*plan.module (state::modul::TUNE), params::TuneSpeed), rec.tuneSpd, 1.1f);
        }
        else SWX_BEKLE (! plan.module (state::modul::TUNE)->enabled);
        // COMP A: oran stil tabanının altına inemez; zamanlar ve karakter reçeteden
        const auto* ca = plan.module (state::modul::COMP_A);
        SWX_BEKLE (paramOf (*ca, params::CaRatio) >= rec.ratioA - 0.06f);
        SWX_BEKLE_YAKIN (paramOf (*ca, params::CaAttack), std::clamp (rec.attackMs, 0.5f, 30.0f), 1.1f);
        if (rec.caKarakter >= 0.0f) SWX_BEKLE_YAKIN (paramOf (*ca, params::CaCharacter), std::round (rec.caKarakter), 0.01f);
        if (rec.caKnee >= 0.0f) SWX_BEKLE_YAKIN (paramOf (*ca, params::CaKnee), rec.caKnee, 0.01f);
        SWX_BEKLE_YAKIN (paramOf (*ca, params::CaMix), rec.caMix, 0.011f);
        SWX_BEKLE_YAKIN (paramOf (*ca, params::CaLookahead), rec.caLookahead, 0.01f);
        // COMP B
        const auto* cb = plan.module (state::modul::COMP_B);
        if (rec.grB > 0.05f)
        {
            SWX_BEKLE_YAKIN (paramOf (*cb, params::CbRatio), std::clamp (rec.cbRatio, 1.1f, 20.0f), 0.06f);
            SWX_BEKLE_YAKIN (paramOf (*cb, params::CbKnee), rec.cbKnee, 0.01f);
            SWX_BEKLE_YAKIN (paramOf (*cb, params::CbAttack), std::clamp (rec.cbAttack, 0.5f, 100.0f), 1.1f);
            SWX_BEKLE_YAKIN (paramOf (*cb, params::CbCharacter), std::round (rec.cbKarakter), 0.01f);
        }
        else SWX_BEKLE (! cb->enabled);
        // DE-ESSER: son aşama sigortası her reçetede açık
        const auto* dsm = plan.module (state::modul::DEESS);
        SWX_BEKLE (dsm->enabled);
        SWX_BEKLE_YAKIN (paramOf (*dsm, params::Ds2On), 1.0f, 0.01f);
        SWX_BEKLE_YAKIN (paramOf (*dsm, params::Ds2Range), std::clamp (rec.ds2Range, 0.0f, 9.0f), 0.06f);
        if (rec.dsTaban >= 0.5f) SWX_BEKLE_YAKIN (paramOf (*dsm, params::DsRange), std::clamp (rec.dsTaban, 0.5f, 8.0f), 0.06f);
        // YUMUŞAT / FORMANT
        if (rec.sooDepth > 0.01f)
        {
            const auto* sm = plan.module (state::modul::YUMUSAT);
            SWX_BEKLE (sm->enabled);
            SWX_BEKLE_YAKIN (paramOf (*sm, params::SooDepth), rec.sooDepth, 0.011f);
            SWX_BEKLE_YAKIN (paramOf (*sm, params::SooSharp), rec.sooSharp, 0.011f);
        }
        if (std::fabs (rec.frmShift) >= 0.25f)
        {
            const auto* fm = plan.module (state::modul::FORMANT);
            SWX_BEKLE (fm->enabled);
            SWX_BEKLE_YAKIN (paramOf (*fm, params::FrmShift), rec.frmShift, 0.06f);
        }
        else SWX_BEKLE (! plan.module (state::modul::FORMANT)->enabled);
        // SAT / KİR / KATMAN
        if (rec.satDrive > 0.1f)
        {
            const auto* sm = plan.module (state::modul::SAT);
            SWX_BEKLE_YAKIN (paramOf (*sm, params::SatDrive), rec.satDrive, 1.1f);    // dull/harsh kanıtı yoksa taban
            SWX_BEKLE_YAKIN (paramOf (*sm, params::SatChar), std::round (rec.satKarakter), 0.01f);
            SWX_BEKLE_YAKIN (paramOf (*sm, params::SatMix), std::clamp (rec.satMix, 0.0f, 1.0f), 0.011f);
        }
        const auto* dm = plan.module (state::modul::DIRT);
        SWX_BEKLE_EQ (dm->enabled, rec.dirtOn > 0.5f);
        if (dm->enabled)
        {
            SWX_BEKLE_YAKIN (paramOf (*dm, params::DirtMode), std::round (rec.dirtMode), 0.01f);
            SWX_BEKLE_YAKIN (paramOf (*dm, params::DirtLevel), std::clamp (rec.dirtLevel, -35.0f, 0.0f), 0.06f);
        }
        const auto* lm = plan.module (state::modul::LAYER);
        const bool katman = rec.layerOn > 0.5f && (std::fabs (rec.layerA) >= 0.5f || std::fabs (rec.layerB) >= 0.5f);
        SWX_BEKLE_EQ (lm->enabled, katman);
        if (katman) SWX_BEKLE_YAKIN (paramOf (*lm, params::LayerA), std::round (rec.layerA), 0.01f);
        // FX: doubler / delay / reverb
        if (rec.dblMix > 0.01f)
        {
            const auto* m = plan.module (state::modul::DOUBLER);
            SWX_BEKLE_YAKIN (paramOf (*m, params::DblSpreadMs), rec.dblSpreadMs, 1.1f);
            SWX_BEKLE_YAKIN (paramOf (*m, params::DblLpHz), rec.dblLpHz, rec.dblLpHz * 0.03f);
        }
        if (rec.dlyMix > 0.01f)
        {
            const auto* m = plan.module (state::modul::DELAY);
            SWX_BEKLE_YAKIN (paramOf (*m, params::DlySync), std::round (rec.dlySync), 0.01f);
            SWX_BEKLE_YAKIN (paramOf (*m, params::DlyTimeMs), rec.dlyTimeMs, 1.1f);
            SWX_BEKLE_YAKIN (paramOf (*m, params::DlyWidth), rec.dlyWidth, 0.011f);
            SWX_BEKLE_YAKIN (paramOf (*m, params::DlyHpHz), rec.dlyHpHz, rec.dlyHpHz * 0.03f);
        }
        if (rec.revMix > 0.01f)
        {
            const auto* m = plan.module (state::modul::REVERB);
            SWX_BEKLE_YAKIN (paramOf (*m, params::RevDecay), rec.revDecay, 0.011f);
            SWX_BEKLE_YAKIN (paramOf (*m, params::RevType), std::round (rec.revTip), 0.01f);
            SWX_BEKLE_YAKIN (paramOf (*m, params::RevPre), rec.revPre, 1.1f);
            SWX_BEKLE_YAKIN (paramOf (*m, params::RevHp), rec.revHpHz, rec.revHpHz * 0.03f);
        }
        // ÇIKIŞ
        SWX_BEKLE_YAKIN (paramOf (*plan.module (state::modul::OUTPUT), params::OutputTrim), rec.outTrimDb, 0.06f);
    }

    // ---- yapısal kimlik denetimi (araştırma çekirdeği; reçete ayarlansa da YÖN değişmemeli) ----
    const auto killoki = planla (4), ati = planla (6), uzi = planla (7), kava = planla (9), ceza = planla (11);
    const auto allame = planla (12), sagopa = planla (14);
    SWX_BEKLE (killoki.module (state::modul::DIRT)->enabled);                                          // trap metal kiri
    SWX_BEKLE (paramOf (*killoki.module (state::modul::SAT), params::SatDrive) >= 8.0f);               // SERT MOD
    SWX_BEKLE (uzi.module (state::modul::LAYER)->enabled);                                             // katman kültürü
    SWX_BEKLE (! allame.module (state::modul::TUNE)->enabled);                                         // Allame: autotune YOK (çok kaynaklı)
    SWX_BEKLE (paramOf (*uzi.module (state::modul::TUNE), params::TuneStrength) >= 0.6f);              // Uzi: duyulur, yedirilmiş tune
    juce::ignoreUnused (ati);
    SWX_BEKLE (! kava.module (state::modul::DIRT)->enabled);                                           // Kava temiz-sert
    // Kava yoğunluğu ORANDAN değil KADEMELİ SEVİYELEMEDEN gelir (Vocal Rider → CLA-2A → RComp kanıtı): Opto karakter + GR
    SWX_BEKLE_YAKIN (paramOf (*kava.module (state::modul::COMP_A), params::CaCharacter), 3.0f, 0.01f);
    SWX_BEKLE (recete (9).grA >= 4.0f);
    SWX_BEKLE (! ceza.module (state::modul::TUNE)->enabled
             || paramOf (*ceza.module (state::modul::TUNE), params::TuneStrength) <= 0.4f);            // Ceza: tune yok/çok az
    SWX_BEKLE (paramOf (*allame.module (state::modul::REVERB), params::RevMix, 0.0f) <= 0.10f);      // Allame kuru boom-bap
    SWX_BEKLE (recete (14).shelfDb <= -2.0f);                                                          // Sagopa koyu/buğulu üst
    SWX_BEKLE (! sagopa.modules.empty());
}

SWX_TEST (planner_m34_denetim_kurallari)
{
    auto planla = [] (AnalysisReport& r, std::vector<ProblemDetection>& dets, int stil, state::Role rol = state::Role::Lead)
    {
        PlannerInput in; in.report = &r; in.detections = &dets; in.role = rol; in.planId = 21; in.style = stil;
        return Planner::plan (in);
    };
    // PT-1: gürültülü kayıtta kapı eşiği −33'e KIRPILMAZ (floorPost + 10, tavan ref − 6)
    {
        auto r = tamRapor(); r.noiseFloorDb = -44.0f; r.snrDb = 17.0f; r.voicedLufs = -28.0f;   // trim +10 → floorPost −34
        std::vector<ProblemDetection> dets = { d (Problem::NoiseFloor, 1.0f, 0.9f, 0.7f, 0, 0, -44.0f) };
        const auto plan = planla (r, dets, 1);
        const float th = paramOf (*plan.module (state::modul::GATE), params::GateThresh);
        SWX_BEKLE_YAKIN (th, -24.0f, 0.6f);
    }
    // PT-2: NoiseFloor güveni çökmüş ama ölçülen SNR 29 dB → gürültülü dal (GateNoise), KİR −4, GLOW ×0.6
    {
        auto r = tamRapor(); r.snrDb = 29.0f; r.noiseFloorDb = -56.0f;
        std::vector<ProblemDetection> dets;
        const auto plan = planla (r, dets, 4);
        const auto* g = plan.module (state::modul::GATE);
        SWX_BEKLE (g->enabled && ! g->reasons.empty() && g->reasons[0].rule == ReasonRule::GateNoise);
        SWX_BEKLE_YAKIN (paramOf (*plan.module (state::modul::DIRT), params::DirtLevel), recete (4).dirtLevel - 4.0f, 0.05f);   // gürültülü kayıt −4
        const float glow = paramOf (*plan.module (state::modul::GLOW), params::GlowDepth);
        SWX_BEKLE_YAKIN (glow, recete (4).glowDepth * 0.6f, 0.011f);                                      // gürültülü kayıt ×0.6
        // taban bilinmiyorsa (duraklamasız kayıt) SNR yedeği DEVREYE GİRMEZ ve kapı taban değerine döner
        auto r2 = r; r2.noiseFloorKnown = false;
        const auto plan2 = planla (r2, dets, 4);
        SWX_BEKLE (plan2.module (state::modul::GATE)->reasons[0].rule == ReasonRule::GateBaseline);
        SWX_BEKLE_YAKIN (paramOf (*plan2.module (state::modul::GATE), params::GateThresh), -54.0f, 0.05f);
        // denetim KA-7: taban bilinmiyorken "gürültü yüksek" uyarısı özet satırına düşmez
        std::vector<ProblemDetection> nfd = { d (Problem::NoiseFloor, 1.0f, 1.0f, 0.0f, 0, 0, -56.0f) };
        const auto plan3 = planla (r2, nfd, 4);
        bool uyari = false; for (auto sl : plan3.summary) uyari = uyari || sl == SummaryLine::NoiseWarning;
        SWX_BEKLE (! uyari);
    }
    // PT-3: stil alçak rafı (Killoki 120 Hz −3) DÜŞMEZ — M3.5: TON EQ modülünde (kanıt bantlarıyla yarışmaz)
    {
        auto r = tamRapor(); r.tonalFrames = 60;
        std::vector<ProblemDetection> dets;
        const auto plan = planla (r, dets, 4);
        const auto& teq = *plan.module (state::modul::TONEQ);
        bool raf = false;
        for (int b = 0; b < params::TEQ_BANT_SAYISI; ++b)
            if (paramOf (teq, params::teqBant (b, 0), 0.0f) > 0.5f && paramOf (teq, params::teqBant (b, 1), -1.0f) == 1.0f
                && paramOf (teq, params::teqBant (b, 2), 0.0f) >= 100.0f && paramOf (teq, params::teqBant (b, 2), 0.0f) <= 130.0f
                && paramOf (teq, params::teqBant (b, 3), 0.0f) <= -2.5f) raf = true;
        SWX_BEKLE (raf);
    }
    // PT-8: 100 Hz gümleme kanıtı Killoki'nin gövde KESİMİNİ (reçete bodyHz, ±0.5 okt dışı) kapatmaz
    {
        auto r = tamRapor(); r.tonalFrames = 60;
        std::vector<ProblemDetection> dets = { d (Problem::Boom, 1.0f, 0.8f, 0.9f, 100.0f, 1.0f, 8.0f) };
        const auto plan = planla (r, dets, 4);
        const auto& teq = *plan.module (state::modul::TONEQ);
        const float hedefHz = recete (4).bodyHz;
        SWX_BEKLE (recete (4).bodyDb <= -0.5f);            // test önkoşulu: Killoki reçetesi gövde kesimi taşıyor
        bool kesim = false;
        for (int b = 0; b < params::TEQ_BANT_SAYISI; ++b)
            if (paramOf (teq, params::teqBant (b, 0), 0.0f) > 0.5f
                && std::fabs (std::log2 (std::max (1.0f, paramOf (teq, params::teqBant (b, 2), 1.0f)) / hedefHz)) < 0.25f
                && paramOf (teq, params::teqBant (b, 3), 0.0f) <= -0.4f) kesim = true;
        SWX_BEKLE (kesim);
    }
    // T1: patlama koruması vokal rolde açık — köşe 1.8·F0_p05, kanıtsız −6, kanıtla −15; master'da yok
    {
        auto r = tamRapor();
        std::vector<ProblemDetection> bos;
        const auto plan = planla (r, bos, 1);
        const auto& eq = *plan.module (state::modul::EQ);
        SWX_BEKLE (paramOf (eq, params::EqPlosOn) > 0.5f);
        const float hz = paramOf (eq, params::EqPlosHz);
        SWX_BEKLE (hz >= 160.0f && hz <= 171.0f);                                                  // 1.8 × 95 (aşağı kuantalı)
        SWX_BEKLE_YAKIN (paramOf (eq, params::EqPlosRange), -6.0f, 0.05f);
        std::vector<ProblemDetection> pd = { d (Problem::Plosive, 1.0f, 1.0f, 1.0f, 60.0f, 0.0f, 30.0f, 10) };
        const auto plan2 = planla (r, pd, 1);
        SWX_BEKLE (paramOf (*plan2.module (state::modul::EQ), params::EqPlosRange) <= -14.9f);
        const auto planM = planla (r, bos, 1, state::Role::Master);
        const auto* eqM = planM.module (state::modul::EQ);
        SWX_BEKLE (eqM == nullptr || paramOf (*eqM, params::EqPlosOn, 0.0f) < 0.5f);
    }
}
