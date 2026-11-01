#pragma once
// SWEAX NEXT — DSP/OutputStage.h
// Çıkış kademesi: TRUE-PEAK farkındalıklı lookahead limiter (mimari §4.1, §7.2).
//  • CANLI (1×): taban hızda çekirdek + 4-fazlı Kaiser-sinc örnekler-arası TEPE TAHMİNİ (test: ≤ tavan + 0.1 dB).
//  • YÜKSEK/ULTRA (4× dahili): yarım-bant polifaz 2 kademeli yukarı/aşağı örnekleme; çekirdek (kayan minimum +
//    hareketli ortalama + dB release/hold) 4× hızda ÇALIŞIR ve kazanç 4× örneklere uygulanır → gerçek TP
//    (test: ≤ tavan + 0.05 dB). §7.2 YÜKSEK 2× der; iki mod da 4× koşulur — latency zaten eşitlenmek zorunda
//    (§7.1 YÜKSEK ≡ ULTRA) ve 4× her örnekleme hızında daha doğrudur (belgelenmiş bilinçli sapma).
//  • Latency = f(sr, mod): CANLI = (L−1) + 7 (tahmin gecikmesi); YÜKSEK/ULTRA = L + 18 (L taban örneği lookahead,
//    18 = yarım-bant zincirinin tam sayı grup gecikmesi: 6+3 yukarı + 3+6 aşağı).
//  • Kayan minimum van Herk (blok prefix/suffix min) + L uzunluklu hareketli ortalama (doğrusal atak) —
//    çekirdek uzunluğu çalışma anında L veya 4L (halkalar 4L için tahsisli; mod geçişi reset'li, RT güvenli).
#include "Module.h"
#include <array>
#include <vector>

namespace sweax
{

struct OutputParams
{
    float ceilingDbTp { -0.3f };
    float releaseMs { 120.0f };
};

class OutputStage : public ModuleBase
{
public:
    static constexpr double LOOKAHEAD_MS = 1.5;
    static constexpr int OVERSAMPLE = 4, TAPS = 16;    // CANLI yolu TP tahmini
    static constexpr int HB_LEN = 25, HB_DELAY = 12;   // yarım-bant: tek sayıda tap, tam grup gecikmesi
    static constexpr int OS = 4;                        // YÜKSEK/ULTRA dahili oran

    void prepare (double sampleRate, int maxBlock, int numChannels);
    void setQuality (Quality mode) noexcept;            // yol seçimi (reset'li; RT güvenli)
    int latencySamples (Quality mode) const noexcept;   // CANLI: (L−1)+7; diğerleri: L+18 (tanım .cpp'de — Quality tam tip)
    float reportedGainDb() const noexcept { return 0.0f; }
    float gainReductionDb() const noexcept { return grOut; }
    void setParams (const OutputParams& p) noexcept { pending = p; paramsDirty = true; }

protected:
    void processWet (float* const* ch, int numCh, int n) noexcept override;
    void idleWet (const float* const* ch, int numCh, int n) noexcept override;
    void resetWet() override;

private:
    // ---- yarım-bant kademesi (durum halkalı basit FIR; OS hızında örnek başına çağrılır) ----
    struct HalfBand
    {
        std::array<float, HB_LEN> h {};
        std::array<float, HB_LEN> z {};
        int zp { 0 };
        void design (double beta) noexcept;
        void reset() noexcept { z.fill (0.0f); zp = 0; }
        inline float tick (float x) noexcept
        {
            z[(size_t) zp] = x;
            float acc = 0.0f;
            int idx = zp;
            for (int k = 0; k < HB_LEN; ++k) { acc += h[(size_t) k] * z[(size_t) idx]; if (--idx < 0) idx = HB_LEN - 1; }
            zp = (zp + 1) % HB_LEN;
            return acc;
        }
    };

    // ---- ortak çekirdek: gerekli-kazanç dizisi → kayan min → hareketli ort → release/hold → kazanç dB ----
    inline float coreGain (float req) noexcept;         // örnek başına (aktif L uzunluğuyla)

    float truePeakEstimate (int ch, float x) noexcept;  // CANLI yolu (4 fazlı ara değer)
    void processBase (float* const* ch, int numCh, int n) noexcept;
    void processOs (float* const* ch, int numCh, int n) noexcept;

    OutputParams pending, cur; bool paramsDirty { false };
    int lookN { 72 };
    int L { 72 };                                        // aktif çekirdek uzunluğu (lookN veya OS·lookN)
    bool osActive { false };

    // CANLI yolu
    std::array<DelayLine, MAX_CH> audioDelay;
    std::array<std::array<float, TAPS>, OVERSAMPLE> fir {};
    std::array<std::array<float, TAPS>, MAX_CH> hist {}; std::array<int, MAX_CH> hpos {};

    // OS yolu
    std::array<HalfBand, MAX_CH> up1, up2, dn2, dn1;
    std::array<DelayLine, MAX_CH> osDelay;               // OS hızında ses gecikmesi (L_os)
    std::vector<float> os2, os4a, os4b;                  // ara tamponlar (maxBlock·2 / ·4, kanal başına yeniden kullanılır)
    std::vector<float> gain4;                            // OS hızında kazanç (kanallar ortak: stereo link)

    // çekirdek durumu (4L için tahsisli; L çalışma anında)
    std::vector<float> reqRing, prefMin, sufMin, avgRing;
    int ringPos { 0 }; double avgSum { 0.0 };
    float relCoef { 0.0f }; int holdN { 0 }, holdLeft { 0 };
    float gSmoothDb { 0.0f };
    float grOut { 0.0f };
    float ceilLin { 0.966f };
};

} // namespace sweax
