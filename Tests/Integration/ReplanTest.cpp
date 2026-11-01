// SWEAX NEXT — Tests/Integration/ReplanTest.cpp
// M3.1 anında yeniden plan (sahip: "sanatçı chain'ini seç › hemen duy"): MixReady'de stil/rol değişimi
// yeniden DİNLEMEDEN, dondurulmuş yakalamadan yeni bağlamla plan kurar. Ölçülen: Temiz › Killoki geçişi
// tune hızını 25 ms › 3 ms ve delay ducking'i 8 › 10 dB yapar; Lead › Backing geçişi WIDTH'i açar;
// yakalama yokken replan dürüstçe reddedilir; ekran hep MixReady kalır.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "Decision/Receteler.h"
#include "PluginProcessor.h"
#include <vector>
#include <random>
#include <cmath>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    using namespace sweax;

    std::vector<float> sahne (double fs)
    {
        std::vector<float> x; std::mt19937 rng (11); std::normal_distribution<float> nd (0.0f, 0.001f);
        double faz = 0.0;
        auto oda = [&] (double sn) { for (size_t i = 0; i < (size_t) (sn * fs); ++i) x.push_back (nd (rng)); };
        auto ton = [&] (double sn, double f0) { for (size_t i = 0; i < (size_t) (sn * fs); ++i) { faz += 2.0 * PI * f0 / fs; float v = 0.0f; for (int h = 1; h <= 8; ++h) v += (float) (std::sin (h * faz) / h); x.push_back (0.1f * v + nd (rng)); } };
        for (int k = 0; k < 3; ++k) { oda (0.4); ton (1.2, 145.0); oda (0.3); ton (1.0, 190.0); }
        oda (0.4);
        return x;
    }
    void dinlet (SweaxProcessor& p, const std::vector<float>& x, int blok = 512)
    {
        juce::AudioBuffer<float> b (2, blok); juce::MidiBuffer m;
        for (int i = 0; i < (int) x.size(); i += blok)
        {
            const int n = std::min (blok, (int) x.size() - i);
            for (int k = 0; k < n; ++k) { b.setSample (0, k, x[(size_t) (i + k)]); b.setSample (1, k, x[(size_t) (i + k)]); }
            juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), 2, n);
            p.processBlock (view, m);
            if ((i / blok) % 16 == 0) p.analysisEngine().processPendingSync();
        }
        p.analysisEngine().processPendingSync();
    }
}

SWX_TEST (replan_stil_ve_rol_aninda)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double fs = 48000.0;
    SweaxProcessor p;
    p.setPlayConfigDetails (2, 2, fs, 512);
    p.prepareToPlay (fs, 512);
    p.analysisEngine().stopThread();                        // testte eşzamanlı
    p.session().setRole (state::Role::Lead);
    p.session().setScreen (state::Screen::Ready);

    // yakalama yokken replan reddedilir
    SWX_BEKLE (! p.replanFromCapture());

    // ---- DİNLE + BİTİR + plan (Temiz varsayılan stil) ----
    p.startListening();
    dinlet (p, sahne (fs));
    p.stopListening();
    p.analysisEngine().processPendingSync();
    SWX_BEKLE (p.planPendingNow());
    p.buildAndPublishPlan();
    SWX_BEKLE (p.session().getScreen() == state::Screen::MixReady);
    auto& c = p.paramCache();
    const uint32_t plan1 = p.session().getPlanId();
    SWX_BEKLE_YAKIN (c.raw (params::TuneSpeed), 25.0f, 1.1f);      // Temiz: 25 ms
    SWX_BEKLE_YAKIN (c.raw (params::DlyDuck), 8.0f, 0.11f);
    SWX_BEKLE (! c.flag (params::WidOn));                          // Lead: mono bütünlüğü

    // ---- STİL: Killoki › anında yeni plan (dinleme YOK, ekran düşmez) ----
    p.session().setStyle ("Killoki");
    SWX_BEKLE (p.replanFromCapture());
    p.analysisEngine().processPendingSync();
    SWX_BEKLE (p.planPendingNow());
    p.buildAndPublishPlan();
    SWX_BEKLE (p.session().getScreen() == state::Screen::MixReady);
    SWX_BEKLE (p.session().getPlanId() != plan1);
    // M3.5: beklenenler REÇETEDEN (Killoki = stil 4) — tablo güncellenince test kendini uyarlar
    SWX_BEKLE_YAKIN (c.raw (params::TuneSpeed), decision::recete (4).tuneSpd, 0.9f);
    SWX_BEKLE_YAKIN (c.raw (params::SatDrive), decision::recete (4).satDrive, 1.1f);
    SWX_BEKLE (c.raw (params::CaRatio) >= decision::recete (4).ratioA - 0.06f);

    // ---- ROL: Backing › anında yeni plan; WIDTH açılır ----
    p.session().setRole (state::Role::Backing);
    SWX_BEKLE (p.replanFromCapture());
    p.analysisEngine().processPendingSync();
    p.buildAndPublishPlan();
    SWX_BEKLE (p.session().getScreen() == state::Screen::MixReady);
    SWX_BEKLE (c.flag (params::WidOn));                            // Backing: geniş
    p.releaseResources();
}
