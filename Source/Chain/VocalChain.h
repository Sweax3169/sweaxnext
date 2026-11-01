#pragma once
// SWEAX NEXT — Chain/VocalChain.h
// Vokal zinciri konteyneri (mimari §1.2, §4, §7.1).
//  • Latency yalnız (sampleRate, kaliteModu) fonksiyonudur; enable/plan/layout latency'yi DEĞİŞTİREMEZ:
//    tüm modüllerin (2. de-esser dahil) gecikme hatları her düzende çalışır, toplam sıradan bağımsızdır.
//  • Kapalı modül işleme yapmaz ama kendi gecikme hattını sürdürür (ModuleBase kuru yolu).
//  • Tüm tahsis prepare()'da. process() gerçek zamanlıdır: tahsis/kilit/String yok; parametreler ParamCache'ten
//    blok başında okunur (atomik), modüllere POD olarak verilir.
//  • Sıra (M3.5, Chain/ZincirSirasi.h): GATE → [ekleme modülleri swx.pos.* sırasıyla] → DBL → DLY → REV → WID →
//    OutputTrim → SafetyLimiter (OutputStage) → Metering. Sıra değişimi 5 ms kıs → değiştir → 5 ms aç (tıksız).
//    swx.layout (M2 düzenleri) artık kullanılmaz; eski oturumların tüm konumları varsayılan = eski Standard zincir.
//  • Kalite modu değişimi (setQuality): modül gecikmeleri + lookahead'li modüllerin reset'i; RT güvenli.
#include "../Core/ParamCache.h"
#include "../Core/Smoother.h"
#include "../Core/DelayLine.h"
#include "../DSP/NoiseGate.h"
#include "../DSP/CorrectiveEQ.h"
#include "../DSP/VocalLeveler.h"
#include "../DSP/Compressor.h"
#include "../DSP/DeEsser.h"
#include "../DSP/Saturator.h"
#include "../DSP/AirExciter.h"
#include "../DSP/GlowModule.h"
#include "../DSP/DirtModule.h"
#include "../DSP/LayerModule.h"
#include "../DSP/YumusatModule.h"
#include "../DSP/FormantModule.h"
#include "../DSP/PitchCorrector.h"
#include "../DSP/Doubler.h"
#include "../DSP/StereoDelay.h"
#include "../DSP/PlateReverb.h"
#include "../DSP/WidthModule.h"
#include "../DSP/OutputStage.h"
#include "../DSP/Metering.h"
#include "ZincirSirasi.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace sweax
{

enum class Quality : int { Canli = 0, Yuksek = 1, Ultra = 2 };
enum class ChainLayout : int { Standard = 0, DeEssBeforeComp = 1, DualDeEss = 2 };

class VocalChain
{
public:
    static constexpr int MAX_CHANNELS = 2;

    void prepare (double sampleRate, int maxBlock, int numChannels);
    void reset();

    // RT güvenli: aktif mod (gecikmeler); prepare sonrası ve mod değişince çağrılır
    void setQuality (Quality mode) noexcept;
    Quality quality() const noexcept { return curMode; }

    // Kalite moduna göre toplam zincir gecikmesi (örnek). YÜKSEK ≡ ULTRA (§7.1).
    int latencySamples (Quality mode) const noexcept;
    int maxLatencySamples() const noexcept { return maxLatency; }

    // Gerçek zamanlı: buffer yerinde işlenir; parametreler cache'ten okunur.
    void process (juce::AudioBuffer<float>& buffer, const ParamCache& cache, Quality mode) noexcept;

    // Yükleme/preset sonrası: tüm yumuşatıcılar hedefe ANINDA (yırtık kombinasyon + crossfade yok)
    void snapToTargets (const ParamCache& cache) noexcept;

    float reportedGainDb() const noexcept;
    Metering& metering() noexcept { return meter; }
    const Metering& metering() const noexcept { return meter; }

    // erişim (UI/test)
    // host temposu (processBlock her blok iletir; host vermezse 120)
    void setBpm (double b) noexcept { bpm = b; }

    const NoiseGate& gateModule() const noexcept { return gate; }
    const PitchCorrector& tuneModule() const noexcept { return tune; }
    const Saturator& satModule() const noexcept { return sat; }
    const AirExciter& airModule() const noexcept { return air; }
    const GlowModule& glowModule() const noexcept { return glow; }
    const DirtModule& dirtModule() const noexcept { return dirt; }
    const LayerModule& layerModule() const noexcept { return layer; }
    const YumusatModule& yumusatModule() const noexcept { return soo; }
    const FormantModule& formantModule() const noexcept { return frm; }
    const CorrectiveEQ& eqModule() const noexcept { return eq; }
    const CorrectiveEQ& tonEqModule() const noexcept { return teq; }
    const VocalLeveler& levelerModule() const noexcept { return lev; }
    const Compressor& compAModule() const noexcept { return compA; }
    const Compressor& compBModule() const noexcept { return compB; }
    const DeEsser& deEsserModule() const noexcept { return ds; }
    const OutputStage& outputModule() const noexcept { return out; }

private:
    void readParams (const ParamCache& cache, bool immediate) noexcept;
    void ekIsle (Ek e, float* const* p, int ch, int n) noexcept;
public:
    const Sira& aktifSira() const noexcept { return sira; }
private:
    Sira sira { varsayilanSira() }, bekleyenSira { varsayilanSira() };
    enum class SiraGecis : uint8_t { Yok, Kapaniyor, Aciliyor };
    SiraGecis siraGecis { SiraGecis::Yok };
    int siraFadeKalan { 0 }, siraFadeN { 240 };
    void applyTrim (juce::AudioBuffer<float>& buffer, DbGainSmoother& trim, int n, int ch) noexcept;

    double sr { 48000.0 };
    int maxBlockSize { 0 }, channels { 2 };
    int maxLatency { 0 };
    Quality curMode { Quality::Yuksek };
    DbGainSmoother inTrim, outTrim;
    NoiseGate gate; PitchCorrector tune; CorrectiveEQ eq, teq; VocalLeveler lev; Compressor compA, compB; DeEsser ds, ds2; GlowModule glow; Saturator sat; DirtModule dirt; AirExciter air; LayerModule layer; YumusatModule soo; FormantModule frm;
    Doubler dbl; StereoDelay dly; PlateReverb rev; WidthModule wid; OutputStage out;
    Metering meter;
    double bpm { 120.0 };
    GateParams gateP; TuneParams tuneP; EqParams eqP, teqP; LevelerParams levP; CompParams caP, cbP; DeEssParams dsP, ds2P; SatParams satP; AirParams airP; GlowParams glowP; DirtParams dirtP; LayerParams layerP; YumusatParams sooP; FormantParams frmP;
    DoublerParams dblP; DelayParams dlyP; ReverbParams revP; WidthParams widP; OutputParams outP;
};

} // namespace sweax
