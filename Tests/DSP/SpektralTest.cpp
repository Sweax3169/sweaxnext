// SWEAX NEXT — Tests/DSP/SpektralTest.cpp
// Ölçülen (M3.5 spektral akışı):
//  MOTOR: N seçimi (sr ve mod), kendi FFT'mizin juce::dsp::FFT ile eşdeğerliği + hız/kilit karşılaştırması,
//         değiştirilmemiş binlerde birim yeniden kurulum (FFT yolu ve birim yol; beyaz gürültü + sinüs, mono +
//         stereo, ≤ −80 dB), gecikme dürtüyle TAM (N − 1), sabit kazanç çerçevesi doğru ölçekli.
//  YUMUŞAT: (a) depth 0 birebir; (b) gecikme dürtüyle tam (her mod, 44.1/48/96k, açık+kapalı); (c) pembe gürültü
//         üstünde +20 dB 3.5 kHz rezonans ≥ 6 dB azalır, 1–2 kHz gürültü < 1 dB değişir; (d) beyaz gürültü toplam
//         azaltma < 1 dB; (e) odak dışı 500 Hz < 1 dB; (f) özdeş L/R → özdeş çıkış (mono = stereo L);
//         (g) blok boyutu değişmezliği; (h) RtGuard process + setQuality 0 ihlal; (i) NaN/Inf sigortası;
//         (j) −90 dBFS'te kazanç 1; (k) açma/kapama crossfade'i hizalı (idleWet).
//  FORMANT: (a) shift 0 birebir; (b) sentetik ünlü F1/F2 ±3 st'te ×1.19 / ×0.84; (c) perde değişmez (F0 ±%1);
//         (d) RMS değişimi ≤ 1.5 dB; (e) gecikme tam (dürtü + gürültü çapraz ilinti); (f) blok boyutu; (g) RtGuard;
//         (h) NaN; (i) mix 0.5 comb yok — mix DOĞRUSAL KARIŞIM özdeşliği ±0.5/−60 dB, α ≠ 1 olan shift'lerde
//         (denetim: eski hâli shift 0 kullanıyordu, orada modül birim yola geçtiği için mix yolu hiç koşmuyordu);
//         (j) perdeye uyarlı lifter: F0 120–400 Hz'de kesim 0.35·T0 ve harmonik kazanç eğrisi düzgün (≤ 1 dB).
//  DENETİM REGRESYONLARI (şüpheci denetim turunda ÖLÇÜLEN kusurlar; her testin başında önce/sonra sayıları var):
//         (1) F0 85–120 Hz'de formant kaydırma GERÇEKTEN oluyor (ortak, çerçeveden bağımsız perde algılayıcı);
//         (2) setQuality ses akarken tık/splatter üretmiyor, Yüksek↔Ultra tam sürekli;
//         (3) YUMUŞAT eşleştirilmiş kaynakta (dürtü dizisi vs gürültü, AYNI zarf) harmonik içeriği fazla
//             bastırmıyor; (4) Kalite modu TON değiştirmiyor; (5) FORMANT kazanç eğrisi zamanda kararlı;
//         (8) modül kopyası sarkan işaretçi taşımıyor; (9) prepare(sr ≤ 0 / NaN) kelepçesi modül+motor tutarlı;
//         (ek) CANLI modda kaydırmanın zayıflığı SAYIYLA sabitlendi (kusur değil, pencere boyutu ödünü).
//  PERFORMANS: 48k stereo 256 blok, iki modülün gerçek zaman yüzdesi yazdırılır.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "DSP/SpektralMotor.h"
#include "DSP/YumusatModule.h"
#include "DSP/FormantModule.h"
#include "Chain/VocalChain.h"
#include "Core/RtGuard.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <chrono>
#include <complex>
#include <limits>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    using namespace sweax;

    bool ayrintili() { return std::getenv ("SWX_VERBOSE") != nullptr; }

    template <typename M>
    void calistir2 (M& m, std::vector<float>& l, std::vector<float>& r, int blok = 256)
    {
        const int n = (int) l.size();
        for (int i = 0; i < n; i += blok) { float* ch[2] = { l.data() + i, r.data() + i }; m.process (ch, 2, std::min (blok, n - i)); }
    }
    template <typename M>
    void calistir1 (M& m, std::vector<float>& x, int blok = 256)
    {
        const int n = (int) x.size();
        for (int i = 0; i < n; i += blok) { float* ch[1] = { x.data() + i }; m.process (ch, 1, std::min (blok, n - i)); }
    }
    std::vector<float> beyaz (int n, unsigned seed, double tepe = 0.25)
    {
        std::mt19937 g (seed);
        std::uniform_real_distribution<double> d (-1.0, 1.0);
        std::vector<float> v ((size_t) n);
        for (auto& s : v) s = (float) (tepe * d (g));
        return v;
    }
    // Paul Kellet ekonomik pembe gürültü (−3 dB/oktav yaklaşığı)
    std::vector<float> pembe (int n, unsigned seed, double olcek = 0.08)
    {
        std::mt19937 g (seed);
        std::uniform_real_distribution<double> d (-1.0, 1.0);
        std::vector<float> v ((size_t) n);
        double b0 = 0, b1 = 0, b2 = 0;
        for (int i = 0; i < n; ++i)
        {
            const double w = d (g);
            b0 = 0.99765 * b0 + w * 0.0990460;
            b1 = 0.96300 * b1 + w * 0.2965164;
            b2 = 0.57000 * b2 + w * 1.0526913;
            v[(size_t) i] = (float) (olcek * (b0 + b1 + b2 + w * 0.1848));
        }
        return v;
    }
    std::vector<float> sinus (double fs, int n, double hz, double genlik)
    {
        std::vector<float> v ((size_t) n);
        for (int i = 0; i < n; ++i) v[(size_t) i] = (float) (genlik * std::sin (2.0 * PI * hz * i / fs));
        return v;
    }
    // gecikme hizalı fark enerjisi / giriş enerjisi (dB), çıkış [a, b)
    double farkDb (const std::vector<float>& y, const std::vector<float>& x, int gecikme, int a, int b)
    {
        double e = 0.0, s = 0.0;
        for (int i = a; i < b; ++i)
        {
            const double xi = (double) x[(size_t) (i - gecikme)];
            const double d = (double) y[(size_t) i] - xi;
            e += d * d; s += xi * xi;
        }
        return 10.0 * std::log10 ((e + 1e-30) / (s + 1e-30));
    }
    double rmsDb (const std::vector<float>& x, int a, int b)
    {
        double s = 0.0; int c = 0;
        for (int i = a; i < b && i < (int) x.size(); ++i) { s += (double) x[(size_t) i] * x[(size_t) i]; ++c; }
        return 10.0 * std::log10 (s / std::max (1, c) + 1e-30);
    }
    double tonDb (const std::vector<float>& x, double fs, double hz, int a, int b)
    {
        const double om = 2.0 * PI * hz / fs, cw = 2.0 * std::cos (om);
        double s1 = 0.0, s2 = 0.0;
        for (int i = a; i < b; ++i) { const double s0 = (double) x[(size_t) i] + cw * s1 - s2; s2 = s1; s1 = s0; }
        const double n = (double) (b - a);
        return 10.0 * std::log10 (std::max (s1 * s1 + s2 * s2 - cw * s1 * s2, 1e-30) / (n * n));
    }
    // Hann N=4096 / hop 2048 bant enerjisi (dB) — [f1, f2)
    double bantDb (const std::vector<float>& x, int a, int b, double fs, double f1, double f2)
    {
        constexpr int N = 4096;
        HizliFFT f; f.hazirla (N);
        std::vector<float> re (N), im (N), w (N);
        for (int i = 0; i < N; ++i) w[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * PI * i / N));
        double top = 0.0;
        for (int s = a; s + N <= b; s += N / 2)
        {
            for (int i = 0; i < N; ++i) { re[(size_t) i] = x[(size_t) (s + i)] * w[(size_t) i]; im[(size_t) i] = 0.0f; }
            f.ileri (re.data(), im.data(), N);
            for (int k = 1; k < N / 2; ++k)
            {
                const double hz = k * fs / N;
                if (hz >= f1 && hz < f2) top += (double) re[(size_t) k] * re[(size_t) k] + (double) im[(size_t) k] * im[(size_t) k];
            }
        }
        return 10.0 * std::log10 (top + 1e-30);
    }
    // motorun normalizasyonuyla (sqrt-Hann, (2/Σw)²) bin başına ortalama güç [f1, f2]
    double binGucu (const std::vector<float>& x, double fs, int N, double f1, double f2)
    {
        HizliFFT f; f.hazirla (N);
        std::vector<float> re ((size_t) N), im ((size_t) N), w ((size_t) N);
        double sw = 0.0;
        for (int i = 0; i < N; ++i) { w[(size_t) i] = (float) std::sqrt (0.5 - 0.5 * std::cos (2.0 * PI * i / N)); sw += w[(size_t) i]; }
        const double norm = (2.0 / sw) * (2.0 / sw);
        double top = 0.0; int say = 0;
        for (int s = 0; s + N <= (int) x.size(); s += N / 4)
        {
            for (int i = 0; i < N; ++i) { re[(size_t) i] = x[(size_t) (s + i)] * w[(size_t) i]; im[(size_t) i] = 0.0f; }
            f.ileri (re.data(), im.data(), N);
            for (int k = 1; k < N / 2; ++k)
            {
                const double hz = k * fs / N;
                if (hz >= f1 && hz <= f2) { top += norm * ((double) re[(size_t) k] * re[(size_t) k] + (double) im[(size_t) k] * im[(size_t) k]); ++say; }
            }
        }
        return top / std::max (1, say);
    }
    int tepeKonumu (const std::vector<float>& y)
    {
        int at = -1; float pk = 0.0f;
        for (int i = 0; i < (int) y.size(); ++i) if (std::fabs (y[(size_t) i]) > pk) { pk = std::fabs (y[(size_t) i]); at = i; }
        return at;
    }

    // test işleyicileri: sabit kazanç, FFT yolunu ZORLA (birim yol değil)
    struct SabitKazanc : SpektralIsleyici
    {
        float g { 1.0f };
        bool cerceveIsle (const SpektralCerceve& c, float* kazanc) noexcept override
        {
            for (int b = 0; b < c.bins; ++b) kazanc[b] = g;
            return true;
        }
    };

    const Quality MODLAR[3] = { Quality::Canli, Quality::Yuksek, Quality::Ultra };

    YumusatModule yumusatKur (double fs, float depth, int ch = 2, Quality q = Quality::Yuksek)
    {
        YumusatModule m;
        m.prepare (fs, 256, ch);
        m.setQuality (q);
        YumusatParams p; p.on = true; p.depth = depth;
        m.setParams (p);
        m.setEnabled (true, true);
        return m;
    }
    FormantModule formantKur (double fs, float shift, float mix = 1.0f, int ch = 2, Quality q = Quality::Yuksek)
    {
        FormantModule m;
        m.prepare (fs, 256, ch);
        m.setQuality (q);
        FormantParams p; p.on = true; p.shiftSt = shift; p.mix = mix;
        m.setParams (p);
        m.setEnabled (true, true);
        return m;
    }
}

// =================================================================================================== MOTOR ====
SWX_TEST (spektral_boyut_secimi)
{
    SWX_BEKLE_EQ (SpektralMotor::boyutSec (22050.0), 512);
    SWX_BEKLE_EQ (SpektralMotor::boyutSec (44100.0), 1024);
    SWX_BEKLE_EQ (SpektralMotor::boyutSec (48000.0), 1024);
    SWX_BEKLE_EQ (SpektralMotor::boyutSec (88200.0), 2048);
    SWX_BEKLE_EQ (SpektralMotor::boyutSec (96000.0), 2048);
    SWX_BEKLE_EQ (SpektralMotor::boyutSec (176400.0), 4096);
    SWX_BEKLE_EQ (SpektralMotor::boyutSec (192000.0), 4096);
    SWX_BEKLE_EQ (SpektralMotor::boyutSec (48000.0, Quality::Canli), 512);
    SWX_BEKLE_EQ (SpektralMotor::boyutSec (48000.0, Quality::Ultra), 1024);
    SWX_BEKLE_EQ (SpektralMotor::gecikmeSec (48000.0, Quality::Canli), 511);
    SWX_BEKLE_EQ (SpektralMotor::gecikmeSec (96000.0, Quality::Yuksek), 2047);
    YumusatModule y; y.prepare (44100.0, 256, 2);
    SWX_BEKLE_EQ (y.latencySamples (Quality::Canli), 511);
    SWX_BEKLE_EQ (y.latencySamples (Quality::Yuksek), 1023);
    FormantModule f; f.prepare (192000.0, 256, 2);
    SWX_BEKLE_EQ (f.latencySamples (Quality::Ultra), 4095);
    SWX_BEKLE_EQ (f.latencySamples (Quality::Canli), 2047);
}

SWX_TEST (spektral_fft_juce_esdegerlik)
{
    std::mt19937 g (7);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    for (int order = 1; order <= 13; ++order)
    {
        const int N = 1 << order;
        HizliFFT biz; biz.hazirla (8192);
        juce::dsp::FFT juceFft (order);
        std::vector<juce::dsp::Complex<float>> giris ((size_t) N), cikis ((size_t) N), geriJ ((size_t) N);
        std::vector<float> re ((size_t) N), im ((size_t) N);
        for (int i = 0; i < N; ++i) { giris[(size_t) i] = { d (g), d (g) }; re[(size_t) i] = giris[(size_t) i].real(); im[(size_t) i] = giris[(size_t) i].imag(); }
        juceFft.perform (giris.data(), cikis.data(), false);
        biz.ileri (re.data(), im.data(), N);
        double hata = 0.0, olcek = 0.0;
        for (int i = 0; i < N; ++i)
        {
            hata = std::max (hata, (double) std::abs (cikis[(size_t) i] - juce::dsp::Complex<float> (re[(size_t) i], im[(size_t) i])));
            olcek = std::max (olcek, (double) std::abs (cikis[(size_t) i]));
        }
        SWX_BEKLE (hata <= 2e-5 * olcek);
        juceFft.perform (cikis.data(), geriJ.data(), true);
        biz.geri (re.data(), im.data(), N);
        double hata2 = 0.0;
        for (int i = 0; i < N; ++i)
            hata2 = std::max (hata2, (double) std::abs (giris[(size_t) i] - juce::dsp::Complex<float> (re[(size_t) i], im[(size_t) i])));
        SWX_BEKLE (hata2 <= 1e-4);
    }
    // hız: N = 1024 ileri+geri
    {
        constexpr int N = 1024, TEKRAR = 20000;
        HizliFFT biz; biz.hazirla (N);
        juce::dsp::FFT juceFft (10);
        std::vector<float> re (N), im (N), gercek (2 * N);
        for (int i = 0; i < N; ++i) { re[(size_t) i] = d (g); im[(size_t) i] = 0.0f; }
        const auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < TEKRAR; ++k) { biz.ileri (re.data(), im.data(), N); biz.geri (re.data(), im.data(), N); }
        const auto t1 = std::chrono::steady_clock::now();
        for (int k = 0; k < TEKRAR; ++k)
        {
            for (int i = 0; i < N; ++i) gercek[(size_t) i] = re[(size_t) i];
            juceFft.performRealOnlyForwardTransform (gercek.data());
            juceFft.performRealOnlyInverseTransform (gercek.data());
        }
        const auto t2 = std::chrono::steady_clock::now();
        const double bizUs = std::chrono::duration<double, std::micro> (t1 - t0).count() / TEKRAR;
        const double juceUs = std::chrono::duration<double, std::micro> (t2 - t1).count() / TEKRAR;
        // RT: JUCE yolu operator new yapıyor mu (alloca/SpinLock sayılmaz — ölçülemez, belgede)
        sweax::rt::resetViolations();
        {
            sweax::rt::Scope s;
            for (int k = 0; k < 10; ++k) { juceFft.performRealOnlyForwardTransform (gercek.data()); juceFft.performRealOnlyInverseTransform (gercek.data()); }
            for (int k = 0; k < 10; ++k) { biz.ileri (re.data(), im.data(), N); biz.geri (re.data(), im.data(), N); }
        }
        std::printf ("   FFT N=1024 ileri+geri: SWEAX %.2f us, juce::dsp::FFT(real-only) %.2f us; RT new ihlali %llu\n",
                     bizUs, juceUs, (unsigned long long) sweax::rt::violations());
        SWX_BEKLE_EQ (sweax::rt::violations(), 0u);
    }
}

SWX_TEST (spektral_motor_birim_yeniden_kurulum)
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (Quality q : { Quality::Canli, Quality::Yuksek })
            for (int kanal : { 1, 2 })
                for (int yol : { 0, 1 })               // 0: birim yol (nullptr), 1: FFT yolu (kazanç 1)
                {
                    SpektralMotor m; m.prepare (fs, 2); m.setQuality (q);
                    const int L = m.latencySamples (q);
                    const int n = (int) fs * 2;
                    auto a = beyaz (n, 11, 0.5);
                    auto s = sinus (fs, n, 1234.5, 0.5);
                    std::vector<float> ya = a, ys = s;
                    SabitKazanc birim;
                    SpektralIsleyici* isl = yol ? &birim : nullptr;
                    // kanal 0 gürültü, kanal 1 sinüs (stereo) / mono: iki ayrı motorla
                    if (kanal == 2)
                    {
                        for (int i = 0; i < n; i += 256)
                        {
                            const int c = std::min (256, n - i);
                            float* io[2] = { ya.data() + i, ys.data() + i };
                            m.isle (io, io, 2, c, isl);
                        }
                    }
                    else
                    {
                        SpektralMotor m2; m2.prepare (fs, 1); m2.setQuality (q);
                        for (int i = 0; i < n; i += 256)
                        {
                            const int c = std::min (256, n - i);
                            float* io1[1] = { ya.data() + i };
                            float* io2[1] = { ys.data() + i };
                            m.isle (io1, io1, 1, c, isl);
                            m2.isle (io2, io2, 1, c, isl);
                        }
                    }
                    const double dA = farkDb (ya, a, L, L + 4096, n);
                    const double dS = farkDb (ys, s, L, L + 4096, n);
                    if (ayrintili()) std::printf ("   fs=%.0f N=%d kanal=%d yol=%d  gürültü %.1f dB  sinüs %.1f dB\n", fs, m.boyut(), kanal, yol, dA, dS);
                    SWX_BEKLE (dA <= -80.0);
                    SWX_BEKLE (dS <= -80.0);
                }
}

SWX_TEST (spektral_motor_gecikme_durtu_ve_olcek)
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (Quality q : MODLAR)
        {
            SpektralMotor m; m.prepare (fs, 2); m.setQuality (q);
            const int L = m.latencySamples (q);
            SWX_BEKLE_EQ (L, SpektralMotor::boyutSec (fs, q) - 1);
            SabitKazanc yarim; yarim.g = 0.5f;
            const int n = 4 * 8192, p = 3001;
            std::vector<float> x ((size_t) n, 0.0f), y ((size_t) n, 0.0f);
            x[(size_t) p] = 1.0f; y[(size_t) p] = 1.0f;
            for (int i = 0; i < n; i += 100) { float* io[2] = { x.data() + i, y.data() + i }; m.isle (io, io, 2, std::min (100, n - i), &yarim); }
            SWX_BEKLE_EQ (tepeKonumu (x), p + L);
            SWX_BEKLE_YAKIN (x[(size_t) (p + L)], 0.5, 1e-4);
            SWX_BEKLE_EQ (tepeKonumu (y), p + L);
            double artik = 0.0;
            for (int i = 0; i < n; ++i) if (i != p + L) artik = std::max (artik, (double) std::fabs (x[(size_t) i]));
            SWX_BEKLE (artik < 1e-4);
        }
}

// ================================================================================================= YUMUŞAT ====
SWX_TEST (yumusat_a_depth0_birebir)
{
    const double fs = 48000.0;
    auto m = yumusatKur (fs, 0.0f);
    const int n = (int) fs * 2, L = m.latencySamples (Quality::Yuksek);
    auto l = beyaz (n, 3, 0.5); auto r = pembe (n, 4);
    auto l0 = l, r0 = r;
    calistir2 (m, l, r);
    SWX_BEKLE (farkDb (l, l0, L, L + 2048, n) <= -80.0);
    SWX_BEKLE (farkDb (r, r0, L, L + 2048, n) <= -80.0);
    SWX_BEKLE_EQ (m.lastReductionDb(), 0.0f);
}

SWX_TEST (yumusat_b_gecikme_durtu)
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (Quality q : MODLAR)
            for (int acik : { 1, 0 })
            {
                auto m = yumusatKur (fs, 0.7f, 2, q);
                m.setEnabled (acik != 0, true);
                const int L = m.latencySamples (q);
                const int n = 3 * 8192, p = 5003;
                std::vector<float> l ((size_t) n, 0.0f), r ((size_t) n, 0.0f);
                l[(size_t) p] = 0.5f; r[(size_t) p] = 0.5f;
                calistir2 (m, l, r, 128);
                SWX_BEKLE_EQ (tepeKonumu (l), p + L);
                SWX_BEKLE_YAKIN (l[(size_t) (p + L)], 0.5, 1e-3);
                SWX_BEKLE_EQ (tepeKonumu (r), p + L);
            }
}

SWX_TEST (yumusat_c_rezonans_bastirma)
{
    const double fs = 48000.0;
    const int n = (int) fs * 4, L = 1023, a = (int) fs * 2, b = n;
    auto gurultu = pembe (n, 21);
    const double yerel = binGucu (gurultu, fs, 1024, 3300.0, 3700.0);
    const double A = std::sqrt (yerel) * 10.0;                                  // yerel bin seviyesinin +20 dB üstü
    auto ton = sinus (fs, n, 3500.0, A);
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i) x[(size_t) i] = gurultu[(size_t) i] + ton[(size_t) i];
    for (float sharp : { 0.5f, 0.0f, 1.0f })
    {
        YumusatModule m; m.prepare (fs, 256, 2);
        YumusatParams p; p.on = true; p.depth = 0.7f; p.sharp = sharp; p.loHz = 2000.0f; p.hiHz = 12000.0f; p.speed = 0.5f;
        m.setParams (p); m.setEnabled (true, true);
        auto l = x, r = x;
        calistir2 (m, l, r);
        const double sinusAzalma = tonDb (l, fs, 3500.0, a, b) - tonDb (x, fs, 3500.0, a - L, b - L);
        const double gurultuDegisim = bantDb (l, a, b, fs, 1000.0, 2000.0) - bantDb (x, a - L, b - L, fs, 1000.0, 2000.0);
        std::printf ("   YUMUSAT rezonans (sharp %.1f, depth 0.7): 3.5 kHz sinüs %.2f dB, 1-2 kHz gürültü %+.2f dB, ölçer %.2f dB\n",
                     sharp, sinusAzalma, gurultuDegisim, m.lastReductionDb());
        SWX_BEKLE (sinusAzalma <= -6.0);
        SWX_BEKLE (std::fabs (gurultuDegisim) < 1.0);
        SWX_BEKLE (m.lastReductionDb() < -0.5f && m.lastReductionDb() <= 0.0f);
        SWX_BEKLE (farkDb (l, r, 0, 0, n) < -200.0);                              // özdeş L/R korunur
    }
}

SWX_TEST (yumusat_d_beyaz_gurultu_dokunulmaz)
{
    const double fs = 48000.0;
    const int n = (int) fs * 4, L = 1023, a = (int) fs;
    auto x = beyaz (n, 99, 0.3);
    for (float sharp : { 0.0f, 0.5f, 1.0f })
        for (float depth : { 0.35f, 0.7f, 1.0f })
        {
            YumusatModule m; m.prepare (fs, 256, 1);
            YumusatParams p; p.on = true; p.depth = depth; p.sharp = sharp;
            m.setParams (p); m.setEnabled (true, true);
            auto y = x;
            calistir1 (m, y);
            const double top = rmsDb (y, a, n) - rmsDb (x, a - L, n - L);
            const double odak = bantDb (y, a, n, fs, 2000.0, 12000.0) - bantDb (x, a - L, n - L, fs, 2000.0, 12000.0);
            if (ayrintili() || (sharp == 0.5f && depth == 0.7f))
                std::printf ("   YUMUSAT beyaz gürültü (sharp %.1f depth %.2f): toplam %+.3f dB, odak bandı %+.3f dB\n", sharp, depth, top, odak);
            SWX_BEKLE (std::fabs (top) < 1.0);
            // asıl ilgilenilen sayı ODAK BANDIDIR (denetim: eskiden yalnız toplam RMS denetleniyordu, odak serbestti)
            SWX_BEKLE (std::fabs (odak) < 1.5);
        }
}

SWX_TEST (yumusat_e_odak_disi_dokunulmaz)
{
    const double fs = 48000.0;
    const int n = (int) fs * 3, L = 1023, a = (int) fs;
    auto g = pembe (n, 5);
    const double yerel = binGucu (g, fs, 1024, 450.0, 550.0);
    auto t = sinus (fs, n, 500.0, std::sqrt (yerel) * 10.0);
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i) x[(size_t) i] = g[(size_t) i] + t[(size_t) i];
    YumusatModule m; m.prepare (fs, 256, 2);
    YumusatParams p; p.on = true; p.depth = 0.7f; p.loHz = 2000.0f; p.hiHz = 12000.0f;
    m.setParams (p); m.setEnabled (true, true);
    auto l = x, r = x;
    calistir2 (m, l, r);
    const double d = tonDb (l, fs, 500.0, a, n) - tonDb (x, fs, 500.0, a - L, n - L);
    if (ayrintili()) std::printf ("   odak dışı 500 Hz: %.3f dB\n", d);
    SWX_BEKLE (std::fabs (d) < 1.0);
}

SWX_TEST (yumusat_f_stereo_ozdes_ve_mono)
{
    const double fs = 48000.0;
    const int n = (int) fs * 2;
    auto g = pembe (n, 8);
    auto t = sinus (fs, n, 4200.0, 0.05);
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i) x[(size_t) i] = g[(size_t) i] + t[(size_t) i];
    auto m = yumusatKur (fs, 0.8f);
    auto l = x, r = x;
    calistir2 (m, l, r);
    bool ozdes = true;
    for (int i = 0; i < n; ++i) if (l[(size_t) i] != r[(size_t) i]) { ozdes = false; break; }
    SWX_BEKLE (ozdes);
    SWX_BEKLE (m.lastReductionDb() < -0.1f);
    auto mono = yumusatKur (fs, 0.8f, 1);
    auto y = x;
    calistir1 (mono, y);
    double maks = 0.0;
    for (int i = 0; i < n; ++i) maks = std::max (maks, (double) std::fabs (y[(size_t) i] - l[(size_t) i]));
    SWX_BEKLE (maks <= 1e-6);
    // L ≠ R: kazanç MID'den ortak → yalnız sağda olan rezonans solu da aynı eğriyle işler (imaj kaymaz)
    auto s = yumusatKur (fs, 0.8f);
    auto l2 = g, r2 = x;
    calistir2 (s, l2, r2);
    bool sonlu = true;
    for (auto v : l2) if (! std::isfinite (v)) { sonlu = false; break; }
    SWX_BEKLE (sonlu);
}

SWX_TEST (yumusat_g_blok_boyutu_degismez)
{
    const double fs = 44100.0;
    const int n = 30000;
    auto g = pembe (n, 13);
    auto t = sinus (fs, n, 3000.0, 0.08);
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i) x[(size_t) i] = g[(size_t) i] + t[(size_t) i];
    std::vector<float> refL, refR;
    for (int blok : { 256, 1, 7, 64, 4096 })
    {
        YumusatModule m; m.prepare (fs, 256, 2);
        YumusatParams p; p.on = true; p.depth = 0.9f; p.sharp = 0.8f; p.speed = 1.0f;
        m.setParams (p); m.setEnabled (true, true);
        auto l = x, r = pembe (n, 14);
        calistir2 (m, l, r, blok);
        if (refL.empty()) { refL = l; refR = r; continue; }
        double maks = 0.0;
        for (int i = 0; i < n; ++i) maks = std::max ({ maks, (double) std::fabs (l[(size_t) i] - refL[(size_t) i]), (double) std::fabs (r[(size_t) i] - refR[(size_t) i]) });
        SWX_BEKLE (maks <= 1e-6);
    }
}

SWX_TEST (yumusat_h_rt_tahsis_yok)
{
    const double fs = 48000.0;
    auto m = yumusatKur (fs, 0.6f);
    auto l = pembe (256, 1), r = pembe (256, 2);
    auto l0 = l, r0 = r;
    YumusatParams p; p.on = true;
    sweax::rt::resetViolations();
    {
        sweax::rt::Scope s;
        for (int k = 0; k < 600; ++k)
        {
            if (k % 50 == 0) m.setQuality (MODLAR[(k / 50) % 3]);
            p.depth = (float) (k % 11) / 10.0f; p.sharp = (float) (k % 7) / 6.0f; p.speed = (float) (k % 5) / 4.0f;
            p.loHz = 500.0f + (float) (k % 9) * 700.0f; p.hiHz = 3000.0f + (float) (k % 13) * 1300.0f;
            m.setParams (p);
            if (k % 97 == 0) m.setEnabled (k % 2 == 0);
            l = l0; r = r0;
            float* ch[2] = { l.data(), r.data() };
            m.process (ch, 2, 256);
        }
    }
    SWX_BEKLE_EQ (sweax::rt::violations(), 0u);
}

SWX_TEST (yumusat_i_nan_sigortasi)
{
    const double fs = 48000.0;
    auto m = yumusatKur (fs, 0.8f);
    const int n = (int) fs * 2;
    auto l = pembe (n, 30), r = pembe (n, 31);
    auto x = l;
    for (int i = 20000; i < 20256; ++i) { l[(size_t) i] = std::numeric_limits<float>::quiet_NaN(); r[(size_t) i] = (i % 2) ? std::numeric_limits<float>::infinity() : -3.0e30f; }
    calistir2 (m, l, r);
    bool sonlu = true;
    for (int i = 0; i < n; ++i) if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) { sonlu = false; break; }
    SWX_BEKLE (sonlu);
    SWX_BEKLE (std::isfinite (m.lastReductionDb()));
    // toparlanma: son 0.5 s giriş seviyesine yakın
    SWX_BEKLE (std::fabs (rmsDb (l, n - 24000, n) - rmsDb (x, n - 24000 - 1023, n - 1023)) < 1.0);
}

SWX_TEST (yumusat_j_sessizlik_pompalanmaz)
{
    const double fs = 48000.0;
    auto m = yumusatKur (fs, 1.0f);
    const int n = (int) fs * 2, L = 1023;
    const double tepe = std::pow (10.0, -90.0 / 20.0) * std::sqrt (3.0);         // düzgün dağılım RMS = tepe/√3 → −90 dBFS
    auto l = beyaz (n, 41, tepe), r = beyaz (n, 42, tepe);
    // sessizlikte bile dar tepe olsun (kapı olmasa bastırılırdı)
    auto t = sinus (fs, n, 5000.0, std::pow (10.0, -80.0 / 20.0));
    for (int i = 0; i < n; ++i) { l[(size_t) i] += t[(size_t) i]; r[(size_t) i] += t[(size_t) i]; }
    auto l0 = l, r0 = r;
    calistir2 (m, l, r);
    SWX_BEKLE (farkDb (l, l0, L, L + 2048, n) <= -80.0);
    SWX_BEKLE (farkDb (r, r0, L, L + 2048, n) <= -80.0);
    SWX_BEKLE_EQ (m.lastReductionDb(), 0.0f);
}

SWX_TEST (yumusat_k_acma_kapama_hizali)
{
    // depth 0 → ıslak = gecikmiş kuru; açma/kapama crossfade'i boyunca çıkış gecikmiş girişe birebir kalmalı
    const double fs = 48000.0;
    for (Quality q : { Quality::Canli, Quality::Yuksek })
    {
        auto m = yumusatKur (fs, 0.0f, 2, q);
        m.setEnabled (false, true);
        const int L = m.latencySamples (q), n = (int) fs * 3;
        auto l = pembe (n, 50), r = sinus (fs, n, 2500.0, 0.3);
        auto l0 = l, r0 = r;
        for (int i = 0, k = 0; i < n; i += 256, ++k)
        {
            if (k % 40 == 7) m.setEnabled (! m.isEnabled());
            float* ch[2] = { l.data() + i, r.data() + i };
            m.process (ch, 2, std::min (256, n - i));
        }
        SWX_BEKLE (farkDb (l, l0, L, L, n) <= -80.0);
        SWX_BEKLE (farkDb (r, r0, L, L, n) <= -80.0);
    }
}

// ================================================================================================= FORMANT ====
namespace
{
    // 2-kutuplu rezonatör kaskadı (F1 700 / F2 1200 / F3 2600) — kaynak dışarıdan gelir, ZARF aynı kalır
    void rezonatorZarfi (std::vector<float>& x, double fs)
    {
        struct R { double f, bw; };
        for (R rz : { R { 700.0, 90.0 }, R { 1200.0, 110.0 }, R { 2600.0, 170.0 } })
        {
            const double rr = std::exp (-PI * rz.bw / fs);
            const double a1 = -2.0 * rr * std::cos (2.0 * PI * rz.f / fs), a2 = rr * rr;
            const double g = (1.0 + a1 + a2);                                       // DC kazancı 1
            double y1 = 0.0, y2 = 0.0;
            for (size_t i = 0; i < x.size(); ++i)
            {
                const double y = g * (double) x[i] - a1 * y1 - a2 * y2;
                y2 = y1; y1 = y; x[i] = (float) y;
            }
        }
        double pk = 0.0;
        for (auto v : x) pk = std::max (pk, (double) std::fabs (v));
        for (auto& v : x) v = (float) (v * 0.3 / std::max (pk, 1.0e-12));
    }
    // F0 dürtü dizisi → rezonatör zarfı (ÖTÜMLÜ: harmonik tarak + zarf)
    std::vector<float> unlu (double fs, int n, double f0 = 120.0)
    {
        std::vector<float> x ((size_t) n, 0.0f);
        const int periyot = (int) std::lround (fs / f0);
        for (int i = 0; i < n; i += periyot) x[(size_t) i] = 1.0f;
        rezonatorZarfi (x, fs);
        return x;
    }
    // beyaz gürültü → AYNI rezonatör zarfı (ÖTÜMSÜZ: tarak yok, spektral biçim özdeş) — eşleştirilmiş kaynak
    std::vector<float> unluGurultu (double fs, int n, unsigned seed)
    {
        auto x = beyaz (n, seed, 1.0);
        rezonatorZarfi (x, fs);
        return x;
    }
    // harmonik örnekli zarfta [f1, f2] içindeki tepe frekansı (dB genliklerinde parabolik ara değer)
    double formantTepesi (const std::vector<float>& x, double fs, double f0, int a, int b, double f1, double f2)
    {
        int hIlk = (int) std::ceil (f1 / f0), hSon = (int) std::floor (f2 / f0);
        int enIyi = hIlk; double enDb = -1e9;
        for (int h = hIlk; h <= hSon; ++h) { const double v = tonDb (x, fs, h * f0, a, b); if (v > enDb) { enDb = v; enIyi = h; } }
        const double y0 = tonDb (x, fs, (enIyi - 1) * f0, a, b), y1 = enDb, y2 = tonDb (x, fs, (enIyi + 1) * f0, a, b);
        const double payda = y0 - 2.0 * y1 + y2;
        const double d = std::fabs (payda) > 1e-9 ? std::clamp (0.5 * (y0 - y2) / payda, -0.5, 0.5) : 0.0;
        return (enIyi + d) * f0;
    }
    double otokorelasyonF0 (const std::vector<float>& x, double fs, int a, int b)
    {
        const int minLag = (int) (fs / 600.0), maxLag = (int) (fs / 80.0);
        std::vector<double> r ((size_t) maxLag + 2, 0.0);
        for (int lag = minLag - 1; lag <= maxLag + 1; ++lag)
        {
            double s = 0.0, e0 = 0.0, e1 = 0.0;
            for (int i = a; i < b - lag; ++i) { s += (double) x[(size_t) i] * x[(size_t) (i + lag)]; e0 += (double) x[(size_t) i] * x[(size_t) i]; e1 += (double) x[(size_t) (i + lag)] * x[(size_t) (i + lag)]; }
            r[(size_t) lag] = s / std::sqrt (e0 * e1 + 1e-30);
        }
        int en = minLag;
        for (int lag = minLag; lag <= maxLag; ++lag) if (r[(size_t) lag] > r[(size_t) en]) en = lag;
        const double y0 = r[(size_t) (en - 1)], y1 = r[(size_t) en], y2 = r[(size_t) (en + 1)];
        const double payda = y0 - 2.0 * y1 + y2;
        const double d = std::fabs (payda) > 1e-12 ? 0.5 * (y0 - y2) / payda : 0.0;
        return fs / ((double) en + d);
    }
}

SWX_TEST (formant_a_shift0_birebir)
{
    const double fs = 48000.0;
    auto m = formantKur (fs, 0.0f);
    const int n = (int) fs * 2, L = 1023;
    auto l = unlu (fs, n), r = beyaz (n, 2, 0.4);
    auto l0 = l, r0 = r;
    calistir2 (m, l, r);
    SWX_BEKLE (farkDb (l, l0, L, L + 2048, n) <= -80.0);
    SWX_BEKLE (farkDb (r, r0, L, L + 2048, n) <= -80.0);
    // mix 0 (shift ≠ 0) de birim
    auto k = formantKur (fs, 4.0f, 0.0f);
    auto l2 = l0, r2 = r0;
    calistir2 (k, l2, r2);
    SWX_BEKLE (farkDb (l2, l0, L, L + 2048, n) <= -80.0);
}

SWX_TEST (formant_b_c_d_unlu_kaydirma_perde_seviye)
{
    const double fs = 48000.0, f0 = 120.0;
    const int n = (int) fs * 3, L = 1023, a = (int) fs, b = n;
    const auto x = unlu (fs, n, f0);
    const double F1g = formantTepesi (x, fs, f0, a - L, b - L, 700.0 / 1.2, 700.0 * 1.2);
    const double F2g = formantTepesi (x, fs, f0, a - L, b - L, 1200.0 / 1.2, 1200.0 * 1.2);
    const double rmsGiris = rmsDb (x, a - L, b - L);
    struct Durum { float st; double oran, tol; };
    for (Durum d : { Durum { 3.0f, 1.19, 0.07 }, Durum { -3.0f, 0.84, 0.06 } })
    {
        auto m = formantKur (fs, d.st, 1.0f, 1);
        auto y = x;
        calistir1 (m, y);
        const double beklenen = std::pow (2.0, d.st / 12.0);
        const double F1c = formantTepesi (y, fs, f0, a, b, 700.0 * beklenen / 1.2, 700.0 * beklenen * 1.2);
        const double F2c = formantTepesi (y, fs, f0, a, b, 1200.0 * beklenen / 1.2, 1200.0 * beklenen * 1.2);
        const double F0c = otokorelasyonF0 (y, fs, a, a + 24000);
        const double rmsFark = rmsDb (y, a, b) - rmsGiris;
        std::printf ("   FORMANT %+.0f st: F1 %.0f→%.0f Hz (x%.3f), F2 %.0f→%.0f Hz (x%.3f), F0 %.2f Hz, RMS %+.2f dB, telafi %+.2f dB\n",
                     d.st, F1g, F1c, F1c / F1g, F2g, F2c, F2c / F2g, F0c, rmsFark, m.lastLevelCompDb());
        SWX_BEKLE_YAKIN (F1c / F1g, d.oran, d.tol);
        SWX_BEKLE_YAKIN (F2c / F2g, d.oran, d.tol);
        SWX_BEKLE_YAKIN (F0c, 120.0, 1.2);                                    // (c) perde ±%1
        SWX_BEKLE (std::fabs (rmsFark) <= 1.5);                               // (d) seviye
        SWX_BEKLE_YAKIN (m.currentShiftSt(), d.st, 0.01);
        SWX_BEKLE_YAKIN (m.lastLifterMs(), 0.35 * 1000.0 / f0, 0.1);          // perdeye uyarlı kesim 0.35·T0
    }
    SWX_BEKLE_YAKIN (otokorelasyonF0 (x, fs, a, a + 24000), 120.0, 1.2);
}

SWX_TEST (formant_e_gecikme_tam)
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (Quality q : MODLAR)
        {
            auto m = formantKur (fs, 0.0f, 1.0f, 2, q);
            const int L = m.latencySamples (q);
            SWX_BEKLE_EQ (L, SpektralMotor::boyutSec (fs, q) - 1);
            const int n = 3 * 8192, p = 4099;
            std::vector<float> l ((size_t) n, 0.0f), r ((size_t) n, 0.0f);
            l[(size_t) p] = 0.7f; r[(size_t) p] = -0.7f;
            calistir2 (m, l, r, 100);
            SWX_BEKLE_EQ (tepeKonumu (l), p + L);
            SWX_BEKLE_YAKIN (r[(size_t) (p + L)], -0.7, 1e-3);
            // kapalı: kuru yol aynı gecikme
            m.setEnabled (false, true);
            std::vector<float> l2 ((size_t) n, 0.0f), r2 ((size_t) n, 0.0f);
            l2[(size_t) p] = 1.0f;
            calistir2 (m, l2, r2, 333);
            SWX_BEKLE_EQ (tepeKonumu (l2), p + L);
        }
    // shift +3 (sıfır fazlı kazanç): gürültü çapraz ilinti tepesi TAM gecikmede
    const double fs = 48000.0;
    auto m = formantKur (fs, 3.0f, 1.0f, 1);
    const int n = (int) fs * 2, L = m.latencySamples (Quality::Yuksek);
    auto x = beyaz (n, 77, 0.4);
    auto y = x;
    calistir1 (m, y);
    int enIyi = 0; double enR = -1e9;
    for (int lag = L - 40; lag <= L + 40; ++lag)
    {
        double s = 0.0;
        for (int i = (int) fs / 2; i < n; ++i) s += (double) y[(size_t) i] * x[(size_t) (i - lag)];
        if (s > enR) { enR = s; enIyi = lag; }
    }
    SWX_BEKLE_EQ (enIyi, L);
}

SWX_TEST (formant_f_blok_boyutu_degismez)
{
    const double fs = 48000.0;
    const int n = 30000;
    const auto x = unlu (fs, n);
    std::vector<float> refL, refR;
    for (int blok : { 256, 1, 7, 64, 4096 })
    {
        auto m = formantKur (fs, 3.0f, 0.7f);
        auto l = x, r = pembe (n, 3);
        // çalışırken shift değişimi (yumuşatma yolu da değişmez olmalı)
        const int yarim = n / 2;
        for (int i = 0; i < n; i += blok)
        {
            if (i >= yarim && i - blok < yarim) { FormantParams p; p.on = true; p.shiftSt = -4.0f; p.mix = 0.7f; m.setParams (p); }
            float* ch[2] = { l.data() + i, r.data() + i };
            m.process (ch, 2, std::min (blok, n - i));
        }
        if (refL.empty()) { refL = l; refR = r; continue; }
        // parametre bloğun başında okunur: değişim anı blok ızgarasına bağlı → yalnız ilk yarı birebir karşılaştırılır
        double maks = 0.0;
        for (int i = 0; i < yarim - 4096; ++i) maks = std::max ({ maks, (double) std::fabs (l[(size_t) i] - refL[(size_t) i]), (double) std::fabs (r[(size_t) i] - refR[(size_t) i]) });
        SWX_BEKLE (maks <= 1e-6);
    }
    // sabit parametre: tamamı birebir
    std::vector<float> ref2;
    for (int blok : { 256, 1, 7, 64, 4096 })
    {
        auto m = formantKur (fs, -5.0f, 0.6f, 1);
        auto y = x;
        calistir1 (m, y, blok);
        if (ref2.empty()) { ref2 = y; continue; }
        double maks = 0.0;
        for (int i = 0; i < n; ++i) maks = std::max (maks, (double) std::fabs (y[(size_t) i] - ref2[(size_t) i]));
        SWX_BEKLE (maks <= 1e-6);
    }
}

SWX_TEST (formant_g_rt_tahsis_yok)
{
    const double fs = 48000.0;
    auto m = formantKur (fs, 2.0f);
    auto l = unlu (fs, 256), r = pembe (256, 2);
    auto l0 = l, r0 = r;
    FormantParams p; p.on = true;
    sweax::rt::resetViolations();
    {
        sweax::rt::Scope s;
        for (int k = 0; k < 600; ++k)
        {
            if (k % 50 == 0) m.setQuality (MODLAR[(k / 50) % 3]);
            p.shiftSt = (float) ((k % 13) - 6); p.mix = (float) (k % 5) / 4.0f;
            m.setParams (p);
            if (k % 97 == 0) m.setEnabled (k % 2 == 0);
            l = l0; r = r0;
            float* ch[2] = { l.data(), r.data() };
            m.process (ch, 2, 256);
        }
    }
    SWX_BEKLE_EQ (sweax::rt::violations(), 0u);
}

SWX_TEST (formant_h_nan_sigortasi)
{
    const double fs = 48000.0;
    auto m = formantKur (fs, -3.0f);
    const int n = (int) fs * 2;
    auto l = unlu (fs, n), r = pembe (n, 31);
    auto x = l;
    for (int i = 15000; i < 15300; ++i) { l[(size_t) i] = std::numeric_limits<float>::quiet_NaN(); r[(size_t) i] = std::numeric_limits<float>::infinity(); }
    calistir2 (m, l, r);
    bool sonlu = true;
    for (int i = 0; i < n; ++i) if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) { sonlu = false; break; }
    SWX_BEKLE (sonlu);
    SWX_BEKLE (std::isfinite (m.lastLevelCompDb()));
    SWX_BEKLE (std::fabs (rmsDb (l, n - 24000, n) - rmsDb (x, n - 24000 - 1023, n - 1023)) < 1.5);
}

SWX_TEST (formant_i_mix_comb_yok)
{
    const double fs = 48000.0;
    const int n = (int) fs * 4, L = 1023, a = (int) fs;
    const auto x = beyaz (n, 123, 0.4);
    // DENETİM: eskiden ilk blok shift 0 / mix 0.5 kuruyordu — shift 0'da modül BİRİM yola geçtiği için mixCur'a
    // hiç ulaşılmıyordu, yani "1/3 oktav birim" iddiası formant_a'nın yeniden ifadesiydi. Artık her shift için
    // mix 0.5 ≡ 0.5·ıslak(mix 1) + 0.5·gecikmiş kuru ÖZDEŞLİĞİ doğrulanıyor (α ≠ 1 olan +0.5 st dahil).
    for (float st : { 0.5f, 2.0f, 4.0f, -3.0f })
    {
        auto tam = formantKur (fs, st, 1.0f, 1);
        auto yarim = formantKur (fs, st, 0.5f, 1);
        auto y1 = x, y2 = x;
        calistir1 (tam, y1);
        calistir1 (yarim, y2);
        std::vector<float> beklenen ((size_t) n, 0.0f);
        for (int i = L; i < n; ++i) beklenen[(size_t) i] = 0.5f * y1[(size_t) i] + 0.5f * x[(size_t) (i - L)];
        const double d = farkDb (y2, beklenen, 0, a, n);
        double enKotu = 0.0;                                                         // 1/3 oktavda comb çukuru yok
        for (double fc = 200.0; fc <= 8000.0 * 1.001; fc *= std::pow (2.0, 1.0 / 3.0))
        {
            const double f1 = fc * std::pow (2.0, -1.0 / 6.0), f2 = fc * std::pow (2.0, 1.0 / 6.0);
            enKotu = std::max (enKotu, std::fabs (bantDb (y2, a, n, fs, f1, f2) - bantDb (beklenen, a, n, fs, f1, f2)));
        }
        // ıslak yolun gerçekten çalıştığını da kanıtla: mix 1 çıkışı kuru girişten AYRI olmalı
        const double islakFark = farkDb (y1, x, L, a, n);
        if (ayrintili())
            std::printf ("   mix 0.5 shift %+.1f st: doğrusal karışım farkı %.1f dB, en kötü 1/3 oktav %.3f dB (ıslak≠kuru %.1f dB)\n",
                         st, d, enKotu, islakFark);
        SWX_BEKLE (d <= -60.0);
        SWX_BEKLE (enKotu <= 0.5);
        SWX_BEKLE (islakFark > -40.0);
    }
}

SWX_TEST (formant_j_perde_dalgalanmasi_zarfa_sizmaz)
{
    // Yumuşak eğimli (tek kutup AGF 1.5 kHz) dürtü dizisi: ideal kazanç eğrisi frekansta düzgündür. Perde dalgalanması
    // zarfa sızarsa harmonik başına kazanç testere gibi dalgalanır (ölçüldü: kesim T0/2 iken F0 400'de 4 dB).
    const double fs = 48000.0;
    const int n = (int) fs * 2, L = 1023, a = (int) fs / 2, b = n;
    for (int periyot : { 120, 192, 267, 400 })                                      // 400 / 250 / 180 / 120 Hz
    {
        const double f0 = fs / periyot;
        std::vector<float> x ((size_t) n, 0.0f);
        for (int i = 0; i < n; i += periyot) x[(size_t) i] = 1.0f;
        const double c = std::exp (-2.0 * PI * 1500.0 / fs);
        double y1 = 0.0;
        for (int i = 0; i < n; ++i) { y1 = (1.0 - c) * x[(size_t) i] + c * y1; x[(size_t) i] = (float) (0.3 * y1); }
        for (float st : { 4.0f, -4.0f })
        {
            auto m = formantKur (fs, st, 1.0f, 1);
            auto y = x;
            calistir1 (m, y);
            std::vector<double> g;
            for (int h = 1; h * f0 <= 8000.0; ++h) g.push_back (tonDb (y, fs, h * f0, a, b) - tonDb (x, fs, h * f0, a - L, b - L));
            double enKotu = 0.0;
            for (size_t h = 1; h + 1 < g.size(); ++h) enKotu = std::max (enKotu, std::fabs (g[h] - 0.5 * (g[h - 1] + g[h + 1])));
            if (ayrintili()) std::printf ("   F0 %.0f Hz %+.0f st: harmonik kazanç 2. fark maks %.2f dB, lifter %.2f ms\n", f0, st, enKotu, m.lastLifterMs());
            SWX_BEKLE (enKotu <= 1.0);
            SWX_BEKLE_YAKIN (m.lastLifterMs(), 0.35 * 1000.0 * periyot / fs, 0.05);
        }
    }
}

// ============================================================================================== PERFORMANS ====
SWX_TEST (spektral_performans_olcumu)
{
    const double fs = 48000.0;
    const int saniye = 20, n = (int) fs * saniye, blok = 256;
    auto g = pembe (n, 900);
    auto v = unlu (fs, n);
    auto t = sinus (fs, n, 3500.0, 0.05);
    std::vector<float> l ((size_t) n), r ((size_t) n);
    for (int i = 0; i < n; ++i) { l[(size_t) i] = v[(size_t) i] + 0.3f * g[(size_t) i] + t[(size_t) i]; r[(size_t) i] = v[(size_t) i] + 0.3f * g[(size_t) (n - 1 - i)] + t[(size_t) i]; }
    auto olc = [&] (auto& m)
    {
        auto a = l, b = r;
        const auto t0 = std::chrono::steady_clock::now();
        calistir2 (m, a, b, blok);
        const auto t1 = std::chrono::steady_clock::now();
        bool sonlu = true;
        for (int i = 0; i < n; i += 97) if (! std::isfinite (a[(size_t) i])) sonlu = false;
        return std::make_pair (100.0 * std::chrono::duration<double> (t1 - t0).count() / (double) saniye, sonlu);
    };
    auto y = yumusatKur (fs, 0.7f);
    auto f = formantKur (fs, 3.0f);
    auto yc = yumusatKur (fs, 0.7f, 2, Quality::Canli);
    auto fc = formantKur (fs, 3.0f, 1.0f, 2, Quality::Canli);
    const auto py = olc (y), pf = olc (f), pyc = olc (yc), pfc = olc (fc);
    std::printf ("   PERFORMANS 48k stereo 256 blok (tek çekirdek): YUMUSAT %.2f%% (CANLI %.2f%%), FORMANT %.2f%% (CANLI %.2f%%) gerçek zaman\n",
                 py.first, pyc.first, pf.first, pfc.first);
    SWX_BEKLE (py.second && pf.second && pyc.second && pfc.second);
}

// ============================================================================ DENETİM REGRESYONLARI (M3.5) ====
// Şüpheci denetim turunda ÖLÇÜLEN kusurların her biri için, kusuru yakalayan test. Her testin yorumunda
// düzeltme öncesi/sonrası ölçüm var; "ÖNCE" değerleri bu testleri düşürürdü.
namespace
{
    // zamanla DEĞİŞMEYEN eğim işleyicisi — motorun kendi dalgalanma tabanını ölçmek için
    struct EgimIsleyici : SpektralIsleyici
    {
        bool cerceveIsle (const SpektralCerceve& c, float* kazanc) noexcept override
        {
            for (int b = 0; b < c.bins; ++b) kazanc[b] = 1.0f + 0.3f * (float) b / (float) c.bins;
            return true;
        }
    };
    // gecikme hizalı 1/3 oktav bant kazancının ZAMAN std'si (Hann 1024 / hop 512 çerçevelerde).
    // Sinyalin kendi dalgalanması oranda sadeleşir → kalan, işleyicinin kazanç eğrisinin oynamasıdır.
    double bantKazancStd (const std::vector<float>& y, const std::vector<float>& x, int L, double fs,
                          int a, int b, double fc)
    {
        constexpr int N = 1024;
        HizliFFT f; f.hazirla (N);
        std::vector<float> re ((size_t) N), im ((size_t) N), w ((size_t) N);
        for (int i = 0; i < N; ++i) w[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * PI * i / N));
        const double f1 = fc * std::pow (2.0, -1.0 / 6.0), f2 = fc * std::pow (2.0, 1.0 / 6.0);
        std::vector<double> oran;
        for (int s = a; s + N <= b; s += N / 2)
        {
            auto enerji = [&] (const std::vector<float>& v, int ofs)
            {
                for (int i = 0; i < N; ++i)
                { re[(size_t) i] = v[(size_t) (s + ofs + i)] * w[(size_t) i]; im[(size_t) i] = 0.0f; }
                f.ileri (re.data(), im.data(), N);
                double t = 0.0;
                for (int k = 1; k < N / 2; ++k)
                {
                    const double hz = k * fs / N;
                    if (hz >= f1 && hz < f2)
                        t += (double) re[(size_t) k] * re[(size_t) k] + (double) im[(size_t) k] * im[(size_t) k];
                }
                return t;
            };
            oran.push_back (10.0 * std::log10 ((enerji (y, 0) + 1e-30) / (enerji (x, -L) + 1e-30)));
        }
        double ort = 0.0;
        for (double v : oran) ort += v;
        ort /= std::max<size_t> (1, oran.size());
        double va = 0.0;
        for (double v : oran) va += (v - ort) * (v - ort);
        return std::sqrt (va / std::max<size_t> (1, oran.size()));
    }
}

// BULGU 1 (kritik): F0 <= ~108 Hz'de formant kaydırma çalışmıyordu. Perde TEK ÇERÇEVEDEN çıkarılıyordu ve 21 ms
// pencereye ~2.3 periyottan az sığan perdelerde çöküyordu (lifter varsayılan 2 ms'ye düşüyor → düz kepstrum).
// ÖNCE (48k, +3 st, hedef ×1.189): F0 85 ×1.035 | 90 ×1.030 | 95 ×1.067 | 100 ×1.019 | 105 ×1.057 | 110 ×1.210
// SONRA (ortak, çerçeveden bağımsız NCCF): 85 ×1.178 | 90 ×1.204 | 95 ×1.189 | 100 ×1.236 | 105 ×1.166 | 110 ×1.208
// Hedef malzeme Türkçe erkek rap vokalidir (F0 85–120 Hz) ve 48 kHz standart oturum hızıdır.
// TOLERANS YÖNE GÖRE AYRI — ölçülen sistematik yanlılık dürüstçe yazılıyor. AŞAĞI yön yukarıdan zayıf:
//   +3 st (hedef ×1.189): 85 ×1.178 | 90 ×1.204 | 100 ×1.236 | 110 ×1.208 | 120 ×1.200   → sapma ≤ 0.047
//   −3 st (hedef ×0.841): 85 ×0.922 | 90 ×0.865 | 100 ×0.894 | 110 ×0.907 | 120 ×0.878   → sapma ≤ 0.081
// Sebep kepstral zarfın ÇÖZÜNÜRLÜĞÜ: 0.35·T0 kesimi 90 Hz genişlikli F1'i ~240 Hz'e yumuşatır, tepe konumu komşu
// F2'ye (1200 Hz) doğru yanlı olur; AŞAĞI kaydırmada bu yanlılık kaydırmaya KARŞI çalışır. KESIM_ORANI 0.30 ve
// 0.25 DENENDİ: aşağı yön düzelmedi (85 Hz'de ×0.929 / ×0.935) ama yukarı yön F0 120'de ×1.077 / ×1.048'e çöktü
// ve formant_j (harmonik kazanç eğrisi düzgünlüğü) düştü → 0.35 korunuyor, tolerans ölçüme göre yazılıyor.
SWX_TEST (denetim_1_formant_dusuk_f0_kaydirma)
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        const int L = SpektralMotor::boyutSec (fs) - 1;
        const int n = (int) fs * 3, a = (int) fs, b = n;
        for (double f0Ham : { 85.0, 90.0, 100.0, 110.0, 120.0 })
        {
            const int per = (int) std::lround (fs / f0Ham);
            const double f0 = fs / per;                       // dürtü ızgarasına oturan gerçek F0
            const auto x = unlu (fs, n, f0Ham);
            const double F1g = formantTepesi (x, fs, f0, a - L, b - L, 700.0 / 1.25, 700.0 * 1.25);
            for (float st : { 3.0f, -3.0f })
            {
                const double hedef = std::pow (2.0, (double) st / 12.0);
                auto m = formantKur (fs, st, 1.0f, 1);
                auto y = x;
                calistir1 (m, y);
                const double F1c = formantTepesi (y, fs, f0, a, b, 700.0 * hedef / 1.3, 700.0 * hedef * 1.3);
                const double oran = F1c / F1g;
                if (ayrintili())
                    std::printf ("   %.0f Hz F0 %6.1f %+.0f st: F1 x%.3f (hedef x%.3f) lifter %.2f ms F0algi %.1f Hz\n",
                                 fs, f0, (double) st, oran, hedef, m.lastLifterMs(), m.lastF0Hz());
                SWX_BEKLE_YAKIN (oran, hedef, st > 0.0f ? 0.075 : 0.090);
                // asıl regresyon: kaydırma GERÇEKTEN oluyor mu (düzeltme öncesi ×1.02–1.07 / ×0.97 idi)
                SWX_BEKLE (st > 0.0f ? oran >= 1.13 : oran <= 0.94);
            }
        }
    }
}

// BULGU 2 + 10: setQuality ses akarken motorun halkalarını ANINDA sıfırlıyordu → tam ölçekli TIK + sert kopma.
// ÖNCE (48k, 3.5 kHz sinüs, blok 256): örnekten örneğe sıçrama 0.4424 (girişin kendi maks sıçraması 0.2270 — iki
//   katı), 12–22 kHz bandı −59.1 dB tabandan +17.4 dB'ye (+76 dB splatter), 9.33 / 20.00 ms SERT sessizlik.
// SONRA (ertelenmiş boyut değişimi + smoothstep sönüm/yükselme): sıçrama 0.064–0.091 (girişinkinin ALTINDA ve
//   mod sabitken ölçülen 0.0779 tabanıyla aynı mertebede), splatter −35.4…−53.0 dB, çukur 12.0 / 22.7 ms ama
//   TAMAMEN rampalı (OLA'nın yeniden dolması için gereken N örnek + 3 ms sönüm + 5 ms yükselme).
SWX_TEST (denetim_2_mod_gecisi_tiksiz)
{
    const double fs = 48000.0;
    const int n = (int) fs * 2, at = (int) fs, blok = 256;
    const auto x = sinus (fs, n, 3500.0, 0.5);
    double girisSicrama = 0.0;
    for (int i = 1; i < n; ++i)
        girisSicrama = std::max (girisSicrama, (double) std::fabs (x[(size_t) i] - x[(size_t) (i - 1)]));

    auto olc = [&] (const char* ad, auto& m, Quality yeni)
    {
        auto y = x;
        bool yapildi = false;
        for (int i = 0; i < n; i += blok)
        {
            if (! yapildi && i >= at) { m.setQuality (yeni); yapildi = true; }
            float* ch[1] = { y.data() + i };
            m.process (ch, 1, std::min (blok, n - i));
        }
        double sicrama = 0.0;
        for (int i = at - 2000; i < at + (int) (fs * 0.15); ++i)
            sicrama = std::max (sicrama, (double) std::fabs (y[(size_t) i] - y[(size_t) (i - 1)]));
        // giriş sinyalinde BOŞ olan bant: geçişin ürettiği geniş bant enerji
        const double splatter = bantDb (y, at - 4096, at + 8192, fs, 12000.0, 22000.0);
        double kar = 0.0;                                     // kararlı seviye (geçişten uzak)
        for (int i = (int) (fs * 0.5); i < (int) (fs * 0.9); ++i) kar += (double) y[(size_t) i] * y[(size_t) i];
        kar = std::sqrt (kar / (0.4 * fs));
        int cukur = 0;
        for (int s = at - 256; s + 64 < at + (int) (fs * 0.15); s += 64)
        {
            double e = 0.0;
            for (int i = s; i < s + 64; ++i) e += (double) y[(size_t) i] * y[(size_t) i];
            if (20.0 * std::log10 (std::max (std::sqrt (e / 64.0), 1e-14) / std::max (kar, 1e-14)) < -20.0) cukur += 64;
        }
        if (ayrintili())
            std::printf ("   %s: sicrama %.4f (giris %.4f) | 12-22k %.1f dB | cukur %.2f ms\n",
                         ad, sicrama, girisSicrama, splatter, 1000.0 * cukur / fs);
        SWX_BEKLE (sicrama <= girisSicrama);        // geçiş, girişin kendisinde olmayan bir basamak ÜRETMEZ
        SWX_BEKLE (splatter <= -30.0);              // ÖNCE: +17.4 dB
        SWX_BEKLE (1000.0 * cukur / fs <= 30.0);    // rampalı çukur, ama sınırlı
    };
    for (Quality yeni : { Quality::Canli, Quality::Yuksek })
    {
        const Quality bas = yeni == Quality::Canli ? Quality::Yuksek : Quality::Canli;
        auto y = yumusatKur (fs, 0.7f, 1, bas);
        olc (yeni == Quality::Canli ? "YUMUSAT YUKSEK->CANLI" : "YUMUSAT CANLI->YUKSEK", y, yeni);
        auto f = formantKur (fs, 3.0f, 1.0f, 1, bas);
        olc (yeni == Quality::Canli ? "FORMANT YUKSEK->CANLI" : "FORMANT CANLI->YUKSEK", f, yeni);
    }
    // Yüksek↔Ultra: N değişmiyor → TAM süreklilik (tek örnek bile farklı olmamalı)
    {
        auto a1 = formantKur (fs, 3.0f, 1.0f, 1, Quality::Yuksek);
        auto a2 = formantKur (fs, 3.0f, 1.0f, 1, Quality::Yuksek);
        auto y1 = x, y2 = x;
        for (int i = 0; i < n; i += blok)
        {
            if (i >= at) a1.setQuality (Quality::Ultra);
            float* c1[1] = { y1.data() + i };
            float* c2[1] = { y2.data() + i };
            a1.process (c1, 1, std::min (blok, n - i));
            a2.process (c2, 1, std::min (blok, n - i));
        }
        double maks = 0.0;
        for (int i = 0; i < n; ++i) maks = std::max (maks, (double) std::fabs (y1[(size_t) i] - y2[(size_t) i]));
        SWX_BEKLE_EQ (maks, 0.0);
    }
}

// BULGU 3 (yüksek): YUMUŞAT rezonans OLMAYAN temiz harmonik sinyali bastırıyordu. CFAR koruma bandı SABİT ±2 bindi;
// harmonik aralığı ~5 bini geçince eğitim binleri vadilere düşüyor, referans çöküyor, her harmonik "fazlalık"
// sayılıyordu. Eşleştirilmiş kaynak sondası: dürtü dizisi (harmonik) ve beyaz gürültü (harmoniksiz) AYNI
// 3-rezonatör zarfından geçer → girişte rezonans YOK, spektral biçim ÖZDEŞ; tek fark harmonik yapıdır.
// ÖNCE (48k, depth 0.7, 2–12 kHz): sharp 0.5 → F0 150 harm −0.93 / gür −1.82 | 300 −2.95 / −1.82 | 400 −7.85 / −1.82
//                                  sharp 1.0 → F0 300 −6.16 / −1.23 | 400 −8.51 / −1.23  (fark −7.18 dB'ye kadar)
// SONRA (genişletme yarıçapı = harmonik aralığının yarısı): fark +0.51 … +1.42 dB — harmonik içerik artık
//   gürültüden DAHA AZ bastırılıyor (güvenli yön).
SWX_TEST (denetim_3_yumusat_harmonik_yanliligi_yok)
{
    const double fs = 48000.0;
    const int n = (int) fs * 3, L = 1023, a = (int) fs, b = n;
    const auto xg = unluGurultu (fs, n, 77);
    for (float sharp : { 0.5f, 1.0f })
    {
        YumusatParams p; p.on = true; p.depth = 0.7f; p.sharp = sharp;
        YumusatModule mg;
        mg.prepare (fs, 256, 1); mg.setParams (p); mg.setEnabled (true, true);
        auto yg = xg;
        calistir1 (mg, yg);
        const double gur = bantDb (yg, a, b, fs, 2000.0, 12000.0) - bantDb (xg, a - L, b - L, fs, 2000.0, 12000.0);
        for (double f0 : { 150.0, 220.0, 300.0, 400.0 })
        {
            const auto xh = unlu (fs, n, f0);
            YumusatModule mh;
            mh.prepare (fs, 256, 1); mh.setParams (p); mh.setEnabled (true, true);
            auto yh = xh;
            calistir1 (mh, yh);
            const double harm = bantDb (yh, a, b, fs, 2000.0, 12000.0) - bantDb (xh, a - L, b - L, fs, 2000.0, 12000.0);
            if (ayrintili())
                std::printf ("   sharp %.1f F0 %5.0f: harmonik %+.2f dB | gurultu %+.2f dB | fark %+.2f (genisletme +-%d bin, F0algi %.0f Hz)\n",
                             (double) sharp, f0, harm, gur, harm - gur, mh.lastExpandBins(), mh.lastF0Hz());
            SWX_BEKLE (harm - gur >= -0.5);          // harmonik içerik, özdeş zarflı gürültüden FAZLA bastırılmaz
            SWX_BEKLE (std::fabs (harm - gur) <= 2.0);
        }
    }
}

// BULGU 4 (orta): aynı ayarla Kalite modu değişince bastırma 7.5 dB değişiyordu (Kalite TON değiştiriyordu) —
// referans penceresi ve koruma bandı BİN cinsindendi, CANLI'da N yarıya inince harmonik aralığı da yarıya iniyordu.
// ÖNCE (48k, depth 0.7, sharp 0.5): F0 150 → 0.54 dB | 300 → 2.54 dB | 400 → 7.53 dB fark.
// SONRA (yarıçap HZ cinsinden, perdeden türetiliyor): <= 0.88 dB.
SWX_TEST (denetim_4_yumusat_kalite_tonu_degistirmez)
{
    const double fs = 48000.0;
    const int n = (int) fs * 3, a = (int) fs, b = n;
    for (float sharp : { 0.5f, 1.0f })
        for (double f0 : { 150.0, 220.0, 300.0, 400.0 })
        {
            const auto x = unlu (fs, n, f0);
            double sonuc[2];
            int qi = 0;
            for (Quality q : { Quality::Yuksek, Quality::Canli })
            {
                const int L = SpektralMotor::boyutSec (fs, q) - 1;
                YumusatParams p; p.on = true; p.depth = 0.7f; p.sharp = sharp;
                YumusatModule m;
                m.prepare (fs, 256, 1); m.setQuality (q); m.setParams (p); m.setEnabled (true, true);
                auto y = x;
                calistir1 (m, y);
                sonuc[qi++] = bantDb (y, a, b, fs, 2000.0, 12000.0) - bantDb (x, a - L, b - L, fs, 2000.0, 12000.0);
            }
            if (ayrintili())
                std::printf ("   sharp %.1f F0 %5.0f: YUKSEK %+.2f dB | CANLI %+.2f dB | fark %.2f dB\n",
                             (double) sharp, f0, sonuc[0], sonuc[1], std::fabs (sonuc[0] - sonuc[1]));
            SWX_BEKLE (std::fabs (sonuc[0] - sonuc[1]) <= 1.5);
        }
}

// BULGU 5 (orta): FORMANT kazanç eğrisinde hiç ZAMAN yumuşatması yoktu → gürültü malzemesinde 1/3 oktav bant
// kazancı çerçeveden çerçeveye oynuyordu (kepstral zarf tahmini üstel dağılımlı periodogramdan geliyor).
// ÖNCE (48k, beyaz gürültü, +3 st): 500 Hz 0.85 | 1k 1.54 | 2k 1.58 | 4k 1.25 | 8k 0.93 | 14k 0.74 dB std.
// SONRA: 0.12–0.39 dB. Sabit eğim işleyicisi TABANI 0.00–0.01 dB (ölçüm yöntemi temiz).
// NOT: STFT her N örnekte (48k'da 21.3 ms) BİR bağımsız spektrum üretir; tek kutuplu yumuşatma bu yüzden ancak
// tau >> N/sr ile kazanç sağlar (20 ms → 0.75 dB, 110 ms → 0.29 dB ölçüldü). Bu yüzden zaman sabiti ÖTÜMLÜLÜĞE
// bağlandı: ötümlüde 25 ms (ünlü geçişleri izlensin, orada dalgalanma zaten düşük), ötümsüzde 120 ms.
SWX_TEST (denetim_5_formant_zaman_kararliligi)
{
    const double fs = 48000.0;
    const int n = (int) fs * 4, L = 1023, a = (int) fs, b = n;
    const auto x = beyaz (n, 4242, 0.4);
    const double BANTLAR[6] = { 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 14000.0 };
    {   // TABAN: aynı motor + ZAMANLA DEĞİŞMEYEN eğim
        SpektralMotor mo;
        mo.prepare (fs, 1);
        mo.setQuality (Quality::Yuksek);
        EgimIsleyici eg;
        auto y = x;
        for (int i = 0; i < n; i += 256)
        {
            const int m = std::min (256, n - i);
            const float* in[1] = { y.data() + i };
            float* out[1] = { y.data() + i };
            mo.isle (in, out, 1, m, &eg);
        }
        for (double fc : BANTLAR)
        {
            const double s = bantKazancStd (y, x, L, fs, a, b, fc);
            if (ayrintili()) std::printf ("   TABAN %5.0f Hz: %.2f dB std\n", fc, s);
            SWX_BEKLE (s <= 0.10);
        }
    }
    for (float st : { 3.0f, -3.0f })
    {
        auto m = formantKur (fs, st, 1.0f, 1);
        auto y = x;
        calistir1 (m, y);
        for (double fc : BANTLAR)
        {
            const double s = bantKazancStd (y, x, L, fs, a, b, fc);
            if (ayrintili()) std::printf ("   FORMANT %+.0f st %5.0f Hz: %.2f dB std\n", (double) st, fc, s);
            SWX_BEKLE (s <= 0.50);
        }
    }
}

// BULGU 8 (orta): SpektralMotor kendi vektörlerine HAM İŞARETÇİ saklıyordu (pencere/olaBirim) → örtük kopyada
// sarkan işaretçi, sessizce yanlış ses. ÖNCE: kopya, kaynak yok edilip yığın zehirlendikten sonra referanstan
// 2468.42 sapıyordu (depth 0'da çıkış gecikmiş girişe BİREBİR eşit olmalı). SONRA: 0.0000.
SWX_TEST (denetim_8_kopyalanan_modul_guvenli)
{
    const double fs = 48000.0;
    const int n = 16384;
    const auto x = beyaz (n, 5, 0.5);
    std::vector<float> ref;
    {
        auto m = yumusatKur (fs, 0.0f, 1);
        ref = x;
        calistir1 (m, ref);
    }
    for (int tur = 0; tur < 2; ++tur)
    {
        auto* kaynak = new YumusatModule();
        kaynak->prepare (fs, 256, 1);
        kaynak->setQuality (Quality::Yuksek);
        YumusatParams p; p.on = true; p.depth = 0.0f;
        kaynak->setParams (p);
        kaynak->setEnabled (true, true);
        YumusatModule kopya = *kaynak;                       // örtük kopya (vektörler derin, ham işaretçi OLMAMALI)
        if (tur == 0) { delete kaynak; kaynak = nullptr; }
        else kaynak->prepare (96000.0, 256, 1);              // kaynağın tamponlarını YENİDEN TAHSİS ettir
        std::vector<std::vector<float>> zehir;               // serbest kalan blokları aynı boyda çöple doldur
        for (int i = 0; i < 4096; ++i) zehir.emplace_back (1024, 1234.5f);
        auto y = x;
        calistir1 (kopya, y);
        delete kaynak;
        double maks = 0.0;
        bool sonlu = true;
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (y[(size_t) i])) sonlu = false;
            maks = std::max (maks, (double) std::fabs (y[(size_t) i] - ref[(size_t) i]));
        }
        if (ayrintili()) std::printf ("   kopya tur %d: referanstan maks fark %.4f\n", tur, maks);
        SWX_BEKLE (sonlu);
        SWX_BEKLE_EQ (maks, 0.0);
    }
}

// BULGU 9 (düşük): prepare(sr <= 0 / NaN) — motor 48 kHz'e kelepçeliyor, modüller kelepçesiz sr ile tahsis
// ediyordu. ÖNCE: FORMANT kRe dizisi 256 iken motor N = 1024 → re[N−b] yığını bozuyordu (STATUS_HEAP_CORRUPTION);
// YUMUŞAT çökmüyordu ama latencySamples 255 diyip GERÇEKTE 1023 örnek geciktiriyordu (host raporu ve kuru/ıslak
// hizası 768 örnek yanlış). SONRA: iki modül de motorla AYNI kelepçeyi kullanıyor.
SWX_TEST (denetim_9_gecersiz_samplerate_kelepcesi)
{
    const int n = 8192, at = 1000;
    for (double kotu : { 0.0, -1.0, std::numeric_limits<double>::quiet_NaN() })
    {
        {
            YumusatModule m;
            m.prepare (kotu, 256, 2);
            m.setQuality (Quality::Yuksek);
            YumusatParams p; p.on = true; p.depth = 0.0f;
            m.setParams (p);
            m.setEnabled (true, true);
            std::vector<float> l ((size_t) n, 0.0f), r ((size_t) n, 0.0f);
            l[(size_t) at] = 1.0f;
            for (int i = 0; i < n; i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; m.process (ch, 2, 256); }
            int tepe = -1;
            float pk = 0.0f;
            for (int i = 0; i < n; ++i)
                if (std::fabs (l[(size_t) i]) > pk) { pk = std::fabs (l[(size_t) i]); tepe = i; }
            if (ayrintili())
                std::printf ("   YUMUSAT prepare(%.1f): latencySamples %d, olculen %d\n",
                             kotu, m.latencySamples (Quality::Yuksek), tepe - at);
            SWX_BEKLE_EQ (m.latencySamples (Quality::Yuksek), SpektralMotor::boyutSec (48000.0) - 1);
            SWX_BEKLE_EQ (tepe - at, m.latencySamples (Quality::Yuksek));
        }
        {
            FormantModule m;                                  // ÖNCE: burada yığın bozulup süreç çöküyordu
            m.prepare (kotu, 256, 2);
            m.setQuality (Quality::Yuksek);
            FormantParams p; p.on = true; p.shiftSt = 3.0f;
            m.setParams (p);
            m.setEnabled (true, true);
            auto l = beyaz (n, 9, 0.5), r = beyaz (n, 10, 0.5);
            for (int i = 0; i < n; i += 256) { float* ch[2] = { l.data() + i, r.data() + i }; m.process (ch, 2, 256); }
            bool sonlu = true;
            for (int i = 0; i < n; ++i)
                if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) sonlu = false;
            SWX_BEKLE (sonlu);
            SWX_BEKLE_EQ (m.latencySamples (Quality::Yuksek), SpektralMotor::boyutSec (48000.0) - 1);
        }
    }
}

// EK ÖLÇÜM (denetim sırasında ortaya çıktı, bulgu listesinde YOKTU): CANLI modda formant kaydırma ZAYIF.
// Kusur değil, PENCERE BOYUTU ödünü — ama entegrasyon kararını etkilediği için sayı testle sabitleniyor:
// 48k, +3 st, hedef ×1.189 → F0 85: YÜKSEK ×1.178 / CANLI ×1.053 | 100: ×1.236 / ×1.043 | 120: ×1.200 / ×1.056.
// Kesim tavanı N/4 → 0.35·N denendi, CANLI ×1.041 → ×1.043 (tavan bağlayıcı değil) → geri alındı.
// Test, CANLI'nın en azından DOĞRU YÖNDE hareket ettiğini ve YÜKSEK'in belirgin şekilde daha güçlü olduğunu
// sabitler; CANLI bir gün YÜKSEK'e yaklaşırsa bu test düşer ve başlıktaki sayılar güncellenmelidir.
SWX_TEST (denetim_ek_canli_kaydirma_zayif)
{
    const double fs = 48000.0;
    const int n = (int) fs * 3, a = (int) fs, b = n;
    for (double f0Ham : { 85.0, 100.0, 120.0 })
    {
        const int per = (int) std::lround (fs / f0Ham);
        const double f0 = fs / per;
        const auto x = unlu (fs, n, f0Ham);
        double oran[2];
        int qi = 0;
        for (Quality q : { Quality::Yuksek, Quality::Canli })
        {
            const int L = SpektralMotor::boyutSec (fs, q) - 1;
            const double F1g = formantTepesi (x, fs, f0, a - L, b - L, 700.0 / 1.25, 700.0 * 1.25);
            auto m = formantKur (fs, 3.0f, 1.0f, 1, q);
            auto y = x;
            calistir1 (m, y);
            oran[qi++] = formantTepesi (y, fs, f0, a, b, 700.0 * 1.189 / 1.3, 700.0 * 1.189 * 1.3) / F1g;
        }
        if (ayrintili())
            std::printf ("   F0 %6.1f +3 st: YUKSEK x%.3f | CANLI x%.3f\n", f0, oran[0], oran[1]);
        SWX_BEKLE (oran[1] > 1.0);                   // CANLI yine de DOĞRU yönde
        SWX_BEKLE (oran[1] < 1.13);                  // ama belirgin şekilde zayıf (başlıktaki sayılar geçerli)
        SWX_BEKLE (oran[0] > oran[1] + 0.08);        // YÜKSEK açıkça daha güçlü
    }
}
