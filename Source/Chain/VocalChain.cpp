// SWEAX NEXT — Chain/VocalChain.cpp
#include "VocalChain.h"
#include <cmath>

namespace sweax
{
using namespace params;

void VocalChain::prepare (double sampleRate, int maxBlock, int numChannels)
{
    sr = sampleRate;
    maxBlockSize = maxBlock;
    channels = std::clamp (numChannels, 1, MAX_CHANNELS);
    inTrim.prepare (sr, 20.0);
    outTrim.prepare (sr, 20.0);
    inTrim.setCurrentAndTargetDb (0.0f);
    outTrim.setCurrentAndTargetDb (0.0f);
    gate.prepare (sr, maxBlock, channels);
    tune.prepare (sr, maxBlock, channels);
    eq.prepare (sr, maxBlock, channels);
    teq.prepare (sr, maxBlock, channels, false);            // M3.5 TON EQ: önden bakışsız (gecikme 0)
    lev.prepare (sr, maxBlock, channels);
    compA.prepare (sr, maxBlock, channels);
    compB.prepare (sr, maxBlock, channels);
    ds.prepare (sr, maxBlock, channels);
    ds2.prepare (sr, maxBlock, channels);
    glow.prepare (sr, maxBlock, channels);
    sat.prepare (sr, maxBlock, channels);
    dirt.prepare (sr, maxBlock, channels);
    air.prepare (sr, maxBlock, channels);
    layer.prepare (sr, maxBlock, channels);
    soo.prepare (sr, maxBlock, channels);
    frm.prepare (sr, maxBlock, channels);
    dbl.prepare (sr, maxBlock, channels);
    dly.prepare (sr, maxBlock, channels);
    rev.prepare (sr, maxBlock, channels);
    wid.prepare (sr, maxBlock, channels);
    out.prepare (sr, maxBlock, channels);
    out.setEnabled (true, true);                       // güvenlik limiter'ı her zaman açık
    meter.prepare (sr, channels, maxBlock);
    siraFadeN = std::max (1, (int) std::lround (0.005 * sr));
    siraGecis = SiraGecis::Yok; siraFadeKalan = 0;
    maxLatency = std::max (latencySamples (Quality::Canli), std::max (latencySamples (Quality::Yuksek), latencySamples (Quality::Ultra)));
    setQuality (curMode);
}

void VocalChain::reset()
{
    gate.reset(); tune.reset(); eq.reset(); teq.reset(); lev.reset(); compA.reset(); compB.reset(); ds.reset(); ds2.reset(); glow.reset(); sat.reset(); dirt.reset(); air.reset(); layer.reset(); soo.reset(); frm.reset();
    dbl.reset(); dly.reset(); rev.reset(); wid.reset(); out.reset();
    meter.reset();
}

void VocalChain::setQuality (Quality mode) noexcept
{
    curMode = mode;
    gate.setQuality (mode);
    eq.setQuality (mode);
    teq.setQuality (mode);
    lev.setQuality (mode); compA.setQuality (mode); compB.setQuality (mode); ds.setQuality (mode); ds2.setQuality (mode);
    soo.setQuality (mode); frm.setQuality (mode);
    out.setQuality (mode);
    // gecikme hatları değişti: lookahead'li modüller sıfırlanır (tek seferlik küçük süreksizlik, kullanıcı eylemi)
    gate.reset(); lev.reset(); compA.reset(); compB.reset(); ds.reset(); ds2.reset(); soo.reset(); frm.reset();
}

int VocalChain::latencySamples (Quality mode) const noexcept
{
    // sıradan ve düzenden bağımsız toplam: her modülün gecikme hattı her düzende çalışır (ds2 dahil)
    return gate.latencySamples (mode) + tune.latencySamples (mode) + eq.latencySamples (mode) + teq.latencySamples (mode) + lev.latencySamples (mode) + compA.latencySamples (mode)
         + ds.latencySamples (mode) + compB.latencySamples (mode) + ds2.latencySamples (mode) + glow.latencySamples (mode) + sat.latencySamples (mode) + dirt.latencySamples (mode) + air.latencySamples (mode) + layer.latencySamples (mode) + soo.latencySamples (mode) + frm.latencySamples (mode)
         + dbl.latencySamples (mode) + dly.latencySamples (mode) + rev.latencySamples (mode) + wid.latencySamples (mode)
         + out.latencySamples (mode);
}

float VocalChain::reportedGainDb() const noexcept
{
    return inTrim.getCurrentDb() + outTrim.getCurrentDb() + lev.reportedGainDb() + compA.reportedGainDb() + compB.reportedGainDb();
}

void VocalChain::readParams (const ParamCache& cache, bool immediate) noexcept
{
    // ---- Gate ----
    gateP.on = cache.flag (GateOn); gateP.threshDb = cache.effective (GateThresh); gateP.rangeDb = cache.effective (GateRange);
    gateP.attackMs = cache.effective (GateAttack); gateP.releaseMs = cache.effective (GateRelease);
    gateP.holdMs = cache.effective (GateHold); gateP.hystDb = cache.effective (GateHyst);
    gate.setParams (gateP); gate.setEnabled (gateP.on, immediate);
    // ---- Tune ----
    tuneP.on = cache.flag (TuneOn); tuneP.strength = cache.effective (TuneStrength); tuneP.speedMs = cache.effective (TuneSpeed);
    tuneP.key = cache.choice (TuneKey); tuneP.scale = cache.choice (TuneScale);
    tune.setParams (tuneP); tune.setEnabled (tuneP.on, immediate);
    // ---- EQ ----
    eqP.on = cache.flag (EqOn);
    eqP.hpfOn = cache.flag (EqHpfOn); eqP.hpfHz = cache.effective (EqHpfHz); eqP.hpfSlope = cache.choice (EqHpfSlope);
    for (int b = 0; b < CorrectiveEQ::BANDS; ++b)
    {
        auto& p = eqP.band[(size_t) b];
        p.on = cache.flag (eqBant (b, 0)); p.type = cache.choice (eqBant (b, 1));
        p.hz = cache.effective (eqBant (b, 2)); p.gainDb = cache.effective (eqBant (b, 3)); p.q = cache.effective (eqBant (b, 4));
        p.dyn = cache.flag (eqBant (b, 5)); p.dynThreshDb = cache.effective (eqBant (b, 6)); p.dynRangeDb = cache.effective (eqBant (b, 7));
        p.dynAttackMs = cache.effective (eqBant (b, 8)); p.dynReleaseMs = cache.effective (eqBant (b, 9));
    }
    eqP.plosOn = cache.flag (EqPlosOn); eqP.plosHz = cache.effective (EqPlosHz); eqP.plosRangeDb = cache.effective (EqPlosRange);
    eq.setParams (eqP); eq.setEnabled (eqP.on, immediate);
    // ---- TON EQ (M3.5): preset ton eğrisi — 4 statik bant, düzeltici EQ ile aynı sınıf (HPF/koruma/dinamik yok) ----
    teqP = EqParams {};
    teqP.on = cache.flag (TeqOn);
    for (int b = 0; b < TEQ_BANT_SAYISI; ++b)
    {
        auto& p = teqP.band[(size_t) b];
        p.on = cache.flag (teqBant (b, 0)); p.type = cache.choice (teqBant (b, 1));
        p.hz = cache.effective (teqBant (b, 2)); p.gainDb = cache.effective (teqBant (b, 3)); p.q = cache.effective (teqBant (b, 4));
    }
    teq.setParams (teqP); teq.setEnabled (teqP.on, immediate);
    // ---- Leveler ----
    levP.on = cache.flag (LevOn); levP.targetLufs = cache.effective (LevTarget); levP.maxUpDb = cache.effective (LevMaxUp);
    levP.maxDownDb = cache.effective (LevMaxDown); levP.speed = cache.effective (LevSpeed);
    lev.setParams (levP); lev.setEnabled (levP.on, immediate);
    // ---- Comp A / B ----
    auto readComp = [&] (CompParams& p, Id on, Id thresh, Id ratio, Id knee, Id attack, Id release, Id look, Id makeup, Id mix, Id det, Id schpf, Id chr)
    {
        p.on = cache.flag (on); p.threshDb = cache.effective (thresh); p.ratio = cache.effective (ratio); p.kneeDb = cache.effective (knee);
        p.attackMs = cache.effective (attack); p.releaseMs = cache.effective (release); p.lookaheadMs = cache.effective (look);
        p.makeupDb = cache.effective (makeup); p.mix = cache.effective (mix); p.detector = cache.choice (det);
        p.scHpfHz = cache.effective (schpf); p.character = cache.choice (chr);
    };
    readComp (caP, CaOn, CaThresh, CaRatio, CaKnee, CaAttack, CaRelease, CaLookahead, CaMakeup, CaMix, CaDetector, CaScHpf, CaCharacter);
    readComp (cbP, CbOn, CbThresh, CbRatio, CbKnee, CbAttack, CbRelease, CbLookahead, CbMakeup, CbMix, CbDetector, CbScHpf, CbCharacter);
    compA.setParams (caP); compA.setEnabled (caP.on, immediate);
    compB.setParams (cbP); compB.setEnabled (cbP.on, immediate);
    // ---- De-esser. ds2 = İKİNCİ AŞAMA, AIR'dan SONRA (M3.4 denetim T3: GLOW/SAT/KİR/AIR s-seslerini gövdeye göre
    // girişten 4-6 dB öne çıkarıyordu — ölçüldü); GÖRELİ eşik (s bandı − gövde, DsPost), range ≤ 6 dB; DS açıkken ----
    dsP.on = cache.flag (DsOn); dsP.mode = cache.choice (DsMode); dsP.hz = cache.effective (DsHz); dsP.threshDb = cache.effective (DsThresh);
    dsP.rangeDb = cache.effective (DsRange); dsP.lookaheadMs = cache.effective (DsLookahead); dsP.attackMs = cache.effective (DsAttack);
    dsP.releaseMs = cache.effective (DsRelease); dsP.link = cache.flag (DsLink);
    ds.setParams (dsP); ds.setEnabled (dsP.on, immediate);
    // M3.5: ds2 artık BİRİNCİ de-esser'dan bağımsız (kanıt yoksa DS kapanıyor, s-sesi sigortası da kapanıyordu).
    // Bant/zamanlama ds ile ortak (aynı sesin s bölgesi), aralık ve açma kendi parametrelerinde.
    ds2P = dsP;
    ds2P.on = cache.flag (Ds2On);
    ds2P.rangeDb = std::min (cache.effective (Ds2Range), 9.0f);
    ds2P.goreli = true; ds2P.goreliEsikDb = cache.effective (DsPost);
    ds2.setParams (ds2P); ds2.setEnabled (ds2P.on, immediate);
    // ---- Glow (M3.2: OTT tarzi 3 bant yukari+asagi) ----
    glowP.on = cache.flag (GlowOn); glowP.depth = cache.effective (GlowDepth); glowP.timeMul = cache.effective (GlowTime);
    glow.setParams (glowP); glow.setEnabled (glowP.on, immediate);
    // ---- Saturator ----
    satP.on = cache.flag (SatOn); satP.driveDb = cache.effective (SatDrive); satP.toneDb = cache.effective (SatTone);
    satP.mix = cache.effective (SatMix); satP.character = cache.choice (SatChar);   // M3.5: Tüp / Bant / Kırık
    sat.setParams (satP); sat.setEnabled (satP.on, immediate);
    // ---- Dirt (M3.4: paralel kir veri yolu) ----
    dirtP.on = cache.flag (DirtOn); dirtP.mode = cache.choice (DirtMode); dirtP.driveDb = cache.effective (DirtDrive);
    dirtP.levelDb = cache.effective (DirtLevel); dirtP.toneHz = cache.effective (DirtTone);
    dirtP.bits = cache.effective (DirtBits); dirtP.rate = cache.effective (DirtRate);
    dirt.setParams (dirtP); dirt.setEnabled (dirtP.on, immediate);
    // ---- Layer (M3.4: pitch-shift katman) ----
    layerP.on = cache.flag (LayerOn); layerP.semiA = cache.effective (LayerA); layerP.semiB = cache.effective (LayerB);
    layerP.levelDb = cache.effective (LayerLevel); layerP.width = cache.effective (LayerWidth);
    layerP.toneHz = cache.effective (LayerTone); layerP.grit = cache.effective (LayerGrit);
    layer.setParams (layerP); layer.setEnabled (layerP.on, immediate);
    // ---- YUMUŞAT (M3.5: soothe2 / Silk Vocal tarzı dinamik rezonans bastırma) ----
    sooP.on = cache.flag (SooOn); sooP.depth = cache.effective (SooDepth); sooP.sharp = cache.effective (SooSharp);
    sooP.loHz = cache.effective (SooLo); sooP.hiHz = cache.effective (SooHi); sooP.speed = cache.effective (SooSpeed);
    soo.setParams (sooP); soo.setEnabled (sooP.on, immediate);
    // ---- FORMANT (M3.5: perdeden bağımsız ses rengi) ----
    frmP.on = cache.flag (FrmOn); frmP.shiftSt = cache.effective (FrmShift); frmP.mix = cache.effective (FrmMix);
    frm.setParams (frmP); frm.setEnabled (frmP.on, immediate);
    // ---- Air (M3: Fresh Air tarzı program-bağımlı hava) ----
    airP.on = cache.flag (AirOn); airP.mid = cache.effective (AirMid); airP.high = cache.effective (AirHigh);
    air.setParams (airP); air.setEnabled (airP.on, immediate);
    // ---- FX: doubler / delay / reverb / width ----
    dblP.on = cache.flag (DblOn); dblP.mix = cache.effective (DblMix); dblP.spreadMs = cache.effective (DblSpreadMs);
    dblP.detuneCents = cache.effective (DblDetune); dblP.hpHz = cache.effective (DblHpHz); dblP.lpHz = cache.effective (DblLpHz);
    dblP.voices = cache.choice (DblVoices);
    dbl.setParams (dblP); dbl.setEnabled (dblP.on, immediate);
    dlyP.on = cache.flag (DlyOn); dlyP.mix = cache.effective (DlyMix); dlyP.sync = cache.choice (DlySync);
    dlyP.timeMs = cache.effective (DlyTimeMs); dlyP.feedback = cache.effective (DlyFeedback);
    dlyP.hpHz = cache.effective (DlyHpHz); dlyP.lpHz = cache.effective (DlyLpHz); dlyP.duckDb = cache.effective (DlyDuck);
    dlyP.width = cache.effective (DlyWidth);
    dlyP.bpm = bpm;
    dly.setParams (dlyP); dly.setEnabled (dlyP.on, immediate);
    revP.on = cache.flag (RevOn); revP.mix = cache.effective (RevMix); revP.preDelayMs = cache.effective (RevPre);
    revP.decaySec = cache.effective (RevDecay); revP.dampHz = cache.effective (RevDamp); revP.hpHz = cache.effective (RevHp);
    revP.width = cache.effective (RevWidth); revP.duckDb = cache.effective (RevDuck); revP.type = cache.choice (RevType);
    rev.setParams (revP); rev.setEnabled (revP.on, immediate);
    // ---- zincir sırası (M3.5) ----
    {
        std::array<float, EK_SAYISI> konum {};
        for (int i = 0; i < EK_SAYISI; ++i) konum[(size_t) i] = cache.raw (EK_KONUM_PARAM[(size_t) i]);
        const Sira hedef = siraHesapla (konum);
        if (immediate) { sira = bekleyenSira = hedef; siraGecis = SiraGecis::Yok; siraFadeKalan = 0; }
        else if (hedef != (siraGecis == SiraGecis::Kapaniyor ? bekleyenSira : sira))
        {
            bekleyenSira = hedef;
            if (siraGecis != SiraGecis::Kapaniyor)
            {
                // açılış sürüyorsa aynı kazanç noktasından kapanmaya dön
                siraFadeKalan = siraGecis == SiraGecis::Aciliyor ? siraFadeN - siraFadeKalan : siraFadeN;
                siraGecis = SiraGecis::Kapaniyor;
            }
        }
    }
    widP.on = cache.flag (WidOn); widP.amount = cache.effective (WidAmount);
    wid.setParams (widP); wid.setEnabled (widP.on, immediate);
    // ---- Output ----
    outP.ceilingDbTp = cache.effective (OutCeiling); outP.releaseMs = cache.effective (OutRelease);
    out.setParams (outP);
    // ---- trim ----
    if (immediate) { inTrim.setCurrentAndTargetDb (cache.effective (InputTrim)); outTrim.setCurrentAndTargetDb (cache.effective (OutputTrim)); }
    else { inTrim.setTargetDb (cache.effective (InputTrim)); outTrim.setTargetDb (cache.effective (OutputTrim)); }
}

void VocalChain::snapToTargets (const ParamCache& cache) noexcept
{
    readParams (cache, true);
}

void VocalChain::applyTrim (juce::AudioBuffer<float>& buffer, DbGainSmoother& trim, int n, int ch) noexcept
{
    if (trim.isSmoothing())
    {
        for (int i = 0; i < n; ++i)
        {
            const float g = trim.nextLinear();
            for (int c = 0; c < ch; ++c) buffer.getWritePointer (c)[i] *= g;
        }
    }
    else
    {
        const float g = trim.nextLinear();
        if (std::fabs (g - 1.0f) > 1.0e-6f) buffer.applyGain (0, n, g);
    }
}

void VocalChain::ekIsle (Ek e, float* const* p, int ch, int n) noexcept
{
    switch (e)
    {
        case Ek::Tune:  tune.process (p, ch, n); break;
        case Ek::Eq:    eq.process (p, ch, n); break;
        case Ek::Lev:   lev.process (p, ch, n); break;
        case Ek::CompA: compA.process (p, ch, n); break;
        case Ek::Ds:    ds.process (p, ch, n); break;
        case Ek::CompB: compB.process (p, ch, n); break;
        case Ek::Glow:  glow.process (p, ch, n); break;
        case Ek::Sat:   sat.process (p, ch, n); break;
        case Ek::Dirt:  dirt.process (p, ch, n); break;
        case Ek::Air:   air.process (p, ch, n); break;
        case Ek::Ds2:   ds2.process (p, ch, n); break;
        case Ek::Layer: layer.process (p, ch, n); break;
        case Ek::TonEq: teq.process (p, ch, n); break;
        case Ek::Yumusat: soo.process (p, ch, n); break;
        case Ek::Formant: frm.process (p, ch, n); break;
    }
}

void VocalChain::process (juce::AudioBuffer<float>& buffer, const ParamCache& cache, Quality mode) noexcept
{
    if (mode != curMode) setQuality (mode);
    const int n = buffer.getNumSamples();
    const int ch = std::min (buffer.getNumChannels(), channels);
    readParams (cache, false);
    float* ptrs[MAX_CHANNELS] = { buffer.getWritePointer (0), ch > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0) };

    applyTrim (buffer, inTrim, n, ch);
    gate.process (ptrs, ch, n);
    for (const auto e : sira) ekIsle ((Ek) e, ptrs, ch, n);          // preset sırası (M3.5)
    dbl.process (ptrs, ch, n);
    dly.process (ptrs, ch, n);
    rev.process (ptrs, ch, n);
    wid.process (ptrs, ch, n);
    applyTrim (buffer, outTrim, n, ch);
    // sıra geçişi: 5 ms kıs → (blok sonunda) sırayı değiştir → 5 ms aç
    if (siraGecis != SiraGecis::Yok)
    {
        for (int i = 0; i < n; ++i)
        {
            float g;
            if (siraGecis == SiraGecis::Kapaniyor) { g = (float) siraFadeKalan / (float) siraFadeN; if (siraFadeKalan > 0) --siraFadeKalan; }
            else { g = 1.0f - (float) siraFadeKalan / (float) siraFadeN; if (siraFadeKalan > 0) --siraFadeKalan; }
            for (int c = 0; c < ch; ++c) ptrs[c][i] *= g;
        }
        if (siraGecis == SiraGecis::Kapaniyor && siraFadeKalan == 0) { sira = bekleyenSira; siraGecis = SiraGecis::Aciliyor; siraFadeKalan = siraFadeN; }
        else if (siraGecis == SiraGecis::Aciliyor && siraFadeKalan == 0) siraGecis = SiraGecis::Yok;
    }
    out.process (ptrs, ch, n);
    meter.process (ptrs, ch, n);
    meter.pushGainReduction (eq.bandGainReductionDb (0), lev.reportedGainDb(), compA.gainReductionDb(), ds.gainReductionDb(), compB.gainReductionDb(), out.gainReductionDb(), gate.gainReductionDb());
}

} // namespace sweax
