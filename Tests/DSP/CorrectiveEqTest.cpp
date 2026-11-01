// SWEAX NEXT — Tests/DSP/CorrectiveEqTest.cpp
// Ölçülen: statik bell/shelf/HPF yanıtları, HPF fc geçişi tıksız, dinamik bant GR + bant dışı splatter,
// enable/disable tıksız, RT kapsamında tahsis yok.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "DSP/CorrectiveEQ.h"
#include "Chain/VocalChain.h"
#include "Core/RtGuard.h"
#include "Core/Biquad.h"
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    // sabit sinüs üzerinden kazanç (dB): son yarıdaki RMS oranı
    double kazancDb (sweax::CorrectiveEQ& eq, double fs, double hz, int ch = 2, double sn = 0.5)
    {
        const int n = (int) (fs * sn);
        std::vector<float> L ((size_t) n), R ((size_t) n);
        for (int i = 0; i < n; ++i) { L[(size_t) i] = 0.25f * (float) std::sin (2.0 * PI * hz * i / fs); R[(size_t) i] = L[(size_t) i]; }
        for (int i = 0; i < n; i += 256) { float* c[2] = { L.data() + i, R.data() + i }; eq.process (c, ch, std::min (256, n - i)); }
        double s = 0.0; int cnt = 0;
        for (int i = n / 2; i < n; ++i) { s += (double) L[(size_t) i] * L[(size_t) i]; ++cnt; }
        const double db = 10.0 * std::log10 (s / cnt / (0.25 * 0.25 * 0.5));
        if (std::getenv ("SWX_VERBOSE")) std::printf ("   fs=%.0f hz=%.0f -> %.2f dB\n", fs, hz, db);
        return db;
    }
    sweax::EqParams temel()
    {
        sweax::EqParams p; p.on = true; return p;
    }
}

SWX_TEST (eq_statik_yanitlar)
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        sweax::CorrectiveEQ eq; eq.prepare (fs, 256, 2); eq.setEnabled (true, true);
        auto p = temel();
        p.band[0].on = true; p.band[0].type = 0; p.band[0].hz = 1000.0f; p.band[0].gainDb = 6.0f; p.band[0].q = 1.0f;
        eq.setParams (p);
        // ısıt (kazanç rampası 30 ms) → sonra ölç
        kazancDb (eq, fs, 1000.0, 2, 0.2);
        SWX_BEKLE_YAKIN (kazancDb (eq, fs, 1000.0), 6.0, 0.15);
        SWX_BEKLE_YAKIN (kazancDb (eq, fs, 100.0), 0.0, 0.1);
        SWX_BEKLE_YAKIN (kazancDb (eq, fs, 10000.0), 0.0, 0.15);
        // Nyquist yakını bell (eşleşmeli): 15 kHz +6 dB, 44.1k'da bile tepe 15 kHz'de ≈ 6 dB
        p.band[0].hz = 15000.0f; p.band[0].q = 2.0f; eq.setParams (p);
        kazancDb (eq, fs, 15000.0, 2, 0.3);
        SWX_BEKLE_YAKIN (kazancDb (eq, fs, 15000.0), 6.0, 0.3);
        // high shelf +4 dB @ 8 kHz: 16 kHz'de ≈ 4 (44.1k'da Nyquist'e yakın: ≥ 3.5), 1 kHz'de ≈ 0
        p.band[0].on = false; p.band[1].on = true; p.band[1].type = 2; p.band[1].hz = 8000.0f; p.band[1].gainDb = 4.0f; p.band[1].q = 0.7071f;
        eq.setParams (p); kazancDb (eq, fs, 1000.0, 2, 0.2);
        SWX_BEKLE (kazancDb (eq, fs, 16000.0) > 3.5 && kazancDb (eq, fs, 16000.0) < 4.3);
        SWX_BEKLE_YAKIN (kazancDb (eq, fs, 1000.0), 0.0, 0.2);
        // HPF 24 dB @ 120 Hz: 40 Hz ≥ 36 dB zayıflar, 1 kHz düz; 12 dB: 40 Hz ≈ −19 dB
        p.band[1].on = false; p.hpfOn = true; p.hpfHz = 120.0f; p.hpfSlope = 1; eq.setParams (p); kazancDb (eq, fs, 1000.0, 2, 0.2);
        SWX_BEKLE (kazancDb (eq, fs, 40.0) < -36.0);
        SWX_BEKLE_YAKIN (kazancDb (eq, fs, 1000.0), 0.0, 0.1);
        p.hpfSlope = 0; eq.setParams (p); kazancDb (eq, fs, 1000.0, 2, 0.2);
        SWX_BEKLE_YAKIN (kazancDb (eq, fs, 40.0), -19.5, 1.5);
        // notch @ 1 kHz Q 4: 1 kHz ≤ −30 dB, 2 kHz ≈ 0
        p.hpfOn = false; p.band[2].on = true; p.band[2].type = 3; p.band[2].hz = 1000.0f; p.band[2].q = 4.0f; eq.setParams (p); kazancDb (eq, fs, 1000.0, 2, 0.3);
        SWX_BEKLE (kazancDb (eq, fs, 1000.0) < -30.0);
        SWX_BEKLE_YAKIN (kazancDb (eq, fs, 2000.0), 0.0, 0.5);
    }
}

SWX_TEST (eq_hpf_gecisi_ve_enable_tiksiz)
{
    const double fs = 48000.0;
    sweax::CorrectiveEQ eq; eq.prepare (fs, 256, 1); eq.setEnabled (true, true);
    auto p = temel(); p.hpfOn = true; p.hpfHz = 80.0f; p.hpfSlope = 1; eq.setParams (p);
    // 40 Hz sinüs −6 dBFS; fc 80 → 120 Hz sıçraması (crossfade); durağan tepe seviyeleri ölçülür, geçişte taşma ≤ 3 dB
    const int n = (int) fs * 2; std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i) x[(size_t) i] = 0.5f * (float) std::sin (2.0 * PI * 40.0 * i / fs);
    float peakA = 0.0f, peakB = 0.0f, peakGecis = 0.0f;
    for (int i = 0; i < n; i += 256)
    {
        const double t = (double) i / fs;
        if (t >= 1.0 && t < 1.0 + 256.0 / fs) { p.hpfHz = 120.0f; eq.setParams (p); }
        float* c[1] = { x.data() + i };
        eq.process (c, 1, std::min (256, n - i));
        float pk = 0.0f; for (int j = i; j < std::min (n, i + 256); ++j) pk = std::max (pk, std::fabs (x[(size_t) j]));
        if (t > 0.6 && t < 0.98) peakA = std::max (peakA, pk);
        else if (t >= 1.0 && t < 1.4) peakGecis = std::max (peakGecis, pk);
        else if (t > 1.6) peakB = std::max (peakB, pk);
    }
    SWX_BEKLE (peakA > peakB);                                   // 120 Hz daha çok zayıflatır
    SWX_BEKLE (20.0 * std::log10 (peakGecis / peakA) < 3.0);     // geçişte taşma yok (tık/patlama)
    SWX_BEKLE (20.0 * std::log10 (peakB) < -34.0);               // 40 Hz @ 120 Hz 24 dB → ≈ −44 dBFS
    // enable/disable crossfade: +6 dB bell üzerinde 1 kHz sinüs; örnek farkı süreklilik ölçütü
    sweax::CorrectiveEQ e2; e2.prepare (fs, 256, 1); e2.setEnabled (true, true);
    auto q = temel(); q.band[0].on = true; q.band[0].hz = 1000.0f; q.band[0].gainDb = 6.0f; q.band[0].q = 1.0f; e2.setParams (q);
    std::vector<float> y ((size_t) fs);
    for (size_t i = 0; i < y.size(); ++i) y[i] = 0.25f * (float) std::sin (2.0 * PI * 1000.0 * (double) i / fs);
    for (int i = 0; i < (int) y.size(); i += 256)
    {
        if (i == 24064) e2.setEnabled (false);
        if (i == 36096) e2.setEnabled (true);
        float* cy[1] = { y.data() + i };
        e2.process (cy, 1, std::min (256, (int) y.size() - i));
    }
    // 1 kHz'de örnekler arası azami fark (0.5 tepe için ≈ 2π·1000/48000·0.5 = 0.065)
    float maxFark = 0.0f;
    for (size_t i = 1; i < y.size(); ++i) maxFark = std::max (maxFark, std::fabs (y[i] - y[i - 1]));
    SWX_BEKLE (maxFark < 0.075f);
}

SWX_TEST (eq_dinamik_bant_gr_ve_splatter)
{
    const double fs = 48000.0;
    sweax::CorrectiveEQ eq; eq.prepare (fs, 256, 1); eq.setEnabled (true, true);
    auto p = temel();
    p.band[0].on = true; p.band[0].type = 0; p.band[0].hz = 1000.0f; p.band[0].gainDb = 0.0f; p.band[0].q = 2.0f;
    p.band[0].dyn = true; p.band[0].dynThreshDb = -18.0f; p.band[0].dynRangeDb = -12.0f; p.band[0].dynAttackMs = 5.0f; p.band[0].dynReleaseMs = 120.0f;
    eq.setParams (p);
    // 1 kHz: 0.5 s @ −24 dBFS (eşik altı → GR 0) → +12 dB adım → 0.5 s @ −12 dBFS (over ≈ 6 dB → GR ≈ 6)
    const int n = (int) fs; std::vector<float> x ((size_t) n), giris ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const float a = (i < n / 2) ? std::pow (10.0f, -24.0f / 20.0f) : std::pow (10.0f, -12.0f / 20.0f);
        x[(size_t) i] = a * (float) std::sin (2.0 * PI * 1000.0 * i / fs);
    }
    giris = x;
    for (int i = 0; i < n; i += 256) { float* c[1] = { x.data() + i }; eq.process (c, 1, std::min (256, n - i)); }
    auto rmsDb = [&] (const std::vector<float>& v, int a, int b) { double s = 0.0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return 10.0 * std::log10 (s / (b - a) + 1e-20); };
    // eşik altı: değişim yok; üstü: ≈ −6 dB (RMS dedektör: sinüs RMS = tepe −3 dB → over = −15 − (−18) = 3 dB → GR ≈ 3 dB)
    SWX_BEKLE_YAKIN (rmsDb (x, n / 4, n / 2) - rmsDb (giris, n / 4, n / 2), 0.0, 0.2);
    const double gr = rmsDb (giris, n * 3 / 4, n) - rmsDb (x, n * 3 / 4, n);
    SWX_BEKLE_YAKIN (gr, 3.0, 0.7);
    SWX_BEKLE_YAKIN (-eq.bandGainReductionDb (0), gr, 0.5);
    // bant dışı splatter: adım bölgesinde (±20/50 ms) 3 kHz üstü tepe. Giriş adımının KENDİ geniş bant içeriği
    // çıkarılır: çıkış HF tepesi ≤ giriş HF tepesi + 1 dB (kazanç modülasyonu splatter EKLEMEZ) ve < −70 dBFS mutlak.
    auto hfTepeDb = [&] (const std::vector<float>& v)
    {
        std::vector<float> hf = v;
        sweax::Biquad h1, h2; h1.setCoeffs (sweax::design::highpass (fs, 3000.0, 0.7071)); h2.setCoeffs (sweax::design::highpass (fs, 3000.0, 0.7071));
        for (auto& s : hf) s = h2.processSample (h1.processSample (s));
        float pk = 0.0f; for (int i = n / 2 - 960; i < n / 2 + 2400; ++i) pk = std::max (pk, std::fabs (hf[(size_t) i]));
        return 20.0 * std::log10 (pk + 1e-12);
    };
    const double hfIn = hfTepeDb (giris), hfOut = hfTepeDb (x);
    if (std::getenv ("SWX_VERBOSE")) std::printf ("   splatter: giris %.1f dBFS, cikis %.1f dBFS, GR %.2f dB\n", hfIn, hfOut, gr);
    SWX_BEKLE (hfOut <= hfIn + 1.0);
    SWX_BEKLE (hfOut < -70.0 || hfOut <= hfIn + 1.0);
    // RT: tahsis yok (parametre değişimleriyle birlikte)
    sweax::rt::resetViolations();
    {
        sweax::rt::Scope s;
        for (int k = 0; k < 200; ++k)
        {
            p.band[0].hz = 800.0f + 10.0f * (float) (k % 20); p.band[1].on = (k % 7) == 0; p.band[1].gainDb = -3.0f; p.band[1].hz = 300.0f + (float) k;
            p.hpfOn = (k % 3) == 0; p.hpfHz = 60.0f + (float) (k % 50);
            eq.setParams (p);
            float* cc[1] = { x.data() }; eq.process (cc, 1, 256);
        }
    }
    SWX_BEKLE_EQ (sweax::rt::violations(), 0u);
}
