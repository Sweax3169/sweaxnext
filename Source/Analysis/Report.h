#pragma once
// SWEAX NEXT — Analysis/Report.h
// Güven motoru (mimari §3.5) + dondurulmuş analiz raporu (§5 girdisi).
//  GÜVEN = KANIT YETERLİLİĞİ, tespit değil. Dedektör başına; hepsi loop-değişmezdir (depo içeriğinden hesaplanır):
//   • level    : sesli süre (≥6 s → 1) × cümle (≥3 → 1)
//   • noise    : oda tonu süresi (0.5 s → 0.5, ≥2 s → 1) × taban kararlılığı (oda karelerinin enerji std'si ≤ 3 dB → 1)
//   • tonal    : sesli süre (≥8 s) × cümle (≥4) × LTAS kararlılığı (ilk/ikinci yarı LTAS farkı 200 Hz–8 kHz: 0 dB → 1, ≥3 dB → 0)
//   • dynamics : cümle (≥5) × sesli süre (≥10 s)
//   • sibilance: sesli süre (≥12 s) × cümle (≥5)   — olay SAYISI güveni artırmaz; olay yoksa p≈0, c yüksek → "Gerekmedi"
//   Yeterli: zorunlu dedektörlerin tümü ≥ 0.7 VEYA sesli süre ≥ 90 s. Düşük güven yalnız AZALTIR (§5.2).
//  Rapor: BS.1770 integrated (depo) + sesli-gate'li momentary ortalaması, true-peak, gürültü tabanı, cümle loudness
//  istatistikleri (medyan / IQR / p95−p05 / cümle-içi std), crest, LTAS (tonal kareler, 1/12 okt dB ortalaması + std),
//  eğim, F0 aralığı, sibilant/plosive/breath olay sayıları ve kalibre bant merkezi, sinyal kalitesi sayaçları.
#include "AnalysisEngine.h"
#include "../Decision/Plan.h"
#include <vector>
#include <cstdint>

namespace sweax
{

struct DetectorConfidence
{
    float level { 0.0f }, noise { 0.0f }, tonal { 0.0f }, dynamics { 0.0f }, sibilance { 0.0f };
    bool  ceiling { false };                      // 90 s sesli tavan
    float overall() const noexcept { float m = level; for (float v : { noise, tonal, dynamics, sibilance }) m = std::min (m, v); return m; }
    bool  enough() const noexcept { return ceiling || overall() >= 0.7f; }
};

struct SentenceStat
{
    double startSec { 0.0 }, endSec { 0.0 };
    int voicedFrames { 0 };
    float loudnessLufs { -100.0f };               // sesli karelerin momentary'sinin ms-alanı ortalaması
    float microStdDb { 0.0f };                    // cümle içi momentary std
    float crestDb { 0.0f };                       // sesli kare crest medyanı
};

struct AnalysisReport
{
    uint32_t generation { 0 };
    double capturedSec { 0.0 }, voicedSec { 0.0 }, roomToneSec { 0.0 };
    int sentences { 0 }, segments { 0 }, gaps { 0 };
    uint64_t droppedBlocks { 0 };
    bool storeFull { false };
    // seviye
    float integratedLufs { -100.0f }, voicedLufs { -100.0f }, maxMomentaryLufs { -100.0f }, truePeakDb { -100.0f }, samplePeakDb { -100.0f };
    float noiseFloorDb { -120.0f }, snrDb { 0.0f };
    bool  noiseFloorKnown { true };        // false: tabana yakın kare yok → taban yalnız üst sınır (M3.4 T5)
    // dinamik
    float sentenceMedianLufs { -100.0f }, sentenceIqrDb { 0.0f }, sentenceP95P05Db { 0.0f }, microStdMedianDb { 0.0f }, crestMedianDb { 0.0f };
    // tonal
    int numBands { 0 };
    float ltasDb[FrameFeatures::MAX_BANDS] {};
    float ltasStdDb[FrameFeatures::MAX_BANDS] {};
    float ltasHalfDiffDb { 0.0f };                // kararlılık ölçüsü
    float slopeMedianDbPerOct { 0.0f }, centroidMedianHz { 0.0f }, flatnessMedianVoiced { 0.0f }, hfRatioMedianVoiced { 0.0f };
    int tonalFrames { 0 };
    // dogallik olcutleri (feedback skoru): onset keskinligi = sesli akinin en ust ondaligi; modulasyon = K-momentary
    // dB dizisinin kare-farki RMS'i (2-8 Hz bolgesinin kaba vekili; kare hizi ~47 Hz)
    float onsetSharpness { 0.0f };
    float momentaryModDb { 0.0f };
    // perde
    float f0MedianHz { 0.0f }, f0P5Hz { 0.0f }, f0P95Hz { 0.0f };
    // gam algilama (M3.1, Krumhansl-Schmuckler): sesli kare perde-sinifi histogrami → 24 profil korelasyonu.
    // keyIndex -1 = guvenilir gam yok (tune kromatik kalir). Confidence = korelasyon marji + kare sayisi.
    int keyIndex { -1 };                          // 0=C .. 11=B
    bool keyMinor { false };
    float keyConfidence { 0.0f };                 // 0..1
    // olaylar
    int sibilantEvents { 0 }, plosiveEvents { 0 }, breathEvents { 0 };
    float sibilantCenterHz { 0.0f }; bool sibilantCalibrated { false };
    float sibilantPerMinute { 0.0f };
    // sinyal kalitesi (depodan tek geçiş)
    uint64_t clippedSamples { 0 };
    float dcOffset { 0.0f };
    // güven
    DetectorConfidence confidence;
    std::vector<SentenceStat> sentenceStats;
    // teşhisler (worker rapor isteğinde Detectors ile doldurur; plan bunlardan türetilir)
    std::vector<decision::ProblemDetection> detections;
    // NİHAİ PLAN (worker: Planner + FeedbackOptimizer; message thread yalnız yayınlar) + geri besleme özeti
    std::shared_ptr<const decision::MixPlan> finalPlan;
    float processedLufs { -100.0f };
    int   feedbackRounds { 0 };
    float score0 { 0.0f }, scoreFinal { 0.0f };
};

// Segment analizlerinden güven (ucuz; ilerleme için). frames/events okur, depoya dokunmaz.
DetectorConfidence computeConfidence (const std::vector<SegmentAnalysis>& segs, double hopSec);

// Tam rapor (worker veya eşzamanlı; O(n) depo loudness ölçümü dahil)
AnalysisReport buildReport (AnalysisEngine& engine);
// Çekirdek: verilen segment analizleri + ham tampon üzerinden (çevrimdışı/feedback yolu da bunu kullanır)
AnalysisReport buildReportCore (const std::vector<SegmentAnalysis>& segs, const float* storeData, int64_t storeLen, double sr);

} // namespace sweax
