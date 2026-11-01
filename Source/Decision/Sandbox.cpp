// SWEAX NEXT — Decision/Sandbox.cpp
#include "Sandbox.h"
#include <cstring>

namespace sweax::decision
{

SandboxParams Sandbox::paramsFromPlan (const MixPlan& plan)
{
    using namespace params;
    SandboxParams sp;
    sp.inTrimDb = plan.inputTrimDb;
    sp.sira = siraGecerli (plan.sira) ? plan.sira : varsayilanSira();
    for (const auto& m : plan.modules)
    {
        const std::string_view id (m.moduleId);
        const bool en = m.enabled;
        if (id == state::modul::EQ) sp.eqOn = en;
        else if (id == state::modul::LEVELER) sp.levOn = en;
        else if (id == state::modul::COMP_A) sp.caOn = en;
        else if (id == state::modul::COMP_B) sp.cbOn = en;
        else if (id == state::modul::DEESS) sp.dsOn = en;
        else if (id == state::modul::GATE) sp.gateOn = en;
        else if (id == state::modul::SAT) sp.satOn = en;
        else if (id == state::modul::DOUBLER) sp.dblOn = en;
        else if (id == state::modul::DELAY) sp.dlyOn = en;
        else if (id == state::modul::REVERB) sp.revOn = en;
        else if (id == state::modul::WIDTH) sp.widOn = en;
        else if (id == state::modul::TUNE) sp.tuneOn = en;
        else if (id == state::modul::AIR) sp.airOn = en;    // denetim M3.2: AIR sandboxta yoktu — feedback ve render AIR'i duymuyordu
        else if (id == state::modul::GLOW) sp.glowOn = en;
        else if (id == state::modul::DIRT) sp.dirtOn = en;
        else if (id == state::modul::LAYER) sp.layerOn = en;
        else if (id == state::modul::TONEQ) sp.teqOn = en;
        else if (id == state::modul::YUMUSAT) sp.sooOn = en;
        else if (id == state::modul::FORMANT) sp.frmOn = en;
        for (const auto& p : m.params)
        {
            const int i = (int) p.id; const float v = p.raw;
            if (p.id == InputTrim) sp.inTrimDb = v;
            else if (p.id == OutputTrim) sp.outTrimDb = v;
            else if (p.id == EqOn) sp.eq.on = v > 0.5f;
            else if (p.id == EqHpfOn) sp.eq.hpfOn = v > 0.5f;
            else if (p.id == EqHpfHz) sp.eq.hpfHz = v;
            else if (p.id == EqHpfSlope) sp.eq.hpfSlope = (int) (v + 0.5f);
            else if (p.id == EqPlosOn) sp.eq.plosOn = v > 0.5f;
            else if (p.id == EqPlosHz) sp.eq.plosHz = v;
            else if (p.id == EqPlosRange) sp.eq.plosRangeDb = v;
            else if (i >= (int) EqB1On && i <= (int) eqBant (EQ_BANT_SAYISI - 1, EQ_BANT_PARAM - 1))
            {
                const int rel = i - (int) EqB1On, b = rel / EQ_BANT_PARAM, alan = rel % EQ_BANT_PARAM;
                auto& bb = sp.eq.band[(size_t) b];
                switch (alan)
                {
                    case 0: bb.on = v > 0.5f; break;      case 1: bb.type = (int) (v + 0.5f); break;
                    case 2: bb.hz = v; break;             case 3: bb.gainDb = v; break;
                    case 4: bb.q = v; break;              case 5: bb.dyn = v > 0.5f; break;
                    case 6: bb.dynThreshDb = v; break;    case 7: bb.dynRangeDb = v; break;
                    case 8: bb.dynAttackMs = v; break;    default: bb.dynReleaseMs = v; break;
                }
            }
            else if (p.id == TeqOn) sp.teq.on = v > 0.5f;
            else if (i >= (int) TeqB1On && i <= (int) teqBant (TEQ_BANT_SAYISI - 1, TEQ_BANT_PARAM - 1))
            {
                const int rel = i - (int) TeqB1On, b = rel / TEQ_BANT_PARAM, alan = rel % TEQ_BANT_PARAM;
                auto& bb = sp.teq.band[(size_t) b];
                switch (alan)
                {
                    case 0: bb.on = v > 0.5f; break;      case 1: bb.type = (int) (v + 0.5f); break;
                    case 2: bb.hz = v; break;             case 3: bb.gainDb = v; break;
                    default: bb.q = v; break;
                }
            }
            else if (p.id == LevOn) sp.lev.on = v > 0.5f;
            else if (p.id == LevTarget) sp.lev.targetLufs = v;
            else if (p.id == LevMaxUp) sp.lev.maxUpDb = v;
            else if (p.id == LevMaxDown) sp.lev.maxDownDb = v;
            else if (p.id == LevSpeed) sp.lev.speed = v;
            else if (p.id == DsOn) sp.ds.on = v > 0.5f;
            else if (p.id == DsMode) sp.ds.mode = (int) (v + 0.5f);
            else if (p.id == DsHz) sp.ds.hz = v;
            else if (p.id == DsThresh) sp.ds.threshDb = v;
            else if (p.id == DsRange) sp.ds.rangeDb = v;
            else if (p.id == DsLookahead) sp.ds.lookaheadMs = v;
            else if (p.id == DsAttack) sp.ds.attackMs = v;
            else if (p.id == DsRelease) sp.ds.releaseMs = v;
            else if (p.id == DsLink) sp.ds.link = v > 0.5f;
            else if (p.id == DsPost) sp.dsPostDb = v;
            else if (p.id == Ds2On) sp.ds2On = v > 0.5f;
            else if (p.id == Ds2Range) sp.ds2RangeDb = v;
            else if (p.id == OutCeiling) sp.out.ceilingDbTp = v;
            else if (p.id == OutRelease) sp.out.releaseMs = v;
            else if (p.id == GateOn) sp.gate.on = v > 0.5f;
            else if (p.id == GateThresh) sp.gate.threshDb = v;
            else if (p.id == GateRange) sp.gate.rangeDb = v;
            else if (p.id == GateAttack) sp.gate.attackMs = v;
            else if (p.id == GateRelease) sp.gate.releaseMs = v;
            else if (p.id == GateHold) sp.gate.holdMs = v;
            else if (p.id == GateHyst) sp.gate.hystDb = v;
            else if (p.id == SatOn) sp.sat.on = v > 0.5f;
            else if (p.id == SatDrive) sp.sat.driveDb = v;
            else if (p.id == SatTone) sp.sat.toneDb = v;
            else if (p.id == SatMix) sp.sat.mix = v;
            else if (p.id == DblOn) sp.dbl.on = v > 0.5f;
            else if (p.id == DblMix) sp.dbl.mix = v;
            else if (p.id == DblSpreadMs) sp.dbl.spreadMs = v;
            else if (p.id == DblDetune) sp.dbl.detuneCents = v;
            else if (p.id == DblHpHz) sp.dbl.hpHz = v;
            else if (p.id == DblLpHz) sp.dbl.lpHz = v;
            else if (p.id == DblVoices) sp.dbl.voices = (int) (v + 0.5f);
            else if (p.id == DlyOn) sp.dly.on = v > 0.5f;
            else if (p.id == DlyMix) sp.dly.mix = v;
            else if (p.id == DlySync) sp.dly.sync = (int) (v + 0.5f);
            else if (p.id == DlyTimeMs) sp.dly.timeMs = v;
            else if (p.id == DlyFeedback) sp.dly.feedback = v;
            else if (p.id == DlyHpHz) sp.dly.hpHz = v;
            else if (p.id == DlyLpHz) sp.dly.lpHz = v;
            else if (p.id == DlyDuck) sp.dly.duckDb = v;
            else if (p.id == RevOn) sp.rev.on = v > 0.5f;
            else if (p.id == RevMix) sp.rev.mix = v;
            else if (p.id == RevPre) sp.rev.preDelayMs = v;
            else if (p.id == RevDecay) sp.rev.decaySec = v;
            else if (p.id == RevDamp) sp.rev.dampHz = v;
            else if (p.id == RevHp) sp.rev.hpHz = v;
            else if (p.id == RevWidth) sp.rev.width = v;
            else if (p.id == RevDuck) sp.rev.duckDb = v;
            else if (p.id == WidOn) sp.wid.on = v > 0.5f;
            else if (p.id == WidAmount) sp.wid.amount = v;
            else if (p.id == TuneOn) sp.tune.on = v > 0.5f;
            else if (p.id == TuneStrength) sp.tune.strength = v;
            else if (p.id == TuneSpeed) sp.tune.speedMs = v;
            else if (p.id == TuneKey) sp.tune.key = (int) (v + 0.5f);
            else if (p.id == TuneScale) sp.tune.scale = (int) (v + 0.5f);
            else if (p.id == AirOn) sp.air.on = v > 0.5f;
            else if (p.id == AirMid) sp.air.mid = v;
            else if (p.id == AirHigh) sp.air.high = v;
            else if (p.id == GlowOn) sp.glowp.on = v > 0.5f;
            else if (p.id == GlowDepth) sp.glowp.depth = v;
            else if (p.id == GlowTime) sp.glowp.timeMul = v;
            else if (p.id == DirtOn) sp.dirtp.on = v > 0.5f;
            else if (p.id == DirtMode) sp.dirtp.mode = (int) (v + 0.5f);
            else if (p.id == DirtDrive) sp.dirtp.driveDb = v;
            else if (p.id == DirtLevel) sp.dirtp.levelDb = v;
            else if (p.id == DirtTone) sp.dirtp.toneHz = v;
            else if (p.id == DirtBits) sp.dirtp.bits = v;
            else if (p.id == DirtRate) sp.dirtp.rate = v;
            else if (p.id == LayerOn) sp.layerp.on = v > 0.5f;
            else if (p.id == LayerA) sp.layerp.semiA = v;
            else if (p.id == LayerB) sp.layerp.semiB = v;
            else if (p.id == LayerLevel) sp.layerp.levelDb = v;
            else if (p.id == LayerWidth) sp.layerp.width = v;
            else if (p.id == LayerTone) sp.layerp.toneHz = v;
            else if (p.id == LayerGrit) sp.layerp.grit = v;
            else if (p.id == SooOn) sp.soop.on = v > 0.5f;
            else if (p.id == SooDepth) sp.soop.depth = v;
            else if (p.id == SooSharp) sp.soop.sharp = v;
            else if (p.id == SooLo) sp.soop.loHz = v;
            else if (p.id == SooHi) sp.soop.hiHz = v;
            else if (p.id == SooSpeed) sp.soop.speed = v;
            else if (p.id == FrmOn) sp.frmp.on = v > 0.5f;
            else if (p.id == FrmShift) sp.frmp.shiftSt = v;
            else if (p.id == FrmMix) sp.frmp.mix = v;
            else if (p.id == SatChar) sp.sat.character = (int) (v + 0.5f);
            else if (p.id == RevType) sp.rev.type = (int) (v + 0.5f);
            else if (p.id == DlyWidth) sp.dly.width = v;
            else
            {
                auto comp = [&] (CompParams& c, Id base)
                {
                    const int rel2 = i - (int) base;
                    switch (rel2)
                    {
                        case 0: c.on = v > 0.5f; break;   case 1: c.threshDb = v; break;  case 2: c.ratio = v; break;
                        case 3: c.kneeDb = v; break;      case 4: c.attackMs = v; break;  case 5: c.releaseMs = v; break;
                        case 6: c.lookaheadMs = v; break; case 7: c.makeupDb = v; break;  case 8: c.mix = v; break;
                        case 9: c.detector = (int) (v + 0.5f); break; case 10: c.scHpfHz = v; break; default: c.character = (int) (v + 0.5f); break;
                    }
                };
                if (i >= (int) CaOn && i <= (int) CaCharacter) comp (sp.ca, CaOn);
                else if (i >= (int) CbOn && i <= (int) CbCharacter) comp (sp.cb, CbOn);
            }
        }
    }
    sp.eq.on = sp.eq.on || sp.eqOn; sp.lev.on = sp.lev.on || sp.levOn; sp.ca.on = sp.ca.on || sp.caOn;
    sp.cb.on = sp.cb.on || sp.cbOn; sp.ds.on = sp.ds.on || sp.dsOn;
    sp.gate.on = sp.gate.on || sp.gateOn; sp.sat.on = sp.sat.on || sp.satOn;
    sp.dbl.on = sp.dbl.on || sp.dblOn; sp.dly.on = sp.dly.on || sp.dlyOn;
    sp.rev.on = sp.rev.on || sp.revOn; sp.wid.on = sp.wid.on || sp.widOn;
    sp.tune.on = sp.tune.on || sp.tuneOn;
    sp.air.on = sp.air.on || sp.airOn; sp.glowp.on = sp.glowp.on || sp.glowOn;
    sp.dirtp.on = sp.dirtp.on || sp.dirtOn; sp.layerp.on = sp.layerp.on || sp.layerOn;
    sp.teq.on = sp.teq.on || sp.teqOn;
    sp.soop.on = sp.soop.on || sp.sooOn; sp.frmp.on = sp.frmp.on || sp.frmOn;
    return sp;
}

namespace
{
    // M3.5: sandbox zinciri TEK yerde (üç render yolu ve iki gecikme sorgusu aynı modül kümesini kuruyordu — yeni
    // modül eklemek beş yeri düzenlemek demekti). tam = TUNE/KATMAN/FX dahil (stereo/dinleme); değilse düzeltici mono.
    struct Zincir
    {
        NoiseGate gate; PitchCorrector tn; CorrectiveEQ eq, teq; VocalLeveler lev; Compressor ca, cb; DeEsser ds, ds2; Saturator sat;
        AirExciter air; GlowModule glow; DirtModule dirt; LayerModule layer; YumusatModule soo; FormantModule frm;
        Doubler dbl; StereoDelay dly; PlateReverb rev; WidthModule wid; OutputStage o;
        bool tam { false };

        void hazirla (double sr, int blk, int kanal, Quality q, bool tamZincir)
        {
            tam = tamZincir;
            gate.prepare (sr, blk, kanal); eq.prepare (sr, blk, kanal); teq.prepare (sr, blk, kanal, false); lev.prepare (sr, blk, kanal);
            ca.prepare (sr, blk, kanal); cb.prepare (sr, blk, kanal); ds.prepare (sr, blk, kanal); ds2.prepare (sr, blk, kanal);
            sat.prepare (sr, blk, kanal); air.prepare (sr, blk, kanal); glow.prepare (sr, blk, kanal); dirt.prepare (sr, blk, kanal);
            soo.prepare (sr, blk, kanal); frm.prepare (sr, blk, kanal);
            o.prepare (sr, blk, kanal);
            if (tam)
            {
                tn.prepare (sr, blk, kanal); layer.prepare (sr, blk, kanal);
                dbl.prepare (sr, blk, kanal); dly.prepare (sr, blk, kanal); rev.prepare (sr, blk, kanal); wid.prepare (sr, blk, kanal);
            }
            // denetim M3.4: limiter dahil tüm lookahead'li modüller ölçülen gecikmeyle AYNI modda çalışmalı
            gate.setQuality (q); eq.setQuality (q); teq.setQuality (q); lev.setQuality (q); ca.setQuality (q); cb.setQuality (q);
            ds.setQuality (q); ds2.setQuality (q); soo.setQuality (q); frm.setQuality (q); o.setQuality (q);
        }
        int gecikme (Quality q) const
        {
            int L = gate.latencySamples (q) + eq.latencySamples (q) + teq.latencySamples (q) + lev.latencySamples (q) + ca.latencySamples (q)
                  + ds.latencySamples (q) + ds2.latencySamples (q) + cb.latencySamples (q) + glow.latencySamples (q) + sat.latencySamples (q)
                  + dirt.latencySamples (q) + air.latencySamples (q) + soo.latencySamples (q) + frm.latencySamples (q)
                  + o.latencySamples (q);
            if (tam)
                L += tn.latencySamples (q) + layer.latencySamples (q) + dbl.latencySamples (q) + dly.latencySamples (q)
                   + rev.latencySamples (q) + wid.latencySamples (q);
            return L;
        }
        void kur (const SandboxParams& sp)
        {
            gate.setEnabled (sp.gateOn, true); eq.setEnabled (sp.eqOn, true); teq.setEnabled (sp.teqOn, true); lev.setEnabled (sp.levOn, true);
            ca.setEnabled (sp.caOn, true); cb.setEnabled (sp.cbOn, true); ds.setEnabled (sp.dsOn, true); ds2.setEnabled (sp.ds2On, true);
            sat.setEnabled (sp.satOn, true); air.setEnabled (sp.airOn, true); glow.setEnabled (sp.glowOn, true); dirt.setEnabled (sp.dirtOn, true);
            o.setEnabled (true, true);
            gate.setParams (sp.gate); eq.setParams (sp.eq); teq.setParams (sp.teq); lev.setParams (sp.lev); ca.setParams (sp.ca); cb.setParams (sp.cb);
            ds.setParams (sp.ds);
            { auto p2 = sp.ds; p2.on = sp.ds2On; p2.rangeDb = std::min (sp.ds2RangeDb, 9.0f); p2.goreli = true; p2.goreliEsikDb = sp.dsPostDb; ds2.setParams (p2); }
            sat.setParams (sp.sat); air.setParams (sp.air); glow.setParams (sp.glowp); dirt.setParams (sp.dirtp); o.setParams (sp.out);
            soo.setEnabled (sp.sooOn, true); soo.setParams (sp.soop);
            frm.setEnabled (sp.frmOn, true); frm.setParams (sp.frmp);
            if (tam)
            {
                tn.setEnabled (sp.tuneOn, true); tn.setParams (sp.tune);
                layer.setEnabled (sp.layerOn, true); layer.setParams (sp.layerp);
                dbl.setEnabled (sp.dblOn, true); dbl.setParams (sp.dbl);
                dly.setEnabled (sp.dlyOn, true); dly.setParams (sp.dly);
                rev.setEnabled (sp.revOn, true); rev.setParams (sp.rev);
                wid.setEnabled (sp.widOn, true); wid.setParams (sp.wid);
            }
        }
        void isle (const Sira& sira, float* const* ch, int kanal, int m, float gOut)
        {
            gate.process (ch, kanal, m);
            for (const auto e : sira)                                   // preset sırası (M3.5); mono düzeltici render TUNE/KATMAN içermez
            {
                switch ((Ek) e)
                {
                    case Ek::Tune:  if (tam) tn.process (ch, kanal, m); break;
                    case Ek::Eq:    eq.process (ch, kanal, m); break;
                    case Ek::Lev:   lev.process (ch, kanal, m); break;
                    case Ek::CompA: ca.process (ch, kanal, m); break;
                    case Ek::Ds:    ds.process (ch, kanal, m); break;
                    case Ek::CompB: cb.process (ch, kanal, m); break;
                    case Ek::Glow:  glow.process (ch, kanal, m); break;
                    case Ek::Sat:   sat.process (ch, kanal, m); break;
                    case Ek::Dirt:  dirt.process (ch, kanal, m); break;
                    case Ek::Air:   air.process (ch, kanal, m); break;
                    case Ek::Ds2:   ds2.process (ch, kanal, m); break;
                    case Ek::Layer: if (tam) layer.process (ch, kanal, m); break;
                    case Ek::TonEq: teq.process (ch, kanal, m); break;
                    case Ek::Yumusat: soo.process (ch, kanal, m); break;
                    case Ek::Formant: frm.process (ch, kanal, m); break;
                }
            }
            if (tam)
            {
                dbl.process (ch, kanal, m);
                dly.process (ch, kanal, m);
                rev.process (ch, kanal, m);
                wid.process (ch, kanal, m);
            }
            for (int k = 0; k < kanal; ++k) for (int i = 0; i < m; ++i) ch[k][i] *= gOut;
            o.process (ch, kanal, m);
        }
    };
}

int Sandbox::monoLatencySamples (double sr, Quality q)
{
    auto z = std::make_unique<Zincir>();
    z->hazirla (sr, 512, 1, q, false);
    return z->gecikme (q);
}

int Sandbox::latencySamples (double sr, Quality q)
{
    auto z = std::make_unique<Zincir>();
    z->hazirla (sr, 512, 2, q, true);
    return z->gecikme (q);
}

void Sandbox::render (const MixPlan& plan, const float* in, float* out, int64_t n, double sr, Quality q, bool gecikmeTelafi)
{
    juce::ScopedNoDenormals noDenormals;               // reverb/comp kuyrukları denormal sürünmesin (hız)
    const auto sp = paramsFromPlan (plan);
    constexpr int BLK = 512;
    auto z = std::make_unique<Zincir>();
    z->hazirla (sr, BLK, 1, q, false);
    z->kur (sp);
    const float g = std::pow (10.0f, sp.inTrimDb * 0.05f);
    const float gOut = std::pow (10.0f, sp.outTrimDb * 0.05f);
    // denetim M3.4 (T6): gecikme telafisi — toplam n+L örnek işlenir, ilk L çıkış atılır
    const int64_t L = gecikmeTelafi ? (int64_t) z->gecikme (q) : 0;
    float blok[BLK];
    for (int64_t i = 0; i < n + L; i += BLK)
    {
        const int m = (int) std::min<int64_t> (BLK, n + L - i);
        for (int k = 0; k < m; ++k) blok[k] = (i + k < n ? in[i + k] : 0.0f) * g;
        float* ch[1] = { blok };
        z->isle (sp.sira, ch, 1, m, gOut);
        for (int k = 0; k < m; ++k)
        {
            const int64_t j = i + k - L;
            if (j >= 0 && j < n) out[j] = blok[k];
        }
    }
}

void Sandbox::renderStereo (const MixPlan& plan, const float* in, float* outL, float* outR, int64_t n, double sr,
                            Quality q, double bpm, bool gecikmeTelafi, int kanal)
{
    kanal = std::clamp (kanal, 1, 2);
    juce::ScopedNoDenormals noDenormals;
    auto sp = paramsFromPlan (plan);
    sp.dly.bpm = bpm;
    constexpr int BLK = 512;
    auto z = std::make_unique<Zincir>();
    z->hazirla (sr, BLK, kanal, q, true);
    z->kur (sp);
    const float g = std::pow (10.0f, sp.inTrimDb * 0.05f);
    const float gOut = std::pow (10.0f, sp.outTrimDb * 0.05f);
    // gecikme sorgusu stereo zincirle aynı modül kümesi (kanal sayısı gecikmeyi değiştirmez)
    const int64_t L = gecikmeTelafi ? (int64_t) z->gecikme (q) : 0;
    float blokL[BLK], blokR[BLK];
    for (int64_t i = 0; i < n + L; i += BLK)
    {
        const int m = (int) std::min<int64_t> (BLK, n + L - i);
        for (int k = 0; k < m; ++k) { blokL[k] = (i + k < n ? in[i + k] : 0.0f) * g; blokR[k] = blokL[k]; }
        float* ch[2] = { blokL, blokR };
        z->isle (sp.sira, ch, kanal, m, gOut);
        for (int k = 0; k < m; ++k)
        {
            const int64_t j = i + k - L;
            if (j >= 0 && j < n) { outL[j] = blokL[k]; outR[j] = kanal == 2 ? blokR[k] : blokL[k]; }
        }
    }
}

} // namespace sweax::decision
