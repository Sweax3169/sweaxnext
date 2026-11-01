// SWEAX NEXT — Tests/Integration/SanatciAyrimTest.cpp
// M3.4 SANATÇI KİMLİK regresyon kilidi (sahip 2026-09-17: "hepsi aynı", "Killoki trap metal yapıyor").
// Beş sanatçı planı GERÇEK zincirden (Sandbox::renderStereo) geçirilir; kimlik YEDİ eksende ÖLÇÜLÜR:
//   GRIT    — saf 220 Hz sinüs geçişinde THD (2..8. harmonik / temel)        → trap metal kiri
//   OKTAV   — aynı geçişte 110 Hz (f/2) / 220 Hz                              → katman kültürü
//   TELEFON — ton geçişinde [0.5-1.5 kHz] − [3-6 kHz] bant eğimi (KİR lofi bant rengi) → Ati242 orta ağırlık
//   RENK    — zengin ton geçişinde 8-14 kHz / 300-800 Hz BANT enerjisi eğimi   → parlak/koyu
//   MEKÂN   — son tondan 0.25-0.55 s sonra boşluk kuyruğu                      → kuru/ıslak
//   GENİŞLİK— ton geçişinde yan/orta enerji                                    → mono yumruk / katman duvarı
//   EŞ-SES  — ton geçişi RMS'leri (kimlik seviyeden gelmez)
// İKİ SENARYO (denetim M3.4 kimlik-4): (a) temiz rapor, (b) SİBİLANS + PARLAKLIK kanıtlı rapor — planner'ın hava rafını
// yarılayan / AIR'ı kısan yolları gerçek korpusta çalışıyor; kimlik o yolda da ayrışmalı.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "Analysis/Report.h"
#include "Decision/Planner.h"
#include "Decision/Sandbox.h"
#include "Decision/Receteler.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <random>
#include <cmath>
#include <cstdio>
#include <string>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    using namespace sweax;
    using namespace sweax::decision;

    // düzen: 0.4 oda | 4×[1.0 ton + 0.6 oda] | 1.2 sinüs 220 Hz | 1.0 oda   (toplam 9.0 s)
    constexpr double T_TON3 = 0.4 + 2 * 1.6;           // 3. ton başlangıcı
    constexpr double T_SONTON_BIT = 0.4 + 3 * 1.6 + 1.0;
    constexpr double T_SINUS = 0.4 + 4 * 1.6;
    // ölçümle kalibre (M3.4 denetim turu): telefon rengi ve sibilanslı senaryoda hava eğimi eşikleri
    constexpr double SANATCI_TELEFON_ESIK = 1.0;
    constexpr double SANATCI_RENK_SIB_ESIK = 3.0;

    std::vector<float> sahne (double fs)
    {
        std::vector<float> x; std::mt19937 rng (5); std::normal_distribution<float> nd (0.0f, 1.0f);
        double faz = 0.0;
        auto oda = [&] (double sn) { for (size_t i = 0; i < (size_t) (sn * fs); ++i) x.push_back (0.0003f * nd (rng)); };
        auto ton = [&] (double sn, double f0)
        {
            for (size_t i = 0; i < (size_t) (sn * fs); ++i)
            {
                faz += 2.0 * PI * f0 / fs;
                float v = 0.0f;
                for (int h = 1; h <= 24; ++h) v += (float) (std::sin (h * faz) / (double) h);
                x.push_back (0.10f * v + 0.004f * nd (rng));
            }
        };
        oda (0.4);
        for (int k = 0; k < 4; ++k) { ton (1.0, 155.0 + 10.0 * k); oda (0.6); }
        { double f = 0.0; for (size_t i = 0; i < (size_t) (1.2 * fs); ++i) { f += 2.0 * PI * 220.0 / fs; x.push_back (0.22f * (float) std::sin (f)); } }
        oda (1.0);
        return x;
    }

    AnalysisReport rapor (bool sibilansli = false)
    {
        AnalysisReport r;
        r.capturedSec = 30.0; r.voicedSec = 20.0; r.roomToneSec = 5.0; r.sentences = 10;
        r.voicedLufs = -20.0f; r.integratedLufs = -22.0f; r.noiseFloorDb = -62.0f; r.snrDb = 40.0f;
        r.crestMedianDb = 9.0f; r.microStdMedianDb = 2.0f; r.sentenceIqrDb = 3.0f; r.sentenceP95P05Db = 4.0f;
        r.f0P5Hz = 110.0f; r.f0MedianHz = 160.0f; r.f0P95Hz = 260.0f;
        r.sibilantCenterHz = 6800.0f; r.sibilantPerMinute = 8.0f; r.sibilantEvents = 4;
        r.tonalFrames = 60;
        if (sibilansli) { r.sibilantPerMinute = 30.0f; r.sibilantEvents = 30; }
        r.confidence.level = r.confidence.noise = r.confidence.tonal = r.confidence.dynamics = r.confidence.sibilance = 1.0f;
        return r;
    }

    double bolgeRms (const std::vector<float>& x, int a, int b)
    {
        double s = 0.0; int c = 0;
        for (int i = a; i < b && i < (int) x.size(); ++i) { s += (double) x[(size_t) i] * x[(size_t) i]; ++c; }
        return 10.0 * std::log10 (s / std::max (1, c) + 1e-24);
    }
    // Hann pencereli Goertzel (dB) — komşu bileşen sızıntısı düşük
    double tonDb (const std::vector<float>& x, double fs, double hz, int a, int b)
    {
        const double om = 2.0 * PI * hz / fs, cw = 2.0 * std::cos (om);
        const int n = b - a;
        double s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * PI * i / (n - 1));
            const double s0 = (double) x[(size_t) (a + i)] * w + cw * s1 - s2; s2 = s1; s1 = s0;
        }
        return 10.0 * std::log10 (std::max (s1 * s1 + s2 * s2 - cw * s1 * s2, 1e-30) / ((double) n * n));
    }
    // Hann pencereli FFT bant enerjisi (dB) — tek-frekans kutusu harmonik-dışına düşüp gürültü/eko ölçüyordu
    double bantDb (const std::vector<float>& x, double fs, double hzA, double hzB, int a)
    {
        constexpr int ORDER = 14, N = 1 << ORDER;
        juce::dsp::FFT fft (ORDER);
        std::vector<float> buf ((size_t) (2 * N), 0.0f);
        for (int i = 0; i < N && a + i < (int) x.size(); ++i)
            buf[(size_t) i] = x[(size_t) (a + i)] * (float) (0.5 - 0.5 * std::cos (2.0 * PI * i / (N - 1)));
        fft.performFrequencyOnlyForwardTransform (buf.data());
        double p = 0.0;
        for (int k = (int) std::ceil (hzA * N / fs); k < (int) std::floor (hzB * N / fs) && k < N / 2; ++k)
            p += (double) buf[(size_t) k] * buf[(size_t) k];
        return 10.0 * std::log10 (p + 1e-30);
    }
    double gucTopla (std::initializer_list<double> dbs) { double p = 0.0; for (double d : dbs) p += std::pow (10.0, d / 10.0); return 10.0 * std::log10 (p + 1e-30); }

    struct Olcum { double thd, oktav, oktavUst, telefon, egim, kuyruk, yan, tonRms, formant; };

    // spektral zarf ağırlık merkezi (300-5000 Hz, kepstral kaba zarf yerine bant-enerji ağırlıklı ortalama Hz)
    double zarfMerkezi (const std::vector<float>& x, double fs, int a2, int b2)
    {
        constexpr int ORDER = 13, N = 1 << ORDER;
        juce::dsp::FFT fft (ORDER);
        std::vector<float> buf ((size_t) (2 * N), 0.0f);
        double pay = 0.0, payda = 0.0;
        for (int bas = a2; bas + N <= b2; bas += N / 2)
        {
            for (int i = 0; i < N; ++i)
                buf[(size_t) i] = x[(size_t) (bas + i)] * (float) (0.5 - 0.5 * std::cos (2.0 * PI * i / (N - 1)));
            std::fill (buf.begin() + N, buf.end(), 0.0f);
            fft.performFrequencyOnlyForwardTransform (buf.data());
            for (int k = (int) std::ceil (300.0 * N / fs); k < (int) std::floor (5000.0 * N / fs); ++k)
            {
                const double pw = (double) buf[(size_t) k] * buf[(size_t) k];
                pay += pw * (k * fs / N); payda += pw;
            }
        }
        return payda > 0.0 ? pay / payda : 0.0;
    }

    Olcum olc (int stil, const std::vector<float>& in, double fs, bool sibilansli)
    {
        auto r = rapor (sibilansli);
        std::vector<ProblemDetection> dets;
        if (sibilansli)
        {
            ProblemDetection s; s.type = Problem::Sibilance; s.probability = 1.0f; s.severity = 1.0f; s.confidence = 1.0f;
            s.centerHz = 7000.0f; s.valueDb = 20.0f; s.count = 30; dets.push_back (s);
            ProblemDetection b; b.type = Problem::Bright; b.probability = 1.0f; b.severity = 0.8f; b.confidence = 1.0f;
            b.centerHz = 8000.0f; b.valueDb = 10.0f; dets.push_back (b);
        }
        PlannerInput pin; pin.report = &r; pin.detections = &dets; pin.role = state::Role::Lead; pin.planId = 31; pin.style = stil;
        const auto plan = Planner::plan (pin);
        const int64_t n = (int64_t) in.size();
        std::vector<float> L ((size_t) n), R ((size_t) n);
        Sandbox::renderStereo (plan, in.data(), L.data(), R.data(), n, fs);
        std::vector<float> orta ((size_t) n), yan ((size_t) n);
        for (int64_t i = 0; i < n; ++i) { orta[(size_t) i] = 0.5f * (L[(size_t) i] + R[(size_t) i]); yan[(size_t) i] = 0.5f * (L[(size_t) i] - R[(size_t) i]); }

        const int sr = (int) fs;
        const int gec = Sandbox::latencySamples (fs);                 // pencereler zincir gecikmesiyle hizalı
        const int tonA = gec + (int) ((T_TON3 + 0.3) * sr), tonB = tonA + (int) (0.5 * sr);
        const int kuyA = gec + (int) ((T_SONTON_BIT + 0.25) * sr), kuyB = gec + (int) ((T_SONTON_BIT + 0.55) * sr);
        const int sinA = gec + (int) ((T_SINUS + 0.4) * sr), sinB = gec + (int) ((T_SINUS + 1.1) * sr);

        Olcum o {};
        const double temel = tonDb (orta, fs, 220.0, sinA, sinB);
        double harm = 0.0;
        for (int k = 2; k <= 8; ++k) harm += std::pow (10.0, tonDb (orta, fs, 220.0 * k, sinA, sinB) / 10.0);
        o.thd = 10.0 * std::log10 (harm + 1e-30) - temel;
        o.oktav = tonDb (orta, fs, 110.0, sinA, sinB) - temel;
        o.oktavUst = tonDb (orta, fs, 440.0, sinA, sinB) - temel;   // M3.5: oktav-ÜSTÜ katman (layerA +12: killoki/ezhel/şehinşah/sagopa)
        o.formant = zarfMerkezi (orta, fs, tonA, tonB);             // M3.5: FORMANT modülü zarf merkezini kaydırır (Hz)
        o.telefon = bantDb (orta, fs, 500.0, 1500.0, tonA) - bantDb (orta, fs, 3000.0, 6000.0, tonA);
        o.egim = bantDb (orta, fs, 8000.0, 14000.0, tonA) - bantDb (orta, fs, 300.0, 800.0, tonA);
        o.kuyruk = bolgeRms (orta, kuyA, kuyB);
        o.yan = bolgeRms (yan, tonA, tonB) - bolgeRms (orta, tonA, tonB);
        o.tonRms = bolgeRms (orta, tonA, tonB);
        return o;
    }
}

SWX_TEST (sanatci_render_ayrimi)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double fs = 48000.0;
    const auto in = sahne (fs);
    // M3.5: 12 sanatçı (stil 4-15). Eksenler: THD (kir), OKTAV alt/üst (katman), TELEFON, EĞİM (parlak-koyu),
    // KUYRUK (ıslaklık), YAN (genişlik), FORMANT (zarf merkezi), eş-ses. Eşikler ölçümle kalibre edildi (SWX_VERBOSE tablo).
    static const char* AD[12] = { "killoki", "reckol", "ati242", "uzi", "motive", "kava", "ezhel", "ceza", "allame", "sehinsah", "sagopa", "benfero" };
    for (bool sib : { false, true })
    {
        std::array<Olcum, 12> o {};
        for (int i = 0; i < 12; ++i) o[(size_t) i] = olc (4 + i, in, fs, sib);
        auto O = [&] (const char* ad) -> const Olcum& { for (int i = 0; i < 12; ++i) if (std::string (AD[i]) == ad) return o[(size_t) i]; return o[0]; };

        if (std::getenv ("SWX_VERBOSE"))
        {
            std::printf ("   [%s] %-9s %7s %7s %8s %8s %7s %8s %7s %8s %7s\n", sib ? "sibilansli" : "temiz", "stil", "thd", "oktav", "oktavUst", "telefon", "egim", "kuyruk", "yan", "formant", "tonRms");
            for (int i = 0; i < 12; ++i)
                std::printf ("   %-9s %7.1f %7.1f %8.1f %8.1f %7.1f %8.1f %7.1f %8.0f %7.1f\n", AD[i], o[(size_t) i].thd, o[(size_t) i].oktav, o[(size_t) i].oktavUst, o[(size_t) i].telefon, o[(size_t) i].egim, o[(size_t) i].kuyruk, o[(size_t) i].yan, o[(size_t) i].formant, o[(size_t) i].tonRms);
        }

        // ---- EŞ-SES: reçete çıkış trimleri farklı; trim çıkarıldıktan sonra ham plan seviyeleri dar bantta kalmalı
        // (tam eş-ses Feedback aşamasının işi — burada zincirin kendi kazanç dağılımı denetlenir)
        {
            double mx = -1e9, mn = 1e9;
            for (int i = 0; i < 12; ++i)
            {
                const double duz = o[(size_t) i].tonRms - (double) decision::recete (4 + i).outTrimDb;
                mx = std::max (mx, duz); mn = std::min (mn, duz);
            }
            // 12 reçete arası ham zincir kazanç yayılımı ölçüldü ≈8.5 dB (GR 2.6-11 dB bandı + kir/glow farkları).
            // GERÇEK eş-ses Feedback aşamasının işi (tam zincir LUFS hedefi −19; FeedbackTest + korpus ölçümü).
            // Buradaki sınır felaket yakalayıcıdır (ör. çift kazanç/unutulmuş trim), hassas eş-ses değil.
            SWX_BEKLE (mx - mn <= 10.0);
        }
        // ---- ÇİFT AYRIŞMASI: her sanatçı çifti en az bir eksende ölçülebilir farklı (normalize uzaklık)
        {
            auto uzak = [] (const Olcum& a2, const Olcum& b2)
            {
                // eksen ölçekleri: dB eksenlerinde "duyulur fark" payları
                const double d[7] = { (a2.thd - b2.thd) / 3.0, (a2.oktav - b2.oktav) / 6.0, (a2.oktavUst - b2.oktavUst) / 6.0,
                                      (a2.egim - b2.egim) / 2.0, (a2.kuyruk - b2.kuyruk) / 3.0, (a2.yan - b2.yan) / 3.0,
                                      (a2.formant - b2.formant) / 150.0 };
                double s2 = 0.0; for (double v : d) s2 += v * v;
                return std::sqrt (s2);
            };
            for (int i = 0; i < 12; ++i)
                for (int j = i + 1; j < 12; ++j)
                {
                    const double d = uzak (o[(size_t) i], o[(size_t) j]);
                    if (d < 1.0 && std::getenv ("SWX_VERBOSE")) std::printf ("   YAKIN ÇİFT %s-%s: %.2f\n", AD[i], AD[j], d);
                    SWX_BEKLE (d >= 1.0);
                }
        }
        // ---- imza denetimleri (yön; reçete ayarlansa da değişmemeli) ----
        SWX_BEKLE (O ("killoki").thd >= O ("reckol").thd + 6.0);                    // trap metal kiri
        SWX_BEKLE (O ("killoki").thd >= O ("allame").thd + 6.0);
        SWX_BEKLE (O ("uzi").oktav >= O ("motive").oktav + 10.0);                   // Uzi oktav-altı katman
        SWX_BEKLE (O ("sagopa").oktavUst >= O ("motive").oktavUst + 6.0);           // Sagopa oktav-üstü katman kültürü
        SWX_BEKLE (O ("reckol").egim >= O ("sagopa").egim + 2.0);                   // parlak drill ↔ koyu/buğulu
        SWX_BEKLE (O ("uzi").kuyruk >= O ("allame").kuyruk + 3.0);                  // ıslak hook ↔ kuru boom-bap
        SWX_BEKLE (O ("allame").yan <= O ("uzi").yan - 3.0);                        // dar/çıplak ↔ katman duvarı
    }
}
