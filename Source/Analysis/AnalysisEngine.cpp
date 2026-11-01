// SWEAX NEXT — Analysis/AnalysisEngine.cpp
#include "AnalysisEngine.h"
#include "Report.h"
#include "../Decision/Detectors.h"
#include "../Decision/Feedback.h"
#include <algorithm>
#include <cmath>

namespace sweax
{

AnalysisEngine::AnalysisEngine() : juce::Thread ("SWEAX Analysis") {}
AnalysisEngine::~AnalysisEngine() { stopThread(); }

void AnalysisEngine::prepare (double sampleRate, int maxBlockSize)
{
    const bool wasRunning = isThreadRunning();
    if (wasRunning) stopThread();
    sr = sampleRate; maxBlock = std::max (16, maxBlockSize);
    windowN = Stft::sizeForDuration (0.085, sr);
    hopN = windowN / 4;
    fifo.prepare (sr, maxBlock, 2.0);
    capture.prepare (sr, STORE_SECONDS);
    mono.reset();
    chunk.assign ((size_t) maxBlock * 2, 0.0f);
    monoBuf.assign ((size_t) maxBlock, 0.0f);
    segs.clear(); segs.reserve (64);
    fe.prepare (sr, windowN, hopN, 1);
    loud.prepare (sr, 1, 8192);
    passes = 0; gapCount = 0; noiseFloorLast = -120.0f;
    progBuf[0] = progBuf[1] = AnalysisProgress {};
    progIdx.store (0);
    if (wasRunning) startThread();
}

void AnalysisEngine::setListening (bool on) { listening.store (on, std::memory_order_relaxed); }

void AnalysisEngine::clearCapture()
{
    clearRequest.fetch_add (1, std::memory_order_relaxed);
    fifo.bumpGeneration();
    if (! isThreadRunning()) processPendingSync();
}

void AnalysisEngine::startThread() { juce::Thread::startThread (juce::Thread::Priority::normal); }
void AnalysisEngine::stopThread() { juce::Thread::stopThread (2000); }

void AnalysisEngine::pushAudio (const float* const* channels, int numChannels, int numSamples, int64_t playhead, bool isPlaying) noexcept
{
    if (! listening.load (std::memory_order_relaxed)) return;
    fifo.push (channels, numChannels, numSamples, playhead, isPlaying);
}

AnalysisProgress AnalysisEngine::progress() const noexcept
{
    return progBuf[progIdx.load (std::memory_order_acquire)];
}

void AnalysisEngine::run()
{
    while (! threadShouldExit())
    {
        const bool got = drainFifo();
        analyzeDirty (0.25, false);
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (got || now - lastPublishMs > 200.0) { publishProgress(); lastPublishMs = now; }
        serviceReportRequest();
        if (! got) wait (5);
    }
}

void AnalysisEngine::processPendingSync()
{
    drainFifo();
    analyzeDirty (1.0e9, true);
    publishProgress();
    serviceReportRequest();
}

void AnalysisEngine::requestReport()
{
    reportRequests.fetch_add (1, std::memory_order_release);
    if (! isThreadRunning()) processPendingSync();
}

std::shared_ptr<const AnalysisReport> AnalysisEngine::latestReport() const
{
    const juce::ScopedLock sl (reportLock);
    return report;
}

void AnalysisEngine::serviceReportRequest()
{
    const uint32_t req = reportRequests.load (std::memory_order_acquire);
    if (req == reportsHandled) return;
    reportsHandled = req;
    analyzeDirty (1.0e9, true);                                 // rapor tam materyal üzerinden
    auto r = std::make_shared<AnalysisReport> (buildReport (*this));
    {
        decision::DetectorContext ctx;
        ctx.clippedSamples = (float) r->clippedSamples;
        ctx.dcOffset = r->dcOffset;
        r->detections = decision::Detectors::detect (*r, segs, ctx);
    }
    // denetim M3.2 (K-8): planWant tüketilmez (exchange değil load) — arka arkaya iki replan isteğinde
    // ikinci rapor da finalPlan'la doğar; (K-7) bağlam seqlock'la TUTARLI okunur (yazım ortası yakalanmaz).
    if (planWant.load (std::memory_order_acquire) && capture.size() > 0)
    {
        int okRole, okStyle; uint64_t okPlanId; uint32_t okBits;
        for (;;)
        {
            const uint32_t s1 = planSeq.load (std::memory_order_acquire);
            if ((s1 & 1u) != 0u) continue;                              // yazım sürüyor
            okRole = planRole.load (std::memory_order_relaxed);
            okPlanId = planPlanId.load (std::memory_order_relaxed);
            okStyle = planStyle.load (std::memory_order_relaxed);
            okBits = planLockBits.load (std::memory_order_relaxed);
            std::atomic_thread_fence (std::memory_order_acquire);
            if (planSeq.load (std::memory_order_relaxed) == s1) break;  // tutarlı kopya
        }
        decision::PlannerInput pin;
        pin.report = r.get(); pin.detections = &r->detections;
        pin.role = (state::Role) okRole; pin.planId = okPlanId; pin.style = okStyle;
        pin.outChannels = planOutCh.load (std::memory_order_relaxed);
        for (size_t i = 0; i < pin.locked.size() && i < (size_t) LOCK_N; ++i) pin.locked[i] = (okBits & (1u << i)) != 0u;
        auto fb = decision::Feedback::optimize (pin, capture.data(), capture.size(), sr);
        // A/B ESIT SES (statik, madde 7.4): ORIJINAL dinlenirken kuru yola uygulanacak kazanc = islenmis - kuru LUFS
        fb.finalPlan.abGainDb = std::clamp (fb.processedLufs - r->integratedLufs, -24.0f, 24.0f);   // M3.4 (DSP-5): giriş trimi aralığı; kuru yolda güvenlik limiter'ı
        r->processedLufs = fb.processedLufs;
        r->feedbackRounds = fb.rounds;
        r->score0 = fb.scores[0];
        r->scoreFinal = fb.scores[fb.chosenRound];
        r->finalPlan = std::make_shared<decision::MixPlan> (std::move (fb.finalPlan));
    }
    {
        const juce::ScopedLock sl (reportLock);
        report = std::move (r);
    }
    reportsBuilt.fetch_add (1, std::memory_order_release);
}

bool AnalysisEngine::drainFifo()
{
    // temizleme isteği
    const uint32_t req = clearRequest.load (std::memory_order_relaxed);
    if (req != clearHandled)
    {
        clearHandled = req;
        capture.clear(); segs.clear(); mono.reset(); gapCount = 0; noiseFloorLast = -120.0f;
    }
    bool any = false;
    CaptureChunkHeader h;
    int guard = 0;
    while (guard++ < 4096 && fifo.pop (h, chunk.data(), maxBlock))
    {
        any = true;
        mono.update (h);
        mono.render (h, chunk.data(), monoBuf.data());
        const auto r = capture.ingest (h, monoBuf.data());
        if (! r.accepted) continue;
        if (r.gapBefore) ++gapCount;
        // segment listesi ile analiz listesini eşle (segmentler yalnız sona eklenir; uzunlukları değişir)
        const auto& cs = capture.segments();
        while (segs.size() < cs.size()) { SegmentAnalysis s; s.storeStart = cs[segs.size()].storeStart; segs.push_back (std::move (s)); }
        for (size_t i = 0; i < cs.size(); ++i)
        {
            auto& a = segs[i]; const auto& s = cs[i];
            a.storeStart = s.storeStart;
            if (a.length != s.length) { const int64_t old = a.length; a.length = s.length; a.dirtyFrom = a.dirty ? std::min (a.dirtyFrom, old) : old; a.dirty = true; }
            // üzerine yazma: yazılan aralık bu segmentin içindeyse
            const int64_t ws = r.storeStart, we = r.storeStart + r.storeLength;
            if (r.overwrite && we > s.storeStart && ws < s.storeStart + s.length)
            {
                const int64_t rel = std::max<int64_t> (0, ws - s.storeStart);
                a.dirtyFrom = a.dirty ? std::min (a.dirtyFrom, rel) : rel; a.dirty = true;
                if (rel < a.analyzedUpTo) a.lastOverwriteMs = juce::Time::getMillisecondCounterHiRes();
            }
        }
    }
    return any;
}

void AnalysisEngine::analyzeDirty (double maxSeconds, bool force)
{
    const auto& cs = capture.segments();
    const double now = juce::Time::getMillisecondCounterHiRes();
    for (size_t i = 0; i < segs.size() && i < cs.size(); ++i)
    {
        auto& a = segs[i];
        if (! a.dirty) continue;
        // loop sırasında her parça segmenti baştan analiz ettirirdi (8 s loop → her 0.25 s'de 8 s analiz);
        // geriye dönük yazma durulana kadar bekle — kanıt zaten değişmiyor (üzerine yazma = aynı proje zamanı)
        const bool restart = a.frames.empty() || a.dirtyFrom < a.analyzedUpTo;
        if (! force && restart && ! a.frames.empty() && now - a.lastOverwriteMs < 1000.0) continue;
        analyzeSegment (a, cs[i], maxSeconds);
    }
}

// Segment analizi: kare k, segment-göreli N + k·hop örnekte biter. Uzama → son kareden devam; geri yazma → baştan.
void AnalysisEngine::analyzeSegment (SegmentAnalysis& a, const CaptureStore::Segment& seg, double maxSeconds)
{
    const float* base = capture.data() + seg.storeStart;
    const int64_t len = seg.length;
    const bool restart = a.frames.empty() || a.dirtyFrom < a.analyzedUpTo;
    int64_t from;                      // yeniden beslenecek ilk örnek (segment-göreli)
    if (restart)
    {
        a.frames.clear(); a.events.clear();
        a.vad.prepare (sr, hopN); a.pitch.prepare (sr, 60.0, 1000.0); a.kw.prepare (sr, 1); a.kAcc = 0.0; a.kFill = 0;
        a.analyzedUpTo = 0; from = 0;
        fe.reset();
    }
    else
    {
        // uzama: STFT'yi son karenin TAM penceresiyle yeniden doldur (bir kare üretir: bitişi = analyzedUpTo → atlanır;
        // böylece akı için önceki genlik, LF filtre durumu ve hop halkaları da dolar). Vad/Pitch/K durumu anlık görüntüden sürer.
        from = a.analyzedUpTo;
        fe.reset();
    }
    const int64_t primeStart = restart ? 0 : std::max<int64_t> (0, from - windowN);
    if (! restart)
        fe.push (base + primeStart, (int) (from - primeStart), [] (const FrameFeatures&) {});
    const int64_t budgetSamples = (int64_t) (maxSeconds * sr);
    const int64_t to = std::min (len, from + std::max<int64_t> (budgetSamples, hopN * 4));
    const int momHops = std::max (1, (int) std::lround (0.4 * sr / hopN));
    const int64_t pos = from;
    std::vector<float> kbuf ((size_t) hopN);
    const int64_t kFrom = restart ? 0 : a.analyzedUpTo;        // K-filtre yalnız yeni örnekler
    int64_t kPos = kFrom;
    auto onFrame = [&] (const FrameFeatures& f)
    {
        // karenin bitişi (segment-göreli): fe zamanı primeStart'tan sayar → mutlak konuma çevir
        const int64_t end = (int64_t) std::llround (f.timeSec * sr) + primeStart;
        if (end <= a.analyzedUpTo) return;                      // önceden doldurma karesi
        // Vad'a MUTLAK kare indeksi ve zaman ver (koşu uzunlukları / olay zamanları parça sınırlarından bağımsız)
        FrameFeatures ff = f;
        ff.frame = (long) a.frames.size();
        ff.timeSec = (double) end / sr;
        // K-ağırlıklı hop ms: tüm yeni örnekler filtrelenir (durum sürekliliği), yalnız [end − hop, end) toplanır
        while (kPos < end && kPos < len)
        {
            const int n = (int) std::min<int64_t> (hopN, end - kPos);
            std::copy (base + kPos, base + kPos + n, kbuf.data());
            float* kp[1] = { kbuf.data() };
            a.kw.process (kp, 1, n);
            for (int i = 0; i < n; ++i)
                if (kPos + i >= end - hopN) { a.kAcc += (double) kbuf[(size_t) i] * kbuf[(size_t) i]; ++a.kFill; }
            kPos += n;
        }
        FrameRecord r;
        r.timeSec = (float) ((double) end / sr);
        r.kHopMs = a.kFill > 0 ? (float) (a.kAcc / a.kFill) : 0.0f;
        a.kAcc = 0.0; a.kFill = 0;
        // perde: karenin bitişinden geriye frameSize()
        PitchTracker::Result pr;
        const int need = a.pitch.frameSize();
        if (end >= need) pr = a.pitch.analyze (base + end - need, need);
        const VadFrame v = a.vad.process (ff, pr, [&] (const VadEvent& e) { a.events.push_back (e); });
        r.flags = v.flags; r.snrDb = v.snrDb; r.sibilantRatioDb = v.sibilantRatioDb; r.lfJumpDb = v.lfJumpDb;
        r.energyDb = f.energyDb; r.lfEnvDb = f.lfEnvDb; r.crestDb = f.crestDb; r.centroidHz = f.centroidHz; r.flatness = f.flatness;
        r.slopeDbPerOct = f.slopeDbPerOct; r.hfRatio = f.hfRatio; r.flux = f.flux; r.pitchHz = pr.hz; r.periodicity = pr.periodicity;
        r.numBands = (uint8_t) std::min (f.numBands, (int) FrameFeatures::MAX_BANDS);
        std::copy (f.bandsDb, f.bandsDb + r.numBands, r.bandsDb);
        // momentary: son momHops karenin K hop ms ortalaması
        double ms = r.kHopMs; int cnt = 1;
        for (int j = 1; j < momHops && (int) a.frames.size() - j >= 0; ++j) { ms += a.frames[a.frames.size() - (size_t) j].kHopMs; ++cnt; }
        r.momentaryLufs = cnt >= momHops ? LoudnessMeter::msToLufs (ms / cnt) : -100.0f;
        a.frames.push_back (r);
        a.analyzedUpTo = end;
    };
    // besle
    const int64_t feedFrom = pos;
    fe.push (base + feedFrom, (int) (to - feedFrom), onFrame);
    a.analyzedUpTo = std::max (a.analyzedUpTo, feedFrom);        // kare üretmeyen artık örnekler bir sonraki turda yeniden beslenir
    if (to >= len) { a.dirty = false; a.dirtyFrom = len; }
    else { a.dirtyFrom = a.analyzedUpTo; a.dirty = true; }
    noiseFloorLast = a.vad.noiseFloorRobustDb();
    ++passes;
}

void AnalysisEngine::publishProgress()
{
    AnalysisProgress p;
    p.generation = capture.lastGeneration();
    p.capturedSec = capture.seconds();
    p.segments = (int) capture.segments().size();
    p.gaps = gapCount;
    p.droppedBlocks = capture.totalDroppedBlocks();
    p.storeFull = capture.full();
    p.listening = listening.load (std::memory_order_relaxed);
    p.analysisPasses = passes;
    p.noiseFloorDb = noiseFloorLast;
    for (const auto& a : segs)
    {
        p.voicedSec += a.vad.voicedSeconds();
        p.roomToneSec += a.vad.roomToneSeconds();
        p.sentences += a.vad.sentenceCount();
        for (const auto& e : a.events)
        {
            if (e.type == VadEventType::Sibilant) ++p.sibilantEvents;
            else if (e.type == VadEventType::Plosive) ++p.plosiveEvents;
            else if (e.type == VadEventType::Breath) ++p.breathEvents;
        }
        for (const auto& f : a.frames) p.maxMomentaryLufs = std::max (p.maxMomentaryLufs, f.momentaryLufs);
    }
    const auto c = computeConfidence (segs, (double) hopN / sr);
    p.confLevel = c.level; p.confNoise = c.noise; p.confTonal = c.tonal; p.confDynamics = c.dynamics; p.confSibilance = c.sibilance;
    p.confEnough = c.enough();
    const int next = 1 - progIdx.load (std::memory_order_relaxed);
    progBuf[next] = p;
    progIdx.store (next, std::memory_order_release);
}

void AnalysisEngine::measureLoudness (float& integratedLufs, float& truePeakDb, float& maxMomentaryLufs)
{
    loud.reset();
    const int64_t n = capture.size();
    const float* d = capture.data();
    for (int64_t i = 0; i < n; i += 8192)
    {
        const float* ch[1] = { d + i };
        loud.process (ch, 1, (int) std::min<int64_t> (8192, n - i));
    }
    integratedLufs = loud.integratedLufs();
    truePeakDb = loud.truePeakDb();
    maxMomentaryLufs = loud.maxMomentaryLufs();
}

} // namespace sweax
