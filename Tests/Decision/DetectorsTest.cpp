// SWEAX NEXT — Tests/Decision/DetectorsTest.cpp
// Sentetik "vokal" sahneleri AnalysisEngine'den geçirilir, rapor + kareler dedektörlere verilir:
//  temiz sahne → tonal/rezonans/sibilans sorunları düşük olasılık;  400 Hz +9 dB tümsek → Mud/Box;
//  dar 1.1 kHz rezonans → Resonance merkez ±3 %;  sibilant patlamaları → Sibilance;  50 Hz hum → Hum;
//  seviye farklı cümleler → MacroDynamics.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "Analysis/AnalysisEngine.h"
#include "Analysis/Report.h"
#include "Decision/Detectors.h"
#include "Core/Biquad.h"
#include <vector>
#include <random>
#include <cstdio>
#include <cstdlib>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    struct Ayar { float mudDb = 0.0f; float resDb = 0.0f; double resHz = 1100.0; bool sib = false; float humDb = -200.0f; float dynDb = 0.0f; unsigned tohum = 3; };

    // 6 cümle (≈16 s): harmonik "vokal" (F0 140/200, 1/h + formant zarfı) + oda gürültüsü; isteğe bağlı bozukluklar
    std::vector<float> sahne (double fs, const Ayar& a)
    {
        std::vector<float> x; std::mt19937 rng (a.tohum); std::normal_distribution<float> nd (0.0f, 0.001f);
        double faz = 0.0;
        sweax::Biquad mud, res, hp5, hp6, lp8, lp9;
        mud.setCoeffs (sweax::design::bellMatched (fs, 400.0, a.mudDb, 0.8));
        res.setCoeffs (sweax::design::bellMatched (fs, a.resHz, a.resDb, 6.0));
        hp5.setCoeffs (sweax::design::highpass (fs, 5000.0, 0.7071)); hp6.setCoeffs (sweax::design::highpass (fs, 5000.0, 0.7071));
        lp8.setCoeffs (sweax::design::lowpass (fs, 9000.0, 0.7071)); lp9.setCoeffs (sweax::design::lowpass (fs, 9000.0, 0.7071));
        auto oda = [&] (double sn) { for (size_t i = 0; i < (size_t) (sn * fs); ++i) { float v = nd (rng); if (a.humDb > -100.0f) v += std::pow (10.0f, a.humDb / 20.0f) * (float) std::sin (2.0 * PI * 50.0 * (double) x.size() / fs); x.push_back (v); } };
        auto ton = [&] (double sn, double f0, float g)
        {
            const size_t N = (size_t) (sn * fs), fade = (size_t) (0.02 * fs);       // 20 ms giriş/çıkış (doğal onset)
            for (size_t i = 0; i < N; ++i)
            {
                // perde kayması ±8 % (0.7 Hz): gerçek sesin harmonikleri rezonansı sürekli tarar, sabit perde taraması yapmaz
                const double fNow = f0 * std::pow (2.0, 0.11 * std::sin (2.0 * PI * 0.7 * (double) x.size() / fs));
                faz += 2.0 * PI * fNow / fs; double s = 0.0;
                for (int h = 1; h <= 24; ++h) { const double fh = f0 * h; if (fh > fs * 0.45) break; const double formant = 1.0 + 1.5 * std::exp (-std::pow ((fh - 700.0) / 300.0, 2.0)) + 0.8 * std::exp (-std::pow ((fh - 2500.0) / 600.0, 2.0)); s += formant * std::sin (h * faz) / h; }
                const float env = (float) std::min (1.0, std::min ((double) i / fade, (double) (N - 1 - i) / fade));
                float v = g * (float) s * 0.25f * env;
                v = res.processSample (mud.processSample (v));
                if (a.humDb > -100.0f) v += std::pow (10.0f, a.humDb / 20.0f) * (float) std::sin (2.0 * PI * 50.0 * (double) x.size() / fs);
                x.push_back (v + nd (rng));
            }
        };
        auto sib = [&] (double sn) { for (size_t i = 0; i < (size_t) (sn * fs); ++i) x.push_back (0.05f * lp9.processSample (lp8.processSample (hp6.processSample (hp5.processSample (nd (rng) * 1000.0f)))) + nd (rng)); };
        for (int k = 0; k < 3; ++k)
        {
            const float gA = std::pow (10.0f, (a.dynDb * ((k % 2) ? 1.0f : 0.0f)) / 20.0f);
            oda (0.5); ton (1.2, 140.0, 1.0f * gA); if (a.sib) sib (0.08); ton (0.9, 200.0, 0.8f); oda (0.4);
            ton (1.0, 165.0, 0.9f / gA); if (a.sib) sib (0.07); ton (1.1, 220.0, 0.7f); oda (0.5);
        }
        return x;
    }
    struct Sonuc { std::shared_ptr<const sweax::AnalysisReport> r; std::vector<sweax::decision::ProblemDetection> d; };
    Sonuc calistir (const Ayar& a, double fs = 48000.0)
    {
        sweax::AnalysisEngine e; e.prepare (fs, 512); e.setListening (true);
        auto x = sahne (fs, a);
        for (size_t i = 0; i < x.size(); i += 512) { const int n = (int) std::min<size_t> (512, x.size() - i); const float* ch[1] = { x.data() + i }; e.pushAudio (ch, 1, n, (int64_t) i, true); if ((i / 512) % 32 == 0) e.processPendingSync(); }
        e.processPendingSync(); e.requestReport();
        Sonuc s; s.r = e.latestReport();
        sweax::decision::DetectorContext ctx; ctx.role = sweax::state::Role::Lead;
        s.d = sweax::decision::Detectors::detect (*s.r, e.segmentAnalyses(), ctx);
        if (std::getenv ("SWX_VERBOSE"))
            for (const auto& p : s.d) std::printf ("   sorun %2d p=%.2f s=%.2f c=%.2f hz=%.0f bw=%.2f val=%.1f n=%d\n", (int) p.type, p.probability, p.severity, p.confidence, p.centerHz, p.bandwidthOct, p.valueDb, p.count);
        return s;
    }
    float pOf (const std::vector<sweax::decision::ProblemDetection>& v, sweax::decision::Problem p) { auto* d = sweax::decision::Detectors::find (v, p); return d ? d->probability : 0.0f; }
}

SWX_TEST (detectors_temiz_sahne_sorun_yok)
{
    using namespace sweax::decision;
    auto s = calistir (Ayar {});
    SWX_BEKLE (s.r->sentences >= 6);                                    // bitişik tonlar tek cümle: 3 tekrar × 2
    SWX_BEKLE (pOf (s.d, Problem::Mud) < 0.5f);
    SWX_BEKLE (pOf (s.d, Problem::Plosive) < 0.5f);
    SWX_BEKLE (pOf (s.d, Problem::Resonance) < 0.5f);
    SWX_BEKLE (pOf (s.d, Problem::Sibilance) < 0.3f);
    SWX_BEKLE (pOf (s.d, Problem::Hum) < 0.3f);
    SWX_BEKLE (pOf (s.d, Problem::Harsh) < 0.5f);
    SWX_BEKLE (pOf (s.d, Problem::Clipping) < 0.1f);
    // sibilans dedektörü kanıt bildirir: olay yok ama güven yüksek → "Gerekmedi" için p≈0, c yüksek
    auto* sib = Detectors::find (s.d, Problem::Sibilance);
    SWX_BEKLE (sib != nullptr && sib->probability < 0.1f && sib->confidence > 0.9f);
}

SWX_TEST (detectors_mud_rezonans_sibilans_hum_dinamik)
{
    using namespace sweax::decision;
    // mud: 400 Hz +9 dB, Q 0.8
    { Ayar a; a.mudDb = 9.0f; auto s = calistir (a);
      SWX_BEKLE (pOf (s.d, Problem::Mud) > 0.6f || pOf (s.d, Problem::Box) > 0.6f);
      auto* m = Detectors::find (s.d, Problem::Mud); if (m) SWX_BEKLE (m->severity > 0.3f && m->confidence > 0.7f); }
    // rezonans: 1.1 kHz +10 dB Q 6 → merkez ±3 %
    { Ayar a; a.resDb = 10.0f; a.resHz = 1100.0; auto s = calistir (a);
      if (std::getenv ("SWX_VERBOSE")) { std::printf ("   LTAS 700 Hz–1.8 kHz:"); for (int b = 62; b < 78; ++b) std::printf (" %.0f:%.1f", Detectors::bandHz (b), s.r->ltasDb[b]); std::printf ("  tonal=%d\n", s.r->tonalFrames); }
      const ProblemDetection* rz = nullptr;
      for (const auto& d : s.d) if (d.type == Problem::Resonance && (rz == nullptr || std::fabs (d.centerHz - 1100.0f) < std::fabs (rz->centerHz - 1100.0f))) rz = &d;
      SWX_BEKLE (rz != nullptr && rz->probability > 0.5f);
      if (rz) { SWX_BEKLE (std::fabs (rz->centerHz - 1100.0f) / 1100.0f < 0.06f); SWX_BEKLE (rz->bandwidthOct < 0.6f); } }
    // sibilans: her cümlede 's' patlaması → olay/dk yüksek
    { Ayar a; a.sib = true; auto s = calistir (a);
      SWX_BEKLE (s.r->sibilantEvents >= 4);
      SWX_BEKLE (pOf (s.d, Problem::Sibilance) > 0.5f); }
    // hum: 50 Hz −40 dBFS (oda gürültüsü −60) → Hum
    { Ayar a; a.humDb = -40.0f; auto s = calistir (a);
      SWX_BEKLE (pOf (s.d, Problem::Hum) > 0.5f);
      auto* h = Detectors::find (s.d, Problem::Hum); if (h) SWX_BEKLE (h->centerHz > 45.0f && h->centerHz < 160.0f); }
    // makro dinamik: cümleler ±6 dB dalgalı → MacroDynamics
    { Ayar a; a.dynDb = 8.0f; auto s = calistir (a);
      SWX_BEKLE (pOf (s.d, Problem::MacroDynamics) > 0.6f);
      Ayar b; auto t = calistir (b);
      SWX_BEKLE (pOf (t.d, Problem::MacroDynamics) < pOf (s.d, Problem::MacroDynamics)); }
}
