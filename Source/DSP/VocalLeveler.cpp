// SWEAX NEXT — DSP/VocalLeveler.cpp
#include "VocalLeveler.h"
#include "../Chain/VocalChain.h"
#include <cmath>
#include <algorithm>

namespace sweax
{

void VocalLeveler::prepare (double sampleRate, int maxBlock, int numChannels)
{
    const int worst = (int) std::ceil (LOOKAHEAD_MS * 0.001 * sampleRate);
    prepareBase (sampleRate, maxBlock, numChannels, worst);
    for (auto& d : audioDelay) d.prepare (worst, 0);
    activeDelay = 0;
    kw.prepare (sr, 1);
    kbuf.assign ((size_t) std::max (1, maxBlock), 0.0f);
    subLen = std::max (1, (int) std::lround (SUB_MS * 0.001 * sr));
    pending = cur = LevelerParams {};
    paramsDirty = true;
    resetWet();
}

int VocalLeveler::latencySamples (Quality mode) const noexcept
{
    return mode == Quality::Canli ? 0 : (int) std::lround (LOOKAHEAD_MS * 0.001 * sr);
}

void VocalLeveler::setQuality (Quality mode) noexcept
{
    activeDelay = latencySamples (mode);
    for (auto& d : audioDelay) d.setDelay (activeDelay);
    setDryDelay (activeDelay);
    upDelaySubs = std::min ((int) upRing.size(), (int) std::ceil ((double) activeDelay / (double) subLen));
    upRing.fill (0.0f); upPos = 0;
}

void VocalLeveler::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    for (int i = 0; i < n; ++i)
        for (int k = 0; k < numCh; ++k) audioDelay[(size_t) k].processSample (ch[k][i]);
    curGainDb = targetGainDb = 0.0f; rampFromDb = rampToDb = 0.0f; rampPos = subLen;
}

void VocalLeveler::resetWet()
{
    for (auto& d : audioDelay) d.reset();
    kw.reset();
    subFill = 0; subAcc = 0.0;
    momRing.fill (0.0); momPos = 0; momCount = 0;
    holdRing.fill (-100.0f); holdPos = 0;
    minRing.fill (100.0f); minPos = 0; minCount = 0;
    momentary = phrase = floorLufs = -100.0f; haveFloor = false;
    hold = false; silentSubs = 0; inactiveSubs = 100;
    upRing.fill (0.0f); upPos = 0;
    targetGainDb = curGainDb = 0.0f; gainStepDb = 0.0f; rampFromDb = rampToDb = 0.0f; rampPos = subLen;
}

void VocalLeveler::subBlockUpdate() noexcept
{
    // momentary: son 20 alt bloğun ms ortalaması. NaN girişi halkayı zehirler → tespitte halka boşaltılır
    // (tek NaN bloğu kazancı süresiz NaN yapıyordu — inceleme).
    if (! std::isfinite (subAcc)) subAcc = 0.0;
    momRing[(size_t) momPos] = subAcc / std::max (1, subLen); momPos = (momPos + 1) % MOM_SUBS; momCount = std::min (momCount + 1, MOM_SUBS);
    subAcc = 0.0; subFill = 0;
    double ms = 0.0; for (int i = 0; i < momCount; ++i) ms += momRing[(size_t) i];
    momentary = LoudnessMeter::msToLufs (ms / std::max (1, momCount));
    if (! std::isfinite (momentary)) { momRing.fill (0.0); momCount = 0; momentary = -100.0f; }
    // gürültü tabanı: 4 s minimum + 2 dB; YÜKSELİŞ ≤ 3 dB/s (kesintisiz uzun vokalde 4 s minimum vokal seviyesine
    // tırmanıp kalıcı hold'a sokuyordu — inceleme bulgusu), düşüş anında
    minRing[(size_t) minPos] = momentary; minPos = (minPos + 1) % MIN_SUBS; minCount = std::min (minCount + 1, MIN_SUBS);
    float mn = 100.0f; for (int i = 0; i < minCount; ++i) mn = std::min (mn, minRing[(size_t) i]);
    if (minCount >= 10)
    {
        const float aday = mn + 2.0f;
        if (! haveFloor || aday < floorLufs) { floorLufs = aday; haveFloor = true; }
        else floorLufs = std::min (aday, floorLufs + (float) (3.0 * SUB_MS * 0.001));
    }
    else floorLufs = -100.0f;
    // VAD-lite: aktif = momentary > taban + 10 dB (taban yoksa > −60)
    const bool active = momentary > ((floorLufs > -99.0f) ? floorLufs + 10.0f : -60.0f);
    // cümle zarfı: aktif alt blokların 1.5 s tepe-tutması — YENİ CÜMLE (≥150 ms sessizlik sonrası etkinlik) zarfı
    // sıfırlar; aksi halde yüksek bir cümlenin tepesi sonraki sessiz cümleyi 1.5 s boyunca bastırır (ölçüldü: −5 dB).
    if (active)
    {
        if (inactiveSubs >= 7) holdRing.fill (-100.0f);
        inactiveSubs = 0;
        holdRing[(size_t) holdPos] = momentary; holdPos = (holdPos + 1) % HOLD_SUBS;
    }
    else ++inactiveSubs;
    float pk = -100.0f; for (float v : holdRing) pk = std::max (pk, v);
    phrase = pk;
    // hedef kazanç
    const double subSec = SUB_MS * 0.001;
    const float speedMul = cur.speed <= 0.5f ? 0.25f + 1.5f * cur.speed : 1.0f + 2.0f * (cur.speed - 0.5f);   // 0→0.25, 0.5→1, 1→2
    const float upMax = (float) (20.0 * subSec) * speedMul, downMax = (float) (60.0 * subSec) * speedMul;
    if (! cur.on) { targetGainDb = 0.0f; hold = false; }
    else if (active)
    {
        hold = false; silentSubs = 0;
        float want = cur.targetLufs - phrase;
        float maxUp = cur.maxUpDb;
        if (floorLufs > -99.0f) maxUp = std::min (maxUp, std::max (0.0f, (phrase - floorLufs) - 20.0f));  // SNR − 20
        want = std::clamp (want, -std::fabs (cur.maxDownDb), maxUp);
        targetGainDb = std::clamp (want, targetGainDb - downMax, targetGainDb + upMax);
    }
    else
    {
        hold = true;
        if (++silentSubs > (int) (1.0 / subSec))                              // > 1 s sessizlik → birime doğru 3 dB/s
        {
            const float step = (float) (3.0 * subSec);
            if (targetGainDb > step) targetGainDb -= step; else if (targetGainDb < -step) targetGainDb += step; else targetGainDb = 0.0f;
        }
    }
    // Lookahead yalnız AŞAĞI yönde öngörür: yükseliş, cümle çıkışa gerçekten ulaşana kadar (D) bekler — aksi halde
    // cümle öncesi boşluğun gürültüsü 150 ms erken yükseltilir (ölçüldü: boşlukta +1…2 dB). effective = min(şimdi, D önce).
    float effective = targetGainDb;
    if (upDelaySubs > 0)
    {
        upRing[(size_t) upPos] = targetGainDb; upPos = (upPos + 1) % upDelaySubs;
        effective = std::min (targetGainDb, upRing[(size_t) upPos]);       // en eski = D önceki hedef
    }
    // örnek başına rampa (bu alt blok boyunca)
    rampFromDb = curGainDb; rampToDb = effective; rampPos = 0;
    gainStepDb = (rampToDb - rampFromDb) / (float) subLen;
}

void VocalLeveler::processWet (float* const* ch, int numCh, int n) noexcept
{
    if (paramsDirty) { cur = pending; paramsDirty = false; }
    int done = 0;
    while (done < n)
    {
        const int m = std::min (n - done, (int) kbuf.size());
        // K-ağırlıklı mono (gecikmesiz)
        for (int i = 0; i < m; ++i) { float s = 0.0f; for (int k = 0; k < numCh; ++k) s += ch[k][done + i]; kbuf[(size_t) i] = s / (float) numCh; }
        float* kp[1] = { kbuf.data() };
        kw.process (kp, 1, m);
        for (int i = 0; i < m; ++i)
        {
            subAcc += (double) kbuf[(size_t) i] * kbuf[(size_t) i];
            if (++subFill >= subLen) subBlockUpdate();
            // kazanç ara değeri
            if (rampPos < subLen) { curGainDb = rampFromDb + gainStepDb * (float) (++rampPos); }
            const float g = std::pow (10.0f, curGainDb * 0.05f);
            for (int k = 0; k < numCh; ++k)
            {
                const float d = audioDelay[(size_t) k].processSample (ch[k][done + i]);
                ch[k][done + i] = d * g;
            }
        }
        done += m;
    }
    // sayısal güvenlik: K-filtre/akümülatör NaN görürse sıfırla (bir kez zehirlenen zarf leveler'ı kalıcı kapatıyordu)
    kw.sanitize();
    if (! std::isfinite (subAcc) || ! std::isfinite (curGainDb)) { subAcc = 0.0; subFill = 0; curGainDb = targetGainDb = 0.0f; rampPos = subLen; }
}

} // namespace sweax
