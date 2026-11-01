#pragma once
// SWEAX NEXT — State/Session.h
// SESSION: message thread'in sahip olduğu, APVTS ağacının ÇOCUĞU OLMAYAN ValueTree (mimari §6).
// Otomasyonsuz oturum verisi: rol, UI modu, stil, kilitler, AI taban değerleri, plan düğümü,
// analiz özeti, A/B eşitleme kazancı, durma noktası. Ham ses ASLA buraya girmez.
//
// Kök serileştirme: <SWEAX_NEXT stateVersion="N"> <PARAMS/> <SESSION/> </SWEAX_NEXT>
// stateVersion arttıkça Migration::migrate() eski kökleri yükseltir (test: v0 → vN).
#include "../Core/ParamIds.h"
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace sweax::state
{
    constexpr int STATE_VERSION = 1;

    enum class Role : int { Lead = 0, Backing = 1, Adlib = 2, Master = 3 };
    enum class UiMode : int { Simple = 0, Advanced = 1 };
    enum class Screen : int { RoleSelect = 0, Ready = 1, Listening = 2, MixReady = 3 };

    // modül anahtarları (kilit ve plan düğümü bunlarla adreslenir)
    namespace modul
    {
        inline constexpr const char* EQ = "eq";
        inline constexpr const char* LEVELER = "lev";
        inline constexpr const char* COMP_A = "compa";
        inline constexpr const char* COMP_B = "compb";
        inline constexpr const char* DEESS = "ds";
        inline constexpr const char* OUTPUT = "out";
        inline constexpr const char* GATE = "gate";
        inline constexpr const char* SAT = "sat";
        inline constexpr const char* DOUBLER = "dbl";
        inline constexpr const char* DELAY = "dly";
        inline constexpr const char* REVERB = "rev";
        inline constexpr const char* WIDTH = "wid";
        inline constexpr const char* TUNE = "tune";
        inline constexpr const char* AIR = "air";
        inline constexpr const char* GLOW = "glow";
        inline constexpr const char* DIRT = "dirt";
        inline constexpr const char* LAYER = "layer";
        inline constexpr const char* YUMUSAT = "soo";     // M3.5
        inline constexpr const char* FORMANT = "frm";     // M3.5
        inline constexpr const char* TONEQ = "teq";       // M3.5
        // sıra = UI kart sırası (varsayılan zincir akışına göre gruplu): gate önce, çıkış sonda
        inline constexpr std::array<const char*, 20> HEPSI { GATE, TUNE, FORMANT, EQ, TONEQ, LEVELER, COMP_A, DEESS, YUMUSAT, COMP_B, GLOW, SAT, DIRT, AIR, LAYER, DOUBLER, DELAY, REVERB, WIDTH, OUTPUT };
    }

    class Session
    {
    public:
        Session();                                  // varsayılan alanlarla yeni oturum
        explicit Session (juce::ValueTree existing); // yüklenen ağaç (migrasyondan geçmiş)

        juce::ValueTree& tree() noexcept { return t; }
        const juce::ValueTree& tree() const noexcept { return t; }

        // ---- basit alanlar ----
        Role   getRole() const;            void setRole (Role, juce::UndoManager* = nullptr);
        UiMode getUiMode() const;          void setUiMode (UiMode, juce::UndoManager* = nullptr);
        Screen getScreen() const;          void setScreen (Screen, juce::UndoManager* = nullptr);
        juce::String getStyle() const;     void setStyle (const juce::String&, juce::UndoManager* = nullptr);
        int getStyleIndex() const;         // 0 Doğal, 1 Temiz, 2 Trap, 3 Agresif; SANATÇI: 4 Killoki, 5 Reçkol, 6 Ati242, 7 Uzi, 8 Motive,
                                           // 9 Kava, 10 Ezhel, 11 Ceza, 12 Allame, 13 Şehinşah, 14 Sagopa Kajmer, 15 Ben Fero (M3.5)
        bool   getTutorialSeen() const;    void setTutorialSeen (bool);
        float  getAbGainDb() const;        void setAbGainDb (float, juce::UndoManager* = nullptr);
        juce::int64 getStopPlayhead() const; void setStopPlayhead (juce::int64);
        juce::uint32 getPlanId() const;    void setPlanId (juce::uint32, juce::UndoManager* = nullptr);

        // ---- kilitler ----
        bool isLocked (const juce::String& modul) const;
        void setLocked (const juce::String& modul, bool locked, juce::UndoManager* = nullptr);
        juce::StringArray lockedModules() const;

        // ---- AI taban değerleri (ham birim) ----
        void   setBaseline (params::Id, float ham, juce::UndoManager* = nullptr);
        float  getBaseline (params::Id, float varsayilan) const;
        bool   hasBaseline (params::Id) const;

        // ---- plan / analiz düğümleri (opak; yazan taraf yapısını bilir) ----
        juce::ValueTree planNode (bool create);      // "PLAN"
        juce::ValueTree analysisNode (bool create);  // "ANALYSIS"

        // yeniden analiz = tek undo adımı için tam anlık görüntü
        juce::ValueTree snapshot() const { return t.createCopy(); }
        void restore (const juce::ValueTree& snap);

        static const juce::Identifier ID_SESSION;

    private:
        juce::ValueTree t;
        juce::ValueTree child (const juce::Identifier& id, bool create);
        void ensureDefaults();
    };

    // ---- kök serileştirme ve migrasyon ----
    namespace Migration
    {
        juce::ValueTree buildRoot (const juce::ValueTree& params, const juce::ValueTree& session);
        // Kökü ayırır; eski sürümü yükseltir. Başarısızsa false (params/session boş kalır).
        bool splitRoot (juce::ValueTree root, juce::ValueTree& paramsOut, juce::ValueTree& sessionOut, int& versionOut);
        juce::ValueTree migrate (juce::ValueTree root); // stateVersion < STATE_VERSION ise yükselt
        int  versionOf (const juce::ValueTree& root);
    }
}
