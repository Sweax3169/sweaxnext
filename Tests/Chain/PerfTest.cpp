// SWEAX NEXT — Tests/Chain/PerfTest.cpp
// RT bütçe ölçümü: TAM zincir (tüm modüller açık, tune dahil) 48 kHz/256 blokta gerçek zamanın
// en fazla %35'ini yiyebilir (Release; tek örnek plugin için güvenli pay). YIN/oversample maliyeti burada yakalanır.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>

SWX_TEST (zincir_rt_butcesi)
{
    using namespace sweax;
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 256);
    proc.prepareToPlay (48000.0, 256);
    proc.analysisEngine().stopThread();
    auto ac = [&] (params::Id id, float v)
    {
        auto* p = proc.paramCache().param (id);
        p->setValueNotifyingHost (proc.paramCache().toNormalized (id, v));
    };
    // her şey açık (en kötü durum)
    for (auto id : { params::GateOn, params::TuneOn, params::EqOn, params::EqHpfOn, params::EqB1On, params::EqB2On,
                     params::EqB3On, params::LevOn, params::CaOn, params::CbOn, params::DsOn, params::SatOn,
                     params::DblOn, params::DlyOn, params::RevOn, params::WidOn })
        ac (id, 1.0f);
    ac (params::TuneStrength, 1.0f);

    const int blok = 256;
    juce::AudioBuffer<float> b (2, blok);
    juce::MidiBuffer m;
    // ısıtma + sinyal: 220 Hz'e yakın falsolu ton (tune aktif çalışsın)
    double faz = 0.0;
    auto doldur = [&]
    {
        for (int i = 0; i < blok; ++i)
        {
            faz += 2.0 * 3.14159265358979 * 225.0 / 48000.0;
            const float v = 0.2f * (float) std::sin (faz);
            b.setSample (0, i, v); b.setSample (1, i, v);
        }
    };
    for (int k = 0; k < 200; ++k) { doldur(); proc.processBlock (b, m); }   // ısıtma ~1 sn
    const int N = 2000;                                                     // ~10.7 sn ses
    const auto t0 = std::chrono::steady_clock::now();
    for (int k = 0; k < N; ++k) { doldur(); proc.processBlock (b, m); }
    const auto t1 = std::chrono::steady_clock::now();
    const double islemSn = std::chrono::duration<double> (t1 - t0).count();
    const double sesSn = (double) N * blok / 48000.0;
    const double oran = islemSn / sesSn;
    std::printf ("  [perf] %.2f sn ses %.3f sn islendi -> gercek zamanin %%%.1f'i\n", sesSn, islemSn, 100.0 * oran);
    // Paylaşımlı CI sunucularında (GitHub Actions CI=true) ölçüm gürültülü ve makine yavaş: orada sınır gerçek zamandır.
    const double sinir = std::getenv ("CI") != nullptr ? 1.0 : 0.35;
    SWX_BEKLE (oran < sinir);
    proc.releaseResources();
}
