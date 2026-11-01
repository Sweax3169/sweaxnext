#pragma once
// SWEAX NEXT — Decision/Feedback.h
// Geri besleme döngüsü (mimari §5.3): plan → sandbox render → YENİDEN ANALİZ (aynı ölçüm hattı) → skor →
// derinlik ayarı → ≤3 tur; YAYINLANAN = EN İYİ SKORLU TUR (plan_0 dahil). Süre bütçesi aşılırsa tur düşer.
//  Skor (küçük iyi): kalan tonal sorunlar + kalan sibilans + doğallık cezaları (crest kaybı > 3 dB, render'da
//  aşırı-sıkışmış tespiti) + render'da YENİ donukluk (aşırı kesim işareti) + referans-LUFS tutarlılık payı.
//  En az iki ölçüt dedektörlerden bağımsızdır (crest farkı, gate'li LUFS).
//  Ayar kuralları (yalnız DERİNLİK; frekans/Q kanıttan geldi, DEĞİŞMEZ): kalan sorun → ilgili bandın derinliği
//  ölçülen kalıntı kadar artar (guardrail tavanına klempli, kuantalı); render'da donukluk belirdiyse de-ess
//  hafifletilir. Değişecek bir şey kalmazsa tur erken biter.
#include "Plan.h"
#include "Planner.h"

namespace sweax::decision
{

struct FeedbackResult
{
    MixPlan finalPlan;
    int rounds { 0 };                  // koşulan render turu sayısı (≥1)
    int chosenRound { 0 };             // 0 = plan_0
    float scores[4] { 0, 0, 0, 0 };
    float processedLufs { -100.0f };   // seçilen planın render'ının gate'li integrated LUFS'u
    float fullChainLufs { -100.0f };   // M3.4: eş-ses sonrası TAM zincir (stil katmanları dahil) tahmini LUFS
};

class Feedback
{
public:
    static FeedbackResult optimize (const PlannerInput& in, const float* store, int64_t n, double sr,
                                    double maxSeconds = 1.5, int maxRounds = 3);

    // Yalnız test çalıştırıcısı açar: tur sayısı duvar saatine değil maxRounds'a bağlı kalsın (yük altındaki veya
    // yavaş CI makinesinde testler aynı planı görsün). Eklentide hep kapalı.
    static inline bool zamanSiniriYok = false;
};

} // namespace sweax::decision
