// SWEAX NEXT — Tests/DSP/AirExciterTest.cpp
// Ölçülen: AIR (1) koyu sinyalde 10 kHz bileşenini kaldırır ama LF bileşenine dokunmaz;
// (2) PROGRAM-BAĞIMLIDIR — HF zaten güçlüyse kaldırma koyu duruma göre belirgin küçüktür (statik shelf değil);
// (3) miktar 0 → çıkış birebir; (4) NaN girişte zarf zehirlenmez, çıkış sonlu kalır.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "DSP/AirExciter.h"
#include <vector>
#include <cmath>

namespace
{
    constexpr double PI = 3.14159265358979323846;

    void calistir (sweax::AirExciter& m, std::vector<float>& l, std::vector<float>& r, int blok = 256)
    {
        const int n = (int) l.size();
        for (int i = 0; i < n; i += blok) { float* ch[2] = { l.data() + i, r.data() + i }; m.process (ch, 2, std::min (blok, n - i)); }
    }
    // Goertzel genlik (dB) — kararlı bölge [a, b)
    double tonDb (const std::vector<float>& x, double fs, double hz, int a, int b)
    {
        const int n = b - a;
        const double om = 2.0 * PI * hz / fs;
        const double cw = 2.0 * std::cos (om);
        double s1 = 0.0, s2 = 0.0;
        for (int i = a; i < b; ++i) { const double s0 = (double) x[(size_t) i] + cw * s1 - s2; s2 = s1; s1 = s0; }
        const double guc = s1 * s1 + s2 * s2 - cw * s1 * s2;
        return 10.0 * std::log10 (std::max (guc, 1e-24) / ((double) n * (double) n / 4.0));
    }
    std::vector<float> karisim (double fs, int n, double lfHz, double lfAmp, double hfHz, double hfAmp)
    {
        std::vector<float> v ((size_t) n);
        for (int i = 0; i < n; ++i)
            v[(size_t) i] = (float) (lfAmp * std::sin (2.0 * PI * lfHz * i / fs) + hfAmp * std::sin (2.0 * PI * hfHz * i / fs));
        return v;
    }
}

SWX_TEST (air_koyu_sinyalde_kaldirir_lf_dokunmaz)
{
    using namespace sweax;
    const double fs = 48000.0;
    const int n = 96000;
    AirExciter air;
    air.prepare (fs, 256, 2);
    AirParams p; p.on = true; p.mid = 1.0f; p.high = 1.0f;
    air.setParams (p);
    air.setEnabled (true, true);
    // koyu vokal taklidi: 300 Hz güçlü + 10 kHz −30 dB
    auto l = karisim (fs, n, 300.0, 0.30, 10000.0, 0.30 * std::pow (10.0, -30.0 / 20.0));
    auto r = l;
    const auto giris = l;
    calistir (air, l, r);
    const int a = n / 2, b = n - 256;                     // zarf/smoother oturduktan sonra ölç
    const double lf0 = tonDb (giris, fs, 300.0, a, b),  lf1 = tonDb (l, fs, 300.0, a, b);
    const double hf0 = tonDb (giris, fs, 10000.0, a, b), hf1 = tonDb (l, fs, 10000.0, a, b);
    SWX_BEKLE (hf1 - hf0 > 3.0);                          // 10 kHz belirgin kalktı (amount 1, koyu ses)
    SWX_BEKLE (std::fabs (lf1 - lf0) < 0.3);              // 300 Hz'e dokunulmadı
    bool sonlu = true;
    for (float v : l) sonlu = sonlu && std::isfinite (v);
    SWX_BEKLE (sonlu);
    SWX_BEKLE (air.lastHighLiftDb() > 2.0f);
}

SWX_TEST (air_program_bagimli_ve_sifir_miktar)
{
    using namespace sweax;
    const double fs = 48000.0;
    const int n = 96000;
    auto kaldirmayiOlc = [&] (double hfDb)
    {
        AirExciter air;
        air.prepare (fs, 256, 2);
        AirParams p; p.on = true; p.mid = 0.0f; p.high = 1.0f;
        air.setParams (p);
        air.setEnabled (true, true);
        auto l = karisim (fs, n, 300.0, 0.30, 10000.0, 0.30 * std::pow (10.0, hfDb / 20.0));
        auto r = l;
        const auto giris = l;
        calistir (air, l, r);
        const int a = n / 2, b = n - 256;
        return tonDb (l, fs, 10000.0, a, b) - tonDb (giris, fs, 10000.0, a, b);
    };
    const double koyuKaldirma = kaldirmayiOlc (-30.0);
    const double parlakKaldirma = kaldirmayiOlc (-6.0);   // HF neredeyse tam bant kadar güçlü
    SWX_BEKLE (koyuKaldirma > 3.0);
    SWX_BEKLE (parlakKaldirma < koyuKaldirma - 2.0);      // program-bağımlılık: parlakta belirgin az
    SWX_BEKLE (parlakKaldirma < 1.5);                     // zaten parlak kayıtta neredeyse dokunmaz

    // miktar 0 → birebir geçiş
    {
        AirExciter air;
        air.prepare (fs, 256, 2);
        AirParams p; p.on = true; p.mid = 0.0f; p.high = 0.0f;
        air.setParams (p);
        air.setEnabled (true, true);
        auto l = karisim (fs, n / 4, 300.0, 0.30, 10000.0, 0.05);
        auto r = l;
        const auto giris = l;
        calistir (air, l, r);
        double fark = 0.0;
        for (size_t i = 0; i < l.size(); ++i) fark = std::max (fark, (double) std::fabs (l[i] - giris[i]));
        SWX_BEKLE (fark < 1.0e-6);
    }

    // NaN sigortası: tek NaN blok — sonraki bloklar sonlu, zarf zehirlenmedi
    {
        AirExciter air;
        air.prepare (fs, 256, 2);
        AirParams p; p.on = true; p.mid = 1.0f; p.high = 1.0f;
        air.setParams (p);
        air.setEnabled (true, true);
        std::vector<float> l (4096, 0.1f), r (4096, 0.1f);
        l[100] = std::numeric_limits<float>::quiet_NaN();
        calistir (air, l, r);
        std::vector<float> l2 (4096, 0.1f), r2 (4096, 0.1f);
        calistir (air, l2, r2);
        bool sonlu2 = true;
        for (float v : l2) sonlu2 = sonlu2 && std::isfinite (v);
        SWX_BEKLE (sonlu2);                               // NaN bloğu SONRASI bloklar temiz (zarf+filtre sigortası)
    }
}
