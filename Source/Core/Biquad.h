#pragma once
// SWEAX NEXT — Core/Biquad.h
// Çift hassasiyetli (double) biquad tasarımı + TDF-II filtre.
//
// TASARIMLAR
//  • RBJ Audio EQ Cookbook: lowpass, highpass, bandpass, notch, allpass, lowShelf, highShelf, bellRbj
//  • bellMatched: Orfanidis (1997) "Digital parametric equalizer design with prescribed
//    Nyquist-frequency gain" — yüksek frekanslı bell'lerin (ör. 12 kHz @ 44.1k) analog şekle uyması.
//    RBJ bell Nyquist'e yaklaşırken sıkışıp yamulur; matched tasarım bunu düzeltir.
//    CorrectiveEQ ve Tone modülleri varsayılan olarak bunu kullanır.
//
// GÜVENLİK
//  • Frekans [10 Hz, 0.49·fs] aralığına klemplenir; Q [0.05, 40]; kazanç [-40, +40] dB.
//  • Katsayılar double; işlem double durum, float giriş/çıkış (denormal riski düşük, gürültü tabanı iyi).
//  • Katsayı DEĞİŞİMİ burada yumuşatılmaz: modül düzeyinde çift-filtre crossfade (bkz. DSP/CorrectiveEQ).
#include <cmath>
#include <algorithm>

namespace sweax
{

struct BiquadCoeffs
{
    double b0 { 1.0 }, b1 { 0.0 }, b2 { 0.0 }, a1 { 0.0 }, a2 { 0.0 }; // a0 = 1'e normalize

    static BiquadCoeffs identity() noexcept { return {}; }

    // |H(e^{jw})| — test ve UI eğrisi için (gerçek zamanlı yolda kullanılmaz)
    double magnitudeAt (double freqHz, double sampleRate) const noexcept
    {
        const double w = 2.0 * 3.14159265358979323846 * freqHz / sampleRate;
        const double c1 = std::cos (w), s1 = std::sin (w);
        const double c2 = std::cos (2.0 * w), s2 = std::sin (2.0 * w);
        const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
        const double dr = 1.0 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        const double num = nr * nr + ni * ni, den = dr * dr + di * di;
        return std::sqrt (num / std::max (den, 1.0e-30));
    }
    double magnitudeDbAt (double freqHz, double sampleRate) const noexcept
    {
        return 20.0 * std::log10 (std::max (magnitudeAt (freqHz, sampleRate), 1.0e-12));
    }
};

namespace design
{
    constexpr double kPi = 3.14159265358979323846;

    inline double clampFreq (double f, double fs) noexcept { return std::clamp (f, 10.0, 0.49 * fs); }
    inline double clampQ (double q) noexcept { return std::clamp (q, 0.05, 40.0); }
    inline double clampDb (double g) noexcept { return std::clamp (g, -40.0, 40.0); }

    inline BiquadCoeffs normalize (double b0, double b1, double b2, double a0, double a1, double a2) noexcept
    {
        BiquadCoeffs c;
        const double inv = 1.0 / a0;
        c.b0 = b0 * inv; c.b1 = b1 * inv; c.b2 = b2 * inv; c.a1 = a1 * inv; c.a2 = a2 * inv;
        return c;
    }

    inline BiquadCoeffs lowpass (double fs, double f, double q) noexcept
    {
        f = clampFreq (f, fs); q = clampQ (q);
        const double w0 = 2.0 * kPi * f / fs, cs = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        return normalize ((1 - cs) * 0.5, 1 - cs, (1 - cs) * 0.5, 1 + al, -2 * cs, 1 - al);
    }
    inline BiquadCoeffs highpass (double fs, double f, double q) noexcept
    {
        f = clampFreq (f, fs); q = clampQ (q);
        const double w0 = 2.0 * kPi * f / fs, cs = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        return normalize ((1 + cs) * 0.5, -(1 + cs), (1 + cs) * 0.5, 1 + al, -2 * cs, 1 - al);
    }
    // sabit tepe kazancı 0 dB olan bant geçiren (sidechain dedektörleri için)
    inline BiquadCoeffs bandpass (double fs, double f, double q) noexcept
    {
        f = clampFreq (f, fs); q = clampQ (q);
        const double w0 = 2.0 * kPi * f / fs, cs = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        return normalize (al, 0.0, -al, 1 + al, -2 * cs, 1 - al);
    }
    inline BiquadCoeffs notch (double fs, double f, double q) noexcept
    {
        f = clampFreq (f, fs); q = clampQ (q);
        const double w0 = 2.0 * kPi * f / fs, cs = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        return normalize (1.0, -2 * cs, 1.0, 1 + al, -2 * cs, 1 - al);
    }
    inline BiquadCoeffs allpass (double fs, double f, double q) noexcept
    {
        f = clampFreq (f, fs); q = clampQ (q);
        const double w0 = 2.0 * kPi * f / fs, cs = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        return normalize (1 - al, -2 * cs, 1 + al, 1 + al, -2 * cs, 1 - al);
    }
    inline BiquadCoeffs lowShelf (double fs, double f, double gainDb, double q = 0.7071) noexcept
    {
        f = clampFreq (f, fs); q = clampQ (q); gainDb = clampDb (gainDb);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * f / fs, cs = std::cos (w0), sn = std::sin (w0);
        const double al = sn / (2.0 * q), sqA2 = 2.0 * std::sqrt (A) * al;
        return normalize (A * ((A + 1) - (A - 1) * cs + sqA2), 2 * A * ((A - 1) - (A + 1) * cs),
                          A * ((A + 1) - (A - 1) * cs - sqA2), (A + 1) + (A - 1) * cs + sqA2,
                          -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - sqA2);
    }
    inline BiquadCoeffs highShelf (double fs, double f, double gainDb, double q = 0.7071) noexcept
    {
        f = clampFreq (f, fs); q = clampQ (q); gainDb = clampDb (gainDb);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * f / fs, cs = std::cos (w0), sn = std::sin (w0);
        const double al = sn / (2.0 * q), sqA2 = 2.0 * std::sqrt (A) * al;
        return normalize (A * ((A + 1) + (A - 1) * cs + sqA2), -2 * A * ((A - 1) + (A + 1) * cs),
                          A * ((A + 1) + (A - 1) * cs - sqA2), (A + 1) - (A - 1) * cs + sqA2,
                          2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - sqA2);
    }
    // RBJ peaking EQ: Q, kazancın yarısında (sqrt(G)) ölçülen bant genişliğine göre
    inline BiquadCoeffs bellRbj (double fs, double f, double gainDb, double q) noexcept
    {
        f = clampFreq (f, fs); q = clampQ (q); gainDb = clampDb (gainDb);
        if (std::fabs (gainDb) < 1.0e-4) return BiquadCoeffs::identity();
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * f / fs, cs = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        return normalize (1 + al * A, -2 * cs, 1 - al * A, 1 + al / A, -2 * cs, 1 - al / A);
    }

    // Orfanidis (JAES 45(6), 1997) eşleşmeli bell — Appendix B `peq.m` ile BİREBİR doğrulandı.
    // Bant genişliği RBJ ile aynı tanımdadır (Δω = ω0/Q, kazancın geometrik ortasında GB = √G),
    // böylece Q anlamı korunur; fark yalnız Nyquist yakınındaki şekildedir.
    // İLK SÜRÜM TUZAĞI (testle yakalandı): W² içine fazladan F00/F11 çarpanı girmişti → tepe
    // büyüklüğü doğru ama merkez 12 kHz'den 15.4 kHz'e kayıyordu. Doğru: W² = √(G11/G00)·tan²(ω0/2).
    inline BiquadCoeffs bellMatched (double fs, double f, double gainDb, double q) noexcept
    {
        f = clampFreq (f, fs); q = clampQ (q); gainDb = clampDb (gainDb);
        if (std::fabs (gainDb) < 1.0e-4) return BiquadCoeffs::identity();

        const double G0 = 1.0;                         // DC kazancı
        const double G  = std::pow (10.0, gainDb / 20.0);
        const double GB = std::sqrt (G0 * G);          // bant kenarı kazancı (geometrik orta)
        const double w0 = 2.0 * kPi * f / fs;
        const double dw = w0 / q;                      // bant genişliği (rad)

        const double G2 = G * G, GB2 = GB * GB, G02 = G0 * G0;
        const double F   = std::fabs (G2 - GB2);
        const double G00 = std::fabs (G2 - G02);
        const double F00 = std::fabs (GB2 - G02);
        if (F < 1.0e-12 || G00 < 1.0e-12)
            return bellRbj (fs, f, gainDb, q);         // dejenere (çok küçük kazanç) → RBJ yeterli

        // Nyquist'te analog prototipin kazancı: A_analog² = F00·Δω²/F (GB geometrik orta olduğu için)
        const double pw = (w0 * w0 - kPi * kPi) * (w0 * w0 - kPi * kPi);
        const double num1 = G02 * pw + G2 * F00 * kPi * kPi * dw * dw / F;
        const double den1 = pw + F00 * kPi * kPi * dw * dw / F;
        const double G1 = std::sqrt (std::max (num1 / std::max (den1, 1.0e-30), 1.0e-12));

        const double G12 = G1 * G1;
        const double G01 = std::fabs (G2 - G0 * G1);
        const double G11 = std::fabs (G2 - G12);
        const double F01 = std::fabs (GB2 - G0 * G1);
        const double F11 = std::fabs (GB2 - G12);
        if (F11 < 1.0e-12) return bellRbj (fs, f, gainDb, q);

        const double t0 = std::tan (w0 * 0.5);
        const double W2 = std::sqrt (G11 / G00) * t0 * t0;
        const double DW = (1.0 + std::sqrt (F00 / F11) * W2) * std::tan (dw * 0.5);
        const double C  = F11 * DW * DW - 2.0 * W2 * (F01 - std::sqrt (F00 * F11));
        const double D  = 2.0 * W2 * (G01 - std::sqrt (G00 * G11));
        const double A  = std::sqrt (std::max ((C + D) / F, 0.0));
        const double B  = std::sqrt (std::max ((G2 * C + GB2 * D) / F, 0.0));
        const double den = 1.0 + W2 + A;

        BiquadCoeffs c;
        c.b0 = (G1 + G0 * W2 + B) / den;
        c.b1 = -2.0 * (G1 - G0 * W2) / den;
        c.b2 = (G1 - B + G0 * W2) / den;
        c.a1 = -2.0 * (1.0 - W2) / den;
        c.a2 = (1.0 + W2 - A) / den;
        if (! (std::isfinite (c.b0) && std::isfinite (c.a1) && std::isfinite (c.a2)) || std::fabs (c.a2) >= 1.0)
            return bellRbj (fs, f, gainDb, q);
        return c;
    }
} // namespace design

// Transposed Direct Form II, double durum. Tek kanal; çok kanal için dizi kullanılır.
class Biquad
{
public:
    void setCoeffs (const BiquadCoeffs& c) noexcept { k = c; }
    const BiquadCoeffs& coeffs() const noexcept { return k; }
    void reset() noexcept { s1 = s2 = 0.0; }

    inline float processSample (float x) noexcept
    {
        const double in = (double) x;
        const double y = k.b0 * in + s1;
        s1 = k.b1 * in - k.a1 * y + s2;
        s2 = k.b2 * in - k.a2 * y;
        return (float) y;
    }
    void process (float* data, int n) noexcept
    {
        for (int i = 0; i < n; ++i) data[i] = processSample (data[i]);
    }
    // denormal/patlama koruması: durum sonlu değilse sıfırla
    void sanitize() noexcept
    {
        if (! (std::isfinite (s1) && std::isfinite (s2))) reset();
    }

private:
    BiquadCoeffs k;
    double s1 { 0.0 }, s2 { 0.0 };
};

} // namespace sweax
