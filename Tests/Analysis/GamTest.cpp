// SWEAX NEXT — Tests/Analysis/GamTest.cpp
// Ölçülen (M3.1 gam algılama): tonik-ağırlıklı LA minör melodisi › keyIndex 9 + minör + güven ≥ 0.5 ve
// Planner tune'u o gama KİLİTLER; DO majör melodisi › majör kilidi; kısa/yetersiz materyal › güven yok,
// tune KROMATİK kalır (yanlış gama kilitlemekten dürüst kaçınma).
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "Analysis/AnalysisEngine.h"
#include "Analysis/Report.h"
#include "Decision/Planner.h"
#include <vector>
#include <random>
#include <cmath>

namespace
{
    constexpr double PI = 3.14159265358979323846;

    // nota dizisinden harmonikli "vokal" sahnesi (VAD'in sesli sayması için 8 harmonik + hafif oda gürültüsü)
    std::vector<float> melodi (double fs, const std::vector<double>& notalarHz, double notaSn)
    {
        std::vector<float> x; std::mt19937 rng (7); std::normal_distribution<float> nd (0.0f, 0.001f);
        for (size_t i = 0; i < (size_t) (0.4 * fs); ++i) x.push_back (nd (rng));          // oda
        double faz = 0.0;
        for (double f0 : notalarHz)
            for (size_t i = 0; i < (size_t) (notaSn * fs); ++i)
            {
                faz += 2.0 * PI * f0 / fs;
                float v = 0.0f; for (int h = 1; h <= 8; ++h) v += (float) (std::sin (h * faz) / h);
                x.push_back (0.08f * v + nd (rng));
            }
        for (size_t i = 0; i < (size_t) (0.4 * fs); ++i) x.push_back (nd (rng));
        return x;
    }
    void bas (sweax::AnalysisEngine& e, const std::vector<float>& x)
    {
        for (size_t i = 0; i < x.size(); i += 512)
        {
            const int n = (int) std::min<size_t> (512, x.size() - i);
            const float* ch[1] = { x.data() + i };
            e.pushAudio (ch, 1, n, (int64_t) i, true);
            if ((i / 512) % 32 == 0) e.processPendingSync();
        }
        e.processPendingSync();
    }
    std::shared_ptr<const sweax::AnalysisReport> raporla (const std::vector<float>& x, double fs)
    {
        sweax::AnalysisEngine e; e.prepare (fs, 512); e.setListening (true);
        bas (e, x);
        e.setListening (false);
        e.requestReport();
        e.processPendingSync();
        return e.latestReport();
    }
    float planParam (const sweax::AnalysisReport& r, sweax::params::Id id)
    {
        using namespace sweax::decision;
        PlannerInput in; in.report = &r; in.detections = &r.detections;
        in.role = sweax::state::Role::Lead; in.planId = 21; in.style = 1;
        const auto plan = Planner::plan (in);
        const auto* m = plan.module (sweax::state::modul::TUNE);
        for (const auto& p : m->params) if (p.id == id) return p.raw;
        return -999.0f;
    }
}

SWX_TEST (gam_algilama_ve_tune_kilidi)
{
    const double fs = 48000.0;
    // LA minör, tonik ağırlıklı (A2/A3 sık): A2 110, B2 123.47, C3 130.81, D3 146.83, E3 164.81, G3 196, A3 220
    const std::vector<double> laMinor = { 110.0, 130.81, 164.81, 110.0, 196.0, 110.0, 146.83, 164.81,
                                          110.0, 123.47, 130.81, 110.0, 164.81, 220.0, 110.0, 110.0 };
    auto r1 = raporla (melodi (fs, laMinor, 0.5), fs);
    SWX_BEKLE (r1 != nullptr);
    SWX_BEKLE_EQ (r1->keyIndex, 9);                       // A
    SWX_BEKLE (r1->keyMinor);
    SWX_BEKLE (r1->keyConfidence >= 0.5f);
    SWX_BEKLE_YAKIN (planParam (*r1, sweax::params::TuneKey), 9.0f, 0.01f);
    SWX_BEKLE_YAKIN (planParam (*r1, sweax::params::TuneScale), 2.0f, 0.01f);   // minör

    // SOL majör, tonik ağırlıklı (denetim K-15: DO majörde TuneKey=0 kromatik yedeğinden ayırt edilemiyordu):
    // G2 98, A2 110, B2 123.47, C3 130.81, D3 146.83, E3 164.81, F#3 185, G3 196
    const std::vector<double> solMajor = { 98.0, 123.47, 146.83, 98.0, 196.0, 98.0, 110.0, 123.47,
                                           98.0, 185.0, 146.83, 98.0, 130.81, 123.47, 98.0, 98.0 };
    auto r2 = raporla (melodi (fs, solMajor, 0.5), fs);
    SWX_BEKLE (r2 != nullptr);
    SWX_BEKLE_EQ (r2->keyIndex, 7);                       // G
    SWX_BEKLE (! r2->keyMinor);
    SWX_BEKLE (r2->keyConfidence >= 0.5f);
    SWX_BEKLE_YAKIN (planParam (*r2, sweax::params::TuneKey), 7.0f, 0.01f);     // anahtar plana taşındı
    SWX_BEKLE_YAKIN (planParam (*r2, sweax::params::TuneScale), 1.0f, 0.01f);   // majör

    // atonal materyal (12 yarım-ton eşit süre, düz histogram): güvenilir gam YOK › kromatik
    std::vector<double> atonal;
    for (int k : { 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5 })
        atonal.push_back (110.0 * std::pow (2.0, k / 12.0));
    auto r3 = raporla (melodi (fs, atonal, 0.5), fs);
    SWX_BEKLE (r3 != nullptr);
    SWX_BEKLE (r3->keyIndex < 0 || r3->keyConfidence < 0.5f);
    SWX_BEKLE_YAKIN (planParam (*r3, sweax::params::TuneScale), 0.0f, 0.01f);   // kromatik
}
