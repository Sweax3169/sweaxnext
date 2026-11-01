// SWEAX NEXT — Tests/DSP/DeEsserTest.cpp
// Ölçülen: sentetik 's' (5–9 kHz gürültü) → hedef range ±1 dB; sentetik parlak 'i' (harmonik) → GR < 0.5 dB
// (gürültülülük kapısı); LR4 toplamı düz; latency sabit; wide mod; RT tahsis yok.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "DSP/DeEsser.h"
#include "Chain/VocalChain.h"
#include "Core/RtGuard.h"
#include "Core/Biquad.h"
#include <vector>
#include <random>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    void calistir (sweax::DeEsser& d, std::vector<float>& x, int blok = 256)
    {
        const int n = (int) x.size();
        for (int i = 0; i < n; i += blok) { float* ch[1] = { x.data() + i }; d.process (ch, 1, std::min (blok, n - i)); }
    }
    double bantRmsDb (const std::vector<float>& x, double fs, double lo, double hi, int a, int b)
    {
        sweax::Biquad h1, h2, l1, l2;
        h1.setCoeffs (sweax::design::highpass (fs, lo, 0.7071)); h2.setCoeffs (sweax::design::highpass (fs, lo, 0.7071));
        l1.setCoeffs (sweax::design::lowpass (fs, hi, 0.7071)); l2.setCoeffs (sweax::design::lowpass (fs, hi, 0.7071));
        double s = 0.0; int cnt = 0;
        for (int i = 0; i < (int) x.size(); ++i) { const float v = l2.processSample (l1.processSample (h2.processSample (h1.processSample (x[(size_t) i])))); if (i >= a && i < b) { s += (double) v * v; ++cnt; } }
        return 10.0 * std::log10 (s / std::max (1, cnt) + 1e-20);
    }
    std::vector<float> sesS (double fs, double sn, float rmsDb, unsigned tohum = 1)
    {
        std::mt19937 rng (tohum); std::normal_distribution<float> nd (0.0f, 1.0f);
        sweax::Biquad h1, h2, l1, l2;
        h1.setCoeffs (sweax::design::highpass (fs, 5000.0, 0.7071)); h2.setCoeffs (sweax::design::highpass (fs, 5000.0, 0.7071));
        l1.setCoeffs (sweax::design::lowpass (fs, 9000.0, 0.7071)); l2.setCoeffs (sweax::design::lowpass (fs, 9000.0, 0.7071));
        std::vector<float> x ((size_t) (fs * sn)); double e = 0.0;
        for (auto& v : x) { v = l2.processSample (l1.processSample (h2.processSample (h1.processSample (nd (rng))))); e += (double) v * v; }
        const float g = std::pow (10.0f, rmsDb / 20.0f) / (float) std::sqrt (e / (double) x.size());
        for (auto& v : x) v *= g;
        return x;
    }
    // parlak 'i': F0 220 Hz, harmonikler 1/h ama 5–8 kHz bölgesi +12 dB (parlak ünlü)
    std::vector<float> sesI (double fs, double sn, float peak)
    {
        std::vector<float> x ((size_t) (fs * sn)); double faz[64] = {};
        for (size_t i = 0; i < x.size(); ++i)
        {
            double s = 0.0;
            for (int h = 1; h <= 40; ++h)
            {
                const double fh = 220.0 * h; if (fh > fs * 0.45) break;
                double a = 1.0 / h; if (fh > 5000.0 && fh < 8000.0) a *= 4.0;
                faz[h] += 2.0 * PI * fh / fs; s += a * std::sin (faz[h]);
            }
            x[i] = (float) s;
        }
        float pk = 0.0f; for (float v : x) pk = std::max (pk, std::fabs (v));
        for (auto& v : x) v *= peak / pk;
        return x;
    }
}

SWX_TEST (deesser_s_ve_i_ayrimi)
{
    const double fs = 48000.0;
    sweax::DeEsser d; d.prepare (fs, 256, 1); d.setQuality (sweax::Quality::Yuksek); d.setEnabled (true, true);
    sweax::DeEssParams p; p.on = true; p.mode = 0; p.hz = 5000.0f; p.threshDb = -45.0f; p.rangeDb = 8.0f; p.lookaheadMs = 1.0f; p.attackMs = 1.0f; p.releaseMs = 60.0f;
    d.setParams (p);
    // 's': bant seviyesi eşiğin ≥10 dB üstünde → range 8 ile sınırlı → GR ≈ 8; ölçüm 8–9.5 kHz (LR4 @5 kHz alt bant sızıntısı ≤ −16 dB)
    if (std::getenv ("SWX_VERBOSE")) { std::printf ("   [1] s uret\n"); std::fflush (stdout); }
    auto s = sesS (fs, 1.0, -20.0f); auto sIn = s;
    if (std::getenv ("SWX_VERBOSE")) { std::printf ("   [2] s isle\n"); std::fflush (stdout); }
    calistir (d, s);
    if (std::getenv ("SWX_VERBOSE")) { std::printf ("   [3] s olc\n"); std::fflush (stdout); }
    const double azalma = bantRmsDb (sIn, fs, 8000.0, 9500.0, (int) fs / 2, (int) fs) - bantRmsDb (s, fs, 8000.0, 9500.0, (int) fs / 2, (int) fs);
    if (std::getenv ("SWX_VERBOSE")) std::printf ("   s: azalma %.2f dB, GR %.2f, ac=%.2f gate=%d\n", azalma, d.gainReductionDb(), d.autocorrPeak(), (int) d.harmonicGateActive());
    // Hedef ±1 dB = KAZANÇ hedefi (GR tam −8.00 ölçüldü). Net bant azalması fizik gereği daha az: LR4 alt bant
    // 8 kHz'de ≈ −16 dB sızar ve üst bantla AYNI FAZDA toplanır → 20·log10(10^(−8/20) + 10^(−16/20)) ≈ −5.1…−6.5 dB.
    SWX_BEKLE (azalma > 5.5 && azalma <= 8.2);
    SWX_BEKLE_YAKIN (d.gainReductionDb(), -8.0, 0.5);
    SWX_BEKLE (! d.harmonicGateActive());
    // alt bant dokunulmaz: 1 kHz sinüs + 's' karışımı → 1 kHz bandı değişmez (LR4 toplam düz)
    d.reset();
    auto mix = sesS (fs, 1.0, -20.0f, 5); for (size_t i = 0; i < mix.size(); ++i) mix[i] += 0.3f * (float) std::sin (2.0 * PI * 1000.0 * (double) i / fs);
    auto mixIn = mix; calistir (d, mix);
    SWX_BEKLE_YAKIN (bantRmsDb (mix, fs, 500.0, 2000.0, (int) fs / 2, (int) fs), bantRmsDb (mixIn, fs, 500.0, 2000.0, (int) fs / 2, (int) fs), 0.2);
    // parlak 'i': harmonik → kapı → GR < 0.5 dB (bant seviyesi eşiğin çok üstünde olsa da)
    d.reset();
    auto v = sesI (fs, 1.0, 0.5f); auto vIn = v;
    calistir (d, v);
    const double azalmaI = bantRmsDb (vIn, fs, 6000.0, 9000.0, (int) fs / 2, (int) fs) - bantRmsDb (v, fs, 6000.0, 9000.0, (int) fs / 2, (int) fs);
    if (std::getenv ("SWX_VERBOSE")) std::printf ("   i: azalma %.2f dB, GR %.2f, ac=%.2f gate=%d bant=%.1f dB\n", azalmaI, d.gainReductionDb(), d.autocorrPeak(), (int) d.harmonicGateActive(), bantRmsDb (vIn, fs, 6000.0, 9000.0, (int) fs / 2, (int) fs));
    SWX_BEKLE (azalmaI < 0.5);
    SWX_BEKLE (d.harmonicGateActive());
    // wide mod: 's'te tüm sinyal azalır (1 kHz taşıyıcı da)
    d.reset(); p.mode = 1; d.setParams (p);
    auto w = sesS (fs, 1.0, -20.0f, 9); for (size_t i = 0; i < w.size(); ++i) w[i] += 0.05f * (float) std::sin (2.0 * PI * 1000.0 * (double) i / fs);
    auto wIn = w; calistir (d, w);
    SWX_BEKLE_YAKIN (bantRmsDb (wIn, fs, 500.0, 2000.0, (int) fs / 2, (int) fs) - bantRmsDb (w, fs, 500.0, 2000.0, (int) fs / 2, (int) fs), 8.0, 1.2);
}

SWX_TEST (deesser_lr4_duz_latency_rt)
{
    const double fs = 48000.0;
    sweax::DeEsser d; d.prepare (fs, 256, 2); d.setQuality (sweax::Quality::Ultra); d.setEnabled (true, true);
    sweax::DeEssParams p; p.on = true; p.threshDb = 0.0f; p.rangeDb = 8.0f; d.setParams (p);        // eşik altı: GR 0
    SWX_BEKLE_EQ (d.latencySamples (sweax::Quality::Canli), 0);
    SWX_BEKLE_EQ (d.latencySamples (sweax::Quality::Ultra), 144);
    // LR4 toplamı: 200 Hz … 15 kHz sinüslerde kazanç 0 ± 0.1 dB
    for (double hz : { 200.0, 1000.0, 4000.0, 6500.0, 10000.0, 15000.0 })
    {
        std::vector<float> L ((size_t) fs / 2), R = L;
        for (size_t i = 0; i < L.size(); ++i) { L[i] = 0.25f * (float) std::sin (2.0 * PI * hz * (double) i / fs); R[i] = L[i]; }
        for (int i = 0; i < (int) L.size(); i += 256) { float* ch[2] = { L.data() + i, R.data() + i }; d.process (ch, 2, std::min (256, (int) L.size() - i)); }
        double s = 0.0; for (int i = (int) L.size() / 2; i < (int) L.size(); ++i) s += (double) L[(size_t) i] * L[(size_t) i];
        SWX_BEKLE_YAKIN (10.0 * std::log10 (s / (L.size() / 2) / (0.25 * 0.25 * 0.5)), 0.0, 0.1);
    }
    // gecikme: dürtü 144 örnek sonra (wide mod: saf gecikme; split modda LR4 dürtüyü yayar, latency aynı)
    d.reset(); p.mode = 1; d.setParams (p);
    std::vector<float> L ((size_t) 2048, 0.0f), R = L; L[100] = 1.0f; R[100] = 1.0f;
    for (int i = 0; i < 2048; i += 256) { float* ch[2] = { L.data() + i, R.data() + i }; d.process (ch, 2, 256); }
    int at = -1; float pk = 0.0f; for (int i = 0; i < 2048; ++i) if (std::fabs (L[(size_t) i]) > pk) { pk = std::fabs (L[(size_t) i]); at = i; }
    SWX_BEKLE_EQ (at, 244);
    // RT
    sweax::rt::resetViolations();
    {
        sweax::rt::Scope s;
        for (int k = 0; k < 200; ++k) { p.hz = 5000.0f + (float) (k % 30) * 100.0f; p.mode = k % 2; p.link = (k % 3) != 0; d.setParams (p); float* ch[2] = { L.data(), R.data() }; d.process (ch, 2, 256); }
    }
    SWX_BEKLE_EQ (sweax::rt::violations(), 0u);
}

SWX_TEST (deesser_goreli_mod_govdeye_gore)
{
    // M3.4 denetim T3: göreli mod s bandını GÖVDEYE göre ölçer — gövde kısıldığında (kompresyon) s aynı mutlak seviyede
    // kalsa bile öne çıkma artar ve GR buna göre gelir; sesli harf (harmonik) GR almaz
    using namespace sweax;
    const double fs = 48000.0;
    auto sahne = [&] (float govdeDb, float sDb)
    {
        // 0.3 s gövde (220 Hz, 20 harmonik) + ortada 80 ms 's' (5-9 kHz gürültü) — gövde s sırasında da sürer
        std::vector<float> x ((size_t) (0.6 * fs));
        double faz = 0.0; const double a = std::pow (10.0, govdeDb / 20.0);
        for (size_t i = 0; i < x.size(); ++i)
        {
            faz += 2.0 * PI * 220.0 / fs; double v = 0.0;
            for (int h = 1; h <= 12; ++h) v += std::sin (h * faz) / h;
            x[i] = (float) (a * 0.6 * v);
        }
        auto s = sesS (fs, 0.08, sDb, 7);
        const size_t bas = (size_t) (0.3 * fs);
        for (size_t i = 0; i < s.size(); ++i) x[bas + i] += s[i];
        return x;
    };
    auto grOlc = [&] (float govdeDb, float sDb, float esik)
    {
        DeEsser dm; dm.prepare (fs, 256, 1); dm.setQuality (Quality::Canli);
        DeEssParams p; p.on = true; p.mode = 0; p.hz = 5000.0f; p.rangeDb = 12.0f; p.attackMs = 1.0f; p.releaseMs = 60.0f;
        p.lookaheadMs = 0.0f; p.goreli = true; p.goreliEsikDb = esik;
        dm.setParams (p); dm.setEnabled (true, true);
        auto x = sahne (govdeDb, sDb); const auto kuru = x;
        calistir (dm, x);
        const int a = (int) (0.33 * fs), b = (int) (0.37 * fs);
        return bantRmsDb (kuru, fs, 5000.0, 9000.0, a, b) - bantRmsDb (x, fs, 5000.0, 9000.0, a, b);
    };
    const double normal = grOlc (-18.0f, -24.0f, 0.0f);     // s gövdenin belirgin altında: GR yok denecek kadar az
    const double ezik = grOlc (-34.0f, -24.0f, 0.0f);       // aynı s, gövde 16 dB kısık (kompresyon): öne çıktı → GR
    if (std::getenv ("SWX_VERBOSE")) std::printf ("   goreli GR: normal %.1f dB, govde ezik %.1f dB\n", normal, ezik);
    SWX_BEKLE (normal < 2.0);
    SWX_BEKLE (ezik > 5.0);
    SWX_BEKLE (ezik - normal > 4.0);
    // sesli harf (parlak 'i', harmonik): göreli modda kapı yok ama s-bandı gövdenin çok altında → GR almaz
    {
        DeEsser dm; dm.prepare (fs, 256, 1); dm.setQuality (Quality::Canli);
        DeEssParams p; p.on = true; p.mode = 0; p.hz = 5000.0f; p.rangeDb = 12.0f; p.goreli = true; p.goreliEsikDb = -6.0f;
        dm.setParams (p); dm.setEnabled (true, true);
        auto x = sesI (fs, 0.6, 0.3f); const auto kuru = x;
        calistir (dm, x);
        SWX_BEKLE (bantRmsDb (kuru, fs, 5000.0, 9000.0, 12000, 28000) - bantRmsDb (x, fs, 5000.0, 9000.0, 12000, 28000) < 1.0);
    }
    // denetim dsp-rt-2: tek NaN örnek gövde dedektörünü kalıcı zehirleyip sürekli tam GR'ye kilitlemez
    {
        DeEsser dm; dm.prepare (fs, 256, 1); dm.setQuality (Quality::Canli);
        DeEssParams p; p.on = true; p.mode = 0; p.hz = 5000.0f; p.rangeDb = 6.0f; p.goreli = true; p.goreliEsikDb = -4.0f;
        dm.setParams (p); dm.setEnabled (true, true);
        auto zeh = sesI (fs, 0.3, 0.3f); zeh[1000] = std::numeric_limits<float>::quiet_NaN();
        calistir (dm, zeh);
        auto x = sesI (fs, 2.0, 0.3f); const auto kuru = x;
        calistir (dm, x);
        const int a = (int) (1.5 * fs), b = (int) (2.0 * fs);
        SWX_BEKLE (bantRmsDb (kuru, fs, 5000.0, 9000.0, a, b) - bantRmsDb (x, fs, 5000.0, 9000.0, a, b) < 1.0);
    }
    // denetim dsp-rt-4: link KAPALI, yalnız sol kanalda vokal — gövde kanal başına ölçülür, mono ortalamanın 6 dB
    // düşük gövdesiyle fazladan kısmaz (link açık tek kanallı sonuçla aynı)
    {
        auto grKanal = [&] (bool ikiKanal)
        {
            DeEsser dm; dm.prepare (fs, 256, ikiKanal ? 2 : 1); dm.setQuality (Quality::Canli);
            DeEssParams p; p.on = true; p.mode = 0; p.hz = 5000.0f; p.rangeDb = 12.0f; p.goreli = true; p.goreliEsikDb = 0.0f; p.link = ! ikiKanal;
            dm.setParams (p); dm.setEnabled (true, true);
            std::vector<float> l ((size_t) (0.6 * fs)); double faz = 0.0;
            for (size_t i = 0; i < l.size(); ++i) { faz += 2.0 * PI * 220.0 / fs; double v = 0.0; for (int h = 1; h <= 12; ++h) v += std::sin (h * faz) / h; l[i] = (float) (0.1 * 0.6 * v); }
            auto s = sesS (fs, 0.08, -24.0f, 7); const size_t bas = (size_t) (0.3 * fs);
            for (size_t i = 0; i < s.size(); ++i) l[bas + i] += s[i];
            const auto kuru = l; std::vector<float> r (l.size(), 0.0f);
            for (size_t i = 0; i < l.size(); i += 256)
            {
                float* ch[2] = { l.data() + i, r.data() + i };
                dm.process (ch, ikiKanal ? 2 : 1, (int) std::min<size_t> (256, l.size() - i));
            }
            const int a = (int) (0.33 * fs), b = (int) (0.37 * fs);
            return bantRmsDb (kuru, fs, 5000.0, 9000.0, a, b) - bantRmsDb (l, fs, 5000.0, 9000.0, a, b);
        };
        const double tek = grKanal (false), stereoLinkYok = grKanal (true);
        if (std::getenv ("SWX_VERBOSE")) std::printf ("   link yok sol-kanal GR %.1f dB, tek kanal %.1f dB\n", stereoLinkYok, tek);
        SWX_BEKLE (std::fabs (stereoLinkYok - tek) < 1.0);
    }
}
