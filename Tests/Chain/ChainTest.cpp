// SWEAX NEXT — Tests/Chain/ChainTest.cpp
// Zincir + processor bütünleşmesi: latency yalnız (sr, mod) fonksiyonu (enable/düzen etkilemez); tüm modüller
// kapalıyken çıkış = gecikmiş giriş; EQ bandı açılınca ölçülen kazanç; mod değişimi çalışırken güvenli;
// parametre değişimleriyle RT tahsis yok; NaN girişte çıkış sonlu.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "PluginProcessor.h"
#include "Core/RtGuard.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <cmath>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    void hazirla (sweax::SweaxProcessor& p, double sr = 48000.0, int blok = 256)
    {
        p.setPlayConfigDetails (2, 2, sr, blok);
        p.prepareToPlay (sr, blok);
        p.analysisEngine().stopThread();
    }
    void ayarla (sweax::SweaxProcessor& p, sweax::params::Id id, float ham)
    {
        auto& c = p.paramCache();
        c.param (id)->setValueNotifyingHost (c.toNormalized (id, ham));
    }
    // sinüsü işle, çıkış (L) döner
    std::vector<float> isle (sweax::SweaxProcessor& p, double fs, double hz, float genlik, double sn, int blok = 256)
    {
        const int n = (int) (fs * sn);
        juce::AudioBuffer<float> b (2, blok); juce::MidiBuffer m;
        std::vector<float> out ((size_t) n);
        for (int i = 0; i < n; i += blok)
        {
            const int len = std::min (blok, n - i);
            for (int k = 0; k < len; ++k) { const float v = genlik * (float) std::sin (2.0 * PI * hz * (double) (i + k) / fs); b.setSample (0, k, v); b.setSample (1, k, v); }
            b.setSize (2, blok, true, false, true);
            juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), 2, len);
            p.processBlock (view, m);
            for (int k = 0; k < len; ++k) out[(size_t) (i + k)] = view.getSample (0, k);
        }
        return out;
    }
    double rmsDb (const std::vector<float>& v, int a, int b) { double s = 0.0; for (int i = a; i < b; ++i) s += (double) v[(size_t) i] * v[(size_t) i]; return 10.0 * std::log10 (s / (b - a) + 1e-20); }
}

SWX_TEST (chain_latency_yalniz_mod_fonksiyonu)
{
    using namespace sweax; using namespace sweax::params;
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor p; hazirla (p);
    auto& ch = p.vocalChain();
    const int canli = ch.latencySamples (Quality::Canli), yuksek = ch.latencySamples (Quality::Yuksek), ultra = ch.latencySamples (Quality::Ultra);
    SWX_BEKLE_EQ (yuksek, ultra);
    SWX_BEKLE (canli < yuksek);
    // M3.5: YUMUŞAT + FORMANT (WOLA STFT) — 48k'da N=1024: CANLI N/2−1=511, YÜKSEK N−1=1023 (modül başına)
    SWX_BEKLE_EQ (canli, 768 + 12 + 12 + (72 - 1 + 7) + 2 * 511);       // tune(768) + sat(12) + dirt(12, M3.4) + limiter (gate CANLI'da 0) + soo+frm
    SWX_BEKLE_EQ (yuksek, 96 + 768 + 240 + 7200 + 240 + 144 + 240 + 144 + 12 + 12 + (72 + 18) + 2 * 1023);   // gate + tune + eq(patlama önden-bakış 5 ms, M3.4) + lev + compA + ds + compB + ds2 + sat + dirt + limiter(4x) + soo+frm
    // enable/düzen latency'yi değiştirmez
    ayarla (p, EqOn, 1.0f); ayarla (p, CaOn, 1.0f); ayarla (p, DsOn, 1.0f); ayarla (p, LevOn, 1.0f); ayarla (p, Layout, 2.0f);
    isle (p, 48000.0, 1000.0, 0.1f, 0.05);
    SWX_BEKLE_EQ (ch.latencySamples (Quality::Yuksek), yuksek);
    SWX_BEKLE_EQ (p.currentLatency(), yuksek);
    // mod değişimi çalışırken: latency güncellenir (audio thread) + host bildirimi timer'da
    ayarla (p, QualityMode, 0.0f);
    isle (p, 48000.0, 1000.0, 0.1f, 0.05);
    SWX_BEKLE_EQ (p.currentLatency(), canli);
    ayarla (p, QualityMode, 2.0f);
    isle (p, 48000.0, 1000.0, 0.1f, 0.05);
    SWX_BEKLE_EQ (p.currentLatency(), ultra);
}

SWX_TEST (chain_kapali_moduller_gecikmis_giris)
{
    using namespace sweax; using namespace sweax::params;
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor p; hazirla (p);
    const double fs = 48000.0;
    // hepsi kapalı (varsayılan), trim 0, limiter eşik altı → çıkış = giriş gecikmiş (latency)
    auto out = isle (p, fs, 1000.0, 0.25f, 1.0);
    const int D = p.currentLatency();
    double err = 0.0;
    for (int i = D + 2400; i < (int) out.size(); ++i)
    {
        const float ref = 0.25f * (float) std::sin (2.0 * PI * 1000.0 * (double) (i - D) / fs);
        err = std::max (err, (double) std::fabs (out[(size_t) i] - ref));
    }
    SWX_BEKLE (err < 1e-4);
    // EQ bell +6 dB @ 1 kHz açılınca ölçülen kazanç +6 ± 0.3 (crossfade ve rampa sonrası)
    ayarla (p, EqOn, 1.0f); ayarla (p, eqBant (0, 0), 1.0f); ayarla (p, eqBant (0, 2), 1000.0f); ayarla (p, eqBant (0, 3), 6.0f); ayarla (p, eqBant (0, 4), 1.0f);
    auto y = isle (p, fs, 1000.0, 0.25f, 1.0);
    SWX_BEKLE_YAKIN (rmsDb (y, (int) fs / 2, (int) fs) - 10.0 * std::log10 (0.25 * 0.25 * 0.5), 6.0, 0.3);
    // 100 Hz'de değişmez
    auto z = isle (p, fs, 100.0, 0.25f, 1.0);
    SWX_BEKLE_YAKIN (rmsDb (z, (int) fs / 2, (int) fs) - 10.0 * std::log10 (0.25 * 0.25 * 0.5), 0.0, 0.2);
    // Comp A açık, eşik −30, oran 4, peak: −12 dBFS tepe → over 18 → GR 13.5 → çıkış tepe ≈ −25.5
    ayarla (p, EqOn, 0.0f); ayarla (p, CaOn, 1.0f); ayarla (p, CaThresh, -30.0f); ayarla (p, CaRatio, 4.0f); ayarla (p, CaKnee, 0.0f); ayarla (p, CaDetector, 0.0f); ayarla (p, CaAttack, 1.0f);
    auto w = isle (p, fs, 1000.0, 0.25f, 1.0);
    float pk = 0.0f; for (int i = (int) fs / 2; i < (int) fs; ++i) pk = std::max (pk, std::fabs (w[(size_t) i]));
    SWX_BEKLE_YAKIN (20.0 * std::log10 (pk), -25.5, 0.6);
    SWX_BEKLE_YAKIN (p.vocalChain().metering().gr (2), -13.5f, 0.6f);
}

SWX_TEST (chain_delta_hizalama_kaniti)
{
    using namespace sweax; using namespace sweax::params;
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor p; hazirla (p);
    const double fs = 48000.0;
    // tüm modüller kapalı + trim 0 + DELTA: wet = gecikmiş giriş = dry → fark ≈ 0 (örnek hizalama KANITI)
    ayarla (p, Delta, 1.0f);
    auto out = isle (p, fs, 997.0, 0.25f, 1.2);
    float pk = 0.0f;
    for (int i = (int) (fs * 0.6); i < (int) out.size(); ++i) pk = std::max (pk, std::fabs (out[(size_t) i]));
    SWX_BEKLE (20.0 * std::log10 (pk + 1e-12) < -80.0);                 // fark tabanı: tam örnek hizalı
    // InputTrim +6 dB: delta = (g−1)·x → tepe ≈ 0.25·(10^(6/20)−1) ≈ 0.2489
    ayarla (p, InputTrim, 6.0f);
    auto out2 = isle (p, fs, 997.0, 0.25f, 1.2);
    float pk2 = 0.0f;
    for (int i = (int) (fs * 0.6); i < (int) out2.size(); ++i) pk2 = std::max (pk2, std::fabs (out2[(size_t) i]));
    SWX_BEKLE_YAKIN (pk2, 0.25f * (std::pow (10.0f, 6.0f / 20.0f) - 1.0f), 0.01f);
    ayarla (p, Delta, 0.0f); ayarla (p, InputTrim, 0.0f);
}

SWX_TEST (chain_rt_tahsis_yok_ve_nan_guvenli)
{
    using namespace sweax; using namespace sweax::params;
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor p; hazirla (p);
    juce::AudioBuffer<float> b (2, 256); juce::MidiBuffer m;
    for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) b.setSample (c, i, 0.3f * std::sin (0.1f * (float) i));
    // tüm modüller açık + her blokta parametre değişimi
    ayarla (p, EqOn, 1.0f); ayarla (p, eqBant (1, 0), 1.0f); ayarla (p, eqBant (1, 5), 1.0f); ayarla (p, EqHpfOn, 1.0f);
    ayarla (p, LevOn, 1.0f); ayarla (p, CaOn, 1.0f); ayarla (p, CbOn, 1.0f); ayarla (p, DsOn, 1.0f); ayarla (p, Layout, 2.0f);
    rt::resetViolations();
    for (int k = 0; k < 300; ++k)
    {
        ayarla (p, eqBant (1, 3), -6.0f + (float) (k % 12)); ayarla (p, CaThresh, -30.0f + (float) (k % 20)); ayarla (p, DsRange, (float) (k % 8));
        ayarla (p, EqHpfHz, 40.0f + (float) (k % 100)); ayarla (p, LevTarget, -24.0f + (float) (k % 6));
        if (k % 50 == 25) ayarla (p, QualityMode, (float) (k / 50 % 3));
        rt::Scope s; p.processBlock (b, m);
    }
    SWX_BEKLE_EQ (rt::violations(), 0u);
    for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) SWX_BEKLE (std::isfinite (b.getSample (c, i)));
    // NaN giriş → çıkış sonlu (blok sıfır + reset + sayaç)
    b.setSample (0, 10, std::numeric_limits<float>::quiet_NaN());
    { rt::Scope s; p.processBlock (b, m); }
    bool sonlu = true; for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) sonlu = sonlu && std::isfinite (b.getSample (c, i));
    SWX_BEKLE (sonlu);
    for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) b.setSample (c, i, 0.3f * std::sin (0.1f * (float) i));
    for (int k = 0; k < 20; ++k) { rt::Scope s; p.processBlock (b, m); }
    sonlu = true; for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) sonlu = sonlu && std::isfinite (b.getSample (c, i));
    SWX_BEKLE (sonlu);
    SWX_BEKLE_EQ (rt::violations(), 0u);
}
