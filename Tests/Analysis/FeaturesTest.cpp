// SWEAX NEXT — Tests/Analysis/FeaturesTest.cpp
// Özellikler ölçülür: beyaz gürültü düzlük yüksek, sinüs düzlük ≈ 0 ve centroid = f; −6 dB/okt (pembe benzeri) eğim;
// HF-only gürültü hfRatio ≈ 1; akı onset'te sıçrar; 60 Hz darbe LF zarfı 1 kHz tondan çok yüksek.
#include "Cati.h"
#include "Analysis/Features.h"
#include <vector>
#include <random>

namespace
{
    struct Toplam { int kare = 0; double flat = 0, cent = 0, slope = 0, hf = 0, roll = 0, lfEnv = -200, fluxMax = 0, crest = 0; std::vector<float> flux; };
    Toplam calistir (sweax::FeatureExtractor& fe, const std::vector<float>& x, int atla = 3)
    {
        Toplam t; int i = 0;
        fe.push (x.data(), (int) x.size(), [&] (const sweax::FrameFeatures& f)
        {
            t.flux.push_back (f.flux);
            if (i++ < atla) return;
            ++t.kare; t.flat += f.flatness; t.cent += f.centroidHz; t.slope += f.slopeDbPerOct; t.hf += f.hfRatio; t.roll += f.rolloffHz;
            t.lfEnv = std::max (t.lfEnv, (double) f.lfEnvDb); t.fluxMax = std::max (t.fluxMax, (double) f.flux); t.crest += f.crestDb;
        });
        if (t.kare > 0) { t.flat /= t.kare; t.cent /= t.kare; t.slope /= t.kare; t.hf /= t.kare; t.roll /= t.kare; t.crest /= t.kare; }
        return t;
    }
}

SWX_TEST (features_gurultu_sinus_egim)
{
    const double fs = 48000.0;
    sweax::FeatureExtractor fe; fe.prepare (fs, 4096, 1024);
    SWX_BEKLE (fe.bands().numBands() > 100);
    std::mt19937 rng (5); std::normal_distribution<float> nd (0.0f, 0.1f);
    // beyaz gürültü: düzlük yüksek, eğim ≈ 0 dB/okt (mean-square bant enerjisi oktav başına 2× artar → bant dB eğimi +3 dB/okt;
    // bantlar sabit-Q olduğundan beyaz gürültüde bant enerjisi f ile orantılıdır)
    std::vector<float> w ((size_t) fs); for (auto& s : w) s = nd (rng);
    auto tw = calistir (fe, w);
    SWX_BEKLE (tw.kare > 30);
    SWX_BEKLE (tw.flat > 0.5);
    SWX_BEKLE_YAKIN (tw.slope, 3.0, 0.8);
    SWX_BEKLE (tw.crest > 8.0 && tw.crest < 16.0);
    // sinüs 1 kHz: düzlük ≈ 0, centroid ≈ 1000, rolloff ≈ 1000, hfRatio ≈ 0
    fe.reset();
    std::vector<float> s ((size_t) fs); for (size_t i = 0; i < s.size(); ++i) s[i] = 0.5f * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * (double) i / fs);
    auto ts = calistir (fe, s);
    SWX_BEKLE (ts.flat < 0.01);
    SWX_BEKLE_YAKIN (ts.cent, 1000.0, 15.0);
    SWX_BEKLE_YAKIN (ts.roll, 1000.0, 15.0);
    SWX_BEKLE (ts.hf < 1e-3);
    SWX_BEKLE_YAKIN (ts.crest, 3.01, 0.1);
    // −3 dB/okt (pembe) gürültü: bant eğimi ≈ 0; 1. derece 6 dB/okt LPF ile → ≈ −3
    fe.reset();
    std::vector<float> pembe ((size_t) fs);
    { // Voss-benzeri yerine basit: beyaz → 1/√f yaklaşımı (3 tek kutuplu paralel) — kabaca −3 dB/okt
        double b0 = 0, b1 = 0, b2 = 0;
        for (size_t i = 0; i < pembe.size(); ++i) { const double wv = nd (rng); b0 = 0.99765 * b0 + wv * 0.0990460; b1 = 0.96300 * b1 + wv * 0.2965164; b2 = 0.57000 * b2 + wv * 1.0526913; pembe[i] = (float) ((b0 + b1 + b2 + wv * 0.1848) * 0.2); }
    }
    auto tp = calistir (fe, pembe);
    SWX_BEKLE (tp.slope > -1.5 && tp.slope < 1.5);
    // HF-only gürültü (8 kHz üstü): hfRatio ≈ 1, centroid > 8 kHz
    fe.reset();
    std::vector<float> hfn ((size_t) fs);
    { sweax::Biquad h1, h2; h1.setCoeffs (sweax::design::highpass (fs, 8000.0, 0.7071)); h2.setCoeffs (sweax::design::highpass (fs, 8000.0, 0.7071));
      for (auto& v : hfn) v = h2.processSample (h1.processSample (nd (rng))); }
    auto th = calistir (fe, hfn);
    SWX_BEKLE (th.hf > 0.97);
    SWX_BEKLE (th.cent > 8000.0);
}

SWX_TEST (features_aki_ve_lf_zarf)
{
    const double fs = 48000.0;
    sweax::FeatureExtractor fe; fe.prepare (fs, 4096, 1024);
    // 0.3 s sessizlik → 1 kHz ton (onset) → akı ilk tonlu karede sıçrar, sonra ~0
    std::vector<float> x ((size_t) fs, 0.0f);
    for (size_t i = (size_t) (fs * 0.3); i < x.size(); ++i) x[i] = 0.5f * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * (double) i / fs);
    auto t = calistir (fe, x, 0);
    int onsetKare = -1; for (size_t i = 0; i < t.flux.size(); ++i) if (t.flux[i] > 0.5) { onsetKare = (int) i; break; }
    SWX_BEKLE (onsetKare > 5);                                          // sessiz başlangıç: akı yok
    SWX_BEKLE (onsetKare > 0 && t.flux[(size_t) onsetKare] > 0.5);
    double sonra = 0.0; int say = 0; for (size_t i = (size_t) onsetKare + 6; i < t.flux.size(); ++i) { sonra += t.flux[i]; ++say; }
    SWX_BEKLE (say > 5 && sonra / say < 0.05);                          // sabit ton: akı ≈ 0
    // LF zarfı: 60 Hz darbe (50 ms) vs 1 kHz ton — 120 Hz LPF zarfı darbede ≥ 25 dB daha yüksek
    fe.reset();
    std::vector<float> ton ((size_t) (fs * 0.5)), darbe ((size_t) (fs * 0.5), 0.0f);
    for (size_t i = 0; i < ton.size(); ++i) ton[i] = 0.5f * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * (double) i / fs);
    for (size_t i = (size_t) (fs * 0.2); i < (size_t) (fs * 0.25); ++i) darbe[i] = 0.5f * (float) std::sin (2.0 * 3.14159265358979 * 60.0 * (double) i / fs);
    auto tt = calistir (fe, ton, 0); fe.reset(); auto td = calistir (fe, darbe, 0);
    SWX_BEKLE (td.lfEnv - tt.lfEnv > 25.0);
    SWX_BEKLE_YAKIN (td.lfEnv, -6.0, 3.0);                              // 0.5 tepe → −6 dB civarı
}
