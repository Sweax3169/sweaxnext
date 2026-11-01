// SWEAX NEXT — Tests/Analysis/LoudnessTest.cpp
// BS.1770-4 referans noktaları (EBU Tech 3341 ruhu, sentetik): 997 Hz tam ölçek → −3.01 LUFS;
// −23 dBFS sinüs → −23.0 LUFS; sessizlik kapıdan dışarı; true-peak örnekler arası tepeyi görür.
#include "Cati.h"
#include "Analysis/Loudness.h"
#include <vector>

namespace
{
    void sinus (std::vector<float>& v, double fs, double hz, float peak, double faz = 0.0)
    {
        for (size_t i = 0; i < v.size(); ++i) v[i] = peak * (float) std::sin (2.0 * 3.14159265358979 * hz * (double) i / fs + faz);
    }
    // kanal = 1: ITU referansı ("tek kanala uygulanan 0 dBFS 997 Hz sinüs → −3.01 LKFS");
    // kanal = 2 (aynı sinyal L+R): BS.1770 kanal toplamı → +3.01 dB (0.0 LKFS)
    float olc (double fs, double hz, float peakLin, double sn, int kanal = 1, float* tp = nullptr, float* mom = nullptr, float* st = nullptr)
    {
        sweax::LoudnessMeter m; m.prepare (fs, kanal, 512);
        std::vector<float> L ((size_t) (fs * sn));
        sinus (L, fs, hz, peakLin);
        for (size_t i = 0; i < L.size(); i += 512)
        {
            const float* blk[2] = { L.data() + i, L.data() + i };
            m.process (blk, kanal, (int) std::min<size_t> (512, L.size() - i));
        }
        if (tp) *tp = m.truePeakDb();
        if (mom) *mom = m.momentaryLufs();
        if (st) *st = m.shortTermLufs();
        return m.integratedLufs();
    }
}

SWX_TEST (loudness_k_agirlik_48k_itu_katsayilari)
{
    // ITU-R BS.1770-4 Tablo 1-2 (48 kHz) katsayıları — analog prototipten yeniden tasarım bunları vermeli
    sweax::KWeighting k; k.prepare (48000.0, 2);
    const auto& s = k.shelfCoeffs(); const auto& h = k.hpfCoeffs();
    SWX_BEKLE_YAKIN (s.b0,  1.53512485958697, 1e-6); SWX_BEKLE_YAKIN (s.b1, -2.69169618940638, 1e-6);
    SWX_BEKLE_YAKIN (s.b2,  1.19839281085285, 1e-6); SWX_BEKLE_YAKIN (s.a1, -1.69065929318241, 1e-6);
    SWX_BEKLE_YAKIN (s.a2,  0.73248077421585, 1e-6);
    SWX_BEKLE_YAKIN (h.b0,  1.0, 1e-9); SWX_BEKLE_YAKIN (h.b1, -2.0, 1e-9); SWX_BEKLE_YAKIN (h.b2, 1.0, 1e-9);
    SWX_BEKLE_YAKIN (h.a1, -1.99004745483398, 1e-6); SWX_BEKLE_YAKIN (h.a2, 0.99007225036621, 1e-6);
    // 44.1k / 96k'da filtre şekli korunur: 997 Hz'de kazanç ≈ +0.69 dB, 38 Hz'de HPF −3 dB civarı
    for (double fs : { 44100.0, 96000.0, 192000.0 })
    {
        sweax::KWeighting k2; k2.prepare (fs, 2);
        const double g997 = k2.shelfCoeffs().magnitudeDbAt (997.0, fs) + k2.hpfCoeffs().magnitudeDbAt (997.0, fs);
        SWX_BEKLE_YAKIN (g997, 0.691, 0.05);
        const double g10k = k2.shelfCoeffs().magnitudeDbAt (10000.0, fs);
        SWX_BEKLE_YAKIN (g10k, 3.99, 0.15);
    }
}

SWX_TEST (loudness_sinus_referanslari)
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        float mom = 0, st = 0;
        const float lufs = olc (fs, 997.0, 1.0f, 6.0, 1, nullptr, &mom, &st);     // tek kanal tam ölçek → −3.01 LUFS
        SWX_BEKLE_YAKIN (lufs, -3.01f, 0.1f);
        SWX_BEKLE_YAKIN (mom, -3.01f, 0.1f);
        SWX_BEKLE_YAKIN (st, -3.01f, 0.1f);
        // EBU Tech 3341 durum 1: STEREO 1 kHz sinüs, kanal başına −23 dBFS → −23.0 LUFS (tek kanal −26.02)
        const float l23 = olc (fs, 1000.0, std::pow (10.0f, -23.0f / 20.0f), 6.0, 2);
        SWX_BEKLE_YAKIN (l23, -23.0f, 0.1f);
        SWX_BEKLE_YAKIN (olc (fs, 1000.0, std::pow (10.0f, -23.0f / 20.0f), 6.0, 1), -26.02f, 0.1f);
        const float stereo = olc (fs, 997.0, 1.0f, 6.0, 2);                          // L+R aynı → kanal toplamı 0.0
        SWX_BEKLE_YAKIN (stereo, 0.0f, 0.1f);
    }
}

SWX_TEST (loudness_kapi_sessizligi_dislar)
{
    // 10 s: 4 s sinüs −23 dBFS, 3 s sessizlik, 3 s sinüs. Kapısız ortalama ≈ −24.5.
    // Kapılı: 64 tam blok (1.0) + sınırlarda 6 kısmi blok (3/4, 2/4, 1/4 dolu ×2; göreli kapının üstünde)
    // → ms ortalaması (64+3)/70 → −23 + 10·log10(0.9571) = −23.19 LUFS. Bu BS.1770'in tam davranışıdır.
    const double fs = 48000.0;
    sweax::LoudnessMeter m; m.prepare (fs, 2, 480);
    std::vector<float> L ((size_t) fs * 10, 0.0f);
    const float pk = std::pow (10.0f, -23.0f / 20.0f);
    for (size_t i = 0; i < L.size(); ++i)
    {
        const double t = (double) i / fs;
        const bool ses = (t < 4.0) || (t >= 7.0);
        L[i] = ses ? pk * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * t) : 0.0f;
    }
    const float* ch[2] = { L.data(), L.data() };
    for (size_t i = 0; i < L.size(); i += 480) { const float* b[2] = { ch[0] + i, ch[1] + i }; m.process (b, 2, 480); }
    SWX_BEKLE_YAKIN (m.integratedLufs(), -23.19f, 0.05f);
    SWX_BEKLE (m.blocksCollected() > 90);
    // tamamen sessiz → −100 (kapı her şeyi dışlar, NaN yok)
    sweax::LoudnessMeter s; s.prepare (fs, 2, 480);
    std::vector<float> Z ((size_t) fs * 2, 0.0f); const float* z[2] = { Z.data(), Z.data() };
    for (size_t i = 0; i < Z.size(); i += 480) { const float* b[2] = { z[0] + i, z[1] + i }; s.process (b, 2, 480); }
    SWX_BEKLE_YAKIN (s.integratedLufs(), -100.0f, 1e-3f);
    SWX_BEKLE (std::isfinite (s.momentaryLufs()));
}

SWX_TEST (loudness_true_peak_orneklerarasi)
{
    // 44.1 kHz'de fs/4 (11025 Hz) sinüs, faz π/4: örnek tepeleri −3.01 dBFS, gerçek tepe 0 dBTP
    const double fs = 44100.0;
    sweax::LoudnessMeter m; m.prepare (fs, 1, 441);
    std::vector<float> L ((size_t) fs * 2);
    sinus (L, fs, fs / 4.0, 1.0f, 3.14159265358979 / 4.0);
    const float* ch[1] = { L.data() };
    for (size_t i = 0; i < L.size(); i += 441) { const float* b[1] = { ch[0] + i }; m.process (b, 1, 441); }
    SWX_BEKLE_YAKIN (m.samplePeakDb(), -3.01f, 0.05f);
    SWX_BEKLE_YAKIN (m.truePeakDb(), 0.0f, 0.25f);
    // örnek üstünde tepe olan sinyalde TP ≈ SP (fazla tahmin yok)
    sweax::LoudnessMeter m2; m2.prepare (fs, 1, 441);
    std::vector<float> D ((size_t) fs);
    sinus (D, fs, 100.0, 0.5f);
    const float* c2[1] = { D.data() };
    for (size_t i = 0; i < D.size(); i += 441) { const float* b[1] = { c2[0] + i }; m2.process (b, 1, 441); }
    SWX_BEKLE_YAKIN (m2.truePeakDb(), -6.02f, 0.1f);
}
