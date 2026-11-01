// SWEAX NEXT — Tests/Analysis/PitchTest.cpp
// Ölçülen doğruluk: harmonik "vokal" sinyali (F0 + 12 harmonik, formant benzeri zarf, SNR 30 dB)
// 80–1000 Hz'de ±3 cent; oktav hatası < %2; sessizlik/gürültü → voiced değil; vibrato izlenir.
#include "Cati.h"
#include "Analysis/Pitch.h"
#include <vector>
#include <random>

namespace
{
    // formant benzeri harmonik ağırlıklar: 1/h ile azalan + 700 Hz ve 1800 Hz civarı tümsekler
    void vokal (std::vector<float>& v, double fs, double f0, double snrDb, unsigned tohum = 1, double vibratoCent = 0.0, double vibratoHz = 5.5)
    {
        std::mt19937 rng (tohum);
        double faz[16] = {}; double enerji = 0.0;
        for (size_t i = 0; i < v.size(); ++i)
        {
            const double t = (double) i / fs;
            const double f = f0 * std::pow (2.0, vibratoCent / 1200.0 * std::sin (2.0 * 3.14159265358979 * vibratoHz * t));
            double s = 0.0;
            for (int h = 1; h <= 12; ++h)
            {
                const double fh = f * h; if (fh > fs * 0.45) break;
                const double formant = 1.0 + 2.0 * std::exp (-std::pow ((fh - 700.0) / 250.0, 2.0)) + 1.0 * std::exp (-std::pow ((fh - 1800.0) / 300.0, 2.0));
                faz[h] += 2.0 * 3.14159265358979 * fh / fs;
                s += (1.0 / h) * formant * std::sin (faz[h]);
            }
            v[i] = (float) (0.3 * s / 3.0); enerji += v[i] * v[i];
        }
        // gürültü SİNYALE GÖRE (SNR dB): std = rms · 10^(−snr/20)
        const double rms = std::sqrt (enerji / (double) v.size());
        std::normal_distribution<double> nd (0.0, rms * std::pow (10.0, -snrDb / 20.0));
        for (auto& x : v) x = (float) (x + nd (rng));
    }
    struct Olcum { double maxCent = 0, ortCent = 0; int voiced = 0, toplam = 0, oktav = 0; };
    Olcum olc (double fs, double f0, double snrDb, double vibratoCent = 0.0)
    {
        sweax::PitchTracker p; p.prepare (fs, 60.0, 1000.0);
        std::vector<float> x ((size_t) (fs * 1.0));
        vokal (x, fs, f0, snrDb, 7, vibratoCent);
        Olcum o; double top = 0.0;
        for (int i = (int) (fs * 0.05); i + p.frameSize() <= (int) x.size(); i += p.hop())
        {
            const auto r = p.analyze (x.data() + i, p.frameSize());
            ++o.toplam;
            if (! r.voiced) continue;
            ++o.voiced;
            // vibrato: pencere ortasındaki anlık f
            const double tm = (double) (i + p.frameSize() / 2) / fs;
            const double fRef = f0 * std::pow (2.0, vibratoCent / 1200.0 * std::sin (2.0 * 3.14159265358979 * 5.5 * tm));
            const double c = std::fabs (sweax::PitchTracker::cents (r.hz, fRef));
            if (c > 600.0) { ++o.oktav; continue; }
            o.maxCent = std::max (o.maxCent, c); top += c;
        }
        o.ortCent = o.voiced > 0 ? top / o.voiced : 0.0;
        return o;
    }
}

SWX_TEST (pitch_harmonik_vokal_3_cent)
{
    for (double fs : { 44100.0, 48000.0 })
        for (double f0 : { 80.0, 110.0, 165.0, 247.0, 440.0, 700.0, 1000.0 })
        {
            const auto o = olc (fs, f0, 30.0);
            SWX_BEKLE (o.voiced > o.toplam * 0.9);
            SWX_BEKLE (o.maxCent <= 3.0);
            SWX_BEKLE (o.oktav == 0);
        }
}

SWX_TEST (pitch_gurultu_ve_vibrato)
{
    // daha gürültülü (SNR 15 dB): ortalama hata küçük, oktav hatası < %2
    for (double f0 : { 95.0, 200.0, 330.0 })
    {
        const auto o = olc (48000.0, f0, 15.0);
        SWX_BEKLE (o.voiced > o.toplam * 0.8);
        SWX_BEKLE (o.ortCent <= 3.0);
        SWX_BEKLE ((double) o.oktav / std::max (1, o.voiced) < 0.02);
    }
    // vibrato ±50 cent @5.5 Hz: izleme hatası (pencere ortalaması) makul, oktav yok
    const auto v = olc (48000.0, 220.0, 30.0, 50.0);
    SWX_BEKLE (v.voiced > v.toplam * 0.9);
    SWX_BEKLE (v.ortCent <= 8.0);
    SWX_BEKLE_EQ (v.oktav, 0);
    // sessizlik ve beyaz gürültü → voiced değil
    sweax::PitchTracker p; p.prepare (48000.0);
    std::vector<float> z ((size_t) p.frameSize(), 0.0f);
    SWX_BEKLE (! p.analyze (z.data(), (int) z.size()).voiced);
    std::mt19937 rng (3); std::normal_distribution<float> nd (0.0f, 0.1f);
    int voicedSay = 0; for (int i = 0; i < 20; ++i) { for (auto& s : z) s = nd (rng); if (p.analyze (z.data(), (int) z.size()).voiced) ++voicedSay; }
    SWX_BEKLE (voicedSay <= 1);
}
