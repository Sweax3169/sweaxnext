#pragma once
// SWEAX NEXT — DSP/Module.h
// Modül sözleşmesi (mimari §4): her modül
//  • prepare()'da worst-case tahsis yapar (en büyük lookahead/oversampling/maxBlock), sonra tahsissiz çalışır;
//  • latencySamples(mode) yalnız (sr, mod) fonksiyonudur — enable/plan değiştiremez;
//  • kapalıyken işleme yapmaz ama TÜM gecikme hatları çalışır: ModuleBase kuru yolu + modülün kendi ıslak-yol
//    gecikme hatları (idleWet kancası) — aksi halde yeniden açılışta hat, donduğu andaki BAYAT sesi çalar
//    (incelemede ölçüldü: leveler 150 ms'lik 1 sn önceki sinyali tam seviyede bastı);
//  • enable/disable = eş-kazanç lineer crossfade (10 ms), tıksız.
//  • process() n > maxBlock bloklarını İÇERİDE parçalar (bazı hostlar prepare boyutunu aşar; taşma incelemede
//    heap bozulması olarak ölçüldü).
// Şablon yöntem: process() → [alt bloklara böl] → captureDry → processWet/idleWet → blendBypass.
#include "../Core/DelayLine.h"
#include "../Core/Smoother.h"
#include "../Core/Biquad.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <algorithm>

namespace sweax
{

enum class Quality : int;   // Chain/VocalChain.h

class ModuleBase
{
public:
    static constexpr int MAX_CH = 2;
    virtual ~ModuleBase() = default;

    // türeyen prepare() bunu çağırır: worstLatency = bu modülün en büyük gecikmesi (tüm modlar)
    void prepareBase (double sampleRate, int maxBlock, int numChannels, int worstLatency)
    {
        sr = sampleRate; maxBlockN = std::max (1, maxBlock); chans = std::clamp (numChannels, 1, MAX_CH);
        for (auto& d : dryDelay) d.prepare (std::max (0, worstLatency), 0);
        dry.assign ((size_t) MAX_CH * (size_t) maxBlockN, 0.0f);
        enableMix.prepare (sr, 10.0);
        enableMix.setCurrentAndTarget (0.0f);
        enabledTarget = false;
    }
    // aktif gecikme (mod değişince zincir çağırır; prepare sonrası, RT dışı)
    void setDryDelay (int samples) noexcept { for (auto& d : dryDelay) d.setDelay (samples); }

    virtual void reset() { for (auto& d : dryDelay) d.reset(); resetWet(); }

    // RT: crossfade (immediate = state yükü/preset: anında)
    void setEnabled (bool on, bool immediate = false) noexcept
    {
        enabledTarget = on;
        if (immediate) enableMix.setCurrentAndTarget (on ? 1.0f : 0.0f); else enableMix.setTarget (on ? 1.0f : 0.0f);
    }
    bool isEnabled() const noexcept { return enabledTarget; }
    bool isAudible() const noexcept { return enableMix.getCurrent() > 0.0f || enableMix.isSmoothing() || enabledTarget; }

    // RT giriş noktası: buffer yerinde; n sınırsız (içeride maxBlock parçalarına bölünür)
    void process (float* const* ch, int numCh, int n) noexcept
    {
        const int c = std::min (numCh, chans);
        int done = 0;
        while (done < n)
        {
            const int m = std::min (n - done, maxBlockN);
            float* sub[MAX_CH];
            for (int k = 0; k < c; ++k) sub[k] = ch[k] + done;
            for (int k = c; k < MAX_CH; ++k) sub[k] = sub[0];
            processChunk (sub, c, m);
            done += m;
        }
    }

    double sampleRate() const noexcept { return sr; }
    int numChannels() const noexcept { return chans; }

protected:
    virtual void processWet (float* const* ch, int numCh, int n) noexcept = 0;
    // kapalıyken gecikme hatlarını besle (girişle); durum ısınmış kalır → yeniden açılış bayat ses çalmaz
    virtual void idleWet (const float* const* ch, int numCh, int n) noexcept { (void) ch; (void) numCh; (void) n; }
    virtual void resetWet() = 0;
    double sr { 48000.0 };
    int maxBlockN { 1 }, chans { 2 };

private:
    void processChunk (float* const* ch, int c, int n) noexcept
    {
        // kuru yol: gecikmiş giriş (her zaman çalışır → latency sabit)
        for (int k = 0; k < c; ++k) dryDelay[(size_t) k].process (ch[k], dry.data() + (size_t) k * (size_t) maxBlockN, n);
        const bool wetNeeded = enabledTarget || enableMix.isSmoothing() || enableMix.getCurrent() > 0.0f;
        if (wetNeeded) processWet (ch, c, n);
        else idleWet (ch, c, n);
        // eş-kazanç lineer crossfade (korele sinyaller)
        if (enableMix.isSmoothing() || enableMix.getCurrent() < 1.0f)
        {
            for (int i = 0; i < n; ++i)
            {
                const float g = enableMix.next();
                for (int k = 0; k < c; ++k)
                {
                    const float d = dry[(size_t) k * (size_t) maxBlockN + (size_t) i];
                    ch[k][i] = wetNeeded ? ch[k][i] * g + d * (1.0f - g) : d;
                }
            }
        }
    }

    std::array<DelayLine, MAX_CH> dryDelay;
    std::vector<float> dry;
    LinearSmoother enableMix;
    bool enabledTarget { false };
};

// Katsayı geçişi (mimari §4): küçük değişim → per-block yeniden hesap; eşik üstü sıçrama → ikinci filtre ISITILIR
// (t_w = max(20 ms, 3/fc)) sonra aynı sürede crossfade. Filtre = kanal başına N biquad kademesi.
// İnceleme düzeltmeleri: (1) geçiş sırasında setCoeffsDirect artık YALNIZ hedef sete yazar (sönen setin eski
// katsayıları korunur — ikisine yazmak crossfade'i boşa çıkarıyordu); (2) Crossfade sürerken gelen yeni sıçrama
// kısmen duyulur sete inmez: KUYRUĞA alınır ve mevcut geçiş bitince yeni ısıtma+crossfade başlar.
template <int MAX_STAGES>
class CrossfadingFilter
{
public:
    void prepare (double sampleRate, int numCh)
    {
        sr = sampleRate; chans = std::clamp (numCh, 1, ModuleBase::MAX_CH);
        reset();
        state = Stable; counter = 0; stages = 1; queued = false;
    }
    void reset() noexcept { for (auto& set : f) for (auto& c : set) for (auto& b : c) b.reset(); state = Stable; counter = 0; queued = false; }
    void setStages (int n) noexcept { stages = std::clamp (n, 1, MAX_STAGES); }

    // küçük değişim: hedef setin katsayılarını güncelle (Stable → aktif set; geçişte → gelmekte olan set)
    void setCoeffsDirect (int stage, const BiquadCoeffs& c) noexcept
    {
        const int target = (state == Stable) ? active : 1 - active;
        for (int k = 0; k < chans; ++k) f[(size_t) target][(size_t) k][(size_t) stage].setCoeffs (c);
    }
    // büyük sıçrama: yeni set + ısıtma + crossfade (süre fc'ye bağlı)
    void beginJump (double fcHz) noexcept
    {
        const int t = (int) std::lround (std::max (0.020, 3.0 / std::max (fcHz, 1.0)) * sr);
        if (state == Stable)
        {
            for (int k = 0; k < chans; ++k) for (auto& b : f[(size_t) (1 - active)][(size_t) k]) b.reset();
            state = Warmup; counter = 0; duration = t;
        }
        else if (state == Warmup) { duration = std::max (duration, t); }           // ısınan set zaten sessiz
        else { queued = true; queuedDuration = t; }                                 // Crossfade: kuyruğa al
    }
    void setPendingCoeffs (int stage, const BiquadCoeffs& c) noexcept
    {
        if (state == Crossfade && queued) { queuedCoeffs[(size_t) stage] = c; queuedValid[(size_t) stage] = true; return; }
        const int target = (state == Stable) ? active : 1 - active;
        for (int k = 0; k < chans; ++k) f[(size_t) target][(size_t) k][(size_t) stage].setCoeffs (c);
    }
    bool inTransition() const noexcept { return state != Stable || queued; }

    inline float processSample (int ch, float x) noexcept
    {
        float a = x;
        for (int s = 0; s < stages; ++s) a = f[(size_t) active][(size_t) ch][(size_t) s].processSample (a);
        if (state == Stable) return a;
        float b = x;
        for (int s = 0; s < stages; ++s) b = f[(size_t) (1 - active)][(size_t) ch][(size_t) s].processSample (b);
        if (state == Warmup) return a;
        const float m = duration > 0 ? (float) counter / (float) duration : 1.0f;
        return a * (1.0f - m) + b * m;
    }
    // her örnekten sonra (tüm kanallar işlendikten sonra) bir kez
    inline void advance() noexcept
    {
        if (state == Stable) return;
        if (++counter >= duration)
        {
            counter = 0;
            if (state == Warmup) state = Crossfade;
            else
            {
                active = 1 - active;
                if (queued)
                {
                    queued = false;
                    for (int k = 0; k < chans; ++k) for (auto& b : f[(size_t) (1 - active)][(size_t) k]) b.reset();
                    for (int s = 0; s < stages; ++s)
                        if (queuedValid[(size_t) s])
                            for (int k = 0; k < chans; ++k) f[(size_t) (1 - active)][(size_t) k][(size_t) s].setCoeffs (queuedCoeffs[(size_t) s]);
                    for (auto& v : queuedValid) v = false;
                    state = Warmup; duration = queuedDuration;
                }
                else state = Stable;
            }
        }
    }
    void sanitize() noexcept { for (auto& set : f) for (auto& c : set) for (auto& b : c) b.sanitize(); }
    float debugB0() const noexcept { return (float) f[(size_t) active][0][0].coeffs().b0; }

private:
    enum State { Stable, Warmup, Crossfade };
    double sr { 48000.0 };
    int chans { 2 }, stages { 1 }, active { 0 }, counter { 0 }, duration { 0 };
    State state { Stable };
    bool queued { false }; int queuedDuration { 0 };
    std::array<BiquadCoeffs, MAX_STAGES> queuedCoeffs {};
    std::array<bool, MAX_STAGES> queuedValid {};
    std::array<std::array<std::array<Biquad, MAX_STAGES>, ModuleBase::MAX_CH>, 2> f;
};

} // namespace sweax
