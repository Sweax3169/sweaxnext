#pragma once
// SWEAX NEXT — PluginProcessor.h
// Kabuk: APVTS (tek hakikat kaynağı) + ParamCache (ses iş parçacığı okuması) + SESSION (message thread)
// + VocalChain + kuru yol + bypass crossfade + metering. Mimari §1.1, §2, §6, §7.
#include "Core/ParamCache.h"
#include "Core/DelayLine.h"
#include "Core/Smoother.h"
#include "State/Session.h"
#include "Chain/VocalChain.h"
#include "Core/Fifo.h"
#include "Analysis/AnalysisEngine.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>
#include <vector>

namespace sweax
{

class SweaxProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    SweaxProcessor();
    ~SweaxProcessor() override;

    // ---- AudioProcessor ----
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SWEAX NEXT"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; } // uzamsal modüller Faz 6'da bildirir
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    juce::AudioProcessorParameter* getBypassParameter() const override; // host bypass bizim yolumuzdan (§1.1)

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- SWEAX ----
    juce::AudioProcessorValueTreeState& apvts() noexcept { return params; }
    ParamCache& paramCache() noexcept { return cache; }
    state::Session& session() noexcept { return sess; }          // yalnız message thread
    juce::UndoManager& undoManager() noexcept { return undo; }

    // ölçerler (ses yazar, UI okur)
    float inputPeakDb() const noexcept { return inPeakDb.load (std::memory_order_relaxed); }
    float outputPeakDb() const noexcept { return outPeakDb.load (std::memory_order_relaxed); }
    bool  sidechainConnected() const noexcept { return scConnected.load (std::memory_order_relaxed); }
    int   currentLatency() const noexcept { return latency.load (std::memory_order_relaxed); }
    uint64_t nonFiniteBlocks() const noexcept { return nonFinite.load (std::memory_order_relaxed); }

    // editör bağlı mı (spektrum FIFO yalnız bağlıyken yazılır — §8)
    void setEditorAttached (bool b) noexcept { editorAttached.store (b, std::memory_order_relaxed); }
    SpscFifo<float>& spectrumInFifo() noexcept { return specIn; }     // UI tüketir (mono karışım)
    SpscFifo<float>& spectrumOutFifo() noexcept { return specOut; }

    // analiz (§3): yalnız message thread; yakalama bayrağını ses iş parçacığı okur
    AnalysisEngine& analysisEngine() noexcept { return analysis; }
    void startListening();             // yakalamayı temizle + dinle → Screen::Listening
    void stopListening();              // dinlemeyi bitir → rapor iste; rapor gelince plan kur+yayınla → MixReady
    bool replanFromCapture();          // MixReady'de stil/rol değişimi: dinlemeden, dondurulmuş yakalamadan yeni bağlamla plan
    void cancelListening();            // dinlemeyi RAPORSUZ iptal et (rol değişimi vb.) — motor durur, ekran Ready
    bool planPendingNow() const noexcept { return planPending.load (std::memory_order_relaxed); }
    void buildAndPublishPlan();        // message thread (timer'dan; test için de çağrılabilir)
    void refreshAbGain() noexcept;     // message thread: A/B eş-ses kazancını oturumdan ses iş parçacığına aktarır

    // preset: tam durum (APVTS + SESSION) dosyaya/dosyadan — message thread
    bool savePresetTo (const juce::File& f);
    bool loadPresetFrom (const juce::File& f);

    Quality qualityMode() const noexcept { return (Quality) cache.choice (params::QualityMode); }
    VocalChain& vocalChain() noexcept { return chain; }        // ölçerler (UI okur)

private:
    void applyLatency();                       // message thread / prepareToPlay
    void timerCallback() override;             // message thread: mod değişiminde latency bildirimi
    void refreshBaselinesFromSession();        // SESSION → ParamCache taban atomikleri

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState params;
    ParamCache cache;
    state::Session sess;
    VocalChain chain;
    AnalysisEngine analysis;

    // kuru yol (A/B, DELTA, bypass): rapor edilen latency kadar geciktirilmiş giriş kopyası
    std::array<DelayLine, VocalChain::MAX_CHANNELS> dryDelay;
    juce::AudioBuffer<float> dryBuffer;        // constructor'da worst-case
    // M3.4 (DSP-5): A/B ORİJİNAL eş-ses kazancı ±24 dB'e çıktı → kuru yola işlenmiş yolla aynı tavanda limiter.
    // Hizalama: ayrı gecikme hattı (zincir gecikmesi − limiter gecikmesi) + limiter = zincir gecikmesi.
    std::array<DelayLine, VocalChain::MAX_CHANNELS> dryLimDelay;
    juce::AudioBuffer<float> dryLimBuffer;
    OutputStage dryLimiter;
    LinearSmoother bypassMix;                  // 0 = wet, 1 = dry (eş-kazanç lineer crossfade)
    LinearSmoother deltaMix;                   // 0 = normal, 1 = delta (wet − dry; hizalı fark dinleme, §7.4)

    SpscFifo<float> specIn { 16384 }, specOut { 16384 };   // spektrum ekranı beslemesi (yalnız editör bağlıyken)
    std::atomic<float> inPeakDb { -100.0f }, outPeakDb { -100.0f };
    std::atomic<bool> scConnected { false }, editorAttached { false };
    std::atomic<bool> planPending { false };
    uint64_t lastReportSerial { 0 };
    std::atomic<float> abGainLin { 1.0f };     // A/B eşit-ses kazancı (lineer; message thread yazar, ses okur)
    std::atomic<uint32_t> stateLoadSeq { 0 };  // denetim: bool bayrak ses bloğu araya girmeyince kaçıyordu
    uint32_t stateLoadSeen { 0 };              // yalnız ses iş parçacığı
    std::atomic<int> latency { 0 };
    std::atomic<bool> latencyDirty { false };   // ses iş parçacığı mod değişimini gördü → message thread host'a bildirir
    Quality lastMode { Quality::Yuksek };
    std::atomic<uint64_t> nonFinite { 0 };
    float meterDecay { 0.9f };
    double sampleRate { 48000.0 };
    int maxBlock { 0 };

    static constexpr int WORST_BLOCK = 8192;
    static constexpr double WORST_SR = 192000.0;
    static constexpr double WORST_LATENCY_SEC = 0.25; // ULTRA leveler 150 ms + oversampling + lookahead payı

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SweaxProcessor)
};

} // namespace sweax
