#pragma once
// SWEAX NEXT — Analysis/AnalysisEngine.h
// Analiz iş parçacığı (mimari §2.2, §3): CaptureFifo → MonoSelector → CaptureStore → SEGMENT BAZLI ARTIMLI ANALİZ.
//  • Analiz, deponun içeriğinin saf bir fonksiyonudur: kareler depo örnek konumuna hizalıdır (kare k, segment başından
//    N + k·hop örnekte biter). Loop/seek ile üzerine yazılan bölge yeniden analiz edilir ve önceki kareler DEĞİŞTİRİLİR →
//    kanıt asla iki kez sayılmaz. Bitişik uzama yalnız yeni kareleri hesaplar (Vad/Pitch/K-filtre durumu anlık görüntüden
//    sürdürülür); geriye dönük üzerine yazma segmenti baştan analiz eder (loop'lar kısa).
//  • Ses iş parçacığı yalnız pushAudio() çağırır (tahsissiz, kilitsiz). Worker asla MessageManagerLock almaz.
//  • Progress: POD, atomik çift tampon (worker yazar, UI okur). AnalysisReport (Faz 2 sonu) ayrı yayınlanır.
//  • Kare başına K-ağırlıklı hop mean-square saklanır → momentary (400 ms) kareye hizalı; integrated/TP rapor anında
//    LoudnessMeter ile depo üzerinden hesaplanır.
#include "CaptureFifo.h"
#include "CaptureStore.h"
#include "Features.h"
#include "Pitch.h"
#include "Vad.h"
#include "Loudness.h"
#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>
#include <memory>

namespace sweax
{

struct FrameRecord
{
    float timeSec { 0.0f };            // depo koordinatında karenin bitişi (s)
    uint32_t flags { 0 };              // VadFlag
    float energyDb { -120.0f }, lfEnvDb { -120.0f }, crestDb { 0.0f };
    float centroidHz { 0.0f }, flatness { 0.0f }, slopeDbPerOct { 0.0f }, hfRatio { 0.0f }, flux { 0.0f };
    float pitchHz { 0.0f }, periodicity { 0.0f };
    float kHopMs { 0.0f };             // K-ağırlıklı hop mean-square
    float momentaryLufs { -100.0f };   // son 400 ms
    float snrDb { 0.0f }, sibilantRatioDb { 0.0f }, lfJumpDb { 0.0f };
    uint8_t numBands { 0 };
    float bandsDb[FrameFeatures::MAX_BANDS] {};
};

struct SegmentAnalysis
{
    int64_t storeStart { 0 }, length { 0 };
    int64_t analyzedUpTo { 0 };        // segment-göreli örnek: bu konuma kadar kareler geçerli
    int64_t dirtyFrom { 0 };           // segment-göreli: bu konumdan itibaren yeniden analiz gerekli (< analyzedUpTo → baştan)
    bool dirty { true };
    double lastOverwriteMs { -1.0e9 };  // geriye dönük üzerine yazma (loop) → yeniden analiz yazma durulunca (≥1 s)
    std::vector<FrameRecord> frames;
    std::vector<VadEvent> events;
    // artımlı sürdürme anlık görüntüsü (son analiz edilen karenin ardından)
    Vad vad; PitchTracker pitch; KWeighting kw; double kAcc { 0.0 }; int kFill { 0 };
};

struct AnalysisProgress
{
    uint32_t generation { 0 };
    double capturedSec { 0.0 };        // depodaki benzersiz materyal
    double voicedSec { 0.0 };
    double roomToneSec { 0.0 };
    int sentences { 0 }, sibilantEvents { 0 }, plosiveEvents { 0 }, breathEvents { 0 };
    int segments { 0 }, gaps { 0 };
    float noiseFloorDb { -120.0f };
    float maxMomentaryLufs { -100.0f };
    uint64_t droppedBlocks { 0 };
    bool storeFull { false };
    bool listening { false };
    uint64_t analysisPasses { 0 };     // her artımlı analiz turu
    // dedektör bazlı kanıt yeterliliği (Report.h: computeConfidence) — UI gerçek yüzdeleri buradan okur
    float confLevel { 0.0f }, confNoise { 0.0f }, confTonal { 0.0f }, confDynamics { 0.0f }, confSibilance { 0.0f };
    bool  confEnough { false };
};

struct AnalysisReport;

class AnalysisEngine : private juce::Thread
{
public:
    AnalysisEngine();
    ~AnalysisEngine() override;

    // message thread
    void prepare (double sampleRate, int maxBlock);
    void setListening (bool on);       // yakalama bayrağı (ses iş parçacığı okur)
    bool isListening() const noexcept { return listening.load (std::memory_order_relaxed); }
    void clearCapture();               // depo + analiz sıfırla (generation++)
    void startThread(); void stopThread();
    bool isThreadRunning() const noexcept { return juce::Thread::isThreadRunning(); }

    // ses iş parçacığı
    void pushAudio (const float* const* channels, int numChannels, int numSamples, int64_t playhead, bool isPlaying) noexcept;

    // UI / message thread
    AnalysisProgress progress() const noexcept;
    // rapor: istek → worker (veya processPendingSync) dondurulmuş raporu üretir ve yayınlar.
    // Plan bağlamı verilmişse worker Planner + Feedback döngüsünü de koşar ve raporun finalPlan alanını doldurur
    // (APVTS'ye dokunmaz; yayın message thread'de PlanPublisher ile).
    static constexpr int LOCK_N = 20;   // = state::modul::HEPSI.size() (PluginProcessor.cpp static_assert korur)
    // denetim M3.2 (K-7/K-8): bağlam alanları artık ATOMİK + seqlock'lu — message thread her stil/rol
    // tıklamasında yazarken worker Feedback içindeyken bile yırtık okuma/UB yok; planWant artık
    // TÜKETİLMEZ (bağlam bir kez kurulunca her istenen rapor finalPlan'la doğar — çift tık yedeğe düşmüyor).
    void setOutputChannels (int n) noexcept { planOutCh.store (std::clamp (n, 1, 2), std::memory_order_relaxed); }
    void setPlanContext (int role, uint64_t planId, const bool* locks, int style) noexcept
    {
        uint32_t bits = 0;
        for (int i = 0; i < LOCK_N; ++i) if (locks[i]) bits |= (1u << i);
        planSeq.fetch_add (1, std::memory_order_acq_rel);              // tek yazar: message thread → tek sayı yeter
        planRole.store (role, std::memory_order_relaxed);
        planPlanId.store (planId, std::memory_order_relaxed);
        planStyle.store (style, std::memory_order_relaxed);
        planLockBits.store (bits, std::memory_order_relaxed);
        planSeq.fetch_add (1, std::memory_order_release);              // çift = tutarlı
        planWant.store (true, std::memory_order_release);
    }
    void requestReport();
    std::shared_ptr<const AnalysisReport> latestReport() const;
    uint64_t reportSerial() const noexcept { return reportsBuilt.load (std::memory_order_acquire); }

    // worker (veya test: eşzamanlı çalıştırma)
    void processPendingSync();         // FIFO'yu boşalt + kirli segmentleri analiz et + progress yayınla
    const CaptureStore& store() const noexcept { return capture; }
    const std::vector<SegmentAnalysis>& segmentAnalyses() const noexcept { return segs; }
    // depo üzerinde integrated/true-peak (rapor anında; O(n))
    void measureLoudness (float& integratedLufs, float& truePeakDb, float& maxMomentaryLufs);

    static constexpr double STORE_SECONDS = 100.0;

private:
    void run() override;
    bool drainFifo();                  // en az bir blok işlendiyse true
    void analyzeDirty (double maxSeconds, bool force);   // force: durulma beklemeden (eşzamanlı/test/rapor)
    void analyzeSegment (SegmentAnalysis& s, const CaptureStore::Segment& seg, double maxSeconds);
    void publishProgress();
    void serviceReportRequest();

    double sr { 48000.0 };
    int maxBlock { 512 };
    int windowN { 4096 }, hopN { 1024 };
    std::atomic<bool> listening { false };
    std::atomic<uint32_t> clearRequest { 0 };
    uint32_t clearHandled { 0 };
    CaptureFifo fifo;
    CaptureStore capture;
    MonoSelector mono;
    std::vector<float> chunk, monoBuf;
    std::vector<SegmentAnalysis> segs;
    FeatureExtractor fe;
    LoudnessMeter loud;
    // progress çift tampon
    AnalysisProgress progBuf[2];
    std::atomic<int> progIdx { 0 };
    uint64_t passes { 0 };
    int gapCount { 0 };
    float noiseFloorLast { -120.0f };
    double lastPublishMs { 0.0 };
    // rapor el değişimi (worker yazar, UI okur; kısa kilit, ses iş parçacığı asla)
    std::atomic<uint32_t> reportRequests { 0 };
    uint32_t reportsHandled { 0 };
    std::atomic<bool> planWant { false };
    std::atomic<uint32_t> planSeq { 0 };
    std::atomic<int> planOutCh { 2 };
    std::atomic<int> planRole { 0 }; std::atomic<uint64_t> planPlanId { 1 };
    std::atomic<int> planStyle { 1 }; std::atomic<uint32_t> planLockBits { 0 };
    std::atomic<uint64_t> reportsBuilt { 0 };
    mutable juce::CriticalSection reportLock;
    std::shared_ptr<const AnalysisReport> report;
};

} // namespace sweax
