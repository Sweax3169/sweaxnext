// SWEAX NEXT — DSP/AirExciter.cpp
#include "AirExciter.h"

namespace sweax
{

void AirExciter::prepare (double sampleRate, int maxBlock, int numChannels)
{
    prepareBase (sampleRate, maxBlock, numChannels, 0);
    // denetim (M3.2): dusuk sr'da iki bant tek banda cokmesin — MID de korunur, HIGH en az 1.4x MID
    const double fMid = std::min ((double) MID_HZ, sr * 0.25);
    const double fHigh = std::max (fMid * 1.4, std::min ((double) HIGH_HZ, sr * 0.42));
    for (int k = 0; k < MAX_CH; ++k)
    {
        hpMid[(size_t) k].setCoeffs (design::highpass (sr, fMid, 0.7071));
        hpHigh[(size_t) k].setCoeffs (design::highpass (sr, fHigh, 0.7071));
    }
    // zarf katsayıları: atak 5 ms / bırakma 60 ms (tek kutup, |x| üzerinde)
    atkCoef = (float) std::exp (-1.0 / (0.005 * sr));
    relCoef = (float) std::exp (-1.0 / (0.060 * sr));
    midLift.prepare (sr, 15.0);
    highLift.prepare (sr, 15.0);
    midLift.setCurrentAndTarget (0.0f);
    highLift.setCurrentAndTarget (0.0f);
    bandBuf.assign ((size_t) MAX_CH * (size_t) maxBlockN * 2, 0.0f);
    resetWet();
}

void AirExciter::resetWet()
{
    for (auto& b : hpMid) b.reset();
    for (auto& b : hpHigh) b.reset();
    envFull.fill (0.0f); envMid.fill (0.0f); envHigh.fill (0.0f);
    midLift.setCurrentAndTarget (0.0f);
    highLift.setCurrentAndTarget (0.0f);
}

// bantları süz + zarfları güncelle (hem wet hem idle yolun ortak gövdesi)
void AirExciter::besle (const float* const* ch, int numCh, int n, float* const* bandMidOut, float* const* bandHighOut) noexcept
{
    for (int k = 0; k < numCh; ++k)
    {
        float eF = envFull[(size_t) k], eM = envMid[(size_t) k], eH = envHigh[(size_t) k];
        auto& fM = hpMid[(size_t) k];
        auto& fH = hpHigh[(size_t) k];
        for (int i = 0; i < n; ++i)
        {
            float x = ch[k][i];
            if (! (x > -1.0e6f && x < 1.0e6f)) x = 0.0f;     // denetim: tek NaN blogu zehirlemesin (Saturator standardi)
            const float bm = fM.processSample (x);
            const float bh = fH.processSample (x);
            if (bandMidOut != nullptr) bandMidOut[k][i] = bm;
            if (bandHighOut != nullptr) bandHighOut[k][i] = bh;
            const float ax = std::fabs (x), am = std::fabs (bm), ah = std::fabs (bh);
            eF = ax > eF ? ax + atkCoef * (eF - ax) : ax + relCoef * (eF - ax);
            eM = am > eM ? am + atkCoef * (eM - am) : am + relCoef * (eM - am);
            eH = ah > eH ? ah + atkCoef * (eH - ah) : ah + relCoef * (eH - ah);
        }
        // NaN sigortası: zarf zehirlendiyse filtre durumları da zehirlenmiştir — ikisini birden temizle
        // (girişteki tek NaN kalıcı olmasın; o bloğun çıkışını işlemcinin son savunması yakalar)
        if (! (std::isfinite (eF) && std::isfinite (eM) && std::isfinite (eH)))
        {
            eF = eM = eH = 0.0f;
            fM.sanitize();
            fH.sanitize();
        }
        envFull[(size_t) k] = eF; envMid[(size_t) k] = eM; envHigh[(size_t) k] = eH;
    }
}

void AirExciter::kaldirmayiGuncelle (int numCh) noexcept
{
    // kanal ortalaması (stereo görüntü bozulmasın diye tek ortak kaldırma) — denetim (M3.2): besle()
    // yalnız numCh kanalı günceller; ortalama da AYNI küme üzerinden alınır (chans değil), yoksa
    // donmuş zarf girdileri karışırdı.
    const int nc = std::clamp (numCh, 1, chans);
    float eF = 0.0f, eM = 0.0f, eH = 0.0f;
    for (int k = 0; k < nc; ++k) { eF += envFull[(size_t) k]; eM += envMid[(size_t) k]; eH += envHigh[(size_t) k]; }
    eF /= (float) nc; eM /= (float) nc; eH /= (float) nc;
    const float eps = 1.0e-9f;
    if (eF < 1.0e-6f)                                   // sessizlik: kaldırma hedefi 0'a insin
    {
        midLift.setTarget (0.0f);
        highLift.setTarget (0.0f);
        return;
    }
    // program-bağımlılık: bandın tam-banda LİNEER oranı referans oranla karşılaştırılır.
    // Oran ≤ referans (tipik/koyu ses) → kaldırma TAM; referansın 2 katında sıfır (denetim M3.2:
    // eski payda 1.75r diz noktasını 0.25r'a kaydırıyordu — ilan edilen +6/+9 dB tavanlara ancak
    // aşırı koyu malzemede ulaşılıyordu).
    auto agirlik = [eps] (float bant, float tam, float refOran)
    {
        const float oran = bant / (tam + eps);
        return juce::jlimit (0.0f, 1.0f, (2.0f * refOran - oran) / (refOran + eps));
    };
    const float wMid = agirlik (eM, eF, 0.25f);
    const float wHigh = agirlik (eH, eF, 0.12f);
    const float midDb = cur.mid * MID_MAX_DB * wMid;
    const float highDb = cur.high * HIGH_MAX_DB * wHigh;
    midLift.setTarget (midDb > 0.01f ? std::pow (10.0f, midDb / 20.0f) - 1.0f : 0.0f);
    highLift.setTarget (highDb > 0.01f ? std::pow (10.0f, highDb / 20.0f) - 1.0f : 0.0f);
}

void AirExciter::processWet (float* const* ch, int numCh, int n) noexcept
{
    cur = pending;
    float* bm[MAX_CH]; float* bh[MAX_CH];
    for (int k = 0; k < MAX_CH; ++k)
    {
        bm[k] = bandBuf.data() + (size_t) k * (size_t) maxBlockN;
        bh[k] = bandBuf.data() + (size_t) (MAX_CH + k) * (size_t) maxBlockN;
    }
    besle (ch, numCh, n, bm, bh);
    kaldirmayiGuncelle (numCh);                          // blok başına hedef; smoother zipper'ı önler
    for (int i = 0; i < n; ++i)
    {
        const float gM = midLift.next();
        const float gH = highLift.next();
        for (int k = 0; k < numCh; ++k)
            ch[k][i] += gM * bm[k][i] + gH * bh[k][i];
    }
}

void AirExciter::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    besle (ch, numCh, n, nullptr, nullptr);              // filtre + zarf durumu ısınmış kalır
    midLift.setCurrentAndTarget (0.0f);
    highLift.setCurrentAndTarget (0.0f);
}

} // namespace sweax
