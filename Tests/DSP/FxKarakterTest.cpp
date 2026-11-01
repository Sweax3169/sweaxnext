// SWEAX NEXT — Tests/DSP/FxKarakterTest.cpp
// M3.5 FX KARAKTER: reverb ODA TİPİ, delay 1/16 + GENİŞLİK, saturator KARAKTER.
// Ölçülen:
//  • Varsayılanlar eski çıkış: üç modül × 3 (örnekleme hızı, parametre seti) parmak izi (enerji, L·R, zamana
//    ağırlıklı enerji, fark enerjisi) 1e-5 göreli toleransla; önce bit özetiydi (C çalışma zamanına bağlıydı).
//  • Reverb: her tipte T60 (Schroeder geri entegrasyon, −5…−25 dB eğimi) hedefin ±%25'i; erken enerji
//    (ilk 80 ms / toplam) Oda > Plate > Salon; tip geçişinde sıçrama sınırlı, NaN yok.
//  • Delay: 120 bpm 1/16 → ilk yankı 125 ms ±1 örnek; width 0 → L ve R örnek örnek EŞİT; width 0 ilk yankı
//    enerjisi width 1'in ±1.5 dB'si içinde.
//  • Saturator: her karakterde −40 dBFS 1 kHz kazanç 0 ±0.3 dB; Bant tek-harmonik (H3 > H2, H2 ≤ −45 dBc);
//    Kırık THD > Tüp THD ve H2 ≥ −40 dBc; transfer eğrisi −2…+2 MONOTON; karakter geçişi sıçramasız.
//  • Üç modülde de RtGuard 0 ihlal, NaN/Inf sigortası, blok boyutu (1/7/64/256/4096) değişmezliği.
//  • Bölüm 5: M3.5 denetim regresyonları (geçiş üstüne geçiş, reset sonrası rampa, tip iptali, T60
//    tablosu, kuyruk yoğunluğu + slap eşitliği, alias bütçesi, karakter seviye eşitliği, delay mono/fb).
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "DSP/PlateReverb.h"
#include "DSP/StereoDelay.h"
#include "DSP/Saturator.h"
#include "Chain/VocalChain.h"
#include "Core/RtGuard.h"
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>
#include <algorithm>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    constexpr int ONISI = 4800;          // mix rampası (15 ms) ve iç durumlar otursun → sonra uyarım

    template <typename M>
    void calistir (M& m, std::vector<float>& l, std::vector<float>& r, int blok = 256)
    {
        const int n = (int) l.size();
        for (int i = 0; i < n; i += blok)
        {
            float* ch[2] = { l.data() + i, r.data() + i };
            m.process (ch, 2, std::min (blok, n - i));
        }
    }

    // ---- bit-birebir özet (deterministik sinyal + FNV-1a) ----
    void sinyal (double fs, int n, std::vector<float>& l, std::vector<float>& r)
    {
        l.assign ((size_t) n, 0.0f); r.assign ((size_t) n, 0.0f);
        uint32_t s = 20260917u;
        const int per = (int) std::lround (0.5 * fs), len = (int) std::lround (0.12 * fs);
        for (int i = 0; i < n; ++i)
        {
            s = s * 1664525u + 1013904223u;
            const float g = (float) ((double) (s >> 9) / 4194304.0 - 1.0) * 0.03f;
            const int ph = i % std::max (1, per);
            float bl = 0.0f, br = 0.0f;
            if (ph < len)
            {
                const double w = 0.5 - 0.5 * std::cos (2.0 * PI * ph / std::max (1, len));
                bl = (float) (0.45 * w * std::sin (2.0 * PI * 220.0 * i / fs));
                br = (float) (0.40 * w * std::sin (2.0 * PI * 330.0 * i / fs));
            }
            l[(size_t) i] = bl + g; r[(size_t) i] = br - g;
        }
    }
    // Çıkış parmak izi: toplam enerji, L·R çapraz terimi, zamana ağırlıklı enerji (kuyruk/zarf) ve fark enerjisi
    // (tiz içerik). İlk sürüm FNV-1a ile BİT BİT özet saklıyordu; aynı kod 2026-11'de bu makinede farklı bitler
    // verdi (değişmemiş d628fd8 de düştü): Windows'un C çalışma zamanı sin/tanh'ı güncellenince son ulp'ler oynuyor.
    // Bir ulp'lik oynama bu toplamları ~1e-7 kadar kıpırdatır; farklı bir işleme yolu ise yüzdelerle değiştirir.
    struct Iz { double e, x, t, d; };
    Iz parmakIzi (const std::vector<float>& l, const std::vector<float>& r)
    {
        Iz z { 0.0, 0.0, 0.0, 0.0 };
        const double n = (double) l.size();
        for (size_t i = 0; i < l.size(); ++i)
        {
            const double a = l[i], b = r[i], en = a * a + b * b;
            z.e += en; z.x += a * b; z.t += en * ((double) i / n);
            if (i > 0)
            {
                const double da = a - l[i - 1], db = b - r[i - 1];
                z.d += da * da + db * db;
            }
        }
        return z;
    }
    template <typename M, typename P>
    Iz ozet (const P& p, double fs)
    {
        const int n = (int) std::lround (3.0 * fs);
        std::vector<float> l, r; sinyal (fs, n, l, r);
        M m; m.prepare (fs, 256, 2); m.setParams (p); m.setEnabled (true, true);
        calistir (m, l, r);
        const auto z = parmakIzi (l, r);
        if (std::getenv ("SWX_VERBOSE")) std::printf ("   iz { %.9e, %.9e, %.9e, %.9e }\n", z.e, z.x, z.t, z.d);
        return z;
    }
    bool ayni (const Iz& a, const Iz& b)
    {
        // L·R terimi sıfıra yakın olabilir → onu da toplam enerji ölçeğinde karşılaştır
        auto yakin = [] (double u, double v, double olcek) { return std::fabs (u - v) <= 1.0e-5 * olcek; };
        return yakin (a.e, b.e, b.e) && yakin (a.x, b.x, b.e) && yakin (a.t, b.t, b.t) && yakin (a.d, b.d, b.d);
    }

    // ---- ölçüm yardımcıları ----
    double enerji (const std::vector<float>& l, const std::vector<float>& r, int a, int b)
    {
        double s = 0.0;
        for (int i = a; i < b && i < (int) l.size(); ++i)
            s += (double) l[(size_t) i] * l[(size_t) i] + (double) r[(size_t) i] * r[(size_t) i];
        return s;
    }
    // Schroeder geri entegrasyon: −5 dB ve −25 dB geçişlerinden T20 → T60 (klasik ISO 3382 yordamı)
    double t60Schroeder (const std::vector<float>& l, const std::vector<float>& r, int bas, double fs)
    {
        const int n = (int) l.size();
        std::vector<double> edc ((size_t) (n - bas) + 1, 0.0);
        double acc = 0.0;
        for (int i = n - 1; i >= bas; --i)
        {
            acc += (double) l[(size_t) i] * l[(size_t) i] + (double) r[(size_t) i] * r[(size_t) i];
            edc[(size_t) (i - bas)] = acc;
        }
        if (acc <= 0.0) return 0.0;
        const double ref = edc[0];
        int n5 = -1, n25 = -1;
        for (size_t k = 0; k < edc.size(); ++k)
        {
            const double db = 10.0 * std::log10 (edc[k] / ref + 1e-300);
            if (n5 < 0 && db <= -5.0) n5 = (int) k;
            if (n25 < 0 && db <= -25.0) { n25 = (int) k; break; }
        }
        if (n5 < 0 || n25 <= n5) return 0.0;
        return 3.0 * (double) (n25 - n5) / fs;                 // T20 → T60
    }
    // tek frekans genliği (Goertzel yerine doğrudan DFT; a..b tam periyot seçilir)
    double tonMag (const std::vector<float>& x, double fs, double f, int a, int b)
    {
        double re = 0.0, im = 0.0;
        for (int i = a; i < b; ++i)
        {
            const double w = 2.0 * PI * f * (i - a) / fs;
            re += x[(size_t) i] * std::cos (w); im -= x[(size_t) i] * std::sin (w);
        }
        return 2.0 * std::sqrt (re * re + im * im) / (double) (b - a);
    }
    double enBuyukAdim (const std::vector<float>& x, int a, int b)
    {
        double m = 0.0;
        for (int i = std::max (1, a); i < b && i < (int) x.size(); ++i)
            m = std::max (m, (double) std::fabs (x[(size_t) i] - x[(size_t) i - 1]));
        return m;
    }
    bool hepsiSonlu (const std::vector<float>& x, int bas = 0)
    {
        for (size_t i = (size_t) bas; i < x.size(); ++i) if (! std::isfinite (x[i])) return false;
        return true;
    }
    // aynı giriş, farklı blok boyutları → aynı çıkış (≤1e-5)
    template <typename M, typename P>
    double blokFarki (const P& p, double fs)
    {
        const int n = 24000;
        std::vector<float> l0, r0; sinyal (fs, n, l0, r0);
        std::vector<float> ref = l0, refR = r0;
        { M m; m.prepare (fs, 4096, 2); m.setParams (p); m.setEnabled (true, true); calistir (m, ref, refR, 256); }
        double mx = 0.0;
        for (int blok : { 1, 7, 64, 256, 4096 })
        {
            std::vector<float> l = l0, r = r0;
            M m; m.prepare (fs, 4096, 2); m.setParams (p); m.setEnabled (true, true); calistir (m, l, r, blok);
            for (int i = 0; i < n; ++i)
                mx = std::max (mx, std::max ((double) std::fabs (l[(size_t) i] - ref[(size_t) i]),
                                             (double) std::fabs (r[(size_t) i] - refR[(size_t) i])));
        }
        return mx;
    }
}

// =====================================================================================================
// 1) VARSAYILAN ÇIKIŞ KORUNUYOR (parmak izi)
// Bit özetleri M3.5 öncesi (commit 1369050) koddan alınmıştı; o koda bit bit eşit olduğu doğrulanmış kodun
// parmak izleri bugün sabit yazılı. Yeni alanların
// varsayılanı (type 0 / width 1 / character 0) eski yolu BİREBİR seçmek zorunda.
// =====================================================================================================
SWX_TEST (fxkarakter_varsayilan_bit_birebir_ozet)
{
    using namespace sweax;
    // parmak izleri: aynı kod (bit özetleri 2026-09'da geçen d628fd8) bu sürümle ölçüldü; SWX_VERBOSE=1 yeniden basar
    const Iz IZ_r0 { 2.447703033e+03, -4.404810624e+01, 1.074765218e+03, 1.769193502e+02 };
    const Iz IZ_r1 { 2.220142419e+03, -4.512006878e+01, 9.739305621e+02, 1.628920790e+02 };
    const Iz IZ_r2 { 4.993538760e+03, -8.088110395e+01, 2.197251668e+03, 3.476516101e+02 };
    const Iz IZ_d0 { 2.438364166e+03, -4.409285908e+01, 1.070102171e+03, 1.769109027e+02 };
    const Iz IZ_d1 { 2.253887535e+03, -4.118772578e+01, 9.907090057e+02, 1.629602455e+02 };
    const Iz IZ_d2 { 4.857546038e+03, -7.773312016e+01, 2.129828382e+03, 3.476093861e+02 };
    const Iz IZ_s0 { 2.180545826e+03, -3.634869348e+01, 9.569900330e+02, 1.294742084e+02 };
    const Iz IZ_s1 { 1.636743225e+03, -6.147325389e+01, 7.232981260e+02, 2.359203960e+02 };
    const Iz IZ_s2 { 4.654534796e+03, -4.673564183e+01, 2.038365871e+03, 1.764711845e+02 };
    ReverbParams r0; r0.on = true;
    ReverbParams r1; r1.on = true; r1.mix = 0.30f; r1.preDelayMs = 20.0f; r1.decaySec = 2.36f; r1.dampHz = 8000.0f; r1.hpHz = 370.0f; r1.width = 0.8f; r1.duckDb = 3.0f;
    ReverbParams r2; r2.on = true; r2.mix = 0.50f; r2.decaySec = 0.8f;
    SWX_BEKLE (ayni (ozet<PlateReverb> (r0, 48000.0), IZ_r0));
    SWX_BEKLE (ayni (ozet<PlateReverb> (r1, 44100.0), IZ_r1));
    SWX_BEKLE (ayni (ozet<PlateReverb> (r2, 96000.0), IZ_r2));

    DelayParams d0; d0.on = true;
    DelayParams d1; d1.on = true; d1.sync = 1; d1.bpm = 140.0; d1.feedback = 0.60f; d1.mix = 0.40f;
    DelayParams d2; d2.on = true; d2.sync = 3; d2.timeMs = 250.0f; d2.duckDb = 12.0f;
    SWX_BEKLE (ayni (ozet<StereoDelay> (d0, 48000.0), IZ_d0));
    SWX_BEKLE (ayni (ozet<StereoDelay> (d1, 44100.0), IZ_d1));
    SWX_BEKLE (ayni (ozet<StereoDelay> (d2, 96000.0), IZ_d2));

    SatParams s0; s0.on = true;
    SatParams s1; s1.on = true; s1.driveDb = 10.0f; s1.toneDb = 4.0f; s1.mix = 0.7f;
    SatParams s2; s2.on = true; s2.driveDb = 0.0f; s2.toneDb = -6.0f; s2.mix = 0.5f;
    SWX_BEKLE (ayni (ozet<Saturator> (s0, 48000.0), IZ_s0));
    SWX_BEKLE (ayni (ozet<Saturator> (s1, 44100.0), IZ_s1));
    SWX_BEKLE (ayni (ozet<Saturator> (s2, 96000.0), IZ_s2));

    // parmak izi ayırt edici: varsayılan dışı karakter aynı izi VERMEZ
    { ReverbParams t = r0; t.type = 1; SWX_BEKLE (! ayni (ozet<PlateReverb> (t, 48000.0), IZ_r0)); }
    { DelayParams t = d0; t.width = 0.5f; SWX_BEKLE (! ayni (ozet<StereoDelay> (t, 48000.0), IZ_d0)); }
    { SatParams t = s0; t.character = 1; SWX_BEKLE (! ayni (ozet<Saturator> (t, 48000.0), IZ_s0)); }

    // yeni alanların varsayılanı gerçekten eski davranış
    SWX_BEKLE_EQ (ReverbParams {}.type, 0);
    SWX_BEKLE_EQ (DelayParams {}.width, 1.0f);
    SWX_BEKLE_EQ (SatParams {}.character, 0);
}

// =====================================================================================================
// 2) REVERB ODA TİPİ
// =====================================================================================================
namespace
{
    // dürtü yanıtı: mix rampası otursun diye uyarım ONISI'de
    void reverbIR (double fs, int tip, float decaySec, std::vector<float>& l, std::vector<float>& r)
    {
        using namespace sweax;
        const int n = ONISI + (int) std::lround (5.0 * fs);
        l.assign ((size_t) n, 0.0f); r.assign ((size_t) n, 0.0f);
        l[(size_t) ONISI] = 1.0f; r[(size_t) ONISI] = 1.0f;
        PlateReverb rv; rv.prepare (fs, 256, 2);
        ReverbParams p; p.on = true; p.mix = 1.0f; p.preDelayMs = 0.0f; p.decaySec = decaySec;
        p.dampHz = 12000.0f; p.hpHz = 100.0f; p.width = 1.0f; p.duckDb = 0.0f; p.type = tip;
        rv.setParams (p); rv.setEnabled (true, true);
        calistir (rv, l, r);
    }
}

SWX_TEST (reverb_tip_t60_ve_erken_enerji)
{
    using namespace sweax;
    const double fs = 48000.0;
    const float hedef = 1.5f;
    double erkenOran[PlateReverb::TIP_SAYISI] {};
    for (int tip = 0; tip < PlateReverb::TIP_SAYISI; ++tip)
    {
        std::vector<float> l, r;
        reverbIR (fs, tip, hedef, l, r);
        SWX_BEKLE (hepsiSonlu (l) && hepsiSonlu (r));
        const int bas = ONISI + 1;                              // kuru dürtü örneği dışarıda
        const double t60 = t60Schroeder (l, r, bas, fs);
        SWX_BEKLE (t60 > (double) hedef * 0.75);
        SWX_BEKLE (t60 < (double) hedef * 1.25);
        const int ms80 = (int) std::lround (0.080 * fs);
        erkenOran[tip] = enerji (l, r, bas, bas + ms80) / std::max (1e-30, enerji (l, r, bas, (int) l.size()));
    }
    // ÖLÇÜLEN (48k, decay 1.5 s, damp 12k, hp 100; M3.5 denetim düzeltmeleri sonrası): T60 Plate 1.442 /
    // Oda 1.378 / Salon 1.515 / Chamber 1.460 s — hepsi hedefin %8'i içinde (test sınırı ±%25). Erken 80 ms
    // enerji payı: Oda 0.540 > Chamber 0.399 > Plate 0.250 > Salon 0.225.
    // erken yansıma yoğunluğu: Oda (kısa tank + ER) > Plate > Salon (uzun tank, geç yoğunlaşır)
    SWX_BEKLE (erkenOran[1] > erkenOran[0]);
    SWX_BEKLE (erkenOran[0] > erkenOran[2]);
}

SWX_TEST (reverb_tip_gecisi_tiksiz_ve_rt_temiz)
{
    using namespace sweax;
    const double fs = 48000.0;
    PlateReverb rv; rv.prepare (fs, 256, 2);
    ReverbParams p; p.on = true; p.mix = 0.6f; p.preDelayMs = 20.0f; p.decaySec = 1.4f;
    p.dampHz = 9000.0f; p.hpHz = 150.0f; p.width = 1.0f; p.duckDb = 0.0f; p.type = 0;
    rv.setParams (p); rv.setEnabled (true, true);

    // DENETİM (bulgu 9): 220 Hz ve 19200'lük anahtar aralığı tam 88 periyot — faz eklenmezse her tip değişimi
    // sinüsün SIFIR GEÇİŞİNE denk gelir ve test crossfade kalksa bile geçerdi. +π/2 fazla anahtar anları artık
    // sinüs TEPESİNDE (girişin en duyarlı anı).
    const int n = (int) std::lround (3.0 * fs);
    std::vector<float> l ((size_t) n), r ((size_t) n);
    for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = 0.30f * (float) std::sin (2.0 * PI * 220.0 * i / fs + 0.5 * PI);

    // kararlı durum (Plate) adım referansı
    const int blok = 256;
    int i = 0;
    for (; i < 48000; i += blok) { float* ch[2] = { l.data() + i, r.data() + i }; rv.process (ch, 2, blok); }
    const double sakinAdim = enBuyukAdim (l, 24000, 48000);
    // 4 tip arasında gez: her 0.4 s'de bir tip değiştir
    int tip = 0;
    for (; i + blok <= n; i += blok)
    {
        if ((i % 19200) == 0) { tip = (tip + 1) % PlateReverb::TIP_SAYISI; p.type = tip; rv.setParams (p); }
        float* ch[2] = { l.data() + i, r.data() + i };
        rv.process (ch, 2, blok);
    }
    const double gecisAdim = enBuyukAdim (l, 48000, i);
    SWX_BEKLE (hepsiSonlu (l) && hepsiSonlu (r));
    // ÖLÇÜLEN (tepe fazında): kararlı Plate'te en büyük örnek adımı ~0.008; 5 tip geçişi boyunca ×~1.1 —
    // 30 ms sön + tank temizle + 30 ms giriş rampası. (Giriş rampası eklenmeden önce ×12'ydi: 0.097.)
    SWX_BEKLE (gecisAdim < 2.0 * sakinAdim);

    // RT: tip dahil parametreler her blokta değişse de tahsis/kilit yok
    rv.reset();
    rt::resetViolations();
    {
        rt::Scope s;
        for (int k = 0; k < 400; ++k)
        {
            p.type = k % PlateReverb::TIP_SAYISI;
            p.decaySec = 0.8f + 0.01f * (float) (k % 50);
            rv.setParams (p);
            float* ch[2] = { l.data(), r.data() };
            rv.process (ch, 2, 256);
        }
    }
    SWX_BEKLE_EQ (rt::violations(), 0u);
    SWX_BEKLE (hepsiSonlu (l) && hepsiSonlu (r));
}

SWX_TEST (reverb_tip_nan_sigortasi_ve_blok_degismezligi)
{
    using namespace sweax;
    const double fs = 48000.0;
    for (int tip = 0; tip < PlateReverb::TIP_SAYISI; ++tip)
    {
        PlateReverb rv; rv.prepare (fs, 256, 2);
        ReverbParams p; p.on = true; p.mix = 0.5f; p.type = tip; p.duckDb = 0.0f;
        rv.setParams (p); rv.setEnabled (true, true);
        std::vector<float> l ((size_t) 24000, 0.1f), r ((size_t) 24000, 0.1f);
        l[100] = std::numeric_limits<float>::quiet_NaN();
        r[200] = std::numeric_limits<float>::infinity();
        l[300] = -std::numeric_limits<float>::infinity();
        calistir (rv, l, r);
        // reverb kuru yola dokunmaz (wet EKLER) → bozuk giriş ÖRNEĞİ kuru olarak geçer; sigortanın işi İÇ
        // DURUMU korumak: uyarımdan sonraki tüm örnekler sonlu (tank/difüzör/HP zehirlenmedi)
        SWX_BEKLE (hepsiSonlu (l, 400) && hepsiSonlu (r, 400));

        ReverbParams q; q.on = true; q.mix = 0.4f; q.type = tip; q.duckDb = 0.0f; q.decaySec = 1.2f;
        SWX_BEKLE (blokFarki<PlateReverb> (q, fs) <= 1.0e-5);
    }
}

// =====================================================================================================
// 3) DELAY — 1/16 senkron + GENİŞLİK
// =====================================================================================================
SWX_TEST (delay_16lik_senkron_ornek_dogru)
{
    using namespace sweax;
    const double fs = 48000.0;
    StereoDelay d; d.prepare (fs, 256, 2);
    DelayParams p; p.on = true; p.mix = 1.0f; p.sync = 4; p.bpm = 120.0; p.feedback = 0.0f;
    p.hpHz = 60.0f; p.lpHz = 16000.0f; p.duckDb = 0.0f; p.width = 1.0f;
    d.setParams (p); d.setEnabled (true, true);
    const int n = 48000;
    std::vector<float> l ((size_t) n, 0.0f), r ((size_t) n, 0.0f);
    l[(size_t) ONISI] = 1.0f; r[(size_t) ONISI] = 1.0f;
    calistir (d, l, r);
    // 120 bpm → 1/4 = 500 ms, 1/16 = 125 ms = 6000 örnek
    const int bekle = ONISI + 6000;
    double tepe = 0.0;
    for (int i = bekle - 200; i < bekle + 200; ++i) tepe = std::max (tepe, (double) std::fabs (l[(size_t) i]));
    int ilk = -1;
    for (int i = ONISI + 1000; i < n; ++i) if (std::fabs (l[(size_t) i]) > 0.05 * tepe) { ilk = i; break; }
    SWX_BEKLE (ilk >= bekle - 1 && ilk <= bekle + 1);           // ÖLÇÜLEN: tam 6000 örnek (sapma 0)
    // mevcut 0..3 anlamı değişmedi: 1/8 = 250 ms, 1/4 = 500 ms
    for (auto tst : { std::pair<int, int> { 1, 12000 }, std::pair<int, int> { 3, 24000 } })
    {
        StereoDelay d2; d2.prepare (fs, 256, 2);
        DelayParams q = p; q.sync = tst.first; d2.setParams (q); d2.setEnabled (true, true);
        std::vector<float> a ((size_t) n, 0.0f), b ((size_t) n, 0.0f);
        a[(size_t) ONISI] = 1.0f; b[(size_t) ONISI] = 1.0f;
        calistir (d2, a, b);
        double tp = 0.0;
        for (int i = ONISI + tst.second - 200; i < ONISI + tst.second + 200; ++i) tp = std::max (tp, (double) std::fabs (a[(size_t) i]));
        int f = -1;
        for (int i = ONISI + 1000; i < n; ++i) if (std::fabs (a[(size_t) i]) > 0.05 * tp) { f = i; break; }
        SWX_BEKLE (f >= ONISI + tst.second - 1 && f <= ONISI + tst.second + 1);
    }
}

SWX_TEST (delay_genislik_orta_mono_ve_enerji_telafisi)
{
    using namespace sweax;
    const double fs = 48000.0;
    const int n = 48000, T = 6000;
    auto kos = [&] (float width, std::vector<float>& l, std::vector<float>& r)
    {
        StereoDelay d; d.prepare (fs, 256, 2);
        DelayParams p; p.on = true; p.mix = 1.0f; p.sync = 4; p.bpm = 120.0; p.feedback = 0.0f;
        p.hpHz = 60.0f; p.lpHz = 16000.0f; p.duckDb = 0.0f; p.width = width;
        d.setParams (p); d.setEnabled (true, true);
        l.assign ((size_t) n, 0.0f); r.assign ((size_t) n, 0.0f);
        l[(size_t) ONISI] = 1.0f; r[(size_t) ONISI] = 1.0f;
        calistir (d, l, r);
    };
    std::vector<float> l1, r1, l0, r0;
    kos (1.0f, l1, r1);
    kos (0.0f, l0, r0);
    // width 0: yankılar ORTADA — L ve R örnek örnek EŞİT (giriş de eşit)
    bool esit = true;
    for (int i = 0; i < n; ++i) if (l0[(size_t) i] != r0[(size_t) i]) { esit = false; break; }
    SWX_BEKLE (esit);
    // width 1: ping-pong — ilk yankı yalnız L'de
    const int a = ONISI + T - 200, b = ONISI + T + 800;
    const double e1L = enerji (l1, l1, a, b) * 0.5, e1R = enerji (r1, r1, a, b) * 0.5;
    SWX_BEKLE (e1L > 20.0 * e1R);
    // enerji telafisi: iki kanal toplam enerjisi width 0 ↔ width 1 aynı (±1.5 dB)
    const double E1 = enerji (l1, r1, a, b), E0 = enerji (l0, r0, a, b);
    const double farkDb = 10.0 * std::log10 (E0 / std::max (1e-30, E1));
    SWX_BEKLE (std::fabs (farkDb) <= 1.5);                     // ÖLÇÜLEN: −0.000 dB (koşan enerji telafisi, bulgu 8b)
    // ara değer: 0.5 → L/R ayrık ama tam ping-pong değil
    std::vector<float> lh, rh; kos (0.5f, lh, rh);
    const double ehL = enerji (lh, lh, a, b) * 0.5, ehR = enerji (rh, rh, a, b) * 0.5;
    SWX_BEKLE (ehR > e1R && ehL < e1L);
}

SWX_TEST (delay_karakter_rt_nan_ve_blok_degismezligi)
{
    using namespace sweax;
    const double fs = 48000.0;
    StereoDelay d; d.prepare (fs, 256, 2);
    DelayParams p; p.on = true; p.mix = 0.5f; p.sync = 4; p.bpm = 120.0; p.feedback = 0.4f; p.width = 1.0f;
    d.setParams (p); d.setEnabled (true, true);
    std::vector<float> l ((size_t) 24000, 0.1f), r ((size_t) 24000, 0.1f);
    l[100] = std::numeric_limits<float>::quiet_NaN();
    r[200] = std::numeric_limits<float>::infinity();
    calistir (d, l, r);
    // delay de kuru yola dokunmaz → bozuk örnek geçer, ama hatlar/filtreler zehirlenmez
    SWX_BEKLE (hepsiSonlu (l, 400) && hepsiSonlu (r, 400));

    // temiz sinyal (NaN'lı örnek kuru yoldan geçmişti; RT turunda yalnız iç durum sınanır)
    for (int k = 0; k < 24000; ++k) l[(size_t) k] = r[(size_t) k] = 0.2f * (float) std::sin (2.0 * PI * 220.0 * k / fs);
    rt::resetViolations();
    {
        rt::Scope s;
        for (int k = 0; k < 400; ++k)
        {
            p.sync = k % 5; p.width = 0.2f * (float) (k % 6); p.bpm = 90.0 + (k % 60);
            d.setParams (p);
            float* ch[2] = { l.data(), r.data() };
            d.process (ch, 2, 256);
        }
    }
    SWX_BEKLE_EQ (rt::violations(), 0u);
    SWX_BEKLE (hepsiSonlu (l) && hepsiSonlu (r));

    for (float w : { 0.0f, 0.5f, 1.0f })
    {
        DelayParams q; q.on = true; q.mix = 0.5f; q.sync = 4; q.bpm = 120.0; q.feedback = 0.35f; q.width = w;
        SWX_BEKLE (blokFarki<StereoDelay> (q, fs) <= 1.0e-5);
    }
}

// =====================================================================================================
// 4) SATURATOR KARAKTER
// =====================================================================================================
SWX_TEST (sat_karakter_birim_kazanc_ve_monotonluk)
{
    using namespace sweax;
    const double fs = 48000.0;
    // −40 dBFS 1 kHz → her karakterde kazanç 0 ±0.3 dB (küçük sinyal birim)
    for (int kar = 0; kar < Saturator::KARAKTER_SAYISI; ++kar)
        for (float drv : { 0.0f, 3.0f, 6.0f, 12.0f })
        {
            Saturator sat; sat.prepare (fs, 256, 2);
            SatParams p; p.on = true; p.driveDb = drv; p.toneDb = 0.0f; p.mix = 1.0f; p.character = kar;
            sat.setParams (p); sat.setEnabled (true, true);
            const int n = 48000;
            std::vector<float> l ((size_t) n), r ((size_t) n);
            for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = 0.01f * (float) std::sin (2.0 * PI * 1000.0 * i / fs);
            auto giris = l;
            calistir (sat, l, r);
            const double gi = tonMag (giris, fs, 1000.0, 4800, n), go = tonMag (l, fs, 1000.0, 4800, n);
            const double db = 20.0 * std::log10 (go / std::max (1e-30, gi));
            if (std::fabs (db) > 0.3) std::printf ("   kar %d drive %.0f kazanc %.3f dB\n", kar, (double) drv, db);
            SWX_BEKLE (std::fabs (db) <= 0.3);
        }

    // transfer eğrisi −2…+2 aralığında MONOTON (her karakter, sert mod dahil)
    for (int kar = 0; kar < Saturator::KARAKTER_SAYISI; ++kar)
        for (float drv : { 0.0f, 3.0f, 6.0f, 8.0f, 12.0f })
        {
            double enKotu = 0.0;
            float onceki = Saturator::transfer (kar, drv, -2.0f);
            for (int k = 1; k <= 4000; ++k)
            {
                const float x = -2.0f + 4.0f * (float) k / 4000.0f;
                const float y = Saturator::transfer (kar, drv, x);
                enKotu = std::min (enKotu, (double) (y - onceki));
                onceki = y;
            }
            SWX_BEKLE (enKotu >= -1.0e-7);
            // küçük sinyal kazancı 1 (statik eğri)
            const float dy = Saturator::transfer (kar, drv, 1.0e-4f) - Saturator::transfer (kar, drv, -1.0e-4f);
            SWX_BEKLE_YAKIN ((double) dy / 2.0e-4, 1.0, 0.02);
        }
}

SWX_TEST (sat_karakter_harmonik_imzalari)
{
    using namespace sweax;
    const double fs = 48000.0;
    const int n = 48000, a = 4800, b = a + 24000;               // 200 Hz'de tam 100 periyot
    auto harmonikler = [&] (int kar, double* h)
    {
        Saturator sat; sat.prepare (fs, 256, 2);
        SatParams p; p.on = true; p.driveDb = 6.0f; p.toneDb = 0.0f; p.mix = 1.0f; p.character = kar;
        sat.setParams (p); sat.setEnabled (true, true);
        std::vector<float> l ((size_t) n), r ((size_t) n);
        for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = 0.2511886f * (float) std::sin (2.0 * PI * 200.0 * i / fs);
        calistir (sat, l, r);
        for (int k = 1; k <= 8; ++k) h[k] = tonMag (l, fs, 200.0 * k, a, b);
    };
    auto thd = [] (const double* h)
    {
        double s = 0.0;
        for (int k = 2; k <= 8; ++k) s += h[k] * h[k];
        return std::sqrt (s) / std::max (1e-30, h[1]);
    };
    double tup[9] {}, bant[9] {}, kirik[9] {};
    harmonikler (0, tup); harmonikler (1, bant); harmonikler (2, kirik);
    auto dbc = [] (const double* h, int k) { return 20.0 * std::log10 (h[k] / std::max (1e-30, h[1])); };
    // ÖLÇÜLEN (48k, drive 6 dB, −12 dBFS 200 Hz):
    //   TÜP   H2 −27.0 dBc  H3 −35.1 dBc  THD %4.83   (çift baskın — tüp imzası, M2 ile aynı)
    //   BANT  H2 −136.4 dBc H3 −34.1 dBc  THD %1.97   (çift harmonik YAPISAL olarak yok; simetrik tanh)
    //   KIRIK H2 −21.9 dBc  H3 −23.4 dBc  THD %10.53  (asimetrik sert diz — çift + tek belirgin, Tüp'ün 2.2×'i)
    // BANT: simetrik → tek harmonik baskın, çift harmonik yapısal olarak yok
    SWX_BEKLE (bant[3] > bant[2]);
    SWX_BEKLE (dbc (bant, 2) <= -45.0);
    // KIRIK: asimetrik ve daha sert → THD Tüp'ten büyük, H2 belirgin
    SWX_BEKLE (thd (kirik) > thd (tup));
    SWX_BEKLE (dbc (kirik, 2) >= -40.0);
    // TÜP: M2 imzası korunur (çift + tek harmonik, ölçülü)
    SWX_BEKLE (tup[2] > 0.0 && tup[3] > 0.0);
}

SWX_TEST (sat_karakter_gecisi_tiksiz_rt_nan_ve_blok)
{
    using namespace sweax;
    const double fs = 48000.0;
    Saturator sat; sat.prepare (fs, 256, 2);
    SatParams p; p.on = true; p.driveDb = 6.0f; p.toneDb = 0.0f; p.mix = 1.0f; p.character = 0;
    sat.setParams (p); sat.setEnabled (true, true);
    // DENETİM (bulgu 9): 9600'lük anahtar aralığı 220 Hz'de tam 44 periyot — faz eklenmezse her karakter
    // değişimi sıfır geçişine denk gelir (crossfade kalksa da test geçerdi). +π/2 → anahtar sinüs TEPESİNDE.
    const int n = 96000;
    std::vector<float> l ((size_t) n), r ((size_t) n);
    for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = 0.4f * (float) std::sin (2.0 * PI * 220.0 * i / fs + 0.5 * PI);
    const int blok = 256;
    int i = 0;
    for (; i < 24000; i += blok) { float* ch[2] = { l.data() + i, r.data() + i }; sat.process (ch, 2, blok); }
    const double sakin = enBuyukAdim (l, 12000, 24000);
    int kar = 0;
    for (; i + blok <= n; i += blok)
    {
        if ((i % 9600) == 0) { kar = (kar + 1) % Saturator::KARAKTER_SAYISI; p.character = kar; sat.setParams (p); }
        float* ch[2] = { l.data() + i, r.data() + i };
        sat.process (ch, 2, blok);
    }
    const double gecis = enBuyukAdim (l, 24000, i);
    SWX_BEKLE (hepsiSonlu (l) && hepsiSonlu (r));
    // ÖLÇÜLEN (8 faz süpürmesiyle kalibre, tepe fazı dahil): kararlı Tüp'te en büyük örnek adımı 0.01181;
    // karakter geçişleri boyunca ×1.00 — kırık haldeyken en kötü faz ×7.39 veriyordu, eşik 1.5 ikisini ayırır.
    SWX_BEKLE (gecis < 1.5 * sakin);

    // NaN / Inf sigortası (her karakterde)
    for (int k = 0; k < Saturator::KARAKTER_SAYISI; ++k)
    {
        Saturator s2; s2.prepare (fs, 256, 2);
        SatParams q; q.on = true; q.driveDb = 9.0f; q.mix = 1.0f; q.character = k;
        s2.setParams (q); s2.setEnabled (true, true);
        std::vector<float> a ((size_t) 24000, 0.2f), b ((size_t) 24000, 0.2f);
        a[100] = std::numeric_limits<float>::quiet_NaN();
        b[200] = std::numeric_limits<float>::infinity();
        calistir (s2, a, b);
        SWX_BEKLE (hepsiSonlu (a) && hepsiSonlu (b));
        SWX_BEKLE (blokFarki<Saturator> (q, fs) <= 1.0e-5);
    }

    rt::resetViolations();
    {
        rt::Scope s;
        for (int k = 0; k < 400; ++k)
        {
            p.character = k % Saturator::KARAKTER_SAYISI;
            p.driveDb = (float) (k % 13);
            sat.setParams (p);
            float* ch[2] = { l.data(), r.data() };
            sat.process (ch, 2, 256);
        }
    }
    SWX_BEKLE_EQ (rt::violations(), 0u);
    SWX_BEKLE (hepsiSonlu (l) && hepsiSonlu (r));
}

// =====================================================================================================
// 5) M3.5 FX KARAKTER DENETİM REGRESYONLARI
// İki bağımsız denetimin doğrulanan bulguları; her biri önce KIRIK haliyle yeniden üretildi (sayılar
// yorumlarda), düzeltme sonrası ölçümle eşiklendi. Anahtar anları sinüs TEPESİNE konur (bulgu 9).
// =====================================================================================================

// BULGU 1: geçiş SÜRERKEN ikinci karakter değişimi karışım ağırlığını sıfırlayıp tık üretiyordu.
// Ölçülen (48k, 220 Hz 0.40, drive 6, blok 64, en kötü faz): kararlı adım 0.01181; kırıkken 0→2 sonra
// +64 örnekte 2→1 = 0.08726 (×7.39); düzeltme (ağırlık vektörü, rampa o anki karışımdan) sonrası ×1.00.
SWX_TEST (fxkarakter_sat_gecis_ustune_gecis)
{
    using namespace sweax;
    const double fs = 48000.0;
    auto kos = [&] (int karA, int karB, int gap, double faz) -> double
    {
        Saturator sat; sat.prepare (fs, 4096, 2);
        SatParams p; p.on = true; p.driveDb = 6.0f; p.toneDb = 0.0f; p.mix = 1.0f; p.character = 0;
        sat.setParams (p); sat.setEnabled (true, true);
        const int n = 48000, blok = 64, t0 = 12032;
        std::vector<float> l ((size_t) n), r ((size_t) n);
        for (int i = 0; i < n; ++i)
            l[(size_t) i] = r[(size_t) i] = 0.40f * (float) std::sin (2.0 * PI * 220.0 * i / fs + faz);
        for (int i = 0; i + blok <= n; i += blok)
        {
            if (i == t0) { p.character = karA; sat.setParams (p); }
            if (gap >= 0 && i == t0 + gap) { p.character = karB; sat.setParams (p); }
            float* ch[2] = { l.data() + i, r.data() + i };
            sat.process (ch, 2, blok);
        }
        return enBuyukAdim (l, t0 - 2048, 30000);
    };
    for (double faz : { 0.0, 0.25 * PI, 0.5 * PI, 0.75 * PI })
    {
        const double taban = kos (0, 0, -1, faz);              // değişim yok
        SWX_BEKLE (taban > 0.0);
        for (int gap : { 64, 256, 768 })
            SWX_BEKLE (kos (2, 1, gap, faz) < 1.5 * taban);    // geçiş üstüne geçiş
        SWX_BEKLE (kos (2, 2, -1, faz) < 1.5 * taban);         // tek geçiş de tıksız
    }
}

// BULGU 2: resetWet 'ilkParam' bayrağı paramsDirty kapısına bağlıydı — arayan her blok setParams
// çağırmazsa reset'ten ÇOK SONRA, tank/durum yeniden dolmuşken gelen ilk tip/karakter değişimi rampasız
// uygulanıyordu. Ölçülen ıslak maks adım: reverb 0.00847 → 0.22742 (×26.8), saturator 0.01181 → 0.09593
// (×8.1). Düzeltme (bosDurum yalnız İLK örneğe kadar) sonrası: reverb 0.0127 (geçişin kendi sön/aç payı),
// saturator ×1.00.
SWX_TEST (fxkarakter_reset_sonrasi_rampa)
{
    using namespace sweax;
    const double fs = 48000.0;
    // --- PlateReverb: reset'ten 0.33 s sonra tip değişimi, ARADA setParams YOK ---
    auto revKos = [&] (bool resetVar) -> double
    {
        PlateReverb rv; rv.prepare (fs, 256, 2);
        ReverbParams p; p.on = true; p.mix = 1.0f; p.preDelayMs = 20.0f; p.decaySec = 2.0f;
        p.dampHz = 9000.0f; p.hpHz = 150.0f; p.width = 1.0f; p.duckDb = 0.0f; p.type = 0;
        rv.setParams (p); rv.setEnabled (true, true);
        const int n = 72000, blok = 256;
        std::vector<float> l ((size_t) n), r ((size_t) n);
        for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = 0.30f * (float) std::sin (2.0 * PI * 220.0 * i / fs + 0.5 * PI);
        std::vector<float> giris = l;
        for (int i = 0; i + blok <= n; i += blok)
        {
            if (resetVar && i == 8192) rv.reset();
            if (i == 24064) { p.type = 1; rv.setParams (p); }
            float* ch[2] = { l.data() + i, r.data() + i };
            rv.process (ch, 2, blok);
        }
        std::vector<float> islak ((size_t) n);
        for (int i = 0; i < n; ++i) islak[(size_t) i] = l[(size_t) i] - giris[(size_t) i];
        return enBuyukAdim (islak, 24064 - 512, 24064 + 8000);
    };
    const double revRef = revKos (false);
    SWX_BEKLE (revRef > 0.0);
    SWX_BEKLE (revKos (true) < 3.0 * revRef);                  // kırıkken ×26.8'di
    // --- Saturator: reset'ten sonra karakter değişimi, arada setParams yok (4 faz) ---
    auto satKos = [&] (bool resetVar, double faz) -> double
    {
        Saturator sat; sat.prepare (fs, 4096, 2);
        SatParams p; p.on = true; p.driveDb = 6.0f; p.toneDb = 0.0f; p.mix = 1.0f; p.character = 0;
        sat.setParams (p); sat.setEnabled (true, true);
        const int n = 48000, blok = 64;
        std::vector<float> l ((size_t) n), r ((size_t) n);
        for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = 0.40f * (float) std::sin (2.0 * PI * 220.0 * i / fs + faz);
        for (int i = 0; i + blok <= n; i += blok)
        {
            if (resetVar && i == 6400) sat.reset();
            if (i == 12800) { p.character = 2; sat.setParams (p); }
            float* ch[2] = { l.data() + i, r.data() + i };
            sat.process (ch, 2, blok);
        }
        return enBuyukAdim (l, 12800 - 512, 24000);
    };
    for (double faz : { 0.0, 0.5 * PI, PI, 1.5 * PI })
        SWX_BEKLE (satKos (true, faz) < 1.5 * satKos (false, faz));   // kırıkken ×8.1'di
}

// BULGU 3: iptal edilen tip değişimi (A→B→A, sönüş bitmeden geri) tankı yine de siliyordu — net değişim
// yokken kuyruk ~150 ms yok oluyordu (ölçülen: T+100 ms'de ıslak RMS −5.5 dB, sonra aşım). Düzeltme
// (durum 3 = İPTAL: tank korunur, çıkış aynı rampayla geri açılır) sonrası fark 0.0 dB.
SWX_TEST (fxkarakter_reverb_tip_iptal)
{
    using namespace sweax;
    const double fs = 48000.0;
    auto kos = [&] (bool iptalVar, std::vector<float>& islak, int& sonTip)
    {
        PlateReverb rv; rv.prepare (fs, 256, 2);
        ReverbParams p; p.on = true; p.mix = 1.0f; p.preDelayMs = 20.0f; p.decaySec = 2.0f;
        p.dampHz = 7000.0f; p.hpHz = 220.0f; p.width = 1.0f; p.duckDb = 0.0f; p.type = 0;
        rv.setParams (p); rv.setEnabled (true, true);
        const int n = 144000, blok = 256, T = 48128;
        std::vector<float> l ((size_t) n), r ((size_t) n);
        for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = 0.30f * (float) std::sin (2.0 * PI * 220.0 * i / fs + 0.5 * PI);
        std::vector<float> giris = l;
        for (int i = 0; i + blok <= n; i += blok)
        {
            if (iptalVar && i == T) { p.type = 1; rv.setParams (p); }          // Plate→Oda
            if (iptalVar && i == T + 768) { p.type = 0; rv.setParams (p); }    // 16 ms sonra GERİ (iptal)
            float* ch[2] = { l.data() + i, r.data() + i };
            rv.process (ch, 2, blok);
        }
        islak.assign ((size_t) n, 0.0f);
        for (int i = 0; i < n; ++i) islak[(size_t) i] = l[(size_t) i] - giris[(size_t) i];
        sonTip = rv.activeType();
    };
    auto rms = [] (const std::vector<float>& x, int a, int b)
    { double s = 0.0; for (int i = a; i < b; ++i) s += (double) x[(size_t) i] * x[(size_t) i]; return std::sqrt (s / std::max (1, b - a)); };
    const int T = 48128;
    std::vector<float> ref, ipt; int st = -1;
    kos (false, ref, st);
    kos (true, ipt, st);
    SWX_BEKLE_EQ (st, 0);                                      // iptal sonrası aktif tip yine Plate
    SWX_BEKLE (enBuyukAdim (ipt, T - 512, T + 24000) < 1.5 * enBuyukAdim (ref, T, T + 24000));
    for (int ms : { 100, 300 })
    {
        const int a = T + (int) (fs * ms / 1000.0), b = a + 4800;
        const double fark = 20.0 * std::log10 (std::max (1e-12, rms (ipt, a, b)) / std::max (1e-12, rms (ref, a, b)));
        SWX_BEKLE (std::fabs (fark) <= 0.5);                   // kırıkken −5.5 dB çukurdu
    }
}

// BULGU 4: T60 eşlemesi (a) tank allpass çınlaması yüzünden kısa decay'lerde tabanlıydı (Plate 0.2–0.6 s
// atıldı: hepsi 0.70 s; Salon 0.61 s; Chamber 0.3 hedefte ×1.26), (b) sönüm filtresinin döngü kaybını
// yok sayıyordu (damp 3 kHz'te ×0.73'e dek kısalma). Düzeltme: kısa decay difüzyon freni + tip başına
// kisaDuzelt + (tip 1–3) sönüm kaybı telafisi. Ölçülen tablo PlateReverb.h başlığında.
SWX_TEST (fxkarakter_reverb_t60_tablosu)
{
    using namespace sweax;
    const double fs = 48000.0;
    auto t60Olc = [&] (int tip, float decaySec, float dampHz) -> double
    {
        const int onisi = 4800;
        const int n = onisi + (int) std::lround (std::min (10.0, 2.5 * (double) decaySec + 1.0) * fs);
        std::vector<float> l ((size_t) n, 0.0f), r ((size_t) n, 0.0f);
        l[(size_t) onisi] = 1.0f; r[(size_t) onisi] = 1.0f;
        PlateReverb rv; rv.prepare (fs, 256, 2);
        ReverbParams p; p.on = true; p.mix = 1.0f; p.preDelayMs = 0.0f; p.decaySec = decaySec;
        p.dampHz = dampHz; p.hpHz = 100.0f; p.width = 1.0f; p.duckDb = 0.0f; p.type = tip;
        rv.setParams (p); rv.setEnabled (true, true);
        calistir (rv, l, r);
        return t60Schroeder (l, r, onisi + 1, fs);
    };
    // sönümsüz referans: 4 tip × {0.3, 0.8, 1.5, 3.0} s → hedefin ±%25'i
    for (int tip = 0; tip < PlateReverb::TIP_SAYISI; ++tip)
        for (float hedef : { 0.3f, 0.8f, 1.5f, 3.0f })
        {
            const double t = t60Olc (tip, hedef, 12000.0f);
            if (! (t > 0.75 * (double) hedef && t < 1.25 * (double) hedef))
                std::printf ("   T60 tip %d hedef %.2f -> %.3f (x%.2f)\n", tip, (double) hedef, t, t / (double) hedef);
            SWX_BEKLE (t > 0.75 * (double) hedef && t < 1.25 * (double) hedef);
        }
    // güçlü sönüm (damp 3 kHz): yeni tiplerde telafi devrede → yine ±%25. (Plate M2 eşlemesiyle bit-birebir
    // kalmak zorunda: aynı koşulda ×0.76'ya kısalır — bilinen M2 sınırı, başlıkta belgeli, test DIŞI.)
    for (int tip = 1; tip < PlateReverb::TIP_SAYISI; ++tip)
        for (float hedef : { 0.8f, 1.5f, 3.0f })
        {
            const double t = t60Olc (tip, hedef, 3000.0f);
            if (! (t > 0.75 * (double) hedef && t < 1.25 * (double) hedef))
                std::printf ("   T60 damp3k tip %d hedef %.2f -> %.3f (x%.2f)\n", tip, (double) hedef, t, t / (double) hedef);
            SWX_BEKLE (t > 0.75 * (double) hedef && t < 1.25 * (double) hedef);
        }
}

// BULGU 7 + 10: Salon dört tipin en seyrek kuyruğuydu (Abel–Huang yoğunluk @300 ms 0.48, Plate 0.79) →
// yeniden denge sonrası 300–400 ms ortalaması 0.88 ≥ Plate 0.79. Tepe (slap) seviyesi tipler arası
// eşitlendi (ER veri yolu tankKazanc'ın içine alındı): ölçülen Oda −0.2 / Salon −0.0 / Chamber +0.6 dB.
SWX_TEST (fxkarakter_reverb_yogunluk_ve_slap)
{
    using namespace sweax;
    const double fs = 48000.0;
    auto yogunluk = [] (const std::vector<float>& x, int merkez, int yarim) -> double
    {
        double s2 = 0.0; int n = 0;
        for (int i = merkez - yarim; i < merkez + yarim; ++i) { s2 += (double) x[(size_t) i] * x[(size_t) i]; ++n; }
        const double sd = std::sqrt (s2 / std::max (1, n));
        int say = 0;
        for (int i = merkez - yarim; i < merkez + yarim; ++i) if (std::fabs (x[(size_t) i]) > sd) ++say;
        return ((double) say / (double) n) / 0.3173;           // 1 ≈ Gauss (tam yoğun) — Abel–Huang
    };
    double yog[PlateReverb::TIP_SAYISI] {}, tepe[PlateReverb::TIP_SAYISI] {};
    for (int tip = 0; tip < PlateReverb::TIP_SAYISI; ++tip)
    {
        std::vector<float> l, r;
        reverbIR (fs, tip, 1.5f, l, r);
        yog[tip] = 0.5 * (yogunluk (l, ONISI + 14400, 480) + yogunluk (l, ONISI + 19200, 480));
        for (int i = ONISI + 1; i < ONISI + 4800; ++i)
            tepe[tip] = std::max (tepe[tip], (double) (std::fabs (l[(size_t) i]) + std::fabs (r[(size_t) i])));
    }
    SWX_BEKLE (yog[2] >= 0.95 * yog[0]);                       // Salon kuyruğu en az Plate kadar yoğun
    for (int tip = 1; tip < PlateReverb::TIP_SAYISI; ++tip)
    {
        const double fark = 20.0 * std::log10 (tepe[tip] / std::max (1e-30, tepe[0]));
        if (std::fabs (fark) > 1.0) std::printf ("   slap tip %d fark %+.2f dB\n", tip, fark);
        SWX_BEKLE (std::fabs (fark) <= 1.0);                   // kırıkken Oda +7.3 dB'ydi
    }
}

// BULGU 5: Kırık'ın eski dördüncü kök biçimli sert dizi 2× OS'nin örtemediği kadar yavaş sönen harmonik
// serisi üretiyordu (44.1k/3 kHz drive 12'de −45.4 dBc). Yumuşatılmış diz + seviye eşitleme sonrası proje
// ölçütü kilitlenir: 1 kHz sinüs, drive {3,6,9,12}, HER karakter → tüm alias bileşenleri temelin −60 dB
// altında. 44.1k seçildi: 44100 1000'e bölünmez → katlanan bileşenler harmoniklerin ÜZERİNE DÜŞMEZ,
// 100 Hz ızgarasında ayrı görünür (48k'da alias'lar harmoniklerle çakışıp maskelenir). Pencere 0.9 s →
// 100 Hz katları tam periyotlu, sızıntı yok. Ölçülen en kötüler: TÜP −122.5 / BANT −142.5 / KIRIK −90.0 dBc.
SWX_TEST (fxkarakter_alias_butcesi)
{
    using namespace sweax;
    const double fs = 44100.0, f0 = 1000.0;
    for (int kar = 0; kar < Saturator::KARAKTER_SAYISI; ++kar)
        for (float drv : { 3.0f, 6.0f, 9.0f, 12.0f })
        {
            Saturator sat; sat.prepare (fs, 4096, 1);
            SatParams p; p.on = true; p.driveDb = drv; p.toneDb = 0.0f; p.mix = 1.0f; p.character = kar;
            sat.setParams (p); sat.setEnabled (true, true);
            const int n = (int) fs;
            std::vector<float> x ((size_t) n);
            for (int i = 0; i < n; ++i) x[(size_t) i] = 0.5f * (float) std::sin (2.0 * PI * f0 * i / fs);
            for (int i = 0; i < n; i += 256) { float* ch[1] = { x.data() + i }; sat.process (ch, 1, std::min (256, n - i)); }
            const int a = (int) (fs / 10.0), b = n;
            const double h1 = tonMag (x, fs, f0, a, b);
            double enKotu = -300.0; double fKotu = 0.0;
            for (double f = 100.0; f < 0.495 * fs; f += 100.0)
            {
                if (std::fabs (f - f0 * std::lround (f / f0)) < 1.0) continue;   // harmonikler ölçüm dışı
                const double m = 20.0 * std::log10 (std::max (1e-30, tonMag (x, fs, f, a, b) / std::max (1e-30, h1)));
                if (m > enKotu) { enKotu = m; fKotu = f; }
            }
            if (enKotu > -60.0) std::printf ("   alias kar %d drive %.0f : %.1f dBc @ %.0f Hz\n", kar, (double) drv, enKotu, fKotu);
            SWX_BEKLE (enKotu <= -60.0);
        }
}

// BULGU 6: karakterler arası çıkış seviyesi eşit değildi (−18 dBFS 1 kHz'te Kırık drive 12'de −1.9 dB) —
// karakter A/B'si seviye farkıyla yanıltıyordu. k·f(x/k) telafisi sonrası ölçülen: Bant −0.0…+0.2 dB,
// Kırık −0.2…−0.4 dB. Küçük-sinyal birim kazanç ve monotonluk testleri (yukarıda) aynen geçmeye devam eder.
SWX_TEST (fxkarakter_sat_seviye_esit)
{
    using namespace sweax;
    const double fs = 48000.0;
    const float amp = 0.12589254f;                             // −18 dBFS
    auto rmsOlc = [&] (int kar, float drive) -> double
    {
        Saturator sat; sat.prepare (fs, 256, 1);
        SatParams p; p.on = true; p.driveDb = drive; p.toneDb = 0.0f; p.mix = 1.0f; p.character = kar;
        sat.setParams (p); sat.setEnabled (true, true);
        const int n = 24000;
        std::vector<float> l ((size_t) n);
        for (int i = 0; i < n; ++i) l[(size_t) i] = amp * (float) std::sin (2.0 * PI * 1000.0 * i / fs);
        for (int i = 0; i < n; i += 256) { float* ch[1] = { l.data() + i }; sat.process (ch, 1, std::min (256, n - i)); }
        double s = 0.0;
        for (int i = 4800; i < n; ++i) s += (double) l[(size_t) i] * l[(size_t) i];
        return std::sqrt (s / (double) (n - 4800));
    };
    for (float drv : { 0.0f, 3.0f, 6.0f, 9.0f, 12.0f })
    {
        const double tup = rmsOlc (0, drv);
        for (int kar = 1; kar < Saturator::KARAKTER_SAYISI; ++kar)
        {
            const double fark = 20.0 * std::log10 (rmsOlc (kar, drv) / std::max (1e-30, tup));
            if (std::fabs (fark) > 0.5) std::printf ("   seviye kar %d drive %.0f : %+.2f dB\n", kar, (double) drv, fark);
            SWX_BEKLE (std::fabs (fark) <= 0.5);
        }
    }
}

// BULGU 8: (a) MONO çıkışta width etkisizdir — bilinen/kabul edilen sınır (başlıkta belgeli): görüntü yok,
// eko seviyesi width'ten bağımsız aynı. (b) geri beslemeli sürdürülen tonda ortaya toplama L/R korelasyonu
// yüzünden +3 dB'ye dek seviye yayılımı yapıyordu (800 Hz fb 0.85'te +2.95 dB) ve ara width'lerde ilk yankı
// −0.69 dB çukurluydu → koşan enerji telafisi (yalnız çıkış tap'inde, döngü DIŞI) ikisini de kapatır.
SWX_TEST (fxkarakter_delay_mono_ve_fb_enerjisi)
{
    using namespace sweax;
    const double fs = 48000.0;
    // (a) mono: width 0 ve 1 çıkışı BİT BİT aynı (aynı kod yolu; seviye korunur)
    {
        auto monoKos = [&] (float w, std::vector<float>& x)
        {
            StereoDelay d; d.prepare (fs, 256, 1);
            DelayParams p; p.on = true; p.mix = 1.0f; p.sync = 4; p.bpm = 120.0; p.feedback = 0.5f;
            p.hpHz = 60.0f; p.lpHz = 16000.0f; p.duckDb = 0.0f; p.width = w;
            d.setParams (p); d.setEnabled (true, true);
            x.assign ((size_t) 48000, 0.0f); x[4800] = 1.0f;
            for (int i = 0; i < 48000; i += 256) { float* ch[1] = { x.data() + i }; d.process (ch, 1, std::min (256, 48000 - i)); }
        };
        std::vector<float> m0, m1;
        monoKos (0.0f, m0); monoKos (1.0f, m1);
        bool ayni = true;
        for (size_t i = 0; i < m0.size(); ++i) if (m0[i] != m1[i]) { ayni = false; break; }
        SWX_BEKLE (ayni);
    }
    // (b) sürdürülen ton + geri besleme: toplam ıslak enerji width 0 ↔ 1 aynı (±1 dB; kırıkken +2.7…+3.0 dB)
    auto tonKos = [&] (float w, float fb, double f0) -> double
    {
        StereoDelay d; d.prepare (fs, 256, 2);
        DelayParams p; p.on = true; p.mix = 1.0f; p.sync = 4; p.bpm = 120.0; p.feedback = fb;
        p.hpHz = 250.0f; p.lpHz = 6500.0f; p.duckDb = 0.0f; p.width = w;
        d.setParams (p); d.setEnabled (true, true);
        const int n = 144000;
        std::vector<float> l ((size_t) n), r ((size_t) n);
        for (int i = 0; i < n; ++i) l[(size_t) i] = r[(size_t) i] = 0.3f * (float) std::sin (2.0 * PI * f0 * i / fs);
        std::vector<float> giris = l;
        for (int i = 0; i < n; i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; d.process (ch, 2, std::min (256, n - i)); }
        for (int i = 0; i < n; ++i) { l[(size_t) i] -= giris[(size_t) i]; r[(size_t) i] -= giris[(size_t) i]; }
        return enerji (l, r, 96000, n);
    };
    for (auto sf : { std::pair<double, float> { 800.0, 0.60f }, std::pair<double, float> { 777.0, 0.85f } })
    {
        const double fark = 10.0 * std::log10 (tonKos (0.0f, sf.second, sf.first)
                                               / std::max (1e-30, tonKos (1.0f, sf.second, sf.first)));
        SWX_BEKLE (std::fabs (fark) <= 1.0);
    }
    // ara width'lerde ilk yankı enerjisi çukursuz (fb 0, dürtü; kırıkken w 0.5'te −0.69 dB)
    auto ilkYanki = [&] (float w) -> double
    {
        StereoDelay d; d.prepare (fs, 256, 2);
        DelayParams p; p.on = true; p.mix = 1.0f; p.sync = 4; p.bpm = 120.0; p.feedback = 0.0f;
        p.hpHz = 60.0f; p.lpHz = 16000.0f; p.duckDb = 0.0f; p.width = w;
        d.setParams (p); d.setEnabled (true, true);
        std::vector<float> l ((size_t) 48000, 0.0f), r ((size_t) 48000, 0.0f);
        l[4800] = 1.0f; r[4800] = 1.0f;
        calistir (d, l, r);
        return enerji (l, r, 4800 + 6000 - 200, 4800 + 6000 + 800);
    };
    const double E1 = ilkYanki (1.0f);
    for (float w : { 0.25f, 0.5f, 0.75f })
        SWX_BEKLE (std::fabs (10.0 * std::log10 (ilkYanki (w) / std::max (1e-30, E1))) <= 0.5);
}
