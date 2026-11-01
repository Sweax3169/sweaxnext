// SWEAX NEXT — Tests/Decision/FeedbackTest.cpp
// Geri besleme: yetersiz ilk kesim → tur(lar) derinleştirir → skor düşer; yayınlanan en iyi tur; determinizm;
// sandbox render plana sadık (EQ kesimi ölçülür); A/B kazancı = işlenmiş − kuru gate'li LUFS (klempli).
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "Analysis/Offline.h"
#include "Decision/Feedback.h"
#include "Decision/Sandbox.h"
#include "Core/Biquad.h"
#include "Analysis/Loudness.h"
#include <random>
#include <cstdio>
#include <cstdlib>

namespace
{
    using namespace sweax;
    using namespace sweax::decision;
    constexpr double PI = 3.14159265358979323846;

    // 400 Hz +12 dB tümsekli vokal (kasıtlı büyük: plan_0'ın ≤6 dB tek kesimi yetmesin → feedback derinleştirsin)
    std::vector<float> sahne (double fs, float mudDb, unsigned tohum = 5)
    {
        std::vector<float> x; std::mt19937 rng (tohum); std::normal_distribution<float> nd (0.0f, 0.001f);
        double faz = 0.0;
        Biquad mud; mud.setCoeffs (design::bellMatched (fs, 400.0, mudDb, 0.9));
        auto oda = [&] (double sn) { for (size_t i = 0; i < (size_t) (sn * fs); ++i) x.push_back (nd (rng)); };
        auto ton = [&] (double sn, double f0, float g)
        {
            const size_t N = (size_t) (sn * fs), fade = (size_t) (0.02 * fs);
            for (size_t i = 0; i < N; ++i)
            {
                const double fNow = f0 * std::pow (2.0, 0.1 * std::sin (2.0 * PI * 0.8 * (double) x.size() / fs));
                faz += 2.0 * PI * fNow / fs; double s = 0.0;
                for (int h = 1; h <= 20; ++h) { const double fh = fNow * h; if (fh > fs * 0.45) break; const double fmt = 1.0 + 1.5 * std::exp (-std::pow ((fh - 700.0) / 300.0, 2.0)); s += fmt * std::sin (h * faz) / h; }
                const float env = (float) std::min (1.0, std::min ((double) i / fade, (double) (N - 1 - i) / fade));
                x.push_back (mud.processSample (g * (float) s * 0.08f * env) + nd (rng));   // kırpılmasın (+12 dB tümsekle bile)
            }
        };
        for (int k = 0; k < 3; ++k) { oda (0.5); ton (1.1, 140.0, 1.0f); ton (0.9, 200.0, 0.85f); oda (0.4); ton (1.0, 165.0, 0.9f); oda (0.4); }
        return x;
    }
}

SWX_TEST (feedback_derinlestirir_ve_en_iyi_turu_secer)
{
    const double fs = 48000.0;
    auto x = sahne (fs, 12.0f);
    auto rin = offline::reportFromBuffer (x.data(), (int64_t) x.size(), fs);
    SWX_BEKLE (rin.sentences >= 4);
    PlannerInput in; in.report = &rin; in.detections = &rin.detections; in.role = state::Role::Lead; in.planId = 2;
    const auto plan0 = Planner::plan (in);
    if (std::getenv ("SWX_VERBOSE"))
    {
        std::printf ("   giris: voicedLufs=%.1f cumle=%d\n", rin.voicedLufs, rin.sentences);
        for (const auto& dd : rin.detections) std::printf ("   det %2d p=%.2f s=%.2f c=%.2f hz=%.0f val=%.1f\n", (int) dd.type, dd.probability, dd.severity, dd.confidence, dd.centerHz, dd.valueDb);
        const auto* eq0 = plan0.module (state::modul::EQ);
        if (eq0 != nullptr) for (const auto& pv : eq0->params) std::printf ("   eq param %d = %.2f\n", (int) pv.id, pv.raw);
    }
    auto fb = Feedback::optimize (in, x.data(), (int64_t) x.size(), fs, 30.0, 3);   // test: geniş bütçe
    if (std::getenv ("SWX_VERBOSE"))
        std::printf ("   tur=%d secilen=%d skorlar: %.3f %.3f %.3f | LUFS islenmis=%.1f kuru=%.1f\n",
                     fb.rounds, fb.chosenRound, fb.scores[0], fb.scores[1], fb.scores[2], fb.processedLufs, rin.integratedLufs);
    SWX_BEKLE (fb.rounds >= 2);                                        // kalıntı vardı → en az bir ayar turu
    SWX_BEKLE (fb.scores[fb.chosenRound] <= fb.scores[0] + 1e-6f);     // yayınlanan ≤ plan_0 skoru (en iyi tur)
    // seçilen planın 250–640 kesim derinliği plan_0'dan sığ DEĞİL
    auto derinlik = [] (const MixPlan& p) -> float
    {
        const auto* eq = p.module (state::modul::EQ);
        float g = 0.0f;
        if (eq == nullptr) return g;
        for (int b = 0; b < params::EQ_BANT_SAYISI; ++b)
        {
            float hz = -1.0f, gain = 0.0f; bool on = false;
            for (const auto& pv : eq->params)
            {
                if (pv.id == params::eqBant (b, 0)) on = pv.raw > 0.5f;
                if (pv.id == params::eqBant (b, 2)) hz = pv.raw;
                if (pv.id == params::eqBant (b, 3)) gain = pv.raw;
            }
            if (on && hz > 250.0f && hz < 640.0f) g = std::min (g, gain);
        }
        return g;
    };
    SWX_BEKLE (derinlik (fb.finalPlan) <= derinlik (plan0) + 1e-4f);
    SWX_BEKLE (derinlik (fb.finalPlan) >= -6.01f);                     // guardrail korunur
    SWX_BEKLE (std::isfinite (fb.processedLufs) && fb.processedLufs > -60.0f);
    // determinizm
    auto fb2 = Feedback::optimize (in, x.data(), (int64_t) x.size(), fs, 30.0, 3);
    SWX_BEKLE_EQ (fb2.chosenRound, fb.chosenRound);
    SWX_BEKLE (fb2.scores[fb2.chosenRound] == fb.scores[fb.chosenRound]);
    SWX_BEKLE_YAKIN (derinlik (fb2.finalPlan), derinlik (fb.finalPlan), 1e-6f);
    // sandbox sadakati: final planın render'ında 400/2k oranı girişten belirgin düşük
    std::vector<float> out (x.size());
    Sandbox::render (fb.finalPlan, x.data(), out.data(), (int64_t) x.size(), fs);
    auto bantDb = [&] (const std::vector<float>& v, double hz)
    {
        Biquad b1, b2; b1.setCoeffs (design::bandpass (fs, hz, 4.0)); b2.setCoeffs (design::bandpass (fs, hz, 4.0));
        double e = 0.0; int cnt = 0;
        for (size_t i = 0; i < v.size(); ++i) { const float s = b2.processSample (b1.processSample (v[i])); if (i > v.size() / 4) { e += (double) s * s; ++cnt; } }
        return 10.0 * std::log10 (e / std::max (1, cnt) + 1e-20);
    };
    const double once = bantDb (x, 400.0) - bantDb (x, 2000.0);
    const double sonra = bantDb (out, 400.0) - bantDb (out, 2000.0);
    if (std::getenv ("SWX_VERBOSE")) std::printf ("   400/2k: giris %.1f dB → render %.1f dB\n", once, sonra);
    SWX_BEKLE (once - sonra >= 3.0);
}

SWX_TEST (feedback_es_ses_stereo_ve_formant_stil_bandi)
{
    // M3.4 denetim DSP-1/T2: eş-ses hedefi DAW ölçerinin okuduğu KANAL-TOPLAMLI stereo LUFS (mono ölçüm 3 dB düşük okuyordu)
    // PT-6: formant bölgesindeki stil presence bandı (Killoki 2.5k) Feedback'te statik KESİME çevrilmez
    const double fs = 48000.0;
    auto x = sahne (fs, 6.0f);
    {
        Biquad reso; reso.setCoeffs (design::bellMatched (fs, 2800.0, 12.0, 6.0));   // formant bölgesinde dar rezonans
        for (auto& v : x) v = reso.processSample (v);
    }
    auto rin = offline::reportFromBuffer (x.data(), (int64_t) x.size(), fs);
    for (int stil : { 1, 4 })
    {
        PlannerInput in; in.report = &rin; in.detections = &rin.detections; in.role = state::Role::Lead; in.planId = 3; in.style = stil;
        in.outChannels = 2;
        auto fb = Feedback::optimize (in, x.data(), (int64_t) x.size(), fs, 30.0, 3);
        std::vector<float> L (x.size()), R (x.size());
        Sandbox::renderStereo (fb.finalPlan, x.data(), L.data(), R.data(), (int64_t) x.size(), fs, Quality::Yuksek, 120.0, true);
        LoudnessMeter lm; lm.prepare (fs, 2, 8192);
        for (size_t i = 0; i < x.size(); i += 8192)
        {
            const float* ch2[2] = { L.data() + i, R.data() + i };
            lm.process (ch2, 2, (int) std::min<size_t> (8192, x.size() - i));
        }
        const float stereo = lm.integratedLufs();
        if (std::getenv ("SWX_VERBOSE")) std::printf ("   stil %d: stereo LUFS %.2f (hedef ~-19±1), tur %d\n", stil, stereo, fb.rounds);
        SWX_BEKLE (stereo >= -20.6f && stereo <= -17.4f);
        if (stil == 4)
        {
            const auto* eq = fb.finalPlan.module (state::modul::EQ);
            bool formantKesim = false;
            for (int b = 0; eq != nullptr && b < params::EQ_BANT_SAYISI; ++b)
            {
                float on = 0, hz = 0, g = 0, dyn = 0;
                for (const auto& pv : eq->params)
                {
                    if (pv.id == params::eqBant (b, 0)) on = pv.raw;
                    if (pv.id == params::eqBant (b, 2)) hz = pv.raw;
                    if (pv.id == params::eqBant (b, 3)) g = pv.raw;
                    if (pv.id == params::eqBant (b, 5)) dyn = pv.raw;
                }
                if (on > 0.5f && dyn < 0.5f && Planner::inFormantZone (hz) && g < -0.05f) formantKesim = true;
            }
            SWX_BEKLE (! formantKesim);
        }
    }
}
