// SWEAX NEXT — Tests/Integration/ProcessorCaptureTest.cpp
// Listening'de processBlock ham girişi analiz FIFO'suna verir (RT kapsamında tahsissiz); dinleme kapalıyken hiçbir şey
// yakalanmaz; startListening yakalamayı temizler; stopListening Ready'ye döner; state yüklemesi dinlemeyi kapatır.
#include "Cati.h"
#include "PluginProcessor.h"
#include "Core/RtGuard.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace
{
    void bloklar (sweax::SweaxProcessor& p, int adet, int blok = 256)
    {
        juce::AudioBuffer<float> b (2, blok); juce::MidiBuffer m;
        for (int k = 0; k < adet; ++k)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < blok; ++i)
                    b.setSample (c, i, 0.25f * std::sin (0.05f * (float) (k * blok + i)));
            p.processBlock (b, m);
        }
    }
}

SWX_TEST (processor_dinleme_yakalar_ve_rt_temiz)
{
    using namespace sweax;
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 256);
    p.prepareToPlay (48000.0, 256);
    auto& eng = p.analysisEngine();
    eng.stopThread();                                       // testte eşzamanlı: worker yok, processPendingSync
    p.session().setRole (state::Role::Lead); p.session().setScreen (state::Screen::Ready);
    // dinleme kapalı → yakalama yok
    bloklar (p, 40); eng.processPendingSync();
    SWX_BEKLE_YAKIN (eng.progress().capturedSec, 0.0, 1e-9);
    // dinle
    p.startListening();
    SWX_BEKLE (p.session().getScreen() == state::Screen::Listening);
    SWX_BEKLE (eng.isListening());
    rt::resetViolations();
    { rt::Scope s; bloklar (p, 200); }
    SWX_BEKLE_EQ (rt::violations(), 0u);
    eng.processPendingSync();
    auto pr = eng.progress();
    SWX_BEKLE_YAKIN (pr.capturedSec, 200.0 * 256.0 / 48000.0, 1e-6);   // host playhead yok → LIVE, kesintisiz
    SWX_BEKLE_EQ (pr.droppedBlocks, (uint64_t) 0);
    SWX_BEKLE_EQ (pr.segments, 1);
    // bitir → Ready, yakalama durur, depo kalır
    p.stopListening();
    SWX_BEKLE (p.session().getScreen() == state::Screen::Ready);
    bloklar (p, 50); eng.processPendingSync();
    SWX_BEKLE_YAKIN (eng.progress().capturedSec, 200.0 * 256.0 / 48000.0, 1e-6);
    // yeniden dinle → temizlenir
    p.startListening(); bloklar (p, 10); eng.processPendingSync();
    SWX_BEKLE_YAKIN (eng.progress().capturedSec, 10.0 * 256.0 / 48000.0, 1e-6);
    // state yükle → dinleme kapanır, ekran Ready
    juce::MemoryBlock mb; p.getStateInformation (mb);
    p.setStateInformation (mb.getData(), (int) mb.getSize());
    SWX_BEKLE (! eng.isListening());
    SWX_BEKLE (p.session().getScreen() == state::Screen::Ready);
    // undo: Listening'e geçiş geri alınabilir (ekran); yakalama bayrağı ekrandan bağımsız kapalı kalır
    p.releaseResources();
}
