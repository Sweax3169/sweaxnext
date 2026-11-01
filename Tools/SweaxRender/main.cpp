// SWEAX NEXT — Tools/SweaxRender/main.cpp
// Toplu değerlendirme aracı (mimari §9): WAV girişi → çevrimdışı analiz (aynı ölçüm hattı) → Planner + Feedback →
// Sandbox render → WAV çıkışı + JSON rapor (stdout veya dosya). Golden testler ve korpus raporları için.
// Kullanım: SweaxRender giris.wav cikis.wav [rapor.json] [--rol ana|back|adlib|master] [--stil ad|0-15]
#include "Analysis/Loudness.h"
#include "Analysis/Offline.h"
#include "Decision/Feedback.h"
#include "Decision/Sandbox.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>

using namespace sweax;
using namespace sweax::decision;

static juce::var detToVar (const ProblemDetection& d)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("tip", (int) d.type);
    o->setProperty ("p", d.probability);
    o->setProperty ("s", d.severity);
    o->setProperty ("c", d.confidence);
    o->setProperty ("hz", d.centerHz);
    o->setProperty ("deger", d.valueDb);
    o->setProperty ("adet", d.count);
    return juce::var (o);
}

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::printf ("SweaxRender giris.wav cikis.wav [rapor.json] [--rol ana|back|adlib|master] [--stil ad|0-15]\n");
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI init;
    state::Role rol = state::Role::Lead;
    int stil = 1;                                          // Temiz (varsayilan)
    for (int i = 3; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--rol" && i + 1 < argc)
        {
            const juce::String v (argv[++i]);
            rol = v == "back" ? state::Role::Backing : v == "adlib" ? state::Role::Adlib : v == "master" ? state::Role::Master : state::Role::Lead;
        }
        else if (a == "--stil" && i + 1 < argc)
        {
            const juce::String v = juce::String (argv[++i]).toLowerCase();
            static const char* adlar[16] = { "dogal", "temiz", "trap", "agresif", "killoki", "reckol", "ati242", "uzi", "motive",
                                             "kava", "ezhel", "ceza", "allame", "sehinsah", "sagopa", "benfero" };
            stil = juce::jlimit (0, 15, v.getIntValue());
            for (int k = 0; k < 16; ++k) if (v == adlar[k]) stil = k;
        }
    }
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    const juce::File inFile = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1]));
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (inFile));
    if (reader == nullptr) { std::printf ("HATA: giris okunamadi: %s\n", argv[1]); return 1; }
    const double sr = reader->sampleRate;
    const int64_t n = (int64_t) reader->lengthInSamples;
    juce::AudioBuffer<float> buf ((int) reader->numChannels, (int) n);
    reader->read (&buf, 0, (int) n, 0, true, true);
    // mono karışım (motorla aynı ruh: eşit ağırlık)
    std::vector<float> mono ((size_t) n, 0.0f);
    for (int c = 0; c < (int) reader->numChannels; ++c)
        for (int64_t i = 0; i < n; ++i) mono[(size_t) i] += buf.getSample (c, (int) i) / (float) reader->numChannels;

    const double t0 = juce::Time::getMillisecondCounterHiRes();
    auto rap = offline::reportFromBuffer (mono.data(), n, sr);
    PlannerInput pin; pin.report = &rap; pin.detections = &rap.detections; pin.role = rol; pin.planId = 1; pin.style = stil;
    pin.outChannels = 2;                                                     // stereo WAV yazılır
    auto fb = Feedback::optimize (pin, mono.data(), n, sr, 10.0, 3);
    fb.finalPlan.abGainDb = juce::jlimit (-24.0f, 24.0f, fb.processedLufs - rap.integratedLufs);
    // TAM zincir (FX dahil) stereo render — GECİKME TELAFİLİ (denetim M3.4 T6: son 185 ms kayboluyor, çıkış
    // hizasız ölçülüyordu). JSON "cikis": analiz alanları mono karışımdan; integratedLufs KANAL-TOPLAMLI stereo
    // BS.1770 (DAW ölçeri ve eklenti etiketiyle aynı sayı), midLufs ayrıca.
    std::vector<float> outL ((size_t) n), outR ((size_t) n);
    Sandbox::renderStereo (fb.finalPlan, mono.data(), outL.data(), outR.data(), n, sr, Quality::Yuksek, 120.0, true);
    std::vector<float> outMono ((size_t) n);
    for (int64_t i = 0; i < n; ++i) outMono[(size_t) i] = 0.5f * (outL[(size_t) i] + outR[(size_t) i]);
    auto rapOut = offline::reportFromBuffer (outMono.data(), n, sr);
    const float midLufs = rapOut.integratedLufs;
    {
        LoudnessMeter lm; lm.prepare (sr, 2, 8192);
        for (int64_t i = 0; i < n; i += 8192)
        {
            const float* ch2[2] = { outL.data() + i, outR.data() + i };
            lm.process (ch2, 2, (int) std::min<int64_t> (8192, n - i));
        }
        rapOut.integratedLufs = lm.integratedLufs();
    }
    const double ms = juce::Time::getMillisecondCounterHiRes() - t0;

    // WAV çıkışı
    const juce::File outFile = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[2]));
    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (new juce::FileOutputStream (outFile), sr, 2, 24, {}, 0));
    if (writer == nullptr) { std::printf ("HATA: cikis yazilamadi: %s\n", argv[2]); return 1; }
    const float* ptr[2] = { outL.data(), outR.data() };
    writer->writeFromFloatArrays (ptr, 2, (int) n);
    writer.reset();

    // JSON rapor
    auto* root = new juce::DynamicObject();
    root->setProperty ("girisDosya", inFile.getFileName());          // yalnız ad: rapor paylaşılabilir kalsın
    root->setProperty ("sr", sr);
    root->setProperty ("saniye", (double) n / sr);
    root->setProperty ("islemMs", ms);
    auto ekle = [&] (const char* ad, const AnalysisReport& r)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("integratedLufs", r.integratedLufs);
        o->setProperty ("voicedLufs", r.voicedLufs);
        o->setProperty ("truePeakDb", r.truePeakDb);
        o->setProperty ("noiseFloorDb", r.noiseFloorDb);
        o->setProperty ("snrDb", r.snrDb);
        o->setProperty ("crestMedianDb", r.crestMedianDb);
        o->setProperty ("sentences", r.sentences);
        o->setProperty ("voicedSec", r.voicedSec);
        o->setProperty ("sibilantEvents", r.sibilantEvents);
        o->setProperty ("f0MedianHz", r.f0MedianHz);
        o->setProperty ("keyIndex", r.keyIndex);
        o->setProperty ("keyMinor", r.keyMinor);
        o->setProperty ("keyConfidence", r.keyConfidence);
        o->setProperty ("guven", r.confidence.overall());
        juce::Array<juce::var> ds;
        for (const auto& d : r.detections) if (d.actionable (0.3f, 0.3f)) ds.add (detToVar (d));
        o->setProperty ("teshisler", ds);
        root->setProperty (ad, juce::var (o));
    };
    ekle ("giris", rap);
    ekle ("cikis", rapOut);
    if (auto* co = root->getProperty ("cikis").getDynamicObject()) co->setProperty ("midLufs", midLufs);
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("tur", fb.rounds);
        o->setProperty ("secilen", fb.chosenRound);
        o->setProperty ("skor0", fb.scores[0]);
        o->setProperty ("skorSecilen", fb.scores[fb.chosenRound]);
        o->setProperty ("abGainDb", fb.finalPlan.abGainDb);
        o->setProperty ("inputTrimDb", fb.finalPlan.inputTrimDb);
        juce::Array<juce::var> mods;
        for (const auto& m : fb.finalPlan.modules)
        {
            auto* mo = new juce::DynamicObject();
            mo->setProperty ("id", juce::String (m.moduleId));
            mo->setProperty ("acik", m.enabled);
            juce::Array<juce::var> ps;
            for (const auto& pv : m.params)
            {
                auto* po = new juce::DynamicObject();
                const auto& t = params::TABLO[(size_t) pv.id];
                po->setProperty ("id", juce::String (t.id.data(), (size_t) t.id.size()));
                po->setProperty ("deger", pv.raw);
                ps.add (juce::var (po));
            }
            mo->setProperty ("parametreler", ps);
            mods.add (juce::var (mo));
        }
        o->setProperty ("moduller", mods);
        // ekleme modüllerinin işleme sırası (GATE başta, FX kuyruğu ve limiter sonda sabit)
        static const char* ekAd[EK_SAYISI] = { "tune", "eq", "lev", "compa", "ds", "compb", "glow", "sat", "dirt", "air",
                                               "ds2", "layer", "soo", "frm", "teq" };
        juce::Array<juce::var> sira;
        for (auto e : fb.finalPlan.sira) sira.add (juce::String (ekAd[e]));
        o->setProperty ("sira", sira);
        root->setProperty ("plan", juce::var (o));
    }
    const juce::String json = juce::JSON::toString (juce::var (root), false);
    if (argc >= 4 && argv[3][0] != '-')
        juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[3])).replaceWithText (json);
    else
        std::printf ("%s\n", json.toRawUTF8());
    std::printf ("TAMAM: %.1f sn ses, %.0f ms islem, feedback %d tur (skor %.3f -> %.3f)\n",
                 (double) n / sr, ms, fb.rounds, fb.scores[0], fb.scores[fb.chosenRound]);
    return 0;
}
