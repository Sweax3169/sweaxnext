// SWEAX NEXT — Decision/Feedback.cpp
#include "Feedback.h"
#include "Sandbox.h"
#include "../Analysis/Offline.h"
#include "../Analysis/Loudness.h"
#include <array>
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>

namespace sweax::decision
{

namespace
{
    float* findParam (ModulePlan& m, params::Id id)
    {
        for (auto& p : m.params) if (p.id == id) return &p.raw;
        return nullptr;
    }
    ModulePlan* findModule (MixPlan& plan, const char* id)
    {
        for (auto& m : plan.modules) if (std::string_view (m.moduleId) == id) return &m;
        return nullptr;
    }
    const ProblemDetection* find (const std::vector<ProblemDetection>& v, Problem p) { return Detectors::find (v, p); }

    // ---- M3.4 denetim T3: MUTLAK s-sesi öne çıkma ölçüsü. Dedektörün göreli ölçüsü (olay HF oranı − sesli HF oranı)
    // global parlaklık artışında düşmüş görünüyordu; oysa s tepeleri vokal gövdesine göre girişten 4-6 dB öne çıkıyordu.
    // Ölçü: girişte bulunan s olaylarında 5-10 kHz 5 ms tepe − ±0.6 s K-AĞIRLIKLI gövdenin 80. yüzdeliği (dB).
    // (300 Hz-3 kHz gövde çamur kesimiyle düşüp s'yi yapay 'öne çıkmış' gösteriyordu — ölçüldü; K-ağırlık geniş bant.)
    // Olay seçimi (yalnız girişte): 5-10 kHz, 300 Hz-3 kHz'yi ≥ 3 dB aşan ≥ 20 ms koşular.
    struct SibKareler { std::vector<float> sDb, bDb, oDb; };   // s bandı, K-gövde, orta bant (olay seçimi)
    SibKareler sibKareleri (const float* x, int64_t n, double sr)
    {
        SibKareler k;
        Biquad hp5, lp10, hp300, lp3k, kS, kH;
        hp5.setCoeffs (design::highpass (sr, 5000.0, 0.7071));
        lp10.setCoeffs (design::lowpass (sr, std::min (10000.0, 0.45 * sr), 0.7071));
        hp300.setCoeffs (design::highpass (sr, 300.0, 0.7071));
        lp3k.setCoeffs (design::lowpass (sr, 3000.0, 0.7071));
        { KWeighting kw; kw.prepare (sr, 1); kS.setCoeffs (kw.shelfCoeffs()); kH.setCoeffs (kw.hpfCoeffs()); }
        const int kN = std::max (1, (int) std::lround (0.005 * sr));
        const int64_t kare = n / kN;
        k.sDb.resize ((size_t) std::max<int64_t> (0, kare)); k.bDb.resize ((size_t) std::max<int64_t> (0, kare)); k.oDb.resize ((size_t) std::max<int64_t> (0, kare));
        for (int64_t f = 0; f < kare; ++f)
        {
            double es = 0.0, eb = 0.0, eo = 0.0;
            for (int i = 0; i < kN; ++i)
            {
                float v = x[f * kN + i];
                if (! (v > -1.0e6f && v < 1.0e6f)) v = 0.0f;
                const float sb = lp10.processSample (hp5.processSample (v));
                const float ob = lp3k.processSample (hp300.processSample (v));
                const float bb = kH.processSample (kS.processSample (v));
                es += (double) sb * sb; eb += (double) bb * bb; eo += (double) ob * ob;
            }
            k.sDb[(size_t) f] = (float) (10.0 * std::log10 (es / kN + 1.0e-20));
            k.bDb[(size_t) f] = (float) (10.0 * std::log10 (eb / kN + 1.0e-20));
            k.oDb[(size_t) f] = (float) (10.0 * std::log10 (eo / kN + 1.0e-20));
        }
        return k;
    }
    std::vector<std::pair<int, int>> sibOlaylari (const SibKareler& k, float esikDb)
    {
        std::vector<std::pair<int, int>> olay;
        int bas = -1, bosluk = 0;
        const int N = (int) k.sDb.size();
        for (int f = 0; f <= N; ++f)
        {
            const bool s = f < N && k.sDb[(size_t) f] > esikDb && k.sDb[(size_t) f] - k.oDb[(size_t) f] >= 3.0f;
            if (s) { if (bas < 0) bas = f; bosluk = 0; continue; }
            if (bas >= 0 && f < N && ++bosluk <= 1) continue;                    // 1 karelik delik birleştirilir
            if (bas >= 0)
            {
                const int son = f - 1 - (f < N ? bosluk - 1 : 0);
                if (son - bas + 1 >= 4 && olay.size() < 4000) olay.push_back ({ bas, son });
            }
            bas = -1; bosluk = 0;
        }
        return olay;
    }
    float sibOneCikma (const SibKareler& k, const std::vector<std::pair<int, int>>& olay)
    {
        if (olay.empty() || k.sDb.empty()) return 0.0f;
        std::vector<float> pen;
        double acc = 0.0; int c = 0;
        const int N = (int) k.sDb.size();
        for (const auto& [a, b] : olay)
        {
            if (b >= N) continue;
            float tepe = -200.0f;
            for (int f = a; f <= b; ++f) tepe = std::max (tepe, k.sDb[(size_t) f]);
            const int wa = std::max (0, a - 120), wb = std::min (N - 1, b + 120);
            pen.assign (k.bDb.begin() + wa, k.bDb.begin() + wb + 1);
            const size_t idx = (size_t) (0.8 * (double) (pen.size() - 1));
            std::nth_element (pen.begin(), pen.begin() + (std::ptrdiff_t) idx, pen.end());
            acc += (double) (tepe - pen[idx]); ++c;
        }
        return c > 0 ? (float) (acc / c) : 0.0f;
    }

    // son aşama de-esser (göreli mod) biriminde GİRİŞ s öne çıkması: olay başına max(s bandı 2 ms − gövde tepe izleyici), medyan
    float sibGoreliGiris (const float* x, int64_t n, double sr, float hz, const std::vector<std::pair<int, int>>& olay)
    {
        if (olay.empty()) return 0.0f;
        Biquad h1, h2, gh, gl;
        const double f = std::clamp ((double) hz, 2000.0, std::min (12000.0, sr * 0.45));
        h1.setCoeffs (design::highpass (sr, f, 0.70710678)); h2.setCoeffs (design::highpass (sr, f, 0.70710678));
        gh.setCoeffs (design::highpass (sr, 300.0, 0.70710678)); gl.setCoeffs (design::lowpass (sr, std::min (3000.0, sr * 0.45), 0.70710678));
        const float cB = (float) std::exp (-1.0 / (0.002 * sr)), cG = (float) std::exp (-1.0 / (0.010 * sr));
        const float cA = (float) std::exp (-1.0 / (0.020 * sr)), cR = (float) std::exp (-1.0 / (1.500 * sr));
        float tepeG = -70.0f;
        const int kN = std::max (1, (int) std::lround (0.005 * sr));
        const int64_t kare = n / kN;
        std::vector<float> kareMax ((size_t) std::max<int64_t> (0, kare), -200.0f);
        float eB = 0.0f, eG = 0.0f;
        for (int64_t i = 0; i < kare * kN; ++i)
        {
            float v = x[i]; if (! (v > -1.0e6f && v < 1.0e6f)) v = 0.0f;
            const float b = h2.processSample (h1.processSample (v));
            const float g = gl.processSample (gh.processSample (v));
            eB = b * b + cB * (eB - b * b); eG = g * g + cG * (eG - g * g);
            const float gDb = std::max (-70.0f, 10.0f * std::log10 (eG + 1.0e-12f));
            tepeG = gDb + (gDb > tepeG ? cA : cR) * (tepeG - gDb);
            const float rel = 10.0f * std::log10 (eB + 1.0e-12f) - tepeG;
            auto& km = kareMax[(size_t) (i / kN)];
            km = std::max (km, rel);
        }
        std::vector<float> tepe;
        for (const auto& [a, b] : olay)
        {
            float m = -200.0f;
            for (int k = a; k <= b && k < (int) kareMax.size(); ++k) m = std::max (m, kareMax[(size_t) k]);
            if (m > -199.0f) tepe.push_back (m);
        }
        if (tepe.empty()) return 0.0f;
        std::nth_element (tepe.begin(), tepe.begin() + (std::ptrdiff_t) (tepe.size() / 2), tepe.end());
        return tepe[tepe.size() / 2];
    }

    // skor: küçük iyi
    float puanla (const AnalysisReport& out, const AnalysisReport& in)
    {
        float tonal = 0.0f, sib = 0.0f, pen = 0.0f;
        for (const auto& d : out.detections)
        {
            if (! d.actionable()) continue;
            switch (d.type)
            {
                case Problem::Boom: case Problem::Mud: case Problem::Box: case Problem::Nasal:
                case Problem::Harsh: case Problem::Bright: case Problem::Thin: case Problem::Resonance:
                    tonal += d.probability * d.severity; break;
                case Problem::Sibilance: sib += d.probability * d.severity; break;
                case Problem::OverCompressed: pen += 0.5f * d.probability * d.severity; break;
                case Problem::Dull:
                {
                    const auto* dIn = find (in.detections, Problem::Dull);
                    const float once = dIn != nullptr ? dIn->probability * dIn->severity : 0.0f;
                    pen += 0.4f * std::max (0.0f, d.probability * d.severity - once);   // YENİ donukluk = aşırı kesim
                    break;
                }
                default: break;
            }
        }
        // dedektörlerden bağımsız ölçütler (dogallik, §5.3): crest kaybı > 3 dB; referans LUFS sapması;
        // GEÇİCİ KORUNUMU: onset keskinliği %30'dan fazla düşerse ceza; MODÜLASYON: 2-8 Hz vekili %35'ten fazla
        // değişirse (aşırı düzleşme = cansızlaşma) ceza
        pen += 0.3f * std::max (0.0f, (in.crestMedianDb - out.crestMedianDb) - 3.0f);
        pen += 0.05f * std::max (0.0f, std::fabs (out.integratedLufs - Planner::INTERNAL_REF_LUFS) - 8.0f);
        if (in.onsetSharpness > 0.05f)
            pen += 0.5f * std::max (0.0f, (in.onsetSharpness - out.onsetSharpness) / in.onsetSharpness - 0.30f);
        if (in.momentaryModDb > 0.2f)
            pen += 0.4f * std::max (0.0f, (in.momentaryModDb - out.momentaryModDb) / in.momentaryModDb - 0.35f);
        return tonal + sib + pen;
    }

    // yalnız derinlik ayarı; true = plan değişti
    bool ayarla (MixPlan& plan, const AnalysisReport& renderRep, const PlannerInput& in0, float sibFazla, float dsPostHedef)
    {
        bool changed = false;
        auto* eq = findModule (plan, state::modul::EQ);
        auto* ds = findModule (plan, state::modul::DEESS);
        // denetim M3.4 (PT-6): stil gerekçeli bantlar (presence/hava/gövde/koyu üst/alçak raf) kimliktir —
        // derinleştirme adayı DEĞİL (formant bölgesindeki Killoki 2.5k presence'ı statik KESİME çevriliyordu)
        std::array<bool, params::EQ_BANT_SAYISI> stilBandi {};
        if (eq != nullptr)
            for (const auto& rs : eq->reasons)
            {
                const bool stil = rs.rule == ReasonRule::StylePresence || rs.rule == ReasonRule::StyleAir || rs.rule == ReasonRule::StyleBody
                               || rs.rule == ReasonRule::StyleBodyCut || rs.rule == ReasonRule::StyleDarkTop || rs.rule == ReasonRule::StyleLowCut
                               || rs.rule == ReasonRule::StyleBand;
                if (! stil) continue;
                for (int b = 0; b < params::EQ_BANT_SAYISI; ++b)
                    if (rs.target == params::eqBant (b, 3) || rs.target == params::eqBant (b, 7)) stilBandi[(size_t) b] = true;
            }
        auto bandNear = [&] (float hz, bool dynWanted, int& outBi) -> bool
        {
            if (eq == nullptr || ! eq->enabled) return false;
            for (int b = 0; b < params::EQ_BANT_SAYISI; ++b)
            {
                if (stilBandi[(size_t) b]) continue;
                auto* on = findParam (*eq, params::eqBant (b, 0));
                auto* bhz = findParam (*eq, params::eqBant (b, 2));
                auto* dyn = findParam (*eq, params::eqBant (b, 5));
                if (on == nullptr || bhz == nullptr || *on < 0.5f) continue;
                const bool isDyn = dyn != nullptr && *dyn > 0.5f;
                if (isDyn != dynWanted) continue;
                if (std::fabs (std::log2 (std::max (1.0f, *bhz) / std::max (1.0f, hz))) < 0.5f) { outBi = b; return true; }
            }
            return false;
        };
        // kalan tonal → statik kesimi derinleştir (guardrail −6)
        for (Problem p : { Problem::Boom, Problem::Mud, Problem::Box, Problem::Nasal })
        {
            const auto* d = find (renderRep.detections, p);
            if (d == nullptr || ! d->actionable()) continue;
            int bi;
            if (! bandNear (d->centerHz, false, bi)) continue;
            auto* g = findParam (*eq, params::eqBant (bi, 3));
            if (g == nullptr) continue;
            const float yeni = quantDb (std::clamp (*g - std::min (2.0f, 0.6f * d->valueDb), -6.0f, 0.0f));
            if (yeni < *g - 0.05f) { *g = yeni; changed = true; }
        }
        // kalan rezonans → eşleşen bandın derinliği (statik veya dinamik)
        for (const auto& d : renderRep.detections)
        {
            if (d.type != Problem::Resonance || ! d.actionable()) continue;
            int bi;
            if (bandNear (d.centerHz, false, bi))
            {
                auto* g = findParam (*eq, params::eqBant (bi, 3));
                auto* bhz = findParam (*eq, params::eqBant (bi, 2));
                const bool formant = bhz != nullptr && Planner::inFormantZone (*bhz);   // formant bölgesinde statik kesim YASAK
                if (g != nullptr && ! formant) { const float yeni = quantDb (std::clamp (*g - std::min (2.0f, 0.5f * d.valueDb), -6.0f, 0.0f)); if (yeni < *g - 0.05f) { *g = yeni; changed = true; } }
            }
            else if (bandNear (d.centerHz, true, bi))
            {
                auto* rg = findParam (*eq, params::eqBant (bi, 7));
                const float cap = Planner::inFormantZone (d.centerHz) ? -3.0f : -9.0f;
                if (rg != nullptr) { const float yeni = quantDb (std::clamp (*rg - std::min (2.0f, 0.5f * d.valueDb), cap, 0.0f)); if (yeni < *rg - 0.05f) { *rg = yeni; changed = true; } }
            }
        }
        // kalan harsh → dinamik bandın range'i
        {
            const auto* d = find (renderRep.detections, Problem::Harsh);
            int bi;
            if (d != nullptr && d->actionable() && bandNear (d->centerHz, true, bi))
            {
                auto* rg = findParam (*eq, params::eqBant (bi, 7));
                const float cap = Planner::inFormantZone (d->centerHz) ? -3.0f : -9.0f;
                if (rg != nullptr) { const float yeni = quantDb (std::clamp (*rg - std::min (2.0f, 0.6f * d->valueDb), cap, 0.0f)); if (yeni < *rg - 0.05f) { *rg = yeni; changed = true; } }
            }
        }
        // kalan sibilans → de-ess derinliği (12 tavanı yalnız ORİJİNAL kanıt ≥0.8 ise)
        if (ds != nullptr && ds->enabled)
        {
            const auto* d = find (renderRep.detections, Problem::Sibilance);
            auto* rg = findParam (*ds, params::DsRange);
            if (d != nullptr && d->actionable() && d->count > 0 && rg != nullptr)
            {
                const auto* d0 = in0.detections != nullptr ? find (*in0.detections, Problem::Sibilance) : nullptr;
                const float cap = (d0 != nullptr && d0->confidence >= 0.8f && d0->severity >= 0.8f) ? 12.0f : 8.0f;
                const float yeni = quantDb (std::clamp (*rg + std::min (2.0f, 0.4f * std::max (0.0f, d->valueDb - 6.0f) + 0.5f), 0.0f, cap));
                if (yeni > *rg + 0.05f) { *rg = yeni; changed = true; }
            }
            // MUTLAK öne çıkma (T3): girişe göre +2 dB'yi aşan her dB → önce range, tavandaysa eşik
            if (sibFazla > 0.25f && rg != nullptr)
            {
                const auto* d0 = in0.detections != nullptr ? find (*in0.detections, Problem::Sibilance) : nullptr;
                const float cap = (d0 != nullptr && d0->confidence >= 0.8f && d0->severity >= 0.8f) ? 12.0f : 8.0f;
                const float yeni = quantDb (std::clamp (*rg + std::min (3.0f, sibFazla), 0.0f, cap));
                if (yeni > *rg + 0.05f) { *rg = yeni; changed = true; }
                else if (auto* th = findParam (*ds, params::DsThresh))
                {
                    const float yeniTh = quantDb (std::max (Planner::INTERNAL_REF_LUFS - 24.0f, *th - std::min (3.0f, sibFazla)));
                    if (yeniTh < *th - 0.05f) { *th = yeniTh; changed = true; }
                }
            }
            // render'da YENİ donukluk → de-ess'i hafiflet
            const auto* dull = find (renderRep.detections, Problem::Dull);
            const ProblemDetection* dullIn = in0.detections != nullptr ? find (*in0.detections, Problem::Dull) : nullptr;
            if (dull != nullptr && dull->actionable() && (dullIn == nullptr || ! dullIn->actionable()) && rg != nullptr && *rg > 1.5f)
            {
                *rg = quantDb (std::max (0.0f, *rg - 1.5f));
                changed = true;
            }
        }
        // DS kapalıyken zincir s-seslerini belirgin öne çıkarıyorsa (T3): hafif taban de-esser aç
        if (ds != nullptr && ! ds->enabled && sibFazla > 1.5f && ! in0.lockedFor (state::modul::DEESS) && in0.role != state::Role::Master)
        {
            const float merkez = (in0.report != nullptr && in0.report->sibilantCenterHz > 3000.0f) ? in0.report->sibilantCenterHz : 6500.0f;
            const float range = quantDb (std::min (6.0f, 2.0f + sibFazla));
            ds->enabled = true; ds->params.clear(); ds->reasons.clear();
            ds->params.push_back ({ params::DsOn, 1.0f });
            ds->params.push_back ({ params::DsMode, 0.0f });
            ds->params.push_back ({ params::DsHz, quantHz (std::clamp (merkez * 0.85f, 3000.0f, 10000.0f)) });
            ds->params.push_back ({ params::DsThresh, quantDb (Planner::INTERNAL_REF_LUFS - 12.0f) });
            ds->params.push_back ({ params::DsRange, range });
            ds->params.push_back ({ params::DsLookahead, 1.0f });
            ds->params.push_back ({ params::DsAttack, 1.0f });
            ds->params.push_back ({ params::DsRelease, 60.0f });
            ds->params.push_back ({ params::DsLink, 1.0f });
            ds->params.push_back ({ params::DsPost, dsPostHedef });
            Reason rs; rs.rule = ReasonRule::DeEssStyle; rs.vals = { merkez * 0.85f, range, 0.0f, 0.0f }; rs.target = params::DsRange;
            ds->reasons.push_back (rs);
            changed = true;
        }
        return changed;
    }
}

FeedbackResult Feedback::optimize (const PlannerInput& in, const float* store, int64_t n, double sr,
                                   double maxSeconds, int maxRounds)
{
    FeedbackResult res;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    maxRounds = std::clamp (maxRounds, 1, 4);
    std::vector<MixPlan> plans;
    plans.push_back (Planner::plan (in));
    std::vector<float> out ((size_t) std::max<int64_t> (1, n));
    float lufs[4] = { -100, -100, -100, -100 };
    int best = 0;
    const auto sibGiris = sibKareleri (store, n, sr);
    // olay eşiği kayıt seviyesine göreli (denetim KA-5: mutlak −60 dBFS sessiz kayıtta olayları siliyordu)
    const auto sibOlay = sibOlaylari (sibGiris, std::min (-60.0f, in.report->voicedLufs - 22.0f));
    const float sibGirisDb = sibOneCikma (sibGiris, sibOlay);
    // son aşama de-esser eşiği KAYDA göre: girişteki s öne çıkmasının 2 dB altı. Stil parlaklığı sesli harf tizini
    // +3…+4 dB kaldırıyor; s'ler bunun üstünde en fazla ~+2 dB kalsın (korpus: ofset +1 → +2.6, −2 → +2.2/+1.6, −4 ≈ aynı)
    float dsPostHedef = -4.0f;                                              // Planner varsayılanı (olay yoksa)
    {
        // DS kapalıysa ayarla'nın açacağı frekansla aynı formül (denetim KA-4)
        const float merkez = in.report->sibilantCenterHz > 3000.0f ? in.report->sibilantCenterHz : 6500.0f;
        float dsHz = quantHz (std::clamp (merkez * 0.85f, 3000.0f, 10000.0f));
        auto* ds0 = findModule (plans[0], state::modul::DEESS);
        if (ds0 != nullptr && ds0->enabled) if (auto* h = findParam (*ds0, params::DsHz)) dsHz = *h;
        if (! sibOlay.empty())
            dsPostHedef = quantDb (std::clamp (sibGoreliGiris (store, n, sr, dsHz, sibOlay) - 2.0f, -12.0f, 12.0f));
        if (ds0 != nullptr && ds0->enabled)
        {
            if (auto* dp = findParam (*ds0, params::DsPost)) *dp = dsPostHedef;
            else ds0->params.push_back ({ params::DsPost, dsPostHedef });
        }
    }
    for (int r = 0; r < maxRounds; ++r)
    {
        if (r > 0 && ! zamanSiniriYok && (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001 > maxSeconds) break;   // bütçe (§5.3)
        Sandbox::render (plans[(size_t) r], store, out.data(), n, sr, Quality::Yuksek, true);   // M3.4 (T6): gecikme hizalı
        const auto rep = offline::reportFromBuffer (out.data(), n, sr);
        lufs[r] = rep.integratedLufs;
        const float sibFazla = sibOlay.empty() ? 0.0f : sibOneCikma (sibKareleri (out.data(), n, sr), sibOlay) - sibGirisDb - 2.0f;
        res.scores[r] = puanla (rep, *in.report) + 0.15f * std::max (0.0f, sibFazla);
        res.rounds = r + 1;
        if (res.scores[r] < res.scores[best]) best = r;
        if (r > 0 && res.scores[r] > res.scores[best] - 0.02f && best != r) break;   // anlamlı iyileşme yok
        if (r + 1 < maxRounds)
        {
            MixPlan next = plans[(size_t) r];
            if (! ayarla (next, rep, in, sibFazla, dsPostHedef)) break;                                     // ayarlanacak bir şey kalmadı
            plans.push_back (std::move (next));
        }
    }
    res.chosenRound = best;
    res.finalPlan = plans[(size_t) best];
    res.processedLufs = lufs[best];

    // ---- M3.4 EŞ-SES: seçilen planın TAM zinciri (KİR/KATMAN/FX dahil) ölçülür, çıkış trimi stil
    // hedefine çekilir. Ölçüldü: düzeltici mono render stil katmanlarını görmediği için stiller arası 3+ dB
    // seviye farkı kalıyordu ("yüksek = iyi" yanılgısı + sahibin "seviye yüksek olmasın" şartı).
    // Hedef: lead −19 / back −22 / adlib −21 LUFS; stil çıkış trimi yalnız ±1 dB "seviye hissi" olarak kalır.
    // Denetim: (1) kilitli seviye-etkili modülde kullanıcının gerçek ayarı plana girmediği için ölçüm yanıltır →
    // eş-ses atlanır (stil trimi kalır); (2) maliyet sınırlı: en enerjik ≤30 s pencere + 2 s ön-rulo.
    // denetim M3.4 (DSP-2): kilitli modül plana kapalı/parametresiz girer; seviye etkisi olan HER kilitte ölçüm yanıltır
    const bool kilitEngel = in.lockedFor (state::modul::OUTPUT) || in.lockedFor (state::modul::DIRT) || in.lockedFor (state::modul::LAYER)
                         || in.lockedFor (state::modul::GLOW) || in.lockedFor (state::modul::SAT) || in.lockedFor (state::modul::DOUBLER)
                         || in.lockedFor (state::modul::DELAY) || in.lockedFor (state::modul::REVERB) || in.lockedFor (state::modul::WIDTH)
                         || in.lockedFor (state::modul::EQ) || in.lockedFor (state::modul::LEVELER) || in.lockedFor (state::modul::COMP_A)
                         || in.lockedFor (state::modul::COMP_B) || in.lockedFor (state::modul::DEESS) || in.lockedFor (state::modul::AIR)
                         || in.lockedFor (state::modul::GATE) || in.lockedFor (state::modul::YUMUSAT) || in.lockedFor (state::modul::FORMANT)
                         || in.lockedFor (state::modul::TONEQ);
    if (in.role != state::Role::Master && n > 0 && ! kilitEngel)
    {
        float* trim = nullptr;
        ModulePlan* outMod = nullptr;
        for (auto& m : res.finalPlan.modules)
            for (auto& pv : m.params)
                if (pv.id == params::OutputTrim) { trim = &pv.raw; outMod = &m; }
        if (trim != nullptr)
        {
            const int64_t sr1 = std::max<int64_t> (1, (int64_t) sr);
            const int64_t pencere = std::min<int64_t> (n, 30 * sr1), onRulo = std::min<int64_t> (2 * sr1, n - pencere);
            int64_t bas = 0;
            if (n > pencere)
            {
                // 1 s adımlı enerji → en enerjik pencere
                const int64_t adim = sr1, adet = n / adim;
                std::vector<double> e ((size_t) adet, 0.0);
                for (int64_t k = 0; k < adet; ++k)
                    for (int64_t i = k * adim; i < (k + 1) * adim; ++i) e[(size_t) k] += (double) store[i] * store[i];
                const int64_t pAdet = pencere / adim;
                double enIyi = -1.0, toplam = 0.0;
                for (int64_t k = 0; k < adet; ++k)
                {
                    toplam += e[(size_t) k];
                    if (k >= pAdet) toplam -= e[(size_t) (k - pAdet)];
                    if (k + 1 >= pAdet && toplam > enIyi) { enIyi = toplam; bas = (k + 1 - pAdet) * adim; }
                }
            }
            const int64_t r0 = std::max<int64_t> (0, bas - onRulo);
            const int64_t uz = std::min<int64_t> (n - r0, pencere + (bas - r0));
            const int64_t atla = bas - r0;
            std::vector<float> L ((size_t) uz), R ((size_t) uz), M ((size_t) std::max<int64_t> (1, uz - atla));
            // mono bus'ta zincir modülleri 1 kanal çalışır (FX ıslak toplamı stereo ortadan farklı — denetim KA-6)
            const int kanal = in.outChannels >= 2 ? 2 : 1;
            Sandbox::renderStereo (res.finalPlan, store + r0, L.data(), R.data(), uz, sr, Quality::Yuksek, 120.0, true, kanal);
            for (int64_t i = atla; i < uz; ++i) M[(size_t) (i - atla)] = kanal == 2 ? 0.5f * (L[(size_t) i] + R[(size_t) i]) : L[(size_t) i];
            const auto tamRep = offline::reportFromBuffer (M.data(), uz - atla, sr);
            // denetim M3.4 (DSP-1/T2): hedef, DAW ölçerinin ve eklenti etiketinin okuduğu KANAL-TOPLAMLI BS.1770
            // değeridir (mono (L+R)/2 ölçümü stereo izde 3 dB düşük okuyordu → çıkış −16 LUFS). Mono çıkışta mono.
            float olculen = tamRep.integratedLufs;
            if (in.outChannels >= 2)
            {
                LoudnessMeter lm; lm.prepare (sr, 2, 8192);
                for (int64_t i = atla; i < uz; i += 8192)
                {
                    const float* ch2[2] = { L.data() + i, R.data() + i };
                    lm.process (ch2, 2, (int) std::min<int64_t> (8192, uz - i));
                }
                olculen = lm.integratedLufs();
            }
            if (tamRep.integratedLufs > -90.0f && olculen > -90.0f)
            {
                const float taban = in.role == state::Role::Backing ? -22.0f : in.role == state::Role::Adlib ? -21.0f : -19.0f;
                const float hedef = taban + std::clamp (0.5f * (*trim - 3.0f), -1.0f, 1.0f);
                const float eski = *trim;
                *trim = quantDb (std::clamp (*trim + (hedef - olculen), -6.0f, 10.0f));
                // A/B eş-ses girişin MONO ölçümüyle karşılaştırılır → işlenmiş seviye de aynı alanda (mono) kalır
                res.processedLufs = tamRep.integratedLufs + (*trim - eski);
                res.fullChainLufs = res.processedLufs;
                for (auto& rs : outMod->reasons)                                   // denetim: gerekçe eski trimi gösteriyordu
                    if (rs.rule == ReasonRule::OutputLevel) rs.vals[0] = *trim;
            }
        }
    }
    return res;
}

} // namespace sweax::decision
