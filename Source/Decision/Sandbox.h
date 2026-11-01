#pragma once
// SWEAX NEXT — Decision/Sandbox.h
// Sandbox render (mimari §5.3): MixPlan → GERÇEK modül sınıflarının ikinci örnekleri, host hızında, mono.
// APVTS'ye DOKUNMAZ: plan parametreleri doğrudan modül POD'larına eşlenir. Feedback döngüsü ve A/B eşit-ses
// kazancı bunu kullanır. Message thread/worker; RT değil (tahsis serbest, süre bütçesi Feedback'te).
#include "Plan.h"
#include "../DSP/CorrectiveEQ.h"
#include "../DSP/VocalLeveler.h"
#include "../DSP/Compressor.h"
#include "../DSP/DeEsser.h"
#include "../DSP/OutputStage.h"
#include "../DSP/AirExciter.h"
#include "../DSP/GlowModule.h"
#include "../DSP/DirtModule.h"
#include "../DSP/LayerModule.h"
#include "../DSP/YumusatModule.h"
#include "../DSP/FormantModule.h"
#include "../Chain/VocalChain.h"
#include <memory>

namespace sweax::decision
{

struct SandboxParams
{
    float inTrimDb { 0.0f }, outTrimDb { 0.0f };
    bool eqOn { false }, levOn { false }, caOn { false }, cbOn { false }, dsOn { false }, gateOn { false }, satOn { false };
    bool dblOn { false }, dlyOn { false }, revOn { false }, widOn { false }, tuneOn { false }, airOn { false }, glowOn { false }, dirtOn { false }, layerOn { false };
    bool teqOn { false }, sooOn { false }, frmOn { false };
    GateParams gate; TuneParams tune; EqParams eq, teq; LevelerParams lev; CompParams ca, cb; DeEssParams ds; SatParams sat;
    AirParams air; GlowParams glowp; DirtParams dirtp; LayerParams layerp; YumusatParams soop; FormantParams frmp;
    float dsPostDb { -4.0f };
    bool ds2On { false };
    float ds2RangeDb { 6.0f };
    Sira sira { varsayilanSira() };
    DoublerParams dbl; DelayParams dly; ReverbParams rev; WidthParams wid; OutputParams out;
};

class Sandbox
{
public:
    // plan parametre listesi → POD'lar (varsayılanlar ParamIds tablosundakiyle aynı başlangıç)
    static SandboxParams paramsFromPlan (const MixPlan& plan);

    // TAM zincir (FX dahil) stereo render — SweaxRender/dinleme için; feedback DÜZELTİCİ mono render'ı kullanır
    // gecikmeTelafi: giriş zincir gecikmesi kadar sıfırla uzatılıp çıkış kaydırılır → out[i] girişin i. örneğine hizalı
    // kanal: 2 = stereo bus; 1 = mono bus (modüller 1 kanal çalışır — FX ıslak toplamı mono'da farklıdır; outR = outL)
    static void renderStereo (const MixPlan& plan, const float* in, float* outL, float* outR, int64_t n, double sr,
                              Quality q = Quality::Yuksek, double bpm = 120.0, bool gecikmeTelafi = false, int kanal = 2);

    // renderStereo zincirinin toplam gecikmesi (örnek) — ölçüm pencerelerini hizalamak için
    static int latencySamples (double sr, Quality q = Quality::Yuksek);
    // mono düzeltici render'ın (render) gecikmesi — FX/TUNE/LAYER içermez
    static int monoLatencySamples (double sr, Quality q = Quality::Yuksek);

    // out.size() == n; mono. Zincir sırası Standard; latency telafisi YOK (skorlama enerji-tabanlı).
    static void render (const MixPlan& plan, const float* in, float* out, int64_t n, double sr, Quality q = Quality::Yuksek,
                        bool gecikmeTelafi = false);
};

} // namespace sweax::decision
