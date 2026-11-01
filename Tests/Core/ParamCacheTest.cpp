// SWEAX NEXT — Tests/Core/ParamCacheTest.cpp
#include "Cati.h"
#include "Core/ParamCache.h"
#include "Core/RtGuard.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
    // APVTS için en küçük AudioProcessor
    struct KukProcessor : juce::AudioProcessor
    {
        KukProcessor() : apvts (*this, nullptr, "P", sweax::ParamCache::createLayout()) {}
        const juce::String getName() const override { return "Kuk"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
        juce::AudioProcessorValueTreeState apvts;
    };
}

SWX_TEST (paramcache_duzen_ve_onbellek)
{
    using namespace sweax::params;
    juce::ScopedJuceInitialiser_GUI init;
    KukProcessor p;
    sweax::ParamCache c; c.attach (p.apvts);
    SWX_BEKLE (c.isAttached());
    // her parametre bulundu ve varsayılanı tabloyla aynı
    // APVTS ham değeri normalize→denormalize gidiş-dönüşünden geçer (skew'lı Hz aralığında ~1e-3 Hz
    // sapma normaldir) → tolerans GÖRELİ: 1e-4·|def| (taban 1e-3). İlk sapan parametre raporlanır.
    int eksik = 0, farkli = 0; std::string ilkFark;
    for (size_t i = 0; i < TABLO.size(); ++i)
    {
        if (c.param ((Id) i) == nullptr) { ++eksik; continue; }
        const float tol = std::max (1e-3f, 1e-4f * std::fabs (TABLO[i].def));
        if (std::fabs (c.raw ((Id) i) - TABLO[i].def) > tol)
        {
            if (ilkFark.empty())
                ilkFark = std::string (TABLO[i].id) + " raw=" + std::to_string (c.raw ((Id) i)) + " def=" + std::to_string (TABLO[i].def);
            ++farkli;
        }
    }
    SWX_BEKLE_EQ (eksik, 0);
    SWX_BEKLE (farkli == 0 && ilkFark.empty());
    if (! ilkFark.empty()) _d.sonHata = "varsayilan sapmasi: " + ilkFark;
    SWX_BEKLE_EQ (c.choice (QualityMode), 1);      // Yuksek
    SWX_BEKLE (! c.flag (EqOn));
    // ham ↔ normalize gidiş dönüş (log skew'lı Hz)
    const float n = c.toNormalized (EqB1Hz, 1000.0f);
    SWX_BEKLE_YAKIN (c.fromNormalized (EqB1Hz, n), 1000.0f, 0.5f);
    // layout otomasyona kapalı
    SWX_BEKLE (! c.param (Layout)->isAutomatable());
    SWX_BEKLE (c.param (EqB1Gain)->isAutomatable());
}

SWX_TEST (paramcache_makro_birlesimi)
{
    using namespace sweax::params;
    juce::ScopedJuceInitialiser_GUI init;
    KukProcessor p;
    sweax::ParamCache c; c.attach (p.apvts);
    auto ayarla = [&] (Id id, float ham) { c.param (id)->setValueNotifyingHost (c.toNormalized (id, ham)); };

    // AI: bant 1 kazancı -4 dB, TEMİZLİK makrosu 0.62 (aiMakro = 0.62)
    ayarla (EqB1Gain, -4.0f);
    ayarla (MacroTemizlik, 0.62f);
    c.setMacroBaseline (Makro::Temizlik, 0.62f);
    SWX_BEKLE_YAKIN (c.effective (EqB1Gain), -4.0f, 0.02f);          // makro AI değerinde → ölçek 1
    ayarla (MacroTemizlik, 0.93f);                                     // kullanıcı %150
    SWX_BEKLE_YAKIN (c.effective (EqB1Gain), -6.0f, 0.05f);
    ayarla (MacroTemizlik, 0.0f);                                      // düzeltme kapalı
    SWX_BEKLE_YAKIN (c.effective (EqB1Gain), 0.0f, 0.02f);
    // AI mud bulmadıysa (kazanç nötr) makro hiçbir şey icat etmez
    ayarla (EqB2Gain, 0.0f);
    ayarla (MacroTemizlik, 1.0f);
    SWX_BEKLE_YAKIN (c.effective (EqB2Gain), 0.0f, 1e-6f);
    // klemp: -18 dB tabanı aşamaz
    ayarla (EqB1Gain, -12.0f);
    SWX_BEKLE_YAKIN (c.effective (EqB1Gain), -18.0f, 0.05f);
    // oran nötrü 1: 2:1 × 1.5 → 2.5:1
    ayarla (CaRatio, 2.0f); ayarla (MacroDenge, 0.75f); c.setMacroBaseline (Makro::Denge, 0.5f);
    SWX_BEKLE_YAKIN (c.effective (CaRatio), 2.5f, 0.02f);
    // makrosuz parametre değişmez
    ayarla (EqB1Q, 3.0f);
    SWX_BEKLE_YAKIN (c.effective (EqB1Q), 3.0f, 0.01f);
}

SWX_TEST (rtguard_tahsis_sayaci)
{
    sweax::rt::resetViolations();
    {
        sweax::rt::Scope kapsam;
        SWX_BEKLE (sweax::rt::active());
        volatile int* p = new int (1); // kasıtlı ihlal
        delete p;
    }
    SWX_BEKLE (! sweax::rt::active());
#if defined (SWEAX_RT_GUARD) && SWEAX_RT_GUARD
    SWX_BEKLE (sweax::rt::violations() >= 1);
#else
    SWX_BEKLE_EQ (sweax::rt::violations(), (uint64_t) 0);
#endif
    sweax::rt::resetViolations();
    // kapsam dışı tahsis ihlal değil
    { auto q = std::make_unique<int> (2); (void) q; }
    SWX_BEKLE_EQ (sweax::rt::violations(), (uint64_t) 0);
}
