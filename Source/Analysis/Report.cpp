// SWEAX NEXT — Analysis/Report.cpp
#include "Report.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <array>

namespace sweax
{

namespace
{
    float ramp (double x, double x0, double x1) noexcept          // x0→0, x1→1, doğrusal, kırpılmış
    {
        if (x1 <= x0) return x >= x1 ? 1.0f : 0.0f;
        return (float) std::clamp ((x - x0) / (x1 - x0), 0.0, 1.0);
    }
    float percentile (std::vector<float>& v, double p)
    {
        if (v.empty()) return 0.0f;
        std::sort (v.begin(), v.end());
        const double idx = p * (double) (v.size() - 1);
        const size_t lo = (size_t) std::floor (idx), hi = std::min (v.size() - 1, lo + 1);
        const double fr = idx - (double) lo;
        return (float) ((1.0 - fr) * v[lo] + fr * v[hi]);
    }
    float median (std::vector<float>& v) { return percentile (v, 0.5); }
    double lufsToMs (float l) { return std::pow (10.0, ((double) l + 0.691) / 10.0); }
    double hopSecOf (const std::vector<SegmentAnalysis>& segs)
    {
        for (const auto& s : segs) if (s.frames.size() >= 2) return (double) (s.frames[1].timeSec - s.frames[0].timeSec);
        return 0.0213;
    }

    // LTAS: tonal karelerin bant dB ortalaması (+ std); half: 0 = hepsi, 1/2 = CÜMLE ÇİFTİ PARİTESİ ile iki yarı
    // (cümle 0,1 → yarı 1; 2,3 → yarı 2; …): cümle düzeyinde çapraz doğrulama — ilk/ikinci zaman yarısı, sıralı
    // değişen cümle tipleri ya da yavaş eğilimlerle yanlı olur (ölçüldü: dönüşümlü iki cümle tipi 3.5 dB fark verdi).
    void ltas (const std::vector<SegmentAnalysis>& segs, int half, float* meanOut, float* stdOut, int& numBands, int& count)
    {
        int nb = 0; for (const auto& s : segs) for (const auto& f : s.frames) { nb = std::max (nb, (int) f.numBands); }
        numBands = nb;
        std::vector<double> sum ((size_t) nb, 0.0), sq ((size_t) nb, 0.0), sqDb ((size_t) nb, 0.0);
        count = 0;
        int sid = 0;                                            // küresel cümle sayacı (segmentler boyunca)
        for (const auto& s : segs)
        {
            // cümle aralıkları (kare indeksleriyle), başlangıca göre sıralı
            std::vector<std::pair<long, long>> cumleler;
            for (const auto& e : s.events) if (e.type == VadEventType::Sentence) cumleler.push_back ({ e.startFrame, e.endFrame });
            std::sort (cumleler.begin(), cumleler.end());
            size_t ci = 0;
            for (const auto& f : s.frames)
            {
                const long k = (long) (&f - s.frames.data());
                while (ci < cumleler.size() && k > cumleler[ci].second) { ++ci; ++sid; }
                if (! (f.flags & VAD_TONAL)) continue;
                const int myHalf = ((sid / 2) % 2 == 0) ? 1 : 2;
                if (half != 0 && myHalf != half) continue;
                ++count;
                // GÜÇ ortalaması (ms), dB'ye sonra çevrilir: kulağın entegre ettiği tonal resim; dB-ortalaması sessiz
                // kareleri aşırı ağırlıklandırır (ölçüldü: dönüşümlü iki nota ile temel bant −28 yerine −59 dB çıktı).
                // std ise dB alanında (karelerin bant dB dağılımı).
                for (int b = 0; b < (int) f.numBands; ++b)
                {
                    sum[(size_t) b] += std::pow (10.0, f.bandsDb[b] / 10.0);
                    sq[(size_t) b] += (double) f.bandsDb[b]; sqDb[(size_t) b] += (double) f.bandsDb[b] * f.bandsDb[b];
                }
            }
            sid += (int) (cumleler.size() - std::min (ci, cumleler.size()));
        }
        for (int b = 0; b < nb; ++b)
        {
            const double m = count > 0 ? 10.0 * std::log10 (std::max (sum[(size_t) b] / count, 1.0e-12)) : -120.0;
            if (meanOut) meanOut[b] = (float) m;
            const double mDb = count > 0 ? sq[(size_t) b] / count : 0.0;
            if (stdOut) stdOut[b] = count > 1 ? (float) std::sqrt (std::max (0.0, sqDb[(size_t) b] / count - mDb * mDb)) : 0.0f;
        }
    }
    float ltasHalfDiff (const std::vector<SegmentAnalysis>& segs)
    {
        float a[FrameFeatures::MAX_BANDS], b[FrameFeatures::MAX_BANDS]; int nb = 0, ca = 0, cb = 0;
        ltas (segs, 1, a, nullptr, nb, ca); ltas (segs, 2, b, nullptr, nb, cb);
        if (ca < 10 || cb < 10) return 99.0f;
        // 200 Hz – 8 kHz bantları (1/12 okt, 20 Hz'den): band ≈ 12·log2(f/20) → 200 Hz ≈ 40, 8 kHz ≈ 104.
        // Kararlılık ölçüsü 1/3 oktav (4 bant) yumuşatılmış LTAS üzerinden: seyrek harmonik yapı (melodi değişimi)
        // 1/12 oktavda bant bant kayar, tonal RESİM ise 1/3 oktavda karşılaştırılmalıdır.
        auto smooth = [nb] (const float* x, int k) -> double
        {
            double s = 0.0; int n = 0;
            for (int j = k - 2; j <= k + 1; ++j) if (j >= 0 && j < nb) { s += std::pow (10.0, x[j] / 10.0); ++n; }
            return 10.0 * std::log10 (std::max (s / std::max (1, n), 1.0e-12));
        };
        double acc = 0.0; int n = 0;
        for (int k = 40; k < std::min (nb, 104); ++k) { const double d = smooth (a, k) - smooth (b, k); acc += d * d; ++n; }
        return n > 0 ? (float) std::sqrt (acc / n) : 99.0f;
    }
}

DetectorConfidence computeConfidence (const std::vector<SegmentAnalysis>& segs, double hopSec)
{
    DetectorConfidence c;
    double voiced = 0.0, room = 0.0; int sentences = 0;
    std::vector<float> roomE;
    for (const auto& s : segs)
    {
        voiced += s.vad.voicedSeconds(); room += s.vad.roomToneSeconds(); sentences += s.vad.sentenceCount();
        for (const auto& f : s.frames) if (f.flags & VAD_ROOMTONE) roomE.push_back (f.energyDb);
    }
    (void) hopSec;
    c.ceiling = voiced >= 90.0;
    c.level = ramp (voiced, 0.0, 6.0) * ramp (sentences, 0.0, 3.0);
    float roomStd = 0.0f;
    if (roomE.size() > 4)
    {
        // denetim M3.4 (PT-2): taban oturmadan 'oda' sayılan yüksek kareler std'yi 6 dB'nin üstüne itip gürültü
        // güvenini SIFIRLIYORDU (−58 dBFS gürültülü kayıt temiz sanılıyordu) → sağlam yayılım 1.4826·MAD
        std::vector<float> srt (roomE);
        std::sort (srt.begin(), srt.end());
        const float med = srt[srt.size() / 2];
        for (auto& e : srt) e = std::fabs (e - med);
        std::sort (srt.begin(), srt.end());
        roomStd = 1.4826f * srt[srt.size() / 2];
    }
    c.noise = (room < 0.5 ? ramp (room, 0.0, 0.5) * 0.5f : 0.5f + 0.5f * ramp (room, 0.5, 2.0)) * (roomE.size() > 4 ? ramp (6.0 - roomStd, 0.0, 3.0) : 1.0f);
    const float stab = ramp (3.0 - ltasHalfDiff (segs), 0.0, 3.0);
    c.tonal = ramp (voiced, 0.0, 8.0) * ramp (sentences, 0.0, 4.0) * stab;
    c.dynamics = ramp (sentences, 0.0, 5.0) * ramp (voiced, 0.0, 10.0);
    c.sibilance = ramp (voiced, 0.0, 12.0) * ramp (sentences, 0.0, 5.0);
    return c;
}

AnalysisReport buildReportCore (const std::vector<SegmentAnalysis>& segs, const float* storeData, int64_t storeLen, double srIn)
{
    AnalysisReport r;
    r.capturedSec = srIn > 0.0 ? (double) storeLen / srIn : 0.0;
    r.segments = (int) segs.size();
    {
        LoudnessMeter lm; lm.prepare (srIn, 1, 8192);
        for (int64_t i = 0; i < storeLen; i += 8192) { const float* ch[1] = { storeData + i }; lm.process (ch, 1, (int) std::min<int64_t> (8192, storeLen - i)); }
        r.integratedLufs = lm.integratedLufs(); r.truePeakDb = lm.truePeakDb(); r.maxMomentaryLufs = lm.maxMomentaryLufs();
    }

    std::vector<float> voicedMom, voicedE, crest, slope, centroid, flat, hf, f0;
    std::array<double, 12> pcHist {};                                            // gam algilama (M3.1)
    float noiseFloor = 1000.0f; bool nfKnown = false; int nfCount = 0; double nfYedek = 0.0; int nfYedekN = 0;
    const int momHops = std::max (1, (int) std::lround (0.4 / hopSecOf (segs)));
    double roomSec = 0.0, voicedSec = 0.0; int sentences = 0;
    const double hopSec = hopSecOf (segs);
    for (const auto& s : segs)
    {
        roomSec += s.vad.roomToneSeconds(); voicedSec += s.vad.voicedSeconds(); sentences += s.vad.sentenceCount();
        // denetim KA-2: tabanı ÖLÇÜLMEMİŞ segment (karesiz/tamamen dijital sessiz) minimumu −120'ye çekmesin
        if (s.vad.floorMeasured()) { noiseFloor = std::min (noiseFloor, s.vad.noiseFloorRobustDb()); nfKnown = nfKnown || s.vad.floorSupported(); ++nfCount; }
        else if (! s.frames.empty()) { nfYedek += s.vad.noiseFloorDb(); ++nfYedekN; }
        for (const auto& f : s.frames)
        {
            if (f.flags & VAD_VOICED)
            {
                if (f.momentaryLufs > -99.0f) voicedMom.push_back (f.momentaryLufs);
                voicedE.push_back (f.energyDb); crest.push_back (f.crestDb); flat.push_back (f.flatness); hf.push_back (f.hfRatio);
                if (f.pitchHz > 0.0f)
                {
                    f0.push_back (f.pitchHz);
                    // gam histogrami: perde sinifi (yumusak agirlik: yarim-ton merkezine yakinlik)
                    const double midi = 69.0 + 12.0 * std::log2 ((double) f.pitchHz / 440.0);
                    const int pc = ((int) std::lround (midi) % 12 + 12) % 12;
                    const double uzaklik = std::fabs (midi - std::lround (midi));   // 0..0.5
                    pcHist[(size_t) pc] += 1.0 - uzaklik;                            // tam merkez 1, sinirda 0.5
                }
            }
            if (f.flags & VAD_TONAL) { slope.push_back (f.slopeDbPerOct); centroid.push_back (f.centroidHz); }
        }
        for (const auto& e : s.events)
        {
            switch (e.type)
            {
                case VadEventType::Sibilant: ++r.sibilantEvents; break;
                case VadEventType::Plosive:  ++r.plosiveEvents; break;
                case VadEventType::Breath:   ++r.breathEvents; break;
                case VadEventType::Sentence:
                {
                    SentenceStat st; st.startSec = e.startSec; st.endSec = e.endSec; st.voicedFrames = e.voicedFrames;
                    // momentary 400 ms penceresi cümlenin başındaki sessizliği de kapsar → ısınma kareleri dışlanır
                    // (cümle yeterince uzunsa); aksi halde tüm sesli kareler
                    const long warm = (e.endFrame - e.startFrame + 1 >= 2 * momHops) ? momHops - 1 : 0;
                    double ms = 0.0; int cnt = 0; std::vector<float> mom, cr;
                    for (long k = e.startFrame + warm; k <= e.endFrame && k < (long) s.frames.size(); ++k)
                    {
                        const auto& f = s.frames[(size_t) k];
                        if (! (f.flags & VAD_VOICED) || f.momentaryLufs <= -99.0f) continue;
                        ms += lufsToMs (f.momentaryLufs); ++cnt; mom.push_back (f.momentaryLufs); cr.push_back (f.crestDb);
                    }
                    if (cnt > 0)
                    {
                        st.loudnessLufs = LoudnessMeter::msToLufs (ms / cnt);
                        const double m = std::accumulate (mom.begin(), mom.end(), 0.0) / cnt;
                        double v = 0.0; for (float x : mom) v += (x - m) * (x - m);
                        st.microStdDb = (float) std::sqrt (v / cnt);
                        st.crestDb = median (cr);
                        r.sentenceStats.push_back (st);
                    }
                    break;
                }
            }
        }
        r.sibilantCenterHz = s.vad.sibilantCenterHz(); r.sibilantCalibrated = s.vad.sibilantCalibrated();
    }
    r.roomToneSec = roomSec; r.voicedSec = voicedSec; r.sentences = sentences;
    r.noiseFloorDb = nfCount > 0 ? noiseFloor : (nfYedekN > 0 ? (float) (nfYedek / nfYedekN) : -120.0f);
    r.noiseFloorKnown = nfCount > 0 && nfKnown;
    // sesli-gate'li loudness: sesli karelerin momentary'si (ms ortalaması)
    if (! voicedMom.empty()) { double ms = 0.0; for (float l : voicedMom) ms += lufsToMs (l); r.voicedLufs = LoudnessMeter::msToLufs (ms / (double) voicedMom.size()); }
    r.snrDb = voicedE.empty() ? 0.0f : median (voicedE) - r.noiseFloorDb;
    r.crestMedianDb = median (crest);
    r.slopeMedianDbPerOct = median (slope); r.centroidMedianHz = median (centroid);
    r.flatnessMedianVoiced = median (flat); r.hfRatioMedianVoiced = median (hf);
    if (! f0.empty()) { r.f0MedianHz = median (f0); r.f0P5Hz = percentile (f0, 0.05); r.f0P95Hz = percentile (f0, 0.95); }
    // ---- gam algilama (M3.1): Krumhansl-Schmuckler profil korelasyonu (12 major + 12 minor) ----
    {
        double toplam = 0.0; for (double v : pcHist) toplam += v;
        if (toplam >= 120.0)                                     // ~2.5+ sn sesli perde karesi olmadan karar yok
        {
            static const double MAJ[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
            static const double MIN[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
            auto korelasyon = [&] (const double* prof, int kok)
            {
                double mx = 0.0, my = 0.0;
                for (int i = 0; i < 12; ++i) { mx += pcHist[(size_t) i]; my += prof[i]; }
                mx /= 12.0; my /= 12.0;
                double pay = 0.0, vx = 0.0, vy = 0.0;
                for (int i = 0; i < 12; ++i)
                {
                    const double x = pcHist[(size_t) ((i + kok) % 12)] - mx, y = prof[i] - my;
                    pay += x * y; vx += x * x; vy += y * y;
                }
                return vx > 0.0 && vy > 0.0 ? pay / std::sqrt (vx * vy) : 0.0;
            };
            double enIyi = -2.0, ikinci = -2.0; int enIyiKok = -1; bool enIyiMinor = false;
            for (int kok = 0; kok < 12; ++kok)
                for (int mnr = 0; mnr < 2; ++mnr)
                {
                    const double c = korelasyon (mnr != 0 ? MIN : MAJ, kok);
                    if (c > enIyi) { ikinci = enIyi; enIyi = c; enIyiKok = kok; enIyiMinor = mnr != 0; }
                    else if (c > ikinci) ikinci = c;
                }
            const double marj = enIyi - ikinci;
            // guven: korelasyon gucu + ayrisma marji (olculen esikler: sentetik dizide ~0.8/0.15, gurultude < 0.4)
            const float guven = (float) std::clamp ((enIyi - 0.35) * 1.8 * std::clamp (marj / 0.10, 0.0, 1.0), 0.0, 1.0);
            if (enIyiKok >= 0 && guven >= 0.15f)   // denetim M3.2 (K-6): mikro güvenli 'gam' raporlanmaz (JSON tüketicileri yanılmıyordu ama yanıltabilirdi)
            {
                r.keyIndex = enIyiKok;
                r.keyMinor = enIyiMinor;
                r.keyConfidence = guven;
            }
        }
    }
    // cümle dinamiği
    if (! r.sentenceStats.empty())
    {
        std::vector<float> l, ms;
        for (const auto& s : r.sentenceStats) { l.push_back (s.loudnessLufs); ms.push_back (s.microStdDb); }
        r.sentenceMedianLufs = median (l);
        r.sentenceIqrDb = percentile (l, 0.75) - percentile (l, 0.25);
        r.sentenceP95P05Db = percentile (l, 0.95) - percentile (l, 0.05);
        r.microStdMedianDb = median (ms);
    }
    // LTAS
    ltas (segs, 0, r.ltasDb, r.ltasStdDb, r.numBands, r.tonalFrames);
    r.ltasHalfDiffDb = ltasHalfDiff (segs);
    r.sibilantPerMinute = voicedSec > 0.0 ? (float) (r.sibilantEvents / (voicedSec / 60.0)) : 0.0f;
    // onset keskinligi: sesli karelerin aki degerlerinin en ust %10 ortalamasi
    {
        std::vector<float> fx;
        for (const auto& s : segs) for (const auto& f : s.frames) if (f.flags & VAD_VOICED) fx.push_back (f.flux);
        if (fx.size() >= 10)
        {
            std::sort (fx.begin(), fx.end());
            double acc = 0.0; int cnt = 0;
            for (size_t i = fx.size() - fx.size() / 10; i < fx.size(); ++i) { acc += fx[i]; ++cnt; }
            r.onsetSharpness = cnt > 0 ? (float) (acc / cnt) : 0.0f;
        }
    }
    // modulasyon vekili: ardisik sesli karelerin momentary dB fark RMS'i
    {
        double acc = 0.0; int cnt = 0;
        for (const auto& s : segs)
            for (size_t k = 1; k < s.frames.size(); ++k)
            {
                const auto& a = s.frames[k - 1]; const auto& b = s.frames[k];
                if (! (a.flags & VAD_VOICED) || ! (b.flags & VAD_VOICED)) continue;
                if (a.momentaryLufs <= -99.0f || b.momentaryLufs <= -99.0f) continue;
                const double d2 = (double) b.momentaryLufs - a.momentaryLufs;
                acc += d2 * d2; ++cnt;
            }
        r.momentaryModDb = cnt > 4 ? (float) std::sqrt (acc / cnt) : 0.0f;
    }
    r.confidence = computeConfidence (segs, hopSec);
    // depodan tek geçiş: örnek tepe + clipping sayacı + DC
    {
        float pk = 0.0f; double dc = 0.0; uint64_t clip = 0;
        const float* d = storeData; const int64_t n = storeLen;
        for (int64_t i = 0; i < n; ++i)
        {
            const float a = std::fabs (d[i]);
            pk = std::max (pk, a);
            if (a >= 0.999f) ++clip;
            dc += d[i];
        }
        r.samplePeakDb = 20.0f * std::log10 (std::max (pk, 1.0e-6f));
        r.clippedSamples = clip;
        r.dcOffset = n > 0 ? (float) (dc / (double) n) : 0.0f;
    }
    return r;
}

AnalysisReport buildReport (AnalysisEngine& engine)
{
    const auto& store = engine.store();
    auto r = buildReportCore (engine.segmentAnalyses(), store.data(), store.size(), store.sampleRate());
    const auto pr = engine.progress();
    r.generation = pr.generation;
    r.segments = (int) store.segments().size();
    r.gaps = pr.gaps; r.droppedBlocks = store.totalDroppedBlocks(); r.storeFull = store.full();
    return r;
}

} // namespace sweax
