// SWEAX NEXT — DSP/DirtModule.cpp
#include "DirtModule.h"

namespace sweax
{

namespace
{
    constexpr float SCREAM_BIAS = 0.25f;                      // çift harmonik (boğaz / tüp asimetrisi)
    constexpr int KONTROL_N = 64;
    constexpr float REF_RMS = 0.126f;                         // −18 dBFS
    constexpr float SESSIZ_GUC = 1.0e-6f;                     // −60 dBFS RMS
    constexpr float ANLIK_TAVAN = 2.0f;                       // +6 dB
    constexpr double DC_HP_HZ = 35.0;
}

float DirtModule::sekil (int mode, float drive, float s) noexcept
{
    if (mode == 0) return std::tanh (drive * s + SCREAM_BIAS) - std::tanh (SCREAM_BIAS);
    if (mode == 1) return std::clamp (drive * 2.0f * s, -1.0f, 0.8f);         // asimetrik sert kırpma
    return s;
}

// −18 dBFS RMS referans sinüsün şekillendirici çıkış RMS'i → kalib = REF / RMS (tahsissiz, RT dışı çağrı sıklığı)
float DirtModule::kalibrasyon (int mode, float drive) noexcept
{
    if (mode == 2) return 1.0f;
    constexpr int N = 96;                                     // 2 periyot × 48 örnek
    const float tepe = REF_RMS * 1.41421356f;
    double guc = 0.0, ort = 0.0;
    for (int i = 0; i < N; ++i)
    {
        const float s = tepe * (float) std::sin (2.0 * 3.141592653589793 * (double) i / 48.0);
        const float y = sekil (mode, drive, s);
        guc += (double) y * y;
        ort += (double) y;
    }
    // veri yolundaki DC engelleyici ortalamayı siler → kalibrasyon da DC'siz RMS'e göre
    ort /= N;
    const double rms = std::sqrt (std::max (0.0, guc / N - ort * ort));
    return rms > 1.0e-6 ? (float) (REF_RMS / rms) : 1.0f;
}

void DirtModule::prepare (double sampleRate, int maxBlock, int numChannels)
{
    prepareBase (sampleRate, maxBlock, numChannels, LATENCY);
    setDryDelay (LATENCY);
    for (auto& f : up) f.design();
    for (auto& f : dn) f.design();
    for (auto& d : dryAlign) d.prepare (LATENCY, LATENCY);
    envCoef = (float) std::exp (-1.0 / (0.150 * sampleRate));
    gecisN = std::max (1, (int) std::lround (0.025 * sampleRate));
    otoKazanc.prepare (sampleRate, 30.0);
    seviye.prepare (sampleRate, 20.0);
    onKazanc.prepare (sampleRate, 10.0);
    paramsDirty = true;
    resetWet();
}

void DirtModule::resetWet()
{
    for (auto& f : up) f.reset();
    for (auto& f : dn) f.reset();
    for (auto& d : dryAlign) d.reset();
    for (auto& b : busHp) b.reset();
    for (auto& b : busLp) b.reset();
    for (auto& b : refLp) b.reset();
    for (auto& b : dcHp) b.reset();
    tut.fill (0.0f); tutSay.fill (0);
    izgaraIn = izgaraOut = 0.0f; izgaraSay = 0;
    envIn = envOut = envOn = 0.0f;
    isinma = 1.0f;
    sonOtoDb = 0.0f;
    gecisKalan = 0;
    otoKazanc.setCurrentAndTarget (0.0f);                     // denetim: 1'den başlamak reset anında +19 dB patlatıyordu
    onKazanc.setCurrentAndTarget (1.0f);
    seviye.setCurrentAndTarget (cur.levelDb <= SEVIYE_KAPALI_DB + 1.0e-3f ? 0.0f : std::pow (10.0f, cur.levelDb / 20.0f));
    paramsDirty = true;
}

void DirtModule::updateParams (bool anlik) noexcept
{
    const int oncekiMod = cur.mode;
    const float oncekiDrive = drive, oncekiKalib = kalib;
    cur = pending;
    cur.mode = std::clamp (cur.mode, 0, 2);
    drive = std::pow (10.0f, std::clamp (cur.driveDb, 0.0f, 36.0f) / 20.0f);
    kalib = kalibrasyon (cur.mode, drive);
    // tıksız geçiş: şekillendirici değiştiyse 25 ms eski↔yeni karışım (ilk kurulumda değil)
    if (! ilkParam && (oncekiMod != cur.mode || std::fabs (oncekiDrive - drive) > 1.0e-4f * drive))
    {
        eskiMod = oncekiMod; eskiDrive = oncekiDrive; eskiKalib = oncekiKalib;
        gecisKalan = gecisN;
    }
    const bool rampasiz = anlik || ilkParam;
    ilkParam = false;
    const double hpHz = cur.mode == 0 ? 300.0 : cur.mode == 1 ? 150.0 : 250.0;
    const double lpHz = std::clamp ((double) cur.toneHz, 2000.0, std::min (12000.0, sr * 0.45));
    for (int k = 0; k < MAX_CH; ++k)
    {
        busHp[(size_t) k].setCoeffs (design::highpass (sr, hpHz, 0.7071));
        busLp[(size_t) k].setCoeffs (design::lowpass (sr, lpHz, 0.7071));
        refLp[(size_t) k].setCoeffs (design::lowpass (sr, lpHz, 0.7071));
        dcHp[(size_t) k].setCoeffs (design::highpass (sr, DC_HP_HZ, 0.7071));
    }
    const float seviyeHedef = cur.levelDb <= SEVIYE_KAPALI_DB + 1.0e-3f
                                  ? 0.0f
                                  : std::pow (10.0f, std::clamp (cur.levelDb, SEVIYE_KAPALI_DB, 0.0f) / 20.0f);
    // kapalıyken/ilk kurulumda rampa yok: −36 ile açılan modül önceki seviyeden 20 ms kir sızdırıyordu
    if (rampasiz) seviye.setCurrentAndTarget (seviyeHedef); else seviye.setTarget (seviyeHedef);
    paramsDirty = false;
}

// Ortak gövde: yaz=false (idle) iken tüm hatlar beslenir, çıkışa dokunulmaz.
void DirtModule::isle (float* const* ch, int numCh, int n, bool yaz) noexcept
{
    if (paramsDirty) updateParams (! yaz);
    const int c = std::min (numCh, chans);
    const int mode = cur.mode;
    const float q = std::pow (2.0f, std::clamp (cur.bits, 4.0f, 16.0f) - 1.0f);
    const float invQ = 1.0f / q;
    const int holdN = std::clamp ((int) std::lround (std::clamp (cur.rate, 1.0f, 8.0f) * sr / 48000.0), 1, 32);
    izgaraKanal = c;

    for (int i = 0; i < n; ++i)
    {
        if (izgaraSay == 0) izgaraG0 = otoKazanc.getCurrent();
        const float gOto = otoKazanc.next();
        const float gOn = onKazanc.next();
        const float lvl = seviye.next();
        // geçiş ağırlığı (eski → yeni şekillendirici), örnek başına bir kez
        float wYeni = 1.0f;
        if (gecisKalan > 0)
        {
            wYeni = 1.0f - (float) gecisKalan / (float) gecisN;
            --gecisKalan;
        }
        for (int k = 0; k < c; ++k)
        {
            float x = ch[k][i];
            if (! (x > -1.0e6f && x < 1.0e6f)) x = 0.0f;
            const float dry = dryAlign[(size_t) k].processSample (x);

            const float bant = busHp[(size_t) k].processSample (x);
            const float refBant = refLp[(size_t) k].processSample (bant);     // veri yoluyla aynı bant sınırı
            izgaraIn += refBant * refBant;

            auto sekilKarisik = [&] (float s) noexcept
            {
                const float yeni = sekil (mode, drive, s) * kalib;
                if (wYeni >= 1.0f) return yeni;
                return wYeni * yeni + (1.0f - wYeni) * sekil (eskiMod, eskiDrive, s) * eskiKalib;
            };
            const float u0 = up[(size_t) k].tick (bant * gOn) * 2.0f;
            const float u1 = up[(size_t) k].tick (0.0f) * 2.0f;
            // kalibrasyon normalize bant (≈ REF) ölçeğinde: çıkışı yeniden bant ölçeğine döndür (1/gOn)
            float y = dn[(size_t) k].tick (sekilKarisik (u0)) / std::max (gOn, 1.0e-6f);
            dn[(size_t) k].tick (sekilKarisik (u1));
            y = dcHp[(size_t) k].processSample (y);                           // asimetrik şekillendiricinin DC'si

            if (mode == 2)
            {
                if (tutSay[(size_t) k] <= 0) { tut[(size_t) k] = std::round (y * gOn * q) * invQ / std::max (gOn, 1.0e-6f); tutSay[(size_t) k] = holdN; }
                --tutSay[(size_t) k];
                y = tut[(size_t) k];
            }
            y = busLp[(size_t) k].processSample (y);
            izgaraOut += y * y;
            if (yaz) ch[k][i] = dry + lvl * gOto * y;
        }
        if (++izgaraSay >= KONTROL_N) kontrolGuncelle (c);
    }
    for (int k = 0; k < c; ++k)
    {
        busHp[(size_t) k].sanitize();
        busLp[(size_t) k].sanitize();
        refLp[(size_t) k].sanitize();
        dcHp[(size_t) k].sanitize();
        if (! std::isfinite (tut[(size_t) k])) tut[(size_t) k] = 0.0f;
    }
}

// sabit KONTROL_N örneklik ızgara: zarflar + oto seviye + anlık tavan + ön kazanç
void DirtModule::kontrolGuncelle (int c) noexcept
{
    const int m = KONTROL_N;
    const float blokIn = izgaraIn / (float) (m * std::max (1, c)), blokOut = izgaraOut / (float) (m * std::max (1, c));
    izgaraIn = izgaraOut = 0.0f; izgaraSay = 0;
    const float kat = std::pow (envCoef, (float) m);
    envIn = blokIn + kat * (envIn - blokIn);
    envOut = blokOut + kat * (envOut - blokOut);
    isinma *= kat;
    if (! std::isfinite (envIn)) envIn = 0.0f;
    if (! std::isfinite (envOut)) envOut = 0.0f;
    const float envInD = envIn / std::max (1.0e-3f, 1.0f - isinma);

    // otomatik kalıntı seviyesi (kalibrasyon ana işi yaptı) + anlık tavan
    float hedef = izgaraG0;
    if (envInD > SESSIZ_GUC && envOut > 1.0e-12f)
    {
        float db = juce::jlimit (-24.0f, 12.0f, 10.0f * std::log10 (envIn / envOut));
        hedef = std::pow (10.0f, db / 20.0f);
        if (blokOut > 1.0e-20f) hedef = std::min (hedef, ANLIK_TAVAN * std::sqrt (blokIn / blokOut));
        sonOtoDb = 20.0f * std::log10 (std::max (hedef, 1.0e-6f));
    }
    else if (envInD <= SESSIZ_GUC)
        hedef = 0.0f;                                                      // sessizlik/oda: veri yolu kapanır
    otoKazanc.setTarget (hedef);

    // ön kazanç: YALNIZ sesli blokta, hızlı atak / yavaş bırakma
    if (blokIn > SESSIZ_GUC)
    {
        const float tau = blokIn > envOn ? 0.005f : 0.150f;
        const float a = (float) std::exp (-(double) m / ((double) tau * sr));
        envOn = blokIn + a * (envOn - blokIn);
        if (! std::isfinite (envOn)) envOn = blokIn;
        onKazanc.setTarget (juce::jlimit (0.25f, 63.0f, REF_RMS / std::sqrt (std::max (envOn, 1.0e-12f))));
    }
}

void DirtModule::processWet (float* const* ch, int numCh, int n) noexcept
{
    isle (ch, numCh, n, true);
}

void DirtModule::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    // gecikme hatları + filtreler + zarflar ısınmış kalır; çıkışa yazılmaz (ModuleBase kuru yolu çalar)
    isle (const_cast<float* const*> (ch), numCh, n, false);
}

} // namespace sweax
