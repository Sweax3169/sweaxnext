#pragma once
// SWEAX NEXT — Decision/Detectors.h
// Teşhis katmanı (mimari §3.4): AnalysisReport + kare kayıtları → ProblemDetection listesi.
// Kural: olasılık ≠ şiddet ≠ güven. Dedektörler karar VERMEZ; planlayıcı kanıt olmadan işlem açmaz.
//  • Tonal: LTAS (tonal kareler, 1/12 okt, güç ortalaması) 1/3 oktav yumuşatılır; referans = PARAMETRİK konuşma/şarkı
//    LTAS önceli (ANSI S3.5 / Byrne 1994 LTASS şekline dayalı: ~150–500 Hz düz tepe, üstte ≈ −6 dB/okt, 150 Hz altı
//    −3 dB/okt) yalnız SEVİYE (ofset) şarkıcıya uydurulur — eğim uydurulmaz ki dull/bright ölçülebilsin. Bu bir EQ
//    reçetesi değil, sapma ölçümü için referans dağılımdır; işlem derinliği ölçülen sapmadan gelir.
//    Mud 200–500, box 300–800, boom 60–150, nasal 800–1200 tepe + 1.5–2.5k çukur, harsh 2–5 kHz (yalnız en yüksek
//    %20 loudness'lı sesli karelerde), dull/bright > 4 kHz, thin < 300 Hz. Proximity: 200 Hz altı +6 dB/okt eğim.
//  • Rezonans: 1/12 okt LTAS'ın 1/3 okt zarfını ≥ +4 dB aşan dar tepe; kalıcılık = bandın zarfı ≥3 dB aştığı tonal kare
//    oranı ≥ 0.6; şarkıcı formantı bölgesi (2.2–3.6 kHz, Q<2) işaretlenir (planlayıcı yalnız dinamik bant).
//  • Dinamik: cümle IQR / p95−p05 → makro; cümle içi std + crest → mikro; crest düşük + std düşük → aşırı sıkışmış.
//  • Sibilans/plosive: olay sıklığı (dk başına) + tepe seviyesi (sesli medyanına göre).
//  • Gürültü: taban, SNR; tip: hum (50/60 Hz harmonik tepeleri), hiss (HF ağırlıklı), LF (HVAC) — oda karelerinin LTAS'ı.
//  • Sinyal kalitesi: clipping (|x| ≥ 0.999 örnek sayısı), DC, düşük seviye, headroom.
#include "Plan.h"
#include "../Analysis/Report.h"

namespace sweax::decision
{

struct DetectorContext
{
    state::Role role { state::Role::Lead };
    float clippedSamples { 0.0f };       // depodan (rapor dışı sayaç)
    float dcOffset { 0.0f };
};

class Detectors
{
public:
    static std::vector<ProblemDetection> detect (const AnalysisReport& r, const std::vector<SegmentAnalysis>& segs, const DetectorContext& ctx);

    // yardımcılar (test edilebilir)
    static float priorLtasDb (double hz) noexcept;                                       // parametrik referans (ofsetsiz)
    // [lo,hi) bölgesinin önceliye göre sapması (dB; medyan ofsetli) — stil hedefleme (presence/air) Planner'da kullanır
    static float regionDeviationDb (const AnalysisReport& r, double lo, double hi) noexcept;
    static void smoothThirdOctave (const float* in, float* out, int n) noexcept;          // 4 bant güç ortalaması
    static double bandHz (int band) noexcept { return 20.0 * std::pow (2.0, ((double) band + 0.5) / 12.0); }
    static int bandOf (double hz) noexcept { return (int) std::floor (12.0 * std::log2 (std::max (hz, 20.0) / 20.0)); }
    static const ProblemDetection* find (const std::vector<ProblemDetection>& v, Problem p) noexcept
    {
        for (const auto& d : v) if (d.type == p) return &d;
        return nullptr;
    }
};

} // namespace sweax::decision
