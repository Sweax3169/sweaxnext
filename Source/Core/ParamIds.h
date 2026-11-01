#pragma once
// SWEAX NEXT — Core/ParamIds.h
// PARAMETRE TABLOSU — TEK KAYNAK. Buradan türeyenler:
//   • APVTS düzeni (ParamCache::createLayout)          • ParamCache atomik işaretçi dizisi
//   • UI kontrol açıklamaları (Metin.h static_assert)   • state migrasyon anahtarları
// Kimlik biçimi: "swx.<modül>.<param>", juce::ParameterID{id, sürüm}. Yayından sonra kimlik DEĞİŞMEZ.
//
// Birimler HAM'dır (Hz, dB, ms, oran) — normalize değil; SR ve aralık değişimine dayanıklı.
// makro: hangi makronun bu parametreyi ölçeklediği (ParamCache §2.2). nötr: makro ölçeği için
// "etkisiz" değer (kazançta 0 dB, range'de 0, oran'da 1).
#include <array>
#include <cstdint>
#include <string_view>

namespace sweax::params
{

enum class Tip : uint8_t { Float, Bool, Choice };
enum class Makro : uint8_t { Yok, Temizlik, Denge, Onde, Parlaklik, Alan, Genislik, Cikis };

struct Tanim
{
    std::string_view id;      // "swx.eq.b1.gain"
    std::string_view ad;      // host'ta görünen kısa ad (İngilizce, otomasyon lane'i)
    Tip tip;
    float min, max, def;      // Float/Choice aralığı; Bool için 0/1
    float skew;               // NormalisableRange skew (1 = doğrusal, <1 log-benzeri)
    std::string_view birim;   // "dB", "Hz", "ms", "" …
    Makro makro;
    float notr;               // makro ölçeği için etkisiz değer
    bool otomasyon;           // false → host otomasyonuna kapalı (layout gibi)
    std::string_view secenekler; // Choice: '|' ile ayrılmış
};

// ---- indeks sabitleri (ParamCache dizisine hızlı erişim) ----
enum Id : uint16_t
{
    Bypass, InputTrim, OutputTrim, QualityMode, Layout, AbMode, AbMatch, Delta,
    MacroTemizlik, MacroDenge, MacroOnde, MacroParlaklik, MacroAlan, MacroGenislik, MacroCikis,
    // EQ
    EqOn, EqHpfOn, EqHpfHz, EqHpfSlope,
    EqB1On, EqB1Type, EqB1Hz, EqB1Gain, EqB1Q, EqB1DynOn, EqB1DynThresh, EqB1DynRange, EqB1DynAttack, EqB1DynRelease,
    EqB2On, EqB2Type, EqB2Hz, EqB2Gain, EqB2Q, EqB2DynOn, EqB2DynThresh, EqB2DynRange, EqB2DynAttack, EqB2DynRelease,
    EqB3On, EqB3Type, EqB3Hz, EqB3Gain, EqB3Q, EqB3DynOn, EqB3DynThresh, EqB3DynRange, EqB3DynAttack, EqB3DynRelease,
    EqB4On, EqB4Type, EqB4Hz, EqB4Gain, EqB4Q, EqB4DynOn, EqB4DynThresh, EqB4DynRange, EqB4DynAttack, EqB4DynRelease,
    EqB5On, EqB5Type, EqB5Hz, EqB5Gain, EqB5Q, EqB5DynOn, EqB5DynThresh, EqB5DynRange, EqB5DynAttack, EqB5DynRelease,
    EqB6On, EqB6Type, EqB6Hz, EqB6Gain, EqB6Q, EqB6DynOn, EqB6DynThresh, EqB6DynRange, EqB6DynAttack, EqB6DynRelease,
    // Leveler
    LevOn, LevTarget, LevMaxUp, LevMaxDown, LevSpeed,
    // Comp A
    CaOn, CaThresh, CaRatio, CaKnee, CaAttack, CaRelease, CaLookahead, CaMakeup, CaMix, CaDetector, CaScHpf, CaCharacter,
    // Comp B
    CbOn, CbThresh, CbRatio, CbKnee, CbAttack, CbRelease, CbLookahead, CbMakeup, CbMix, CbDetector, CbScHpf, CbCharacter,
    // De-esser
    DsOn, DsMode, DsHz, DsThresh, DsRange, DsLookahead, DsAttack, DsRelease, DsLink,
    // Output
    OutCeiling, OutRelease,
    // Gate (M2: gürültü kapısı — zincir başı)
    GateOn, GateThresh, GateRange, GateAttack, GateRelease, GateHold, GateHyst,
    // Saturator (M2: karakter/doygunluk)
    SatOn, SatDrive, SatTone, SatMix,
    // Doubler (M2)
    DblOn, DblMix, DblSpreadMs, DblDetune, DblHpHz, DblLpHz, DblVoices,
    // Delay (M2)
    DlyOn, DlyMix, DlySync, DlyTimeMs, DlyFeedback, DlyHpHz, DlyLpHz, DlyDuck,
    // Reverb (M2)
    RevOn, RevMix, RevPre, RevDecay, RevDamp, RevHp, RevWidth, RevDuck,
    // Width (M2: M/S genişlik)
    WidOn, WidAmount,
    // Tune (M2: perde düzeltme)
    TuneOn, TuneStrength, TuneSpeed, TuneKey, TuneScale,
    // Air (M3: Fresh Air tarzı program-bağımlı hava — Dolby-A mirası iki üst bant)
    AirOn, AirMid, AirHigh,
    // Glow (M3.2: OTT tarzı 3 bant yukarı+aşağı — "anında iyi ses"; araştırma raporu tabanlı)
    GlowOn, GlowDepth, GlowTime,
    // Dirt (M3.4: paralel kir veri yolu — trap metal scream / fuzz / lo-fi)
    DirtOn, DirtMode, DirtDrive, DirtLevel, DirtTone, DirtBits, DirtRate,
    // Layer (M3.4: pitch-shift katman sesleri — oktav-altı / harmoni)
    LayerOn, LayerA, LayerB, LayerLevel, LayerWidth, LayerTone, LayerGrit,
    // Patlama koruması (M3.4: HPF öncesi F0-altı tetikli dinamik low-shelf — "harfler patlamasın")
    EqPlosOn, EqPlosHz, EqPlosRange,
    // De-esser SON AŞAMA (M3.4 T3: AIR sonrası s-sesi / gövde öne çıkma sınırı; M3.5: kendi açma/aralığı)
    DsPost, Ds2On, Ds2Range,
    // Zincir sırası (M3.5: preset başına özel sıralama — ekleme bölümü; Chain/ZincirSirasi.h)
    PosTune, PosEq, PosLev, PosCompA, PosDs, PosCompB, PosGlow, PosSat, PosDirt, PosAir, PosDs2, PosLayer,
    // YUMUŞAT (M3.5: soothe2 / Silk Vocal tarzı dinamik rezonans bastırma — spektral)
    SooOn, SooDepth, SooSharp, SooLo, SooHi, SooSpeed,
    // FORMANT (M3.5: Little AlterBoy / Pitcher 'gender' tarzı perdeden bağımsız formant kaydırma — spektral)
    FrmOn, FrmShift, FrmMix,
    // TON EQ (M3.5: preset ton EQ'su — düzeltici EQ'dan AYRI, reçete sırasında istenen yerde; 4 statik bant)
    TeqOn,
    TeqB1On, TeqB1Type, TeqB1Hz, TeqB1Gain, TeqB1Q,
    TeqB2On, TeqB2Type, TeqB2Hz, TeqB2Gain, TeqB2Q,
    TeqB3On, TeqB3Type, TeqB3Hz, TeqB3Gain, TeqB3Q,
    TeqB4On, TeqB4Type, TeqB4Hz, TeqB4Gain, TeqB4Q,
    // karakter seçenekleri (M3.5)
    RevType, DlyWidth, SatChar,
    // yeni ekleme modüllerinin sıra konumları (M3.5)
    PosSoo, PosFrm, PosTeq,
    SAYI
};

constexpr int EQ_BANT_SAYISI = 6;
constexpr int EQ_BANT_PARAM  = 10; // On Type Hz Gain Q DynOn DynThresh DynRange DynAttack DynRelease
constexpr Id  eqBant (int bant, int alan) noexcept { return (Id) (EqB1On + bant * EQ_BANT_PARAM + alan); }
constexpr int TEQ_BANT_SAYISI = 4;
constexpr int TEQ_BANT_PARAM  = 5; // On Type Hz Gain Q
constexpr Id  teqBant (int bant, int alan) noexcept { return (Id) (TeqB1On + bant * TEQ_BANT_PARAM + alan); }

namespace detay
{
    constexpr Tanim F (std::string_view id, std::string_view ad, float mn, float mx, float df, float skew,
                       std::string_view birim, Makro m = Makro::Yok, float notr = 0.0f, bool oto = true)
    { return { id, ad, Tip::Float, mn, mx, df, skew, birim, m, notr, oto, {} }; }
    constexpr Tanim B (std::string_view id, std::string_view ad, bool df, bool oto = true)
    { return { id, ad, Tip::Bool, 0.0f, 1.0f, df ? 1.0f : 0.0f, 1.0f, {}, Makro::Yok, 0.0f, oto, {} }; }
    constexpr Tanim C (std::string_view id, std::string_view ad, std::string_view secenekler, int adet, int df, bool oto = true)
    { return { id, ad, Tip::Choice, 0.0f, (float) (adet - 1), (float) df, 1.0f, {}, Makro::Yok, 0.0f, oto, secenekler }; }

    // Bir EQ bandının 10 tanımı (bant 1..6). Varsayılan merkezler yalnız UI başlangıç değeridir;
    // gerçek frekans ANALİZDEN gelir (sabit tarif yasağı — §5.2).
    #define SWX_EQ_BANT(N, DEFHZ) \
        B ("swx.eq.b" #N ".on",      "EQ B" #N " On", false), \
        C ("swx.eq.b" #N ".type",    "EQ B" #N " Type", "Bell|Low Shelf|High Shelf|Notch", 4, 0), \
        F ("swx.eq.b" #N ".hz",      "EQ B" #N " Freq", 20.0f, 20000.0f, DEFHZ, 0.25f, "Hz"), \
        F ("swx.eq.b" #N ".gain",    "EQ B" #N " Gain", -18.0f, 12.0f, 0.0f, 1.0f, "dB", Makro::Temizlik, 0.0f), \
        F ("swx.eq.b" #N ".q",       "EQ B" #N " Q", 0.1f, 16.0f, 1.0f, 0.35f, ""), \
        B ("swx.eq.b" #N ".dyn",     "EQ B" #N " Dynamic", false), \
        F ("swx.eq.b" #N ".dynthr",  "EQ B" #N " Dyn Thresh", -60.0f, 0.0f, -24.0f, 1.0f, "dB"), \
        F ("swx.eq.b" #N ".dynrng",  "EQ B" #N " Dyn Range", -18.0f, 0.0f, 0.0f, 1.0f, "dB", Makro::Temizlik, 0.0f), \
        F ("swx.eq.b" #N ".dynatk",  "EQ B" #N " Dyn Attack", 0.5f, 100.0f, 5.0f, 0.4f, "ms"), \
        F ("swx.eq.b" #N ".dynrel",  "EQ B" #N " Dyn Release", 10.0f, 1000.0f, 120.0f, 0.4f, "ms")

    #define SWX_COMP(P, AD) \
        B ("swx." P ".on",        AD " On", false), \
        F ("swx." P ".thresh",    AD " Threshold", -60.0f, 0.0f, -18.0f, 1.0f, "dB", Makro::Denge, 0.0f), \
        F ("swx." P ".ratio",     AD " Ratio", 1.0f, 20.0f, 2.0f, 0.4f, ":1", Makro::Denge, 1.0f), \
        F ("swx." P ".knee",      AD " Knee", 0.0f, 24.0f, 6.0f, 1.0f, "dB"), \
        F ("swx." P ".attack",    AD " Attack", 0.1f, 100.0f, 10.0f, 0.35f, "ms"), \
        F ("swx." P ".release",   AD " Release", 30.0f, 1000.0f, 120.0f, 0.4f, "ms"), \
        F ("swx." P ".lookahead", AD " Lookahead", 0.0f, 5.0f, 0.0f, 1.0f, "ms"), \
        F ("swx." P ".makeup",    AD " Makeup", 0.0f, 18.0f, 0.0f, 1.0f, "dB"), \
        F ("swx." P ".mix",       AD " Mix", 0.0f, 1.0f, 1.0f, 1.0f, "", Makro::Denge, 0.0f), \
        C ("swx." P ".detector",  AD " Detector", "Peak|RMS|Hybrid", 3, 2), \
        F ("swx." P ".schpf",     AD " SC HPF", 20.0f, 500.0f, 80.0f, 0.4f, "Hz"), \
        C ("swx." P ".character", AD " Character", "Clean|Smooth|Punch|Opto", 4, 0)

    #define SWX_TEQ_BANT(N, DEFHZ)         B ("swx.teq.b" #N ".on",     "Tone EQ B" #N " On", false),         C ("swx.teq.b" #N ".type",   "Tone EQ B" #N " Type", "Bell|Low Shelf|High Shelf", 3, 0),         F ("swx.teq.b" #N ".hz",     "Tone EQ B" #N " Freq", 20.0f, 20000.0f, DEFHZ, 0.25f, "Hz"),         F ("swx.teq.b" #N ".gain",   "Tone EQ B" #N " Gain", -12.0f, 12.0f, 0.0f, 1.0f, "dB"),         F ("swx.teq.b" #N ".q",      "Tone EQ B" #N " Q", 0.1f, 8.0f, 0.8f, 0.35f, "")
}

using namespace detay;

// SIRA = enum Id sırası. Test: TABLO[i].id ile enum adı eşleşmesi (ParamCacheTest).
inline constexpr std::array<Tanim, (size_t) SAYI> TABLO = {
    B ("swx.bypass",        "Bypass", false),
    F ("swx.input.trim",    "Input Trim", -24.0f, 24.0f, 0.0f, 1.0f, "dB"),
    F ("swx.output.trim",   "Output Trim", -24.0f, 24.0f, 0.0f, 1.0f, "dB", Makro::Cikis, 0.0f),
    C ("swx.quality",       "Quality", "Canli|Yuksek|Ultra", 3, 1),
    C ("swx.layout",        "Chain Layout", "Standard|DeEssBeforeComp|DualDeEss", 3, 0, false),
    C ("swx.ab.mode",       "A/B", "Processed|Original", 2, 0),
    B ("swx.ab.match",      "A/B Loudness Match", true),
    B ("swx.delta",         "Delta Listen", false),
    F ("swx.macro.temizlik",  "Macro Clean",     0.0f, 1.0f, 0.5f, 1.0f, ""),
    F ("swx.macro.denge",     "Macro Balance",   0.0f, 1.0f, 0.5f, 1.0f, ""),
    F ("swx.macro.onde",      "Macro Forward",   0.0f, 1.0f, 0.5f, 1.0f, ""),
    F ("swx.macro.parlaklik", "Macro Bright",    0.0f, 1.0f, 0.5f, 1.0f, ""),
    F ("swx.macro.alan",      "Macro Space",     0.0f, 1.0f, 0.5f, 1.0f, ""),
    F ("swx.macro.genislik",  "Macro Width",     0.0f, 1.0f, 0.5f, 1.0f, ""),
    F ("swx.macro.cikis",     "Macro Output",    0.0f, 1.0f, 0.5f, 1.0f, ""),
    // EQ
    B ("swx.eq.on",         "EQ On", false),
    B ("swx.eq.hpf.on",     "HPF On", false),
    F ("swx.eq.hpf.hz",     "HPF Freq", 20.0f, 400.0f, 60.0f, 0.4f, "Hz"),
    C ("swx.eq.hpf.slope",  "HPF Slope", "12 dB|24 dB", 2, 0),
    SWX_EQ_BANT (1, 120.0f), SWX_EQ_BANT (2, 300.0f), SWX_EQ_BANT (3, 700.0f),
    SWX_EQ_BANT (4, 1800.0f), SWX_EQ_BANT (5, 4000.0f), SWX_EQ_BANT (6, 9000.0f),
    // Leveler
    B ("swx.lev.on",        "Leveler On", false),
    F ("swx.lev.target",    "Leveler Target", -36.0f, -6.0f, -18.0f, 1.0f, "LUFS"),
    F ("swx.lev.maxup",     "Leveler Max Up", 0.0f, 12.0f, 6.0f, 1.0f, "dB", Makro::Denge, 0.0f),
    F ("swx.lev.maxdown",   "Leveler Max Down", 0.0f, 12.0f, 6.0f, 1.0f, "dB", Makro::Denge, 0.0f),
    F ("swx.lev.speed",     "Leveler Speed", 0.0f, 1.0f, 0.5f, 1.0f, ""),
    SWX_COMP ("compa", "Comp A"),
    SWX_COMP ("compb", "Comp B"),
    // De-esser
    B ("swx.ds.on",         "De-Ess On", false),
    C ("swx.ds.mode",       "De-Ess Mode", "Split|Wide", 2, 0),
    F ("swx.ds.hz",         "De-Ess Freq", 2000.0f, 12000.0f, 6500.0f, 0.3f, "Hz"),
    F ("swx.ds.thresh",     "De-Ess Threshold", -60.0f, 0.0f, -24.0f, 1.0f, "dB"),
    F ("swx.ds.range",      "De-Ess Range", 0.0f, 12.0f, 0.0f, 1.0f, "dB", Makro::Temizlik, 0.0f),
    F ("swx.ds.lookahead",  "De-Ess Lookahead", 0.0f, 3.0f, 1.0f, 1.0f, "ms"),
    F ("swx.ds.attack",     "De-Ess Attack", 0.1f, 20.0f, 1.0f, 0.4f, "ms"),
    F ("swx.ds.release",    "De-Ess Release", 10.0f, 300.0f, 60.0f, 0.4f, "ms"),
    B ("swx.ds.link",       "De-Ess Stereo Link", true),
    // Output
    F ("swx.out.ceiling",   "Limiter Ceiling", -6.0f, 0.0f, -0.3f, 1.0f, "dBTP"),
    F ("swx.out.release",   "Limiter Release", 50.0f, 1000.0f, 120.0f, 0.4f, "ms"),
    // Gate
    B ("swx.gate.on",       "Gate On", false),
    F ("swx.gate.thresh",   "Gate Threshold", -80.0f, -20.0f, -55.0f, 1.0f, "dB"),
    F ("swx.gate.range",    "Gate Range", 0.0f, 40.0f, 15.0f, 1.0f, "dB", Makro::Temizlik, 0.0f),
    F ("swx.gate.attack",   "Gate Attack", 0.1f, 20.0f, 1.5f, 0.4f, "ms"),
    F ("swx.gate.release",  "Gate Release", 20.0f, 500.0f, 120.0f, 0.4f, "ms"),
    F ("swx.gate.hold",     "Gate Hold", 0.0f, 300.0f, 80.0f, 1.0f, "ms"),
    F ("swx.gate.hyst",     "Gate Hysteresis", 0.0f, 12.0f, 4.0f, 1.0f, "dB"),
    // Saturator
    B ("swx.sat.on",        "Saturator On", false),
    F ("swx.sat.drive",     "Sat Drive", 0.0f, 12.0f, 3.0f, 1.0f, "dB", Makro::Onde, 0.0f),
    F ("swx.sat.tone",      "Sat Tone", -6.0f, 6.0f, 0.0f, 1.0f, "dB"),
    F ("swx.sat.mix",       "Sat Mix", 0.0f, 1.0f, 1.0f, 1.0f, "", Makro::Onde, 0.0f),
    // Doubler
    B ("swx.dbl.on",        "Doubler On", false),
    F ("swx.dbl.mix",       "Doubler Mix", 0.0f, 1.0f, 0.35f, 1.0f, "", Makro::Genislik, 0.0f),
    F ("swx.dbl.spread",    "Doubler Spread", 5.0f, 40.0f, 18.0f, 1.0f, "ms"),
    F ("swx.dbl.detune",    "Doubler Detune", 0.0f, 25.0f, 9.0f, 1.0f, "c"),
    F ("swx.dbl.hp",        "Doubler HPF", 60.0f, 600.0f, 180.0f, 0.4f, "Hz"),
    F ("swx.dbl.lp",        "Doubler LPF", 2000.0f, 16000.0f, 9000.0f, 0.4f, "Hz"),
    C ("swx.dbl.voices",    "Doubler Voices", "2|4", 2, 0),
    // Delay
    B ("swx.dly.on",        "Delay On", false),
    F ("swx.dly.mix",       "Delay Mix", 0.0f, 1.0f, 0.16f, 1.0f, "", Makro::Alan, 0.0f),
    C ("swx.dly.sync",      "Delay Sync", "Slap|1/8|1/8.|1/4|1/16", 5, 0),
    F ("swx.dly.time",      "Delay Time", 40.0f, 1200.0f, 110.0f, 0.5f, "ms"),
    F ("swx.dly.fb",        "Delay Feedback", 0.0f, 0.85f, 0.28f, 1.0f, ""),
    F ("swx.dly.hp",        "Delay HPF", 60.0f, 1000.0f, 250.0f, 0.4f, "Hz"),
    F ("swx.dly.lp",        "Delay LPF", 1000.0f, 16000.0f, 6500.0f, 0.4f, "Hz"),
    F ("swx.dly.duck",      "Delay Duck", 0.0f, 18.0f, 8.0f, 1.0f, "dB"),
    // Reverb
    B ("swx.rev.on",        "Reverb On", false),
    F ("swx.rev.mix",       "Reverb Mix", 0.0f, 1.0f, 0.14f, 1.0f, "", Makro::Alan, 0.0f),
    F ("swx.rev.pre",       "Reverb PreDelay", 0.0f, 120.0f, 45.0f, 1.0f, "ms"),
    F ("swx.rev.decay",     "Reverb Decay", 0.2f, 6.0f, 1.4f, 0.4f, "s"),
    F ("swx.rev.damp",      "Reverb Damping", 2000.0f, 16000.0f, 7000.0f, 0.4f, "Hz"),
    F ("swx.rev.hpf",       "Reverb HPF", 60.0f, 1000.0f, 220.0f, 0.4f, "Hz"),
    F ("swx.rev.width",     "Reverb Width", 0.0f, 1.0f, 1.0f, 1.0f, ""),
    F ("swx.rev.duck",      "Reverb Duck", 0.0f, 18.0f, 6.0f, 1.0f, "dB"),
    // Width
    B ("swx.wid.on",        "Width On", false),
    F ("swx.wid.amount",    "Width Amount", 0.0f, 2.0f, 1.0f, 1.0f, "", Makro::Genislik, 1.0f),
    // Tune
    B ("swx.tune.on",       "Tune On", false),
    F ("swx.tune.strength", "Tune Strength", 0.0f, 1.0f, 0.8f, 1.0f, ""),
    F ("swx.tune.speed",    "Tune Speed", 1.0f, 400.0f, 20.0f, 0.35f, "ms"),
    C ("swx.tune.key",      "Tune Key", "C|C#|D|D#|E|F|F#|G|G#|A|A#|B", 12, 0),
    C ("swx.tune.scale",    "Tune Scale", "Kromatik|Major|Minor", 3, 0),
    // Air
    B ("swx.air.on",        "Air On", false),
    F ("swx.air.mid",       "Air Mid", 0.0f, 1.0f, 0.25f, 1.0f, ""),
    F ("swx.air.high",      "Air High", 0.0f, 1.0f, 0.20f, 1.0f, ""),
    // Glow
    B ("swx.glow.on",       "Glow On", false),
    F ("swx.glow.depth",    "Glow Depth", 0.0f, 1.0f, 0.18f, 1.0f, ""),
    F ("swx.glow.time",     "Glow Time", 0.33f, 3.0f, 1.0f, 0.5f, "x"),
    // Dirt
    B ("swx.dirt.on",       "Dirt On", false),
    C ("swx.dirt.mode",     "Dirt Mode", "Scream|Fuzz|Lofi", 3, 0),
    F ("swx.dirt.drive",    "Dirt Drive", 0.0f, 36.0f, 18.0f, 1.0f, "dB"),
    F ("swx.dirt.level",    "Dirt Level", -36.0f, 0.0f, -12.0f, 1.0f, "dB"),
    F ("swx.dirt.tone",     "Dirt Tone", 2000.0f, 12000.0f, 5000.0f, 0.4f, "Hz"),
    F ("swx.dirt.bits",     "Dirt Bits", 4.0f, 16.0f, 10.0f, 1.0f, "bit"),
    F ("swx.dirt.rate",     "Dirt Rate", 1.0f, 8.0f, 2.0f, 1.0f, "x"),
    // Layer
    B ("swx.layer.on",      "Layer On", false),
    F ("swx.layer.a",       "Layer A", -12.0f, 12.0f, -12.0f, 1.0f, "st"),
    F ("swx.layer.b",       "Layer B", -12.0f, 12.0f, 0.0f, 1.0f, "st"),
    F ("swx.layer.level",   "Layer Level", -36.0f, 0.0f, -10.0f, 1.0f, "dB"),
    F ("swx.layer.width",   "Layer Width", 0.0f, 1.0f, 0.6f, 1.0f, ""),
    F ("swx.layer.tone",    "Layer Tone", 1500.0f, 12000.0f, 6000.0f, 0.4f, "Hz"),
    F ("swx.layer.grit",    "Layer Grit", 0.0f, 1.0f, 0.0f, 1.0f, ""),
    // Patlama koruması (EQ modülünün parçası)
    B ("swx.eq.plos.on",    "Plosive Guard On", false),
    F ("swx.eq.plos.hz",    "Plosive Guard Freq", 60.0f, 250.0f, 150.0f, 0.5f, "Hz"),
    F ("swx.eq.plos.range", "Plosive Guard Range", -18.0f, 0.0f, -6.0f, 1.0f, "dB", Makro::Temizlik, 0.0f),
    F ("swx.ds.post",       "De-Ess Post Threshold", -18.0f, 12.0f, -4.0f, 1.0f, "dB"),
    B ("swx.ds2.on",        "De-Ess Post On", false),
    F ("swx.ds2.range",     "De-Ess Post Range", 0.0f, 9.0f, 6.0f, 1.0f, "dB", Makro::Temizlik, 0.0f),
    // Zincir sırası (otomasyona kapalı)
    F ("swx.pos.tune",          "Order Tune", 0.0f, 15.0f, 0.0f, 1.0f, "", Makro::Yok, 0.0f, false),
    F ("swx.pos.eq",            "Order EQ", 0.0f, 15.0f, 1.0f, 1.0f, "", Makro::Yok, 0.0f, false),
    F ("swx.pos.lev",           "Order Leveler", 0.0f, 15.0f, 2.0f, 1.0f, "", Makro::Yok, 0.0f, false),
    F ("swx.pos.compa",         "Order Comp A", 0.0f, 15.0f, 3.0f, 1.0f, "", Makro::Yok, 0.0f, false),
    F ("swx.pos.ds",            "Order De-Ess", 0.0f, 15.0f, 4.0f, 1.0f, "", Makro::Yok, 0.0f, false),
    F ("swx.pos.compb",         "Order Comp B", 0.0f, 15.0f, 5.0f, 1.0f, "", Makro::Yok, 0.0f, false),
    F ("swx.pos.glow",          "Order Glow", 0.0f, 15.0f, 6.0f, 1.0f, "", Makro::Yok, 0.0f, false),
    F ("swx.pos.sat",           "Order Saturator", 0.0f, 15.0f, 7.0f, 1.0f, "", Makro::Yok, 0.0f, false),
    F ("swx.pos.dirt",          "Order Dirt", 0.0f, 15.0f, 8.0f, 1.0f, "", Makro::Yok, 0.0f, false),
    F ("swx.pos.air",           "Order Air", 0.0f, 15.0f, 9.0f, 1.0f, "", Makro::Yok, 0.0f, false),
    F ("swx.pos.ds2",           "Order De-Ess 2", 0.0f, 15.0f, 10.0f, 1.0f, "", Makro::Yok, 0.0f, false),
    F ("swx.pos.layer",         "Order Layer", 0.0f, 15.0f, 11.0f, 1.0f, "", Makro::Yok, 0.0f, false),
    // YUMUŞAT
    B ("swx.soo.on",        "Soothe On", false),
    F ("swx.soo.depth",     "Soothe Depth", 0.0f, 1.0f, 0.35f, 1.0f, "", Makro::Temizlik, 0.0f),
    F ("swx.soo.sharp",     "Soothe Sharpness", 0.0f, 1.0f, 0.5f, 1.0f, ""),
    F ("swx.soo.lo",        "Soothe Focus Low", 300.0f, 8000.0f, 2000.0f, 0.4f, "Hz"),
    F ("swx.soo.hi",        "Soothe Focus High", 2000.0f, 20000.0f, 12000.0f, 0.4f, "Hz"),
    F ("swx.soo.speed",     "Soothe Speed", 0.0f, 1.0f, 0.5f, 1.0f, ""),
    // FORMANT
    B ("swx.frm.on",        "Formant On", false),
    F ("swx.frm.shift",     "Formant Shift", -6.0f, 6.0f, 0.0f, 1.0f, "st"),
    F ("swx.frm.mix",       "Formant Mix", 0.0f, 1.0f, 1.0f, 1.0f, ""),
    // TON EQ
    B ("swx.teq.on",        "Tone EQ On", false),
    SWX_TEQ_BANT (1, 150.0f), SWX_TEQ_BANT (2, 700.0f), SWX_TEQ_BANT (3, 3000.0f), SWX_TEQ_BANT (4, 10000.0f),
    // karakter seçenekleri
    C ("swx.rev.type",      "Reverb Type", "Plate|Oda|Salon|Chamber", 4, 0),
    F ("swx.dly.width",     "Delay Width", 0.0f, 1.0f, 1.0f, 1.0f, ""),
    C ("swx.sat.char",      "Sat Character", "Tup|Bant|Kirik", 3, 0),
    // yeni sıra konumları (otomasyona kapalı)
    F ("swx.pos.soo",           "Order Soothe", 0.0f, 15.0f, 12.0f, 1.0f, "", Makro::Yok, 0.0f, false),
    F ("swx.pos.frm",           "Order Formant", 0.0f, 15.0f, 13.0f, 1.0f, "", Makro::Yok, 0.0f, false),
    F ("swx.pos.teq",           "Order Tone EQ", 0.0f, 15.0f, 14.0f, 1.0f, "", Makro::Yok, 0.0f, false),
};

#undef SWX_EQ_BANT
#undef SWX_COMP
#undef SWX_TEQ_BANT

static_assert (TABLO.size() == (size_t) SAYI, "TABLO ile enum Id uzunluğu farklı");
static_assert (eqBant (5, 9) == EqB6DynRelease, "EQ bant indeks hesabı kaydı");
static_assert (TABLO[(size_t) OutRelease].id == "swx.out.release", "limiter tanım hizası bozuk");
static_assert (TABLO[(size_t) GateHyst].id == "swx.gate.hyst", "gate hizası bozuk");
static_assert (TABLO[(size_t) SatMix].id == "swx.sat.mix", "sat hizası bozuk");
static_assert (TABLO[(size_t) DlyDuck].id == "swx.dly.duck", "delay hizası bozuk");
static_assert (TABLO[(size_t) RevDuck].id == "swx.rev.duck", "reverb hizası bozuk");
static_assert (TABLO[(size_t) TuneScale].id == "swx.tune.scale", "tune hizası bozuk");
static_assert (TABLO[(size_t) AirHigh].id == "swx.air.high", "air hizası bozuk");
static_assert (TABLO[(size_t) GlowTime].id == "swx.glow.time", "glow hizası bozuk");
static_assert (TABLO[(size_t) DirtRate].id == "swx.dirt.rate", "dirt hizası bozuk");
static_assert (TABLO[(size_t) LayerGrit].id == "swx.layer.grit", "layer hizası bozuk");
static_assert (TABLO[(size_t) EqPlosRange].id == "swx.eq.plos.range", "patlama koruması hizası bozuk");
static_assert (TABLO[(size_t) DsPost].id == "swx.ds.post", "de-ess son aşama hizası bozuk");
static_assert (TABLO[(size_t) Ds2Range].id == "swx.ds2.range", "son de-esser hizası bozuk");
static_assert (TABLO[(size_t) PosTune].id == "swx.pos.tune", "sıra başlangıç hizası bozuk");
static_assert (TABLO[(size_t) PosLayer].id == "swx.pos.layer", "zincir sırası hizası bozuk");
static_assert (TABLO[(size_t) SooSpeed].id == "swx.soo.speed", "yumuşat hizası bozuk");
static_assert (TABLO[(size_t) FrmMix].id == "swx.frm.mix", "formant hizası bozuk");
static_assert (teqBant (3, 4) == TeqB4Q, "ton EQ bant indeks hesabı kaydı");
static_assert (TABLO[(size_t) TeqB4Q].id == "swx.teq.b4.q", "ton EQ hizası bozuk");
static_assert (TABLO[(size_t) SatChar].id == "swx.sat.char", "karakter seçenekleri hizası bozuk");
static_assert (TABLO[(size_t) PosTeq].id == "swx.pos.teq", "son tanım (yeni sıra konumları) hizası bozuk");
static_assert (TABLO[(size_t) EqB3Hz].id == "swx.eq.b3.hz", "EQ bant hizası bozuk");
static_assert (TABLO[(size_t) CbCharacter].id == "swx.compb.character", "Comp B hizası bozuk");

constexpr int PARAM_SURUM = 1; // juce::ParameterID sürümü (yalnız kimlik anlamı değişirse artar)

} // namespace sweax::params
