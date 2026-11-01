// SWEAX NEXT — Tests/Analysis/StftTest.cpp
#include "Cati.h"
#include "Analysis/Stft.h"
#include <vector>

SWX_TEST (stft_parseval_ve_tepe)
{
    const double fs = 48000.0;
    sweax::Stft st; st.prepare (fs, 4096, 1024, 1);
    // bin merkezinde sinüs (k=100 → 1171.875 Hz), genlik 0.5 → mean-square 0.125
    const double hz = 100.0 * st.binHz();
    std::vector<float> x ((size_t) fs);
    for (size_t i = 0; i < x.size(); ++i) x[i] = 0.5f * (float) std::sin (2.0 * 3.14159265358979 * hz * (double) i / fs);
    int kare = 0; double toplam = 0.0, tepe = 0.0; int tepeBin = -1;
    st.push (x.data(), (int) x.size(), [&] (const float* p, int bins, long) {
        ++kare; toplam = 0.0; tepe = 0.0; tepeBin = -1;
        for (int k = 0; k < bins; ++k) { toplam += p[k]; if (p[k] > tepe) { tepe = p[k]; tepeBin = k; } }
    });
    SWX_BEKLE (kare > 40);
    SWX_BEKLE_EQ (tepeBin, 100);
    SWX_BEKLE_YAKIN (toplam, 0.125, 0.125 * 0.02);          // Parseval ±%2
    // Hann ana lob: tepe bin + iki komşu ≈ tüm enerji
    SWX_BEKLE (tepe > 0.125 * 0.3 && tepe < 0.125 * 0.8);
    // sıfır doldurma 4×: bin sayısı 4 kat, Parseval korunur
    sweax::Stft z; z.prepare (fs, 4096, 1024, 4);
    SWX_BEKLE_EQ (z.fftSize(), 16384);
    double toplamZ = 0.0;
    z.push (x.data(), (int) x.size(), [&] (const float* p, int bins, long) { toplamZ = 0.0; for (int k = 0; k < bins; ++k) toplamZ += p[k]; });
    SWX_BEKLE_YAKIN (toplamZ, 0.125, 0.125 * 0.03);
    // süre → boyut
    SWX_BEKLE_EQ (sweax::Stft::sizeForDuration (0.085, 48000.0), 4096);
    SWX_BEKLE_EQ (sweax::Stft::sizeForDuration (0.021, 48000.0), 1024);
    SWX_BEKLE_EQ (sweax::Stft::sizeForDuration (0.085, 96000.0), 8192);
}

SWX_TEST (logbands_eslestirme)
{
    const double fs = 48000.0;
    sweax::LogBands lb; lb.prepare (fs, 4096, 12, 20.0, 0.45);
    SWX_BEKLE (lb.numBands() > 100);                 // 20 Hz → 21.6 kHz ≈ 10 oktav × 12
    SWX_BEKLE_YAKIN (lb.lowHz (0), 20.0, 1e-9);
    SWX_BEKLE (lb.highHz (lb.numBands() - 1) <= fs * 0.45 + 1e-6);
    const int b1k = lb.bandForHz (1000.0);
    SWX_BEKLE (lb.lowHz (b1k) <= 1000.0 && 1000.0 < lb.highHz (b1k));
    SWX_BEKLE_YAKIN (lb.highHz (b1k) / lb.lowHz (b1k), std::pow (2.0, 1.0 / 12.0), 1e-9);
    // bant enerjisi: 1 kHz sinüs → 1 kHz bandı (ve komşuları) enerjiyi taşır, uzak bantlar ~0
    sweax::Stft st; st.prepare (fs, 4096, 1024, 1);
    std::vector<float> x ((size_t) fs);
    for (size_t i = 0; i < x.size(); ++i) x[i] = 0.5f * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * (double) i / fs);
    std::vector<float> bant ((size_t) lb.numBands());
    st.push (x.data(), (int) x.size(), [&] (const float* p, int, long) { lb.apply (p, bant.data()); });
    double yakin = 0.0, uzak = 0.0;
    for (int b = 0; b < lb.numBands(); ++b) { if (std::abs (b - b1k) <= 2) yakin += bant[(size_t) b]; else uzak += bant[(size_t) b]; }
    SWX_BEKLE (yakin > 0.125 * 0.95);
    SWX_BEKLE (uzak < 0.125 * 0.05);
}
