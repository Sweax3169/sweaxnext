// SWEAX NEXT — Analysis/Offline.cpp
#include "Offline.h"
#include "Stft.h"
#include "../Decision/Detectors.h"
#include <algorithm>
#include <cmath>

namespace sweax::offline
{

// AnalysisEngine::analyzeSegment'in tam-geçiş (restart) yolu ile aynı akış; bileşenler aynı sınıflar.
void analyzeMonoBuffer (SegmentAnalysis& a, const float* x, int64_t n, double sr)
{
    const int windowN = Stft::sizeForDuration (0.085, sr);
    const int hopN = windowN / 4;
    FeatureExtractor fe; fe.prepare (sr, windowN, hopN, 1);
    a.storeStart = 0; a.length = n; a.analyzedUpTo = 0; a.dirtyFrom = 0; a.dirty = false;
    a.frames.clear(); a.events.clear();
    a.vad.prepare (sr, hopN); a.pitch.prepare (sr, 60.0, 1000.0); a.kw.prepare (sr, 1);
    a.kAcc = 0.0; a.kFill = 0;
    const int momHops = std::max (1, (int) std::lround (0.4 * sr / hopN));
    std::vector<float> kbuf ((size_t) hopN);
    int64_t kPos = 0;
    auto onFrame = [&] (const FrameFeatures& f)
    {
        const int64_t end = (int64_t) std::llround (f.timeSec * sr);
        if (end <= a.analyzedUpTo) return;
        FrameFeatures ff = f;
        ff.frame = (long) a.frames.size();
        ff.timeSec = (double) end / sr;
        while (kPos < end && kPos < n)
        {
            const int m = (int) std::min<int64_t> (hopN, end - kPos);
            std::copy (x + kPos, x + kPos + m, kbuf.data());
            float* kp[1] = { kbuf.data() };
            a.kw.process (kp, 1, m);
            for (int i = 0; i < m; ++i)
                if (kPos + i >= end - hopN) { a.kAcc += (double) kbuf[(size_t) i] * kbuf[(size_t) i]; ++a.kFill; }
            kPos += m;
        }
        FrameRecord r;
        r.timeSec = (float) ((double) end / sr);
        r.kHopMs = a.kFill > 0 ? (float) (a.kAcc / a.kFill) : 0.0f;
        a.kAcc = 0.0; a.kFill = 0;
        PitchTracker::Result pr;
        const int need = a.pitch.frameSize();
        if (end >= need) pr = a.pitch.analyze (x + end - need, need);
        const VadFrame v = a.vad.process (ff, pr, [&] (const VadEvent& e) { a.events.push_back (e); });
        r.flags = v.flags; r.snrDb = v.snrDb; r.sibilantRatioDb = v.sibilantRatioDb; r.lfJumpDb = v.lfJumpDb;
        r.energyDb = f.energyDb; r.lfEnvDb = f.lfEnvDb; r.crestDb = f.crestDb; r.centroidHz = f.centroidHz; r.flatness = f.flatness;
        r.slopeDbPerOct = f.slopeDbPerOct; r.hfRatio = f.hfRatio; r.flux = f.flux; r.pitchHz = pr.hz; r.periodicity = pr.periodicity;
        r.numBands = (uint8_t) std::min (f.numBands, (int) FrameFeatures::MAX_BANDS);
        std::copy (f.bandsDb, f.bandsDb + r.numBands, r.bandsDb);
        double ms = r.kHopMs; int cnt = 1;
        for (int j = 1; j < momHops && (int) a.frames.size() - j >= 0; ++j) { ms += a.frames[a.frames.size() - (size_t) j].kHopMs; ++cnt; }
        r.momentaryLufs = cnt >= momHops ? LoudnessMeter::msToLufs (ms / cnt) : -100.0f;
        a.frames.push_back (r);
        a.analyzedUpTo = end;
    };
    fe.push (x, (int) n, onFrame);
    a.vad.flush ([&] (const VadEvent& e) { a.events.push_back (e); });
}

AnalysisReport reportFromBuffer (const float* x, int64_t n, double sr)
{
    std::vector<SegmentAnalysis> segs (1);
    analyzeMonoBuffer (segs[0], x, n, sr);
    auto r = buildReportCore (segs, x, n, sr);
    decision::DetectorContext ctx;
    ctx.clippedSamples = (float) r.clippedSamples;
    ctx.dcOffset = r.dcOffset;
    r.detections = decision::Detectors::detect (r, segs, ctx);
    return r;
}

} // namespace sweax::offline
