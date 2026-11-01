// SWEAX NEXT — DSP/DeEsser.cpp
#include "DeEsser.h"
#include "../Chain/VocalChain.h"
#include <cmath>

namespace sweax
{

namespace
{
    constexpr float KNEE_DB = 3.0f;
    inline float coefFromMs (double sr, double ms) noexcept { return (float) std::exp (-1.0 / (std::max (0.01, ms) * 0.001 * sr)); }
}

void DeEsser::prepare (double sampleRate, int maxBlock, int numChannels)
{
    const int worst = (int) std::ceil (LOOKAHEAD_MAX_MS * 0.001 * sampleRate);
    prepareBase (sampleRate, maxBlock, numChannels, worst);
    for (int k = 0; k < MAX_CH; ++k)
    {
        lowDelay[(size_t) k].prepare (worst, 0); highDelay[(size_t) k].prepare (worst, 0); wideDelay[(size_t) k].prepare (worst, 0);
        gainDelay[(size_t) k].prepare (worst, 0);
    }
    activeDelay = 0;
    envLpCoef = coefFromMs (sr, 0.16);                                   // ≈1 kHz zarf (desimasyon öncesi anti-alias niyetine)
    decim = std::max (1, (int) std::lround (sr / 6000.0));               // zarf hızı ≈ 6 kHz (sr'den bağımsız)
    const double envRate = sr / (double) decim;
    envLen = std::clamp ((int) std::lround (0.032 * envRate), 64, ENV_MAX);
    lagLo = std::max (2, (int) std::lround (envRate / 1000.0));          // F0 1000 Hz
    lagHi = std::min (envLen - 1, std::max (lagLo + 1, (int) std::lround (envRate / 70.0)));   // F0 70 Hz
    acInterval = std::max (256, (int) std::lround (0.005 * sr));         // her 5 ms
    lastHz = -1.0f;
    pending = cur = DeEssParams {};
    paramsDirty = true;
    resetWet();
}

int DeEsser::latencySamples (Quality mode) const noexcept
{
    return mode == Quality::Canli ? 0 : (int) std::lround (LOOKAHEAD_MAX_MS * 0.001 * sr);
}

void DeEsser::setQuality (Quality mode) noexcept
{
    activeDelay = latencySamples (mode);
    for (int k = 0; k < MAX_CH; ++k) { lowDelay[(size_t) k].setDelay (activeDelay); highDelay[(size_t) k].setDelay (activeDelay); wideDelay[(size_t) k].setDelay (activeDelay); }
    setDryDelay (activeDelay);
    paramsDirty = true;
}

void DeEsser::resetWet()
{
    for (auto& c : lp) for (auto& b : c) b.reset();
    for (auto& c : hp) for (auto& b : c) b.reset();
    for (auto& s : detBand) s.reset();
    for (int k = 0; k < MAX_CH; ++k) { lowDelay[(size_t) k].reset(); highDelay[(size_t) k].reset(); wideDelay[(size_t) k].reset(); gainDelay[(size_t) k].reset(); }
    for (auto& d : det) d = Detector {};
    envRing.fill (0.0f); envPos = 0; envLp = 0.0f; decCounter = 0; acCounter = 0; acPeak = 0.0f; gateHarmonic = false; gateOn = gateOff = 0; grOut = 0.0f;
    for (int k = 0; k < MAX_CH; ++k) { govdeHp[(size_t) k].reset(); govdeLp[(size_t) k].reset(); }
    govdeEnv.fill (0.0f); govdeTepe.fill (-70.0f); govdeDb.fill (-70.0f);
}

// kapalıyken: crossover filtreleri + ses yolu gecikme hatları girişle beslenir (yeniden açılış bayat/boş çalmaz);
// dedektör ve kapı dinlenir
void DeEsser::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    if (paramsDirty) { cur = pending; paramsDirty = false; updateCoeffs(); }
    for (int i = 0; i < n; ++i)
    {
        if (cur.goreli)                                                    // gövde zarfı ısınık kalır
        {
            if (cur.link) { float mw = 0.0f; for (int k = 0; k < numCh; ++k) mw += ch[k][i]; govdeIsle (0, mw / (float) std::max (1, numCh)); }
            else for (int k = 0; k < numCh; ++k) govdeIsle (k, ch[k][i]);
        }
        for (int k = 0; k < numCh; ++k)
        {
            const float x = ch[k][i];
            auto& L = lp[(size_t) k]; auto& H = hp[(size_t) k];
            lowDelay[(size_t) k].processSample (L[1].processSample (L[0].processSample (x)));
            highDelay[(size_t) k].processSample (H[1].processSample (H[0].processSample (x)));
            wideDelay[(size_t) k].processSample (x);
            gainDelay[(size_t) k].processSample (0.0f);
        }
    }
    for (auto& d : det) d.grDb = 0.0f;
    grOut = 0.0f;
    govdeSanitize();
}

void DeEsser::updateCoeffs() noexcept
{
    if (cur.hz != lastHz)
    {
        const double f = std::clamp ((double) cur.hz, 2000.0, std::min (12000.0, sr * 0.45));
        const auto lpc = design::lowpass (sr, f, 0.70710678), hpc = design::highpass (sr, f, 0.70710678);
        for (int k = 0; k < MAX_CH; ++k)
        {
            for (auto& b : lp[(size_t) k]) b.setCoeffs (lpc);
            for (auto& b : hp[(size_t) k]) b.setCoeffs (hpc);
            detBand[(size_t) k].setCoeffs (sr, f, 1.0);
        }
        lastHz = cur.hz;
    }
    envCoef = coefFromMs (sr, 2.0); wideCoef = coefFromMs (sr, 2.0);
    govdeCoef = coefFromMs (sr, 10.0); govdeAtk = coefFromMs (sr, 20.0); govdeRel = coefFromMs (sr, 1500.0);
    for (int k = 0; k < MAX_CH; ++k)
    {
        govdeHp[(size_t) k].setCoeffs (design::highpass (sr, 300.0, 0.70710678));
        govdeLp[(size_t) k].setCoeffs (design::lowpass (sr, std::min (3000.0, sr * 0.45), 0.70710678));
    }
    atkCoef = coefFromMs (sr, cur.attackMs); relCoef = coefFromMs (sr, std::max (10.0f, cur.releaseMs));
    const int look = std::clamp ((int) std::lround (cur.lookaheadMs * 0.001 * sr), 0, activeDelay);
    for (auto& g : gainDelay) g.setDelay (std::max (0, activeDelay - look));
}

float DeEsser::computeGr (Detector& d, float band, float wide, float govde) noexcept
{
    d.bandEnv = band * band + envCoef * (d.bandEnv - band * band);
    d.wideEnv = wide * wide + wideCoef * (d.wideEnv - wide * wide);
    const float bandDb = 10.0f * std::log10 (d.bandEnv + 1.0e-12f);
    const float over = cur.goreli ? (bandDb - govde - cur.goreliEsikDb) : (bandDb - cur.threshDb);
    float gr;
    if (over <= -KNEE_DB * 0.5f) gr = 0.0f;
    else if (over >= KNEE_DB * 0.5f) gr = over;
    else { const float t = over + KNEE_DB * 0.5f; gr = t * t / (2.0f * KNEE_DB); }
    gr = -std::min (std::clamp (cur.rangeDb, 0.0f, 12.0f), gr);
    // harmonik içerik: GR yok (parlak 'i' lisp'lemez). Göreli modda kapı KULLANILMAZ: zincir sonunda (sıkıştırılmış +
    // doygunluklu sinyal) kapı zamanın ~%80'inde kapalı kalıp ünlüye bitişik s'leri kilitliyordu (ölçüldü); sesli
    // harfler göreli eşiğin zaten 15+ dB altında
    if (gateHarmonic && ! cur.goreli) gr = 0.0f;
    const float c = gr < d.grDb ? atkCoef : relCoef;
    d.grDb = gr + c * (d.grDb - gr);
    return d.grDb;
}

void DeEsser::processWet (float* const* ch, int numCh, int n) noexcept
{
    if (paramsDirty) { cur = pending; paramsDirty = false; updateCoeffs(); }
    for (int i = 0; i < n; ++i)
    {
        float highs[MAX_CH] {}, lows[MAX_CH] {}, bands[MAX_CH] {};
        float monoBand = 0.0f, monoWide = 0.0f;
        for (int k = 0; k < numCh; ++k)
        {
            const float x = ch[k][i];
            auto& L = lp[(size_t) k]; auto& H = hp[(size_t) k];
            lows[k] = L[1].processSample (L[0].processSample (x));
            highs[k] = H[1].processSample (H[0].processSample (x));
            bands[k] = (cur.mode == 0) ? highs[k] : detBand[(size_t) k].bandpassNormalized (x);
            monoBand += bands[k]; monoWide += x;
        }
        monoBand /= (float) numCh; monoWide /= (float) numCh;
        if (cur.goreli)
        {
            if (cur.link) govdeIsle (0, monoWide);
            else for (int k = 0; k < numCh; ++k) govdeIsle (k, ch[k][i]);           // ch henüz yazılmadı (aşağıda)
        }
        // harmoniklik kapısı: bant zarfı → ≈6 kHz desimasyon → F0 gecikmelerinde dairesel otokorelasyon (her 5 ms)
        {
            const float e = std::fabs (monoBand);
            envLp = e + envLpCoef * (envLp - e);
            if (++decCounter >= decim) { decCounter = 0; envRing[(size_t) envPos] = envLp; envPos = (envPos + 1) % envLen; }
        }
        if (++acCounter >= acInterval)
        {
            acCounter = 0;
            // doğrusallaştır (modulo iç döngüden çıkar): lin[j] = halka kronolojik + kuyruk kopyası
            float lin[2 * ENV_MAX];
            for (int j = 0; j < envLen; ++j) lin[j] = envRing[(size_t) ((envPos + j) % envLen)];
            double mean = 0.0; for (int j = 0; j < envLen; ++j) mean += lin[j];
            mean /= envLen;
            for (int j = 0; j < envLen; ++j) { lin[j] -= (float) mean; lin[envLen + j] = lin[j]; }
            double e0 = 0.0; for (int j = 0; j < envLen; ++j) e0 += (double) lin[j] * lin[j];
            float best = 0.0f;
            if (e0 > 1.0e-12 && mean > 1.0e-5 && std::isfinite ((float) e0))
            {
                for (int lag = lagLo; lag <= lagHi; lag += 2)
                {
                    double r = 0.0;
                    const float* a = lin; const float* b = lin + lag;
                    for (int j = 0; j < envLen; ++j) r += (double) a[j] * b[j];
                    best = std::max (best, (float) (r / e0));
                }
            }
            acPeak = best;
            // zamansal histerezis: 2 ardışık tespit açar / 2 ardışık düşük kapatır (≈10 ms)
            if (best > 0.35f) { if (++gateOn >= 2) { gateHarmonic = true; gateOff = 0; } }
            else if (best < 0.25f) { if (++gateOff >= 2) { gateHarmonic = false; gateOn = 0; } }
            else { gateOn = 0; gateOff = 0; }
        }
        float gDbLinked = 0.0f;
        if (cur.link) gDbLinked = computeGr (det[0], monoBand, monoWide, govdeDb[0]);
        for (int k = 0; k < numCh; ++k)
        {
            const float gDb = cur.link ? gDbLinked : computeGr (det[(size_t) k], bands[k], ch[k][i], govdeDb[(size_t) k]);
            const float gDelayed = gainDelay[(size_t) k].processSample (gDb);
            const float g = std::pow (10.0f, gDelayed * 0.05f);
            if (cur.mode == 0)
            {
                const float lo = lowDelay[(size_t) k].processSample (lows[k]);
                const float hi = highDelay[(size_t) k].processSample (highs[k]);
                ch[k][i] = lo + hi * g;
            }
            else
            {
                ch[k][i] = wideDelay[(size_t) k].processSample (ch[k][i]) * g;
            }
            if (k == 0) grOut = gDelayed;
        }
    }
    for (auto& c : lp) for (auto& b : c) b.sanitize();
    for (auto& c : hp) for (auto& b : c) b.sanitize();
    for (auto& s : detBand) s.sanitize();
    // sayısal güvenlik: zehirlenen zarf/GR kalıcı tam-bastırmaya kilitliyordu (inceleme) → sıfırla
    for (auto& d : det)
        if (! (std::isfinite (d.bandEnv) && std::isfinite (d.wideEnv) && std::isfinite (d.grDb))) d = Detector {};
    if (! std::isfinite (envLp)) { envLp = 0.0f; envRing.fill (0.0f); }
    govdeSanitize();
}

} // namespace sweax
