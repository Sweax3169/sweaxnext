// SWEAX NEXT — Tests/DSP/CompressorTest.cpp
// Ölçülen: GR doğruluğu (peak dedektör, oran, knee), 100 Hz THD < %1 en hızlı ayarda (hold guardrail), lookahead
// latency sabit ve tam örnek, mix paralel, attack guardrail (CANLI'da <1 ms klemp), RT tahsis yok.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "DSP/Compressor.h"
#include "Chain/VocalChain.h"
#include "Core/RtGuard.h"
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    void calistir (sweax::Compressor& c, std::vector<float>& x, int blok = 256)
    {
        const int n = (int) x.size();
        for (int i = 0; i < n; i += blok) { float* ch[1] = { x.data() + i }; c.process (ch, 1, std::min (blok, n - i)); }
    }
    double tepeDb (const std::vector<float>& x, int a, int b) { float pk = 0.0f; for (int i = a; i < b; ++i) pk = std::max (pk, std::fabs (x[(size_t) i])); return 20.0 * std::log10 (pk + 1e-12); }
    // THD: 1. harmonik en küçük kareler genliği; artık RMS / temel RMS
    double thdYuzde (const std::vector<float>& y, double fs, double hz, int a, int b)
    {
        double sc = 0.0, ss = 0.0, cc = 0.0;
        for (int i = a; i < b; ++i) { const double c = std::cos (2.0 * PI * hz * i / fs), s = std::sin (2.0 * PI * hz * i / fs); sc += y[(size_t) i] * c; ss += y[(size_t) i] * s; cc += c * c; }
        const double A = sc / cc, B = ss / cc;
        double res = 0.0, fund = 0.0;
        for (int i = a; i < b; ++i)
        {
            const double f = A * std::cos (2.0 * PI * hz * i / fs) + B * std::sin (2.0 * PI * hz * i / fs);
            res += (y[(size_t) i] - f) * (y[(size_t) i] - f); fund += f * f;
        }
        return 100.0 * std::sqrt (res / std::max (fund, 1e-20));
    }
}

SWX_TEST (comp_gr_dogrulugu_ve_mix)
{
    const double fs = 48000.0;
    sweax::Compressor c; c.prepare (fs, 256, 1); c.setQuality (sweax::Quality::Yuksek); c.setEnabled (true, true);
    sweax::CompParams p; p.on = true; p.threshDb = -18.0f; p.ratio = 4.0f; p.kneeDb = 0.0f; p.attackMs = 1.0f; p.releaseMs = 100.0f;
    p.lookaheadMs = 0.0f; p.detector = 0; p.mix = 1.0f; p.scHpfHz = 20.0f; c.setParams (p);
    // 1 kHz −6 dBFS tepe → over 12 dB → GR 9 dB → çıkış tepe −15 dBFS
    std::vector<float> x ((size_t) fs);
    for (size_t i = 0; i < x.size(); ++i) x[i] = 0.5f * (float) std::sin (2.0 * PI * 1000.0 * (double) i / fs);
    calistir (c, x);
    SWX_BEKLE_YAKIN (tepeDb (x, (int) fs / 2, (int) fs), -15.0, 0.4);
    SWX_BEKLE_YAKIN (c.gainReductionDb(), -9.0, 0.4);
    // knee 12 dB: over 12 → knee içinde değil (≥ knee/2) → aynı; over 3 dB (−15 dBFS giriş) → knee içinde GR < 3·0.75
    p.kneeDb = 12.0f; c.setParams (p);
    std::vector<float> y ((size_t) fs);
    for (size_t i = 0; i < y.size(); ++i) y[i] = std::pow (10.0f, -15.0f / 20.0f) * (float) std::sin (2.0 * PI * 1000.0 * (double) i / fs);
    calistir (c, y);
    const double grKnee = -c.gainReductionDb();
    SWX_BEKLE_YAKIN (grKnee, 2.53, 0.3);                              // soft knee: slope·(over+knee/2)²/(2·knee) = 0.75·81/24 = 2.53
    // mix 0.5: paralel — çıkış = 0.5·wet + 0.5·dry → tepe (0.5·10^(−15/20) + 0.5·0.5)
    p.kneeDb = 0.0f; p.mix = 0.5f; c.setParams (p);
    std::vector<float> z ((size_t) fs);
    for (size_t i = 0; i < z.size(); ++i) z[i] = 0.5f * (float) std::sin (2.0 * PI * 1000.0 * (double) i / fs);
    calistir (c, z);
    SWX_BEKLE_YAKIN (tepeDb (z, (int) fs / 2, (int) fs), 20.0 * std::log10 (0.5 * std::pow (10.0, -15.0 / 20.0) + 0.25), 0.4);
    // makeup +6 → tepe +6
    p.mix = 1.0f; p.makeupDb = 6.0f; c.setParams (p);
    std::vector<float> w ((size_t) fs);
    for (size_t i = 0; i < w.size(); ++i) w[i] = 0.5f * (float) std::sin (2.0 * PI * 1000.0 * (double) i / fs);
    calistir (c, w);
    SWX_BEKLE_YAKIN (tepeDb (w, (int) fs / 2, (int) fs), -9.0, 0.4);
}

SWX_TEST (comp_100hz_thd_ve_guardrail)
{
    const double fs = 48000.0;
    for (int det : { 0, 1, 2 })
    {
        sweax::Compressor c; c.prepare (fs, 256, 1); c.setQuality (sweax::Quality::Yuksek); c.setEnabled (true, true);
        sweax::CompParams p; p.on = true; p.threshDb = -14.0f; p.ratio = 20.0f; p.kneeDb = 0.0f;
        p.attackMs = 0.1f; p.releaseMs = 30.0f; p.lookaheadMs = 5.0f; p.detector = det; p.scHpfHz = 20.0f; c.setParams (p);
        // 100 Hz, ≈6 dB GR hedefi: giriş tepe −8 dBFS (over 6 → GR 5.7)
        std::vector<float> x ((size_t) fs * 2);
        for (size_t i = 0; i < x.size(); ++i) x[i] = std::pow (10.0f, -8.0f / 20.0f) * (float) std::sin (2.0 * PI * 100.0 * (double) i / fs);
        calistir (c, x);
        const double thd = thdYuzde (x, fs, 100.0, (int) fs, (int) fs * 2);
        if (std::getenv ("SWX_VERBOSE")) std::printf ("   det=%d THD=%.3f%% GR=%.2f dB\n", det, thd, c.gainReductionDb());
        SWX_BEKLE (thd < 1.0);
        // peak: over 6 → GR 5.7; RMS sinüsü 3 dB düşük okur → ≈ 2.9; hibrit arada
        SWX_BEKLE (-c.gainReductionDb() > (det == 1 ? 2.5 : 4.0));
    }
    // CANLI: latency 0 → lookahead yok → attack 0.1 ms 1 ms'e klemplenir: 1 kHz adımda GR 63 % süresi ≥ ~1 ms
    sweax::Compressor c; c.prepare (fs, 256, 1); c.setQuality (sweax::Quality::Canli); c.setEnabled (true, true);
    SWX_BEKLE_EQ (c.latencySamples (sweax::Quality::Canli), 0);
    SWX_BEKLE_EQ (c.latencySamples (sweax::Quality::Yuksek), 240);
    SWX_BEKLE_EQ (c.latencySamples (sweax::Quality::Ultra), 240);
    sweax::CompParams p; p.on = true; p.threshDb = -30.0f; p.ratio = 20.0f; p.kneeDb = 0.0f; p.attackMs = 0.1f; p.releaseMs = 100.0f; p.lookaheadMs = 5.0f; p.detector = 0; p.scHpfHz = 20.0f;
    c.setParams (p);
    std::vector<float> x ((size_t) fs / 2, 0.0f);
    for (size_t i = (size_t) (fs / 4); i < x.size(); ++i) x[i] = 0.5f * (float) std::sin (2.0 * PI * 1000.0 * (double) i / fs);
    // örnek örnek GR izle: adımdan sonra GR'nin hedefin %63'üne ulaştığı süre
    int n63 = -1; const int start = (int) fs / 4;
    for (int i = 0; i < (int) x.size(); i += 32)
    {
        float* ch[1] = { x.data() + i }; c.process (ch, 1, 32);
        if (i >= start && n63 < 0 && -c.gainReductionDb() > 0.63 * 22.0) n63 = i + 32 - start;
    }
    SWX_BEKLE (n63 >= 40 && n63 < 200);                               // 1 ms ≈ 48 örnek (klemp); 0.1 ms olsaydı < 32
}

SWX_TEST (comp_lookahead_latency_ve_rt)
{
    const double fs = 48000.0;
    sweax::Compressor c; c.prepare (fs, 256, 2); c.setQuality (sweax::Quality::Ultra); c.setEnabled (true, true);
    sweax::CompParams p; p.on = true; p.threshDb = 0.0f; p.lookaheadMs = 3.0f; c.setParams (p);     // eşik altı: yalnız gecikme
    std::vector<float> L ((size_t) 2048, 0.0f), R = L; L[100] = 1.0f; R[100] = 1.0f;
    for (int i = 0; i < 2048; i += 256) { float* ch[2] = { L.data() + i, R.data() + i }; c.process (ch, 2, 256); }
    int at = -1; for (int i = 0; i < 2048; ++i) if (std::fabs (L[(size_t) i]) > 0.5f) { at = i; break; }
    SWX_BEKLE_EQ (at, 100 + 240);
    // kapalı modül: kuru yol aynı gecikmede
    c.setEnabled (false, true);
    std::vector<float> M ((size_t) 2048, 0.0f), N = M; M[100] = 1.0f; N[100] = 1.0f;
    for (int i = 0; i < 2048; i += 256) { float* ch[2] = { M.data() + i, N.data() + i }; c.process (ch, 2, 256); }
    at = -1; for (int i = 0; i < 2048; ++i) if (std::fabs (M[(size_t) i]) > 0.5f) { at = i; break; }
    SWX_BEKLE_EQ (at, 100 + 240);
    // RT
    c.setEnabled (true, true);
    sweax::rt::resetViolations();
    {
        sweax::rt::Scope s;
        for (int k = 0; k < 200; ++k) { p.threshDb = -20.0f + (float) (k % 10); p.character = k % 4; p.detector = k % 3; c.setParams (p); float* ch[2] = { L.data(), R.data() }; c.process (ch, 2, 256); }
    }
    SWX_BEKLE_EQ (sweax::rt::violations(), 0u);
}
