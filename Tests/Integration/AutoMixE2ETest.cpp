// SWEAX NEXT â€” Tests/Integration/AutoMixE2ETest.cpp
// M1 Ã§ekirdek dÃ¶ngÃ¼sÃ¼ uÃ§tan uca: sorunlu sentetik vokal â†’ DÄ°NLE (processBlock yakalar) â†’ BÄ°TÄ°R â†’ rapor â†’
// plan â†’ yayÄ±n (MixReady) â†’ zincir iÅŸlemi Ã–LÃ‡ÃœLEBÄ°LÄ°R dÃ¼zeltme yapar (400 Hz bÃ¶lgesi 2 kHz'e gÃ¶re inceldi,
// S-sesi bandÄ± azaldÄ±) ve temiz sahnede YENÄ°DEN ANALÄ°Z modÃ¼lleri kapatÄ±r ("kanÄ±t yoksa iÅŸlem yok").
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "PluginProcessor.h"
#include "Analysis/Report.h"
#include "Decision/PlanPublisher.h"
#include "Core/Biquad.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <random>
#include <cstdio>
#include <cstdlib>

namespace
{
    using namespace sweax;
    constexpr double PI = 3.14159265358979323846;

    // "kirli" vokal: harmonik ton (F0 140/165/200) + 400 Hz +9 dB tÃ¼msek + her cÃ¼mlede sibilant patlamasÄ± + oda
    std::vector<float> kirliSahne (double fs, bool kirli, unsigned tohum = 5)
    {
        std::vector<float> x; std::mt19937 rng (tohum); std::normal_distribution<float> nd (0.0f, 0.001f);
        double faz = 0.0;
        Biquad mud; mud.setCoeffs (design::bellMatched (fs, 400.0, kirli ? 9.0 : 0.0, 0.8));
        Biquad h1, h2, l1, l2;
        h1.setCoeffs (design::highpass (fs, 5000.0, 0.7071)); h2.setCoeffs (design::highpass (fs, 5000.0, 0.7071));
        l1.setCoeffs (design::lowpass (fs, 9000.0, 0.7071)); l2.setCoeffs (design::lowpass (fs, 9000.0, 0.7071));
        auto oda = [&] (double sn) { for (size_t i = 0; i < (size_t) (sn * fs); ++i) x.push_back (nd (rng)); };
        auto ton = [&] (double sn, double f0, float g)
        {
            const size_t N = (size_t) (sn * fs), fade = (size_t) (0.02 * fs);
            for (size_t i = 0; i < N; ++i)
            {
                const double fNow = f0 * std::pow (2.0, 0.1 * std::sin (2.0 * PI * 0.8 * (double) x.size() / fs));
                faz += 2.0 * PI * fNow / fs; double s = 0.0;
                for (int h = 1; h <= 20; ++h) { const double fh = fNow * h; if (fh > fs * 0.45) break; const double fmt = 1.0 + 1.5 * std::exp (-std::pow ((fh - 700.0) / 300.0, 2.0)); s += fmt * std::sin (h * faz) / h; }
                const float env = (float) std::min (1.0, std::min ((double) i / fade, (double) (N - 1 - i) / fade));
                x.push_back (mud.processSample (g * (float) s * 0.22f * env) + nd (rng));
            }
        };
        auto sib = [&] (double sn) { for (size_t i = 0; i < (size_t) (sn * fs); ++i) x.push_back (0.12f * l2.processSample (l1.processSample (h2.processSample (h1.processSample (nd (rng) * 1000.0f)))) + nd (rng)); };
        for (int k = 0; k < 3; ++k)
        {
            oda (0.5); ton (1.1, 140.0, 1.0f); if (kirli) sib (0.09);
            ton (0.9, 200.0, 0.85f); oda (0.4); ton (1.0, 165.0, 0.9f); if (kirli) sib (0.08); oda (0.5);
        }
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

    double bantOrani (SweaxProcessor& p, double fs)
    {
        // 400 Hz ve 2 kHz eÅŸit genlikli sinÃ¼sleri ayrÄ± ayrÄ± iÅŸle: Ã§Ä±kÄ±ÅŸ RMS oranÄ± (dB) â€” EQ kesimi oranÄ± dÃ¼ÅŸÃ¼rÃ¼r.
        // M2: genlik comp eÅŸiÄŸinin ALTINDA (âˆ’40 dB) â€” her zaman aÃ§Ä±k kompresÃ¶r ayrÄ±-geÃ§iÅŸ Ã¶lÃ§Ã¼mÃ¼nde EQ farkÄ±nÄ±
        // geri sÄ±kÄ±ÅŸtÄ±rÄ±r (GR seviye baÄŸÄ±mlÄ±); dÃ¼ÅŸÃ¼k seviyede zincir lineer bÃ¶lgede kalÄ±r ve EQ farkÄ± Ã¶lÃ§Ã¼lÃ¼r.
        auto olc = [&] (double hz)
        {
            juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
            const int n = (int) fs;
            double e = 0.0; int cnt = 0;
            for (int i = 0; i < n; i += 512)
            {
                const int len = std::min (512, n - i);
                for (int k = 0; k < len; ++k) { const float v = 0.01f * (float) std::sin (2.0 * PI * hz * (double) (i + k) / fs); b.setSample (0, k, v); b.setSample (1, k, v); }
                juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), 2, len);
                p.processBlock (view, m);
                if (i > n / 2) for (int k = 0; k < len; ++k) { const double v = view.getSample (0, k); e += v * v; ++cnt; }
            }
            return 10.0 * std::log10 (e / std::max (1, cnt) + 1e-20);
        };
        return olc (400.0) - olc (2000.0);
    }
}

SWX_TEST (automix_uctan_uca_m1)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double fs = 48000.0;
    SweaxProcessor p;
    p.setPlayConfigDetails (2, 2, fs, 512);
    p.prepareToPlay (fs, 512);
    p.analysisEngine().stopThread();                        // testte eÅŸzamanlÄ±
    p.session().setRole (state::Role::Lead);
    p.session().setScreen (state::Screen::Ready);

    // referans: iÅŸlem yokken bant oranÄ±
    const double oranOnce = bantOrani (p, fs);

    // ---- DÄ°NLE + kirli sahne + BÄ°TÄ°R + plan ----
    p.startListening();
    auto x = kirliSahne (fs, true);
    dinlet (p, x);
    p.stopListening();                                       // rapor eÅŸzamanlÄ± Ã¼retildi (worker kapalÄ±)
    SWX_BEKLE (p.planPendingNow());
    p.buildAndPublishPlan();                                 // timer yerine doÄŸrudan (message thread)
    SWX_BEKLE (! p.planPendingNow());
    SWX_BEKLE (p.session().getScreen() == state::Screen::MixReady);
    auto& c = p.paramCache();
    SWX_BEKLE (c.flag (params::EqOn));
    SWX_BEKLE (c.flag (params::DsOn));
    // 250â€“640 aralÄ±ÄŸÄ±nda bir kesim bandÄ± var, kazanÃ§ â‰¤ 0 ve â‰¥ âˆ’6
    bool kesimVar = false;
    for (int b = 0; b < params::EQ_BANT_SAYISI; ++b)
    {
        if (! c.flag (params::eqBant (b, 0))) continue;
        const float hz = c.raw (params::eqBant (b, 2)), g = c.raw (params::eqBant (b, 3));
        if (std::getenv ("SWX_VERBOSE")) std::printf ("   bant %d: %.0f Hz %.1f dB q=%.2f dyn=%d\n", b, hz, g, c.raw (params::eqBant (b, 4)), (int) c.flag (params::eqBant (b, 5)));
        if (hz > 250.0f && hz < 640.0f && g <= -1.0f && g >= -6.01f) kesimVar = true;
    }
    SWX_BEKLE (kesimVar);
    SWX_BEKLE (c.raw (params::DsRange) >= 2.0f && c.raw (params::DsRange) <= 12.01f);
    // undo bir adÄ±mda plana kadar dÃ¶ner
    SWX_BEKLE (p.undoManager().canUndo());

    // ---- iÅŸlem Ã¶lÃ§Ã¼lebilir: 400/2k oranÄ± en az 1.5 dB dÃ¼ÅŸtÃ¼ ----
    // M3.2: GLOW'un yukarÄ±-kompresyonu dÃ¼ÅŸÃ¼k seviyeli ayrÄ±-geÃ§iÅŸ Ã¶lÃ§Ã¼mÃ¼nÃ¼ GERI DOLDURUR (M2'de comp
    // iÃ§in yaÅŸanan maskelemenin aynÄ±sÄ±; gerÃ§ek davranÄ±ÅŸ, ama burasÄ± EQ kesimini izole Ã¶lÃ§er) â€”
    // Ã¶lÃ§Ã¼m sÃ¼resince GLOW kapatÄ±lÄ±r.
    {
        auto& cg = p.paramCache();
        cg.param (params::GlowOn)->setValueNotifyingHost (cg.toNormalized (params::GlowOn, 0.0f));
    }
    const double oranSonra = bantOrani (p, fs);
    if (std::getenv ("SWX_VERBOSE")) std::printf ("   oran once %.2f dB â†’ sonra %.2f dB\n", oranOnce, oranSonra);
    SWX_BEKLE (oranOnce - oranSonra >= 1.5);

    // ---- A/B EÅÄ°T SES: worker feedback raporu abGain'i doldurdu; ORÄ°JÄ°NAL modunda kuru yol bu kazanÃ§la Ã§alÄ±nÄ±r ----
    {
        auto rep = p.analysisEngine().latestReport();
        SWX_BEKLE (rep != nullptr && rep->finalPlan != nullptr);
        SWX_BEKLE (rep->feedbackRounds >= 1);
        const float ab = p.session().getAbGainDb();
        if (std::getenv ("SWX_VERBOSE")) std::printf ("   abGain=%.2f dB, feedback tur=%d skor %.3fâ†’%.3f\n", ab, rep->feedbackRounds, rep->score0, rep->scoreFinal);
        SWX_BEKLE (std::fabs (ab) <= 24.0f);                     // M3.4 (DSP-5): giriş trimi aralığı; kuru yolda limiter
        SWX_BEKLE (rep->scoreFinal <= rep->score0 + 1e-6f);
        // AbMode=Original + Match: 1 kHz sinÃ¼s Ã§Ä±kÄ±ÅŸÄ± = giriÅŸ Ã— abGain (gecikmiÅŸ), iÅŸlenmemiÅŸ
        auto& c2 = p.paramCache();
        c2.param (params::AbMode)->setValueNotifyingHost (c2.toNormalized (params::AbMode, 1.0f));
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
        const int n = (int) fs;
        double e = 0.0; int cnt = 0;
        for (int i = 0; i < n; i += 512)
        {
            const int len = std::min (512, n - i);
            for (int k = 0; k < len; ++k) { const float v = 0.1f * (float) std::sin (2.0 * PI * 1000.0 * (double) (i + k) / fs); b.setSample (0, k, v); b.setSample (1, k, v); }
            juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), 2, len);
            p.processBlock (view, m);
            if (i > n / 2) for (int k = 0; k < len; ++k) { const double v = view.getSample (0, k); e += v * v; ++cnt; }
        }
        const double outDb = 10.0 * std::log10 (e / std::max (1, cnt));
        const double inDb = 20.0 * std::log10 (0.1) - 3.01;
        SWX_BEKLE_YAKIN (outDb - inDb, (double) ab, 0.4);
        c2.param (params::AbMode)->setValueNotifyingHost (c2.toNormalized (params::AbMode, 0.0f));
    }

    // ---- YENÄ°DEN ANALÄ°Z temiz sahneyle: modÃ¼ller kapanÄ±r (kanÄ±t yoksa iÅŸlem yok) ----
    p.startListening();
    auto temiz = kirliSahne (fs, false, 9);
    dinlet (p, temiz);
    p.stopListening();
    p.buildAndPublishPlan();
    SWX_BEKLE (c.raw (params::DsRange) < 0.05f);             // sibilans kanıtı yok → birinci aşama 0 dB
    SWX_BEKLE (c.flag (params::Ds2On));                     // son aşama sigortası her zaman açık (M3.5)
    SWX_BEKLE ((int) p.session().getPlanId() >= 2);
    p.releaseResources();
}
