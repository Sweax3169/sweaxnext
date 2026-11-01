// SWEAX NEXT — Tests/DSP/BiquadTest.cpp
// Filtre tasarımları ÖLÇÜLEREK doğrulanır: impuls → filtre → FFT → |H(f)| vs analitik/hedef.
#include "Cati.h"
#include "Core/Biquad.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace
{
    // impuls yanıtından ölçülen büyüklük (dB) — analitik magnitudeAt'ı bağımsız doğrular
    struct OlculenYanit
    {
        std::vector<float> mag; double fs; int N;
        double dbAt (double hz) const
        {
            const double bin = hz / fs * N;
            const int i = (int) std::lround (bin);
            return 20.0 * std::log10 (std::max ((double) mag[(size_t) std::clamp (i, 0, N / 2)], 1e-12));
        }
    };
    OlculenYanit olc (const sweax::BiquadCoeffs& c, double fs, int N = 1 << 15)
    {
        sweax::Biquad f; f.setCoeffs (c);
        std::vector<float> x ((size_t) N * 2, 0.0f);
        x[0] = 1.0f;
        f.process (x.data(), N);
        juce::dsp::FFT fft ((int) std::log2 (N));
        fft.performFrequencyOnlyForwardTransform (x.data());
        OlculenYanit r; r.fs = fs; r.N = N; r.mag.assign (x.begin(), x.begin() + N / 2 + 1);
        return r;
    }
}

SWX_TEST (biquad_bell_rbj_tepe_ve_kenar)
{
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        auto c = sweax::design::bellRbj (fs, 1000.0, 6.0, 1.4);
        auto r = olc (c, fs);
        SWX_BEKLE_YAKIN (r.dbAt (1000.0), 6.0, 0.15);
        SWX_BEKLE_YAKIN (c.magnitudeDbAt (1000.0, fs), 6.0, 0.02); // analitik
        SWX_BEKLE_YAKIN (r.dbAt (20.0), 0.0, 0.1);
        // RBJ Q tanımı: bant kenarlarında kazancın yarısı (3 dB)
        const double bw = 1000.0 / 1.4;
        const double fl = std::sqrt (1000.0 * 1000.0 + bw * bw / 4.0) - bw / 2.0;
        SWX_BEKLE_YAKIN (c.magnitudeDbAt (fl, fs), 3.0, 0.3);
    }
}

SWX_TEST (biquad_bell_matched_nyquist_yakini)
{
    // 12 kHz +6 dB @ 44.1k: RBJ Nyquist'te 0 dB'ye çöker; matched analog gibi yüksek kalır.
    const double fs = 44100.0;
    auto rbj = sweax::design::bellRbj (fs, 12000.0, 6.0, 1.0);
    auto mt  = sweax::design::bellMatched (fs, 12000.0, 6.0, 1.0);
    SWX_BEKLE_YAKIN (mt.magnitudeDbAt (12000.0, fs), 6.0, 0.25);   // tepe korunur
    SWX_BEKLE_YAKIN (mt.magnitudeDbAt (100.0, fs), 0.0, 0.05);      // DC etkilenmez
    const double rbjNy = rbj.magnitudeDbAt (22000.0, fs), mtNy = mt.magnitudeDbAt (22000.0, fs);
    SWX_BEKLE (rbjNy < 0.2);          // RBJ Nyquist'te sıfıra döner
    SWX_BEKLE (mtNy > 2.0);           // matched analogun Nyquist kazancına yaklaşır
    // düşük frekansta iki tasarım aynı olmalı (Nyquist'ten uzakta fark yok)
    auto rbjL = sweax::design::bellRbj (fs, 300.0, -4.0, 2.0), mtL = sweax::design::bellMatched (fs, 300.0, -4.0, 2.0);
    for (double f : { 100.0, 250.0, 300.0, 400.0, 1000.0 })
        SWX_BEKLE_YAKIN (rbjL.magnitudeDbAt (f, fs), mtL.magnitudeDbAt (f, fs), 0.1);
    // kararlılık: kutuplar birim çember içinde
    SWX_BEKLE (std::fabs (mt.a2) < 1.0 && std::fabs (mt.a1) < 1.0 + mt.a2);
}

SWX_TEST (biquad_shelf_hpf_lpf_notch)
{
    const double fs = 48000.0;
    auto hs = sweax::design::highShelf (fs, 8000.0, 4.0);
    SWX_BEKLE_YAKIN (hs.magnitudeDbAt (20000.0, fs), 4.0, 0.3);
    SWX_BEKLE_YAKIN (hs.magnitudeDbAt (100.0, fs), 0.0, 0.05);
    SWX_BEKLE_YAKIN (hs.magnitudeDbAt (8000.0, fs), 2.0, 0.3);       // orta kazanç köşede
    auto ls = sweax::design::lowShelf (fs, 120.0, -3.0);
    SWX_BEKLE_YAKIN (ls.magnitudeDbAt (20.0, fs), -3.0, 0.2);
    SWX_BEKLE_YAKIN (ls.magnitudeDbAt (5000.0, fs), 0.0, 0.05);
    auto hp = sweax::design::highpass (fs, 100.0, 0.7071);
    SWX_BEKLE_YAKIN (hp.magnitudeDbAt (100.0, fs), -3.0, 0.15);
    SWX_BEKLE (hp.magnitudeDbAt (25.0, fs) < -22.0);                 // 12 dB/okt: 2 oktav ≈ -24
    auto lp = sweax::design::lowpass (fs, 5000.0, 0.7071);
    SWX_BEKLE_YAKIN (lp.magnitudeDbAt (5000.0, fs), -3.0, 0.15);
    auto nt = sweax::design::notch (fs, 3000.0, 8.0);
    SWX_BEKLE (nt.magnitudeDbAt (3000.0, fs) < -40.0);
    SWX_BEKLE_YAKIN (nt.magnitudeDbAt (300.0, fs), 0.0, 0.05);
    auto ap = sweax::design::allpass (fs, 1000.0, 0.7);
    for (double f : { 50.0, 1000.0, 15000.0 }) SWX_BEKLE_YAKIN (ap.magnitudeDbAt (f, fs), 0.0, 0.01);
}

SWX_TEST (biquad_klemp_ve_guvenlik)
{
    const double fs = 44100.0;
    auto c = sweax::design::bellMatched (fs, 30000.0, 40.0, 0.01); // Nyquist üstü + aşırı değerler
    SWX_BEKLE (std::isfinite (c.b0) && std::isfinite (c.a1) && std::isfinite (c.a2));
    SWX_BEKLE (std::fabs (c.a2) < 1.0);
    sweax::Biquad f; f.setCoeffs (c);
    float x[512]; for (int i = 0; i < 512; ++i) x[i] = (i % 2 ? 1.0f : -1.0f);
    f.process (x, 512);
    bool sonlu = true; for (float v : x) sonlu = sonlu && std::isfinite (v);
    SWX_BEKLE (sonlu);
    auto id = sweax::design::bellRbj (fs, 1000.0, 0.0, 1.0);
    SWX_BEKLE_YAKIN (id.b0, 1.0, 1e-12); SWX_BEKLE_YAKIN (id.a1, 0.0, 1e-12);
}
