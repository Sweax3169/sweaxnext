// SWEAX NEXT — DSP/GlowModule.cpp
#include "GlowModule.h"

namespace sweax
{

namespace
{
    // Araştırma tabanlı bant tabloları (RMS dBFS; zincirde leveler+comp sonrası nominal ≈ −18..−22)
    constexpr float THR_DOWN[GlowModule::BANT] = { -25.0f, -23.0f, -27.0f };   // korpus hizalamasi: eski esikler nominal RMS altinda surekli GR uretiyordu (-3.3 dB olculdu)
    constexpr float RATIO_DOWN[GlowModule::BANT] = { 8.0f, 8.0f, 12.0f };      // high'ta ∞ yok (ess sigortası)
    constexpr float THR_UP[GlowModule::BANT] = { -38.0f, -40.0f, -42.0f };
    constexpr float RATIO_UP[GlowModule::BANT] = { 3.0f, 4.0f, 3.0f };
    constexpr float ATK_MS[GlowModule::BANT] = { 45.0f, 22.0f, 14.0f };        // Ableton OTT preset süreleri
    constexpr float REL_MS[GlowModule::BANT] = { 280.0f, 280.0f, 130.0f };
    constexpr float MAKYAJ_DB[GlowModule::BANT] = { 1.0f, 0.5f, 1.5f };   // es-ses icin inceltildi (korpus olcumu)        // gülümseme eğrisi (rapor)
    constexpr float UP_TAVAN_DB = 10.0f;                                       // Vital 30 dB — vokalde tehlikeli
    constexpr float SESSIZ_DB = -55.0f, SESSIZ_GECIS_DB = 5.0f;                // altında yukarı kaldırma yok
    constexpr float KNEE_DB = 6.0f;
    constexpr float ES_SES_TELAFI = 0.9441f;   /* -0.5 dB */                                   // −1.5 dB (wet yoluna)
    constexpr int KONTROL_N = 128;                                             // kazanç güncelleme alt-parçası

    // soft-knee aşım (dB): x = seviyenin eşiğe göre konumu (+ = eşik üstü); dönüş: etkin aşım ≥ 0
    inline float dizAsim (float x) noexcept
    {
        if (x <= -KNEE_DB * 0.5f) return 0.0f;
        if (x >= KNEE_DB * 0.5f) return x;
        const float t = x + KNEE_DB * 0.5f;
        return t * t / (2.0f * KNEE_DB);
    }
}

void GlowModule::prepare (double sampleRate, int maxBlock, int numChannels)
{
    prepareBase (sampleRate, maxBlock, numChannels, 0);
    srSaklanan = sampleRate;
    for (auto& b : bolucu) b.hazirla (sr);
    rmsCoef = (float) std::exp (-1.0 / (0.015 * sr));      // 15 ms RMS tabanı (A/R bunun üstünde)
    sonTimeMul = -1.0f;                                    // katsayılar ilk processWet'te kurulur
    for (auto& k : kazanc) { k.prepare (sr, 10.0); k.setCurrentAndTarget (1.0f); }
    bantBuf.assign ((size_t) BANT * (size_t) MAX_CH * (size_t) maxBlockN, 0.0f);
    resetWet();
}

void GlowModule::resetWet()
{
    for (auto& b : bolucu) b.sifirla();
    env.fill (0.0f);
    sonKazancDb.fill (0.0f);
    for (auto& k : kazanc) k.setCurrentAndTarget (1.0f);
}

void GlowModule::zarflariGuncelle (int numCh) noexcept
{
    juce::ignoreUnused (numCh);
    if (cur.timeMul != sonTimeMul)
    {
        sonTimeMul = cur.timeMul;
        const float m = juce::jlimit (0.33f, 3.0f, cur.timeMul);
        for (int b = 0; b < BANT; ++b)
        {
            atk[(size_t) b] = (float) std::exp (-1.0 / (ATK_MS[b] * 0.001 * (double) m * srSaklanan));
            rel[(size_t) b] = (float) std::exp (-1.0 / (REL_MS[b] * 0.001 * (double) m * srSaklanan));
        }
    }
}

void GlowModule::processWet (float* const* ch, int numCh, int n) noexcept
{
    cur = pending;
    zarflariGuncelle (numCh);
    const float depth = juce::jlimit (0.0f, 1.0f, cur.depth);
    const int nc = std::min (numCh, chans);

    // bantlara böl (tam blok) — bantBuf düzeni: [bant][kanal][örnek]
    auto* bb = bantBuf.data();
    auto bandPtr = [&] (int b, int k) { return bb + ((size_t) b * (size_t) MAX_CH + (size_t) k) * (size_t) maxBlockN; };
    for (int k = 0; k < nc; ++k)
    {
        auto& bl = bolucu[(size_t) k];
        float* lo = bandPtr (0, k);
        float* mi = bandPtr (1, k);
        float* hi = bandPtr (2, k);
        for (int i = 0; i < n; ++i)
        {
            float x = ch[k][i];
            if (! (x > -1.0e6f && x < 1.0e6f)) x = 0.0f;                       // NaN örnek kapısı
            bl.bol (x, lo[i], mi[i], hi[i]);
        }
        bl.temizle();                                                          // durum sigortası (ucuz)
    }

    // kontrol alt-parçaları: zarf (örnek başına) + kazanç hedefi (KONTROL_N'de bir) + karışım
    for (int base = 0; base < n; base += KONTROL_N)
    {
        const int m = std::min (KONTROL_N, n - base);
        for (int b = 0; b < BANT; ++b)
        {
            float e = env[(size_t) b];
            const float a = atk[(size_t) b], r2 = rel[(size_t) b];
            for (int i = 0; i < m; ++i)
            {
                float guc = 0.0f;
                for (int k = 0; k < nc; ++k) { const float v = bandPtr (b, k)[base + i]; guc += v * v; }
                guc /= (float) nc;
                const float rc = guc > e ? (a < rmsCoef ? a : rmsCoef) : r2;   // atak hızlı (≤ RMS tabanı), bırakma bant süresi
                e = guc + rc * (e - guc);
            }
            if (! std::isfinite (e)) e = 0.0f;
            env[(size_t) b] = e;

            // kazanç hedefi (dB): aşağı + yukarı + makyaj; sessizlik tabanı yukarıyı söndürür
            const float envDb = 10.0f * std::log10 (std::max (e, 1.0e-12f));
            const float gDown = -dizAsim (envDb - THR_DOWN[b]) * (1.0f - 1.0f / RATIO_DOWN[b]);
            float gUp = std::min (UP_TAVAN_DB, dizAsim (THR_UP[b] - envDb) * (1.0f - 1.0f / RATIO_UP[b]));
            gUp *= juce::jlimit (0.0f, 1.0f, (envDb - SESSIZ_DB) / SESSIZ_GECIS_DB);
            const float gDb = gDown + gUp + MAKYAJ_DB[b];
            sonKazancDb[(size_t) b] = gDb;
            kazanc[(size_t) b].setTarget (std::pow (10.0f, gDb / 20.0f));
        }
        // paralel karışım BANT ALANINDA (ölçüldü: kuru girişle karışım, LR bölmenin allpass fazıyla
        // TARAK İPTALİ yapıyordu — Temiz %12 derinlikte bile −1.3 dB kayıp. Kuru eşdeğeri = kazançsız
        // bant toplamı; karışım bant kazancına taşınır: g' = 1 + depth·(g·telafi − 1) — faz tutarlı.)
        for (int i = 0; i < m; ++i)
        {
            const float g0 = 1.0f + depth * (kazanc[0].next() * ES_SES_TELAFI - 1.0f);
            const float g1 = 1.0f + depth * (kazanc[1].next() * ES_SES_TELAFI - 1.0f);
            const float g2 = 1.0f + depth * (kazanc[2].next() * ES_SES_TELAFI - 1.0f);
            for (int k = 0; k < nc; ++k)
            {
                const int ix = base + i;
                ch[k][ix] = bandPtr (0, k)[ix] * g0 + bandPtr (1, k)[ix] * g1 + bandPtr (2, k)[ix] * g2;
            }
        }
    }
}

void GlowModule::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    // filtre + zarf durumu ısınmış kalır (yeniden açılış bayat/patlamalı olmaz); kazançlar nötre süzülür
    const int nc = std::min (numCh, chans);
    for (int k = 0; k < nc; ++k)
    {
        auto& bl = bolucu[(size_t) k];
        for (int i = 0; i < n; ++i)
        {
            float x = ch[k][i];
            if (! (x > -1.0e6f && x < 1.0e6f)) x = 0.0f;
            float lo, mi, hi;
            bl.bol (x, lo, mi, hi);
            if (k == 0)
            {
                const float rc0 = rmsCoef;
                float* e = env.data();
                const float g0 = lo * lo, g1 = mi * mi, g2 = hi * hi;
                e[0] = g0 + rc0 * (e[0] - g0);
                e[1] = g1 + rc0 * (e[1] - g1);
                e[2] = g2 + rc0 * (e[2] - g2);
            }
        }
        bl.temizle();
    }
    for (int b = 0; b < BANT; ++b)
        if (! std::isfinite (env[(size_t) b])) env[(size_t) b] = 0.0f;
    for (auto& k : kazanc) k.setCurrentAndTarget (1.0f);
}

} // namespace sweax
