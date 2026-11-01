// SWEAX NEXT — Tests/Analysis/EngineTest.cpp
// AnalysisEngine: eşzamanlı (processPendingSync) ve iş parçacıklı mod; loop üzerine yazma kanıtı çoğaltmaz;
// seek yeni segment; duran host yakalanmaz; pushAudio RT kapsamında tahsissiz; loudness ölçümü depodan.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "Analysis/AnalysisEngine.h"
#include <cstdlib>
#include <cstdio>
#include "Core/RtGuard.h"
#include <vector>
#include <random>
#include <thread>
#include <chrono>

namespace
{
    // 0.5 s oda → 1.0 s ton 140 Hz → 0.3 s oda → 1.0 s ton 200 Hz → 0.5 s oda  (3.3 s)
    std::vector<float> sahne (double fs, unsigned tohum = 3)
    {
        std::vector<float> x; std::mt19937 rng (tohum); std::normal_distribution<float> nd (0.0f, 0.001f);
        double faz = 0.0;
        auto oda = [&] (double sn) { for (size_t i = 0; i < (size_t) (sn * fs); ++i) x.push_back (nd (rng)); };
        auto ton = [&] (double sn, double f0) { for (size_t i = 0; i < (size_t) (sn * fs); ++i) { faz += 2.0 * 3.14159265358979 * f0 / fs; float v = 0.0f; for (int h = 1; h <= 8; ++h) v += (float) (std::sin (h * faz) / h); x.push_back (0.08f * v + nd (rng)); } };
        oda (0.5); ton (1.0, 140.0); oda (0.3); ton (1.0, 200.0); oda (0.5);
        return x;
    }
    void bas (sweax::AnalysisEngine& e, const std::vector<float>& x, int64_t playhead0, bool playing, bool sync, int blok = 512)
    {
        for (size_t i = 0; i < x.size(); i += (size_t) blok)
        {
            const int n = (int) std::min<size_t> ((size_t) blok, x.size() - i);
            const float* ch[1] = { x.data() + i };
            e.pushAudio (ch, 1, n, playhead0 + (int64_t) i, playing);
            if (sync && (i / (size_t) blok) % 16 == 0) e.processPendingSync();
        }
        if (sync) e.processPendingSync();
    }
}

SWX_TEST (engine_eszamanli_loop_seek_durma)
{
    const double fs = 48000.0;
    sweax::AnalysisEngine e; e.prepare (fs, 512); e.setListening (true);
    auto x = sahne (fs);
    bas (e, x, 0, true, true);
    auto p = e.progress();
    SWX_BEKLE_YAKIN (p.capturedSec, 3.3, 0.02);
    SWX_BEKLE_EQ (p.segments, 1);
    SWX_BEKLE_EQ (p.gaps, 1);                                          // ilk blok = akış başlangıcı
    SWX_BEKLE_EQ (p.droppedBlocks, (uint64_t) 0);
    SWX_BEKLE (p.voicedSec > 1.7 && p.voicedSec < 2.2);
    SWX_BEKLE_EQ (p.sentences, 2);
    SWX_BEKLE (p.noiseFloorDb > -66.0f && p.noiseFloorDb < -54.0f);
    SWX_BEKLE (p.maxMomentaryLufs > -40.0f && p.maxMomentaryLufs < -10.0f);
    const size_t kare1 = e.segmentAnalyses()[0].frames.size();
    SWX_BEKLE (kare1 > 140 && kare1 < 160);
    if (std::getenv ("SWX_VERBOSE"))
    {
        for (const auto& ev : e.segmentAnalyses()[0].events) std::printf ("OLAY tip=%d %.3f-%.3f voiced=%d\n", (int) ev.type, ev.startSec, ev.endSec, ev.voicedFrames);
        for (const auto& f : e.segmentAnalyses()[0].frames)
            std::printf ("t=%.3f E=%6.1f snr=%5.1f f0=%6.1f per=%.2f mom=%6.1f fl=%c%c%c%c\n", f.timeSec, f.energyDb, f.snrDb, f.pitchHz, f.periodicity, f.momentaryLufs,
                         (f.flags & sweax::VAD_VOICED) ? 'V' : '.', (f.flags & sweax::VAD_SILENCE) ? 'z' : '.', (f.flags & sweax::VAD_ROOMTONE) ? 'R' : '.', (f.flags & sweax::VAD_TONAL) ? 'T' : '.');
    }
    // loop: aynı proje zamanı yeniden → depo/kanıt aynı kalır
    bas (e, x, 0, true, true);
    p = e.progress();
    SWX_BEKLE_YAKIN (p.capturedSec, 3.3, 0.02);
    SWX_BEKLE_EQ (p.segments, 1);
    SWX_BEKLE_EQ (p.sentences, 2);
    SWX_BEKLE (p.voicedSec > 1.7 && p.voicedSec < 2.2);
    SWX_BEKLE_EQ (e.segmentAnalyses()[0].frames.size(), kare1);
    SWX_BEKLE_EQ (p.gaps, 2);
    // seek: 10 s'ye atla → yeni segment
    std::vector<float> ek (x.begin() + (size_t) (0.5 * fs), x.begin() + (size_t) (1.5 * fs));
    bas (e, ek, (int64_t) (10.0 * fs), true, true);
    p = e.progress();
    SWX_BEKLE_YAKIN (p.capturedSec, 4.3, 0.02);
    SWX_BEKLE_EQ (p.segments, 2);
    SWX_BEKLE_EQ (p.gaps, 3);
    // duran host → yakalanmaz
    bas (e, ek, (int64_t) (20.0 * fs), false, true);
    p = e.progress();
    SWX_BEKLE_YAKIN (p.capturedSec, 4.3, 0.02);
    // bitişik uzama: 10 s segmentine devam
    bas (e, ek, (int64_t) (11.0 * fs), true, true);
    p = e.progress();
    SWX_BEKLE_YAKIN (p.capturedSec, 5.3, 0.02);
    SWX_BEKLE_EQ (p.segments, 2);
    SWX_BEKLE (e.segmentAnalyses()[1].frames.size() > 85);             // 2 s → ~90 kare
    // loudness depodan
    float lufs = 0, tp = 0, mom = 0; e.measureLoudness (lufs, tp, mom);
    SWX_BEKLE (std::isfinite (lufs) && lufs > -40.0f && lufs < -10.0f);
    SWX_BEKLE (tp > -30.0f && tp < 0.0f);
    // temizle
    e.clearCapture();
    p = e.progress();
    SWX_BEKLE_YAKIN (p.capturedSec, 0.0, 1e-9);
    SWX_BEKLE_EQ (p.segments, 0);
}

SWX_TEST (engine_rt_push_tahsissiz_ve_dinleme_kapali)
{
    sweax::AnalysisEngine e; e.prepare (48000.0, 256);
    std::vector<float> L (256, 0.1f), R (256, -0.1f); const float* ch[2] = { L.data(), R.data() };
    sweax::rt::resetViolations();
    {
        sweax::rt::Scope rt;
        for (int i = 0; i < 200; ++i) e.pushAudio (ch, 2, 256, (int64_t) i * 256, true);
    }
    SWX_BEKLE_EQ (sweax::rt::violations(), 0u);
    e.processPendingSync();
    SWX_BEKLE_YAKIN (e.progress().capturedSec, 0.0, 1e-9);            // dinleme kapalı: hiçbir şey yakalanmaz
    e.setListening (true);
    {
        sweax::rt::Scope rt;
        for (int i = 0; i < 100; ++i) e.pushAudio (ch, 2, 256, (int64_t) i * 256, true);
    }
    SWX_BEKLE_EQ (sweax::rt::violations(), 0u);
    e.processPendingSync();
    SWX_BEKLE_YAKIN (e.progress().capturedSec, 100.0 * 256.0 / 48000.0, 1e-6);
}

SWX_TEST (engine_is_parcacigi)
{
    const double fs = 48000.0;
    sweax::AnalysisEngine e; e.prepare (fs, 512); e.setListening (true);
    e.startThread();
    auto x = sahne (fs);
    // gerçek zamana yakın besleme (FIFO 2 s; worker boşaltır)
    for (size_t i = 0; i < x.size(); i += 512)
    {
        const int n = (int) std::min<size_t> (512, x.size() - i);
        const float* ch[1] = { x.data() + i };
        e.pushAudio (ch, 1, n, (int64_t) i, true);
        if ((i / 512) % 32 == 0) std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
    const auto t0 = std::chrono::steady_clock::now();
    sweax::AnalysisProgress p;
    while (true)
    {
        p = e.progress();
        if (p.capturedSec >= 3.29 && p.sentences >= 2) break;
        if (std::chrono::steady_clock::now() - t0 > std::chrono::seconds (10)) break;
        std::this_thread::sleep_for (std::chrono::milliseconds (5));
    }
    e.stopThread();
    SWX_BEKLE_YAKIN (p.capturedSec, 3.3, 0.02);
    SWX_BEKLE_EQ (p.sentences, 2);
    SWX_BEKLE (p.voicedSec > 1.7 && p.voicedSec < 2.2);
    SWX_BEKLE_EQ (p.droppedBlocks, (uint64_t) 0);
}
