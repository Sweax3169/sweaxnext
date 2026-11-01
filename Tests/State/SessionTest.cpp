// SWEAX NEXT — Tests/State/SessionTest.cpp
#include "Cati.h"
#include "State/Session.h"
#include "Core/ParamCache.h"

SWX_TEST (session_varsayilan_ve_alanlar)
{
    using namespace sweax::state;
    Session s;
    SWX_BEKLE (s.getRole() == Role::Lead);
    SWX_BEKLE (s.getUiMode() == UiMode::Simple);
    SWX_BEKLE (s.getScreen() == Screen::RoleSelect);
    SWX_BEKLE_EQ (s.getPlanId(), (juce::uint32) 0);
    s.setRole (Role::Backing); s.setLocked (modul::EQ, true); s.setBaseline (sweax::params::EqB1Gain, -3.5f);
    s.setAbGainDb (-2.25f); s.setPlanId (7);
    SWX_BEKLE (s.getRole() == Role::Backing);
    SWX_BEKLE (s.isLocked (modul::EQ) && ! s.isLocked (modul::DEESS));
    SWX_BEKLE_EQ (s.lockedModules().size(), 1);
    SWX_BEKLE_YAKIN (s.getBaseline (sweax::params::EqB1Gain, 0.0f), -3.5f, 1e-6f);
    SWX_BEKLE (s.hasBaseline (sweax::params::EqB1Gain) && ! s.hasBaseline (sweax::params::EqB2Gain));
    // snapshot / restore = yeniden analizin tek undo adımı
    auto snap = s.snapshot();
    s.setRole (Role::Adlib); s.setLocked (modul::EQ, false); s.setPlanId (9);
    s.restore (snap);
    SWX_BEKLE (s.getRole() == Role::Backing && s.isLocked (modul::EQ) && s.getPlanId() == 7);
}

SWX_TEST (state_kok_serilestirme_ve_migrasyon)
{
    using namespace sweax::state;
    juce::ScopedJuceInitialiser_GUI init;
    // PARAMS yerine küçük bir ağaç (APVTS köküyle aynı rolde)
    juce::ValueTree params ("SWEAXPARAMS");
    params.setProperty ("x", 1.5, nullptr);
    Session s; s.setRole (Role::Master); s.setLocked (modul::COMP_A, true); s.setBaseline (sweax::params::CaThresh, -20.0f);
    s.planNode (true).setProperty ("layout", "DeEssBeforeComp", nullptr);

    auto root = Migration::buildRoot (params, s.tree());
    SWX_BEKLE_EQ (Migration::versionOf (root), STATE_VERSION);
    // XML gidiş-dönüş (host state'i böyle taşınır)
    auto xml = root.createXml();
    SWX_BEKLE (xml != nullptr);
    auto geri = juce::ValueTree::fromXml (*xml);
    juce::ValueTree p2, s2; int v = -1;
    SWX_BEKLE (Migration::splitRoot (geri, p2, s2, v));
    SWX_BEKLE_EQ (v, STATE_VERSION);
    SWX_BEKLE_YAKIN ((double) p2.getProperty ("x"), 1.5, 1e-9);
    Session s3 (s2);
    SWX_BEKLE (s3.getRole() == Role::Master);
    SWX_BEKLE (s3.isLocked (modul::COMP_A));
    SWX_BEKLE_YAKIN (s3.getBaseline (sweax::params::CaThresh, 0.0f), -20.0f, 1e-6f);
    SWX_BEKLE (s3.planNode (false).getProperty ("layout").toString() == "DeEssBeforeComp");

    // v0 (sürümsüz, yalnız APVTS ağacı) → v1 migrasyonu: eski biçim yüklenebilir, SESSION varsayılanla gelir
    juce::ValueTree eski ("SWEAXPARAMS"); eski.setProperty ("y", 2, nullptr);
    juce::ValueTree p4, s4; int v4 = -1;
    SWX_BEKLE (Migration::splitRoot (eski, p4, s4, v4));
    SWX_BEKLE_EQ (v4, STATE_VERSION);
    SWX_BEKLE (p4.isValid() && (int) p4.getProperty ("y") == 2);
    Session s5 (s4); // geçersiz SESSION → varsayılanlar
    SWX_BEKLE (s5.getRole() == Role::Lead && s5.getPlanId() == 0);
    // ileri sürüm: dokunulmaz, ama okunur
    juce::ValueTree ileri ("SWEAX_NEXT"); ileri.setProperty ("stateVersion", 99, nullptr);
    ileri.addChild (juce::ValueTree ("SWEAXPARAMS"), -1, nullptr); ileri.addChild (juce::ValueTree ("SESSION"), -1, nullptr);
    juce::ValueTree p6, s6; int v6 = -1;
    SWX_BEKLE (Migration::splitRoot (ileri, p6, s6, v6));
    SWX_BEKLE_EQ (v6, 99);
}
