// SWEAX NEXT — Decision/Planner.cpp
#include "Planner.h"
#include "Receteler.h"
#include <algorithm>

namespace sweax::decision
{

namespace
{
    using params::Id;

    void pv (ModulePlan& m, Id id, float raw) { m.params.push_back ({ id, raw }); }

    struct BandUse
    {
        float hz, gainDb, q; int type; bool dyn; float dynThresh, dynRange, dynAtk, dynRel;
        Reason reason; float priority;
    };

    // yardımcı: gerekçeli "kapalı" modül
    ModulePlan disabledModule (const char* id, ReasonRule rule, const ProblemDetection* d)
    {
        ModulePlan m; m.moduleId = id; m.enabled = false;
        Reason r; r.rule = rule;
        if (d != nullptr) { r.det = d->type; r.vals = { d->probability, d->severity, d->confidence, 0.0f }; }
        m.reasons.push_back (r);
        return m;
    }
}

MixPlan Planner::plan (const PlannerInput& in)
{
    MixPlan out;
    const auto& r = *in.report;
    const auto& dets = *in.detections;
    out.role = in.role;
    out.planId = in.planId;
    out.detections = dets;
    out.overallConfidence = r.confidence.overall();
    out.layout = 0;                                        // Standard; DualDeEss yalnız aşırı sibilans kanıtıyla (aşağıda)

    auto det = [&] (Problem p) { return Detectors::find (dets, p); };
    const float lowConfMul = r.confidence.enough() ? 1.0f : 0.7f;      // düşük güven cerrahi derinliği AZALTIR (§5.2)
    // denetim M3.4 (PT-2): gürültülü kayıt korumaları (kapı aralığı, KİR −4 dB, GLOW ×0.6) yalnız NoiseFloor güvenine
    // bağlıydı; güven çökünce SNR 29 dB kayıt temiz planlanıyordu → ölçülen SNR yedeği, tek bayrak
    const auto* nfOrtak = det (Problem::NoiseFloor);
    const bool nfKanit = nfOrtak != nullptr && nfOrtak->actionable (0.4f, 0.25f);
    const bool gurultuluKayit = nfKanit || (r.noiseFloorKnown && r.roomToneSec > 0.2 && r.snrDb > 0.0f && r.snrDb < 35.0f);
    const float gurultuSiddet = nfKanit ? nfOrtak->severity : (float) std::clamp ((45.0 - (double) r.snrDb - 5.0) / 25.0, 0.0, 1.0);

    // ---- PRESET REÇETESİ (M3.5, Decision/Receteler.h) ----
    // Tarihçe: M2 rol/stil tabanı (analiz İNCELTİR, kapatmaz) → M3 sanatçı profilleri → M3.4 diskografi kimliği
    // (KİR/KATMAN) → M3.5 sahip: "her biri için özel sıralama, özel kalite" — reçete zincirin TAMAMINI taşır:
    // ekleme sırası + her modülün stil tabanı (eskiden tüm stillerde ortak sabitler). Sanatçı reçeteleri internet
    // preset/tutorial arkeolojisi + şarkı kanıtı + tasarımcı/eleştirmen/hakem hattından.
    const auto& stil = recete (in.style);
    out.sira = siraGecerli (stil.sira) ? stil.sira : varsayilanSira();
    const bool vokalRol = in.role == state::Role::Lead || in.role == state::Role::Backing || in.role == state::Role::Adlib;
    const bool masterRol = in.role == state::Role::Master;

    // ---------------- Input trim: sesli-gate'li loudness → dahili referans ----------------
    {
        const float kaynak = masterRol ? r.integratedLufs : r.voicedLufs;   // master: tam karışım entegre
        const float trim = quantDb (std::clamp (INTERNAL_REF_LUFS - kaynak, -24.0f, 24.0f));
        const bool yeterli = masterRol ? r.capturedSec > 2.0 : r.voicedSec > 1.0;
        out.inputTrimDb = (yeterli && std::fabs (trim) >= 0.5f) ? trim : 0.0f;
    }
    const float ref = INTERNAL_REF_LUFS;                   // trim sonrası sesli-gate'li seviye ≈ −18

    // ---------------- Gate: ölçülen tabana göre gürültü kapısı ----------------
    {
        ModulePlan g; g.moduleId = state::modul::GATE;
        if (in.lockedFor (state::modul::GATE)) { Reason rs; rs.rule = ReasonRule::Locked; g.reasons.push_back (rs); }
        else if (vokalRol && r.noiseFloorDb > -110.0f && r.roomToneSec > 0.2)
        {
            const float floorPost = r.noiseFloorDb + out.inputTrimDb;              // kapı trim SONRASI sinyali görür
            // denetim M3.4 (PT-1): −33 dBFS tavanı gürültülü kayıtta eşiği gürültü RMS'ine indiriyordu (kapı hiç
            // kapanmıyor, duraklama SNR'ı 6 dB kötüleşiyordu) → tavan vokal referansına göre (ref − 6).
            // (T5) taban bilinmiyorsa (tabana yakın kare yok, yalnız üst sınır) taban değeri: ref − 36.
            const float thresh = r.noiseFloorKnown ? quantDb (std::clamp (floorPost + 10.0f, -80.0f, ref - 6.0f))
                                                   : quantDb (ref - 36.0f);
            // TEMİZLİK turu: "arkadaki gürültü duyulmasın" — noisy eşiği duyarlılaştı (0.5/0.35 › 0.4/0.25),
            // taban bastırma 8 › 12 dB, noisy tavan 18 › 22 dB, hold 80 › 120 ms (kelime kuyruğu kesilmesin).
            const bool noisy = gurultuluKayit;
            const float range = quantDb (noisy ? std::min (22.0f, 12.0f + 10.0f * gurultuSiddet) : 12.0f);
            g.enabled = true;
            pv (g, params::GateOn, 1.0f);
            pv (g, params::GateThresh, thresh);
            pv (g, params::GateRange, range);
            pv (g, params::GateAttack, 1.5f);
            pv (g, params::GateRelease, quantMs (noisy ? std::min (100.0f, stil.gateRelMs) : stil.gateRelMs));
            pv (g, params::GateHold, quantMs (stil.gateHoldMs));
            pv (g, params::GateHyst, 4.0f);
            Reason rs; rs.rule = noisy ? ReasonRule::GateNoise : ReasonRule::GateBaseline;
            if (noisy) rs.det = Problem::NoiseFloor;
            rs.vals = { thresh, floorPost, range, 0.0f }; rs.target = params::GateThresh;
            g.reasons.push_back (rs);
        }
        else g = disabledModule (state::modul::GATE, vokalRol ? ReasonRule::LowConfidence : ReasonRule::StyleOff, vokalRol ? det (Problem::NoiseFloor) : nullptr);
        out.modules.push_back (std::move (g));
    }

    // ---------------- EQ ----------------
    // M3.5: düzeltici EQ (HPF/patlama koruması/kanıt kesimleri) = Eq modülü; preset TON bantları (presence/hava/gövde/
    // koyu üst/alçak raf/reçete ek bantları) = AYRI TonEq modülü → reçete sırasında istenen yere (ör. kompresörden sonra)
    // konur ve kanıt kesimlerinin 6 bantlık bütçesini yemez (M3.4'te stil bantları 3 yuvaya sıkıştırılıyordu).
    std::vector<BandUse> stilBands;
    {
        ModulePlan eq; eq.moduleId = state::modul::EQ;
        std::vector<BandUse> bands;
        const auto* f0ok = r.f0P5Hz > 20.0f ? &r : nullptr;

        // HPF: yalnız LF gürültü / boom / proximity / plosive kanıtıyla; fc ≤ 0.75·F0_p05, asla sabit sayı
        {
            const auto* lf = det (Problem::LfNoise); const auto* bo = det (Problem::Boom);
            const auto* px = det (Problem::Proximity); const auto* pl = det (Problem::Plosive);
            const ProblemDetection* best = nullptr;
            for (const auto* d : { lf, bo, px }) if (d != nullptr && d->actionable() && (best == nullptr || d->probability * d->severity > best->probability * best->severity)) best = d;
            const bool plosiveEv = pl != nullptr && pl->actionable();
            if ((best != nullptr || plosiveEv) && f0ok != nullptr)
            {
                const float fcMax = 0.75f * r.f0P5Hz;
                // merkez: kanıtın merkezinin ~1.4 katına kadar (enerjiyi kesmek için üstüne çık), tavan fcMax
                const float evHz = best != nullptr ? best->centerHz : (pl != nullptr ? pl->centerHz : 80.0f);
                const float fc = quantHzDown (std::min (fcMax, std::max (30.0f, evHz * 1.4f)));
                const float sev = best != nullptr ? best->severity : pl->severity;
                eq.enabled = true;
                pv (eq, params::EqHpfOn, 1.0f);
                pv (eq, params::EqHpfHz, fc);
                pv (eq, params::EqHpfSlope, sev > 0.5f ? 1.0f : 0.0f);
                Reason rs; rs.rule = best == lf && lf != nullptr ? ReasonRule::HpfLfNoise
                                   : best == px && px != nullptr ? ReasonRule::HpfProximity
                                   : best != nullptr ? ReasonRule::HpfLfNoise : ReasonRule::HpfPlosive;
                rs.det = best != nullptr ? best->type : Problem::Plosive;
                rs.vals = { fc, r.f0P5Hz, best != nullptr ? best->valueDb : (pl != nullptr ? pl->valueDb : 0.0f), 0.0f };
                rs.target = params::EqHpfHz;
                eq.reasons.push_back (rs);
            }
            // M2 taban: kanıt yoksa da rap vokal HPF'i (F0'a bağlı, sabit sayı değil; ≤0.75·F0_p05 guardrail'i içinde)
            if (! eq.enabled && vokalRol && f0ok != nullptr)
            {
                // denetim düzeltmesi: F0_p05 < 66.7 Hz'de eski clamp(lo=50, hi=0.75·f0) UB idi (hi < lo)
                const float fc = quantHzDown (std::clamp (0.55f * r.f0P5Hz, 30.0f, std::max (30.0f, 0.75f * r.f0P5Hz)));
                eq.enabled = true;
                pv (eq, params::EqHpfOn, 1.0f);
                pv (eq, params::EqHpfHz, fc);
                pv (eq, params::EqHpfSlope, 0.0f);
                Reason rs; rs.rule = ReasonRule::HpfBaseline;
                rs.vals = { fc, r.f0P5Hz, 0.0f, 0.0f }; rs.target = params::EqHpfHz;
                eq.reasons.push_back (rs);
            }
            // PATLAMA KORUMASI (M3.4 denetim T1 — sahip: "harfler patlamasın"): HPF ÖNCESİ, F0-altı tetikli dinamik
            // low-shelf. Köşe ≈1.8·F0_p05 (100-180 Hz): patlamanın 80-200 Hz gövdesini kapsar; tetik köşenin yarısı
            // altı ve gövdeye göreli (+6 dB) — sesli heceye dokunmaz. Gerçek vokal ızgara araması (anavokal4, 18 patlama):
            // 80-200 Hz patlama −6.9 dB, sesli −0.2 dB. Kanıt yoksa taban −6, kanıtla −15'e kadar.
            if (vokalRol && f0ok != nullptr)
            {
                const float plHz = quantHzDown (std::clamp (1.8f * r.f0P5Hz, 100.0f, 180.0f));
                const float plRange = quantDb (plosiveEv ? -std::clamp (6.0f + 9.0f * pl->severity * pl->confidence, 6.0f, 15.0f) : -6.0f);
                eq.enabled = true;
                pv (eq, params::EqPlosOn, 1.0f);
                pv (eq, params::EqPlosHz, plHz);
                pv (eq, params::EqPlosRange, plRange);
                Reason rs; rs.rule = ReasonRule::DynPlosiveLf; rs.det = Problem::Plosive;
                rs.vals = { plHz, plRange, plosiveEv ? (float) pl->count : 0.0f, 0.0f }; rs.target = params::EqPlosRange;
                eq.reasons.push_back (rs);
            }
        }
        // tonal kesimler: boom/mud/box/nasal — statik bell, derinlik = şiddet×güven, Q bant genişliğinden
        auto tonalCut = [&] (Problem p, ReasonRule rule)
        {
            const auto* d = det (p);
            if (d == nullptr || ! d->actionable()) return;
            BandUse b {};
            b.hz = quantHz (d->centerHz);
            b.type = 0;
            b.gainDb = quantDb (-std::min (6.0f, (1.5f + 4.5f * d->severity) * d->confidence * lowConfMul));   // statik ≤6
            b.q = quantQ (std::clamp (qFromBandwidth (d->bandwidthOct), 0.5f, 4.0f));                          // rezonanssız Q ≤4
            b.reason.rule = rule; b.reason.det = p; b.reason.vals = { b.hz, b.gainDb, b.q, d->valueDb };
            b.priority = d->probability * d->severity * d->confidence;
            bands.push_back (b);
        };
        tonalCut (Problem::Boom, ReasonRule::CutBoom);
        tonalCut (Problem::Mud, ReasonRule::CutMud);
        tonalCut (Problem::Box, ReasonRule::CutBox);
        tonalCut (Problem::Nasal, ReasonRule::CutNasal);
        // rezonanslar: en güçlü 2; formant bölgesinde YALNIZ dinamik ≤3 dB, dışında statik ≤6 / Q ≤8
        {
            std::vector<const ProblemDetection*> rz;
            for (const auto& d : dets) if (d.type == Problem::Resonance && d.actionable()) rz.push_back (&d);
            std::sort (rz.begin(), rz.end(), [] (auto* a, auto* b) { return a->probability * a->severity > b->probability * b->severity; });
            for (size_t i = 0; i < rz.size() && i < 2; ++i)
            {
                const auto* d = rz[i];
                BandUse b {};
                b.hz = quantHz (d->centerHz);
                b.q = quantQ (std::clamp (qFromBandwidth (d->bandwidthOct), 1.0f, 8.0f));   // Q ≤8 yalnız rezonans kanıtı
                b.type = 0;
                const float depth = std::min (6.0f, 0.6f * d->valueDb * d->confidence * lowConfMul);
                if (inFormantZone (b.hz))
                {
                    b.dyn = true; b.gainDb = 0.0f;
                    b.dynRange = quantDb (-std::min (3.0f, depth));                        // formant koruması
                    b.dynThresh = quantDb (ref - 3.0f); b.dynAtk = 2.0f; b.dynRel = 120.0f;
                }
                else b.gainDb = quantDb (-depth);
                b.reason.rule = ReasonRule::CutResonance; b.reason.det = Problem::Resonance;
                b.reason.vals = { b.hz, b.dyn ? b.dynRange : b.gainDb, b.q, d->valueDb };
                b.priority = d->probability * d->severity * d->confidence * 1.1f;
                bands.push_back (b);
            }
        }
        // harsh: dinamik bant (seviye bağımlı), formant bölgesindeyse ≤3 dB
        {
            const auto* d = det (Problem::Harsh);
            if (d != nullptr && d->actionable())
            {
                BandUse b {};
                b.hz = quantHz (d->centerHz); b.type = 0; b.gainDb = 0.0f;
                b.q = quantQ (std::clamp (qFromBandwidth (d->bandwidthOct), 0.7f, 3.0f));
                b.dyn = true;
                const float cap = inFormantZone (b.hz) ? 3.0f : 9.0f;
                b.dynRange = quantDb (-std::min (cap, (2.0f + 5.0f * d->severity) * d->confidence * lowConfMul));
                b.dynThresh = quantDb (ref - 2.0f);                    // yalnız yüksek pasajlarda (harsh seviye bağımlı)
                b.dynAtk = 3.0f; b.dynRel = 150.0f;
                b.reason.rule = ReasonRule::DynHarsh; b.reason.det = Problem::Harsh;
                b.reason.vals = { b.hz, b.dynRange, b.dynThresh, d->valueDb };
                b.priority = d->probability * d->severity * d->confidence;
                bands.push_back (b);
            }
        }
        // eğim düzeltmeleri: dull → hafif high-shelf boost (≤3), bright → high-shelf kesim (≤6), thin → low-shelf boost (≤3)
        auto shelf = [&] (Problem p, ReasonRule rule, int type, bool boost)
        {
            const auto* d = det (p);
            if (d == nullptr || ! d->actionable()) return;
            BandUse b {};
            b.type = type;
            b.hz = quantHz (d->centerHz);
            const float mag = (boost ? std::min (3.0f, 1.0f + 2.0f * d->severity) : std::min (6.0f, 1.5f + 4.5f * d->severity)) * d->confidence * lowConfMul;
            b.gainDb = quantDb (boost ? mag : -mag);
            b.q = 0.7f;
            b.reason.rule = rule; b.reason.det = p; b.reason.vals = { b.hz, b.gainDb, 0.0f, d->valueDb };
            b.priority = d->probability * d->severity * d->confidence * 0.8f;
            bands.push_back (b);
        };
        shelf (Problem::Dull, ReasonRule::ShelfDull, 2, true);
        // Bright: modern rap parlaklığı İSTER — kesim yalnız sapma ciddi (>6 dB) ise ve ≤2 dB (M2)
        {
            const auto* d = det (Problem::Bright);
            if (d != nullptr && d->actionable() && d->valueDb > 6.0f)
            {
                BandUse b {};
                b.type = 2; b.hz = quantHz (d->centerHz);
                b.gainDb = quantDb (-std::min (2.0f, 0.4f * (d->valueDb - 4.0f) * d->confidence * lowConfMul));
                b.q = 0.7f;
                b.reason.rule = ReasonRule::ShelfBright; b.reason.det = Problem::Bright;
                b.reason.vals = { b.hz, b.gainDb, 0.0f, d->valueDb };
                b.priority = d->probability * d->severity * d->confidence * 0.8f;
                bands.push_back (b);
            }
        }
        shelf (Problem::Thin, ReasonRule::ShelfThin, 1, true);
        // hum: dar notch (statik, Q ≤8 hum kanıtıyla — dar mekanik ton)
        {
            const auto* d = det (Problem::Hum);
            if (d != nullptr && d->actionable())
            {
                BandUse b {};
                b.hz = quantHz (d->centerHz); b.type = 3; b.gainDb = 0.0f; b.q = 8.0f;
                b.reason.rule = ReasonRule::CutResonance; b.reason.det = Problem::Hum; b.reason.vals = { b.hz, 0.0f, 8.0f, d->valueDb };
                b.priority = d->probability * d->severity;
                bands.push_back (b);
            }
        }
        // M2 stil hedefi: ticari rap eğrisine doğru presence (4.3k bell) + hava (10.5k shelf); ölçülen sapmaya
        // göre EKSİK kadar boost (guardrail ≤3 dB); zaten parlaksa dokunma. Formant bölgesinin (2.2–3.6k) ÜSTÜnde.
        if (vokalRol && r.tonalFrames >= 20)
        {
            const float presDev = Detectors::regionDeviationDb (r, 2500.0, 5000.0);
            const float airDev  = Detectors::regionDeviationDb (r, 8000.0, 14000.0);
            const float bodyDev = Detectors::regionDeviationDb (r, 150.0, 350.0);
            // gövde (cılız ses telafisi): yalnız EKSİKSE ve mud/boom fazlalığı ölçülmediyse
            {
                const auto* mudD = det (Problem::Mud); const auto* boomD = det (Problem::Boom);
                const bool fazlalik = (mudD != nullptr && mudD->actionable()) || (boomD != nullptr && boomD->actionable());
                // denetim M3.4 (PT-8): stil KESİMİ yalnız aynı bölgede (±0.5 okt) kanıt kesimi varsa atlanır —
                // 100 Hz gümlemesi 500 Hz kimlik kesimini kapatıyordu
                bool yakinKanitKesim = false;
                for (const auto& kb : bands)
                    if ((kb.reason.rule == ReasonRule::CutBoom || kb.reason.rule == ReasonRule::CutMud || kb.reason.rule == ReasonRule::CutBox)
                        && std::fabs (std::log2 (std::max (1.0f, kb.hz) / std::max (1.0f, stil.bodyHz))) < 0.5f)
                        yakinKanitKesim = true;
                // M3.4: işaretli gövde — pozitif: eksikse yarı telafili boost; negatif: stil kesimi (Ati242 200-350 hafif kesim)
                const float bodyBoost = stil.bodyDb >= 0.0f
                    ? quantDb (std::clamp (stil.bodyDb - 0.5f * bodyDev, 0.0f, 3.0f))
                    : quantDb (std::clamp (stil.bodyDb, -3.0f, 0.0f));
                if ((stil.bodyDb < 0.0f && bodyBoost <= -0.5f && ! yakinKanitKesim) || (stil.bodyDb >= 0.0f && ! fazlalik && bodyBoost >= 0.5f))
                {
                    BandUse b {};
                    b.type = 0; b.hz = quantHz (stil.bodyHz); b.gainDb = bodyBoost; b.q = 0.9f;
                    b.reason.rule = bodyBoost < 0.0f ? ReasonRule::StyleBodyCut : ReasonRule::StyleBody; b.reason.vals = { b.hz, bodyBoost, bodyDev, 0.0f };
                    b.priority = 0.6f;
                    stilBands.push_back (b);
                }
            }
            const float presBoost = quantDb (std::clamp (stil.presenceDb - 0.5f * presDev, 0.0f, 3.0f));   // M3.3: yarı telafi
            // denetim M3.2 (K-2/K-3): hava shelf'i artık (a) sibilans kanıtıyla yarılanır (AIR korumasıyla
            // aynı mantık — tek taraf korumak anlamsızdı) ve (b) stil AIR HIGH yükünün beklenen katkısı
            // (≈ airHigh·9 dB·0.4 program payı) düşülür — EQ+AIR toplamı artık ~+3 dB bandında kalır.
            const auto* sibD = det (Problem::Sibilance);
            const float sibKis = (sibD != nullptr && sibD->actionable()) ? 0.5f : 1.0f;
            const float airYuku = stil.airHigh * 9.0f * 0.4f;
            // denetim M3.4 (PT-5): negatif raflı stilde (Killoki koyu üst) donuk kayıtta +3 hava VE −2.5 koyu üst
            // birlikte çıkıp kimliği iptal ediyordu → hava rafı yalnız pozitif shelfDb'den
            const float airBoost  = stil.shelfDb > 0.0f
                ? quantDb (std::clamp ((stil.shelfDb - 0.5f * airDev - airYuku) * sibKis, 0.0f, 3.0f))   // M3.3: yarı telafi
                : 0.0f;
            const float airRol = in.role == state::Role::Backing ? 1.0f : 0.0f;    // back vokal daha karanlık durur
            if (presBoost >= 0.5f)
            {
                BandUse b {};
                b.type = 0; b.hz = quantHz (stil.presHz); b.gainDb = presBoost; b.q = 0.8f;   // M3.4: trap metal 2.5k mid-spike / diğerleri 4.3k
                b.reason.rule = ReasonRule::StylePresence; b.reason.vals = { b.hz, presBoost, presDev, 0.0f };
                b.priority = 0.65f;
                stilBands.push_back (b);
            }
            if (airBoost - airRol >= 0.5f)
            {
                BandUse b {};
                b.type = 2; b.hz = quantHz (stil.havaHz); b.gainDb = quantDb (airBoost - airRol); b.q = 0.7f;
                b.reason.rule = ReasonRule::StyleAir; b.reason.vals = { b.hz, b.gainDb, airDev, 0.0f };
                b.priority = 0.6f;
                stilBands.push_back (b);
            }
            // M3.4: koyu üst (trap metal "11 kHz üstü −2 dB") — negatif raf
            if (stil.shelfDb <= -0.5f)
            {
                BandUse b {};
                b.type = 2; b.hz = quantHz (stil.koyuHz); b.gainDb = quantDb (std::clamp (stil.shelfDb, -4.0f, 0.0f)); b.q = 0.7f;
                b.reason.rule = ReasonRule::StyleDarkTop; b.reason.vals = { b.hz, b.gainDb, airDev, 0.0f };
                b.priority = 0.62f;
                stilBands.push_back (b);
            }
            // M3.4: stil alçak kesimi (trap metal HP 150 / drill 110) — HPF guardrail'i (≤0.75·F0) KORUNUR:
            // HPF'i yükseltmek yerine o frekansta −3 dB alçak raf (inceltir, temeli yok etmez)
            if (stil.hpfMinHz >= 60.0f)
            {
                BandUse b {};
                b.type = 1; b.hz = quantHz (stil.hpfMinHz); b.gainDb = -3.0f; b.q = 0.7f;
                b.reason.rule = ReasonRule::StyleLowCut; b.reason.vals = { b.hz, -3.0f, bodyDev, 0.0f };
                b.priority = 0.58f;
                stilBands.push_back (b);
            }
            // M3.5 REÇETE EK BANTLARI (preset kimliği: ör. orta çukur, gülümseme eğrisi, üst-orta vurgu).
            // Kesim: statik ≤4 dB; formant bölgesinde (2.2–3.6k) yalnız dinamik ≤3 (guardrail); yakınında (±0.5 okt)
            // kanıt kesimi varsa atlanır (çift kesim yok). Yükseltme: ölçülen bölge sapmasına göre YARI telafili ≤3 dB.
            for (const auto& e : stil.ek)
            {
                if (std::fabs (e.db) < 0.5f || e.hz < 40.0f) continue;
                const int tip = std::clamp ((int) std::lround (e.tip), 0, 2);
                const float hz = quantHz (std::clamp (e.hz, 40.0f, 16000.0f));
                const float lo = tip == 2 ? hz : tip == 1 ? std::max (40.0f, hz / 2.5f) : hz / 1.41f;
                const float hi = tip == 2 ? std::min (16000.0f, hz * 2.5f) : tip == 1 ? hz : hz * 1.41f;
                const float dev = Detectors::regionDeviationDb (r, (double) lo, (double) hi);
                BandUse b {};
                b.type = tip; b.hz = hz; b.q = quantQ (std::clamp (e.q, 0.3f, 4.0f));
                if (e.db < 0.0f)
                {
                    bool yakinKanit = false;
                    for (const auto& kb : bands)
                        if ((kb.reason.rule == ReasonRule::CutBoom || kb.reason.rule == ReasonRule::CutMud || kb.reason.rule == ReasonRule::CutBox
                             || kb.reason.rule == ReasonRule::CutNasal || kb.reason.rule == ReasonRule::CutResonance)
                            && std::fabs (std::log2 (std::max (1.0f, kb.hz) / hz)) < 0.5f)
                            yakinKanit = true;
                    if (yakinKanit) continue;
                    if (tip == 0 && inFormantZone (hz))
                    {
                        b.dyn = true; b.gainDb = 0.0f;
                        b.dynRange = quantDb (std::clamp (e.db, -3.0f, 0.0f));
                        b.dynThresh = quantDb (ref - 4.0f); b.dynAtk = 3.0f; b.dynRel = 150.0f;
                    }
                    else b.gainDb = quantDb (std::clamp (e.db, -4.0f, 0.0f));
                }
                else
                {
                    // denetim M3.5: telafi SINIRLI (±1.5 dB) — kayıt o bölgede güçlüyse kimlik bandı yok olmuyor, inceliyor
                    b.gainDb = quantDb (std::clamp (e.db - std::clamp (0.5f * dev, -1.5f, 1.5f), 0.0f, 3.0f));
                    if (b.gainDb < 0.3f) continue;
                }
                b.reason.rule = ReasonRule::StyleBand; b.reason.vals = { b.hz, b.dyn ? b.dynRange : b.gainDb, dev, 0.0f };
                b.priority = 0.61f;
                if (b.dyn) bands.push_back (b);                 // formant bölgesi dinamik kesimi düzeltici EQ'da (TonEq statiktir)
                else stilBands.push_back (b);
            }
        }
        // MASTER guardrail: tam karışımda EQ hamleleri KÜÇÜK olur — kesim ≤2 dB, boost ≤1.5, dinamik ≤3
        if (masterRol)
            for (auto& b : bands)
            {
                b.gainDb = quantDb (std::clamp (b.gainDb, -2.0f, 1.5f));
                b.dynRange = quantDb (std::clamp (b.dynRange, -3.0f, 0.0f));
            }
        // bütçe: 6 bant, öncelik sırasına göre; frekansa göre sırala (UI okunurluğu, determinizm)
        std::sort (bands.begin(), bands.end(), [] (const BandUse& a, const BandUse& b) { return a.priority > b.priority; });
        if ((int) bands.size() > params::EQ_BANT_SAYISI) bands.resize ((size_t) params::EQ_BANT_SAYISI);
        std::sort (bands.begin(), bands.end(), [] (const BandUse& a, const BandUse& b) { return a.hz < b.hz; });
        for (size_t i = 0; i < bands.size(); ++i)
        {
            const auto& b = bands[i];
            const int bi = (int) i;
            eq.enabled = true;
            pv (eq, params::eqBant (bi, 0), 1.0f);
            pv (eq, params::eqBant (bi, 1), (float) b.type);
            pv (eq, params::eqBant (bi, 2), b.hz);
            pv (eq, params::eqBant (bi, 3), b.gainDb);
            pv (eq, params::eqBant (bi, 4), b.q);
            pv (eq, params::eqBant (bi, 5), b.dyn ? 1.0f : 0.0f);
            if (b.dyn)
            {
                pv (eq, params::eqBant (bi, 6), b.dynThresh);
                pv (eq, params::eqBant (bi, 7), b.dynRange);
                pv (eq, params::eqBant (bi, 8), quantMs (b.dynAtk));
                pv (eq, params::eqBant (bi, 9), quantMs (b.dynRel));
            }
            Reason rs = b.reason; rs.target = params::eqBant (bi, b.dyn ? 7 : 3);
            eq.reasons.push_back (rs);
        }
        if (eq.enabled) pv (eq, params::EqOn, 1.0f);
        else eq = disabledModule (state::modul::EQ, ReasonRule::NoEvidence, nullptr);
        if (in.lockedFor (state::modul::EQ)) { eq = ModulePlan {}; eq.moduleId = state::modul::EQ; Reason rs; rs.rule = ReasonRule::Locked; eq.reasons.push_back (rs); }
        out.modules.push_back (std::move (eq));
    }

    // ---------------- TON EQ (M3.5): preset ton eğrisi — en çok 4 statik bant, öncelik sırasıyla ----------------
    {
        ModulePlan teq; teq.moduleId = state::modul::TONEQ;
        if (in.lockedFor (state::modul::TONEQ)) { Reason rs; rs.rule = ReasonRule::Locked; teq.reasons.push_back (rs); }
        else if (! stilBands.empty())
        {
            std::stable_sort (stilBands.begin(), stilBands.end(), [] (const BandUse& a, const BandUse& b) { return a.priority > b.priority; });
            if ((int) stilBands.size() > params::TEQ_BANT_SAYISI) stilBands.resize ((size_t) params::TEQ_BANT_SAYISI);
            std::sort (stilBands.begin(), stilBands.end(), [] (const BandUse& a, const BandUse& b) { return a.hz < b.hz; });
            teq.enabled = true;
            pv (teq, params::TeqOn, 1.0f);
            for (size_t i = 0; i < stilBands.size(); ++i)
            {
                const auto& b = stilBands[i];
                const int bi = (int) i;
                pv (teq, params::teqBant (bi, 0), 1.0f);
                pv (teq, params::teqBant (bi, 1), (float) std::clamp (b.type, 0, 2));
                pv (teq, params::teqBant (bi, 2), b.hz);
                pv (teq, params::teqBant (bi, 3), quantDb (std::clamp (b.gainDb, -12.0f, 12.0f)));
                pv (teq, params::teqBant (bi, 4), quantQ (std::clamp (b.q, 0.1f, 8.0f)));
                Reason rs = b.reason; rs.target = params::teqBant (bi, 3);
                teq.reasons.push_back (rs);
            }
        }
        else teq = disabledModule (state::modul::TONEQ, ReasonRule::StyleOff, nullptr);
        out.modules.push_back (std::move (teq));
    }

    // ---------------- Leveler: cümleler arası makro dinamik ----------------
    {
        ModulePlan lev; lev.moduleId = state::modul::LEVELER;
        const auto* d = det (Problem::MacroDynamics);
        if (in.lockedFor (state::modul::LEVELER)) { Reason rs; rs.rule = ReasonRule::Locked; lev.reasons.push_back (rs); }
        else if (masterRol) lev = disabledModule (state::modul::LEVELER, ReasonRule::StyleOff, nullptr);   // tam karışımda vokal sürücüsü pompalar
        else if (d != nullptr && d->actionable())
        {
            lev.enabled = true;
            const float span = std::min (12.0f, 0.7f * d->valueDb * d->confidence * lowConfMul + 1.0f);
            pv (lev, params::LevOn, 1.0f);
            pv (lev, params::LevTarget, quantDb (ref));
            pv (lev, params::LevMaxUp, quantDb (std::clamp (span, 1.0f, 8.0f)));
            pv (lev, params::LevMaxDown, quantDb (std::clamp (span, 1.0f, 8.0f)));
            pv (lev, params::LevSpeed, stil.levHiz);
            Reason rs; rs.rule = ReasonRule::LevelerMacro; rs.det = Problem::MacroDynamics;
            rs.vals = { r.sentenceIqrDb, r.sentenceP95P05Db, ref, span }; rs.target = params::LevMaxUp;
            lev.reasons.push_back (rs);
        }
        else if (vokalRol)
        {
            // M2 taban: kanıt zayıf olsa da rap vokal cümle dengesi ister (varsayılan ±4 dB, hızlı değil)
            lev.enabled = true;
            pv (lev, params::LevOn, 1.0f);
            pv (lev, params::LevTarget, quantDb (ref));
            pv (lev, params::LevMaxUp, quantDb (stil.levAralik));
            pv (lev, params::LevMaxDown, quantDb (stil.levAralik));
            pv (lev, params::LevSpeed, stil.levHiz);
            Reason rs; rs.rule = ReasonRule::LevBaseline; rs.vals = { stil.levAralik, 0.0f, 0.0f, 0.0f }; rs.target = params::LevMaxUp;
            lev.reasons.push_back (rs);
        }
        else lev = disabledModule (state::modul::LEVELER, ReasonRule::NoEvidence, d);
        out.modules.push_back (std::move (lev));
    }

    // ---------------- Comp A: cümle içi mikro dinamik ----------------
    {
        ModulePlan ca; ca.moduleId = state::modul::COMP_A;
        const auto* micro = det (Problem::MicroDynamics);
        const auto* over = det (Problem::OverCompressed);
        if (in.lockedFor (state::modul::COMP_A)) { Reason rs; rs.rule = ReasonRule::Locked; ca.reasons.push_back (rs); }
        else if (over != nullptr && over->actionable())
            ca = disabledModule (state::modul::COMP_A, ReasonRule::CompSkipOverCompressed, over);
        else if (micro != nullptr && micro->actionable() && vokalRol)
        {
            // denetim M3.2 (K-1): kanıt yolu stil profilini YUTUYORDU (Killoki 6:1 → 2.85:1 Clean'e
            // düşüyordu) — sözleşme "stil TABAN, analiz DERİNLEŞTİRİR": oran ve GR hedefi stil tabanının
            // ALTINA inemez; mikro-dinamik kanıtı yalnız derinlik EKLER; punch karakteri stilden gelir.
            ca.enabled = true;
            const float stilGr = stil.grA + std::max (0.0f, 0.4f * (r.crestMedianDb - 8.0f));
            const float kanitGr = (2.0f + 4.0f * micro->severity) * micro->confidence * lowConfMul;
            const float grTarget = std::min (roleGrCeiling (in.role), std::max (stilGr, kanitGr));
            const float ratio = quantQ (std::max (stil.ratioA, std::clamp (1.8f + 1.5f * micro->severity, 1.5f, 4.0f)));
            // eşik: referans etrafında; GR hedefi ~ (tepe − eşik)·(1 − 1/oran) → eşik = ref + crest_pay − GR·oran/(oran−1)
            const float crestPay = std::min (10.0f, r.crestMedianDb);
            const float thresh = quantDb (ref + crestPay - grTarget * ratio / std::max (0.5f, ratio - 1.0f));
            const bool punch = stil.ratioA >= 4.0f;
            pv (ca, params::CaOn, 1.0f);
            pv (ca, params::CaThresh, std::clamp (thresh, -60.0f, 0.0f));
            pv (ca, params::CaRatio, ratio);
            pv (ca, params::CaKnee, stil.caKnee >= 0.0f ? stil.caKnee : 6.0f);
            pv (ca, params::CaAttack, quantMs (std::clamp (stil.attackMs, 0.5f, 30.0f)));    // denetim: kanıt yolu stil zamanlamasını yok sayıyordu
            pv (ca, params::CaRelease, quantMs (std::clamp (stil.releaseMs, 30.0f, 300.0f)));
            pv (ca, params::CaLookahead, stil.caLookahead);
            pv (ca, params::CaMakeup, 0.0f);                       // seviye leveler'ın işi; çift kazanç yok
            pv (ca, params::CaMix, stil.caMix);
            pv (ca, params::CaDetector, std::round (stil.caDetector));
            pv (ca, params::CaScHpf, quantHz (std::max (60.0f, r.f0P5Hz > 20.0f ? 0.6f * r.f0P5Hz : 60.0f)));
            pv (ca, params::CaCharacter, stil.caKarakter >= 0.0f ? std::round (stil.caKarakter) : (punch ? 2.0f : 0.0f));
            Reason rs; rs.rule = ReasonRule::CompMicro; rs.det = Problem::MicroDynamics;
            rs.vals = { grTarget, ratio, thresh, micro->valueDb }; rs.target = params::CaThresh;
            ca.reasons.push_back (rs);
        }
        else if (vokalRol)
        {
            // M2 taban: rap vokal yoğunluğu stil gereğidir — comp HER ZAMAN müzikal tabanla açılır (sahip onayı).
            // GR hedefi stilden; crest yüksekse artar; rol tavanı guardrail olarak kalır.
            ca.enabled = true;
            const float grTarget = std::min (roleGrCeiling (in.role),
                                             stil.grA + std::max (0.0f, 0.4f * (r.crestMedianDb - 8.0f)));
            const float ratio = quantQ (stil.ratioA);
            const float crestPay = std::clamp (r.crestMedianDb, 4.0f, 10.0f);
            const float thresh = quantDb (ref + crestPay - grTarget * ratio / std::max (0.5f, ratio - 1.0f));
            const bool punch = stil.ratioA >= 4.0f;    // agresif kompresyon isteyen stiller (trap/agresif/rage/parti/psikedelik); Ati242/Uzi yumusak karakterde kalir
            pv (ca, params::CaOn, 1.0f);
            pv (ca, params::CaThresh, std::clamp (thresh, -60.0f, 0.0f));
            pv (ca, params::CaRatio, ratio);
            pv (ca, params::CaKnee, stil.caKnee >= 0.0f ? stil.caKnee : 4.0f);
            pv (ca, params::CaAttack, quantMs (std::clamp (stil.attackMs, 0.5f, 30.0f)));    // M3.4: stil atak (trap metal 3 ms / teknik rap 15 ms)
            pv (ca, params::CaRelease, quantMs (std::clamp (stil.releaseMs, 30.0f, 300.0f)));
            pv (ca, params::CaLookahead, stil.caLookahead);
            pv (ca, params::CaMakeup, 0.0f);
            pv (ca, params::CaMix, stil.caMix);
            pv (ca, params::CaDetector, std::round (stil.caDetector));
            pv (ca, params::CaScHpf, quantHz (std::max (60.0f, r.f0P5Hz > 20.0f ? 0.6f * r.f0P5Hz : 60.0f)));
            pv (ca, params::CaCharacter, stil.caKarakter >= 0.0f ? std::round (stil.caKarakter) : (punch ? 2.0f : 0.0f));
            Reason rs; rs.rule = ReasonRule::CompBaseline;
            rs.vals = { ratio, grTarget, r.crestMedianDb, (float) in.style }; rs.target = params::CaThresh;
            ca.reasons.push_back (rs);
        }
        else ca = disabledModule (state::modul::COMP_A, ReasonRule::NoEvidence, micro);
        out.modules.push_back (std::move (ca));
    }

    // ---------------- Comp B: seri ikinci kademe — yavaş "tutkal" (M2: her zaman; OverCompressed atlar) ----------------
    {
        ModulePlan cb; cb.moduleId = state::modul::COMP_B;
        const auto* over = det (Problem::OverCompressed);
        if (in.lockedFor (state::modul::COMP_B)) { Reason rs; rs.rule = ReasonRule::Locked; cb.reasons.push_back (rs); }
        else if (over != nullptr && over->actionable())
            cb = disabledModule (state::modul::COMP_B, ReasonRule::CompSkipOverCompressed, over);
        else if (masterRol)
        {
            // MASTER tutkal: 1.5:1, ~1.5 dB GR, yavaş RMS — karışımı yapıştırır, ezmez
            cb.enabled = true;
            const float ratio = 1.5f, gr = 1.5f;
            const float thresh = quantDb (ref + std::clamp (r.crestMedianDb, 4.0f, 10.0f) - gr * ratio / (ratio - 1.0f));
            pv (cb, params::CbOn, 1.0f);
            pv (cb, params::CbThresh, std::clamp (thresh, -60.0f, 0.0f));
            pv (cb, params::CbRatio, ratio);
            pv (cb, params::CbKnee, 10.0f);
            pv (cb, params::CbAttack, quantMs (30.0f));
            pv (cb, params::CbRelease, quantMs (300.0f));
            pv (cb, params::CbLookahead, 0.0f);
            pv (cb, params::CbMakeup, 0.0f);
            pv (cb, params::CbMix, 1.0f);
            pv (cb, params::CbDetector, 1.0f);
            pv (cb, params::CbScHpf, quantHz (60.0f));
            pv (cb, params::CbCharacter, 1.0f);
            Reason rs; rs.rule = ReasonRule::CompBGlue; rs.vals = { ratio, gr, 0.0f, 0.0f }; rs.target = params::CbThresh;
            cb.reasons.push_back (rs);
        }
        else if (vokalRol && stil.grB > 0.05f)
        {
            cb.enabled = true;
            const float gr = stil.grB;
            const float ratio = quantQ (std::clamp (stil.cbRatio, 1.1f, 20.0f));    // M3.5: reçete (eskiden tüm stillerde 2:1)
            const float thresh = quantDb (ref + 0.5f * std::clamp (r.crestMedianDb, 4.0f, 10.0f) - gr * ratio / (ratio - 1.0f));
            pv (cb, params::CbOn, 1.0f);
            pv (cb, params::CbThresh, std::clamp (thresh, -60.0f, 0.0f));
            pv (cb, params::CbRatio, ratio);
            pv (cb, params::CbKnee, stil.cbKnee);
            pv (cb, params::CbAttack, quantMs (std::clamp (stil.cbAttack, 0.5f, 100.0f)));
            pv (cb, params::CbRelease, quantMs (std::clamp (stil.cbRelease, 30.0f, 1000.0f)));
            pv (cb, params::CbLookahead, 0.0f);
            pv (cb, params::CbMakeup, 0.0f);
            pv (cb, params::CbMix, stil.cbMix);
            pv (cb, params::CbDetector, std::round (stil.cbDetector));
            pv (cb, params::CbScHpf, quantHz (std::max (60.0f, r.f0P5Hz > 20.0f ? 0.6f * r.f0P5Hz : 60.0f)));
            pv (cb, params::CbCharacter, std::round (stil.cbKarakter));
            Reason rs; rs.rule = ReasonRule::CompBGlue; rs.vals = { ratio, gr, 0.0f, 0.0f }; rs.target = params::CbThresh;
            cb.reasons.push_back (rs);
        }
        else cb = disabledModule (state::modul::COMP_B, vokalRol ? ReasonRule::StyleOff : ReasonRule::NoEvidence, nullptr);
        out.modules.push_back (std::move (cb));
    }

    // ---------------- De-esser ----------------
    {
        ModulePlan ds; ds.moduleId = state::modul::DEESS;
        const auto* d = det (Problem::Sibilance);
        if (in.lockedFor (state::modul::DEESS)) { Reason rs; rs.rule = ReasonRule::Locked; ds.reasons.push_back (rs); }
        else if (masterRol) ds = disabledModule (state::modul::DEESS, ReasonRule::StyleOff, nullptr);   // tam karışımda de-ess hihat'leri ezer
        else if (d != nullptr && d->actionable (0.25f, 0.4f) && d->count > 0)
        {
            ds.enabled = true;
            const float cap = (d->confidence >= 0.8f && d->severity >= 0.8f) ? 12.0f : 8.0f;
            const float range = quantDb (std::clamp ((2.0f + 6.0f * d->severity) * d->confidence * lowConfMul, 3.0f, cap));
            pv (ds, params::DsOn, 1.0f);
            pv (ds, params::DsMode, std::round (stil.dsMode));
            pv (ds, params::DsHz, quantHz (std::clamp (d->centerHz * stil.dsHzOran, 3000.0f, 10000.0f)));   // bant, merkezin altından başlar
            pv (ds, params::DsThresh, quantDb (ref - 12.0f));
            pv (ds, params::DsRange, range);
            pv (ds, params::DsLookahead, 1.0f);
            pv (ds, params::DsAttack, 1.0f);
            pv (ds, params::DsRelease, quantMs (stil.dsRelease));
            pv (ds, params::DsLink, 1.0f);
            pv (ds, params::DsPost, -4.0f);                        // son aşama göreli eşik (Feedback kayda göre kalibre eder)
            pv (ds, params::Ds2On, 1.0f);
            pv (ds, params::Ds2Range, quantDb (std::clamp (stil.ds2Range, 0.0f, 9.0f)));
            Reason rs; rs.rule = ReasonRule::DeEss; rs.det = Problem::Sibilance;
            rs.vals = { d->centerHz, range, r.sibilantPerMinute, d->valueDb }; rs.target = params::DsRange;
            ds.reasons.push_back (rs);
        }
        else if (vokalRol && (stil.dsTaban >= 0.0f ? stil.dsTaban >= 0.5f : (stil.dirtOn > 0.5f || stil.airHigh >= 0.25f || stil.satToneDb >= 1.5f)))
        {
            // M3.4 TABAN: bu stil de-esser'dan SONRA tizi kabartıyor (KİR / AIR high / parlak sat tonu) — kanıt
            // olmasa da "harfler patlamasın" (sahip) için hafif de-esser. Harmonik kapısı sesli harflere dokunmaz.
            ds.enabled = true;
            const float merkez = r.sibilantCenterHz > 3000.0f ? r.sibilantCenterHz : 6500.0f;
            pv (ds, params::DsOn, 1.0f);
            pv (ds, params::DsMode, std::round (stil.dsMode));
            const float tabanRange = stil.dsTaban >= 0.0f ? quantDb (std::clamp (stil.dsTaban, 0.5f, 8.0f)) : 3.0f;
            pv (ds, params::DsHz, quantHz (std::clamp (merkez * stil.dsHzOran, 3000.0f, 10000.0f)));
            pv (ds, params::DsThresh, quantDb (ref - 12.0f));
            pv (ds, params::DsRange, tabanRange);
            pv (ds, params::DsLookahead, 1.0f);
            pv (ds, params::DsAttack, 1.0f);
            pv (ds, params::DsRelease, quantMs (stil.dsRelease));
            pv (ds, params::DsLink, 1.0f);
            pv (ds, params::DsPost, -4.0f);                        // son aşama göreli eşik (Feedback kayda göre kalibre eder)
            pv (ds, params::Ds2On, 1.0f);
            pv (ds, params::Ds2Range, quantDb (std::clamp (stil.ds2Range, 0.0f, 9.0f)));
            Reason rs; rs.rule = ReasonRule::DeEssStyle;
            rs.vals = { merkez * stil.dsHzOran, tabanRange, 0.0f, 0.0f }; rs.target = params::DsRange;
            ds.reasons.push_back (rs);
        }
        else if (vokalRol)
        {
            // denetim M3.5 (eleştirmen): kanıt ve stil tabanı yoksa bile SON AŞAMA sigortası açık kalır — göreli eşik
            // yüzünden fazlalık yoksa hiçbir şey yapmaz, ama parlatıcı zincir s'yi öne çıkarırsa sahibin şartını korur.
            const float merkez = r.sibilantCenterHz > 3000.0f ? r.sibilantCenterHz : 6500.0f;
            ds.enabled = true;
            pv (ds, params::DsOn, 1.0f);
            pv (ds, params::DsMode, std::round (stil.dsMode));
            pv (ds, params::DsHz, quantHz (std::clamp (merkez * stil.dsHzOran, 3000.0f, 10000.0f)));
            pv (ds, params::DsThresh, quantDb (ref - 12.0f));
            pv (ds, params::DsRange, 0.0f);                        // birinci aşama şeffaf
            pv (ds, params::DsLookahead, 1.0f);
            pv (ds, params::DsAttack, 1.0f);
            pv (ds, params::DsRelease, quantMs (stil.dsRelease));
            pv (ds, params::DsLink, 1.0f);
            pv (ds, params::DsPost, -4.0f);
            pv (ds, params::Ds2On, 1.0f);
            pv (ds, params::Ds2Range, quantDb (std::clamp (stil.ds2Range, 0.0f, 9.0f)));
            Reason rs; rs.rule = ReasonRule::DeEssStyle;
            rs.vals = { merkez * stil.dsHzOran, 0.0f, stil.ds2Range, 0.0f }; rs.target = params::Ds2Range;
            ds.reasons.push_back (rs);
        }
        else ds = disabledModule (state::modul::DEESS, d != nullptr && d->confidence >= 0.5f ? ReasonRule::NoEvidence : ReasonRule::LowConfidence, d);
        out.modules.push_back (std::move (ds));
    }

    // ---------------- YUMUŞAT (M3.5): preset dinamik rezonans bastırma — reçete tabanı, sertlik kanıtı derinleştirir ----------------
    {
        ModulePlan m; m.moduleId = state::modul::YUMUSAT;
        float derinlik = stil.sooDepth;
        if (const auto* h = det (Problem::Harsh); h != nullptr && h->actionable() && derinlik > 0.01f)
            derinlik += 0.12f * h->severity * h->confidence;                  // sert kayıt: biraz daha
        if (const auto* du = det (Problem::Dull); du != nullptr && du->actionable())
            derinlik *= 0.7f;                                                 // donuk kayıt: parlaklığı daha da alma
        derinlik = std::clamp (derinlik, 0.0f, 1.0f);
        if (in.lockedFor (state::modul::YUMUSAT)) { Reason rs; rs.rule = ReasonRule::Locked; m.reasons.push_back (rs); }
        else if (vokalRol && derinlik > 0.01f)
        {
            const float lo = quantHz (std::clamp (stil.sooLo, 300.0f, 8000.0f));
            const float hi = quantHz (std::clamp (std::max (stil.sooHi, lo * 1.5f), 2000.0f, 20000.0f));
            m.enabled = true;
            pv (m, params::SooOn, 1.0f);
            pv (m, params::SooDepth, std::round (derinlik * 100.0f) / 100.0f);
            pv (m, params::SooSharp, std::clamp (stil.sooSharp, 0.0f, 1.0f));
            pv (m, params::SooLo, lo);
            pv (m, params::SooHi, hi);
            pv (m, params::SooSpeed, std::clamp (stil.sooSpeed, 0.0f, 1.0f));
            Reason rs; rs.rule = ReasonRule::SootheStyle; rs.vals = { derinlik, lo, hi, 0.0f }; rs.target = params::SooDepth;
            m.reasons.push_back (rs);
        }
        else m = disabledModule (state::modul::YUMUSAT, ReasonRule::StyleOff, nullptr);
        out.modules.push_back (std::move (m));
    }

    // ---------------- Saturator: stil karakteri (M2) ----------------
    {
        ModulePlan sm; sm.moduleId = state::modul::SAT;
        if (in.lockedFor (state::modul::SAT)) { Reason rs; rs.rule = ReasonRule::Locked; sm.reasons.push_back (rs); }
        else if (masterRol && stil.satDrive * 0.3f >= 0.5f)
        {
            // MASTER: çok hafif renk (stil sürüşünün %30'u, ≤2 dB)
            const float drive = quantDb (std::clamp (stil.satDrive * 0.3f, 0.5f, 2.0f));
            sm.enabled = true;
            pv (sm, params::SatOn, 1.0f);
            pv (sm, params::SatDrive, drive);
            pv (sm, params::SatTone, quantDb (stil.satToneDb * 0.5f));
            pv (sm, params::SatMix, 1.0f);
            pv (sm, params::SatChar, 0.0f);                        // master: tüp (şeffaf renk)
            Reason rs; rs.rule = ReasonRule::SatStyle; rs.vals = { drive, (float) in.style, 0.0f, 0.0f }; rs.target = params::SatDrive;
            sm.reasons.push_back (rs);
        }
        else if (vokalRol && stil.satDrive > 0.1f)
        {
            const auto* dull = det (Problem::Dull);
            const auto* harsh = det (Problem::Harsh);
            float drive = stil.satDrive;
            if (dull != nullptr && dull->actionable()) drive += 1.0f;              // karanlık kaynak → harmonikler parlatır
            if (harsh != nullptr && harsh->actionable() && harsh->severity > 0.5f) drive -= 1.5f;   // sert kaynakta geri dur
            drive = quantDb (std::clamp (drive, 0.5f, 12.0f));   // M3.4: SERT MOD bolgesi (8+) trap metal icin acildi (eski 8 kelepcesi M2 donemiydi)
            sm.enabled = true;
            pv (sm, params::SatOn, 1.0f);
            pv (sm, params::SatDrive, drive);
            pv (sm, params::SatTone, quantDb (stil.satToneDb));   // M3.3 KİMLİK: ana renk (koyu/parlak) — hep 0'dı, ölçülen eşitlenmenin büyük nedeni
            pv (sm, params::SatMix, std::clamp (stil.satMix, 0.0f, 1.0f));
            pv (sm, params::SatChar, std::clamp (std::round (stil.satKarakter), 0.0f, 2.0f));   // M3.5: Tüp / Bant / Kırık
            Reason rs; rs.rule = ReasonRule::SatStyle; rs.vals = { drive, (float) in.style, 0.0f, 0.0f }; rs.target = params::SatDrive;
            sm.reasons.push_back (rs);
        }
        else sm = disabledModule (state::modul::SAT, ReasonRule::StyleOff, nullptr);
        out.modules.push_back (std::move (sm));
    }

    // ---------------- Dirt: paralel kir veri yolu (M3.4 sanatçı kimliği) ----------------
    {
        ModulePlan m; m.moduleId = state::modul::DIRT;
        if (in.lockedFor (state::modul::DIRT)) { Reason rs; rs.rule = ReasonRule::Locked; m.reasons.push_back (rs); }
        else if (vokalRol && stil.dirtOn > 0.5f)
        {
            // gürültülü kayıtta kir veri yolu 4 dB geri (distorsiyon taban gürültüsünü de sürür); back vokal 3 dB geri
            float seviye = stil.dirtLevel;
            if (gurultuluKayit) seviye -= 4.0f;
            if (in.role == state::Role::Backing) seviye -= 3.0f;
            m.enabled = true;
            pv (m, params::DirtOn, 1.0f);
            pv (m, params::DirtMode, std::clamp (std::round (stil.dirtMode), 0.0f, 2.0f));
            pv (m, params::DirtDrive, quantDb (std::clamp (stil.dirtDrive, 0.0f, 36.0f)));
            pv (m, params::DirtLevel, quantDb (std::clamp (seviye, -35.0f, 0.0f)));          // −36 = kapalı (DSP); plan bilinçli olarak üretmez
            pv (m, params::DirtTone, quantHz (std::clamp (stil.dirtTone, 2000.0f, 12000.0f)));
            pv (m, params::DirtBits, std::clamp (std::round (stil.dirtBits), 4.0f, 16.0f));
            pv (m, params::DirtRate, std::clamp (std::round (stil.dirtRate), 1.0f, 8.0f));
            Reason rs; rs.rule = ReasonRule::DirtStyle; rs.vals = { std::round (stil.dirtMode), stil.dirtDrive, seviye, 0.0f }; rs.target = params::DirtLevel;
            m.reasons.push_back (rs);
        }
        else m = disabledModule (state::modul::DIRT, ReasonRule::StyleOff, nullptr);
        out.modules.push_back (std::move (m));
    }

    // ---------------- Tune: stil perde düzeltme (M2) ----------------
    {
        ModulePlan m; m.moduleId = state::modul::TUNE;
        if (in.lockedFor (state::modul::TUNE)) { Reason rs; rs.rule = ReasonRule::Locked; m.reasons.push_back (rs); }
        else if (vokalRol && stil.tuneStr > 0.01f && r.f0MedianHz > 60.0f && r.f0MedianHz < 500.0f && r.voicedSec > 2.0)
        {
            m.enabled = true;
            pv (m, params::TuneOn, 1.0f);
            pv (m, params::TuneStrength, stil.tuneStr);
            pv (m, params::TuneSpeed, quantMs (stil.tuneSpd));
            // M3.1 gam algilama: analiz güvenilir gam bulduysa tune ona kilitlenir (kromatik yanlis notaya
            // çekebilir); güven < 0.5 ise kromatik kalir (dürüst seçim).
            const bool gamVar = r.keyIndex >= 0 && r.keyConfidence >= 0.5f;
            pv (m, params::TuneKey, gamVar ? (float) r.keyIndex : 0.0f);
            pv (m, params::TuneScale, gamVar ? (r.keyMinor ? 2.0f : 1.0f) : 0.0f);
            Reason rs; rs.rule = ReasonRule::TuneStyle; rs.vals = { stil.tuneStr, stil.tuneSpd, r.f0MedianHz,
                                                                    gamVar ? (float) (1 + r.keyIndex + (r.keyMinor ? 12 : 0)) : 0.0f };
            rs.target = params::TuneStrength;
            m.reasons.push_back (rs);
        }
        else m = disabledModule (state::modul::TUNE, stil.tuneStr > 0.01f ? ReasonRule::LowConfidence : ReasonRule::StyleOff, nullptr);
        out.modules.push_back (std::move (m));
    }

    // ---------------- FORMANT (M3.5): preset ses rengi (perde aynı) ----------------
    {
        ModulePlan m; m.moduleId = state::modul::FORMANT;
        if (in.lockedFor (state::modul::FORMANT)) { Reason rs; rs.rule = ReasonRule::Locked; m.reasons.push_back (rs); }
        else if (vokalRol && std::fabs (stil.frmShift) >= 0.25f)
        {
            const float kay = std::round (std::clamp (stil.frmShift, -6.0f, 6.0f) * 10.0f) / 10.0f;
            const float mix = std::clamp (stil.frmMix, 0.0f, 1.0f);
            m.enabled = true;
            pv (m, params::FrmOn, 1.0f);
            pv (m, params::FrmShift, kay);
            pv (m, params::FrmMix, mix);
            Reason rs; rs.rule = ReasonRule::FormantStyle; rs.vals = { kay, mix, 0.0f, 0.0f }; rs.target = params::FrmShift;
            m.reasons.push_back (rs);
        }
        else m = disabledModule (state::modul::FORMANT, ReasonRule::StyleOff, nullptr);
        out.modules.push_back (std::move (m));
    }

    // ---------------- FX: doubler / delay / reverb / width (stil + rol tabanı, M2) ----------------
    {
        const bool back = in.role == state::Role::Backing, adlib = in.role == state::Role::Adlib;
        // Layer (M3.4: pitch-shift katman — Uzi oktav-altı/harmoni, rage +7/−12)
        {
            ModulePlan m; m.moduleId = state::modul::LAYER;
            if (in.lockedFor (state::modul::LAYER)) { Reason rs; rs.rule = ReasonRule::Locked; m.reasons.push_back (rs); }
            else if (vokalRol && stil.layerOn > 0.5f && (std::fabs (stil.layerA) >= 0.5f || std::fabs (stil.layerB) >= 0.5f))
            {
                float seviye = stil.layerLevel + (adlib ? 2.0f : 0.0f);            // adlib: katman daha önde
                m.enabled = true;
                pv (m, params::LayerOn, 1.0f);
                pv (m, params::LayerA, std::clamp (std::round (stil.layerA), -12.0f, 12.0f));
                pv (m, params::LayerB, std::clamp (std::round (stil.layerB), -12.0f, 12.0f));
                pv (m, params::LayerLevel, quantDb (std::clamp (seviye, -36.0f, 0.0f)));
                pv (m, params::LayerWidth, std::clamp (stil.layerWidth, 0.0f, 1.0f));
                pv (m, params::LayerTone, quantHz (std::clamp (stil.layerTone, 1500.0f, 12000.0f)));
                pv (m, params::LayerGrit, std::clamp (stil.layerGrit, 0.0f, 1.0f));
                Reason rs; rs.rule = ReasonRule::LayerStyle; rs.vals = { stil.layerA, stil.layerB, seviye, 0.0f }; rs.target = params::LayerLevel;
                m.reasons.push_back (rs);
            }
            else m = disabledModule (state::modul::LAYER, ReasonRule::StyleOff, nullptr);
            out.modules.push_back (std::move (m));
        }
        // Doubler
        {
            ModulePlan m; m.moduleId = state::modul::DOUBLER;
            const float mix = std::clamp (stil.dblMix + (back ? 0.15f : 0.0f), 0.0f, 0.6f);
            if (in.lockedFor (state::modul::DOUBLER)) { Reason rs; rs.rule = ReasonRule::Locked; m.reasons.push_back (rs); }
            else if (vokalRol && mix > 0.01f)
            {
                m.enabled = true;
                pv (m, params::DblOn, 1.0f);
                pv (m, params::DblMix, quantDb (mix * 10.0f) / 10.0f);
                pv (m, params::DblSpreadMs, quantMs (std::clamp (stil.dblSpreadMs + (back ? 6.0f : 0.0f), 5.0f, 40.0f)));
                pv (m, params::DblDetune, quantDb (stil.dblDetune));
                pv (m, params::DblHpHz, quantHz (std::clamp (stil.dblHpHz + (back ? 40.0f : 0.0f), 60.0f, 600.0f)));
                pv (m, params::DblLpHz, quantHz (stil.dblLpHz));    // M3.4: rage double duvarı lead'den koyu (araştırma)
                pv (m, params::DblVoices, (back || stil.dblVoices > 0.5f) ? 1.0f : 0.0f);
                Reason rs; rs.rule = ReasonRule::FxStyle; rs.vals = { mix, (float) in.style, 0.0f, 0.0f }; rs.target = params::DblMix;
                m.reasons.push_back (rs);
            }
            else m = disabledModule (state::modul::DOUBLER, ReasonRule::StyleOff, nullptr);
            out.modules.push_back (std::move (m));
        }
        // Delay
        {
            ModulePlan m; m.moduleId = state::modul::DELAY;
            const float mix = std::clamp (stil.dlyMix * (back ? 0.5f : adlib ? 1.6f : 1.0f), 0.0f, 0.5f);
            if (in.lockedFor (state::modul::DELAY)) { Reason rs; rs.rule = ReasonRule::Locked; m.reasons.push_back (rs); }
            else if (vokalRol && mix > 0.01f)
            {
                m.enabled = true;
                pv (m, params::DlyOn, 1.0f);
                pv (m, params::DlyMix, quantDb (mix * 10.0f) / 10.0f);
                pv (m, params::DlySync, std::round (adlib ? stil.dlyAdlibSync : stil.dlySync));   // adlib: atma (varsayılan 1/4); diğerleri stil
                pv (m, params::DlyTimeMs, quantMs (stil.dlyTimeMs));
                pv (m, params::DlyFeedback, adlib ? std::max (0.35f, stil.dlyFb) : stil.dlyFb);
                pv (m, params::DlyHpHz, quantHz (stil.dlyHpHz));
                pv (m, params::DlyLpHz, quantHz (stil.dlyLpHz));
                pv (m, params::DlyDuck, stil.dlyDuck);
                pv (m, params::DlyWidth, std::clamp (stil.dlyWidth, 0.0f, 1.0f));
                Reason rs; rs.rule = ReasonRule::FxStyle; rs.vals = { mix, (float) in.style, 0.0f, 0.0f }; rs.target = params::DlyMix;
                m.reasons.push_back (rs);
            }
            else m = disabledModule (state::modul::DELAY, ReasonRule::StyleOff, nullptr);
            out.modules.push_back (std::move (m));
        }
        // Reverb
        {
            ModulePlan m; m.moduleId = state::modul::REVERB;
            const float mix = std::clamp (stil.revMix + (back ? 0.05f : 0.0f) + (adlib ? 0.04f : 0.0f), 0.0f, 0.4f);
            if (in.lockedFor (state::modul::REVERB)) { Reason rs; rs.rule = ReasonRule::Locked; m.reasons.push_back (rs); }
            else if (vokalRol && mix > 0.01f)
            {
                m.enabled = true;
                pv (m, params::RevOn, 1.0f);
                pv (m, params::RevMix, quantDb (mix * 10.0f) / 10.0f);
                pv (m, params::RevPre, quantMs (stil.revPre));
                pv (m, params::RevDecay, stil.revDecay);
                pv (m, params::RevDamp, quantHz (stil.revDampHz));
                pv (m, params::RevHp, quantHz (stil.revHpHz));
                pv (m, params::RevWidth, std::clamp (stil.revWidth, 0.0f, 1.0f));
                pv (m, params::RevDuck, stil.revDuck);
                pv (m, params::RevType, std::clamp (std::round (stil.revTip), 0.0f, 3.0f));
                Reason rs; rs.rule = ReasonRule::FxStyle; rs.vals = { mix, (float) in.style, 0.0f, 0.0f }; rs.target = params::RevMix;
                m.reasons.push_back (rs);
            }
            else m = disabledModule (state::modul::REVERB, ReasonRule::StyleOff, nullptr);
            out.modules.push_back (std::move (m));
        }
        // Glow (M3.2: OTT tarzı 3 bant yukarı+aşağı — "anında iyi ses"; araştırma dozları.
        // Aşırı gürültülü kayıtta derinlik kısılır: upward nefes/oda pompalaması riski.)
        {
            ModulePlan m; m.moduleId = state::modul::GLOW;
            float derinlik = stil.glowDepth;
            if (gurultuluKayit) derinlik *= 0.6f;
            if (in.lockedFor (state::modul::GLOW)) { Reason rs; rs.rule = ReasonRule::Locked; m.reasons.push_back (rs); }
            else if (vokalRol && derinlik > 0.01f)
            {
                m.enabled = true;
                pv (m, params::GlowOn, 1.0f);
                pv (m, params::GlowDepth, quantDb (derinlik * 10.0f) / 10.0f);
                pv (m, params::GlowTime, quantQ (stil.glowTime));
                Reason rs; rs.rule = ReasonRule::FxStyle; rs.vals = { derinlik, stil.glowTime, 0.0f, 0.0f }; rs.target = params::GlowDepth;
                m.reasons.push_back (rs);
            }
            else m = disabledModule (state::modul::GLOW, ReasonRule::StyleOff, nullptr);
            out.modules.push_back (std::move (m));
        }
        // Air (M3: Fresh Air tarzı program-bağımlı hava — stil tabanı; sibilans/parlaklık kanıtı kısar)
        {
            ModulePlan m; m.moduleId = state::modul::AIR;
            float mid = stil.airMid, high = stil.airHigh;
            const auto* sib = det (Problem::Sibilance);
            const auto* br = det (Problem::Bright);
            if (sib != nullptr && sib->actionable()) high *= 0.6f;                 // ess'leri azdırma
            if (br != nullptr && br->actionable()) { mid *= 0.6f; high *= 0.5f; }  // zaten parlak kayıt
            if (in.lockedFor (state::modul::AIR)) { Reason rs; rs.rule = ReasonRule::Locked; m.reasons.push_back (rs); }
            else if (vokalRol && (mid > 0.01f || high > 0.01f))
            {
                m.enabled = true;
                pv (m, params::AirOn, 1.0f);
                pv (m, params::AirMid, quantDb (mid * 10.0f) / 10.0f);
                pv (m, params::AirHigh, quantDb (high * 10.0f) / 10.0f);
                Reason rs; rs.rule = ReasonRule::FxStyle; rs.vals = { mid, high, 0.0f, 0.0f }; rs.target = params::AirMid;
                m.reasons.push_back (rs);
            }
            else m = disabledModule (state::modul::AIR, ReasonRule::StyleOff, nullptr);
            out.modules.push_back (std::move (m));
        }
        // Width
        {
            ModulePlan m; m.moduleId = state::modul::WIDTH;
            if (in.lockedFor (state::modul::WIDTH)) { Reason rs; rs.rule = ReasonRule::Locked; m.reasons.push_back (rs); }
            else if (back || adlib || masterRol || stil.leadWidth > 1.01f)
            {
                m.enabled = true;
                const float amt = masterRol ? 1.08f : back ? 1.25f : adlib ? 1.15f
                                : stil.leadWidth;                                  // M3.3 KİMLİK: Motive/Reçkol lead'de de geniş
                pv (m, params::WidOn, 1.0f);
                pv (m, params::WidAmount, amt);
                Reason rs; rs.rule = ReasonRule::FxStyle; rs.vals = { amt, 0.0f, 0.0f, 0.0f }; rs.target = params::WidAmount;
                m.reasons.push_back (rs);
            }
            else m = disabledModule (state::modul::WIDTH, ReasonRule::StyleOff, nullptr);   // lead: mono bütünlüğü
            out.modules.push_back (std::move (m));
        }
    }

    // ---------------- Output: güvenlik limiter'ı (her zaman) ----------------
    {
        ModulePlan o; o.moduleId = state::modul::OUTPUT; o.enabled = true;
        if (in.lockedFor (state::modul::OUTPUT)) { o = ModulePlan {}; o.moduleId = state::modul::OUTPUT; Reason rs; rs.rule = ReasonRule::Locked; o.reasons.push_back (rs); }
        else
        {
            pv (o, params::OutCeiling, -1.0f);
            pv (o, params::OutRelease, quantMs (120.0f));
            Reason rs; rs.rule = ReasonRule::LimiterSafety; rs.vals = { -1.0f, 0.0f, 0.0f, 0.0f }; rs.target = params::OutCeiling;
            o.reasons.push_back (rs);
            // ticari vokal seviyesi (M2.1: "cılız bırakma"): stil çıkış trimi; limiter tepeleri güvenle yakalar
            if (vokalRol)
            {
                const float trim = quantDb (stil.outTrimDb);
                pv (o, params::OutputTrim, trim);
                Reason rs2; rs2.rule = ReasonRule::OutputLevel; rs2.vals = { trim, 0.0f, 0.0f, 0.0f }; rs2.target = params::OutputTrim;
                o.reasons.push_back (rs2);
            }
            else if (masterRol)
            {
                // MASTER: stil hedef yayın seviyesi (dahili ref −18'den hedefe; limiter yoğunluğu mastering'dir)
                const float hedef = stil.masterLufs;
                const float trim = quantDb (std::clamp (hedef - INTERNAL_REF_LUFS, -6.0f, 12.0f));
                pv (o, params::OutputTrim, trim);
                Reason rs2; rs2.rule = ReasonRule::MasterLoudness; rs2.vals = { hedef, trim, 0.0f, 0.0f }; rs2.target = params::OutputTrim;
                o.reasons.push_back (rs2);
            }
        }
        out.modules.push_back (std::move (o));
    }

    // ---------------- özet satırları (≤4) ----------------
    {
        int k = 0;
        auto say = [&] (SummaryLine s) { if (k < 4) out.summary[(size_t) k++] = s; };
        const auto* clip = det (Problem::Clipping);
        if (clip != nullptr && clip->actionable()) say (SummaryLine::ClippingWarning);
        const auto* nf = det (Problem::NoiseFloor);
        if (nf != nullptr && r.noiseFloorKnown && nf->probability > 0.7f && nf->severity > 0.5f) say (SummaryLine::NoiseWarning);
        if (std::fabs (out.inputTrimDb) >= 0.5f) say (SummaryLine::TrimSet);
        const auto* eqm = out.module (state::modul::EQ);
        if (eqm != nullptr && eqm->enabled)
        {
            bool hpf = false, mud = false, res = false;
            for (const auto& rs : eqm->reasons)
            {
                hpf = hpf || rs.rule == ReasonRule::HpfLfNoise || rs.rule == ReasonRule::HpfPlosive || rs.rule == ReasonRule::HpfProximity;
                mud = mud || rs.rule == ReasonRule::CutMud || rs.rule == ReasonRule::CutBox || rs.rule == ReasonRule::CutBoom;
                res = res || rs.rule == ReasonRule::CutResonance;
            }
            if (hpf) say (SummaryLine::HpfApplied);
            if (mud) say (SummaryLine::MudCut);
            if (res) say (SummaryLine::ResonanceCut);
        }
        const auto* dsm = out.module (state::modul::DEESS);
        if (dsm != nullptr && dsm->enabled) say (SummaryLine::DeEssApplied);
        const auto* lv = out.module (state::modul::LEVELER);
        if (lv != nullptr && lv->enabled) say (SummaryLine::Leveled);
        const auto* cam = out.module (state::modul::COMP_A);
        if (cam != nullptr && cam->enabled) say (SummaryLine::Compressed);
        if (k == 0) say (r.confidence.enough() ? SummaryLine::NothingNeeded : SummaryLine::LowConfidenceOverall);
    }
    return out;
}

} // namespace sweax::decision
