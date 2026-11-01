// SWEAX NEXT — Tests/Integration/BuyukBlokTest.cpp
// Denetim regresyonu: bazi hostlar prepare'daki blok boyutunu asar. Eskiden kuru yol/A-B karisimi yalniz ilk
// WORST_BLOCK ornegi isliyordu -> hizasiz kuru + kirpik karisim. Simdi tum yol parcalanir.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

SWX_TEST (buyuk_blok_bypass_hizasi)
{
    using namespace sweax;
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    proc.analysisEngine().stopThread();
    // bypass AC: cikis = latency kadar gecikmis giris (tum blok boyunca)
    {
        auto* p = proc.paramCache().param (params::Bypass);
        p->setValueNotifyingHost (1.0f);
    }
    const int n = 20000;                                   // WORST_BLOCK(8192)'nin cok ustunde
    juce::AudioBuffer<float> b (2, n);
    juce::MidiBuffer m;
    const double PI = 3.14159265358979323846;
    // isitma (crossfade otursun + gecikme hatti dolsun)
    for (int r = 0; r < 3; ++r)
    {
        for (int i = 0; i < n; ++i)
        {
            const float v = 0.25f * (float) std::sin (2.0 * PI * 220.0 * (double) (r * n + i) / 48000.0);
            b.setSample (0, i, v); b.setSample (1, i, v);
        }
        proc.processBlock (b, m);
    }
    // olcum turu
    for (int i = 0; i < n; ++i)
    {
        const float v = 0.25f * (float) std::sin (2.0 * PI * 220.0 * (double) (3 * n + i) / 48000.0);
        b.setSample (0, i, v); b.setSample (1, i, v);
    }
    proc.processBlock (b, m);
    const int lat = proc.currentLatency();
    // SON parca dahil her bolgede cikis, latency kadar gecikmis girisle birebir olmali
    double maxFark = 0.0;
    for (int i = lat + 100; i < n; ++i)
    {
        const float beklenen = 0.25f * (float) std::sin (2.0 * PI * 220.0 * (double) (3 * n + i - lat) / 48000.0);
        maxFark = std::max (maxFark, (double) std::fabs (b.getSample (0, i) - beklenen));
    }
    if (std::getenv ("SWX_VERBOSE")) std::printf ("   buyukBlok maxFark=%.6f (lat=%d)\n", maxFark, lat);
    SWX_BEKLE (maxFark < 1.0e-3);
    proc.releaseResources();
}
