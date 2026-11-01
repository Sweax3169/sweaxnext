#pragma once
// SWEAX NEXT — Decision/Plan.h
// Karar katmanı veri modelleri (mimari §5.1). Ölçüm (Analysis) → teşhis (Detectors) → karar (Planner) → yayın.
//  • ProblemDetection: olasılık ≠ şiddet ≠ güven. Güven = kanıt yeterliliği (rapor güveninden), tespit değil.
//  • Reason KODLUDUR (Türkçe metin state'e yazılmaz; UI Metin.h'den render eder). Her enable bir Reason'a bağlıdır.
//  • Plan parametreleri HAM birimde (Hz/dB/ms) ve KUANTALI (frekans 1/24 okt, kazanç 0.1 dB, Q 0.05, ms 1) → SSE/AVX
//    kayan-nokta farkları planı çevirmez; determinizm testi bunu doğrular.
#include "../Core/ParamIds.h"
#include "../Chain/ZincirSirasi.h"
#include "../State/Session.h"
#include <array>
#include <vector>
#include <cstdint>
#include <cmath>

namespace sweax::decision
{

enum class Problem : int
{
    Clipping, LowLevel, DcOffset, NoiseFloor, Hum, Hiss, LfNoise,
    Boom, Proximity, Mud, Box, Nasal, Harsh, Dull, Bright, Thin, Resonance,
    MacroDynamics, MicroDynamics, OverCompressed, Sibilance, Plosive,
    COUNT
};

struct ProblemDetection
{
    Problem type { Problem::COUNT };
    float probability { 0.0f };     // 0..1: bu sorun var mı
    float severity { 0.0f };        // 0..1: ne kadar kötü (dB/oran sapmasından)
    float confidence { 0.0f };      // 0..1: kanıt yeterliliği (rapor güveni; olay sayısı artırmaz)
    float centerHz { 0.0f };        // ilgili merkez (tonal/rezonans/sibilans)
    float bandwidthOct { 0.0f };    // ilgili bant genişliği (oktav)
    float valueDb { 0.0f };         // ölçülen sapma / seviye (raporlama)
    int   count { 0 };              // olay sayısı (sibilant/plosive) veya bant sayısı
    bool  actionable (float pMin = 0.5f, float cMin = 0.5f) const noexcept { return probability >= pMin && confidence >= cMin; }
};

enum class ReasonRule : int
{
    NoEvidence,            // dedektör düşük olasılık → modül/işlem GEREKMEDİ (vals: p, s, c)
    LowConfidence,         // kanıt yetersiz → yapılmadı / azaltıldı (vals: c)
    InputTrimReference,    // sesli-gate'li loudness dahili referansa (vals: ölçülen LUFS, referans, kazanç)
    HpfLfNoise,            // LF gürültü/boom kanıtı (vals: fc, F0_p05, LF seviye)
    HpfPlosive,            // plosive olayları (vals: fc, adet/dk)
    HpfProximity,          // proximity eğimi (vals: fc, eğim dB/okt)
    CutMud, CutBox, CutBoom, CutNasal,     // tonal fazlalık kesimi (vals: fc, dB, Q, sapma)
    CutResonance,          // dar rezonans (vals: fc, dB, Q, kalıcılık)
    DynHarsh,              // 2–5 kHz seviye-bağımlı sertlik → dinamik bant (vals: fc, range, eşik)
    DynPlosiveLf,          // plosive için dinamik LF bandı (vals: fc, range)
    ShelfDull, ShelfBright,// hafif eğim düzeltmesi (vals: fc, dB)
    ShelfThin,             // LF eksikliği (vals: fc, dB)
    DeEss,                 // sibilans (vals: fc, range, eşik, olay/dk)
    LevelerMacro,          // cümleler arası seviye farkı (vals: IQR, p95−p05, hedef)
    CompMicro,             // cümle içi dinamik (vals: oran, eşik, GR hedefi)
    CompSkipOverCompressed,// zaten sıkışmış → kompresyon yok (vals: crest)
    LimiterSafety,         // güvenlik limiter'ı (vals: tavan)
    UserRequest,           // YİNE DE AÇ (nötr değerler, kilitli)
    Locked,                // kilitli: plan dokunmadı
    Automated,             // host otomasyonu var: plan dokunmadı
    // ---- M2 (sahip onaylı revizyon): rol/stil tabanlı müzikal taban + analizle inceltme ----
    GateNoise,             // ölçülen taban gürültüsü → kapı (vals: eşik, taban, range)
    GateBaseline,          // temiz kayıtta hafif kapı (vals: eşik, range)
    HpfBaseline,           // rap vokal tabanı: F0'a bağlı HPF (vals: fc, F0_p05)
    CompBaseline,          // rap vokal tabanı: yoğunluk kompresyonu (vals: oran, GR hedefi, crest)
    CompBGlue,             // seri ikinci kademe: yavaş tutkal (vals: oran, GR)
    LevBaseline,           // taban leveler (vals: aralık)
    StylePresence,         // stil hedefi: 2.5–5k presence (vals: fc, dB, ölçülen sapma)
    StyleAir,              // stil hedefi: 8–14k hava (vals: fc, dB, ölçülen sapma)
    SatStyle,              // stil karakteri: doygunluk (vals: drive, stil)
    FxStyle,               // stil efekti: doubler/delay/reverb/width (vals: miktar)
    TuneStyle,             // stil perde düzeltme (vals: güç, hız)
    StyleOff,              // stil gereği kapalı (Doğal vb.)
    OutputLevel,           // çıkış ticari vokal seviyesine (vals: trim dB)
    StyleBody,             // stil hedefi: gövde 150–350 Hz (cılız ses telafisi; vals: fc, dB, ölçülen sapma)
    MasterLoudness,        // MASTER: çıkış hedef yayın seviyesine (vals: hedef LUFS, trim dB)
    StyleBodyCut,          // M3.4 stil: gövde inceltme (vals: fc, dB)
    StyleDarkTop,          // M3.4 stil: koyu üst raf (vals: fc, dB)
    StyleLowCut,           // M3.4 stil: alçak raf inceltme (vals: fc, dB)
    DeEssStyle,            // M3.4 stil tizi kabartıyor → önleyici hafif de-esser (vals: fc, range)
    DirtStyle,             // M3.4 stil kiri (vals: mod, sürüş dB, seviye dB)
    LayerStyle,            // M3.4 stil katmanı (vals: A st, B st, seviye dB)
    StyleBand,             // M3.5 reçete ek bandı (vals: fc, dB, ölçülen sapma)
    SootheStyle,           // M3.5 reçete YUMUŞAT (vals: derinlik, odak alt Hz, odak üst Hz)
    FormantStyle,          // M3.5 reçete FORMANT (vals: kaydırma st, karışım)
    COUNT
};

struct Reason
{
    ReasonRule rule { ReasonRule::NoEvidence };
    Problem det { Problem::COUNT };
    std::array<float, 4> vals { 0.0f, 0.0f, 0.0f, 0.0f };
    params::Id target { params::SAYI };   // ilgili parametre (yoksa SAYI)
};

struct ParamValue { params::Id id; float raw; };

struct ModulePlan
{
    const char* moduleId { "" };          // state::modul::EQ / LEVELER / COMP_A / COMP_B / DEESS / OUTPUT
    bool enabled { false };
    std::vector<ParamValue> params;       // ham birim, kuantalı
    std::vector<Reason> reasons;          // enabled → en az bir gerekçe; kapalı → NoEvidence/LowConfidence/…
};

enum class SummaryLine : int
{
    None, TrimSet, HpfApplied, MudCut, BoxCut, ResonanceCut, HarshTamed, DeEssApplied, Leveled, Compressed,
    NothingNeeded, LowConfidenceOverall, NoiseWarning, ClippingWarning, COUNT
};

struct MixPlan
{
    state::Role role { state::Role::Lead };
    int layout { 0 };                                     // ChainLayout (M3.5'ten beri kullanılmaz)
    Sira sira { varsayilanSira() };                       // ekleme modülleri işleme sırası (M3.5, preset başına)
    std::vector<ModulePlan> modules;
    std::array<SummaryLine, 4> summary { SummaryLine::None, SummaryLine::None, SummaryLine::None, SummaryLine::None };
    float abGainDb { 0.0f };                              // A/B eşit ses (statik)
    float inputTrimDb { 0.0f };
    uint64_t planId { 0 };
    std::vector<ProblemDetection> detections;
    float overallConfidence { 0.0f };
    const ModulePlan* module (const char* id) const noexcept
    {
        for (const auto& m : modules) if (std::string_view (m.moduleId) == id) return &m;
        return nullptr;
    }
};

// Kuantalama (determinizm): frekans 1/24 oktav ızgara (20 Hz tabanlı), kazanç 0.1 dB, Q 0.05, ms 1
inline float quantHz (float hz) noexcept { const double k = std::round (24.0 * std::log2 (std::max (20.0, (double) hz) / 20.0)); return (float) (20.0 * std::pow (2.0, k / 24.0)); }
// tavanlı değerler için AŞAĞI kuantala (guardrail'i asla aşma — ör. HPF ≤ 0.75·F0_p05)
inline float quantHzDown (float hz) noexcept { const double k = std::floor (24.0 * std::log2 (std::max (20.0, (double) hz) / 20.0) + 1e-9); return (float) (20.0 * std::pow (2.0, k / 24.0)); }
inline float quantDb (float db) noexcept { return std::round (db * 10.0f) / 10.0f; }
inline float quantQ (float q) noexcept { return std::round (q * 20.0f) / 20.0f; }
inline float quantMs (float ms) noexcept { return std::round (ms); }

} // namespace sweax::decision
