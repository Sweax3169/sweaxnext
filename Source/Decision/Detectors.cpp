// SWEAX NEXT — Decision/Detectors.cpp
#include "Detectors.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace sweax::decision
{

namespace
{
    inline float clamp01 (double x) noexcept { return (float) std::clamp (x, 0.0, 1.0); }
    // yumuşak eşik: x0'da 0, x1'de 1 (doğrusal)
    inline float ramp (double x, double x0, double x1) noexcept { return x1 <= x0 ? (x >= x1 ? 1.0f : 0.0f) : clamp01 ((x - x0) / (x1 - x0)); }
    inline double msOf (float db) noexcept { return std::pow (10.0, db / 10.0); }
    inline float dbOf (double ms) noexcept { return (float) (10.0 * std::log10 (std::max (ms, 1.0e-12))); }

    // bant aralığı [lo, hi) Hz üzerinde güç ortalaması (dB)
    float bandMeanDb (const float* ltas, int nb, double lo, double hi) noexcept
    {
        double s = 0.0; int c = 0;
        for (int b = 0; b < nb; ++b) { const double f = Detectors::bandHz (b); if (f >= lo && f < hi) { s += msOf (ltas[b]); ++c; } }
        return c > 0 ? dbOf (s / c) : -120.0f;
    }
    // referans önceli ile ofset: 150 Hz–4 kHz bölgesinde farkın MEDYANI (yalnız seviye uydurulur; medyan geniş bir
    // tümseğin ofseti çekmesini önler — ortalama ile +9 dB mud tümseği ~3 dB kaybediyordu)
    float priorOffset (const float* ltas, int nb) noexcept
    {
        // geniş aralık (100 Hz – 8 kHz): geniş bir tümsek bant sayısının küçük bir kesri kalır → medyan onu izlemez
        float d[FrameFeatures::MAX_BANDS]; int c = 0;
        for (int b = 0; b < nb && c < FrameFeatures::MAX_BANDS; ++b) { const double f = Detectors::bandHz (b); if (f >= 100.0 && f < 8000.0) d[c++] = ltas[b] - Detectors::priorLtasDb (f); }
        if (c == 0) return 0.0f;
        std::nth_element (d, d + c / 2, d + c);
        return d[c / 2];
    }
    // bölge fazlalığı: (ölçülen − önceli − ofset) ortalaması, dB
    float regionDev (const float* ltas, int nb, float offset, double lo, double hi) noexcept
    {
        double s = 0.0; int c = 0;
        for (int b = 0; b < nb; ++b) { const double f = Detectors::bandHz (b); if (f >= lo && f < hi) { s += ltas[b] - Detectors::priorLtasDb (f) - offset; ++c; } }
        return c > 0 ? (float) (s / c) : 0.0f;
    }
    ProblemDetection make (Problem t, float p, float s, float c, float hz = 0.0f, float bw = 0.0f, float val = 0.0f, int cnt = 0)
    {
        ProblemDetection d; d.type = t; d.probability = p; d.severity = s; d.confidence = c; d.centerHz = hz; d.bandwidthOct = bw; d.valueDb = val; d.count = cnt; return d;
    }
}

// Parametrik referans (ofsetsiz, dB): 150–500 Hz düz (0 dB), 500 Hz üstü −6 dB/okt, 150 Hz altı −3 dB/okt (proximity'siz kayıt)
float Detectors::priorLtasDb (double hz) noexcept
{
    if (hz < 150.0) return (float) (-3.0 * std::log2 (150.0 / std::max (hz, 20.0)));
    if (hz <= 500.0) return 0.0f;
    return (float) (-6.0 * std::log2 (hz / 500.0));
}

float Detectors::regionDeviationDb (const AnalysisReport& r, double lo, double hi) noexcept
{
    const int nb = r.numBands;
    if (nb <= 8 || r.tonalFrames < 10) return 0.0f;
    float sm[FrameFeatures::MAX_BANDS];
    smoothThirdOctave (r.ltasDb, sm, nb);
    return regionDev (sm, nb, priorOffset (sm, nb), lo, hi);
}

void Detectors::smoothThirdOctave (const float* in, float* out, int n) noexcept
{
    for (int k = 0; k < n; ++k)
    {
        double s = 0.0; int c = 0;
        for (int j = k - 2; j <= k + 1; ++j) if (j >= 0 && j < n) { s += msOf (in[j]); ++c; }
        out[k] = dbOf (s / std::max (1, c));
    }
}

std::vector<ProblemDetection> Detectors::detect (const AnalysisReport& r, const std::vector<SegmentAnalysis>& segs, const DetectorContext& ctx)
{
    std::vector<ProblemDetection> out;
    const auto& cf = r.confidence;
    const int nb = r.numBands;

    // ---------------- sinyal kalitesi ----------------
    {
        const double totalSamples = std::max (1.0, r.capturedSec * 48000.0);
        const float clipFrac = (float) (ctx.clippedSamples / totalSamples);
        if (ctx.clippedSamples > 0.0f)
            out.push_back (make (Problem::Clipping, ramp (ctx.clippedSamples, 2.0, 50.0), ramp (clipFrac, 0.0, 1.0e-3), cf.level, 0, 0, r.samplePeakDb, (int) ctx.clippedSamples));
        if (std::fabs (ctx.dcOffset) > 1.0e-4f)
            out.push_back (make (Problem::DcOffset, ramp (std::fabs (ctx.dcOffset), 1.0e-4, 1.0e-2), ramp (std::fabs (ctx.dcOffset), 1.0e-3, 5.0e-2), cf.level, 0, 0, 20.0f * std::log10 (std::fabs (ctx.dcOffset))));
        if (r.voicedLufs < -30.0f)
            out.push_back (make (Problem::LowLevel, ramp (-r.voicedLufs, 30.0, 40.0), ramp (-r.voicedLufs, 30.0, 50.0), cf.level, 0, 0, r.voicedLufs));
    }

    // ---------------- gürültü ----------------
    {
        // SNR = sesli medyan enerji − taban; < 40 dB dikkat, < 25 dB ciddi
        const float snr = r.snrDb;
        if (r.roomToneSec > 0.2)
        {
            out.push_back (make (Problem::NoiseFloor, ramp (45.0 - snr, 0.0, 15.0), ramp (45.0 - snr, 5.0, 30.0), r.noiseFloorKnown ? cf.noise : 0.0f, 0, 0, r.noiseFloorDb));
            // tip: oda karelerinin LTAS'ı — yalnız komşuları da (±2 kare) sesli olmayan "iç" oda kareleri (ton kuyrukları
            // hariç) ve dB-ortalaması (tek bir yüksek kare güç ortalamasını ele geçiriyordu → 180 Hz sahte hum)
            std::vector<double> room ((size_t) std::max (nb, 1), 0.0); int cnt = 0;
            for (const auto& s : segs)
            {
                const auto& fr = s.frames;
                for (size_t k = 2; k + 2 < fr.size(); ++k)
                {
                    if (! (fr[k].flags & VAD_ROOMTONE)) continue;
                    bool ic = true; for (size_t j = k - 2; j <= k + 2; ++j) if (fr[j].flags & (VAD_VOICED | VAD_TONAL | VAD_SIBILANT)) { ic = false; break; }
                    if (! ic) continue;
                    ++cnt; for (int b = 0; b < (int) fr[k].numBands && b < nb; ++b) room[(size_t) b] += fr[k].bandsDb[b];
                }
            }
            if (cnt >= 10 && nb > 0)
            {
                std::vector<float> rl ((size_t) nb);
                for (int b = 0; b < nb; ++b) rl[(size_t) b] = (float) (room[(size_t) b] / cnt);
                const float lf = bandMeanDb (rl.data(), nb, 20.0, 120.0), mid = bandMeanDb (rl.data(), nb, 300.0, 3000.0), hf = bandMeanDb (rl.data(), nb, 4000.0, 16000.0);
                // hum: 50/60 Hz ve harmonikleri (100/120, 150/180) bantlarında yerel tepe, ±½ oktav komşulara göre
                // (100 Hz altında 1/12 okt bantlar aynı FFT bin'lerini paylaşır → ±2 bant karşılaştırması 0 verir)
                float humPeak = 0.0f; float humHz = 0.0f;
                for (double f0 : { 50.0, 60.0 })
                    for (int h = 1; h <= 3; ++h)
                    {
                        const int b = bandOf (f0 * h); if (b < 6 || b + 6 >= nb) continue;
                        const float nbh = 0.5f * (rl[(size_t) b - 6] + rl[(size_t) b + 6]);
                        const float pk = std::max (rl[(size_t) b], std::max (rl[(size_t) b - 1], rl[(size_t) b + 1])) - nbh;
                        if (pk > humPeak) { humPeak = pk; humHz = (float) (f0 * h); }
                    }
                if (humPeak > 6.0f) out.push_back (make (Problem::Hum, ramp (humPeak, 6.0, 14.0), ramp (humPeak, 8.0, 25.0), r.noiseFloorKnown ? cf.noise : 0.0f, humHz, 0.1f, humPeak));
                // hiss: YOĞUNLUK (bant gücü / bant genişliği) eğimi pembeden (−3 dB/okt) düz: 1/12 okt bantlarda beyaz gürültü
                // bant gücü +3 dB/okt yükselir; ham bant farkı beyaz odayı da "hiss" sayar → genişlik normalize edilir
                const float dens = (hf - 10.0f * std::log10 (8000.0f)) - (mid - 10.0f * std::log10 (950.0f));   // beyaz → 0, pembe → −9.3
                if (dens > -6.0f) out.push_back (make (Problem::Hiss, ramp (dens, -6.0, 0.0), ramp (45.0 - snr, 5.0, 30.0), r.noiseFloorKnown ? cf.noise : 0.0f, 8000.0f, 2.0f, dens));
                if (lf - mid > 6.0f) out.push_back (make (Problem::LfNoise, ramp (lf - mid, 6.0, 15.0), ramp (lf - mid, 6.0, 25.0) * ramp (45.0 - snr, 0.0, 25.0), r.noiseFloorKnown ? cf.noise : 0.0f, 60.0f, 2.0f, lf - mid));
            }
        }
    }

    // ---------------- tonal (LTAS) ----------------
    if (nb > 40 && r.tonalFrames >= 20)
    {
        std::vector<float> sm ((size_t) nb);
        smoothThirdOctave (r.ltasDb, sm.data(), nb);
        const float off = priorOffset (sm.data(), nb);
        const float c = cf.tonal;
        // Tümsek fazlalığı: y(b) = LTAS − önceli; 1–2 oktav ötedeki yan bantlardan ([lo/4, lo/2] ∪ [2hi, 4hi])
        // log2-f üzerinde doğrusal EĞİLİM bölge merkezinde değerlendirilir; fazlalık = bölge ortalaması − eğilim.
        // (Medyan ofset geniş +9 dB tümseğin ~3 dB'sini yutuyordu; bitişik komşu karşılaştırması etekleri de
        // kaldırdığı için 2.3 dB gösteriyordu — ölçüldü. Eğilim, seviye ofsetini de otomatik düşürür.)
        // eğilim uydurma aralığı: [max(100, 0.9·F0_p5), 8 kHz] — temelin ALTI vokalde boştur ve eğilimi saptırır
        // (ölçüldü: temiz vokalde alt yan bölge −30 dB → 316 Hz'de +20 dB sahte mud)
        const double fitLo = std::max (100.0, r.f0P5Hz > 20.0f ? 0.9 * (double) r.f0P5Hz : 100.0);
        auto bumpExcess = [&] (double lo, double hi) -> float
        {
            // tümsek bölgesi (±%50) uydurmadan hariç; kalan tüm [fitLo, 8k] bantlarına log2-f doğrusu
            double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0; int c2 = 0;
            double xMin = 1e9, xMax = -1e9;
            const double xc = std::log2 (std::sqrt (lo * hi));
            for (int b2 = 0; b2 < nb; ++b2)
            {
                const double f = bandHz (b2);
                if (f < fitLo || f > 8000.0) continue;
                if (f >= lo / 1.5 && f < hi * 1.5) continue;
                const double xk = std::log2 (f) - xc, yk = sm[(size_t) b2] - priorLtasDb (f);
                sx += xk; sy += yk; sxx += xk * xk; sxy += xk * yk; ++c2;
                xMin = std::min (xMin, xk); xMax = std::max (xMax, xk);
            }
            double trend;
            if (c2 >= 6)
            {
                const double den = c2 * sxx - sx * sx;
                const double slope = den > 1e-9 ? (c2 * sxy - sx * sy) / den : 0.0;
                const double x0 = std::clamp (0.0, xMin, xMax);       // uydurma aralığı dışına EKSTRAPOLE ETME
                trend = (sy / c2 - slope * (sx / c2)) + slope * x0;   // doğrunun x0'daki değeri
            }
            else trend = off;                                          // kenar: medyan ofsete düş
            double s2 = 0.0; int cnt = 0;
            for (int b2 = 0; b2 < nb; ++b2) { const double f = bandHz (b2); if (f >= lo && f < hi && f >= fitLo * 0.9) { s2 += sm[(size_t) b2] - priorLtasDb (f); ++cnt; } }
            return cnt > 0 ? (float) (s2 / cnt - trend) : 0.0f;
        };
        auto region = [&] (Problem p, double lo, double hi, float thrDb, float sevDb, float hz, float bw)
        {
            const float ev = bumpExcess (lo, hi);
            if (ev > thrDb * 0.5f)
                out.push_back (make (p, ramp (ev, thrDb * 0.5, thrDb * 1.5), ramp (ev, thrDb * 0.8, thrDb * 0.8 + sevDb), c, hz, bw, ev));
        };
        region (Problem::Boom, 60.0, 150.0, 4.0f, 8.0f, 100.0f, 1.2f);
        region (Problem::Mud, 200.0, 500.0, 3.0f, 6.0f, 320.0f, 1.3f);
        region (Problem::Box, 300.0, 800.0, 3.0f, 6.0f, 500.0f, 1.4f);
        // nasal: 800–1200 tepe VE 1.5–2.5k çukur
        {
            const float pk = regionDev (sm.data(), nb, off, 800.0, 1200.0), dip = regionDev (sm.data(), nb, off, 1500.0, 2500.0);
            const float ev = pk - dip;
            if (pk > 2.0f && ev > 4.0f) out.push_back (make (Problem::Nasal, ramp (ev, 4.0, 9.0), ramp (ev, 5.0, 12.0), c, 1000.0f, 0.6f, ev));
        }
        // dull / bright: 4–12 kHz sapması
        {
            const float dev = regionDev (sm.data(), nb, off, 4000.0, 12000.0);
            if (dev < -4.0f) out.push_back (make (Problem::Dull, ramp (-dev, 4.0, 9.0), ramp (-dev, 5.0, 12.0), c, 6000.0f, 1.6f, dev));
            if (dev > 4.0f) out.push_back (make (Problem::Bright, ramp (dev, 4.0, 9.0), ramp (dev, 5.0, 12.0), c, 6000.0f, 1.6f, dev));
        }
        // thin: 120–300 Hz eksikliği
        {
            const float dev = regionDev (sm.data(), nb, off, 120.0, 300.0);
            if (dev < -5.0f) out.push_back (make (Problem::Thin, ramp (-dev, 5.0, 10.0), ramp (-dev, 6.0, 14.0), c, 200.0f, 1.3f, dev));
        }
        // proximity: 80–200 Hz eğimi (dB/okt) pozitif ve büyük → aşağıya doğru artan LF
        {
            const float a = bandMeanDb (sm.data(), nb, 80.0, 110.0), b = bandMeanDb (sm.data(), nb, 150.0, 210.0);
            const float slope = (a - b) / (float) std::log2 (180.0 / 95.0);   // dB/okt, pozitif = aşağı doğru artıyor
            const float ev = slope - (float) (-(priorLtasDb (95.0) - priorLtasDb (180.0)) / std::log2 (180.0 / 95.0)); // önceliye göre
            if (ev > 3.0f) out.push_back (make (Problem::Proximity, ramp (ev, 3.0, 8.0), ramp (ev, 4.0, 12.0), c, 100.0f, 1.0f, ev));
        }
        // harsh: yalnız en yüksek %20 loudness'lı sesli karelerde 2–5 kHz fazlası (seviye bağımlı)
        {
            std::vector<const FrameRecord*> voiced;
            for (const auto& s : segs) for (const auto& f : s.frames) if ((f.flags & VAD_VOICED) && f.momentaryLufs > -99.0f) voiced.push_back (&f);
            if (voiced.size() >= 25)
            {
                std::vector<float> lv; lv.reserve (voiced.size()); for (auto* f : voiced) lv.push_back (f->momentaryLufs);
                std::sort (lv.begin(), lv.end());
                const float thr = lv[(size_t) (lv.size() * 0.8)];
                std::vector<double> acc ((size_t) nb, 0.0); int cnt = 0;
                for (auto* f : voiced) if (f->momentaryLufs >= thr) { ++cnt; for (int b = 0; b < (int) f->numBands && b < nb; ++b) acc[(size_t) b] += msOf (f->bandsDb[b]); }
                if (cnt >= 5)
                {
                    std::vector<float> loud ((size_t) nb), lsm ((size_t) nb);
                    for (int b = 0; b < nb; ++b) loud[(size_t) b] = dbOf (acc[(size_t) b] / cnt);
                    smoothThirdOctave (loud.data(), lsm.data(), nb);
                    const float loff = priorOffset (lsm.data(), nb);
                    const float dev = regionDev (lsm.data(), nb, loff, 2000.0, 5000.0);
                    const float allDev = regionDev (sm.data(), nb, off, 2000.0, 5000.0);
                    const float ev = std::max (dev, 0.5f * (dev + allDev));
                    if (ev > 2.0f) out.push_back (make (Problem::Harsh, ramp (ev, 2.0, 6.0), ramp (ev, 3.0, 10.0), c, 3200.0f, 1.3f, ev));
                }
            }
        }
        // rezonans: 1/12 okt LTAS − 1 OKTAV zarf ≥ +4 dB (1/3 okt zarf tepeyi içine alıyordu → ölçülemiyordu) VE
        // − 1/3 okt zarf ≥ +2 dB (sabit perdeli harmonik tarak tek bant tepeleri üretir; bu ikinci ölçüt onları eler),
        // 100 Hz–8 kHz; kalıcılık tonal karelerden
        {
            // zarf = "halka medyanı": ±3…±6 bant (tepenin kendisi ve omuzları hariç) dB medyanı — güç ortalaması tepeyi
            // kendi referansına katıp fazlalığı 4 dB'ye düşürüyordu (ölçüldü); medyan hem tepeye hem harmonik tarağa dayanıklı
            // halka üzerinde YEREL DOĞRUSAL EĞİLİM (en küçük kareler, dB vs bant) ve k'da değerlendirme: formant eteği gibi
            // dik eğimli bölgelerde medyan üst kümeye düşüp fazlalığı yarıya indiriyordu (ölçüldü: 7.4 → 4.2 dB)
            std::vector<float> envOct ((size_t) nb);
            for (int k = 0; k < nb; ++k)
            {
                double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0; int c2 = 0;
                for (int j = k - 6; j <= k + 6; ++j)
                    if (j >= 0 && j < nb && std::abs (j - k) >= 3) { const double xk = j - k, yk = r.ltasDb[j]; sx += xk; sy += yk; sxx += xk * xk; sxy += xk * yk; ++c2; }
                if (c2 < 3) { envOct[(size_t) k] = r.ltasDb[k]; continue; }
                const double den = c2 * sxx - sx * sx;
                const double slope = den > 1e-9 ? (c2 * sxy - sx * sy) / den : 0.0;
                const double icept = (sy - slope * sx) / c2;          // x = 0 → k'daki eğilim değeri
                envOct[(size_t) k] = (float) icept;
            }
            for (int b = 4; b < nb - 2; ++b)
            {
                const double f = bandHz (b);
                if (f < 100.0 || f > 8000.0) continue;
                const float ex = r.ltasDb[b] - envOct[(size_t) b];
                const float ex3 = r.ltasDb[b] - sm[(size_t) b];
                if (ex < 4.0f || ex3 < 2.0f) continue;
                if (r.ltasDb[b] < r.ltasDb[b - 1] || r.ltasDb[b] < r.ltasDb[b + 1]) continue;   // yerel tepe
                // kalıcılık: yalnız BANDI UYARAN tonal karelerde (bölge zarfı karenin tepesinin 25 dB içinde) bant, kendi
                // 1/3 okt zarfını ≥ 3 dB aşıyor mu (§3.4: "kalıcılık yalnız bandı uyaran kareler üzerinden")
                int tot = 0, hit = 0;
                for (const auto& s : segs) for (const auto& fr : s.frames)
                {
                    if (! (fr.flags & VAD_TONAL) || (int) fr.numBands <= b + 1) continue;
                    double env = 0.0; int c2 = 0; for (int j = b - 2; j <= b + 1; ++j) if (j >= 0 && j < (int) fr.numBands) { env += msOf (fr.bandsDb[j]); ++c2; }
                    const float envDb = dbOf (env / std::max (1, c2));
                    float mx = -200.0f; for (int j = 0; j < (int) fr.numBands; ++j) mx = std::max (mx, fr.bandsDb[j]);
                    if (envDb < mx - 25.0f) continue;                    // bant uyarılmamış
                    ++tot;
                    if (fr.bandsDb[b] - envDb >= 3.0f) ++hit;
                }
                // dar bir bant seyrek harmoniklerle karelerin yalnız ~⅓–½'sinde uyarılır (harmonik aralığı 2–3 bant);
                // kalıcılık eşiği buna göre: 0.2 altı yok, 0.5 üstü tam
                const float persist = tot >= 10 ? (float) hit / (float) tot : 0.0f;
                if (persist < 0.2f) continue;
                // genişlik: tepe −3 dB noktaları bant sayısı → Q ≈ fc / bw; rezonans DAR'dır (≤ ½ oktav, Q ≥ ~3):
                // geniş tepeler (temel frekans bölgesi, formant) tonal dedektörlerin işidir (ölçüldü: 123 Hz, 3.5 okt)
                int w = 1; for (int j = b - 1; j >= 0 && r.ltasDb[j] > r.ltasDb[b] - 3.0f; --j) ++w;
                for (int j = b + 1; j < nb && r.ltasDb[j] > r.ltasDb[b] - 3.0f; ++j) ++w;
                const float bwOct = (float) w / 12.0f;
                if (bwOct > 0.5f) continue;
                // temel frekans koruması: perde dağılımı darsa (p95/p5 < 1.3) F0 medyanı ±15 % içindeki tepe temeldir
                if (r.f0MedianHz > 0.0f && r.f0P5Hz > 0.0f && r.f0P95Hz / r.f0P5Hz < 1.3f && std::fabs (f - r.f0MedianHz) < 0.15 * r.f0MedianHz) continue;
                auto d = make (Problem::Resonance, ramp (ex, 3.0, 6.0) * ramp (persist, 0.15, 0.4), ramp (ex, 4.0, 12.0), cf.tonal, (float) f, bwOct, ex, 1);
                out.push_back (d);
            }
        }
    }

    // ---------------- dinamik ----------------
    if (r.sentences >= 2)
    {
        const float c = cf.dynamics;
        const float macro = std::max (r.sentenceIqrDb, 0.6f * r.sentenceP95P05Db);
        if (macro > 2.0f) out.push_back (make (Problem::MacroDynamics, ramp (macro, 2.0, 5.0), ramp (macro, 3.0, 12.0), c, 0, 0, macro, r.sentences));
        const float micro = r.microStdMedianDb;
        const bool overCompressed = r.crestMedianDb < 7.0f && micro < 1.0f;
        if (overCompressed) out.push_back (make (Problem::OverCompressed, ramp (7.0 - r.crestMedianDb, 0.0, 3.0), ramp (7.0 - r.crestMedianDb, 0.0, 4.0), c, 0, 0, r.crestMedianDb));
        else if (micro > 1.5f || r.crestMedianDb > 12.0f)
            out.push_back (make (Problem::MicroDynamics, std::max (ramp (micro, 1.5, 4.0), ramp (r.crestMedianDb, 12.0, 18.0)), std::max (ramp (micro, 2.0, 6.0), ramp (r.crestMedianDb, 12.0, 20.0)), c, 0, 0, micro));
    }

    // ---------------- sibilans / plosive ----------------
    {
        if (r.sibilantEvents > 0)
        {
            // belirginlik: olay karelerinin SİBİLANT BANT ORANI (kalibre bant / 1 kHz–0.7f_s) − sesli karelerin medyanı.
            // Geniş bant enerji farkı yanıltır ('s' vokalden −10…−15 dB daha sessizdir ama HF'de çok daha güçlüdür).
            std::vector<float> voicedR; for (const auto& s : segs) for (const auto& f : s.frames) if (f.flags & VAD_VOICED) voicedR.push_back (f.sibilantRatioDb);
            float vmed = -20.0f; if (! voicedR.empty()) { std::nth_element (voicedR.begin(), voicedR.begin() + (long) (voicedR.size() / 2), voicedR.end()); vmed = voicedR[voicedR.size() / 2]; }
            double prom = 0.0; int n = 0;
            for (const auto& s : segs) for (const auto& e : s.events)
            {
                if (e.type != VadEventType::Sibilant) continue;
                double acc = 0.0; int c = 0;
                for (long k = e.startFrame; k <= e.endFrame && k < (long) s.frames.size(); ++k) { acc += s.frames[(size_t) k].sibilantRatioDb; ++c; }
                if (c > 0) { prom += acc / c - vmed; ++n; }
            }
            const float promDb = n > 0 ? (float) (prom / n) : 0.0f;
            const float rate = r.sibilantPerMinute;
            const float p = ramp (rate, 4.0, 15.0) * ramp (promDb, 6.0, 15.0);
            const float sev = std::max (ramp (promDb, 8.0, 24.0), 0.5f * ramp (rate, 10.0, 40.0));
            out.push_back (make (Problem::Sibilance, p, sev, cf.sibilance, r.sibilantCenterHz, 1.0f, promDb, r.sibilantEvents));
        }
        else if (cf.sibilance > 0.0f)
            out.push_back (make (Problem::Sibilance, 0.0f, 0.0f, cf.sibilance, r.sibilantCenterHz, 1.0f, 0.0f, 0));
        if (r.plosiveEvents > 0)
        {
            // olasılık: sıklık (dk) × olay sayısı yeterliliği (≥3 olay); tek tük onset artefaktı p'yi doldurmaz
            const float perMin = r.voicedSec > 0.0 ? (float) (r.plosiveEvents / (r.voicedSec / 60.0)) : 0.0f;
            const float p = ramp (perMin, 2.0, 10.0) * ramp (r.plosiveEvents, 1.0, 3.0);
            out.push_back (make (Problem::Plosive, p, ramp (perMin, 3.0, 20.0), cf.level, 80.0f, 1.0f, perMin, r.plosiveEvents));
        }
    }
    return out;
}

} // namespace sweax::decision
