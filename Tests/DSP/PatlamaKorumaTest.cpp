// SWEAX NEXT — Tests/DSP/PatlamaKorumaTest.cpp
// M3.4 denetim T1 (sahip: "harfler patlamasın"): patlama koruması p/b patlamasının 80-200 Hz gövdesini ≥ 6 dB
// bastırır; sesli heceye (F0 150 Hz, zengin harmonik) dokunmaz (≤ 1 dB); kapalıyken birebir.
#include "Cati.h"
#include "DSP/CorrectiveEQ.h"
#include "Chain/VocalChain.h"
#include "Core/Biquad.h"
#include <vector>
#include <random>
#include <cmath>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    using namespace sweax;

    struct Sahne { std::vector<float> x; std::vector<std::pair<int, int>> patlama, sesli; };

    Sahne sahne (double fs)
    {
        Sahne s; std::mt19937 rng (3); std::normal_distribution<float> nd (0.0f, 1.0f);
        // patlama: 80 Hz altı gövde (4. derece LP) + 80-200 Hz kuyruk (4. derece bant); orta bant neredeyse boş
        Biquad lp1, lp2, bh1, bh2, bl1, bl2;
        lp1.setCoeffs (design::lowpass (fs, 80.0, 0.7071)); lp2.setCoeffs (design::lowpass (fs, 80.0, 0.7071));
        bh1.setCoeffs (design::highpass (fs, 80.0, 0.7071)); bh2.setCoeffs (design::highpass (fs, 80.0, 0.7071));
        bl1.setCoeffs (design::lowpass (fs, 200.0, 0.7071)); bl2.setCoeffs (design::lowpass (fs, 200.0, 0.7071));
        double faz = 0.0;
        for (int k = 0; k < 6; ++k)
        {
            for (int i = 0; i < (int) (0.3 * fs); ++i) s.x.push_back (1.0e-5f * nd (rng));
            int a = (int) s.x.size();
            for (int i = 0; i < (int) (0.05 * fs); ++i)
            {
                const float z = (float) std::exp (-(double) i / (0.012 * fs));
                const float govde = lp2.processSample (lp1.processSample (nd (rng)));
                const float kuyruk = bl2.processSample (bl1.processSample (bh2.processSample (bh1.processSample (nd (rng)))));
                s.x.push_back (z * (4.0f * govde + 1.2f * kuyruk));
            }
            s.patlama.push_back ({ a, (int) s.x.size() });
            a = (int) s.x.size();
            for (int i = 0; i < (int) (0.5 * fs); ++i)
            {
                faz += 2.0 * PI * 150.0 / fs;
                double v = 0.0;
                for (int h = 1; h <= 24; ++h) v += std::sin (h * faz) / h;
                s.x.push_back ((float) (0.1 * v));
            }
            s.sesli.push_back ({ a + (int) (0.1 * fs), (int) s.x.size() });
        }
        return s;
    }
    std::vector<float> bant (const std::vector<float>& v, double fs, double lo, double hi)
    {
        Biquad h1, h2, l1, l2;
        h1.setCoeffs (design::highpass (fs, lo, 0.7071)); h2.setCoeffs (design::highpass (fs, lo, 0.7071));
        l1.setCoeffs (design::lowpass (fs, hi, 0.7071)); l2.setCoeffs (design::lowpass (fs, hi, 0.7071));
        std::vector<float> o (v.size());
        for (size_t i = 0; i < v.size(); ++i) o[i] = l2.processSample (l1.processSample (h2.processSample (h1.processSample (v[i]))));
        return o;
    }
    double enerjiDb (const std::vector<float>& v, int a, int b)
    {
        double e = 0.0; for (int i = a; i < b; ++i) e += (double) v[(size_t) i] * v[(size_t) i];
        return 10.0 * std::log10 (e / std::max (1, b - a) + 1e-24);
    }
}

SWX_TEST (eq_patlama_korumasi_patlamayi_keser_sesliye_dokunmaz)
{
    const double fs = 48000.0;
    const auto s = sahne (fs);
    auto isle = [&] (bool acik)
    {
        CorrectiveEQ eq; eq.prepare (fs, 512, 1);
        EqParams p; p.on = true; p.plosOn = acik; p.plosHz = 180.0f; p.plosRangeDb = -12.0f;
        eq.setParams (p); eq.setEnabled (true, true);
        auto y = s.x;
        float enDerin = 0.0f;
        for (size_t i = 0; i < y.size(); i += 64)
        {
            float* ch[1] = { y.data() + i };
            eq.process (ch, 1, (int) std::min<size_t> (64, y.size() - i));
            enDerin = std::min (enDerin, eq.plosiveGainReductionDb());
        }
        if (std::getenv ("SWX_VERBOSE")) std::printf ("   koruma %s: en derin GR %.1f dB\n", acik ? "acik" : "kapali", enDerin);
        return y;
    };
    const auto kapali = isle (false), acik = isle (true);
    const int D = (int) std::lround (CorrectiveEQ::PLOS_LOOKAHEAD_MS * 0.001 * fs);   // YÜKSEK önden-bakış gecikmesi
    double fark = 0.0;
    for (size_t i = (size_t) D; i < kapali.size(); ++i) fark = std::max (fark, (double) std::fabs (kapali[i] - s.x[i - (size_t) D]));
    SWX_BEKLE (fark < 1.0e-6);                                         // kapalı = (sabit gecikmeli) birebir
    { CorrectiveEQ e3; e3.prepare (fs, 512, 1); SWX_BEKLE_EQ (e3.latencySamples (Quality::Yuksek), D); SWX_BEKLE_EQ (e3.latencySamples (Quality::Canli), 0); }
    const auto bK = bant (kapali, fs, 80.0, 200.0), bA = bant (acik, fs, 80.0, 200.0);
    double patlamaAzalma = 0.0, sesliDegisim = 0.0;
    for (const auto& [a, b] : s.patlama) patlamaAzalma += enerjiDb (bK, a, b) - enerjiDb (bA, a, b);
    for (const auto& [a, b] : s.sesli) sesliDegisim += std::fabs (enerjiDb (bK, a, b) - enerjiDb (bA, a, b));
    patlamaAzalma /= (double) s.patlama.size(); sesliDegisim /= (double) s.sesli.size();
    if (std::getenv ("SWX_VERBOSE")) std::printf ("   80-200 Hz: patlama -%.1f dB, sesli degisim %.2f dB\n", patlamaAzalma, sesliDegisim);
    SWX_BEKLE (patlamaAzalma >= 6.0);
    SWX_BEKLE (sesliDegisim <= 1.0);
}
