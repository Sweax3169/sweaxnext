// SWEAX NEXT — Tests/DSP/RobustlukTest.cpp
// İnceleme bulgularının regresyonları: n > maxBlock parçalanır (taşma yok); kapalı modülün gecikme hatları
// beslenir (yeniden açılış bayat ses çalmaz); DelayLine delay 0'dayken de geçmiş yazar; lookahead değişimi
// tıksız; bant/HPF kapama tıksız; NaN sonrası tüm modüller toparlanır.
#define _CRT_SECURE_NO_WARNINGS
#include "Cati.h"
#include "DSP/Compressor.h"
#include "DSP/DeEsser.h"
#include "DSP/VocalLeveler.h"
#include "DSP/OutputStage.h"
#include "DSP/CorrectiveEQ.h"
#include "Chain/VocalChain.h"
#include "Core/DelayLine.h"
#include <vector>
#include <cmath>
#include <limits>
#include <cstdio>
#include <cstdlib>

namespace
{
    constexpr double PI = 3.14159265358979323846;
    std::vector<float> sinus (double fs, double sn, double hz, float a)
    {
        std::vector<float> v ((size_t) (fs * sn));
        for (size_t i = 0; i < v.size(); ++i) v[i] = a * (float) std::sin (2.0 * PI * hz * (double) i / fs);
        return v;
    }
    float tepe (const std::vector<float>& v, int a, int b) { float p = 0.0f; for (int i = a; i < b && i < (int) v.size(); ++i) p = std::max (p, std::fabs (v[(size_t) i])); return p; }
    // NaN blogundan sonra gecikme hatlari lat kadar NaN bosaltir (dogru davranis); sonrasi sonlu olmali
    bool sonluSonra (const std::vector<float>& v, int from) { for (int i = from; i < (int) v.size(); ++i) if (! std::isfinite (v[(size_t) i])) return false; return true; }
}

SWX_TEST (robust_buyuk_blok_parcalama)
{
    // prepare 64'lük blokla, 4096'lık TEK çağrı: taşma yok, çıkış = latency kadar gecikmiş giriş (eşik altı)
    const double fs = 48000.0;
    sweax::Compressor c; c.prepare (fs, 64, 2); c.setQuality (sweax::Quality::Yuksek); c.setEnabled (true, true);
    sweax::CompParams p; p.on = true; p.threshDb = 0.0f; c.setParams (p);
    const int D = c.latencySamples (sweax::Quality::Yuksek);
    std::vector<float> L (4096, 0.0f), R (4096, 0.0f); L[100] = 0.5f; R[100] = 0.5f;
    float* ch[2] = { L.data(), R.data() };
    c.process (ch, 2, 4096);
    int at = -1; for (int i = 0; i < 4096; ++i) if (std::fabs (L[(size_t) i]) > 0.25f) { at = i; break; }
    SWX_BEKLE_EQ (at, 100 + D);
    // leveler için de (150 ms hat, 480'lik prepare, 8192 çağrı)
    sweax::VocalLeveler l; l.prepare (fs, 480, 1); l.setQuality (sweax::Quality::Ultra); l.setEnabled (true, true);
    std::vector<float> x (8192, 0.0f); x[10] = 0.4f;
    float* cx[1] = { x.data() };
    l.process (cx, 1, 8192);
    at = -1; for (int i = 0; i < 8192; ++i) if (std::fabs (x[(size_t) i]) > 0.2f) { at = i; break; }
    SWX_BEKLE_EQ (at, 10 + l.latencySamples (sweax::Quality::Ultra));
}

SWX_TEST (robust_yeniden_acilis_bayat_ses_yok)
{
    // leveler: açıkken 1 s sinüs → kapat → 0.5 s sessizlik (kapalı) → aç → çıkışta bayat sinüs patlaması YOK
    const double fs = 48000.0;
    sweax::VocalLeveler l; l.prepare (fs, 480, 1); l.setQuality (sweax::Quality::Yuksek); l.setEnabled (true, true);
    auto ses = sinus (fs, 1.0, 220.0, 0.25f);
    for (int i = 0; i < (int) ses.size(); i += 480) { float* ch[1] = { ses.data() + i }; l.process (ch, 1, std::min (480, (int) ses.size() - i)); }
    l.setEnabled (false);                                              // crossfade ile kapan
    std::vector<float> sessiz ((size_t) (fs * 0.5), 0.0f);
    for (int i = 0; i < (int) sessiz.size(); i += 480) { float* ch[1] = { sessiz.data() + i }; l.process (ch, 1, std::min (480, (int) sessiz.size() - i)); }
    l.setEnabled (true);                                               // yeniden aç
    std::vector<float> out ((size_t) (fs * 0.4), 0.0f);
    for (int i = 0; i < (int) out.size(); i += 480) { float* ch[1] = { out.data() + i }; l.process (ch, 1, std::min (480, (int) out.size() - i)); }
    SWX_BEKLE (20.0 * std::log10 (tepe (out, 0, (int) out.size()) + 1e-12) < -60.0);
    // de-esser split modda da: kapalıyken crossover+hatlar beslendi → açılışta sürekli sinüste tık yok
    sweax::DeEsser d; d.prepare (fs, 256, 1); d.setQuality (sweax::Quality::Yuksek); d.setEnabled (true, true);
    sweax::DeEssParams dp; dp.on = true; dp.mode = 0; dp.hz = 6000.0f; dp.threshDb = 0.0f; d.setParams (dp);
    auto y = sinus (fs, 1.2, 1000.0, 0.25f);
    for (int i = 0; i < (int) y.size(); i += 256)
    {
        if (i == 19200) d.setEnabled (false);
        if (i == 38400) d.setEnabled (true);
        float* ch[1] = { y.data() + i }; d.process (ch, 1, std::min (256, (int) y.size() - i));
    }
    float maxFark = 0.0f;
    for (size_t i = 4800; i < y.size(); ++i) maxFark = std::max (maxFark, std::fabs (y[i] - y[i - 1]));
    SWX_BEKLE (maxFark < 0.075f);                                      // 1 kHz doğal adım ≈ 0.033; tık yok
}

SWX_TEST (robust_delayline_ve_lookahead_degisimi)
{
    auto CP = [] (const char* m) { if (std::getenv ("SWX_VERBOSE")) { std::printf ("      CP %s\n", m); std::fflush (stdout); } };
    CP ("basla");
    // delay 0'dayken geçmiş yazılır: 0 → 10 geçişi gerçek geçmişi okur (sıfır/bayat değil)
    sweax::DelayLine dl; dl.prepare (100, 0); CP ("dl hazir");
    for (int i = 0; i < 50; ++i) dl.processSample ((float) i);
    dl.setDelay (10);
    SWX_BEKLE_YAKIN (dl.processSample (50.0f), 40.0f, 1e-6f);          // 50 yazıldı (indeks 50); 10 geriye = indeks 40
    CP ("dl bitti");
    // kompresör: sabit GR'de lookahead 5 → 0 → 5 ms: kazanç sıçraması yok (durağan sinüs, örnek farkı sınırlı)
    const double fs = 48000.0;
    sweax::Compressor c; c.prepare (fs, 256, 1); c.setQuality (sweax::Quality::Yuksek); c.setEnabled (true, true);
    sweax::CompParams p; p.on = true; p.threshDb = -20.0f; p.ratio = 4.0f; p.kneeDb = 0.0f; p.attackMs = 5.0f; p.releaseMs = 100.0f;
    p.lookaheadMs = 5.0f; p.detector = 0; p.scHpfHz = 20.0f;
    c.setParams (p); CP ("comp hazir");
    auto x = sinus (fs, 1.5, 500.0, 0.5f); CP ("sinus hazir");
    for (int i = 0; i < (int) x.size(); i += 256)
    {
        if (i == 24064) { p.lookaheadMs = 0.0f; c.setParams (p); }
        if (i == 48128) { p.lookaheadMs = 5.0f; c.setParams (p); }
        float* ch[1] = { x.data() + i }; c.process (ch, 1, std::min (256, (int) x.size() - i));
    }
    CP ("islem bitti");
    float maxFark = 0.0f;
    for (size_t i = 24000; i < x.size(); ++i) maxFark = std::max (maxFark, std::fabs (x[i] - x[i - 1]));
    SWX_BEKLE (maxFark < 0.05f);                                       // 500 Hz doğal adım ≈ 0.021; sıçrama yok
}

SWX_TEST (robust_eq_bant_ve_hpf_kapama_tiksiz)
{
    const double fs = 48000.0;
    sweax::CorrectiveEQ eq; eq.prepare (fs, 256, 1); eq.setEnabled (true, true);
    sweax::EqParams p; p.on = true;
    p.band[0].on = true; p.band[0].type = 0; p.band[0].hz = 1000.0f; p.band[0].gainDb = 6.0f; p.band[0].q = 1.0f;
    eq.setParams (p);
    auto x = sinus (fs, 1.5, 1000.0, 0.25f);
    for (int i = 0; i < (int) x.size(); i += 256)
    {
        if (i == 24064) { p.band[0].on = false; eq.setParams (p); }    // bandı KAPAT (modül açık kalır)
        float* ch[1] = { x.data() + i }; eq.process (ch, 1, std::min (256, (int) x.size() - i));
    }
    float maxFark = 0.0f;
    for (size_t i = 4800; i < x.size(); ++i) maxFark = std::max (maxFark, std::fabs (x[i] - x[i - 1]));
    SWX_BEKLE (maxFark < 0.075f);
    SWX_BEKLE_YAKIN (tepe (x, (int) fs * 14 / 10, (int) x.size()), 0.25f, 0.01f);   // sonunda birim kazanç
    // HPF kapama: 60 Hz sinüs, HPF 120/24 dB açık → kapat → tıksız, sonunda birim
    sweax::CorrectiveEQ e2; e2.prepare (fs, 256, 1); e2.setEnabled (true, true);
    sweax::EqParams q; q.on = true; q.hpfOn = true; q.hpfHz = 120.0f; q.hpfSlope = 1; e2.setParams (q);
    auto y = sinus (fs, 2.0, 60.0, 0.4f);
    for (int i = 0; i < (int) y.size(); i += 256)
    {
        if (i == 24064) { q.hpfOn = false; e2.setParams (q); }
        float* ch[1] = { y.data() + i }; e2.process (ch, 1, std::min (256, (int) y.size() - i));
    }
    float maxFark2 = 0.0f;
    for (size_t i = 4800; i < y.size(); ++i) maxFark2 = std::max (maxFark2, std::fabs (y[i] - y[i - 1]));
    SWX_BEKLE (maxFark2 < 0.02f);                                      // 60 Hz doğal adım ≈ 0.0031; crossfade payı
    SWX_BEKLE_YAKIN (tepe (y, (int) fs * 18 / 10, (int) y.size()), 0.4f, 0.02f);
}

SWX_TEST (robust_nan_kurtarma_tum_moduller)
{
    auto CP = [] (const char* m) { if (std::getenv ("SWX_VERBOSE")) { std::printf ("      CP %s\n", m); std::fflush (stdout); } };
    const double fs = 48000.0;
    // her modüle: 1 blok NaN → ardından sinüs blokları → çıkış sonlu ve GR ~0'a döner
    auto nanBlok = [] (std::vector<float>& v) { v.assign (256, std::numeric_limits<float>::quiet_NaN()); };
    CP ("deesser");
    {
        sweax::DeEsser d; d.prepare (fs, 256, 1); d.setQuality (sweax::Quality::Yuksek); d.setEnabled (true, true);
        sweax::DeEssParams p; p.on = true; p.threshDb = -30.0f; p.rangeDb = 8.0f; d.setParams (p);
        std::vector<float> b; nanBlok (b); float* ch[1] = { b.data() };
        d.process (ch, 1, 256);
        auto s = sinus (fs, 0.5, 1000.0, 0.1f);
        for (int i = 0; i < (int) s.size(); i += 256) { float* c2[1] = { s.data() + i }; d.process (c2, 1, std::min (256, (int) s.size() - i)); }
        SWX_BEKLE (sonluSonra (s, d.latencySamples (sweax::Quality::Yuksek) + 512));
        SWX_BEKLE (d.gainReductionDb() > -1.0f);
    }
    CP ("eq");
    {
        sweax::CorrectiveEQ eq; eq.prepare (fs, 256, 1); eq.setEnabled (true, true);
        sweax::EqParams p; p.on = true; p.band[0].on = true; p.band[0].dyn = true; p.band[0].hz = 1000.0f; p.band[0].q = 2.0f;
        p.band[0].dynThreshDb = -30.0f; p.band[0].dynRangeDb = -12.0f; eq.setParams (p);
        std::vector<float> b; nanBlok (b); float* ch[1] = { b.data() };
        eq.process (ch, 1, 256);
        auto s = sinus (fs, 0.5, 1000.0, 0.01f);                       // eşik altı
        for (int i = 0; i < (int) s.size(); i += 256) { float* c2[1] = { s.data() + i }; eq.process (c2, 1, std::min (256, (int) s.size() - i)); }
        SWX_BEKLE (sonluSonra (s, 512));
        SWX_BEKLE (eq.bandGainReductionDb (0) > -1.0f);
    }
    CP ("leveler");
    {
        sweax::VocalLeveler l; l.prepare (fs, 480, 1); l.setQuality (sweax::Quality::Canli); l.setEnabled (true, true);
        sweax::LevelerParams p; p.on = true; l.setParams (p);
        std::vector<float> b (480, std::numeric_limits<float>::quiet_NaN()); float* ch[1] = { b.data() };
        l.process (ch, 1, 480);
        auto s = sinus (fs, 0.5, 220.0, 0.1f);
        for (int i = 0; i < (int) s.size(); i += 480) { float* c2[1] = { s.data() + i }; l.process (c2, 1, std::min (480, (int) s.size() - i)); }
        SWX_BEKLE (sonluSonra (s, 960));
    }
    CP ("output");
    {
        sweax::OutputStage o; o.prepare (fs, 256, 1); o.setEnabled (true, true);
        sweax::OutputParams p; p.ceilingDbTp = -0.3f; o.setParams (p);
        std::vector<float> b; nanBlok (b); float* ch[1] = { b.data() };
        o.process (ch, 1, 256);
        auto s = sinus (fs, 0.5, 1000.0, 0.25f);                       // tavan altı
        for (int i = 0; i < (int) s.size(); i += 256) { float* c2[1] = { s.data() + i }; o.process (c2, 1, std::min (256, (int) s.size() - i)); }
        SWX_BEKLE (sonluSonra (s, o.latencySamples (sweax::Quality::Canli) + 512));
        SWX_BEKLE (o.gainReductionDb() > -1.0f);                       // Inf/NaN kilidi yok, toparlandı
        SWX_BEKLE_YAKIN (tepe (s, (int) s.size() / 2, (int) s.size()), 0.25f, 0.01f);
    }
}
