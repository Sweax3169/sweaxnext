// SWEAX NEXT — Tests/Analysis/VadTest.cpp
// Sentetik "vokal" sahnesi: oda gürültüsü → sesli ton → sibilant patlaması → sesli → LF darbe (plosive) → oda →
// nefes → oda → sesli. Beklenti: gürültü tabanı ≈ −60 dB, voiced yalnız tonlarda, 1 sibilant olayı (60–120 ms),
// ≥1 plosive olayı darbede, ≥1 breath olayı nefeste, 2 cümle (sibilant boşluğu <150 ms cümleyi bölmez).
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "Analysis/Features.h"
#include "Analysis/Pitch.h"
#include "Analysis/Vad.h"
#include <vector>
#include <random>
#include <cstdlib>
#include <cstdio>

namespace
{
    struct Sahne { std::vector<float> x; std::vector<std::pair<double, double>> voicedAralik, sibAralik, ploAralik, nefesAralik; };

    Sahne kur (double fs)
    {
        Sahne s; std::mt19937 rng (11); std::normal_distribution<float> nd (0.0f, 1.0f);
        auto ekle = [&] (double sn, const std::function<float (double, size_t)>& gen) { const size_t n = (size_t) (sn * fs); const double t0 = (double) s.x.size() / fs; for (size_t i = 0; i < n; ++i) s.x.push_back (gen (t0 + (double) i / fs, i)); return t0; };
        auto oda = [&] (double, size_t) { return 0.001f * nd (rng); };                                   // −60 dBFS
        double faz = 0.0;
        auto ses = [&] (double f0) { return [&, f0] (double, size_t) { faz += 2.0 * 3.14159265358979 * f0 / fs; float v = 0.0f; for (int h = 1; h <= 8; ++h) v += (float) (std::sin (h * faz) / h); return 0.08f * v + 0.001f * nd (rng); }; };
        sweax::Biquad hp1, hp2, lp1, lp2, lpB;
        hp1.setCoeffs (sweax::design::highpass (fs, 5000.0, 0.7071)); hp2.setCoeffs (sweax::design::highpass (fs, 5000.0, 0.7071));
        lp1.setCoeffs (sweax::design::lowpass (fs, 9000.0, 0.7071)); lp2.setCoeffs (sweax::design::lowpass (fs, 9000.0, 0.7071));
        lpB.setCoeffs (sweax::design::lowpass (fs, 3000.0, 0.7071));
        auto sib = [&] (double, size_t) { return 0.06f * lp2.processSample (lp1.processSample (hp2.processSample (hp1.processSample (nd (rng))))) + 0.001f * nd (rng); };
        auto darbe = [&] (double t, size_t i) { const double tl = (double) i / fs; return (float) (0.4 * std::exp (-tl * 40.0) * std::sin (2.0 * 3.14159265358979 * 60.0 * tl)) + 0.001f * nd (rng) + 0.0f * (float) t; };
        auto nefes = [&] (double, size_t) { return 0.025f * lpB.processSample (nd (rng)) + 0.001f * nd (rng); };  // ≈ −41 dBFS RMS
        ekle (0.6, oda);
        double t = ekle (0.6, ses (140.0)); s.voicedAralik.push_back ({ t, t + 0.6 });
        t = ekle (0.08, sib); s.sibAralik.push_back ({ t, t + 0.08 });
        t = ekle (0.5, ses (150.0)); s.voicedAralik.push_back ({ t, t + 0.5 });
        t = ekle (0.06, darbe); s.ploAralik.push_back ({ t, t + 0.06 });
        ekle (0.3, oda);
        t = ekle (0.2, nefes); s.nefesAralik.push_back ({ t, t + 0.2 });
        ekle (0.5, oda);
        t = ekle (0.7, ses (200.0)); s.voicedAralik.push_back ({ t, t + 0.7 });
        ekle (0.4, oda);
        return s;
    }
    bool icinde (double t, const std::vector<std::pair<double, double>>& a, double pay = 0.03) { for (auto& p : a) if (t >= p.first - pay && t <= p.second + pay) return true; return false; }
}

SWX_TEST (vad_sahne_bayraklar_ve_olaylar)
{
    const double fs = 48000.0;
    auto s = kur (fs);
    sweax::FeatureExtractor fe; fe.prepare (fs, 4096, 1024);
    sweax::PitchTracker pt; pt.prepare (fs, 60.0, 1000.0);
    sweax::Vad vad; vad.prepare (fs, 1024);
    std::vector<sweax::VadEvent> olaylar;
    int voicedDogru = 0, voicedYanlis = 0, sibKare = 0, sibYanlis = 0;
    size_t pos = 0;
    auto onEvent = [&] (const sweax::VadEvent& e) { olaylar.push_back (e); if (std::getenv ("SWX_VERBOSE")) std::printf ("OLAY tip=%d %.3f-%.3f peak=%.1f center=%.0f voiced=%d\n", (int) e.type, e.startSec, e.endSec, e.peakDb, e.centerHz, e.voicedFrames); };
    // özellik çıkarıcı hop hop ilerler; perde aynı karenin sonundan geriye frameSize() örnekle
    fe.push (s.x.data(), (int) s.x.size(), [&] (const sweax::FrameFeatures& f)
    {
        const long end = (long) std::llround (f.timeSec * fs);
        sweax::PitchTracker::Result pr;
        if (end >= pt.frameSize()) pr = pt.analyze (s.x.data() + end - pt.frameSize(), pt.frameSize());
        const auto v = vad.process (f, pr, onEvent);
        const double tMid = f.timeSec - 0.5 * fe.windowSize() / fs;
        if (std::getenv ("SWX_VERBOSE"))
            std::printf ("t=%.3f E=%6.1f floor=%6.1f snr=%5.1f flat=%.2f hf=%.2f cent=%5.0f sib=%5.1f lfE=%6.1f lfJ=%5.1f flux=%.2f f0=%5.1f per=%.2f fl=%c%c%c%c%c%c%c\n",
                         tMid, f.energyDb, v.noiseFloorDb, v.snrDb, f.flatness, f.hfRatio, f.centroidHz, v.sibilantRatioDb, f.lfEnvDb, v.lfJumpDb, f.flux, pr.hz, pr.periodicity,
                         v.has (sweax::VAD_VOICED) ? 'V' : '.', v.has (sweax::VAD_SIBILANT) ? 'S' : '.', v.has (sweax::VAD_PLOSIVE) ? 'P' : '.', v.has (sweax::VAD_BREATH) ? 'B' : '.',
                         v.has (sweax::VAD_SILENCE) ? 'z' : '.', v.has (sweax::VAD_ROOMTONE) ? 'R' : '.', v.has (sweax::VAD_TONAL) ? 'T' : '.');
        if (v.has (sweax::VAD_VOICED)) { if (icinde (tMid, s.voicedAralik, 0.06)) ++voicedDogru; else ++voicedYanlis; }
        if (v.has (sweax::VAD_SIBILANT)) { if (icinde (tMid, s.sibAralik, 0.06)) ++sibKare; else ++sibYanlis; }
        (void) pos;
    });
    vad.flush (onEvent);
    SWX_BEKLE_YAKIN (vad.noiseFloorDb(), -60.0, 5.0);
    SWX_BEKLE (voicedDogru > 60);                                       // 1.8 s sesli ≈ 84 kare
    SWX_BEKLE (voicedYanlis <= 2);
    SWX_BEKLE (sibKare >= 2 && sibYanlis <= 1);
    SWX_BEKLE (vad.voicedSeconds() > 1.4 && vad.voicedSeconds() < 2.2);
    SWX_BEKLE (vad.roomToneSeconds() > 1.2);
    int sib = 0, plo = 0, bre = 0, sen = 0; bool sibYer = true, ploYer = true, breYer = true;
    for (auto& e : olaylar)
    {
        // olay zamanları hop-doğru (özellik karesinin bitiş zamanı); spektral olaylar için STFT penceresi (85 ms) kadar pay
        const double tm = 0.5 * (e.startSec + e.endSec);
        switch (e.type)
        {
            case sweax::VadEventType::Sibilant: ++sib; sibYer = sibYer && icinde (tm, s.sibAralik, 0.09); SWX_BEKLE (e.centerHz > 5000.0f && e.centerHz < 10000.0f); break;
            case sweax::VadEventType::Plosive:  ++plo; ploYer = ploYer && icinde (tm, s.ploAralik, 0.06); break;
            case sweax::VadEventType::Breath:   ++bre; breYer = breYer && icinde (tm, s.nefesAralik, 0.12); break;
            case sweax::VadEventType::Sentence: ++sen; SWX_BEKLE (e.voicedFrames > 15); break;
        }
    }
    SWX_BEKLE_EQ (sib, 1); SWX_BEKLE (sibYer);
    SWX_BEKLE (plo >= 1); SWX_BEKLE (ploYer);
    SWX_BEKLE (bre >= 1); SWX_BEKLE (breYer);
    SWX_BEKLE_EQ (sen, 2); SWX_BEKLE_EQ (vad.sentenceCount(), 2);
    SWX_BEKLE_EQ (vad.sibilantEventCount(), 1);
}

SWX_TEST (vad_sessizlik_ve_dusuk_f0_plosive_degil)
{
    // tamamen sessiz → silence, voiced yok, olay yok; 90 Hz sesli ton (LF harmonik) → plosive DEĞİL
    const double fs = 48000.0;
    sweax::FeatureExtractor fe; fe.prepare (fs, 4096, 1024);
    sweax::PitchTracker pt; pt.prepare (fs);
    sweax::Vad vad; vad.prepare (fs, 1024);
    std::vector<float> z ((size_t) fs, 0.0f);
    int sil = 0, kare = 0, olay = 0;
    fe.push (z.data(), (int) z.size(), [&] (const sweax::FrameFeatures& f) { const auto v = vad.process (f, {}, [&] (const sweax::VadEvent&) { ++olay; }); ++kare; if (v.has (sweax::VAD_SILENCE)) ++sil; });
    SWX_BEKLE (sil == kare && olay == 0);
    vad.reset(); fe.reset(); pt.reset();
    std::vector<float> lo ((size_t) fs * 2);
    double faz = 0.0; std::mt19937 rng (2); std::normal_distribution<float> nd (0.0f, 0.001f);
    for (size_t i = 0; i < lo.size(); ++i)
    {
        const double t = (double) i / fs;
        if (t < 0.5) { lo[i] = nd (rng); continue; }
        faz += 2.0 * 3.14159265358979 * 90.0 / fs; float v = 0.0f; for (int h = 1; h <= 10; ++h) v += (float) (std::sin (h * faz) / h);
        lo[i] = 0.1f * v + nd (rng);
    }
    int plo = 0, voiced = 0;
    fe.push (lo.data(), (int) lo.size(), [&] (const sweax::FrameFeatures& f)
    {
        const long end = (long) std::llround (f.timeSec * fs);
        sweax::PitchTracker::Result pr; if (end >= pt.frameSize()) pr = pt.analyze (lo.data() + end - pt.frameSize(), pt.frameSize());
        const auto v = vad.process (f, pr, [&] (const sweax::VadEvent& e) { if (e.type == sweax::VadEventType::Plosive) ++plo; });
        if (v.has (sweax::VAD_VOICED)) ++voiced;
    });
    vad.flush ([&] (const sweax::VadEvent& e) { if (e.type == sweax::VadEventType::Plosive) ++plo; });
    SWX_BEKLE (voiced > 50);
    SWX_BEKLE_EQ (plo, 0);
}
