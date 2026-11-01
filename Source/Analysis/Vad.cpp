// SWEAX NEXT — Analysis/Vad.cpp
#include "Vad.h"
#include <algorithm>
#include <cmath>

namespace sweax
{

void Vad::prepare (double sampleRate, int hopSize)
{
    sr = sampleRate; hop = std::max (1, hopSize); hopSec = hop / sr;
    subLen = std::max (1, (int) std::lround (0.20 / hopSec));            // 200 ms alt pencere (×20 = 4 s)
    sentenceGapFrames = std::max (1, (int) std::lround (0.15 / hopSec));  // ≥150 ms boşluk cümleyi böler
    minSib = std::max (1, (int) std::lround (0.02 / hopSec));  maxSib = std::max (minSib, (int) std::lround (0.40 / hopSec));
    minBre = std::max (1, (int) std::lround (0.08 / hopSec));  maxBre = std::max (minBre, (int) std::lround (0.60 / hopSec));
    maxPlo = std::max (1, (int) std::lround (0.08 / hopSec));
    reset();
}

void Vad::reset()
{
    smoothDb = -120.0f; haveSmooth = false;
    for (auto& m : subMin) m = 1000.0f;
    subIdx = 0; subFill = 0; subFrames = 0; floorDb = -120.0f; floorValid = false; haveFloor = false;
    floorMinDb = 1000.0f; tabanKosu = 0; destekAdayDb = 1000.0f;
    iz2.reset(); dijitalKare = 0; yakinKare = 0;
    for (auto& h : hist) h = 0; histTotal = 0;
    lfBaseDb = -60.0f; haveLfBase = false;
    sibCenterHz = 6000.0f; sibCenterAcc = 0.0; sibEvents = 0;
    roomFrames = 0; voicedFrames = 0; sentences = 0;
    sib = plo = bre = sen = Run {};
    sentenceGap = 0; lastFrame = -1; prevCentroid = 0.0f;
}

// minimum-istatistik: yumuşatılmış enerji (≈100 ms), SUBWIN×150 ms alt pencere minimumlarının minimumu + 2 dB sapma payı
void Vad::updateFloor (float energyDb) noexcept
{
    const float a = (float) std::min (1.0, hopSec / 0.1);
    if (! haveSmooth) { smoothDb = energyDb; haveSmooth = true; } else smoothDb += a * (energyDb - smoothDb);
    subMin[subIdx] = std::min (subMin[subIdx], smoothDb);
    if (++subFrames >= subLen)
    {
        subFrames = 0; subIdx = (subIdx + 1) % SUBWIN; subMin[subIdx] = 1000.0f; subFill = std::min (subFill + 1, SUBWIN);
    }
    // oturum histogramı (yumuşatılmış enerji, 1 dB kutu)
    {
        const int bin = std::clamp ((int) std::lround (smoothDb) + 120, 0, HIST_BINS - 1);
        ++hist[bin]; ++histTotal;
    }
    float aday;
    if (subFill >= 1 || subFrames >= subLen / 2)                        // ≥ ~100 ms geçmiş
    {
        float m = 1000.0f;
        for (int i = 0; i < SUBWIN; ++i) m = std::min (m, subMin[i]);
        // 5. yüzdelik
        uint32_t acc = 0; float pct = smoothDb;
        for (int i = 0; i < HIST_BINS; ++i) { acc += hist[i]; if (acc * 20 >= histTotal) { pct = (float) (i - 120); break; } }
        aday = std::min (m, pct) + 2.0f;
        floorValid = true;
    }
    else { aday = smoothDb; floorValid = false; }
    if (! haveFloor || aday < floorDb) { floorDb = aday; haveFloor = true; }
    else floorDb = std::min (aday, floorDb + (float) (3.0 * hopSec));  // yükseliş ≤ 3 dB/s
    if (floorValid && smoothDb > -90.0f) floorMinDb = std::min (floorMinDb, floorDb);
    if (energyDb <= -110.0f) ++dijitalKare;
    else
    {
        iz2.update (energyDb, hopSec, subLen);
        if (iz2.valid && iz2.smooth <= iz2.floor + 6.0f) ++yakinKare;
    }
}

// kalibre sibilant bandı: [0.7 f_s, 1.6 f_s] enerjisi / [1 kHz, 0.7 f_s] enerjisi (dB), bantlar üzerinden
float Vad::sibilantRatio (const FrameFeatures& f) const noexcept
{
    double hiE = 0.0, loE = 0.0;
    const double fLo = 1000.0, fMid = 0.7 * sibCenterHz, fHi = 1.6 * sibCenterHz;
    // bant merkezleri: 20 Hz'den 1/12 oktav (LogBands ile aynı ızgara)
    double fc = 20.0 * std::pow (2.0, 1.0 / 24.0);
    for (int b = 0; b < f.numBands; ++b, fc *= std::pow (2.0, 1.0 / 12.0))
    {
        const double e = std::pow (10.0, f.bandsDb[b] / 10.0);
        if (fc >= fLo && fc < fMid) loE += e; else if (fc >= fMid && fc < fHi) hiE += e;
    }
    return (float) (10.0 * std::log10 (std::max (hiE, 1e-14) / std::max (loE, 1e-14)));
}

// 3.5–12 kHz aralığında enerji ağırlık merkezi
float Vad::sibilantCentroid (const FrameFeatures& f) const noexcept
{
    double w = 0.0, s = 0.0; double fc = 20.0 * std::pow (2.0, 1.0 / 24.0);
    for (int b = 0; b < f.numBands; ++b, fc *= std::pow (2.0, 1.0 / 12.0))
    {
        if (fc < 3500.0 || fc > 12000.0) continue;
        const double e = std::pow (10.0, f.bandsDb[b] / 10.0); w += e * fc; s += e;
    }
    return s > 1e-14 ? (float) (w / s) : sibCenterHz;
}

void Vad::closeRun (Run& r, VadEventType type, long frame, int minFrames, int maxFrames, const std::function<void (const VadEvent&)>& onEvent)
{
    if (! r.active) return;
    const int len = (int) (r.last - r.start + 1);
    const bool ok = len >= minFrames && len <= maxFrames;
    if (ok)
    {
        VadEvent e { type, r.start, r.last, r.startSec, r.endSec, r.peakDb,
                     (float) (r.centerW > 0.0 ? r.centerAcc / r.centerW : 0.0), r.voiced };
        if (type == VadEventType::Sibilant)
        {
            // kalibrasyon: ilk 10 olayın ağırlık merkezi
            if (sibEvents < 10) { sibCenterAcc += e.centerHz; ++sibEvents; sibCenterHz = (float) (sibCenterAcc / sibEvents); }
            else ++sibEvents;
        }
        if (type == VadEventType::Sentence) ++sentences;
        if (onEvent) onEvent (e);
    }
    (void) frame;
    r = Run {};
}

VadFrame Vad::process (const FrameFeatures& f, const PitchTracker::Result& pitch, const std::function<void (const VadEvent&)>& onEvent)
{
    VadFrame o; o.frame = f.frame; o.pitchHz = pitch.hz; o.periodicity = pitch.periodicity;
    updateFloor (f.energyDb);
    o.noiseFloorDb = floorDb;
    o.snrDb = f.energyDb - floorDb;

    const bool silence = f.energyDb < -75.0f || (floorValid && f.energyDb < floorDb - 3.0f);
    const bool room = ! silence && floorValid && o.snrDb <= 6.0f;
    const bool loud = o.snrDb > 10.0f && ! silence;
    const bool voiced = loud && pitch.voiced && pitch.periodicity > 0.85f;
    const bool tonal = voiced || (o.snrDb > 20.0f && f.flatness < 0.4f && ! silence);
    // taban DESTEĞİ (M3.4 T5): ≥ 200 ms kesintisiz sesli-olmayan koşu tabanın +6 dB içinde kaldıysa taban ölçülmüştür;
    // hiç yoksa (baştan sona konuşma) taban yalnız üst sınırdır
    // (sürekli sinyalde taban sinyalin kendisine eşitlenir ve her kare "sesli değil" görünür → ayrıca koşu kaydın gür
    // kısmının (oturum histogramı p95) en az 20 dB altında olmalı)
    // (dijital ön-rulo bayrak izleyicisini −120'de tuttuğunda koşu ikinci izleyicinin tabanına göre değerlendirilir)
    const bool iz2Ref = dijitalKare > 0 && iz2.valid;
    const float refTaban = iz2Ref ? iz2.floor : floorDb;
    if (! voiced && (floorValid || iz2Ref) && smoothDb > -90.0f && smoothDb <= refTaban + 6.0f)
    {
        if ((double) (++tabanKosu) * hopSec >= 0.2) destekAdayDb = std::min (destekAdayDb, smoothDb);
    }
    else tabanKosu = 0;

    // sibilant: kalibre bant oranı + centroid + düzlük; voiced ile birlikte olabilir (/z/)
    o.sibilantRatioDb = sibilantRatio (f);
    const bool sibilant = loud && o.sibilantRatioDb > 6.0f && f.centroidHz > 3500.0f && f.flatness > 0.1f
                          && f.hfRatio > 0.5f;

    // plosive: LF zarf sıçraması ≥ +8 dB sesli LF taban çizgisine göre, geçici, LF harmonik yapısı yok
    if (voiced && f.lfEnvDb > -90.0f)
    {
        // taban çizgisi: sesli karelerde yavaş izleyici (yaklaşık medyan)
        if (! haveLfBase) { lfBaseDb = f.lfEnvDb; haveLfBase = true; }
        else lfBaseDb += (f.lfEnvDb > lfBaseDb ? 0.05f : -0.05f) * std::min (3.0f, std::fabs (f.lfEnvDb - lfBaseDb));
    }
    o.lfJumpDb = haveLfBase ? f.lfEnvDb - lfBaseDb : 0.0f;
    const bool lfDominant = f.lfEnvDb - f.energyDb > -3.0f;                 // LF, karenin enerjisini domine ediyor
    // LF (<150 Hz) enerjisi orta banda (300 Hz–3 kHz) göre: gerçek "pop"ta LF ≫ orta; düşük nota başlangıcında
    // harmonikler/formantlar orta bandı aynı anda doldurur → plosive değil (ölçüldü: 140 Hz ton onset'i 6 sahte plosive verdi)
    // bant indeksleri (1/12 okt, 20 Hz'den): 100 Hz ≈ 28, 150 Hz ≈ 35, 200 Hz ≈ 40, 300 Hz ≈ 47, 3 kHz ≈ 87
    double subE = 0.0, lowE = 0.0, lfE = 0.0, midE = 0.0;
    for (int b = 0; b < f.numBands; ++b)
    {
        const double e = std::pow (10.0, f.bandsDb[b] / 10.0);
        if (b < 28) subE += e; else if (b < 40) lowE += e;
        if (b < 35) lfE += e; else if (b >= 47 && b < 88) midE += e;
    }
    const bool lfOverMid = 10.0 * std::log10 (std::max (lfE, 1e-14) / std::max (midE, 1e-14)) > 3.0;
    // gerçek "pop" 20–100 Hz'de yoğunlaşır; 100–200 Hz'lik temel (düşük nota) sub-bass'i doldurmaz
    const bool subBass = 10.0 * std::log10 (std::max (subE, 1e-14) / std::max (lowE, 1e-14)) > 3.0;
    // F0 bandı LF zarfını açıklıyorsa (sesli, F0 < 150 Hz) → harmonik, plosive değil
    bool f0Explains = false;
    if (voiced && pitch.hz > 20.0f && pitch.hz < 150.0f)
    {
        const int fb = (int) std::floor (12.0 * std::log2 (pitch.hz / 20.0));
        if (fb >= 0 && fb < f.numBands) f0Explains = f.bandsDb[fb] > f.lfEnvDb - 8.0f;
    }
    const bool harmonicLf = (voiced && pitch.hz < 160.0f && ! lfDominant) || f0Explains;
    const bool plosive = haveLfBase && o.lfJumpDb >= 8.0f && (f.flux > 0.2f || lfDominant) && lfOverMid && subBass && ! harmonicLf && ! silence;

    // breath: düşük-orta enerji, düzlük yüksek, HF oranı orta, voiced değil
    const bool breath = ! voiced && ! silence && floorValid && o.snrDb > 6.0f && o.snrDb < 25.0f
                        && f.flatness > 0.15f && f.hfRatio > 0.05f && f.hfRatio < 0.7f && ! sibilant;

    if (silence) o.flags |= VAD_SILENCE;
    if (room) { o.flags |= VAD_ROOMTONE; ++roomFrames; }
    if (voiced) { o.flags |= VAD_VOICED; ++voicedFrames; }
    if (tonal) o.flags |= VAD_TONAL;
    if (sibilant) o.flags |= VAD_SIBILANT;
    if (plosive) o.flags |= VAD_PLOSIVE;
    if (breath) o.flags |= VAD_BREATH;

    // koşular / olaylar
    auto step = [&] (Run& r, bool on, VadEventType type, int minF, int maxF, float centerHz)
    {
        if (on)
        {
            if (! r.active) { r.active = true; r.start = f.frame; r.startSec = f.timeSec - hopSec; r.peakDb = -200.0f; r.centerAcc = 0.0; r.centerW = 0.0; r.voiced = 0; }
            r.last = f.frame; r.endSec = f.timeSec; r.peakDb = std::max (r.peakDb, f.energyDb);
            const double w = std::pow (10.0, f.energyDb / 10.0); r.centerAcc += centerHz * w; r.centerW += w;
            if (voiced) ++r.voiced;
        }
        else closeRun (r, type, f.frame, minF, maxF, onEvent);
    };
    step (sib, sibilant, VadEventType::Sibilant, minSib, maxSib, sibilantCentroid (f));
    // plosive koşusu düşük bir SESLİ notaya kapanıyorsa bu bir nota başlangıcı artefaktıdır → iptal (olay yok).
    // Bilinen sınır: hemen ardından düşük nota gelen gerçek bir "p" de sayılmaz (kaçırma, yanlış alarm yerine tercih).
    if (plo.active && ! plosive && voiced && pitch.hz < 150.0f) plo = Run {};
    step (plo, plosive, VadEventType::Plosive, 1, maxPlo, 0.0f);
    step (bre, breath, VadEventType::Breath, minBre, maxBre, 0.0f);
    // cümle: voiced koşusu; ≥ sentenceGapFrames voiced-olmayan kare kapatır
    if (voiced)
    {
        if (! sen.active) { sen.active = true; sen.start = f.frame; sen.startSec = f.timeSec - hopSec; sen.peakDb = -200.0f; sen.voiced = 0; sen.centerAcc = 0.0; sen.centerW = 0.0; }
        sen.last = f.frame; sen.endSec = f.timeSec; sen.peakDb = std::max (sen.peakDb, f.energyDb); ++sen.voiced; sentenceGap = 0;
    }
    else if (sen.active)
    {
        if (++sentenceGap >= sentenceGapFrames) closeRun (sen, VadEventType::Sentence, f.frame, 2, 1 << 30, onEvent);
    }
    lastFrame = f.frame; prevCentroid = f.centroidHz;
    return o;
}

void Vad::flush (const std::function<void (const VadEvent&)>& onEvent)
{
    closeRun (sib, VadEventType::Sibilant, lastFrame, minSib, maxSib, onEvent);
    closeRun (plo, VadEventType::Plosive, lastFrame, 1, maxPlo, onEvent);
    closeRun (bre, VadEventType::Breath, lastFrame, minBre, maxBre, onEvent);
    closeRun (sen, VadEventType::Sentence, lastFrame, 2, 1 << 30, onEvent);
}

} // namespace sweax
