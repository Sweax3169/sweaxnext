// SWEAX NEXT — Decision/PlanPublisher.cpp
#include "PlanPublisher.h"
#include <algorithm>

namespace sweax::decision
{

namespace
{
    using params::Id;
}

namespace
{
    // APVTS parametre anlık görüntüsü: undo'da eski ham değerleri geri yazar (gesture'sız — programatik geri alma)
    class ParamSnapshotAction : public juce::UndoableAction
    {
    public:
        ParamSnapshotAction (ParamCache& c, std::vector<std::pair<Id, float>> newVals) : cache (c), news (std::move (newVals))
        {
            olds.reserve (news.size());
            for (const auto& [id, v] : news) { juce::ignoreUnused (v); olds.push_back ({ id, cache.raw (id) }); }
        }
        bool perform() override { yaz (news, true); return true; }
        bool undo() override { yaz (olds, false); return true; }
        int getSizeInUnits() override { return (int) news.size(); }
    private:
        void yaz (const std::vector<std::pair<Id, float>>& vals, bool gesture)
        {
            for (const auto& [id, v] : vals)
            {
                auto* p = cache.param (id);
                if (p == nullptr) continue;
                const float norm = cache.toNormalized (id, v);
                if (gesture) p->beginChangeGesture();
                p->setValueNotifyingHost (norm);
                if (gesture) p->endChangeGesture();
            }
        }
        ParamCache& cache;
        std::vector<std::pair<Id, float>> news, olds;
    };

    bool isLockedPlan (const ModulePlan& m)
    {
        for (const auto& r : m.reasons) if (r.rule == ReasonRule::Locked) return true;
        return false;
    }
}

juce::ValueTree PlanPublisher::moduleNode (state::Session& sess, const char* moduleId)
{
    auto pn = sess.planNode (false);
    if (! pn.isValid()) return {};
    for (int i = 0; i < pn.getNumChildren(); ++i)
        if (pn.getChild (i).getProperty ("id").toString() == juce::String (moduleId)) return pn.getChild (i);
    return {};
}

int PlanPublisher::apply (const MixPlan& plan, ParamCache& cache, state::Session& sess, juce::UndoManager& undo)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    undo.beginNewTransaction ("Analiz plani");

    // ---- 1) parametre listesi (kilitli modüller atlanır; EQ'da kullanılmayan bantlar kapatılır) ----
    std::vector<std::pair<Id, float>> vals;
    vals.push_back ({ params::InputTrim, plan.inputTrimDb });
    // makrolar AI orta noktasına (0.5): plan değerleri = %100 AI; kullanıcı makroyu oynatınca effective ölçeklenir (§2.2)
    for (auto m : { params::Makro::Temizlik, params::Makro::Denge, params::Makro::Onde, params::Makro::Parlaklik,
                    params::Makro::Alan, params::Makro::Genislik, params::Makro::Cikis })
        vals.push_back ({ ParamCache::macroParam (m), 0.5f });
    // zincir sırası (M3.5): her plan kendi sırasını yazar
    {
        const Sira s = siraGecerli (plan.sira) ? plan.sira : varsayilanSira();
        const auto k = siraKonumlari (s);
        for (int i = 0; i < EK_SAYISI; ++i) vals.push_back ({ EK_KONUM_PARAM[(size_t) i], k[(size_t) i] });
    }
    for (const auto& m : plan.modules)
    {
        if (isLockedPlan (m)) continue;
        const std::string_view id (m.moduleId);
        if (! m.enabled)
        {
            if (id == state::modul::EQ) vals.push_back ({ params::EqOn, 0.0f });
            else if (id == state::modul::LEVELER) vals.push_back ({ params::LevOn, 0.0f });
            else if (id == state::modul::COMP_A) vals.push_back ({ params::CaOn, 0.0f });
            else if (id == state::modul::COMP_B) vals.push_back ({ params::CbOn, 0.0f });
            else if (id == state::modul::DEESS) vals.push_back ({ params::DsOn, 0.0f });
            else if (id == state::modul::GATE) vals.push_back ({ params::GateOn, 0.0f });
            else if (id == state::modul::SAT) vals.push_back ({ params::SatOn, 0.0f });
            else if (id == state::modul::DOUBLER) vals.push_back ({ params::DblOn, 0.0f });
            else if (id == state::modul::DELAY) vals.push_back ({ params::DlyOn, 0.0f });
            else if (id == state::modul::REVERB) vals.push_back ({ params::RevOn, 0.0f });
            else if (id == state::modul::WIDTH) vals.push_back ({ params::WidOn, 0.0f });
            else if (id == state::modul::TUNE) vals.push_back ({ params::TuneOn, 0.0f });
            else if (id == state::modul::AIR) vals.push_back ({ params::AirOn, 0.0f });
            else if (id == state::modul::GLOW) vals.push_back ({ params::GlowOn, 0.0f });
            else if (id == state::modul::DIRT) vals.push_back ({ params::DirtOn, 0.0f });
            else if (id == state::modul::LAYER) vals.push_back ({ params::LayerOn, 0.0f });
            else if (id == state::modul::YUMUSAT) vals.push_back ({ params::SooOn, 0.0f });
            else if (id == state::modul::FORMANT) vals.push_back ({ params::FrmOn, 0.0f });
            else if (id == state::modul::TONEQ) vals.push_back ({ params::TeqOn, 0.0f });
            continue;                                       // kapalı modülün diğer parametrelerine DOKUNMA
        }
        bool bandOn[params::EQ_BANT_SAYISI] = {};
        bool teqOn[params::TEQ_BANT_SAYISI] = {};
        bool hpfOn = false, plosOn = false;
        for (const auto& p : m.params)
        {
            vals.push_back ({ p.id, p.raw });
            for (int b = 0; b < params::EQ_BANT_SAYISI; ++b) if (p.id == params::eqBant (b, 0)) bandOn[b] = true;
            for (int b = 0; b < params::TEQ_BANT_SAYISI; ++b) if (p.id == params::teqBant (b, 0)) teqOn[b] = true;
            if (p.id == params::EqHpfOn) hpfOn = true;
            if (p.id == params::EqPlosOn) plosOn = true;
        }
        if (id == state::modul::EQ)
        {
            for (int b = 0; b < params::EQ_BANT_SAYISI; ++b) if (! bandOn[b]) vals.push_back ({ params::eqBant (b, 0), 0.0f });
            if (! hpfOn) vals.push_back ({ params::EqHpfOn, 0.0f });
            if (! plosOn) vals.push_back ({ params::EqPlosOn, 0.0f });
        }
        if (id == state::modul::TONEQ)
            for (int b = 0; b < params::TEQ_BANT_SAYISI; ++b) if (! teqOn[b]) vals.push_back ({ params::teqBant (b, 0), 0.0f });
    }

    // ---- 2) uygula (undo yığınına tek eylem) ----
    undo.perform (new ParamSnapshotAction (cache, vals));

    // ---- 3) SESSION (aynı işlem içinde; tree değişimleri UndoManager'a bağlanır) ----
    for (const auto& [id, v] : vals) { sess.setBaseline (id, v, &undo); cache.setBaseline (id, v); }
    for (auto m : { params::Makro::Temizlik, params::Makro::Denge, params::Makro::Onde, params::Makro::Parlaklik,
                    params::Makro::Alan, params::Makro::Genislik, params::Makro::Cikis })
        cache.setMacroBaseline (m, 0.5f);
    sess.setPlanId ((juce::uint32) plan.planId, &undo);
    sess.setAbGainDb (plan.abGainDb, &undo);
    sess.setScreen (state::Screen::MixReady, &undo);

    auto pn = sess.planNode (true);
    pn.removeAllChildren (&undo);
    pn.setProperty ("planId", (int) plan.planId, &undo);
    pn.setProperty ("conf", plan.overallConfidence, &undo);
    for (int i = 0; i < 4; ++i) pn.setProperty (juce::Identifier ("sum" + juce::String (i)), (int) plan.summary[(size_t) i], &undo);
    for (const auto& m : plan.modules)
    {
        juce::ValueTree mn ("MOD");
        mn.setProperty ("id", juce::String (m.moduleId), nullptr);
        mn.setProperty ("on", m.enabled, nullptr);
        for (const auto& rs : m.reasons)
        {
            juce::ValueTree rn ("R");
            rn.setProperty ("rule", (int) rs.rule, nullptr);
            rn.setProperty ("det", (int) rs.det, nullptr);
            for (int k = 0; k < 4; ++k) rn.setProperty (juce::Identifier ("v" + juce::String (k)), rs.vals[(size_t) k], nullptr);
            rn.setProperty ("hedef", (int) rs.target, nullptr);
            mn.addChild (rn, -1, nullptr);
        }
        pn.addChild (mn, -1, &undo);
    }
    return (int) vals.size();
}

void PlanPublisher::forceEnableNeutral (const char* moduleId, ParamCache& cache, state::Session& sess, juce::UndoManager& undo)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    using namespace params;
    undo.beginNewTransaction ("Yine de ac");
    std::vector<std::pair<Id, float>> vals;
    const std::string_view id (moduleId);
    if (id == state::modul::EQ)
    {
        // denetim düzeltmesi: yalnız EqOn=1 bayat bantları geri açıyordu — nötr = tüm bantlar + HPF kapalı
        vals = { { EqOn, 1.0f }, { EqHpfOn, 0.0f }, { EqPlosOn, 0.0f } };
        for (int b = 0; b < EQ_BANT_SAYISI; ++b) vals.push_back ({ eqBant (b, 0), 0.0f });
    }
    else if (id == state::modul::LEVELER) vals = { { LevOn, 1.0f }, { LevMaxUp, 0.0f }, { LevMaxDown, 0.0f } };
    else if (id == state::modul::COMP_A) vals = { { CaOn, 1.0f }, { CaRatio, 1.0f }, { CaMakeup, 0.0f }, { CaMix, 1.0f } };
    else if (id == state::modul::COMP_B) vals = { { CbOn, 1.0f }, { CbRatio, 1.0f }, { CbMakeup, 0.0f }, { CbMix, 1.0f } };
    else if (id == state::modul::DEESS) vals = { { DsOn, 1.0f }, { DsRange, 0.0f }, { Ds2On, 0.0f } };
    else if (id == state::modul::GATE) vals = { { GateOn, 1.0f }, { GateRange, 0.0f } };
    else if (id == state::modul::SAT) vals = { { SatOn, 1.0f }, { SatMix, 0.0f } };     // mix 0 = birebir (drive 0 bile tanh büker — ölçüldü)
    else if (id == state::modul::DOUBLER) vals = { { DblOn, 1.0f }, { DblMix, 0.0f } };
    else if (id == state::modul::DELAY) vals = { { DlyOn, 1.0f }, { DlyMix, 0.0f } };
    else if (id == state::modul::REVERB) vals = { { RevOn, 1.0f }, { RevMix, 0.0f } };
    else if (id == state::modul::WIDTH) vals = { { WidOn, 1.0f }, { WidAmount, 1.0f } };
    else if (id == state::modul::TUNE) vals = { { TuneOn, 1.0f }, { TuneStrength, 0.0f } };
    else if (id == state::modul::AIR) vals = { { AirOn, 1.0f }, { AirMid, 0.0f }, { AirHigh, 0.0f } };
    else if (id == state::modul::GLOW) vals = { { GlowOn, 1.0f }, { GlowDepth, 0.0f } };
    else if (id == state::modul::DIRT) vals = { { DirtOn, 1.0f }, { DirtLevel, -36.0f } };
    else if (id == state::modul::LAYER) vals = { { LayerOn, 1.0f }, { LayerA, 0.0f }, { LayerB, 0.0f } };
    else if (id == state::modul::YUMUSAT) vals = { { SooOn, 1.0f }, { SooDepth, 0.0f } };
    else if (id == state::modul::FORMANT) vals = { { FrmOn, 1.0f }, { FrmShift, 0.0f }, { FrmMix, 1.0f } };
    else if (id == state::modul::TONEQ)
    {
        vals = { { TeqOn, 1.0f } };
        for (int b = 0; b < TEQ_BANT_SAYISI; ++b) vals.push_back ({ teqBant (b, 0), 0.0f });
    }
    else return;                                                                        // OUTPUT zaten hep açık
    undo.perform (new ParamSnapshotAction (cache, vals));
    for (const auto& [pid, v] : vals) { sess.setBaseline (pid, v, &undo); cache.setBaseline (pid, v); }
    sess.setLocked (moduleId, true, &undo);                                             // otomatik kilit (§8)
    auto pn = sess.planNode (true);
    auto mn = moduleNode (sess, moduleId);
    if (! mn.isValid()) { mn = juce::ValueTree ("MOD"); mn.setProperty ("id", juce::String (moduleId), nullptr); pn.addChild (mn, -1, &undo); }
    mn.setProperty ("on", true, &undo);
    juce::ValueTree rn ("R");
    rn.setProperty ("rule", (int) ReasonRule::UserRequest, nullptr);
    rn.setProperty ("det", (int) Problem::COUNT, nullptr);
    mn.addChild (rn, -1, &undo);
}

void PlanPublisher::returnMacroToAi (params::Makro m, ParamCache& cache, state::Session& sess, juce::UndoManager& undo)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    juce::ignoreUnused (sess);
    undo.beginNewTransaction ("AI degerine don");
    undo.perform (new ParamSnapshotAction (cache, { { ParamCache::macroParam (m), cache.getMacroBaseline (m) } }));
}

void PlanPublisher::returnAllToAi (ParamCache& cache, state::Session& sess, juce::UndoManager& undo)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());
    undo.beginNewTransaction ("Tum AI degerlerine don");
    std::vector<std::pair<params::Id, float>> vals;
    for (size_t i = 0; i < params::TABLO.size(); ++i)
        if (sess.hasBaseline ((params::Id) i))
            vals.push_back ({ (params::Id) i, sess.getBaseline ((params::Id) i, params::TABLO[i].def) });
    if (! vals.empty()) undo.perform (new ParamSnapshotAction (cache, vals));
}

} // namespace sweax::decision
