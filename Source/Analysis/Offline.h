#pragma once
// SWEAX NEXT — Analysis/Offline.h
// Tampon üzerinde çevrimdışı analiz (worker/sandbox): mono ses → SegmentAnalysis (kareler+olaylar) → AnalysisReport
// (+ teşhisler). Geri besleme döngüsü işlenmiş sesi BUNUNLA yeniden analiz eder (§5.3: "process → re-analyze");
// motorla aynı bileşenler (FeatureExtractor/Vad/Pitch/KWeighting) — ayrı bir "ikinci gerçek" yoktur.
#include "Report.h"

namespace sweax::offline
{
    // tam geçiş analizi (AnalysisEngine::analyzeSegment'in restart yolu ile aynı akış)
    void analyzeMonoBuffer (SegmentAnalysis& out, const float* x, int64_t n, double sr);

    // rapor + teşhisler (Detectors dahil)
    AnalysisReport reportFromBuffer (const float* x, int64_t n, double sr);
}
