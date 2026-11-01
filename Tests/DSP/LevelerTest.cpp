// SWEAX NEXT — Tests/DSP/LevelerTest.cpp
// Ölçülen: farklı seviyeli cümleler hedefe ±1.5 dB; gürültü aralıklarında kazanç artışı ≤ 1 dB (hold);
// SNR sınırı (gürültü yakınsa yükseltme yok); latency sabit; RT tahsis yok.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "DSP/VocalLeveler.h"
#include "Analysis/Loudness.h"
#include "Chain/VocalChain.h"
#include "Core/RtGuard.h"
#include <vector>
#include <random>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    // sahne: [oda 0.5] [ton A 1.2 s] [oda 0.4] [ton B 1.2 s] [oda 0.4] ×3 ; oda = beyaz gürültü (odaDb RMS)
    struct Sahne { std::vector<float> x; std::vector<std::pair<int, int>> ton; std::vector<std::pair<int, int>> oda; };
    Sahne kur (double fs, float aDb, float bDb, float odaDb)
    {
        Sahne s; std::mt19937 rng (4); std::normal_distribution<float> nd (0.0f, std::pow (10.0f, odaDb / 20.0f));
        double faz = 0.0;
        auto oda = [&] (double sn) { const int a = (int) s.x.size(); for (size_t i = 0; i < (size_t) (sn * fs); ++i) s.x.push_back (nd (rng)); s.oda.push_back ({ a, (int) s.x.size() }); };
        auto ton = [&] (double sn, float db, double f0) { const int a = (int) s.x.size(); const float g = std::pow (10.0f, db / 20.0f) * 1.4142f; for (size_t i = 0; i < (size_t) (sn * fs); ++i) { faz += 2.0 * PI * f0 / fs; s.x.push_back (g * (float) std::sin (faz) + nd (rng)); } s.ton.push_back ({ a, (int) s.x.size() }); };
        for (int r = 0; r < 3; ++r) { oda (0.5); ton (1.2, aDb, 220.0); oda (0.4); ton (1.2, bDb, 330.0); oda (0.4); }
        return s;
    }
    // K-ağırlıksız yaklaşık: tonlar 220/330 Hz → K-filtre ≈ +0 dB civarı; ölçüm LoudnessMeter momentary ile
    float momentaryAt (const std::vector<float>& x, double fs, int end)
    {
        sweax::LoudnessMeter m; m.prepare (fs, 1, 4800);
        const int start = std::max (0, end - (int) (0.6 * fs));
        for (int i = start; i < end; i += 4800) { const float* ch[1] = { x.data() + i }; m.process (ch, 1, std::min (4800, end - i)); }
        return m.momentaryLufs();
    }
    void calistir (sweax::VocalLeveler& l, std::vector<float>& x) { const int n = (int) x.size(); for (int i = 0; i < n; i += 480) { float* ch[1] = { x.data() + i }; l.process (ch, 1, std::min (480, n - i)); } }
}

SWX_TEST (leveler_cumleler_hedefe_ve_hold)
{
    const double fs = 48000.0;
    sweax::VocalLeveler l; l.prepare (fs, 480, 1); l.setQuality (sweax::Quality::Yuksek); l.setEnabled (true, true);
    sweax::LevelerParams p; p.on = true; p.targetLufs = -24.0f; p.maxUpDb = 8.0f; p.maxDownDb = 8.0f; p.speed = 0.5f; l.setParams (p);
    // oda −70 dB: SNR ≈ 37 → maxUp sınırı (SNR − 20) 8 dB'yi kısmaz; SNR sınırı ayrı testte
    auto s = kur (fs, -30.0f, -18.0f, -70.0f);
    auto giris = s.x; calistir (l, s.x);
    const int gecikme = l.latencySamples (sweax::Quality::Yuksek);
    SWX_BEKLE_EQ (gecikme, 7200);
    // her tonun son 600 ms'i (gecikme dahil) hedefe ±1.5 dB (ilk cümle hariç: eğim sınırı 20 dB/s → 6 dB 0.3 s'de yetişir)
    for (size_t t = 1; t < s.ton.size(); ++t)
    {
        const int end = s.ton[t].second + gecikme;
        const float L = momentaryAt (s.x, fs, end), Lin = momentaryAt (giris, fs, s.ton[t].second);
        if (std::getenv ("SWX_VERBOSE")) std::printf ("   ton %zu: giris %.1f → cikis %.1f LUFS\n", t, Lin, L);
        SWX_BEKLE_YAKIN (L, -24.0f, 1.5f);
    }
    // oda aralıklarında kazanç değişimi ≤ 1 dB: giriş/çıkış oda enerjisi oranının aralık başı-sonu farkı
    for (size_t o = 2; o < s.oda.size(); ++o)
    {
        const int a = s.oda[o].first + gecikme + 2400, b = std::min ((int) s.x.size(), s.oda[o].second + gecikme - 2400);
        if (b - a < 4800) continue;
        auto rms = [&] (const std::vector<float>& v, int from, int to) { double e = 0.0; for (int i = from; i < to; ++i) e += (double) v[(size_t) i] * v[(size_t) i]; return 10.0 * std::log10 (e / (to - from) + 1e-20); };
        const double gBas = rms (s.x, a, a + 2400) - rms (giris, a - gecikme, a - gecikme + 2400);
        const double gSon = rms (s.x, b - 2400, b) - rms (giris, b - gecikme - 2400, b - gecikme);
        if (std::getenv ("SWX_VERBOSE")) std::printf ("   oda %zu: kazanç bas %.2f son %.2f dB\n", o, gBas, gSon);
        SWX_BEKLE (gSon - gBas <= 1.0);                                  // aralıkta yükselme yok
    }
    SWX_BEKLE (l.noiseFloorLufs() < -55.0f && l.noiseFloorLufs() > -85.0f);
}

SWX_TEST (leveler_snr_siniri_ve_rt)
{
    const double fs = 48000.0;
    sweax::VocalLeveler l; l.prepare (fs, 480, 2); l.setQuality (sweax::Quality::Ultra); l.setEnabled (true, true);
    sweax::LevelerParams p; p.on = true; p.targetLufs = -18.0f; p.maxUpDb = 10.0f; p.maxDownDb = 10.0f; l.setParams (p);
    // gürültü −38 dB, cümle −30: SNR ≈ 8 → maxUp ≤ SNR − 20 < 0 → yükseltme YOK
    auto s = kur (fs, -30.0f, -30.0f, -38.0f);
    auto giris = s.x; calistir (l, s.x);
    const int gecikme = l.latencySamples (sweax::Quality::Ultra);
    for (size_t t = 1; t < s.ton.size(); ++t)
    {
        const float L = momentaryAt (s.x, fs, s.ton[t].second + gecikme), Lin = momentaryAt (giris, fs, s.ton[t].second);
        SWX_BEKLE (L <= Lin + 1.0f);
    }
    // RT
    sweax::rt::resetViolations();
    std::vector<float> L2 (480, 0.1f), R2 (480, 0.1f);
    { sweax::rt::Scope sc; for (int k = 0; k < 300; ++k) { p.targetLufs = -20.0f + (float) (k % 5); l.setParams (p); float* ch[2] = { L2.data(), R2.data() }; l.process (ch, 2, 480); } }
    SWX_BEKLE_EQ (sweax::rt::violations(), 0u);
}
