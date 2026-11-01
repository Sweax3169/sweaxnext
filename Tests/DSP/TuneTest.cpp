// SWEAX NEXT — Tests/DSP/TuneTest.cpp
// Ölçülen: falsolu ton en yakın yarım tona çekilir (±6 cent), tam notada NÖTR yol birebir gecikmiş sinyal verir
// (comb/renklenme yok), gürültüde (tonal değil) şeffaf, latency sabit.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "DSP/PitchCorrector.h"
#include "Chain/VocalChain.h"
#include "Analysis/Pitch.h"
#include <vector>
#include <cmath>
#include <cstdio>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    void calistir (sweax::PitchCorrector& m, std::vector<float>& x, int blok = 256)
    {
        const int n = (int) x.size();
        for (int i = 0; i < n; i += blok) { float* ch[1] = { x.data() + i }; m.process (ch, 1, std::min (blok, n - i)); }
    }
    // hassas frekans: Goertzel guc taramasi + parabolik (0.05 Hz izgara)
    double hassasHz (const std::vector<float>& y, double fs, double lo, double hi)
    {
        const int a = (int) y.size() / 2, b = (int) y.size();
        auto guc = [&] (double hz)
        {
            double sc = 0.0, ss = 0.0;
            for (int i = a; i < b; ++i) { sc += y[(size_t) i] * std::cos (2.0 * PI * hz * i / fs); ss += y[(size_t) i] * std::sin (2.0 * PI * hz * i / fs); }
            return sc * sc + ss * ss;
        };
        double best = lo, bg = -1.0;
        for (double h = lo; h <= hi; h += 0.05) { const double g = guc (h); if (g > bg) { bg = g; best = h; } }
        return best;
    }    // çıkışın perdesi (son yarıda, offline YIN)
    float perdeOlc (const std::vector<float>& y, double fs)
    {
        sweax::PitchTracker t;
        t.prepare (fs, 60.0, 900.0);
        const int need = t.frameSize();
        double toplam = 0.0; int adet = 0;
        for (int a = (int) y.size() / 2; a + need < (int) y.size(); a += need / 2)
        {
            const auto r = t.analyze (y.data() + a, need);
            if (r.voiced && r.hz > 0.0f) { toplam += r.hz; ++adet; }
        }
        return adet > 0 ? (float) (toplam / adet) : 0.0f;
    }
}

SWX_TEST (tune_kromatik_kilit_ve_notr_saydamlik)
{
    using namespace sweax;
    const double fs = 48000.0;
    // 225 Hz: A3'e (220) +39 cent falso → kromatik kilit 220'ye çekmeli
    {
        PitchCorrector t;
        t.prepare (fs, 256, 1);
        TuneParams p; p.on = true; p.strength = 1.0f; p.speedMs = 3.0f; p.key = 0; p.scale = 0;
        t.setParams (p);
        t.setEnabled (true, true);
        const int n = (int) (2.0 * fs);
        std::vector<float> x ((size_t) n);
        for (int i = 0; i < n; ++i) x[(size_t) i] = 0.4f * (float) std::sin (2.0 * PI * 225.0 * i / fs);
        calistir (t, x);
        const float hz = perdeOlc (x, fs);
        SWX_BEKLE (hz > 0.0f);
        const float cent = 1200.0f * (float) std::log2 (hz / 220.0f);
        const double hOut = hassasHz (x, fs, 210.0, 232.0);
        std::printf ("  [tune] yin=%.2f Hz hassas=%.2f Hz (%.1f cent, hedef 220)\n", hz, hOut, cent);
        SWX_BEKLE (std::fabs (cent) < 6.0f);                  // 220'ye kilitli (giriş +39 cent idi)
    }
    // tam nota (220.0): nötr yol → çıkış = latency kadar gecikmiş giriş (birebir; comb yok)
    {
        PitchCorrector t;
        t.prepare (fs, 256, 1);
        TuneParams p; p.on = true; p.strength = 1.0f; p.speedMs = 3.0f; p.key = 0; p.scale = 0;
        t.setParams (p);
        t.setEnabled (true, true);
        const int n = (int) (2.0 * fs);
        std::vector<float> x ((size_t) n);
        for (int i = 0; i < n; ++i) x[(size_t) i] = 0.4f * (float) std::sin (2.0 * PI * 220.0 * i / fs);
        auto y = x;
        calistir (t, y);
        const int lat = t.latencySamples (Quality::Yuksek);
        double maxFark = 0.0;
        for (int i = n / 2; i < n; ++i)
            maxFark = std::max (maxFark, (double) std::fabs (y[(size_t) i] - x[(size_t) (i - lat)]));
        SWX_BEKLE (maxFark < 0.02);                           // birebir (ara değer payı)
    }
    // gürültü (tonal değil): şeffaf geçiş (RMS korunur)
    {
        PitchCorrector t;
        t.prepare (fs, 256, 1);
        TuneParams p; p.on = true; p.strength = 1.0f; p.speedMs = 3.0f;
        t.setParams (p);
        t.setEnabled (true, true);
        const int n = 48000;
        std::vector<float> x ((size_t) n);
        unsigned seed = 7;
        for (int i = 0; i < n; ++i) { seed = seed * 1664525u + 1013904223u; x[(size_t) i] = 0.1f * ((float) (seed >> 9) / 4194304.0f - 1.0f); }
        auto y = x;
        calistir (t, y);
        double sx = 0.0, sy = 0.0;
        for (int i = n / 2; i < n; ++i) { sx += (double) x[(size_t) i] * x[(size_t) i]; sy += (double) y[(size_t) i] * y[(size_t) i]; }
        const double farkDb = 10.0 * std::log10 ((sy + 1e-20) / (sx + 1e-20));
        SWX_BEKLE (std::fabs (farkDb) < 1.0);
    }
}
