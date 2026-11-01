// SWEAX NEXT — Tests/Analysis/ReportTest.cpp
// Güven = kanıt yeterliliği: kısa materyalde düşük, uzun/çok cümleli materyalde yüksek; LOOP güveni ARTIRMAZ
// (aynı 3.3 s × 5 loop == tek geçiş); rapor alanları tutarlı (LUFS, gürültü, cümle istatistikleri, LTAS, F0).
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "Analysis/AnalysisEngine.h"
#include "Analysis/Report.h"
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <random>

namespace
{
    std::vector<float> sahne (double fs, unsigned tohum, double f0a, double f0b)
    {
        std::vector<float> x; std::mt19937 rng (tohum); std::normal_distribution<float> nd (0.0f, 0.001f);
        double faz = 0.0;
        auto oda = [&] (double sn) { for (size_t i = 0; i < (size_t) (sn * fs); ++i) x.push_back (nd (rng)); };
        auto ton = [&] (double sn, double f0, float g) { for (size_t i = 0; i < (size_t) (sn * fs); ++i) { faz += 2.0 * 3.14159265358979 * f0 / fs; float v = 0.0f; for (int h = 1; h <= 8; ++h) v += (float) (std::sin (h * faz) / h); x.push_back (g * v + nd (rng)); } };
        oda (0.5); ton (1.0, f0a, 0.08f); oda (0.3); ton (1.0, f0b, 0.05f); oda (0.5);
        return x;
    }
    void bas (sweax::AnalysisEngine& e, const std::vector<float>& x, int64_t ph)
    {
        for (size_t i = 0; i < x.size(); i += 512)
        {
            const int n = (int) std::min<size_t> (512, x.size() - i);
            const float* ch[1] = { x.data() + i };
            e.pushAudio (ch, 1, n, ph + (int64_t) i, true);
            if ((i / 512) % 32 == 0) e.processPendingSync();
        }
        e.processPendingSync();
    }
}

SWX_TEST (report_guven_kisa_uzun_ve_loop)
{
    const double fs = 48000.0;
    sweax::AnalysisEngine e; e.prepare (fs, 512); e.setListening (true);
    auto x = sahne (fs, 3, 140.0, 200.0);
    bas (e, x, 0);
    e.requestReport();
    auto r1 = e.latestReport();
    SWX_BEKLE (r1 != nullptr);
    SWX_BEKLE_EQ (r1->sentences, 2);
    SWX_BEKLE (r1->confidence.overall() < 0.7f);                      // kısa: yetersiz kanıt
    SWX_BEKLE (! r1->confidence.enough());
    SWX_BEKLE (r1->confidence.noise > 0.4f);                          // 1.3 s oda tonu var
    SWX_BEKLE (r1->confidence.sibilance < 0.3f);
    // loop ×5 aynı zaman → güven ve sayılar AYNI
    for (int k = 0; k < 5; ++k) bas (e, x, 0);
    e.requestReport();
    auto r2 = e.latestReport();
    SWX_BEKLE_YAKIN (r2->capturedSec, r1->capturedSec, 1e-6);
    SWX_BEKLE_EQ (r2->sentences, r1->sentences);
    SWX_BEKLE_YAKIN (r2->confidence.overall(), r1->confidence.overall(), 1e-4);
    SWX_BEKLE_YAKIN (r2->voicedSec, r1->voicedSec, 1e-6);
    // uzun materyal: 6 sahne daha ardışık (≈23 s, 14 cümle; aynı "şarkıcı": aynı F0'lar, farklı gürültü tohumu)
    // → seviye/tonal/dinamik/sibilans kanıtı yeter
    int64_t ph = (int64_t) x.size();
    for (int k = 1; k <= 6; ++k)
    {
        auto y = sahne (fs, 10 + (unsigned) k, 140.0, 200.0);
        bas (e, y, ph); ph += (int64_t) y.size();
    }
    e.requestReport();
    auto r3 = e.latestReport();
    if (std::getenv ("SWX_VERBOSE"))
    {
        std::printf ("captured=%.2f voiced=%.2f room=%.2f sent=%d int=%.1f voicedLufs=%.1f tp=%.1f sp=%.1f floor=%.1f snr=%.1f\n",
                     r3->capturedSec, r3->voicedSec, r3->roomToneSec, r3->sentences, r3->integratedLufs, r3->voicedLufs, r3->truePeakDb, r3->samplePeakDb, r3->noiseFloorDb, r3->snrDb);
        std::printf ("sentMed=%.1f iqr=%.2f p95p05=%.2f microStd=%.2f crest=%.1f slope=%.2f centroid=%.0f flat=%.3f hf=%.3f f0=%.1f/%.1f/%.1f bands=%d tonal=%d halfDiff=%.2f sib=%d plo=%d bre=%d\n",
                     r3->sentenceMedianLufs, r3->sentenceIqrDb, r3->sentenceP95P05Db, r3->microStdMedianDb, r3->crestMedianDb, r3->slopeMedianDbPerOct, r3->centroidMedianHz,
                     r3->flatnessMedianVoiced, r3->hfRatioMedianVoiced, r3->f0P5Hz, r3->f0MedianHz, r3->f0P95Hz, r3->numBands, r3->tonalFrames, r3->ltasHalfDiffDb, r3->sibilantEvents, r3->plosiveEvents, r3->breathEvents);
        std::printf ("conf level=%.2f noise=%.2f tonal=%.2f dyn=%.2f sib=%.2f\n", r3->confidence.level, r3->confidence.noise, r3->confidence.tonal, r3->confidence.dynamics, r3->confidence.sibilance);
        std::printf ("ltas:"); for (int b = 28; b < 44; ++b) std::printf (" [%d]=%.1f", b, r3->ltasDb[b]); std::printf ("  [110]=%.1f [115]=%.1f\n", r3->ltasDb[110], r3->ltasDb[115]);
        for (const auto& s : e.segmentAnalyses()) for (const auto& ev : s.events) if (ev.type == sweax::VadEventType::Plosive) std::printf ("  PLOSIVE %.3f-%.3f peak=%.1f\n", ev.startSec, ev.endSec, ev.peakDb);
        for (const auto& s : r3->sentenceStats) std::printf ("  cumle %.2f-%.2f L=%.1f std=%.2f crest=%.1f vf=%d\n", s.startSec, s.endSec, s.loudnessLufs, s.microStdDb, s.crestDb, s.voicedFrames);
    }
    SWX_BEKLE_EQ (r3->sentences, 14);
    SWX_BEKLE (r3->voicedSec > 12.0);
    SWX_BEKLE (r3->confidence.level >= 0.99f);
    SWX_BEKLE (r3->confidence.dynamics >= 0.99f);
    SWX_BEKLE (r3->confidence.sibilance >= 0.99f);
    SWX_BEKLE (r3->confidence.tonal > 0.7f);                          // benzer LTAS'lı yarılar → kararlı
    SWX_BEKLE (r3->confidence.enough());
    // rapor alanları
    SWX_BEKLE (r3->integratedLufs > -40.0f && r3->integratedLufs < -10.0f);
    SWX_BEKLE (r3->voicedLufs > r3->integratedLufs - 1.0f);         // sesli-gate'li ≥ tüm depo (sessizlikler dahil değil)
    SWX_BEKLE (r3->truePeakDb > -25.0f && r3->truePeakDb < 0.0f);
    SWX_BEKLE (r3->samplePeakDb <= r3->truePeakDb + 0.01f);
    SWX_BEKLE (r3->noiseFloorDb > -66.0f && r3->noiseFloorDb < -54.0f);
    SWX_BEKLE (r3->snrDb > 25.0f);
    SWX_BEKLE_EQ ((int) r3->sentenceStats.size(), 14);
    SWX_BEKLE (r3->sentenceIqrDb > 2.0f && r3->sentenceIqrDb < 6.0f);  // −24 vs −28 dB'lik iki cümle türü
    SWX_BEKLE (r3->microStdMedianDb < 1.5f);                          // sabit tonlar: cümle içi dalgalanma küçük
    SWX_BEKLE (r3->f0MedianHz > 120.0f && r3->f0MedianHz < 260.0f);
    SWX_BEKLE (r3->f0P5Hz < r3->f0MedianHz && r3->f0P95Hz > r3->f0MedianHz);
    SWX_BEKLE (r3->numBands > 100 && r3->tonalFrames > 500);
    SWX_BEKLE (r3->ltasDb[33] > r3->ltasDb[110] + 20.0f);             // 140 Hz bandı (temel) 11 kHz bandından ≥20 dB güçlü
    SWX_BEKLE (r3->slopeMedianDbPerOct < -3.0f);                      // 1/h harmonikler → düşen eğim
    SWX_BEKLE_EQ (r3->sibilantEvents, 0);
    SWX_BEKLE (r3->crestMedianDb > 3.0f && r3->crestMedianDb < 12.0f);
}
