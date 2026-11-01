// SWEAX NEXT — Tests/Decision/SandboxHizaTest.cpp
// Denetim dsp-rt-1: gecikme telafili Sandbox render'ı girişe ÖRNEK DOĞRULUĞUNDA hizalı (limiter kalite moduna
// geçirilmiyordu → YÜKSEK'te çıkış girişten 12 örnek öndeydi). Tüm modüller kapalı plan + bant sınırlı gürültü.
#include "Cati.h"
#include "Decision/Sandbox.h"
#include "Core/Biquad.h"
#include <vector>
#include <random>
#include <cmath>

namespace
{
    int enIyiGecikme (const std::vector<float>& ref, const std::vector<float>& y)
    {
        int enIyi = 0; double enIyiK = -1.0e30;
        const int n = (int) ref.size();
        for (int lag = -64; lag <= 64; ++lag)
        {
            double k = 0.0;
            for (int i = 2000; i < n - 2000; ++i) k += (double) ref[(size_t) i] * y[(size_t) (i + lag)];
            if (k > enIyiK) { enIyiK = k; enIyi = lag; }
        }
        return enIyi;
    }
}

SWX_TEST (sandbox_gecikme_telafisi_ornek_dogru)
{
    using namespace sweax; using namespace sweax::decision;
    const double fs = 48000.0;
    std::mt19937 rng (21); std::normal_distribution<float> nd (0.0f, 1.0f);
    Biquad l1, l2; l1.setCoeffs (design::lowpass (fs, 6000.0, 0.7071)); l2.setCoeffs (design::lowpass (fs, 6000.0, 0.7071));
    std::vector<float> x ((size_t) fs);
    for (auto& v : x) v = 0.05f * l2.processSample (l1.processSample (nd (rng)));
    const MixPlan bos;                                                      // tüm modüller kapalı, limiter açık
    for (auto q : { Quality::Yuksek, Quality::Canli })
    {
        std::vector<float> L (x.size()), R (x.size()), M (x.size());
        Sandbox::renderStereo (bos, x.data(), L.data(), R.data(), (int64_t) x.size(), fs, q, 120.0, true);
        SWX_BEKLE_EQ (enIyiGecikme (x, L), 0);
        Sandbox::renderStereo (bos, x.data(), L.data(), R.data(), (int64_t) x.size(), fs, q, 120.0, true, 1);
        SWX_BEKLE_EQ (enIyiGecikme (x, L), 0);
        Sandbox::render (bos, x.data(), M.data(), (int64_t) x.size(), fs, q, true);
        SWX_BEKLE_EQ (enIyiGecikme (x, M), 0);
    }
}
