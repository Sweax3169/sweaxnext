// SWEAX NEXT — Tests/Integration/ProcessorStateTest.cpp
// Spec §111-113 + M1 kabulü: state round-trip, kopya örnek, editör aç/kapa DSP'yi değiştirmez,
// processBlock RT kapsamı temiz, latency enable/parametreye bağlı değil, mono→stereo ve bypass tıksız.
#include "Cati.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/RtGuard.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdio>
#include <cstdlib>

namespace
{
    void hazirla (sweax::SweaxProcessor& p, double sr = 48000.0, int blok = 256)
    {
        p.setPlayConfigDetails (2, 2, sr, blok);
        p.prepareToPlay (sr, blok);
    }
    void bloklar (sweax::SweaxProcessor& p, int adet, int blok = 256, float genlik = 0.25f)
    {
        juce::AudioBuffer<float> b (2, blok); juce::MidiBuffer m;
        for (int k = 0; k < adet; ++k)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < blok; ++i)
                    b.setSample (c, i, genlik * std::sin (0.05f * (float) (k * blok + i)));
            p.processBlock (b, m);
        }
    }
    void hamAyarla (sweax::SweaxProcessor& p, sweax::params::Id id, float ham)
    {
        auto& c = p.paramCache();
        c.param (id)->setValueNotifyingHost (c.toNormalized (id, ham));
    }
}

SWX_TEST (processor_state_round_trip_ve_kopya)
{
    using namespace sweax; using namespace sweax::params;
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor a; hazirla (a);
    // 1) parametreler  2) "plan" tabanları  3) kilit  4) rol/ekran/planId
    hamAyarla (a, EqB1Gain, -4.5f); hamAyarla (a, EqB1Hz, 312.0f); hamAyarla (a, CaThresh, -21.0f);
    hamAyarla (a, DsOn, 1.0f); hamAyarla (a, MacroTemizlik, 0.62f); hamAyarla (a, QualityMode, 2.0f);
    a.session().setBaseline (EqB1Gain, -4.5f); a.session().setBaseline (CaThresh, -21.0f);
    a.session().setLocked (state::modul::EQ, true);
    a.session().setRole (state::Role::Backing); a.session().setScreen (state::Screen::MixReady);
    a.session().setPlanId (42); a.session().setAbGainDb (-1.75f);
    a.session().planNode (true).setProperty ("layout", "DeEssBeforeComp", nullptr);
    // 5) kaydet
    juce::MemoryBlock blob; a.getStateInformation (blob);
    SWX_BEKLE (blob.getSize() > 100);
    // 6) yeni örnek 7) yükle 8) HER alan eşit
    SweaxProcessor b; hazirla (b);
    b.setStateInformation (blob.getData(), (int) blob.getSize());
    int fark = 0; std::string ilk;
    for (size_t i = 0; i < TABLO.size(); ++i)
    {
        const float va = a.paramCache().raw ((Id) i), vb = b.paramCache().raw ((Id) i);
        if (std::fabs (va - vb) > 1e-4f) { ++fark; if (ilk.empty()) ilk = std::string (TABLO[i].id); }
    }
    SWX_BEKLE (fark == 0);
    if (fark) _d.sonHata = "param farki: " + ilk;
    SWX_BEKLE (b.session().getRole() == state::Role::Backing);
    SWX_BEKLE (b.session().getScreen() == state::Screen::MixReady);
    SWX_BEKLE_EQ (b.session().getPlanId(), (juce::uint32) 42);
    SWX_BEKLE (b.session().isLocked (state::modul::EQ) && ! b.session().isLocked (state::modul::DEESS));
    SWX_BEKLE_YAKIN (b.session().getBaseline (EqB1Gain, 0.0f), -4.5f, 1e-6f);
    SWX_BEKLE_YAKIN (b.session().getAbGainDb(), -1.75f, 1e-6f);
    SWX_BEKLE (b.session().planNode (false).getProperty ("layout").toString() == "DeEssBeforeComp");
    SWX_BEKLE_YAKIN (b.paramCache().getBaseline (CaThresh), -21.0f, 1e-6f); // SESSION → ParamCache taban
    // kopya: aynı blob'dan üçüncü örnek de birebir (sessiz yeniden analiz yok — planId aynı)
    SweaxProcessor c; hazirla (c);
    c.setStateInformation (blob.getData(), (int) blob.getSize());
    SWX_BEKLE_EQ (c.session().getPlanId(), (juce::uint32) 42);
    SWX_BEKLE_YAKIN (c.paramCache().raw (EqB1Hz), 312.0f, 0.05f);
    // 9) yükleme sırasında Listening kaydedildiyse Ready/MixReady'e düşer (§6)
    a.session().setScreen (state::Screen::Listening);
    juce::MemoryBlock blob2; a.getStateInformation (blob2);
    SweaxProcessor d; hazirla (d);
    d.setStateInformation (blob2.getData(), (int) blob2.getSize());
    SWX_BEKLE (d.session().getScreen() == state::Screen::MixReady);
}

SWX_TEST (processor_state_eski_oturum_yeni_param_varsayilan)
{
    // M3.4 göçü: KİR/KATMAN parametreleri olmayan eski oturum/preset yüklenince bu modüller açık KALMAZ
    using namespace sweax; using namespace sweax::params;
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor a; hazirla (a);
    hamAyarla (a, CaThresh, -17.0f);
    juce::MemoryBlock blob; a.getStateInformation (blob);
    auto xml = juce::AudioProcessor::getXmlFromBinary (blob.getData(), (int) blob.getSize());
    SWX_BEKLE (xml != nullptr);
    if (xml == nullptr) return;
    int silinen = 0;
    std::function<void (juce::XmlElement&)> ayikla = [&] (juce::XmlElement& e)
    {
        juce::Array<juce::XmlElement*> sil;
        for (auto* c : e.getChildIterator())
        {
            const auto id = c->getStringAttribute ("id");
            if (c->hasTagName ("PARAM") && (id.startsWith ("swx.dirt") || id.startsWith ("swx.layer"))) sil.add (c);
            else ayikla (*c);
        }
        for (auto* c : sil) { e.removeChildElement (c, true); ++silinen; }
    };
    ayikla (*xml);
    SWX_BEKLE (silinen >= 14);
    juce::MemoryBlock eski;
    juce::AudioProcessor::copyXmlToBinary (*xml, eski);
    // yükleyen örnekte KİR/KATMAN önceden AÇIK (kullanıcı başka bir şarkıda açmış)
    SweaxProcessor b; hazirla (b);
    hamAyarla (b, DirtOn, 1.0f); hamAyarla (b, DirtLevel, -6.0f); hamAyarla (b, LayerOn, 1.0f); hamAyarla (b, LayerA, -12.0f);
    b.setStateInformation (eski.getData(), (int) eski.getSize());
    SWX_BEKLE (b.paramCache().raw (DirtOn) < 0.5f);
    SWX_BEKLE (b.paramCache().raw (LayerOn) < 0.5f);
    SWX_BEKLE_YAKIN (b.paramCache().raw (DirtLevel), TABLO[(size_t) DirtLevel].def, 1e-3f);
    SWX_BEKLE_YAKIN (b.paramCache().raw (LayerA), TABLO[(size_t) LayerA].def, 1e-3f);
    SWX_BEKLE_YAKIN (b.paramCache().raw (CaThresh), -17.0f, 0.05f);
}

SWX_TEST (processor_editor_ac_kapa_dsp_degismez)
{
    using namespace sweax; using namespace sweax::params;
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor p; hazirla (p);
    hamAyarla (p, InputTrim, 3.0f); hamAyarla (p, OutputTrim, -2.0f);
    bloklar (p, 20);
    juce::MemoryBlock once; p.getStateInformation (once);
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        SWX_BEKLE (ed != nullptr);
        bloklar (p, 5);
    }
    juce::MemoryBlock sonra; p.getStateInformation (sonra);
    SWX_BEKLE (once == sonra);
    SWX_BEKLE_YAKIN (p.paramCache().raw (InputTrim), 3.0f, 1e-4f);
}

SWX_TEST (processor_rt_kapsam_ve_latency_sabit)
{
    using namespace sweax; using namespace sweax::params;
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor p; hazirla (p);
    bloklar (p, 4);
    sweax::rt::resetViolations();
    bloklar (p, 50);                       // ısınmış durumda 50 blok: tahsis olmamalı
    hamAyarla (p, InputTrim, -6.0f);       // parametre değişimi de blok içinde tahsis yapmaz
    bloklar (p, 20);
    SWX_BEKLE_EQ (sweax::rt::violations(), (uint64_t) 0);
    // latency: enable/parametre değişimi latency'yi DEĞİŞTİRMEZ (§7.1)
    const int l0 = p.getLatencySamples();
    hamAyarla (p, EqOn, 1.0f); hamAyarla (p, CaOn, 1.0f); hamAyarla (p, DsOn, 1.0f); hamAyarla (p, LevOn, 1.0f);
    bloklar (p, 5);
    SWX_BEKLE_EQ (p.getLatencySamples(), l0);
    SWX_BEKLE_EQ (p.currentLatency(), l0);
}

SWX_TEST (processor_bypass_tiksiz_ve_mono_stereo)
{
    using namespace sweax; using namespace sweax::params;
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor p; hazirla (p, 48000.0, 64);
    hamAyarla (p, OutputTrim, -12.0f);     // bypass'ta belirgin fark olsun
    bloklar (p, 40, 64);
    // bypass'ı aç: geçiş boyunca örnekler arası sıçrama küçük olmalı (crossfade 10 ms = 480 örnek)
    hamAyarla (p, Bypass, 1.0f);
    juce::AudioBuffer<float> b (2, 64); juce::MidiBuffer m;
    float maksSicrama = 0.0f, onceki = 0.0f; bool ilk = true;
    for (int k = 0; k < 12; ++k)
    {
        for (int i = 0; i < 64; ++i) { const float v = 0.5f * std::sin (0.02f * (float) (k * 64 + i)); b.setSample (0, i, v); b.setSample (1, i, v); }
        p.processBlock (b, m);
        for (int i = 0; i < 64; ++i)
        {
            const float v = b.getSample (0, i);
            if (! ilk) maksSicrama = std::max (maksSicrama, std::fabs (v - onceki));
            onceki = v; ilk = false;
        }
    }
    // sinüsün doğal adımı ~0.5*0.02 = 0.01; crossfade bunu en fazla küçük payla aşar (tık yok)
    SWX_BEKLE (maksSicrama < 0.03f);
    // sonunda tamamen kuru: çıkış = giriş (-12 dB uygulanmıyor). Kuru yol zincir latency'si kadar gecikir → önce doldur.
    const int kuruBlok = p.currentLatency() / 64 + 12;
    for (int i = 0; i < 64; ++i) { b.setSample (0, i, 0.5f); b.setSample (1, i, 0.5f); }
    for (int k = 0; k < kuruBlok; ++k) p.processBlock (b, m), std::fill_n (b.getWritePointer (0), 64, 0.5f), std::fill_n (b.getWritePointer (1), 64, 0.5f);
    p.processBlock (b, m);
    SWX_BEKLE_YAKIN (b.getSample (0, 63), 0.5f, 1e-4f);
    // mono giriş → stereo çıkış: 2. kanal 1.'nin kopyası
    SweaxProcessor q;
    q.setPlayConfigDetails (1, 2, 48000.0, 64);
    q.prepareToPlay (48000.0, 64);
    juce::AudioBuffer<float> mb (2, 64);
    for (int i = 0; i < 64; ++i) { mb.setSample (0, i, 0.3f); mb.setSample (1, i, 99.0f); } // 2. kanal çöp
    q.processBlock (mb, m);
    SWX_BEKLE_YAKIN (mb.getSample (1, 40), mb.getSample (0, 40), 1e-6f);
    SWX_BEKLE (std::fabs (mb.getSample (1, 40)) < 1.0f);
}

SWX_TEST (processor_ab_orijinal_kuru_yol_limiterli)
{
    // M3.4 denetim DSP-5: A/B eş-ses kazancı ±24 dB → ORİJİNAL kuru yolu −1 dBTP tavanını aşmaz; kazanç 0'da birebire yakın
    using namespace sweax; using namespace sweax::params;
    juce::ScopedJuceInitialiser_GUI init;
    SweaxProcessor p; hazirla (p, 48000.0, 256);
    hamAyarla (p, AbMode, 1.0f); hamAyarla (p, AbMatch, 1.0f);
    auto tepe = [&] (float abDb, float genlik)
    {
        p.session().setAbGainDb (abDb);
        p.refreshAbGain();                                              // abGainLin oturumdan tazelenir (timer'ın işi)
        juce::AudioBuffer<float> b (2, 256); juce::MidiBuffer m;
        float mx = 0.0f; double hata = 0.0;
        const int blok = 200;
        const int L = p.currentLatency();
        std::vector<float> giris;
        for (int k = 0; k < blok; ++k)
        {
            for (int i = 0; i < 256; ++i)
            {
                const float v = genlik * std::sin (2.0f * 3.14159265f * 440.0f * (float) (k * 256 + i) / 48000.0f);
                b.setSample (0, i, v); b.setSample (1, i, v); giris.push_back (v);
            }
            p.processBlock (b, m);
            if (k > blok / 2)
                for (int i = 0; i < 256; ++i)
                {
                    mx = std::max (mx, std::fabs (b.getSample (0, i)));
                    const size_t j = (size_t) (k * 256 + i - L);
                    if (j < giris.size()) hata = std::max (hata, (double) std::fabs (b.getSample (0, i) - giris[j]));
                }
        }
        return std::make_pair (mx, hata);
    };
    const auto yuksek = tepe (20.0f, 0.5f);
    if (std::getenv ("SWX_VERBOSE")) std::printf ("   ab+20: tepe %.3f hata %.4f\n", yuksek.first, yuksek.second);
    SWX_BEKLE (yuksek.first <= 0.90f);                                  // −1 dBTP ≈ 0.891 (+ tahmin payı)
    const auto sifir = tepe (0.0f, 0.3f);
    if (std::getenv ("SWX_VERBOSE")) std::printf ("   ab 0: tepe %.3f hata %.4f gecikme %d\n", sifir.first, sifir.second, p.currentLatency());
    SWX_BEKLE (sifir.second < 0.02);                                    // kazanç 0 ve tavan altı: kuru ≈ giriş
}
