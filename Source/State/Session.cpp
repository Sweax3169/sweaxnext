// SWEAX NEXT — State/Session.cpp
#include "Session.h"

namespace sweax::state
{
    namespace
    {
        const juce::Identifier ID_ROOT ("SWEAX_NEXT");
        const juce::Identifier ID_VERSION ("stateVersion");
        const juce::Identifier ID_PARAMS ("PARAMS");
        const juce::Identifier ID_LOCKS ("LOCKS");
        const juce::Identifier ID_BASELINE ("BASELINE");
        const juce::Identifier ID_PLAN ("PLAN");
        const juce::Identifier ID_ANALYSIS ("ANALYSIS");
        const juce::Identifier P_ROLE ("role"), P_UI ("uiMode"), P_SCREEN ("screen"), P_STYLE ("style"),
            P_TUT ("tutorialSeen"), P_AB ("abGainDb"), P_STOP ("stopPlayhead"), P_PLANID ("planId");

        // ParamIds kimlikleri "swx.eq.b1.gain" → ValueTree özellik adı olarak nokta geçersiz → alt çizgi
        juce::Identifier propFor (params::Id id)
        {
            return juce::Identifier (juce::String (params::TABLO[(size_t) id].id.data(),
                                                   params::TABLO[(size_t) id].id.size()).replaceCharacter ('.', '_'));
        }
    }

    const juce::Identifier Session::ID_SESSION ("SESSION");

    Session::Session() : t (ID_SESSION) { ensureDefaults(); }
    Session::Session (juce::ValueTree existing) : t (existing.isValid() && existing.hasType (ID_SESSION) ? existing : juce::ValueTree (ID_SESSION))
    { ensureDefaults(); }

    void Session::ensureDefaults()
    {
        if (! t.hasProperty (P_ROLE))   t.setProperty (P_ROLE, (int) Role::Lead, nullptr);
        if (! t.hasProperty (P_UI))     t.setProperty (P_UI, (int) UiMode::Simple, nullptr);
        if (! t.hasProperty (P_SCREEN)) t.setProperty (P_SCREEN, (int) Screen::RoleSelect, nullptr);
        if (! t.hasProperty (P_STYLE))  t.setProperty (P_STYLE, "Clean", nullptr);
        if (! t.hasProperty (P_TUT))    t.setProperty (P_TUT, false, nullptr);
        if (! t.hasProperty (P_AB))     t.setProperty (P_AB, 0.0f, nullptr);
        if (! t.hasProperty (P_STOP))   t.setProperty (P_STOP, (juce::int64) 0, nullptr);
        if (! t.hasProperty (P_PLANID)) t.setProperty (P_PLANID, 0, nullptr);
        child (ID_LOCKS, true);
        child (ID_BASELINE, true);
    }

    juce::ValueTree Session::child (const juce::Identifier& id, bool create)
    {
        auto c = t.getChildWithName (id);
        if (! c.isValid() && create) { c = juce::ValueTree (id); t.addChild (c, -1, nullptr); }
        return c;
    }

    Role Session::getRole() const { return (Role) (int) t.getProperty (P_ROLE, (int) Role::Lead); }
    void Session::setRole (Role r, juce::UndoManager* um) { t.setProperty (P_ROLE, (int) r, um); }
    UiMode Session::getUiMode() const { return (UiMode) (int) t.getProperty (P_UI, 0); }
    void Session::setUiMode (UiMode m, juce::UndoManager* um) { t.setProperty (P_UI, (int) m, um); }
    Screen Session::getScreen() const { return (Screen) (int) t.getProperty (P_SCREEN, 0); }
    void Session::setScreen (Screen s, juce::UndoManager* um) { t.setProperty (P_SCREEN, (int) s, um); }
    juce::String Session::getStyle() const { return t.getProperty (P_STYLE, "Clean").toString(); }
    void Session::setStyle (const juce::String& s, juce::UndoManager* um) { t.setProperty (P_STYLE, s, um); }
    int Session::getStyleIndex() const
    {
        const auto s = getStyle();
        if (s == "Natural") return 0;
        if (s == "Trap") return 2;
        if (s == "Aggressive") return 3;
        if (s == "Killoki") return 4;      // sanatçı vokal chain profilleri (M3)
        if (s == "Reckol") return 5;
        if (s == "Ati242") return 6;
        if (s == "Uzi") return 7;
        if (s == "Motive") return 8;
        if (s == "Kava") return 9;         // M3.5 sanatçı reçeteleri
        if (s == "Ezhel") return 10;
        if (s == "Ceza") return 11;
        if (s == "Allame") return 12;
        if (s == "Sehinsah") return 13;
        if (s == "Sagopa") return 14;
        if (s == "BenFero") return 15;
        return 1;   // Clean (varsayılan)
    }
    bool Session::getTutorialSeen() const { return (bool) t.getProperty (P_TUT, false); }
    void Session::setTutorialSeen (bool v) { t.setProperty (P_TUT, v, nullptr); }
    float Session::getAbGainDb() const { return (float) (double) t.getProperty (P_AB, 0.0); }
    void Session::setAbGainDb (float v, juce::UndoManager* um) { t.setProperty (P_AB, v, um); }
    juce::int64 Session::getStopPlayhead() const { return (juce::int64) t.getProperty (P_STOP, (juce::int64) 0); }
    void Session::setStopPlayhead (juce::int64 v) { t.setProperty (P_STOP, v, nullptr); }
    juce::uint32 Session::getPlanId() const { return (juce::uint32) (int) t.getProperty (P_PLANID, 0); }
    void Session::setPlanId (juce::uint32 v, juce::UndoManager* um) { t.setProperty (P_PLANID, (int) v, um); }

    bool Session::isLocked (const juce::String& modul) const
    {
        auto l = t.getChildWithName (ID_LOCKS);
        return l.isValid() && (bool) l.getProperty (juce::Identifier (modul), false);
    }
    void Session::setLocked (const juce::String& modul, bool locked, juce::UndoManager* um)
    {
        child (ID_LOCKS, true).setProperty (juce::Identifier (modul), locked, um);
    }
    juce::StringArray Session::lockedModules() const
    {
        juce::StringArray out;
        auto l = t.getChildWithName (ID_LOCKS);
        if (! l.isValid()) return out;
        for (int i = 0; i < l.getNumProperties(); ++i)
        {
            const auto n = l.getPropertyName (i);
            if ((bool) l.getProperty (n, false)) out.add (n.toString());
        }
        return out;
    }

    void Session::setBaseline (params::Id id, float ham, juce::UndoManager* um)
    {
        child (ID_BASELINE, true).setProperty (propFor (id), ham, um);
    }
    float Session::getBaseline (params::Id id, float varsayilan) const
    {
        auto b = t.getChildWithName (ID_BASELINE);
        return b.isValid() ? (float) (double) b.getProperty (propFor (id), varsayilan) : varsayilan;
    }
    bool Session::hasBaseline (params::Id id) const
    {
        auto b = t.getChildWithName (ID_BASELINE);
        return b.isValid() && b.hasProperty (propFor (id));
    }

    juce::ValueTree Session::planNode (bool create) { return child (ID_PLAN, create); }
    juce::ValueTree Session::analysisNode (bool create) { return child (ID_ANALYSIS, create); }

    void Session::restore (const juce::ValueTree& snap)
    {
        if (! snap.isValid() || ! snap.hasType (ID_SESSION)) return;
        t.copyPropertiesAndChildrenFrom (snap, nullptr);
        ensureDefaults();
    }

    // ---------------- Migration ----------------
    namespace Migration
    {
        juce::ValueTree buildRoot (const juce::ValueTree& params, const juce::ValueTree& session)
        {
            juce::ValueTree root (ID_ROOT);
            root.setProperty (ID_VERSION, STATE_VERSION, nullptr);
            root.addChild (params.createCopy(), -1, nullptr);
            root.addChild (session.createCopy(), -1, nullptr);
            return root;
        }

        int versionOf (const juce::ValueTree& root)
        {
            return root.isValid() ? (int) root.getProperty (ID_VERSION, 0) : -1;
        }

        // v0 → v1: sürümsüz kök (ilk geliştirme derlemeleri). Alanlar zaten uyumlu; yalnız damga.
        static juce::ValueTree v0_to_v1 (juce::ValueTree root)
        {
            root.setProperty (ID_VERSION, 1, nullptr);
            return root;
        }

        juce::ValueTree migrate (juce::ValueTree root)
        {
            if (! root.isValid()) return root;
            int v = versionOf (root);
            if (v < 0) return root;
            while (v < STATE_VERSION)
            {
                switch (v)
                {
                    case 0: root = v0_to_v1 (root); break;
                    default: return root; // bilinmeyen adım: olduğu gibi bırak (ileri sürüm)
                }
                v = versionOf (root);
            }
            return root;
        }

        bool splitRoot (juce::ValueTree root, juce::ValueTree& paramsOut, juce::ValueTree& sessionOut, int& versionOut)
        {
            paramsOut = {}; sessionOut = {}; versionOut = -1;
            if (! root.isValid()) return false;
            // Eski tek-APVTS biçimi (kök doğrudan PARAMS ağacı) → v0 sarmalayıcı
            if (! root.hasType (ID_ROOT))
            {
                juce::ValueTree wrap (ID_ROOT);
                wrap.setProperty (ID_VERSION, 0, nullptr);
                wrap.addChild (root.createCopy(), -1, nullptr);
                root = wrap;
            }
            root = migrate (root);
            versionOut = versionOf (root);
            for (int i = 0; i < root.getNumChildren(); ++i)
            {
                auto c = root.getChild (i);
                if (c.hasType (Session::ID_SESSION)) sessionOut = c;
                else if (! paramsOut.isValid()) paramsOut = c; // PARAMS: APVTS kök tipi ne olursa olsun ilk sessiz-olmayan çocuk
            }
            return paramsOut.isValid();
        }
    }
}
