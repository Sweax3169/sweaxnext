// SWEAX NEXT — Tests/DSP/DirtLayerTest.cpp
// Ölçülen (M3.4 kimlik modülleri):
//  DIRT: SCREAM harmonik grit üretir (THD bariz), seviye −36'da lead neredeyse dokunulmaz; otomatik seviye
//        sürüşten bağımsız veri yolu seviyesi (±3 dB); LOFI harmonik-dışı hışırtı tabanı yükseltir; latency
//        sabit 12 (açık/kapalı); sessizlik gürültü taşımaz; NaN sigortası.
//  LAYER: −12 oktav-altı (f/2) ve +7 beşli bileşenleri üretir; iki ses A sol / B sağ; ses yoksa birebir.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "DSP/DirtModule.h"
#include "DSP/LayerModule.h"
#include <vector>
#include <cmath>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    using namespace sweax;

    template <typename M>
    void calistir (M& m, std::vector<float>& l, std::vector<float>& r, int blok = 256)
    {
        const int n = (int) l.size();
        for (int i = 0; i < n; i += blok) { float* ch[2] = { l.data() + i, r.data() + i }; m.process (ch, 2, std::min (blok, n - i)); }
    }
    std::vector<float> sinus (double fs, int n, double hz, double tepeDb)
    {
        std::vector<float> v ((size_t) n);
        const double a = std::pow (10.0, tepeDb / 20.0);
        for (int i = 0; i < n; ++i) v[(size_t) i] = (float) (a * std::sin (2.0 * PI * hz * i / fs));
        return v;
    }
    double tonDb (const std::vector<float>& x, double fs, double hz, int a, int b)
    {
        const double om = 2.0 * PI * hz / fs, cw = 2.0 * std::cos (om);
        double s1 = 0.0, s2 = 0.0;
        for (int i = a; i < b; ++i) { const double s0 = (double) x[(size_t) i] + cw * s1 - s2; s2 = s1; s1 = s0; }
        const double n = (double) (b - a);
        return 10.0 * std::log10 (std::max (s1 * s1 + s2 * s2 - cw * s1 * s2, 1e-30) / (n * n));
    }
    double rmsDb (const std::vector<float>& x, int a, int b)
    {
        double s = 0.0; int c = 0;
        for (int i = a; i < b && i < (int) x.size(); ++i) { s += (double) x[(size_t) i] * x[(size_t) i]; ++c; }
        return 10.0 * std::log10 (s / std::max (1, c) + 1e-30);
    }
    // THD (dB): 2..8. harmonik enerji toplamı / temel
    double thdDb (const std::vector<float>& x, double fs, double f0, int a, int b)
    {
        const double temel = std::pow (10.0, tonDb (x, fs, f0, a, b) / 10.0);
        double h = 0.0;
        for (int k = 2; k <= 8; ++k) h += std::pow (10.0, tonDb (x, fs, f0 * k, a, b) / 10.0);
        return 10.0 * std::log10 (h / std::max (temel, 1e-30));
    }
    DirtModule kirKur (double fs, int mode, float drive, float level, float bits = 10.0f, float rate = 2.0f)
    {
        DirtModule d;
        d.prepare (fs, 256, 2);
        DirtParams p; p.on = true; p.mode = mode; p.driveDb = drive; p.levelDb = level; p.toneHz = 8000.0f; p.bits = bits; p.rate = rate;
        d.setParams (p);
        d.setEnabled (true, true);
        return d;
    }
}

SWX_TEST (dirt_scream_grit_otoseviye_ve_latency)
{
    const double fs = 48000.0;
    const int n = 48000, a = n / 2, b = n;
    const double f0 = 440.0;

    // SCREAM tam seviye: THD bariz; −36 dB: lead neredeyse temiz
    {
        auto d = kirKur (fs, 0, 24.0f, 0.0f);
        auto l = sinus (fs, n, f0, -12.0); auto r = l;
        calistir (d, l, r);
        SWX_BEKLE (thdDb (l, fs, f0, a, b) > -20.0);
    }
    // SÜRÜŞ SEVİYEDEN BAĞIMSIZ (M3.4 regresyonu): −30 dBFS girişte de grit −12 dBFS'teki kadar bariz (±4 dB)
    {
        auto thdSeviye = [&] (double tepeDb)
        {
            auto d = kirKur (fs, 0, 24.0f, 0.0f);
            auto l = sinus (fs, n, f0, tepeDb); auto r = l;
            calistir (d, l, r);
            return thdDb (l, fs, f0, a, b);
        };
        SWX_BEKLE (std::fabs (thdSeviye (-30.0) - thdSeviye (-12.0)) <= 4.0);
        SWX_BEKLE (thdSeviye (-30.0) > -20.0);
    }
    {
        auto d = kirKur (fs, 0, 24.0f, -36.0f);
        auto l = sinus (fs, n, f0, -12.0); auto r = l;
        calistir (d, l, r);
        SWX_BEKLE (thdDb (l, fs, f0, a, b) < -40.0);
    }
    // otomatik seviye: veri yolu seviyesi sürüşten bağımsız (drive 3 vs 30, ±3 dB)
    {
        auto busRms = [&] (float drive)
        {
            auto dTam = kirKur (fs, 0, drive, -12.0f);
            auto dRef = kirKur (fs, 0, drive, -36.0f);
            auto l1 = sinus (fs, n, f0, -12.0); auto r1 = l1;
            auto l2 = l1; auto r2 = l1;
            calistir (dTam, l1, r1);
            calistir (dRef, l2, r2);
            std::vector<float> fark ((size_t) n);
            for (int i = 0; i < n; ++i) fark[(size_t) i] = l1[(size_t) i] - l2[(size_t) i];
            return rmsDb (fark, a, b);
        };
        SWX_BEKLE (std::fabs (busRms (3.0f) - busRms (30.0f)) <= 3.0);
    }
    // latency: açık ve kapalı 12 örnek
    for (bool acik : { true, false })
    {
        DirtModule d;
        d.prepare (fs, 256, 2);
        DirtParams p; p.on = acik; p.mode = 0; p.driveDb = 12.0f; p.levelDb = -36.0f;
        d.setParams (p);
        d.setEnabled (acik, true);
        std::vector<float> l (256, 0.0f), r (256, 0.0f);
        l[20] = 0.5f; r[20] = 0.5f;
        calistir (d, l, r);
        int pk = 0; float mx = 0.0f;
        for (int i = 0; i < 256; ++i) if (std::fabs (l[(size_t) i]) > mx) { mx = std::fabs (l[(size_t) i]); pk = i; }
        SWX_BEKLE_EQ (pk, 20 + DirtModule::LATENCY);
    }
}

SWX_TEST (dirt_lofi_hisirti_sessizlik_ve_nan)
{
    const double fs = 48000.0;
    const int n = 48000, a = n / 2, b = n;
    // LOFI: harmonik-dışı taban (1000 Hz, 440'ın harmoniği değil). Referans AYNI seviyede (0 dB) ama 16 bit/tut 1:
    // fark bit azaltmadan gelmeli, veri yolunun açılmasından değil.
    auto lofiOlc = [&] (float bits, float rate, double hz)
    {
        auto d = kirKur (fs, 2, 0.0f, 0.0f, bits, rate);
        auto l = sinus (fs, n, 440.0, -12.0); auto r = l;
        calistir (d, l, r);
        return tonDb (l, fs, hz, a, b) - tonDb (l, fs, 440.0, a, b);
    };
    SWX_BEKLE (lofiOlc (5.0f, 1.0f, 1000.0) - lofiOlc (16.0f, 1.0f, 1000.0) >= 25.0);
    // tut (örnek tutma) ayrı bir eksen: tut 4 → fs/4 − f0 = 11560 Hz imgesi; tut 1'de yok (aynı 16 bit)
    SWX_BEKLE (lofiOlc (16.0f, 4.0f, 11560.0) - lofiOlc (16.0f, 1.0f, 11560.0) >= 20.0);
    // sessizlik: kir veri yolu gürültü taşımaz — dijital sıfır
    {
        auto d = kirKur (fs, 1, 36.0f, 0.0f);
        std::vector<float> l ((size_t) n, 0.0f), r ((size_t) n, 0.0f);
        calistir (d, l, r);
        SWX_BEKLE (rmsDb (l, a, b) < -120.0);
    }
    // ...ve gerçek oda: −70 dBFS gürültüde en sert ayar (Fuzz 36 dB, seviye 0) gürültüyü BÜYÜTMEZ
    {
        auto d = kirKur (fs, 1, 36.0f, 0.0f);
        std::vector<float> l ((size_t) n), r ((size_t) n);
        uint32_t tohum = 12345u;
        for (int i = 0; i < n; ++i)
        {
            tohum = tohum * 1664525u + 1013904223u;
            l[(size_t) i] = r[(size_t) i] = 3.16e-4f * 1.732f * (2.0f * (float) (tohum >> 8) / 16777216.0f - 1.0f);
        }
        const double girisDb = rmsDb (l, a, b);
        calistir (d, l, r);
        SWX_BEKLE (rmsDb (l, a, b) - girisDb <= 1.0);
    }
    // NaN: zehirli blok sonrası sonlu
    {
        auto d = kirKur (fs, 0, 24.0f, 0.0f);
        auto l = sinus (fs, 4096, 440.0, -12.0); auto r = l;
        l[100] = std::numeric_limits<float>::quiet_NaN();
        calistir (d, l, r);
        auto l2 = sinus (fs, 4096, 440.0, -12.0); auto r2 = l2;
        calistir (d, l2, r2);
        bool sonlu = true;
        for (float v : l2) sonlu = sonlu && std::isfinite (v);
        SWX_BEKLE (sonlu);
    }
}

SWX_TEST (dirt_reset_patlamasiz_ve_kapali_seviye_birebir)
{
    const double fs = 48000.0;
    const int n = 48000;
    // reset() sonrası ilk 50 ms tepe, oturmuş tepeden en fazla +3 dB (denetim: otoKazanc 1'den başlayınca +19 dB)
    {
        auto d = kirKur (fs, 0, 24.0f, 0.0f);
        auto l = sinus (fs, n, 220.0, -12.0); auto r = l;
        calistir (d, l, r);
        float oturmus = 0.0f;
        for (int i = n / 2; i < n; ++i) oturmus = std::max (oturmus, std::fabs (l[(size_t) i]));
        d.reset();
        auto l2 = sinus (fs, 2400, 220.0, -12.0); auto r2 = l2;
        calistir (d, l2, r2);
        float ilk = 0.0f;
        for (float v : l2) ilk = std::max (ilk, std::fabs (v));
        SWX_BEKLE (20.0 * std::log10 (ilk / std::max (oturmus, 1.0e-9f)) <= 3.0);
    }
    // seviye −36 = KAPALI: çıkış 12 örnek gecikmiş kuru sinyalle birebir (sürüş ne olursa olsun)
    {
        auto d = kirKur (fs, 1, 36.0f, -36.0f);
        auto l = sinus (fs, 8192, 330.0, -6.0); auto r = l;
        const auto in = l;
        calistir (d, l, r);
        double fark = 0.0;
        for (size_t i = (size_t) DirtModule::LATENCY; i < l.size(); ++i)
            fark = std::max (fark, (double) std::fabs (l[i] - in[i - (size_t) DirtModule::LATENCY]));
        SWX_BEKLE (fark < 1.0e-6);
    }
}

SWX_TEST (dirt_dc_yok_ve_blok_bagimsiz)
{
    const double fs = 48000.0;
    const int n = 48000;
    // DC (denetim M3.4): asimetrik Scream/Fuzz şekillendiricisinin DC'si kir yoluna sızmaz (yol ortalaması RMS'in −40 dB altı)
    for (int mod : { 0, 1 })
    {
        auto dm = kirKur (fs, mod, 26.0f, 0.0f);
        auto l = sinus (fs, n, 220.0, -12.0); auto r = l;
        const auto in = l;
        calistir (dm, l, r);
        double ort = 0.0, guc = 0.0; int c = 0;
        for (int i = n / 2; i < n; ++i) { const double w = (double) l[(size_t) i] - in[(size_t) (i - DirtModule::LATENCY)]; ort += w; guc += w * w; ++c; }
        ort /= c; guc /= c;
        SWX_BEKLE (20.0 * std::log10 (std::fabs (ort) + 1e-12) - 10.0 * std::log10 (guc + 1e-20) < -40.0);
    }
    // blok bağımsızlığı (denetim M3.4): 17 örneklik host bloğunda kir yolu 512'dekiyle aynı (≤ 0.1 dB)
    auto yolRms = [&] (int blok)
    {
        auto dm = kirKur (fs, 0, 26.0f, -8.0f);
        std::vector<float> l ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / fs;
            const double zarf = std::fmod (t, 0.25) < 0.15 ? 1.0 : 0.001;
            l[(size_t) i] = (float) (0.25 * zarf * std::sin (2.0 * PI * 220.0 * t));
        }
        auto r = l; const auto in = l;
        calistir (dm, l, r, blok);
        double g = 0.0; int c = 0;
        for (int i = n / 4; i < n; ++i) { const double w = (double) l[(size_t) i] - in[(size_t) (i - DirtModule::LATENCY)]; g += w * w; ++c; }
        return 10.0 * std::log10 (g / c + 1e-20);
    };
    SWX_BEKLE (std::fabs (yolRms (512) - yolRms (17)) <= 0.1);
    SWX_BEKLE (std::fabs (yolRms (512) - yolRms (1)) <= 0.1);
}

SWX_TEST (layer_oktav_besli_pan_ve_bos_birebir)
{
    const double fs = 48000.0;
    const int n = 48000, a = n / 2, b = n;
    const double f0 = 440.0;
    auto kur = [&] (float semiA, float semiB, float width)
    {
        LayerModule m;
        m.prepare (fs, 256, 2);
        LayerParams p; p.on = true; p.semiA = semiA; p.semiB = semiB; p.levelDb = 0.0f; p.width = width; p.toneHz = 12000.0f; p.grit = 0.0f;
        m.setParams (p);
        m.setEnabled (true, true);
        return m;
    };
    // −12: f/2 bileşeni güçlü — VOKAL PERDE TARAMASI (M3.4 regresyonu: sabit pencerede 220 Hz tam iptal,
    // 110 Hz −200 dB ölçülmüştü; periyot kilidiyle her perdede var olmalı). Blok 512 (Sandbox) ve düşük seviye.
    {
        double enKotu = 1.0e9;
        for (double hz : { 130.81, 146.83, 165.0, 185.0, 196.0, 220.0, 246.94, 261.63, 293.66, 330.0, 392.0 })
        {
            LayerModule m;
            m.prepare (fs, 512, 2);
            m.setEnabled (true, true);
            LayerParams p; p.on = true; p.semiA = -12.0f; p.semiB = 0.0f; p.levelDb = 0.0f; p.width = 0.0f; p.toneHz = 12000.0f;
            m.setParams (p);
            auto l = sinus (fs, n, hz, -25.0); auto r = l;
            calistir (m, l, r, 512);
            const double alt = tonDb (l, fs, hz * 0.5, a, b) - tonDb (l, fs, hz, a, b);
            enKotu = std::min (enKotu, alt);
        }
        SWX_BEKLE (enKotu > -12.0);
        auto in = sinus (fs, n, f0, -12.0);
        SWX_BEKLE (tonDb (in, fs, f0 * 0.5, a, b) - tonDb (in, fs, f0, a, b) < -60.0);
    }
    // +7: beşli (f·2^(7/12)) bileşeni
    {
        auto m = kur (7.0f, 0.0f, 0.0f);
        auto l = sinus (fs, n, f0, -12.0); auto r = l;
        calistir (m, l, r);
        SWX_BEKLE (tonDb (l, fs, f0 * std::pow (2.0, 7.0 / 12.0), a, b) - tonDb (l, fs, f0, a, b) > -12.0);
    }
    // iki ses, genişlik 1: A (−12) SOL, B (+12) SAĞ
    {
        auto m = kur (-12.0f, 12.0f, 1.0f);
        auto l = sinus (fs, n, f0, -12.0); auto r = l;
        calistir (m, l, r);
        SWX_BEKLE (tonDb (l, fs, f0 * 0.5, a, b) - tonDb (r, fs, f0 * 0.5, a, b) > 15.0);
        SWX_BEKLE (tonDb (r, fs, f0 * 2.0, a, b) - tonDb (l, fs, f0 * 2.0, a, b) > 15.0);
    }
    // ses yok (0/0): birebir
    {
        auto m = kur (0.0f, 0.0f, 0.5f);
        auto l = sinus (fs, 4096, f0, -12.0); auto r = l;
        const auto in = l;
        calistir (m, l, r);
        double fark = 0.0;
        for (size_t i = 0; i < l.size(); ++i) fark = std::max (fark, (double) std::fabs (l[i] - in[i]));
        SWX_BEKLE (fark < 1.0e-7);
    }
}
