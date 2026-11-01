// SWEAX NEXT — Tests/DSP/GlowTest.cpp
// Ölçülen (M3.2 GLOW): (1) nötr koridor + bant toplamı DÜZ (crossover'larda çukur yok, ±1.5 dB);
// (2) sessiz pasaj YUKARI kalkar, güçlü pasaj AŞAĞI sıkışır (dinamik aralık daralır) ama SESSİZLİK
// (−70 dB) pompalanmaz; (3) depth 0 birebir; (4) NaN sigortası; (5) kapalı›açık geçişte patlama yok.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "DSP/GlowModule.h"
#include <vector>
#include <cmath>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    using namespace sweax;

    void calistir (GlowModule& m, std::vector<float>& l, std::vector<float>& r, int blok = 256)
    {
        const int n = (int) l.size();
        for (int i = 0; i < n; i += blok) { float* ch[2] = { l.data() + i, r.data() + i }; m.process (ch, 2, std::min (blok, n - i)); }
    }
    std::vector<float> sinus (double fs, int n, double hz, double db)
    {
        std::vector<float> v ((size_t) n);
        const double a = std::pow (10.0, db / 20.0);
        for (int i = 0; i < n; ++i) v[(size_t) i] = (float) (a * std::sin (2.0 * PI * hz * i / fs));
        return v;
    }
    double rmsDb (const std::vector<float>& x, int a, int b)
    {
        double s = 0.0; int c = 0;
        for (int i = a; i < b && i < (int) x.size(); ++i) { s += (double) x[(size_t) i] * x[(size_t) i]; ++c; }
        return 10.0 * std::log10 (s / std::max (1, c) + 1e-24);
    }
    GlowModule kur (double fs, float depth, float time = 1.0f)
    {
        GlowModule g;
        g.prepare (fs, 256, 2);
        GlowParams p; p.on = true; p.depth = depth; p.timeMul = time;
        g.setParams (p);
        g.setEnabled (true, true);
        return g;
    }
}

SWX_TEST (glow_koridor_duz_ve_depth0_birebir)
{
    const double fs = 48000.0;
    const int n = 48000;
    // nötr koridor: −34 dB RMS sinüsler (tüm bantlarda up eşiklerinin üstü, down eşiklerinin altı):
    // depth 1'de bile kazanç ≈ makyaj − telafi (≤ +0.5 dB) — bölme+toplama DÜZ kalmalı (AP hizası kanıtı)
    for (double hz : { 100.0, 180.0, 500.0, 2400.0, 5000.0 })
    {
        auto g = kur (fs, 1.0f);
        auto l = sinus (fs, n, hz, -31.0);      // tepe −31 dB → RMS −34
        auto r = l;
        const auto in = l;
        calistir (g, l, r);
        const double fark = rmsDb (l, n / 2, n) - rmsDb (in, n / 2, n);
        SWX_BEKLE (std::fabs (fark) < 1.6);     // çentik/tepe yok (LR4+AP hizası ve koridor nötrlüğü)
    }
    // depth 0 → genlik birebir (karışım bant alanında: çıkış allpass'lı toplam — dalga şekli değil
    // GENLİK korunur; modul kapalıyken ModuleBase kuru yolu zaten birebir)
    {
        auto g = kur (fs, 0.0f);
        auto l = sinus (fs, n / 2, 700.0, -20.0);
        auto r = l;
        const auto in = l;
        calistir (g, l, r);
        const double fark = rmsDb (l, n / 4, n / 2) - rmsDb (in, n / 4, n / 2);
        SWX_BEKLE (std::fabs (fark) < 0.1);
    }
}

SWX_TEST (glow_yukari_asagi_ve_sessizlik_pompalamaz)
{
    const double fs = 48000.0;
    const int n = 96000;
    // (a) sessiz pasaj (−48 dB RMS, up bölgesi) yukarı kalkar
    {
        auto g = kur (fs, 1.0f);
        auto l = sinus (fs, n, 700.0, -45.0);   // RMS −48
        auto r = l;
        const auto in = l;
        calistir (g, l, r);
        const double kaldirma = rmsDb (l, n / 2, n) - rmsDb (in, n / 2, n);
        SWX_BEKLE (kaldirma > 3.0);             // upward çalışıyor
        SWX_BEKLE (kaldirma < 12.0);            // tavan + telafi sınırlı tutuyor
    }
    // (b) güçlü pasaj (−17 dB RMS, down bölgesi) sıkışır: net kazanç ≤ +1 dB (makyaja rağmen)
    {
        auto g = kur (fs, 1.0f);
        auto l = sinus (fs, n, 700.0, -14.0);   // RMS −17 (mid down eşiği −26'nın 9 dB üstü)
        auto r = l;
        const auto in = l;
        calistir (g, l, r);
        const double fark = rmsDb (l, n / 2, n) - rmsDb (in, n / 2, n);
        SWX_BEKLE (fark < 1.0);
        SWX_BEKLE (fark > -12.0);               // olculdu: -8.4 hesap + bant gecis kayiplari ~-9.3
        SWX_BEKLE (g.lastGainDb (1) < 0.0f);    // mid bandda gerçek GR var
    }
    // (a)+(b): dinamik aralık daraldı — sessiz ile güçlü arasındaki fark küçüldü (OTT'nin özü)
    // (c) SESSİZLİK (−70 dB) pompalanmaz
    {
        auto g = kur (fs, 1.0f);
        auto l = sinus (fs, n / 2, 700.0, -67.0);   // RMS −70 (sessizlik tabanı −55'in altında)
        auto r = l;
        const auto in = l;
        calistir (g, l, r);
        const double fark = rmsDb (l, n / 4, n / 2) - rmsDb (in, n / 4, n / 2);
        SWX_BEKLE (fark < 3.0);                 // upward kapalı: nefes/oda patlaması yok
    }
    // (d) NaN sigortası: zehirli blok sonrası temiz bloklar sonlu
    {
        auto g = kur (fs, 1.0f);
        std::vector<float> l (4096, 0.05f), r (4096, 0.05f);
        l[64] = std::numeric_limits<float>::quiet_NaN();
        calistir (g, l, r);
        std::vector<float> l2 (4096, 0.05f), r2 (4096, 0.05f);
        calistir (g, l2, r2);
        bool sonlu = true;
        for (float v : l2) sonlu = sonlu && std::isfinite (v);
        SWX_BEKLE (sonlu);
    }
    // (e) kapalı 1 sn beslenip açılınca ilk 50 ms'de patlama yok (idleWet ısınması)
    {
        GlowModule g;
        g.prepare (fs, 256, 2);
        GlowParams p; p.on = true; p.depth = 1.0f; p.timeMul = 1.0f;
        g.setParams (p);
        g.setEnabled (false, true);
        auto l = sinus (fs, 48000, 700.0, -25.0);
        auto r = l;
        calistir (g, l, r);                     // kapalı: idleWet ısınır
        g.setEnabled (true, false);             // crossfade ile aç
        auto l2 = sinus (fs, 4800, 700.0, -25.0);
        auto r2 = l2;
        const auto in2 = l2;
        calistir (g, l2, r2);
        const double fark = rmsDb (l2, 0, 2400) - rmsDb (in2, 0, 2400);
        SWX_BEKLE (fark < 6.0);                 // bayat/patlamalı açılış yok
    }
}
