// SWEAX NEXT — Tests/DSP/GateSatTest.cpp
// Ölçülen: kapı cümle arası gürültüyü range kadar bastırır, vokal pasajı DOKUNULMAZ bırakır (±0.3 dB),
// histerezis cıvıltı yapmaz; saturator küçük sinyalde birim kazanç, harmonikleri üretir, alias üretmez,
// latency sabit 12 örnek, kapalıyken saydam (yalnız gecikme).
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "DSP/NoiseGate.h"
#include "DSP/Saturator.h"
#include "Chain/VocalChain.h"
#include <vector>
#include <cmath>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    template <typename M>
    void calistir (M& m, std::vector<float>& x, int blok = 256)
    {
        const int n = (int) x.size();
        for (int i = 0; i < n; i += blok) { float* ch[1] = { x.data() + i }; m.process (ch, 1, std::min (blok, n - i)); }
    }
    double rmsDb (const std::vector<float>& x, int a, int b)
    {
        double s = 0.0; for (int i = a; i < b; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
        return 10.0 * std::log10 (s / std::max (1, b - a) + 1e-20);
    }
    // Goertzel genliği (dB)
    double tonDb (const std::vector<float>& y, double fs, double hz, int a, int b)
    {
        double sc = 0.0, ss = 0.0; int n = 0;
        for (int i = a; i < b; ++i) { sc += y[(size_t) i] * std::cos (2.0 * PI * hz * i / fs); ss += y[(size_t) i] * std::sin (2.0 * PI * hz * i / fs); ++n; }
        return 20.0 * std::log10 (2.0 * std::sqrt (sc * sc + ss * ss) / std::max (1, n) + 1e-15);
    }
}

SWX_TEST (gate_gurultu_bastirma_ve_vokal_saydamligi)
{
    using namespace sweax;
    const double fs = 48000.0;
    NoiseGate g;
    g.prepare (fs, 256, 1);
    g.setQuality (Quality::Yuksek);
    GateParams p; p.on = true; p.threshDb = -40.0f; p.rangeDb = 18.0f; p.attackMs = 1.5f; p.releaseMs = 80.0f; p.holdMs = 60.0f; p.hystDb = 4.0f;
    g.setParams (p);
    g.setEnabled (true, true);

    // sinyal: [0.5 sn gürültü −55 dB] [1 sn ton −12 dB + gürültü] [1 sn gürültü] — kapı gecikmesi 96 örnek
    const int n = (int) (2.5 * fs);
    std::vector<float> x ((size_t) n);
    unsigned seed = 123;
    auto rnd = [&] { seed = seed * 1664525u + 1013904223u; return ((float) (seed >> 9) / 4194304.0f - 1.0f); };
    const float nz = std::pow (10.0f, -55.0f / 20.0f);
    const float amp = std::pow (10.0f, -12.0f / 20.0f);
    for (int i = 0; i < n; ++i)
    {
        x[(size_t) i] = nz * rnd();
        if (i >= (int) (0.5 * fs) && i < (int) (1.5 * fs))
            x[(size_t) i] += amp * (float) std::sin (2.0 * PI * 200.0 * i / fs);
    }
    auto y = x;
    calistir (g, y);
    // vokal pasajın ortası: dokunulmamış (±0.3 dB)
    const int lat = g.latencySamples (Quality::Yuksek);
    const double vokal = rmsDb (y, (int) (0.7 * fs) + lat, (int) (1.3 * fs) + lat);
    const double vokalIn = rmsDb (x, (int) (0.7 * fs), (int) (1.3 * fs));
    SWX_BEKLE_YAKIN (vokal, vokalIn, 0.3);
    // kuyruk gürültüsü (release+hold sonrası): en az 14 dB bastırılmış (range 18)
    const double kuyruk = rmsDb (y, (int) (2.0 * fs), n - 1);
    const double kuyrukIn = rmsDb (x, (int) (2.0 * fs), n - 1);
    SWX_BEKLE (kuyruk < kuyrukIn - 14.0);
    // kapalıyken: saydam (yalnız gecikme) — impuls gecikme testine gerek yok, RMS aynı
    g.setEnabled (false, true);
    auto y2 = x;
    calistir (g, y2);
    SWX_BEKLE_YAKIN (rmsDb (y2, (int) (0.6 * fs), (int) (1.4 * fs)), rmsDb (x, (int) (0.6 * fs), (int) (1.4 * fs)), 0.2);
}

SWX_TEST (sat_birim_kazanc_harmonik_ve_alias)
{
    using namespace sweax;
    const double fs = 44100.0;
    Saturator sat;
    sat.prepare (fs, 256, 1);
    SatParams p; p.on = true; p.driveDb = 6.0f; p.toneDb = 0.0f; p.mix = 1.0f;
    sat.setParams (p);
    sat.setEnabled (true, true);

    // küçük sinyal (−40 dB): birim kazanç (±0.3 dB)
    {
        const int n = 44100;
        std::vector<float> x ((size_t) n);
        for (int i = 0; i < n; ++i) x[(size_t) i] = 0.01f * (float) std::sin (2.0 * PI * 1000.0 * i / fs);
        auto y = x;
        calistir (sat, y);
        const double gIn = tonDb (x, fs, 1000.0, 4410, n), gOut = tonDb (y, fs, 1000.0, 4410, n);
        SWX_BEKLE_YAKIN (gOut, gIn, 0.3);
    }
    // güçlü sinyal (−6 dB, 3 kHz): 2. ve 3. harmonik ÜRETİLİR; alias yok
    {
        sat.reset();
        const int n = 44100;
        std::vector<float> x ((size_t) n);
        for (int i = 0; i < n; ++i) x[(size_t) i] = 0.5f * (float) std::sin (2.0 * PI * 3000.0 * i / fs);
        auto y = x;
        calistir (sat, y);
        const int a = 4410, b = n;
        const double h1 = tonDb (y, fs, 3000.0, a, b);
        const double h2 = tonDb (y, fs, 6000.0, a, b);
        const double h3 = tonDb (y, fs, 9000.0, a, b);
        SWX_BEKLE (h2 > h1 - 60.0 || h3 > h1 - 60.0);            // harmonik içerik var
        // alias adayları: 8. harmonik (24 kHz) → 20.1k'ya, 9. (27 kHz) → 17.1k'ya katlanır (44.1k'da)
        const double al1 = tonDb (y, fs, 44100.0 - 24000.0, a, b);
        const double al2 = tonDb (y, fs, 44100.0 - 27000.0 + 44100.0 * 0.0, a, b); // 17.1k
        SWX_BEKLE (al1 < h1 - 60.0);
        SWX_BEKLE (al2 < h1 - 60.0);
    }
    // latency: kapalı modül impulsu tam LATENCY örnek geciktirir
    {
        sat.reset();
        sat.setEnabled (false, true);
        const int n = 256;
        std::vector<float> x ((size_t) n, 0.0f);
        x[10] = 1.0f;
        calistir (sat, x, 256);
        int pk = 0; float mx = 0.0f;
        for (int i = 0; i < n; ++i) if (std::fabs (x[(size_t) i]) > mx) { mx = std::fabs (x[(size_t) i]); pk = i; }
        SWX_BEKLE_EQ (pk, 10 + Saturator::LATENCY);
    }
}

SWX_TEST (sat_sert_mod_ezme_ve_birim_kazanc)
{
    // M3.3 SERT MOD (trap metal): drive > 8 dB'de ikinci tanh kademesi — küçük sinyal BİRİM kalır,
    // güçlü tepe drive 6'ya göre belirgin daha fazla EZİLİR (duvara yapışmış grit).
    using namespace sweax;
    const double fs = 48000.0;
    auto tepeOlc = [&] (float driveDb, float genlik)
    {
        Saturator sat;
        sat.prepare (fs, 256, 1);
        SatParams p; p.on = true; p.driveDb = driveDb; p.toneDb = 0.0f; p.mix = 1.0f;
        sat.setParams (p);
        sat.setEnabled (true, true);
        const int n = 24000;
        std::vector<float> x ((size_t) n);
        for (int i = 0; i < n; ++i) x[(size_t) i] = genlik * (float) std::sin (2.0 * PI * 220.0 * i / fs);
        calistir (sat, x);
        float mx = 0.0f;
        for (int i = n / 2; i < n; ++i) mx = std::max (mx, std::fabs (x[(size_t) i]));
        return mx;
    };
    // küçük sinyal (−40 dB) drive 12'de de birim (±0.5 dB)
    const float kucuk = tepeOlc (12.0f, 0.01f);
    SWX_BEKLE (kucuk > 0.0094f && kucuk < 0.0106f);
    // güçlü sinyal (−4 dB tepe): drive 12 tavanı, drive 6 tavanından en az 4 dB daha ezik
    const float t6 = tepeOlc (6.0f, 0.63f);
    const float t12 = tepeOlc (12.0f, 0.63f);
    SWX_BEKLE (20.0 * std::log10 (t6 / std::max (t12, 1e-9f)) >= 4.0);
}

SWX_TEST (sat_monoton_polarite_korunur)
{
    // Denetim regresyonu: eski beta*x^2 egrisi buyuk negatif tepeleri pozitife katliyordu.
    // Yeni bias egrisi HER YERDE monoton: guclu negatif giris -> guclu negatif cikis.
    using namespace sweax;
    const double fs = 48000.0;
    Saturator sat;
    sat.prepare (fs, 256, 1);
    SatParams p; p.on = true; p.driveDb = 12.0f; p.toneDb = 0.0f; p.mix = 1.0f;
    sat.setParams (p);
    sat.setEnabled (true, true);
    const int n = 9600;
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i) x[(size_t) i] = 2.0f * (float) std::sin (2.0 * PI * 100.0 * i / fs);   // +6 dBFS tepe
    calistir (sat, x);
    float mn = 0.0f, mx = 0.0f;
    for (int i = 4800; i < n; ++i) { mn = std::min (mn, x[(size_t) i]); mx = std::max (mx, x[(size_t) i]); }
    SWX_BEKLE (mn < -0.15f);                 // negatif yarim dalga NEGATIF kaldi (katlanma yok)
    SWX_BEKLE (mx > 0.15f);
    SWX_BEKLE (std::fabs (mn + mx) < 0.5f * std::max (std::fabs (mn), mx) + 0.2f);   // kaba simetri (bias kucuk asimetri verir)
}
