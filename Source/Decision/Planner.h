#pragma once
// SWEAX NEXT — Decision/Planner.h
// Düzeltme planlayıcısı (mimari §5.2): AnalysisReport + ProblemDetection[] → MixPlan.
//  İLKELER (kod bunlardır, süs değil):
//   1. KANIT YOKSA İŞLEM YOK — her enabled modül/bant bir Reason'a bağlıdır; kanıt eşiği actionable(p≥0.5, c≥0.5).
//   2. Derinlik = şiddet × güven; düşük güven yalnız AZALTIR.
//   3. SABİT TARİF YASAK: frekans dedektör merkezinden, derinlik şiddetten, Q bant genişliğinden türetilir.
//   4. Guardrails: statik kesme ≤6 dB, dinamik ≤9 dB, boost ≤3 dB, Q ≤8 yalnız rezonans kanıtıyla (yoksa ≤4),
//      şarkıcı formant bandı (2.2–3.6 kHz) yalnız dinamik ve ≤3 dB, de-ess ≤8 dB (12 yalnız kanıt ≥0.8),
//      HPF ≤ 0.75·F0_p05, GR hedefi rol bağımlı 2–7 dB.
//   5. Eşikler dahili referansa göre: InputTrim sesli-gate'li loudness'ı −18 LUFS'a getirir; tüm eşikler buna göre.
//   6. DETERMİNİZM: parametreler kuantalı (quantHz/Db/Q/Ms); aynı rapor → aynı plan (test).
//  Kilit/otomasyon: kilitli modüller plana Locked gerekçesiyle DOKUNULMADAN geçer (Publisher atlar).
#include "Plan.h"
#include "Detectors.h"

namespace sweax::decision
{

struct PlannerInput
{
    const AnalysisReport* report { nullptr };
    const std::vector<ProblemDetection>* detections { nullptr };
    state::Role role { state::Role::Lead };
    uint64_t planId { 1 };
    int style { 1 };                                            // 0 Doğal, 1 Temiz, 2 Trap, 3 Agresif
    std::array<bool, state::modul::HEPSI.size()> locked {};     // HEPSI sırasında modül kilitleri
    int outChannels { 2 };                                      // eş-ses ölçümü: çıkış bus'ının GERÇEK kanal sayısı

    bool lockedFor (const char* id) const noexcept
    {
        for (size_t i = 0; i < locked.size(); ++i)
            if (std::string_view (state::modul::HEPSI[i]) == std::string_view (id)) return locked[i];
        return false;
    }
    void setLocked (const char* id, bool v) noexcept
    {
        for (size_t i = 0; i < locked.size(); ++i)
            if (std::string_view (state::modul::HEPSI[i]) == std::string_view (id)) { locked[i] = v; return; }
    }
};

class Planner
{
public:
    static constexpr float INTERNAL_REF_LUFS = -18.0f;

    static MixPlan plan (const PlannerInput& in);

    // rol bağımlı GR hedefi tavanı (dB) — mimari §5.2 guardrail
    static float roleGrCeiling (state::Role r) noexcept
    {
        // M3.4: Lead 5 → 8 dB — diskografi araştırması (trap metal 6-9 GR, radyofonik hook 8-12 standart);
        // eski tavan M1 muhafazakârlığıydı, agresif sanatçı stillerini kırpıyordu. Master 2 kalır.
        switch (r) { case state::Role::Lead: return 8.0f; case state::Role::Backing: return 8.0f; case state::Role::Adlib: return 7.0f; default: return 2.0f; }
    }
    // bant genişliği (oktav) → Q (yaklaşık: Q = f0/BW = 1/(2^(b/2) − 2^(−b/2)))
    static float qFromBandwidth (float bwOct) noexcept
    {
        const double b = std::clamp ((double) bwOct, 0.05, 4.0);
        return (float) (1.0 / (std::pow (2.0, b * 0.5) - std::pow (2.0, -b * 0.5)));
    }
    static bool inFormantZone (float hz) noexcept { return hz >= 2200.0f && hz <= 3600.0f; }
};

} // namespace sweax::decision
