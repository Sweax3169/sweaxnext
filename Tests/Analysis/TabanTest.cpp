// SWEAX NEXT — Tests/Analysis/TabanTest.cpp
// M3.4 denetim T5: gürültü tabanı segment SONU değeri değil — cümle ortasında biten kayıtta taban 20-33 dB
// yüksek ölçülüyordu (sahte NoiseFloor, kapı eşiği sıçraması). Hiç duraklama yoksa taban "bilinmiyor".
#include "Cati.h"
#include "Analysis/Offline.h"
#include <vector>
#include <random>
#include <cmath>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    void oda (std::vector<float>& x, double fs, double sn, std::mt19937& rng)
    {
        std::normal_distribution<float> nd (0.0f, 3.16e-4f);                      // ≈ −70 dBFS RMS
        for (size_t i = 0; i < (size_t) (sn * fs); ++i) x.push_back (nd (rng));
    }
    void ton (std::vector<float>& x, double fs, double sn, double f0, std::mt19937& rng, double& faz)
    {
        std::normal_distribution<float> nd (0.0f, 3.16e-4f);
        for (size_t i = 0; i < (size_t) (sn * fs); ++i)
        {
            const double f = f0 * std::pow (2.0, 0.08 * std::sin (2.0 * PI * 0.9 * (double) x.size() / fs));
            faz += 2.0 * PI * f / fs;
            double v = 0.0; for (int h = 1; h <= 20; ++h) v += std::sin (h * faz) / h;
            x.push_back ((float) (0.1 * v) + nd (rng));
        }
    }
}

SWX_TEST (rapor_taban_cumle_ortasinda_biten_kayit)
{
    using namespace sweax;
    const double fs = 48000.0;
    std::mt19937 rng (11); double faz = 0.0;
    std::vector<float> x;
    oda (x, fs, 1.5, rng);
    for (int k = 0; k < 6; ++k) { ton (x, fs, 1.2, 140.0 + 12.0 * k, rng, faz); oda (x, fs, 0.4, rng); }
    ton (x, fs, 5.0, 160.0, rng, faz);                                          // SON: cümle ortasında biter (4 s+ sesli)
    const auto r = offline::reportFromBuffer (x.data(), (int64_t) x.size(), fs);
    if (std::getenv ("SWX_VERBOSE")) std::printf ("   taban %.1f dB (bilinen %d), snr %.1f\n", r.noiseFloorDb, (int) r.noiseFloorKnown, r.snrDb);
    SWX_BEKLE (r.noiseFloorKnown);
    SWX_BEKLE (r.noiseFloorDb < -60.0f);                                        // gerçek ≈ −70 (+2 sapma payı); eski: son 4 s sesli → −40'lar

    // hiç duraklama yok: taban yalnız üst sınır → bilinmiyor, NoiseFloor tespiti güvensiz
    std::vector<float> y;
    ton (y, fs, 8.0, 150.0, rng, faz);
    const auto r2 = offline::reportFromBuffer (y.data(), (int64_t) y.size(), fs);
    if (std::getenv ("SWX_VERBOSE")) std::printf ("   duraklamasiz: taban %.1f dB (bilinen %d)\n", r2.noiseFloorDb, (int) r2.noiseFloorKnown);
    SWX_BEKLE (! r2.noiseFloorKnown);
    for (const auto& dd : r2.detections)
        if (dd.type == decision::Problem::NoiseFloor) SWX_BEKLE (! dd.actionable (0.4f, 0.25f));

    // denetim KA-1: başta DİJİTAL SIFIR ön-rulo (0.2 s ve 2 s) tabanı −120'ye çekmez
    for (double sifirSn : { 0.2, 2.0 })
    {
        std::vector<float> z ((size_t) (sifirSn * fs), 0.0f);
        z.insert (z.end(), x.begin(), x.end());
        const auto r3 = offline::reportFromBuffer (z.data(), (int64_t) z.size(), fs);
        if (std::getenv ("SWX_VERBOSE")) std::printf ("   %.1f s dijital sifir + sahne: taban %.1f dB (bilinen %d)\n", sifirSn, r3.noiseFloorDb, (int) r3.noiseFloorKnown);
        SWX_BEKLE (r3.noiseFloorDb > -90.0f && r3.noiseFloorDb < -60.0f);
        SWX_BEKLE (r3.noiseFloorKnown);
    }
    // denetim KA-3: destek kararı sıraya bağlı değil — ODA ÖNCE sonra duraklamasız konuşma da "bilinen"
    {
        std::vector<float> w; std::mt19937 rng2 (13); double faz2 = 0.0;
        oda (w, fs, 1.5, rng2);
        ton (w, fs, 8.0, 150.0, rng2, faz2);
        const auto r4 = offline::reportFromBuffer (w.data(), (int64_t) w.size(), fs);
        if (std::getenv ("SWX_VERBOSE")) std::printf ("   oda once + ton: taban %.1f dB (bilinen %d)\n", r4.noiseFloorDb, (int) r4.noiseFloorKnown);
        SWX_BEKLE (r4.noiseFloorKnown);
        SWX_BEKLE (r4.noiseFloorDb < -60.0f);
    }
}
