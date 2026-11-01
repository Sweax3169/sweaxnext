// SWEAX NEXT — DSP/PlateReverb.cpp
// Dattorro plate: tüm gecikme/tap değerleri yayının 29761 Hz referans tablosundan, sr'a ölçekli.
// M3.5: oda tipi = aynı tank, tip ölçekli uzunluklar + difüzyon/modülasyon + (Oda/Chamber) erken yansıma.
#include "PlateReverb.h"
#include "../Chain/VocalChain.h"
#include <cmath>

namespace sweax
{

namespace
{
    constexpr double PI = 3.141592653589793;
    constexpr double REF_SR = 29761.0;
    constexpr double ER_MAX_MS = 65.0;
    BiquadCoeffs butterHp (double sr, double fc) noexcept
    {
        const double w0 = 2.0 * PI * std::clamp (fc, 20.0, 0.45 * sr) / sr, cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * 0.70710678), a0 = 1.0 + alpha;
        BiquadCoeffs c;
        c.b0 = (1.0 + cw) * 0.5 / a0; c.b1 = -(1.0 + cw) / a0; c.b2 = (1.0 + cw) * 0.5 / a0;
        c.a1 = -2.0 * cw / a0; c.a2 = (1.0 - alpha) / a0;
        return c;
    }
    inline float coefFromMs (double sr, double ms) noexcept { return (float) std::exp (-1.0 / (std::max (0.01, ms) * 0.001 * sr)); }

    // Tip tablosu. Plate satırı M2 sabitleriyle BİREBİR (bit-birebir özet testi bunu kilitler).
    struct TipAyar
    {
        double olcek;                 // tank + difüzör uzunluk ölçeği
        float  difA, difB;            // giriş difüzyonu (142/107 çifti, 379/277 çifti)
        float  tankG, tankG2;         // tank modüle AP g (decay diffusion 1) / tank AP g (decay diffusion 2)
        double modHz, modDerinlik;    // LFO hızı, sapma çarpanı (Dattorro ±8 örnek × bu)
        float  bant;                  // giriş bant LP katsayısı (0.05 = bandwidth 0.95)
        double sonumCarpan;           // damping fc çarpanı (1/√ölçek × parlaklık) — Plate'te kullanılmaz
        double kisaDuzelt;            // kısa decay eşleme düzeltmesi (bulgu 4; turevHesapla'da, Plate'te kullanılmaz)
        int    er;                    // erken yansıma tap sayısı (0 = yok)
        float  erSeviye;              // erken yansıma veri yolu kazancı (tap tablosu × bu)
        float  tankKazanc;            // tip seviye eşlemesi (kuyruk + ER birlikte; Plate'e göre) — Plate'te uygulanmaz
        float  erMsL[7], erGL[7], erMsR[7], erGR[7];
    };
    // erken yansıma tap kazançları −6 … −9 dB (0.5 dB adım), L/R işaret desenleri farklı (alt frekans tarak birikmesi
    // yok). ER veri yolu artık tankKazanc'ın İÇİNDE (denetim, bulgu 10): tipler arası tepe (slap) seviyesi de
    // eşlensin diye erSeviye, dürtü tepesi Plate'in ±1 dB'sine gelecek şekilde ölçülerek ayarlandı (test bağlar).
    const TipAyar TIPLER[PlateReverb::TIP_SAYISI] =
    {
        // 0 PLATE
        { 1.0, 0.75f, 0.625f, 0.70f, 0.5f, 1.0, 1.0, 0.05f, 1.0, 0.0, 0, 0.0f, 1.0f, {}, {}, {}, {} },
        // 1 ODA — kısa tank, sığ modülasyon, 5–45 ms yansımalar
        { 0.42, 0.75f, 0.625f, 0.70f, 0.5f, 1.3, 0.5, 0.05f, 1.0 / std::sqrt (0.42), 0.0, 7, 0.11f, 0.88f,
          { 5.3f, 9.1f, 13.7f, 19.3f, 25.9f, 34.1f, 44.3f },
          { 0.5012f, -0.4732f, 0.4467f, 0.4217f, -0.3981f, 0.3758f, -0.3548f },
          { 6.7f, 11.3f, 16.1f, 22.9f, 29.3f, 38.7f, 42.1f },
          { 0.5012f, 0.4732f, -0.4467f, 0.4217f, -0.3981f, -0.3758f, 0.3548f } },
        // 2 SALON — M3.5 denetimi (bulgu 7) YENİDEN DENGELENDİ: eski ölçek 1.55 + düşük difüzyon (0.55/0.45)
        // kuyruğu dört tipin EN SEYREĞİ yapıyordu (Abel–Huang yoğunluk @300 ms 0.48, Plate 0.79; tank tipik
        // decay'de döngü başına −45 dB kaybediyordu) ve 0.5 s altı T60 topolojik olarak ULAŞILAMAZDI (döngü
        // 1.12 s). Şimdi ölçek 1.12 + Plate düzeyinde difüzyon; 'büyük alan' hissi derin/yavaş modülasyon +
        // görece geç yoğunlaşan başlangıçla korunur (erken enerji sırası Oda > Plate > Salon testte).
        { 1.12, 0.75f, 0.625f, 0.70f, 0.55f, 0.6, 1.4, 0.05f, 1.0 / std::sqrt (1.12), 0.30, 0, 0.0f, 0.96f, {}, {}, {}, {} },
        // 3 CHAMBER — orta tank, yüksek difüzyon, parlak (sönüm fc ×1.3), 10–60 ms yansımalar
        { 0.70, 0.80f, 0.70f, 0.72f, 0.55f, 0.9, 0.8, 0.05f, 1.3 / std::sqrt (0.70), 0.35, 7, 0.19f, 0.84f,
          { 10.3f, 16.9f, 23.1f, 30.7f, 38.3f, 47.9f, 59.1f },
          { 0.5012f, -0.4732f, 0.4467f, 0.4217f, -0.3981f, 0.3758f, -0.3548f },
          { 12.1f, 19.7f, 26.3f, 34.9f, 42.7f, 51.1f, 56.3f },
          { 0.5012f, 0.4732f, -0.4467f, 0.4217f, -0.3981f, -0.3758f, 0.3548f } },
    };
}

void PlateReverb::uzunlukHesapla (int tip, Uzunluk& u) const noexcept
{
    const TipAyar& a = TIPLER[(size_t) tip];
    const double olcek = scale * a.olcek;                            // Plate: scale·1.0 == scale (birebir)
    auto T  = [olcek] (int ref) noexcept { return std::max (1, (int) std::lround (olcek * ref)); };
    auto Tf = [olcek] (double ref) noexcept { return std::max (1, (int) std::lround (olcek * ref)); };
    // giriş difüzörleri (142/107/379/277)
    u.dif[0] = T (142); u.dif[1] = T (107); u.dif[2] = T (379); u.dif[3] = T (277);
    // tank: modüle AP 672/908, sapma payı 16·derinlik
    u.modD[0] = T (672); u.modD[1] = T (908);
    u.exc = Tf (8.0 * a.modDerinlik);
    u.modPay = Tf (16.0 * a.modDerinlik);
    u.d1[0] = T (4453); u.d3[0] = T (3720); u.d1[1] = T (4217); u.d3[1] = T (3163);
    u.ap2[0] = T (1800); u.ap2[1] = T (2656);
    // çıkış tapları (Dattorro tablo): L 266 2974 1913 1996 1990 187 1066 / R 353 3627 1228 2673 2111 335 121
    constexpr int CIKIS[14] = { 266, 2974, 1913, 1996, 1990, 187, 1066, 353, 3627, 1228, 2673, 2111, 335, 121 };
    for (int k = 0; k < 14; ++k) u.cikis[k] = T (CIKIS[k]);
    for (int k = 0; k < 7; ++k)
    {
        u.er[0][k] = a.er > 0 ? std::max (1, (int) std::lround (a.erMsL[k] * 0.001 * sr)) : 1;
        u.er[1][k] = a.er > 0 ? std::max (1, (int) std::lround (a.erMsR[k] * 0.001 * sr)) : 1;
    }
}

void PlateReverb::prepare (double sampleRate, int maxBlock, int numChannels)
{
    prepareBase (sampleRate, maxBlock, numChannels, 0);
    setDryDelay (0);
    scale = sampleRate / REF_SR;
    pre.init ((int) std::lround (0.130 * sampleRate) + 4);
    // kapasiteler: tüm tiplerin en büyüğü (tip değişimi tahsissiz)
    Uzunluk mx {};
    int modCapL = 0, modCapR = 0;
    for (int t = 0; t < TIP_SAYISI; ++t)
    {
        Uzunluk u {};
        uzunlukHesapla (t, u);
        for (int k = 0; k < 4; ++k) mx.dif[k] = std::max (mx.dif[k], u.dif[k]);
        for (int k = 0; k < 2; ++k)
        {
            mx.d1[k] = std::max (mx.d1[k], u.d1[k]); mx.d3[k] = std::max (mx.d3[k], u.d3[k]);
            mx.ap2[k] = std::max (mx.ap2[k], u.ap2[k]);
        }
        modCapL = std::max (modCapL, u.modD[0] + u.modPay + 4);
        modCapR = std::max (modCapR, u.modD[1] + u.modPay + 4);
    }
    dif1.allocate (mx.dif[0]); dif2.allocate (mx.dif[1]); dif3.allocate (mx.dif[2]); dif4.allocate (mx.dif[3]);
    modRingL.init (modCapL); modRingR.init (modCapR);
    d1L.init (mx.d1[0] + 2); d3L.init (mx.d3[0] + 2);
    d1R.init (mx.d1[1] + 2); d3R.init (mx.d3[1] + 2);
    ap2L.allocate (mx.ap2[0]); ap2R.allocate (mx.ap2[1]);
    erHat.init ((int) std::lround (ER_MAX_MS * 0.001 * sampleRate) + 4);
    duckRise = coefFromMs (sampleRate, 5.0);
    duckFall = coefFromMs (sampleRate, 250.0);
    mixS.prepare (sampleRate, 15.0);
    mixS.setCurrentAndTarget (0.0f);
    tipAdim = 1.0f / std::max (1.0f, (float) (TIP_GECIS_MS * 0.001 * sampleRate));
    hedefTip = std::clamp (pending.type, 0, TIP_SAYISI - 1);
    tipUygula (hedefTip);
    tipDurum = 0; tipFade = 1.0f; girisFade = 1.0f;
    paramsDirty = true;
    resetWet();
}

// RT güvenli: yalnız mantıksal uzunluk + katsayı + halka temizliği (kapasite prepare'da ayrıldı)
void PlateReverb::tipUygula (int tip) noexcept
{
    tip = std::clamp (tip, 0, TIP_SAYISI - 1);
    const TipAyar& a = TIPLER[(size_t) tip];
    Uzunluk u {};
    uzunlukHesapla (tip, u);
    dif1.set (u.dif[0], a.difA); dif2.set (u.dif[1], a.difA);
    dif3.set (u.dif[2], a.difB); dif4.set (u.dif[3], a.difB);
    modDL = u.modD[0]; modDR = u.modD[1];
    modRingL.setLen (modDL + u.modPay + 4); modRingR.setLen (modDR + u.modPay + 4);
    excN = u.exc;
    tankG = a.tankG;
    d1L.setLen (u.d1[0] + 2); d3L.setLen (u.d3[0] + 2);
    d1R.setLen (u.d1[1] + 2); d3R.setLen (u.d3[1] + 2);
    n1L = u.d1[0]; n3L = u.d3[0]; n1R = u.d1[1]; n3R = u.d3[1];
    ap2L.set (u.ap2[0], a.tankG2); ap2R.set (u.ap2[1], a.tankG2);
    for (int k = 0; k < 14; ++k) cikisTap[k] = u.cikis[k];
    bantK = a.bant;
    tankK = a.tankKazanc;
    lfoInc = 2.0 * PI * a.modHz / sr;
    erSayi = a.er;
    for (int k = 0; k < 7; ++k)
    {
        erTapL[k] = u.er[0][k]; erTapR[k] = u.er[1][k];
        erGL[k] = a.erGL[k] * a.erSeviye; erGR[k] = a.erGR[k] * a.erSeviye;
    }
    dampL = dampR = 0.0f; fbL = fbR = 0.0f;
    erHat.reset();                                       // bayat yansıma yok (Plate'te hat beslenmiyordu)
    aktifTip = tip;
    turevHesapla();
}

void PlateReverb::turevHesapla() noexcept
{
    // sönüm: tek kutup LP katsayısı (yüksek dampHz → az sönüm)
    const double fcK = std::clamp ((double) cur.dampHz, 1000.0, 16000.0);
    if (aktifTip == 0)
        dampCoef = (float) std::exp (-2.0 * PI * fcK / sr);
    else
        dampCoef = (float) std::exp (-2.0 * PI * std::min (fcK * TIPLER[(size_t) aktifTip].sonumCarpan, 0.45 * sr) / sr);
    // decay: T60'tan — tam döngü örnekleri (iki yarımın toplamı, TİP ÖLÇEKLİ), döngü başına 4 decay
    const double loopSec = (double) (modDL + n1L + ap2L.d + n3L + modDR + n1R + ap2R.d + n3R) / sr;
    // KISA DECAY DİFÜZYON FRENİ (denetim, bulgu 4): tank allpass'ları decayG'den BAĞIMSIZ kendi başına çınlar
    // (ap2 g 0.5, tur 90–100 ms → tek başına ~0.45 s; modüle AP g 0.7 → ~0.6 s) ve 0.5 s altı T60'ı tabanlıyordu
    // (ölçüldü: decayG tabandayken Plate 0.46 s, Salon 0.51 s). 0.6 s altında tank difüzyon kazançları decay'le
    // doğrusal kısılır (en çok ×0.5); 0.6 s ve üstünde çarpan TAM 1 → Plate özet yolları (decay ≥ 0.8) bit-birebir.
    {
        const TipAyar& a = TIPLER[(size_t) aktifTip];
        const float fren = (float) std::clamp ((double) std::max (0.2f, cur.decaySec) / 0.6, 0.5, 1.0);
        tankG = a.tankG * fren;
        ap2L.g = a.tankG2 * fren;                        // Ap::set ÇAĞRILMAZ (halkayı sıfırlar) — yalnız kazanç
        ap2R.g = a.tankG2 * fren;
    }
    if (aktifTip == 0)
    {
        // PLATE: M2 eşlemesi BİREBİR (özet testleri decay ≥ 0.8 s'yi kilitler). Tek fark (denetim, bulgu 4):
        // alt klemp 0.15 → 0.01 — 0.15'e ancak decay < 0.66 s'de iniliyordu, yani decay ≥ 0.66 s bit-birebir.
        // Eski tabanla decay 0.2–0.6 s ATILDI (hepsi T60 ≈ 0.70 s veriyordu); şimdi 0.3 s'de ~0.35 s ölçülüyor.
        // Sönüm kaybı düzeltmesi Plate'e UYGULANMAZ (aynı bit-birebir zorunluluğu): güçlü sönümde T60 kısalır
        // (damp 3 kHz, decay 3 s → ×0.76) — M2'den beri bilinen davranış, başlıkta belgeli.
        const double g = std::pow (10.0, -3.0 * loopSec / (4.0 * std::max (0.2f, cur.decaySec)));
        decayG = (float) std::clamp (g, 0.01, 0.93);
    }
    else
    {
        const TipAyar& a = TIPLER[(size_t) aktifTip];
        // KISA UÇ DÜZELTMESİ (denetim, bulgu 4): Schroeder eğimi kısa decay'lerde difüzör/ER başının sabit
        // süresi yüzünden hedefin üstünde ölçülüyordu (Chamber 0.3 s → 0.377 s, ×1.26). 0.7 s altında hedef
        // tip başına ölçülü katsayıyla kısılır; 0.7 s ve üstü birebir (C1 sürekli, işitilir sıçrama yok).
        double t60 = (double) std::max (0.2f, cur.decaySec);
        const double eksik = std::max (0.0, 1.0 - t60 / 0.7);
        t60 *= 1.0 - a.kisaDuzelt * eksik * eksik;
        double g = std::pow (10.0, -3.0 * loopSec / (4.0 * t60));
        // SÖNÜM DÖNGÜ KAYBI (denetim, bulgu 4): damp LP'si döngü başına 2 kez uygulanır ve geniş bant T60'ı
        // kısaltır (damp 3 kHz'te ×0.73'e dek ölçüldü). Kuyruk enerjisinin ağırlık merkezini temsil eden
        // frekansta LP genliği D alınır; hedef döngü kaybı g⁴·D² olduğundan decay başına düzeltme D^(1/2) =
        // h2^(1/4). Temsil frekansı döngü uzunluğuyla ölçeklenir (kısa döngü = saniyede daha çok pas = tayf
        // daha çabuk LF'e çöker → pas başına gerçek kayıp küçülür; sabit 2.5 kHz Oda'da damp 3 kHz decay 3 s'de
        // ×1.26'ya AŞIRI düzeltiyordu, ölçekli seçim ×1.05'e indirdi). dampHz 12 kHz'te D ≈ 1 → fiilen kapalı.
        const double aC = (double) dampCoef;
        const double fRep = std::clamp (2600.0 * std::pow (loopSec, 0.25), 800.0, 0.4 * sr);
        const double w = 2.0 * PI * fRep / sr;
        const double h2 = (1.0 - aC) * (1.0 - aC) / std::max (1.0e-6, 1.0 + aC * aC - 2.0 * aC * std::cos (w));
        g /= std::pow (std::min (1.0, h2), 0.25);
        decayG = (float) std::clamp (g, 0.005, 0.93);
    }
}

void PlateReverb::updateParams() noexcept
{
    cur = pending;
    preN = std::clamp ((int) std::lround (cur.preDelayMs * 0.001 * sr), 0, pre.len - 2);
    inHp.setCoeffs (butterHp (sr, cur.hpHz));
    hedefTip = std::clamp (cur.type, 0, TIP_SAYISI - 1);
    if (bosDurum)
    {
        // prepare/reset sonrası, hiç örnek İŞLENMEDEN: tank boş → geçiş anında (sön-aç gereksiz, duyulmuyor)
        if (hedefTip != aktifTip) tipUygula (hedefTip);
        tipDurum = 0; tipFade = 1.0f; girisFade = 1.0f;
    }
    turevHesapla();
    widthAmt = std::clamp (cur.width, 0.0f, 1.0f);
    duckAmt = std::clamp (cur.duckDb, 0.0f, 18.0f);
    mixS.setTarget (std::clamp (cur.mix, 0.0f, 1.0f));
    paramsDirty = false;
}

void PlateReverb::tickTank (float in, float& outL, float& outR) noexcept
{
    // LFO (iki yarım 90° faz farklı)
    lfo += lfoInc; if (lfo > 2.0 * PI) lfo -= 2.0 * PI;
    const double excL = (double) excN * (1.0 + std::sin (lfo));
    const double excR = (double) excN * (1.0 + std::cos (lfo));

    // sol yarım: giriş + sağdan geri besleme
    {
        const float x = in + fbR;
        // modüle allpass (Plate g = 0.70): kesirli tap
        const float t = modRingL.tapFrac ((double) modDL + excL);
        const float y = t - tankG * x;
        modRingL.push (x + tankG * y);
        // gecikme 1 → sönüm → decay
        d1L.push (y);
        float s = d1L.tap (n1L);
        dampL = s + dampCoef * (dampL - s);                    // tek kutup LP
        s = dampL * decayG;
        // allpass 2 → gecikme 2 → decay → çapraza
        s = ap2L.tick (s);
        d3L.push (s);
        fbL = d3L.tap (n3L) * decayG;
    }
    // sağ yarım: giriş + soldan geri besleme
    {
        const float x = in + fbL;
        const float t = modRingR.tapFrac ((double) modDR + excR);
        const float y = t - tankG * x;
        modRingR.push (x + tankG * y);
        d1R.push (y);
        float s = d1R.tap (n1R);
        dampR = s + dampCoef * (dampR - s);
        s = dampR * decayG;
        s = ap2R.tick (s);
        d3R.push (s);
        fbR = d3R.tap (n3R) * decayG;
    }
    // çıkış tapları (Dattorro tablo, tip ölçekli; 0.6 ölçek)
    const int* k = cikisTap;
    outL = 0.6f * (  d1R.tap (k[0]) + d1R.tap (k[1])
                   - ap2R.r.tap (k[2])
                   + d3R.tap (k[3])
                   - d1L.tap (k[4])
                   - ap2L.r.tap (k[5])
                   - d3L.tap (k[6]));
    outR = 0.6f * (  d1L.tap (k[7]) + d1L.tap (k[8])
                   - ap2L.r.tap (k[9])
                   + d3L.tap (k[10])
                   - d1R.tap (k[11])
                   - ap2R.r.tap (k[12])
                   - d3R.tap (k[13]));
}

void PlateReverb::processWet (float* const* ch, int numCh, int n) noexcept
{
    if (paramsDirty) updateParams();
    const int c = std::min (numCh, chans);
    for (int i = 0; i < n; ++i)
    {
        // tip geçişi: ÇIKIŞ söner (1) → temizle + uzunluk değiştir → boş tanka GİRİŞ rampası (2). Giriş rampası
        // sürerken yeni hedef gelirse çıkış tam seviyeden söner, giriş rampası kesilmeden sürer (iki kazanç ayrı).
        // DURUM 3 = İPTAL (denetim düzeltmesi): sönüş sürerken kullanıcı ESKİ tipe dönerse tank SİLİNMEZ, çıkış
        // aynı rampayla geri açılır. Eskiden sönüş koşulsuz tamamlanıp tipUygula çağrılıyordu → hiçbir şeyin
        // değişmemesi gereken durumda kuyruk çukuru oluşuyordu (ölçüldü: T+100 ms'de ıslak RMS −5.5 dB).
        if (hedefTip != aktifTip)
        {
            if (tipDurum == 0 || tipDurum == 2) { tipDurum = 1; tipFade = 1.0f; }
            else if (tipDurum == 3) tipDurum = 1;                 // iptal geri alındı: sönüşe kaldığı yerden devam
        }
        else if (tipDurum == 1) tipDurum = 3;                     // İPTAL
        if (tipDurum == 1)
        {
            tipFade -= tipAdim;
            if (tipFade <= 0.0f) { tipFade = 0.0f; tipUygula (hedefTip); tipDurum = 2; girisFade = 0.0f; }
        }
        else if (tipDurum == 3)
        {
            tipFade += tipAdim;
            if (tipFade >= 1.0f) { tipFade = 1.0f; tipDurum = 0; }
        }
        if (girisFade < 1.0f)
        {
            girisFade = std::min (1.0f, girisFade + tipAdim);
            if (tipDurum == 2 && girisFade >= 1.0f) { tipDurum = 0; tipFade = 1.0f; }
        }

        float in = ch[0][i];
        if (c > 1) in = 0.5f * (in + ch[1][i]);
        if (! (in > -1.0e6f && in < 1.0e6f)) in = 0.0f;

        // ducking zarfı (kuru)
        const float a = std::fabs (in);
        duckEnv = a + (a > duckEnv ? duckRise : duckFall) * (duckEnv - a);
        const float envDb = duckEnv > 1.0e-6f ? 20.0f * std::log10 (duckEnv) : -120.0f;
        const float tt = std::clamp ((envDb + 50.0f) / 20.0f, 0.0f, 1.0f);
        const float duckGain = std::pow (10.0f, -duckAmt * tt * 0.05f);

        // pre-delay → HP → bant LP → difüzörler → tank
        pre.push (in);
        float x = pre.tap (preN);
        x = inHp.processSample (x);
        bwState = x + bantK * (bwState - x);                   // hafif bant sınırı (Plate: bandwidth 0.95)
        // tip geçişi açılışı: boş tanka GİRİŞ rampalanır — çıkış rampası yetmiyordu: sürekli sinyalin dalga cephesi
        // temizlenmiş gecikme hatlarından 30 ms rampa BİTTİKTEN sonra tap'lere ulaşıp tık yapıyordu (ölçüldü: Oda'da
        // +64 ms'de 0.097 sıçrama, kararlı durum 0.020)
        float tankIn = bwState;
        if (girisFade < 1.0f) tankIn *= girisFade;
        x = dif4.tick (dif3.tick (dif2.tick (dif1.tick (tankIn))));
        float wl = 0.0f, wr = 0.0f;
        tickTank (x, wl, wr);

        // erken yansımalar (Oda / Chamber): difüzyon ÖNCESİ bant sinyalinden, tanka paralel
        if (erSayi > 0)
        {
            erHat.push (tankIn);
            float eL = 0.0f, eR = 0.0f;
            for (int k = 0; k < erSayi; ++k)
            {
                eL += erGL[k] * erHat.tap (erTapL[k]);
                eR += erGR[k] * erHat.tap (erTapR[k]);
            }
            wl += eL; wr += eR;
        }
        // tip seviye eşlemesi ER DAHİL tüm ıslak yola (denetim, bulgu 10) — Plate'e dokunulmaz
        if (aktifTip != 0) { wl *= tankK; wr *= tankK; }

        // width (M/S)
        const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * widthAmt;
        wl = mid + side; wr = mid - side;

        float m = mixS.next() * duckGain;
        if (tipDurum == 1 || tipDurum == 3) m *= tipFade;   // sönüş (1) / iptal sonrası geri açılış (3)
        ch[0][i] += m * wl;
        if (c > 1) ch[1][i] += m * wr; else ch[0][i] += m * wr;
    }
    if (n > 0) bosDurum = false;                           // artık tank boş değil: sonraki tip SÖN-AÇ ile gelir
}

void PlateReverb::idleWet (const float* const* ch, int numCh, int n) noexcept
{
    juce::ignoreUnused (ch, numCh);
    // kapalıyken tip değişimi / yarım kalan geçiş: duyulmuyor → anında uygula (yeniden açılışta boşluk olmaz)
    const int h = std::clamp (pending.type, 0, TIP_SAYISI - 1);
    if (h != aktifTip || tipDurum != 0)
    {
        hedefTip = h;
        if (h != aktifTip) tipUygula (h);
        tipDurum = 0; tipFade = 1.0f; girisFade = 1.0f;
    }
    // kuyruk doğal söner (giriş 0) — denetim düzeltmesi: difüzörler de BOŞALTILIR, yoksa yeniden açılışta
    // içlerindeki bayat sinyal hayalet kuyruk olarak çalınıyordu
    for (int i = 0; i < n; ++i)
    {
        pre.push (0.0f);
        float x = inHp.processSample (0.0f);
        bwState = x + bantK * (bwState - x);
        if (erSayi > 0) erHat.push (bwState);                // erken yansıma hattı da boşalır
        x = dif4.tick (dif3.tick (dif2.tick (dif1.tick (bwState))));
        float wl = 0.0f, wr = 0.0f;
        tickTank (x, wl, wr);                              // artık difüzör kalıntısı tanka akıp doğal söner
    }
}

void PlateReverb::resetWet()
{
    pre.reset(); inHp.reset(); bwState = 0.0f;
    dif1.reset(); dif2.reset(); dif3.reset(); dif4.reset();
    modRingL.reset(); modRingR.reset();
    d1L.reset(); d3L.reset(); d1R.reset(); d3R.reset();
    ap2L.reset(); ap2R.reset();
    erHat.reset();
    dampL = dampR = 0.0f; fbL = fbR = 0.0f;
    duckEnv = 0.0f; lfo = 0.0;
    tipDurum = 0; tipFade = 1.0f; girisFade = 1.0f;
    bosDurum = true;                                  // İLK örnek işlenene kadar: tip anında uygulanır
}

} // namespace sweax
