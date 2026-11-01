// SWEAX NEXT — Tests/DSP/FxTest.cpp
// Ölçülen: doubler stereo yan içerik üretir + kuru dokunulmaz; delay ping-pong yankı zamanları doğru + ducking
// vokal varken wet'i kısar; reverb kuyruğu T60 ile tutarlı söner + L/R dekorele; width M/S matrisi birebir.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "DSP/Doubler.h"
#include "DSP/StereoDelay.h"
#include "DSP/PlateReverb.h"
#include "DSP/WidthModule.h"
#include "Chain/VocalChain.h"
#include <vector>
#include <cmath>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    template <typename M>
    void calistir (M& m, std::vector<float>& l, std::vector<float>& r, int blok = 256)
    {
        const int n = (int) l.size();
        for (int i = 0; i < n; i += blok) { float* ch[2] = { l.data() + i, r.data() + i }; m.process (ch, 2, std::min (blok, n - i)); }
    }
    double rmsDb (const std::vector<float>& x, int a, int b)
    {
        double s = 0.0; for (int i = a; i < b && i < (int) x.size(); ++i) s += (double) x[(size_t) i] * x[(size_t) i];
        return 10.0 * std::log10 (s / std::max (1, b - a) + 1e-20);
    }
    double tepe (const std::vector<float>& x, int a, int b)
    {
        double p = 0.0; for (int i = a; i < b && i < (int) x.size(); ++i) p = std::max (p, (double) std::fabs (x[(size_t) i]));
        return p;
    }
}

SWX_TEST (doubler_stereo_yan_icerik_ve_kuru_koruma)
{
    using namespace sweax;
    const double fs = 48000.0;
    Doubler d;
    d.prepare (fs, 256, 2);
    DoublerParams p; p.on = true; p.mix = 0.5f; p.spreadMs = 18.0f; p.detuneCents = 9.0f; p.hpHz = 100.0f; p.lpHz = 12000.0f; p.voices = 0;
    d.setParams (p);
    d.setEnabled (true, true);
    const int n = 48000;
    std::vector<float> l ((size_t) n), r ((size_t) n);
    for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = 0.3f * (float) std::sin (2.0 * PI * 220.0 * i / fs);
    auto inL = l;
    calistir (d, l, r);
    // yan içerik üretildi (detune + farklı pan → L≠R)
    std::vector<float> side ((size_t) n);
    for (int i = 0; i < n; ++i) side[(size_t) i] = 0.5f * (l[(size_t) i] - r[(size_t) i]);
    const double sideDb = rmsDb (side, 24000, n), inDb = rmsDb (inL, 24000, n);
    SWX_BEKLE (sideDb > inDb - 30.0);                        // yan içerik duyulur seviyede
    // mix 0 → kuru birebir korunur
    d.reset(); p.mix = 0.0f; d.setParams (p);
    std::vector<float> l2 ((size_t) n), r2 ((size_t) n);
    for (int i = 0; i < n; ++i) l2[(size_t) i] = r2[(size_t) i] = inL[(size_t) i];
    calistir (d, l2, r2);
    double maxFark = 0.0;
    for (int i = 0; i < n; ++i) maxFark = std::max (maxFark, (double) std::fabs (l2[(size_t) i] - inL[(size_t) i]));
    SWX_BEKLE (maxFark < 1.0e-4);
}

SWX_TEST (delay_pingpong_zamanlari_ve_ducking)
{
    using namespace sweax;
    const double fs = 48000.0;
    StereoDelay d;
    d.prepare (fs, 256, 2);
    DelayParams p; p.on = true; p.mix = 0.5f; p.sync = 0; p.timeMs = 100.0f; p.feedback = 0.5f;
    p.hpHz = 60.0f; p.lpHz = 16000.0f; p.duckDb = 0.0f; p.bpm = 120.0;
    d.setParams (p);
    d.setEnabled (true, true);
    const int n = 48000;
    std::vector<float> l ((size_t) n, 0.0f), r ((size_t) n, 0.0f);
    for (int i = 100; i < 148; ++i) l[(size_t) i] = r[(size_t) i] = 0.5f;   // 1 ms'lik patlama
    calistir (d, l, r);
    const int T = 4800;                                       // 100 ms
    // yankı 1: L kanalında T'de; yankı 2: R kanalında 2T'de (ping-pong)
    const double e1L = tepe (l, 100 + T - 100, 100 + T + 200), e1R = tepe (r, 100 + T - 100, 100 + T + 200);
    const double e2R = tepe (r, 100 + 2 * T - 100, 100 + 2 * T + 200), e2L = tepe (l, 100 + 2 * T - 100, 100 + 2 * T + 200);
    SWX_BEKLE (e1L > 0.1);
    SWX_BEKLE (e1L > 4.0 * e1R);
    SWX_BEKLE (e2R > 0.05);
    SWX_BEKLE (e2R > 2.0 * e2L);
    // ducking: sürekli yüksek ton varken wet, ducking'siz duruma göre kısılır
    auto koydur = [&] (float duckDb) -> double
    {
        StereoDelay d2; d2.prepare (fs, 256, 2);
        DelayParams p2 = p; p2.duckDb = duckDb; p2.feedback = 0.0f; p2.mix = 0.5f;
        d2.setParams (p2); d2.setEnabled (true, true);
        std::vector<float> a ((size_t) n), b ((size_t) n);
        for (int i = 0; i < n; ++i) a[(size_t) i] = b[(size_t) i] = 0.3f * (float) std::sin (2.0 * PI * 200.0 * i / fs);
        calistir (d2, a, b);
        // wet = çıkış − kuru
        std::vector<float> w ((size_t) n);
        for (int i = 0; i < n; ++i) w[(size_t) i] = a[(size_t) i] - 0.3f * (float) std::sin (2.0 * PI * 200.0 * i / fs);
        return rmsDb (w, 24000, n);
    };
    const double wet0 = koydur (0.0f), wet12 = koydur (12.0f);
    SWX_BEKLE (wet12 < wet0 - 8.0);                           // sürekli sinyalde ~12 dB duck (tolerans 8)
}

SWX_TEST (reverb_kuyruk_sonumu_ve_dekorelasyon)
{
    using namespace sweax;
    const double fs = 48000.0;
    PlateReverb rv;
    rv.prepare (fs, 256, 2);
    ReverbParams p; p.on = true; p.mix = 1.0f; p.preDelayMs = 10.0f; p.decaySec = 1.2f;
    p.dampHz = 8000.0f; p.hpHz = 100.0f; p.width = 1.0f; p.duckDb = 0.0f;
    rv.setParams (p);
    rv.setEnabled (true, true);
    const int n = (int) (3.0 * fs);
    std::vector<float> l ((size_t) n, 0.0f), r ((size_t) n, 0.0f);
    for (int i = 100; i < 196; ++i) l[(size_t) i] = r[(size_t) i] = 0.5f;
    calistir (rv, l, r);
    const double t03 = rmsDb (l, (int) (0.25 * fs), (int) (0.45 * fs));
    const double t12 = rmsDb (l, (int) (1.15 * fs), (int) (1.35 * fs));
    const double t25 = rmsDb (l, (int) (2.5 * fs), (int) (2.9 * fs));
    SWX_BEKLE (t03 > -60.0);                                  // kuyruk var
    SWX_BEKLE (t03 - t12 > 8.0);                              // sönüyor
    SWX_BEKLE (t12 - t25 > 6.0);                              // sönmeye devam
    // dekorelasyon: L/R farkı anlamlı (plate genişliği)
    std::vector<float> side ((size_t) n);
    for (int i = 0; i < n; ++i) side[(size_t) i] = 0.5f * (l[(size_t) i] - r[(size_t) i]);
    SWX_BEKLE (rmsDb (side, (int) (0.25 * fs), (int) (0.45 * fs)) > t03 - 12.0);
}

SWX_TEST (width_ms_matrisi)
{
    using namespace sweax;
    WidthModule w;
    w.prepare (48000.0, 256, 2);
    w.setParams ({ true, 2.0f });
    w.setEnabled (true, true);
    const int n = 4800;                                       // 20 ms rampa payıyla
    std::vector<float> l ((size_t) n, 1.0f), r ((size_t) n, 0.0f);
    calistir (w, l, r);
    // amount 2: side ×2 → L = 0.5 + 1.0 = 1.5, R = 0.5 − 1.0 = −0.5 (rampa sonrası)
    SWX_BEKLE_YAKIN (l[(size_t) (n - 1)], 1.5f, 1.0e-3f);
    SWX_BEKLE_YAKIN (r[(size_t) (n - 1)], -0.5f, 1.0e-3f);
}
