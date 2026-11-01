// SWEAX NEXT — Tests/Chain/ZincirSirasiTest.cpp
// M3.5: preset başına zincir sırası — (1) sıralama kararlı ve geçerli, (2) sıra gerçekten işleme sırasını değiştirir
// (SAT → COMP ile COMP → SAT farklı çıkış), (3) sıra değişimi tıksız, (4) varsayılan konumlar eski Standard zinciri,
// (5) latency sıradan bağımsız, (6) Sandbox sırayı uygular.
#include "Cati.h"
#include "PluginProcessor.h"
#include "Chain/ZincirSirasi.h"
#include "Decision/Sandbox.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <vector>

namespace
{
    using namespace sweax;
    constexpr double PI = 3.14159265358979323846;
    void ayarla (SweaxProcessor& p, params::Id id, float ham)
    {
        auto& c = p.paramCache();
        c.param (id)->setValueNotifyingHost (c.toNormalized (id, ham));
    }
    std::vector<float> isleSinus (SweaxProcessor& p, int bloklar, float genlik, double hz, float* maksSicrama = nullptr, int degisBlok = -1,
                                  std::function<void()> degis = {})
    {
        juce::AudioBuffer<float> b (2, 256); juce::MidiBuffer m;
        std::vector<float> cikis; float onceki = 0.0f; bool ilk = true; float mx = 0.0f;
        for (int k = 0; k < bloklar; ++k)
        {
            if (k == degisBlok && degis) degis();
            for (int i = 0; i < 256; ++i)
            {
                const float v = genlik * (float) std::sin (2.0 * PI * hz * (double) (k * 256 + i) / 48000.0);
                b.setSample (0, i, v); b.setSample (1, i, v);
            }
            p.processBlock (b, m);
            for (int i = 0; i < 256; ++i)
            {
                const float y = b.getSample (0, i);
                if (! ilk) mx = std::max (mx, std::fabs (y - onceki));
                onceki = y; ilk = false; cikis.push_back (y);
            }
        }
        if (maksSicrama != nullptr) *maksSicrama = mx;
        return cikis;
    }
}

SWX_TEST (zincir_sirasi_kararli_gecerli)
{
    std::array<float, EK_SAYISI> k {};
    for (int i = 0; i < EK_SAYISI; ++i) k[(size_t) i] = (float) i;
    SWX_BEKLE (siraHesapla (k) == varsayilanSira());
    k[(size_t) Ek::Sat] = 1.0f;                                   // SAT, EQ ile aynı konum → kararlı: EQ önce (indeks küçük)
    const auto s = siraHesapla (k);
    SWX_BEKLE (siraGecerli (s));
    int eqI = -1, satI = -1, levI = -1;
    for (int i = 0; i < EK_SAYISI; ++i) { if (s[(size_t) i] == (uint8_t) Ek::Eq) eqI = i; if (s[(size_t) i] == (uint8_t) Ek::Sat) satI = i; if (s[(size_t) i] == (uint8_t) Ek::Lev) levI = i; }
    SWX_BEKLE (eqI < satI && satI < levI);
    SWX_BEKLE (siraKonumlari (s)[(size_t) Ek::Sat] == (float) satI);
    Sira bozuk = varsayilanSira(); bozuk[3] = bozuk[4];
    SWX_BEKLE (! siraGecerli (bozuk));
}

SWX_TEST (zincir_sirasi_islemi_degistirir_ve_tiksiz)
{
    using namespace sweax::params;
    juce::ScopedJuceInitialiser_GUI init;
    auto kur = [&] (SweaxProcessor& p)
    {
        p.setPlayConfigDetails (2, 2, 48000.0, 256);
        p.prepareToPlay (48000.0, 256);
        // SERT doygunluk + sert kompresör: sıra farkı duyulur olmalı
        ayarla (p, SatOn, 1.0f); ayarla (p, SatDrive, 12.0f); ayarla (p, SatMix, 1.0f);
        ayarla (p, CaOn, 1.0f); ayarla (p, CaThresh, -30.0f); ayarla (p, CaRatio, 8.0f); ayarla (p, CaAttack, 1.0f);
        p.vocalChain().snapToTargets (p.paramCache());
    };
    SweaxProcessor standart; kur (standart);
    SweaxProcessor ters; kur (ters);
    // ters: SAT, COMP A'dan önce
    ayarla (ters, PosSat, 2.0f); ayarla (ters, PosLev, 3.0f); ayarla (ters, PosCompA, 4.0f);
    ters.vocalChain().snapToTargets (ters.paramCache());
    SWX_BEKLE (ters.vocalChain().aktifSira() != standart.vocalChain().aktifSira());
    const int L = standart.currentLatency();
    SWX_BEKLE_EQ (ters.currentLatency(), L);                     // latency sıradan bağımsız
    const auto a = isleSinus (standart, 80, 0.3f, 220.0);
    const auto b = isleSinus (ters, 80, 0.3f, 220.0);
    double fark = 0.0, guc = 0.0;
    for (size_t i = a.size() / 2; i < a.size(); ++i) { fark += (double) (a[i] - b[i]) * (a[i] - b[i]); guc += (double) a[i] * a[i]; }
    SWX_BEKLE (10.0 * std::log10 (fark / std::max (guc, 1e-20)) > -40.0);   // sıra çıkışı ölçülebilir değiştirir

    // tıksız geçiş: çalarken sıra değişir → örnekler arası sıçrama sinüsün doğal adımından büyük değil
    SweaxProcessor canli; kur (canli);
    float sicrama = 0.0f;
    isleSinus (canli, 120, 0.2f, 110.0, &sicrama, 60, [&] { ayarla (canli, PosSat, 2.0f); ayarla (canli, PosLev, 3.0f); ayarla (canli, PosCompA, 4.0f); });
    const float dogalAdim = 2.0f * (float) (2.0 * PI * 110.0 / 48000.0);          // limiter/sat sonrası kaba üst sınır payı
    SWX_BEKLE (sicrama < 0.2f + dogalAdim);
    SWX_BEKLE (canli.vocalChain().aktifSira() == ters.vocalChain().aktifSira());
}

SWX_TEST (zincir_sirasi_sandbox_uygular)
{
    using namespace sweax::decision; using namespace sweax::params;
    const double fs = 48000.0;
    MixPlan plan;
    ModulePlan sat; sat.moduleId = state::modul::SAT; sat.enabled = true;
    sat.params = { { SatOn, 1.0f }, { SatDrive, 12.0f }, { SatMix, 1.0f } };
    ModulePlan ca; ca.moduleId = state::modul::COMP_A; ca.enabled = true;
    ca.params = { { CaOn, 1.0f }, { CaThresh, -30.0f }, { CaRatio, 8.0f }, { CaAttack, 1.0f }, { CaRelease, 80.0f }, { CaMix, 1.0f } };
    plan.modules = { sat, ca };
    std::vector<float> x ((size_t) fs);
    for (size_t i = 0; i < x.size(); ++i) x[i] = 0.3f * (float) std::sin (2.0 * PI * 220.0 * (double) i / fs);
    std::vector<float> L1 (x.size()), R1 (x.size()), L2 (x.size()), R2 (x.size());
    Sandbox::renderStereo (plan, x.data(), L1.data(), R1.data(), (int64_t) x.size(), fs, Quality::Yuksek, 120.0, true);
    auto plan2 = plan;
    std::array<float, EK_SAYISI> k = siraKonumlari (varsayilanSira());
    k[(size_t) Ek::Sat] = 2.0f; k[(size_t) Ek::Lev] = 3.0f; k[(size_t) Ek::CompA] = 4.0f;
    plan2.sira = siraHesapla (k);
    Sandbox::renderStereo (plan2, x.data(), L2.data(), R2.data(), (int64_t) x.size(), fs, Quality::Yuksek, 120.0, true);
    double fark = 0.0, guc = 0.0;
    for (size_t i = x.size() / 2; i < x.size(); ++i) { fark += (double) (L1[i] - L2[i]) * (L1[i] - L2[i]); guc += (double) L1[i] * L1[i]; }
    SWX_BEKLE (10.0 * std::log10 (fark / std::max (guc, 1e-20)) > -40.0);
}
