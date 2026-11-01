// SWEAX NEXT — DSP/Saturator.cpp
#include "Saturator.h"
#include "../Chain/VocalChain.h"
#include <cmath>

namespace sweax
{

namespace
{
    constexpr double PI = 3.141592653589793;
    constexpr double BANT_VURGU_HZ = 3000.0, BANT_VURGU_DB = 3.0;
    constexpr double KIRIK_ON = 1.4125375446227544;      // +3 dB iç sürüş ('Punish')
    constexpr double KIRIK_POZ = 0.5, KIRIK_NEG = 0.9;   // asimetrik diz seviyeleri (pozitif yarım dalga önce kırpılır)
    // RBJ high-shelf (Audio EQ Cookbook), S = 1
    BiquadCoeffs highShelf (double sr, double fc, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * PI * fc / sr, cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw * 0.5 * std::sqrt ((A + 1.0 / A) * (1.0 / 1.0 - 1.0) + 2.0);
        const double b0 = A * ((A + 1) + (A - 1) * cw + 2.0 * std::sqrt (A) * alpha);
        const double b1 = -2.0 * A * ((A - 1) + (A + 1) * cw);
        const double b2 = A * ((A + 1) + (A - 1) * cw - 2.0 * std::sqrt (A) * alpha);
        const double a0 = (A + 1) - (A - 1) * cw + 2.0 * std::sqrt (A) * alpha;
        const double a1 = 2.0 * ((A - 1) - (A + 1) * cw);
        const double a2 = (A + 1) - (A - 1) * cw - 2.0 * std::sqrt (A) * alpha;
        BiquadCoeffs c;
        c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0; c.a1 = a1 / a0; c.a2 = a2 / a0;
        return c;
    }
}

Saturator::Egri Saturator::egriHesapla (float driveDb) noexcept
{
    Egri r;
    r.drive = std::pow (10.0f, std::max (0.0f, driveDb) * 0.05f);
    r.invDrive = 1.0f / std::max (1.0e-3f, r.drive);
    r.biasT = std::tanh (BIAS);
    r.invSmall = 1.0f / (1.0f - r.biasT * r.biasT);          // sech²(b) küçük-sinyal telafisi
    r.sert = std::clamp ((driveDb - 8.0f) / 4.0f, 0.0f, 1.0f);   // SERT MOD: 8 dB üstü sürüşte açılır
    r.sertH = 1.0f + 2.0f * r.sert;
    r.sertInvH = 1.0f / r.sertH;
    r.kDrive = (double) r.drive * KIRIK_ON;
    r.kInvDrive = 1.0 / r.kDrive;
    // KIRIK SEVİYE EŞİTLEME (M3.5 denetimi, bulgu 6): karakterler arası A/B seviye farkıyla yanıltmasın diye
    // −18 dBFS 1 kHz referansta Kırık çıkışı Tüp'e eşitlenir (ölçülen fark drive 12'de −1.86 dB'ydi; Bant
    // zaten ±0.2 dB içinde, telafisiz bırakıldı). Telafi STATİK ve k·f(x/k) biçiminde: küçük-sinyal eğimi
    // birebir 1 kalır, yalnız tavan k kadar yükselir. k, 64 noktalı sinüs karesel ortalamasıyla (DC'si
    // alınmış — modüldeki DC engelleyiciye denk) sabit nokta yinelemesiyle çözülür; tahsissiz, RT güvenli.
    {
        constexpr int N = 64;
        constexpr double AMP = 0.12589254;               // −18 dBFS
        auto rmsDc = [&r] (bool kirikMi) noexcept -> double
        {
            double kare = 0.0, ort = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const float x = (float) (AMP * std::sin (2.0 * PI * (double) i / (double) N));
                const double y = (double) (kirikMi ? egriKirik (r, x) : egriTup (r, x));
                kare += y * y; ort += y;
            }
            ort /= (double) N;
            return std::sqrt (std::max (1.0e-30, kare / (double) N - ort * ort));
        };
        const double hedefRms = rmsDc (false);           // Tüp'ün o sürüşteki seviyesi (SERT MOD dahil)
        double k = 1.0;
        for (int it = 0; it < 6; ++it)
        {
            r.kSev = k; r.kSevInv = 1.0 / k;
            k = std::clamp (k * hedefRms / rmsDc (true), 0.5, 2.0);
        }
        r.kSev = k; r.kSevInv = 1.0 / k;
    }
    return r;
}

// 0 TÜP — M2/M3.3 eğrisi (aritmetik sırası birebir korunur)
inline float Saturator::egriTup (const Egri& c, float s) noexcept
{
    const float xd = c.drive * s;
    const float y1 = (std::tanh (xd + BIAS) - c.biasT) * c.invDrive * c.invSmall;
    // SERT MOD (M3.3, trap metal): drive > 8 dB'de ikinci tanh kademesi karışır —
    // küçük-sinyal birim kalır (tanh(h·y)/h), tavan ezilir (grit); monoton, OS içinde.
    if (c.sert <= 0.0f) return y1;
    return y1 + c.sert * (std::tanh (c.sertH * y1) * c.sertInvH - y1);
}

// 1 BANT — simetrik, yalnız tek harmonik
inline float Saturator::egriBant (const Egri& c, float s) noexcept
{
    return std::tanh (c.drive * s) * c.invDrive;
}

// 2 KIRIK — asimetrik diz, monoton, küçük sinyal birim
inline float Saturator::egriKirik (const Egri& c, float s) noexcept
{
    // SEVİYE EŞİTLEME (denetim, bulgu 6): y = k·f(x/k) — tavan k kadar yükselir, f'(0) = 1 korunur.
    // k, egriHesapla'da −18 dBFS sinüs RMS'i Tüp'ünkine eşitleyecek şekilde çözülür (aşağıda).
    const double u = c.kDrive * c.kSevInv * (double) s;
    const double v = u / (u >= 0.0 ? KIRIK_POZ : KIRIK_NEG);
    return (float) (u / std::sqrt (1.0 + v * v) * c.kInvDrive * c.kSev);
}

float Saturator::transfer (int character, float driveDb, float x) noexcept
{
    const Egri c = egriHesapla (driveDb);
    switch (std::clamp (character, 0, KARAKTER_SAYISI - 1))
    {
        case 1:  return egriBant (c, x);
        case 2:  return egriKirik (c, x);
        default: return egriTup (c, x);
    }
}

void Saturator::prepare (double sampleRate, int maxBlock, int numChannels)
{
    prepareBase (sampleRate, maxBlock, numChannels, LATENCY);
    setDryDelay (LATENCY);
    for (auto& f : up) f.design();
    for (auto& f : dn) f.design();
    for (auto& d : dryAlign) d.prepare (LATENCY, LATENCY);
    dcCoef = (float) std::exp (-2.0 * PI * 10.0 / sampleRate);   // ~10 Hz DC engelleyici
    mixS.prepare (sampleRate, 15.0);
    mixS.setCurrentAndTarget (1.0f);
    // Bant vurgu çifti 2× hızda (şekillendiriciyle aynı alan); ±3 dB RBJ shelf birbirinin tam tersi
    for (auto& b : bantOn) b.setCoeffs (highShelf (2.0 * sampleRate, BANT_VURGU_HZ, BANT_VURGU_DB));
    for (auto& b : bantArka) b.setCoeffs (highShelf (2.0 * sampleRate, BANT_VURGU_HZ, -BANT_VURGU_DB));
    gecisN = std::max (1, (int) std::lround (GECIS_MS * 0.001 * sampleRate));
    paramsDirty = true;
    resetWet();
}

void Saturator::updateParams() noexcept
{
    cur = pending;
    e = egriHesapla (cur.driveDb);
    for (int k = 0; k < chans; ++k) tone[(size_t) k].setCoeffs (highShelf (sr, 1800.0, (double) cur.toneDb));
    mixS.setTarget (std::clamp (cur.mix, 0.0f, 1.0f));
    const int yeni = std::clamp (cur.character, 0, KARAKTER_SAYISI - 1);
    if (bosDurum)
    {
        // prepare/reset sonrası, hiç örnek İŞLENMEDEN: durum boş, rampa gereksiz (duyulacak bir şey yok)
        karakter = yeni;
        karW.fill (0.0f); karW[(size_t) yeni] = 1.0f; karBas = karW;
        gecisKalan = 0;
    }
    else if (yeni != karakter)
    {
        // Bant vurgu durumları YALNIZ ağırlığı sıfırken bayatlar (ağırlık > 0 ise her örnek tıklanıyor)
        if (yeni == 1 && karW[(size_t) 1] <= 0.0f)
        {
            for (auto& b : bantOn) b.reset();
            for (auto& b : bantArka) b.reset();
        }
        // DENETİM DÜZELTMESİ: yeni rampa O ANKİ KARIŞIMDAN başlar. Eskiden `eskiKarakter = karakter` +
        // `gecisKalan = gecisN` yapılıyordu; geçiş SÜRERKEN ikinci değişim gelince ağırlık 0'a DÖNÜYOR ve çıkış
        // tek örnekte (1−w)·(hedef − önceki) kadar BASAMAK yapıyordu. Ölçüldü (48k, 220 Hz 0.40, drive 6 dB,
        // blok 32/64, 8 giriş fazının en kötüsü): kararlı adım 0.01181; 0→2 sonra +64 örnekte 2→1 = 0.08726
        // (×7.39), +128 → ×6.57, +256 → ×5.75, +512 → ×3.65, +768 → ×1.66, +960 (geçiş bitmiş) → ×1.00.
        karBas = karW;
        karakter = yeni;
        gecisKalan = gecisN;
    }
    paramsDirty = false;
}

float Saturator::sekilTek (int kar, int k, float s) noexcept
{
    if (kar == 1)
    {
        const float p = bantOn[(size_t) k].processSample (s);
        return bantArka[(size_t) k].processSample (egriBant (e, p));
    }
    if (kar == 2) return egriKirik (e, s);
    return egriTup (e, s);
}

void Saturator::processWet (float* const* ch, int numCh, int n) noexcept
{
    if (paramsDirty) updateParams();
    const int c = std::min (numCh, chans);
    for (int i = 0; i < n; ++i)
    {
        const float m = mixS.next();
        // karakter ağırlık vektörü (örnek başına bir kez): rampa karBas'tan hedefe DOĞRUSAL, son örnekte tam 1
        if (gecisKalan > 0)
        {
            --gecisKalan;
            const float t = 1.0f - (float) gecisKalan / (float) gecisN;      // ilk örnek 1/N, son örnek tam 1
            for (int k = 0; k < KARAKTER_SAYISI; ++k)
            {
                const float hedef = k == karakter ? 1.0f : 0.0f;
                karW[(size_t) k] = karBas[(size_t) k] + t * (hedef - karBas[(size_t) k]);
            }
        }
        const bool tupSade = gecisKalan == 0 && karakter == 0;
        for (int k = 0; k < c; ++k)
        {
            float x = ch[k][i];
            if (! (x > -1.0e6f && x < 1.0e6f)) x = 0.0f;
            const float dry = dryAlign[(size_t) k].processSample (x);
            // ---- 2× yukarı (sıfır doldur + HB, ×2 kazanç), şekillendir, 2× aşağı ----
            auto shape = [&] (float s) noexcept
            {
                if (tupSade) return egriTup (e, s);                   // M2 yolu (birebir)
                float y = 0.0f;
                // ağırlığı sıfır olan şekillendirici HESAPLANMAZ (Bant'ın vurgu durumu da o sırada donar —
                // yeniden devreye girerken updateParams sıfırlıyor)
                for (int q = 0; q < KARAKTER_SAYISI; ++q)
                    if (karW[(size_t) q] != 0.0f) y += karW[(size_t) q] * sekilTek (q, k, s);
                return y;
            };
            const float u0 = up[(size_t) k].tick (x) * 2.0f;
            const float u1 = up[(size_t) k].tick (0.0f) * 2.0f;
            const float s0 = dn[(size_t) k].tick (shape (u0));
            dn[(size_t) k].tick (shape (u1));                   // atılan faz (decimasyon)
            // ---- DC engelleyici ----
            float& dc = dcState[(size_t) k];
            const float y = s0 - dc;
            dc = dc * dcCoef + s0 * (1.0f - dcCoef);
            // ---- tone + mix ----
            const float t = tone[(size_t) k].processSample (y);
            ch[k][i] = dry + (t - dry) * m;
        }
    }
    if (n > 0) bosDurum = false;                         // artık durum "boş" değil: sonraki karakter RAMPAlanır
    // sigorta: IIR durumları sonlu kalsın (NaN kalıcılığı denetimde ölçüldü)
    for (int k = 0; k < c; ++k)
    {
        if (! std::isfinite (dcState[(size_t) k])) dcState[(size_t) k] = 0.0f;
        tone[(size_t) k].sanitize();
        bantOn[(size_t) k].sanitize();
        bantArka[(size_t) k].sanitize();
    }
}

void Saturator::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    // kapalıyken karakter değişimi / yarım kalan geçiş: duyulmuyor → anında (yeniden açılışta 20 ms karışım yok)
    karakter = std::clamp (pending.character, 0, KARAKTER_SAYISI - 1);
    karW.fill (0.0f); karW[(size_t) karakter] = 1.0f; karBas = karW;
    gecisKalan = 0;
    for (auto& b : bantOn) b.reset();                    // vurgu durumu bayatlamasın (açılış 10 ms crossfade ile)
    for (auto& b : bantArka) b.reset();
    const int c = std::min (numCh, chans);
    for (int k = 0; k < c; ++k)
        for (int i = 0; i < n; ++i)
        {
            float x = ch[k][i];
            if (! (x > -1.0e6f && x < 1.0e6f)) x = 0.0f;   // denetim: NaN, FIR üzerinden IIR'lara sızıyordu
            dryAlign[(size_t) k].processSample (x);
            up[(size_t) k].tick (x);
            up[(size_t) k].tick (0.0f);
            dn[(size_t) k].tick (0.0f);
            dn[(size_t) k].tick (0.0f);
        }
}

void Saturator::resetWet()
{
    for (auto& f : up) f.reset();
    for (auto& f : dn) f.reset();
    for (auto& d : dryAlign) d.reset();
    for (auto& b : tone) b.reset();
    for (auto& b : bantOn) b.reset();
    for (auto& b : bantArka) b.reset();
    dcState.fill (0.0f);
    gecisKalan = 0;
    karW.fill (0.0f); karW[(size_t) karakter] = 1.0f; karBas = karW;
    bosDurum = true;                                     // İLK örnek işlenene kadar: karakter anında uygulanır
}

} // namespace sweax
