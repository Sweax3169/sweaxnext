// SWEAX NEXT — Tests/DSP/OutputStageTest.cpp
// Ölçülen: sıcak sinyalde çıkış TRUE-PEAK ≤ tavan + 0.1 dB (örnekler arası tepeler dahil); eşik altı sinyal
// değişmez; latency tam; release ≥ 50 ms; RT tahsis yok.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "DSP/OutputStage.h"
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
    void calistir (sweax::OutputStage& o, std::vector<float>& L, std::vector<float>& R, int blok = 256)
    {
        const int n = (int) L.size();
        for (int i = 0; i < n; i += blok) { float* ch[2] = { L.data() + i, R.data() + i }; o.process (ch, 2, std::min (blok, n - i)); }
    }
    float truePeakDb (const std::vector<float>& x, double fs, int from)
    {
        sweax::LoudnessMeter m; m.prepare (fs, 1, 4096);
        for (int i = from; i < (int) x.size(); i += 4096) { const float* ch[1] = { x.data() + i }; m.process (ch, 1, std::min (4096, (int) x.size() - i)); }
        return m.truePeakDb();
    }
}

SWX_TEST (limiter_true_peak_tavan)
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        sweax::OutputStage o; o.prepare (fs, 256, 2); o.setEnabled (true, true);
        sweax::OutputParams p; p.ceilingDbTp = -1.0f; p.releaseMs = 100.0f; o.setParams (p);
        // sıcak karışım: +6 dBFS 1 kHz patlamaları + fs/4 sinüs faz π/4 (örnek tepeleri −3, TP 0) ×2 (+6 dBTP) + gürültü
        std::mt19937 rng (7); std::normal_distribution<float> nd (0.0f, 0.05f);
        std::vector<float> L ((size_t) fs * 2), R ((size_t) fs * 2);
        for (size_t i = 0; i < L.size(); ++i)
        {
            const double t = (double) i / fs;
            const bool burst = std::fmod (t, 0.25) < 0.1;
            float v = burst ? 2.0f * (float) std::sin (2.0 * PI * 1000.0 * t) : 0.3f * (float) std::sin (2.0 * PI * 200.0 * t);
            v += 2.0f * (float) std::sin (2.0 * PI * (fs / 4.0) * (double) i / fs + PI / 4.0) * (std::fmod (t, 0.5) > 0.3 ? 1.0f : 0.0f);
            L[i] = v + nd (rng); R[i] = 0.8f * v + nd (rng);
        }
        auto Lin = L;
        calistir (o, L, R);
        const float tp = truePeakDb (L, fs, (int) (0.1 * fs)), tpR = truePeakDb (R, fs, (int) (0.1 * fs));
        if (std::getenv ("SWX_VERBOSE")) std::printf ("   fs=%.0f: giris TP %.2f → cikis TP %.2f / %.2f dBTP, GR %.1f\n", fs, truePeakDb (Lin, fs, 0), tp, tpR, o.gainReductionDb());
        SWX_BEKLE (tp <= -1.0f + 0.1f);
        SWX_BEKLE (tpR <= -1.0f + 0.1f);
        SWX_BEKLE (tp > -3.0f);                                          // tavana yakın çalışıyor (aşırı bastırma yok)
    }
}

SWX_TEST (limiter_oversample_4x_gercek_tp)
{
    // YÜKSEK/ULTRA: 4× dahili — çıkış TP ≤ tavan + 0.05 dB (örnekler-arası dahil); gecikme L+18 tam; eşik altı şeffaf
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        sweax::OutputStage o; o.prepare (fs, 256, 2); o.setQuality (sweax::Quality::Yuksek); o.setEnabled (true, true);
        sweax::OutputParams p; p.ceilingDbTp = -1.0f; p.releaseMs = 100.0f; o.setParams (p);
        const int lookN = (int) std::lround (0.0015 * fs);
        SWX_BEKLE_EQ (o.latencySamples (sweax::Quality::Yuksek), lookN + 18);
        SWX_BEKLE_EQ (o.latencySamples (sweax::Quality::Ultra), lookN + 18);
        // sıcak karışım (taban yolu testiyle aynı): +6 dBFS patlamalar + fs/4 sinüs faz π/4 (TP > örnek tepe)
        std::mt19937 rng (7); std::normal_distribution<float> nd (0.0f, 0.05f);
        std::vector<float> L ((size_t) fs * 2), R ((size_t) fs * 2);
        for (size_t i = 0; i < L.size(); ++i)
        {
            const double t = (double) i / fs;
            const bool burst = std::fmod (t, 0.25) < 0.1;
            float v = burst ? 2.0f * (float) std::sin (2.0 * PI * 1000.0 * t) : 0.3f * (float) std::sin (2.0 * PI * 200.0 * t);
            v += 2.0f * (float) std::sin (2.0 * PI * (fs / 4.0) * (double) i / fs + PI / 4.0) * (std::fmod (t, 0.5) > 0.3 ? 1.0f : 0.0f);
            L[i] = v + nd (rng); R[i] = 0.8f * v + nd (rng);
        }
        calistir (o, L, R);
        const float tp = truePeakDb (L, fs, (int) (0.1 * fs)), tpR = truePeakDb (R, fs, (int) (0.1 * fs));
        if (std::getenv ("SWX_VERBOSE")) std::printf ("   4x fs=%.0f: cikis TP %.3f / %.3f dBTP, GR %.1f\n", fs, tp, tpR, o.gainReductionDb());
        SWX_BEKLE (tp <= -1.0f + 0.05f);
        SWX_BEKLE (tpR <= -1.0f + 0.05f);
        SWX_BEKLE (tp > -3.0f);
        // eşik altı şeffaflık: −6 dBFS sinüs, gecikme sonrası fark küçük (yarım-bant dalgalanma payı ≤ 0.05 dB)
        sweax::OutputStage o2; o2.prepare (fs, 256, 2); o2.setQuality (sweax::Quality::Ultra); o2.setEnabled (true, true);
        o2.setParams (p);
        std::vector<float> A ((size_t) fs), B ((size_t) fs);
        for (size_t i = 0; i < A.size(); ++i) { A[i] = 0.5f * (float) std::sin (2.0 * PI * 1000.0 * (double) i / fs); B[i] = A[i]; }
        auto Ain = A;
        calistir (o2, A, B);
        const int D = o2.latencySamples (sweax::Quality::Ultra);
        double rmsErr = 0.0; int cnt = 0;
        for (int i = 4000; i < (int) A.size(); ++i) { const double e = A[(size_t) i] - Ain[(size_t) (i - D)]; rmsErr += e * e; ++cnt; }
        const double errDb = 10.0 * std::log10 (rmsErr / cnt / (0.5 * 0.5 * 0.5) + 1e-20);
        if (std::getenv ("SWX_VERBOSE")) std::printf ("   4x fs=%.0f seffaflik hata %.1f dB\n", fs, errDb);
        SWX_BEKLE (errDb < -50.0);                                       // yarım-bant zinciri artığı
    }
}

SWX_TEST (limiter_esik_alti_latency_release_rt)
{
    const double fs = 48000.0;
    sweax::OutputStage o; o.prepare (fs, 256, 2); o.setEnabled (true, true);
    sweax::OutputParams p; p.ceilingDbTp = -0.3f; p.releaseMs = 50.0f; o.setParams (p);
    const int D = o.latencySamples (sweax::Quality::Canli);
    SWX_BEKLE_EQ (D, 72 - 1 + 7);                                      // (L−1) + TP tahmin gecikmesi
    // eşik altı: −6 dBFS sinüs değişmez (gecikme hariç)
    std::vector<float> L ((size_t) fs), R ((size_t) fs);
    for (size_t i = 0; i < L.size(); ++i) { L[i] = 0.5f * (float) std::sin (2.0 * PI * 1000.0 * (double) i / fs); R[i] = L[i]; }
    auto Lin = L; calistir (o, L, R);
    double err = 0.0; for (int i = 1000; i < (int) L.size(); ++i) err = std::max (err, (double) std::fabs (L[(size_t) i] - Lin[(size_t) i - D]));
    SWX_BEKLE (err < 1e-4);
    // dürtü gecikmesi
    o.reset();
    std::vector<float> M ((size_t) 2048, 0.0f), N = M; M[100] = 0.5f; N[100] = 0.5f;
    calistir (o, M, N);
    int at = -1; for (int i = 0; i < 2048; ++i) if (std::fabs (M[(size_t) i]) > 0.25f) { at = i; break; }
    SWX_BEKLE_EQ (at, 100 + D);
    // release: +6 dB patlama sonrası kazanç 63 % toparlanma ≥ 50 ms (hold 10 ms dahil ≥ 60 ms)
    o.reset();
    std::vector<float> A ((size_t) fs, 0.0f), B ((size_t) fs, 0.0f);
    for (size_t i = 0; i < (size_t) (0.2 * fs); ++i) { A[i] = 2.0f * (float) std::sin (2.0 * PI * 1000.0 * (double) i / fs); B[i] = A[i]; }
    for (size_t i = (size_t) (0.2 * fs); i < A.size(); ++i) { A[i] = 0.1f * (float) std::sin (2.0 * PI * 1000.0 * (double) i / fs); B[i] = A[i]; }
    int i63 = -1; const int adim = 32; float grBas = 0.0f;
    for (int i = 0; i < (int) A.size(); i += adim)
    {
        float* ch[2] = { A.data() + i, B.data() + i }; o.process (ch, 2, adim);
        const int t = i + adim - (int) (0.2 * fs) - D;
        if (t == 0 || (t > 0 && grBas == 0.0f)) grBas = o.gainReductionDb();
        if (t > 0 && i63 < 0 && o.gainReductionDb() > grBas * 0.37f) i63 = t;
    }
    if (std::getenv ("SWX_VERBOSE")) std::printf ("   release: GR bas %.1f dB, 63%% toparlanma %d ornek (%.1f ms)\n", grBas, i63, i63 / fs * 1000.0);
    SWX_BEKLE (grBas < -5.0f);
    SWX_BEKLE (i63 >= (int) (0.055 * fs));
    // RT
    sweax::rt::resetViolations();
    { sweax::rt::Scope s; for (int k = 0; k < 200; ++k) { p.ceilingDbTp = -0.3f - (float) (k % 5); o.setParams (p); float* ch[2] = { A.data(), B.data() }; o.process (ch, 2, 256); } }
    SWX_BEKLE_EQ (sweax::rt::violations(), 0u);
}
