// SWEAX NEXT — PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/RtGuard.h"
#include "Analysis/Report.h"
#include "Decision/Planner.h"
#include "Decision/PlanPublisher.h"
#include <cmath>

namespace sweax
{
using namespace params;

SweaxProcessor::SweaxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Beat", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, &undo, "SWEAXPARAMS", ParamCache::createLayout())
{
    cache.attach (params);
    // WORST-CASE TAHSİS (§2.3): prepareToPlay yeniden tahsis etmez.
    const int worstDelay = (int) std::ceil (WORST_SR * WORST_LATENCY_SEC);
    for (auto& d : dryDelay) d.prepare (worstDelay, 0);
    dryBuffer.setSize (VocalChain::MAX_CHANNELS, WORST_BLOCK, false, true, true);
    for (auto& d : dryLimDelay) d.prepare (worstDelay, 0);
    dryLimBuffer.setSize (VocalChain::MAX_CHANNELS, WORST_BLOCK, false, true, true);
    bypassMix.prepare (WORST_SR, 10.0);
    bypassMix.setCurrentAndTarget (0.0f);
    deltaMix.prepare (WORST_SR, 10.0);
    deltaMix.setCurrentAndTarget (0.0f);
    startTimer (100);
}

void SweaxProcessor::refreshAbGain() noexcept
{
    abGainLin.store (DbGainSmoother::dbToLin (sess.getAbGainDb()), std::memory_order_relaxed);
}

void SweaxProcessor::timerCallback()
{
    if (latencyDirty.exchange (false, std::memory_order_acq_rel))
        setLatencySamples (latency.load (std::memory_order_relaxed));   // host'a bildirim (updateHostDisplay dahil)
    refreshAbGain();                                                  // A/B eşit ses (statik, §7.4)
    if (planPending.load (std::memory_order_relaxed) && analysis.reportSerial() != lastReportSerial)
        buildAndPublishPlan();
}

bool SweaxProcessor::savePresetTo (const juce::File& f)
{
    juce::MemoryBlock mb;
    getStateInformation (mb);
    f.getParentDirectory().createDirectory();
    return f.replaceWithData (mb.getData(), mb.getSize());
}

bool SweaxProcessor::loadPresetFrom (const juce::File& f)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb) || mb.getSize() == 0) return false;
    setStateInformation (mb.getData(), (int) mb.getSize());
    return true;
}

void SweaxProcessor::buildAndPublishPlan()
{
    // denetim M3.2 (K-9): dinleme sürerken asla plan yayınlama — geç gelen eski rapor kullanıcıyı
    // kayıt ortasında MixReady'ye fırlatmıyordu ama artık yapısal olarak da imkansız.
    if (sess.getScreen() == state::Screen::Listening) return;
    auto rep = analysis.latestReport();
    if (rep == nullptr) return;
    lastReportSerial = analysis.reportSerial();
    planPending.store (false, std::memory_order_relaxed);
    if (rep->finalPlan != nullptr)
        decision::PlanPublisher::apply (*rep->finalPlan, cache, sess, undo);   // worker planı (feedback + A/B kazancı dahil)
    else
    {
        // yedek yol (plan bağlamı verilmemişse): feedback'siz plan
        decision::PlannerInput in;
        in.report = rep.get();
        in.detections = &rep->detections;
        in.role = sess.getRole();
        in.planId = (uint64_t) sess.getPlanId() + 1;
        in.style = sess.getStyleIndex();
        for (size_t i = 0; i < in.locked.size(); ++i) in.locked[i] = sess.isLocked (state::modul::HEPSI[i]);
        decision::PlanPublisher::apply (decision::Planner::plan (in), cache, sess, undo);
    }
    abGainLin.store (DbGainSmoother::dbToLin (sess.getAbGainDb()), std::memory_order_relaxed);
}

SweaxProcessor::~SweaxProcessor()
{
    stopTimer();
    analysis.setListening (false);
    analysis.stopThread();
}

void SweaxProcessor::startListening()
{
    planPending.store (false, std::memory_order_relaxed);   // denetim M3.2 (K-9): bekleyen replan yayını iptal
    analysis.clearCapture();
    analysis.setListening (true);
    undo.beginNewTransaction ("Dinle");
    sess.setScreen (state::Screen::Listening, &undo);
}

static_assert (sweax::AnalysisEngine::LOCK_N == (int) sweax::state::modul::HEPSI.size(),
               "LOCK_N modul listesiyle esitlenmeli (M3 denetimi: 13 kalmisti, kilit dizisi tasiyordu)");

void SweaxProcessor::stopListening()
{
    analysis.setListening (false);
    // plan bağlamı worker'a: rol + planId + kilitler (Planner+Feedback worker'da koşar; yayın burada, timer'da)
    bool locks[AnalysisEngine::LOCK_N] = {};
    for (size_t i = 0; i < state::modul::HEPSI.size(); ++i) locks[i] = sess.isLocked (state::modul::HEPSI[i]);
    analysis.setPlanContext ((int) sess.getRole(), (uint64_t) sess.getPlanId() + 1, locks, sess.getStyleIndex());
    analysis.requestReport();          // dondurulmuş rapor: worker üretir; timer plan yayınlar (§3.5)
    planPending.store (true, std::memory_order_relaxed);
    undo.beginNewTransaction ("Dinlemeyi bitir");
    sess.setScreen (state::Screen::Ready, &undo);
}

void SweaxProcessor::cancelListening()
{
    // denetim M3.2 (UI-2): Listening sırasında rol değişimi motoru KAPATMADAN Ready'ye düşüyordu —
    // motor arkada 90 s tavanını dolduruyordu. İptal: dinleme durur, rapor İSTENMEZ, plan yayını iptal.
    analysis.setListening (false);
    planPending.store (false, std::memory_order_relaxed);
    undo.beginNewTransaction ("Dinlemeyi iptal");
    sess.setScreen (state::Screen::Ready, &undo);
}

bool SweaxProcessor::replanFromCapture()
{
    // Stil/rol değişimi anında uygulansın (sahip: sanatçı chain'i seç → hemen duy): yakalama duruyorsa
    // yeniden DİNLEMEDEN, dondurulmuş yakalamadan yeni bağlamla (rol+stil+kilitler) worker Planner+Feedback
    // koşar; timer yayınlar (stopListening ile aynı yol, ekran MixReady kalır).
    if (analysis.isListening() || analysis.progress().capturedSec < 0.05) return false;
    bool locks[AnalysisEngine::LOCK_N] = {};
    for (size_t i = 0; i < state::modul::HEPSI.size(); ++i) locks[i] = sess.isLocked (state::modul::HEPSI[i]);
    analysis.setPlanContext ((int) sess.getRole(), (uint64_t) sess.getPlanId() + 1, locks, sess.getStyleIndex());
    analysis.requestReport();
    planPending.store (true, std::memory_order_relaxed);
    return true;
}

juce::AudioProcessorParameter* SweaxProcessor::getBypassParameter() const
{
    return cache.param (Bypass);
}

bool SweaxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const bool inOk = (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
    const bool outOk = (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
    if (! inOk || ! outOk) return false;
    if (in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::mono()) return false; // stereo→mono desteklenmez
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! (sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo()))
            return false;
    }
    return true;
}

void SweaxProcessor::applyLatency()
{
    const int l = chain.latencySamples (qualityMode());
    latency.store (l, std::memory_order_relaxed);
    for (auto& d : dryDelay) d.setDelay (l);
    const int lLim = std::max (0, l - dryLimiter.latencySamples (qualityMode()));
    for (auto& d : dryLimDelay) d.setDelay (lLim);
    setLatencySamples (l);
}

void SweaxProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    maxBlock = samplesPerBlock;
    // §2.3 sırası: önce analiz iş parçacığı durur (prepare içinde join), yeniden hazırlanır ve başlar
    analysis.prepare (sr, std::min (samplesPerBlock, WORST_BLOCK));
    if (! analysis.isThreadRunning()) analysis.startThread();
    chain.prepare (sr, std::min (samplesPerBlock, WORST_BLOCK), getTotalNumOutputChannels());
    analysis.setOutputChannels (getTotalNumOutputChannels());
    dryLimiter.prepare (sr, std::min (samplesPerBlock, WORST_BLOCK), getTotalNumOutputChannels());
    dryLimiter.setEnabled (true, true);
    { OutputParams op; op.ceilingDbTp = -1.0f; op.releaseMs = 120.0f; dryLimiter.setParams (op); }
    lastMode = qualityMode();
    chain.setQuality (lastMode);
    dryLimiter.setQuality (lastMode);
    chain.snapToTargets (cache);
    bypassMix.prepare (sr, 10.0);
    bypassMix.setCurrentAndTarget (cache.flag (Bypass) ? 1.0f : 0.0f);
    deltaMix.prepare (sr, 10.0);
    deltaMix.setCurrentAndTarget (cache.flag (Delta) ? 1.0f : 0.0f);
    for (auto& d : dryDelay) d.reset();
    for (auto& d : dryLimDelay) d.reset();
    const double blocksPerSec = sr / std::max (1, samplesPerBlock);
    meterDecay = (float) std::pow (0.5, 1.0 / (blocksPerSec * 0.3));
    applyLatency();
}

void SweaxProcessor::releaseResources()
{
    chain.reset();
}

void SweaxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    rt::Scope rtKapsam;                      // Debug/test: bu blokta tahsis = ihlal sayacı
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    if (n == 0) return;

    // ---- bus okuma (§1.1): giriş yalnız bus 0; SC yalnız etkinse ----
    auto mainBus = getBusBuffer (buffer, false, 0);
    const int numIn = getMainBusNumInputChannels();
    const int numOut = std::min (mainBus.getNumChannels(), VocalChain::MAX_CHANNELS);
    if (numIn == 1 && numOut == 2)
        mainBus.copyFrom (1, 0, mainBus, 0, 0, n); // mono→stereo: 2. kanal GİRİŞ değildir, kopyala
    {
        bool sc = false;
        if (getBusCount (true) > 1)
            if (auto* b = getBus (true, 1)) sc = b->isEnabled() && b->getNumberOfChannels() > 0;
        scConnected.store (sc, std::memory_order_relaxed);
    }

    // ---- host temposu → zincir (delay senkronu; RT güvenli, yalnız double yazımı) ----
    if (auto* headBpm = getPlayHead())
        if (const auto posBpm = headBpm->getPosition())
            if (const auto b = posBpm->getBpm())
                if (*b > 1.0) chain.setBpm (*b);

    // ---- yakalama (Listening): HAM giriş → analiz FIFO'su (tahsissiz; yer yoksa tüm blok atılır, §3.1) ----
    if (analysis.isListening())
    {
        int64_t playhead = -1; bool playing = true;             // host konum vermezse LIVE (iç sayaç)
        if (auto* head = getPlayHead())
            if (const auto pos = head->getPosition())
            {
                if (const auto t = pos->getTimeInSamples()) playhead = *t;
                playing = pos->getIsPlaying();
            }
        const float* chans[2] = { mainBus.getReadPointer (0), mainBus.getReadPointer (numOut > 1 ? 1 : 0) };
        analysis.pushAudio (chans, (numIn >= 2 && numOut >= 2) ? 2 : 1, n, playhead, playing);
    }

    // ---- state yükü: modülleri sıfırla, hedefler anında (§4) ----
    {
        const uint32_t sls = stateLoadSeq.load (std::memory_order_acquire);
        if (sls != stateLoadSeen)
        {
            stateLoadSeen = sls;
            chain.reset();
            chain.snapToTargets (cache);
            bypassMix.setCurrentAndTarget (cache.flag (Bypass) ? 1.0f : 0.0f);
        }
    }

    // ---- kalite modu değişimi: gecikmeler anında (RT güvenli), host bildirimi message thread'de (§7.1) ----
    const auto mode = qualityMode();
    if (mode != lastMode)
    {
        lastMode = mode;
        chain.setQuality (mode);
        dryLimiter.setQuality (mode);
        const int l = chain.latencySamples (mode);
        latency.store (l, std::memory_order_relaxed);
        for (auto& d : dryDelay) d.setDelay (l);
        for (auto& d : dryLimDelay) d.setDelay (std::max (0, l - dryLimiter.latencySamples (mode)));
        latencyDirty.store (true, std::memory_order_release);
    }

    // ---- giriş ölçer ----
    {
        float pk = 0.0f;
        for (int c = 0; c < numOut; ++c) pk = std::max (pk, mainBus.getMagnitude (c, 0, n));
        const float db = juce::Decibels::gainToDecibels (pk, -100.0f);
        inPeakDb.store (std::max (db, inPeakDb.load (std::memory_order_relaxed) * meterDecay + (1.0f - meterDecay) * -100.0f),
                        std::memory_order_relaxed);
    }

    // ---- zincir öncesi tap ----
    // spektrum GİRİŞ tap'ı (mono karışım; yalnız editör bağlıyken — FIFO dolarsa fazlası atılır)
    if (editorAttached.load (std::memory_order_relaxed))
    {
        const float* a = mainBus.getReadPointer (0);
        const float* b = mainBus.getReadPointer (numOut > 1 ? 1 : 0);
        float tmp[512];
        int done = 0;
        while (done < n)
        {
            const int m = std::min (512, n - done);
            for (int k = 0; k < m; ++k)
            {
                float v = 0.5f * (a[done + k] + b[done + k]);
                tmp[k] = (v > -1.0e6f && v < 1.0e6f) ? v : 0.0f;   // denetim: NaN UI eğrisini kilitliyordu
            }
            specIn.push (tmp, (size_t) m);
            done += m;
        }
    }
    // ---- kuru yol + zincir + DELTA + bypass/A-B: WORST_BLOCK parçaları halinde ----
    // (denetim düzeltmesi: n > WORST_BLOCK veren hostlarda parça dışı örnekler kuru hizadan ve A/B
    // karışımından kaçıyordu; zincir kendi içinde parçalıyor ama kuru kopya/karışım parçalanmıyordu)
    const bool bypassOn = cache.flag (Bypass);
    deltaMix.setTarget ((cache.flag (Delta) && ! bypassOn) ? 1.0f : 0.0f);
    const bool abOriginal = ! bypassOn && cache.choice (AbMode) == 1;
    const bool dryWanted = bypassOn || abOriginal;
    const bool abEsSes = abOriginal && cache.flag (AbMatch);
    bypassMix.setTarget (dryWanted ? 1.0f : 0.0f);
    for (int base = 0; base < n; base += WORST_BLOCK)
    {
        const int blk = std::min (n - base, WORST_BLOCK);
        float* wptr[VocalChain::MAX_CHANNELS] = {};
        for (int c = 0; c < numOut; ++c) wptr[c] = mainBus.getWritePointer (c) + base;
        for (int c = 0; c < numOut; ++c)
            dryDelay[(size_t) c].process (wptr[c], dryBuffer.getWritePointer (c), blk);
        // A/B ORİJİNAL kuru yolu: kazanç → güvenlik limiter'ı (hep çalışır: lookahead hattı ısınık kalır)
        {
            float* lp[VocalChain::MAX_CHANNELS] = {};
            for (int c = 0; c < numOut; ++c)
            {
                lp[c] = dryLimBuffer.getWritePointer (c);
                dryLimDelay[(size_t) c].process (wptr[c], lp[c], blk);
                const float gAb = abGainLin.load (std::memory_order_relaxed);
                for (int i = 0; i < blk; ++i) lp[c][i] *= gAb;
            }
            dryLimiter.process (lp, numOut, blk);
        }
        juce::AudioBuffer<float> view (wptr, numOut, blk);
        chain.process (view, cache, mode);
        if (deltaMix.isSmoothing() || deltaMix.getCurrent() > 0.0f)
            for (int i = 0; i < blk; ++i)
            {
                const float dm = deltaMix.next();
                for (int c = 0; c < numOut; ++c) wptr[c][i] -= dryBuffer.getReadPointer (c)[i] * dm;
            }
        if (bypassMix.isSmoothing() || bypassMix.getCurrent() > 0.0f)
            for (int i = 0; i < blk; ++i)
            {
                const float m = bypassMix.next();
                for (int c = 0; c < numOut; ++c)
                    wptr[c][i] = wptr[c][i] * (1.0f - m) + (abEsSes ? dryLimBuffer.getReadPointer (c)[i] : dryBuffer.getReadPointer (c)[i]) * m;
            }
    }

    // spektrum ÇIKIŞ tap'ı (A/B-DELTA-bypass SONRASI duyulan sinyal; NaN savunması sanitize eder)
    if (editorAttached.load (std::memory_order_relaxed))
    {
        const float* a2 = mainBus.getReadPointer (0);
        const float* b2 = mainBus.getReadPointer (numOut > 1 ? 1 : 0);
        float tmp2[512];
        int done2 = 0;
        while (done2 < n)
        {
            const int m2 = std::min (512, n - done2);
            for (int k = 0; k < m2; ++k) { float v = 0.5f * (a2[done2 + k] + b2[done2 + k]); tmp2[k] = (v > -1.0e6f && v < 1.0e6f) ? v : 0.0f; }
            specOut.push (tmp2, (size_t) m2);
            done2 += m2;
        }
    }

    // ---- son savunma: sonlu değilse blok sıfır + reset (§7.4) ----
    for (int c = 0; c < numOut; ++c)
    {
        const auto* p = mainBus.getReadPointer (c);
        bool ok = true;
        for (int i = 0; i < n; ++i) if (! std::isfinite (p[i])) { ok = false; break; }   // tam blok taranır
        if (! ok)
        {
            mainBus.clear();
            chain.reset();
            nonFinite.fetch_add (1, std::memory_order_relaxed);
            break;
        }
    }

    // ---- çıkış ölçer ----
    {
        float pk = 0.0f;
        for (int c = 0; c < numOut; ++c) pk = std::max (pk, mainBus.getMagnitude (c, 0, n));
        const float db = juce::Decibels::gainToDecibels (pk, -100.0f);
        outPeakDb.store (std::max (db, outPeakDb.load (std::memory_order_relaxed) * meterDecay + (1.0f - meterDecay) * -100.0f),
                         std::memory_order_relaxed);
    }
}

// ---------------- STATE (§6) ----------------
void SweaxProcessor::refreshBaselinesFromSession()
{
    for (size_t i = 0; i < TABLO.size(); ++i)
        cache.setBaseline ((Id) i, sess.getBaseline ((Id) i, TABLO[i].def));
    // makro AI tabanları (etkin değer ölçeği bunlara bölünür — §2.2)
    for (auto m : { Makro::Temizlik, Makro::Denge, Makro::Onde, Makro::Parlaklik, Makro::Alan, Makro::Genislik, Makro::Cikis })
        cache.setMacroBaseline (m, sess.getBaseline (ParamCache::macroParam (m), 0.5f));
}

void SweaxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // message thread değilse kilit (pluginval arka plandan çağırır); alınamazsa (kapanış) çık
    std::unique_ptr<juce::MessageManagerLock> mml;
    if (! juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        mml = std::make_unique<juce::MessageManagerLock> (juce::Thread::getCurrentThread());
        if (! mml->lockWasGained()) return;
    }
    auto root = state::Migration::buildRoot (params.copyState(), sess.tree());
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void SweaxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    juce::ValueTree p, s; int v = -1;
    if (! state::Migration::splitRoot (root, p, s, v)) return;

    std::unique_ptr<juce::MessageManagerLock> mml;
    if (! juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        mml = std::make_unique<juce::MessageManagerLock> (juce::Thread::getCurrentThread());
        if (! mml->lockWasGained()) return;
    }
    // denetim M3.4: eski preset/oturumda olmayan (yeni) parametreler replaceState'te ÖNCEKİ değerde kalıyordu
    // (KİR/KATMAN açık kalabiliyordu) → eksik PARAM çocukları tablo varsayılanıyla doldurulur.
    for (const auto& t : params::TABLO)
    {
        const juce::String idStr (t.id.data(), t.id.size());
        if (! p.getChildWithProperty ("id", idStr).isValid())
        {
            juce::ValueTree c ("PARAM");
            c.setProperty ("id", idStr, nullptr);
            c.setProperty ("value", t.def, nullptr);
            p.appendChild (c, nullptr);
        }
    }
    params.replaceState (p);
    if (s.isValid()) sess.restore (s);
    refreshBaselinesFromSession();
    undo.clearUndoHistory();
    stateLoadSeq.fetch_add (1, std::memory_order_release);   // ses iş parçacığı bir SONRAKİ blokta kesin görür
    // yükleme sonrası ekran: planId varsa MixReady, yoksa Ready (§6) — Listening'e yalnız buton sokar
    analysis.setListening (false);
    if (sess.getScreen() == state::Screen::Listening)
        sess.setScreen (sess.getPlanId() != 0 ? state::Screen::MixReady : state::Screen::Ready);
}

juce::AudioProcessorEditor* SweaxProcessor::createEditor()
{
    return new SweaxEditor (*this);
}

} // namespace sweax

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new sweax::SweaxProcessor();
}
