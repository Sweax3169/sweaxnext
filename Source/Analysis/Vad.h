#pragma once
// SWEAX NEXT — Analysis/Vad.h
// Vokal etkinlik bayrakları (mimari §3.3): kare başına ÇOK-ETİKETLİ bayrak vektörü — karşılıklı dışlayıcı değil
// (/z/ hem voiced hem sibilant). Ölçüm katmanı: yalnız kanıt üretir, karar vermez.
//  • Gürültü tabanı: minimum-istatistik (Martin ruhu, basitleştirilmiş): yumuşatılmış enerji, 10 × 150 ms alt pencere
//    minimumu + sapma payı. RoomTone < 0.5 s ise gürültü güveni düşük (confidence tarafı okur).
//  • voiced: perde periyodikliği + enerji > taban + 10 dB. tonalEligible: voiced VEYA (enerji > taban+20 VE düzlük<0.4).
//  • sibilant: şarkıcıya kalibre bant (ilk 10 olayın enerji ağırlık merkezi f_s; oran [0.7f_s,1.6f_s]/[1 kHz,0.7f_s]),
//    arama 3.5–12 kHz; süre 20–400 ms (olay düzeyinde); centroid sıçraması + düzlük.
//  • plosive: LF zarf sıçraması ≥ +8 dB (şarkıcının SESLİ LF taban çizgisine göre) VE geçici (akı) VE LF'de harmonik yapı
//    yok (F0 harmonikleri LF enerjisini açıklıyorsa plosive değil).
//  • breath: düşük-orta enerji (taban+6…+25 dB), düzlük yüksek, HF oranı orta, voiced değil, 80–600 ms (olay düzeyinde).
//  • roomtone: taban + 6 dB altı (sessiz değil). silence: mutlak −75 dBFS altı veya taban −3 dB altı.
//  • Cümle: voiced koşuları, ≥ 150 ms voiced-olmayan boşlukla bölünür (§3.4 dinamik dedektör bunu kullanır).
//  • Olaylar (sibilant/plosive/breath) koşu kapanınca süre kısıtıyla yayımlanır; kare bayrakları anlıktır.
#include "Features.h"
#include "Pitch.h"
#include <cstdint>
#include <vector>
#include <functional>

namespace sweax
{

enum VadFlag : uint32_t
{
    VAD_VOICED   = 1u << 0,
    VAD_SIBILANT = 1u << 1,
    VAD_PLOSIVE  = 1u << 2,
    VAD_BREATH   = 1u << 3,
    VAD_SILENCE  = 1u << 4,
    VAD_ROOMTONE = 1u << 5,
    VAD_BLEED    = 1u << 6,     // SC ile (Faz 8)
    VAD_TONAL    = 1u << 7,     // tonal istatistiğe uygun kare
};

struct VadFrame
{
    long frame { 0 };
    uint32_t flags { 0 };
    float noiseFloorDb { -120.0f };
    float snrDb { 0.0f };               // enerji − taban
    float sibilantRatioDb { 0.0f };     // kalibre bant oranı
    float lfJumpDb { 0.0f };            // LF zarfı − sesli LF taban çizgisi
    float pitchHz { 0.0f }, periodicity { 0.0f };
    bool has (VadFlag f) const noexcept { return (flags & f) != 0; }
};

enum class VadEventType { Sibilant, Plosive, Breath, Sentence };

struct VadEvent
{
    VadEventType type;
    long startFrame, endFrame;          // [start, end] dahil
    double startSec, endSec;
    float peakDb;                       // olay tepe enerjisi (dB)
    float centerHz;                     // sibilant: enerji ağırlık merkezi; diğerleri 0
    int voicedFrames;                   // cümle: voiced kare sayısı
};

class Vad
{
public:
    void prepare (double sampleRate, int hopSize);
    void reset();

    // her özellik karesi için; olaylar kapanınca onEvent (nullptr olabilir)
    VadFrame process (const FrameFeatures& f, const PitchTracker::Result& pitch,
                      const std::function<void (const VadEvent&)>& onEvent);
    // akış sonu: açık koşuları kapat
    void flush (const std::function<void (const VadEvent&)>& onEvent);

    float noiseFloorDb() const noexcept { return floorDb; }
    // denetim M3.4 (T5): segment SONU değeri cümle ortasında biten kayıtta 20-33 dB yüksek kalıyordu →
    // geçerli karelerdeki izleyici MİNİMUMU (dijital sıfır < −90 dBFS hariç); tabana yakın kare hiç yoksa
    // taban yalnız bir üst sınırdır (floorSupported = false)
    // denetim KA-1: dijital sıfır (ön-rulo, bölge boşluğu) bayrak izleyicisini −120'ye çekip minimuma yapışıyordu →
    // dijital kare görüldüyse ve ikinci izleyici (dijital kareleri görmez) yeterince doğrulandıysa onun minimumu
    float noiseFloorRobustDb() const noexcept
    {
        if (dijitalKare > 0 && yakinKare > dijitalKare && iz2.floorMin < 999.0f) return iz2.floorMin;
        return floorMinDb < 999.0f ? floorMinDb : floorDb;
    }
    bool floorMeasured() const noexcept { return floorMinDb < 999.0f || iz2.floorMin < 999.0f; }
    // denetim KA-3: destek kararı NEDENSEL değil — ≥200 ms koşu adayı saklanır, sorguda oturumun SON p95'iyle karşılaştırılır
    bool floorSupported() const noexcept
    {
        if (destekAdayDb > 999.0f || histTotal == 0) return false;
        uint32_t acc = 0; float p95 = -120.0f;
        for (int i = 0; i < HIST_BINS; ++i) { acc += hist[i]; if (acc * 20 >= histTotal * 19) { p95 = (float) (i - 120); break; } }
        return destekAdayDb <= p95 - 20.0f;
    }
    double roomToneSeconds() const noexcept { return roomFrames * hopSec; }
    double voicedSeconds() const noexcept { return voicedFrames * hopSec; }
    int sentenceCount() const noexcept { return sentences; }
    int sibilantEventCount() const noexcept { return sibEvents; }
    float sibilantCenterHz() const noexcept { return sibCenterHz; }
    bool sibilantCalibrated() const noexcept { return sibEvents >= 10; }

private:
    struct Run { bool active { false }; long start { 0 }; long last { 0 }; double startSec { 0.0 }, endSec { 0.0 }; float peakDb { -200.0f }; double centerAcc { 0.0 }, centerW { 0.0 }; int voiced { 0 }; };
    void updateFloor (float energyDb) noexcept;
    float sibilantRatio (const FrameFeatures& f) const noexcept;
    float sibilantCentroid (const FrameFeatures& f) const noexcept;
    void closeRun (Run& r, VadEventType type, long frame, int minFrames, int maxFrames, const std::function<void (const VadEvent&)>& onEvent);

    double sr { 48000.0 }, hopSec { 0.0213 };
    int hop { 1024 };
    // gürültü tabanı: (a) minimum-istatistik (SUBWIN × 200 ms kayan pencere) ve (b) oturum histogramı 5. yüzdelik
    // (1 dB kutu, −120…0). Uzun kesintisiz vokal cümlelerinde (a) yükselir; (b) cümleler arası sessizliği hatırlar →
    // taban = min(a, b) + sapma payı. Yükseliş hızı sınırlı (≤ 3 dB/s), düşüş anında.
    float smoothDb { -120.0f }; bool haveSmooth { false };
    static constexpr int SUBWIN = 20;
    float subMin[SUBWIN] {}; int subIdx { 0 }, subFill { 0 }, subFrames { 0 }, subLen { 9 };
    static constexpr int HIST_BINS = 121;
    uint32_t hist[HIST_BINS] {}; uint32_t histTotal { 0 };
    float floorDb { -120.0f }; bool floorValid { false }; bool haveFloor { false };
    float floorMinDb { 1000.0f }; int tabanKosu { 0 }; float destekAdayDb { 1000.0f };
    // ikinci taban izleyicisi: aynı algoritma, yalnız dijital OLMAYAN kareler (energyDb > −110)
    struct TabanIz
    {
        float smooth { -120.0f }; bool haveSmooth { false };
        float subMin[SUBWIN] {}; int subIdx { 0 }, subFill { 0 }, subFrames { 0 };
        uint32_t hist[HIST_BINS] {}; uint32_t histTotal { 0 };
        float floor { -120.0f }; bool valid { false }, have { false };
        float floorMin { 1000.0f };
        void reset() noexcept
        {
            smooth = -120.0f; haveSmooth = false; for (auto& m : subMin) m = 1000.0f; subIdx = subFill = subFrames = 0;
            for (auto& h : hist) h = 0; histTotal = 0; floor = -120.0f; valid = have = false; floorMin = 1000.0f;
        }
        void update (float e, double hopS, int subL) noexcept
        {
            const float a = (float) std::min (1.0, hopS / 0.1);
            if (! haveSmooth) { smooth = e; haveSmooth = true; } else smooth += a * (e - smooth);
            subMin[subIdx] = std::min (subMin[subIdx], smooth);
            if (++subFrames >= subL) { subFrames = 0; subIdx = (subIdx + 1) % SUBWIN; subMin[subIdx] = 1000.0f; subFill = std::min (subFill + 1, SUBWIN); }
            { const int bin = std::clamp ((int) std::lround (smooth) + 120, 0, HIST_BINS - 1); ++hist[bin]; ++histTotal; }
            float aday;
            if (subFill >= 1 || subFrames >= subL / 2)
            {
                float m = 1000.0f; for (int i = 0; i < SUBWIN; ++i) m = std::min (m, subMin[i]);
                uint32_t acc = 0; float pct = smooth;
                for (int i = 0; i < HIST_BINS; ++i) { acc += hist[i]; if (acc * 20 >= histTotal) { pct = (float) (i - 120); break; } }
                aday = std::min (m, pct) + 2.0f; valid = true;
            }
            else { aday = smooth; valid = false; }
            if (! have || aday < floor) { floor = aday; have = true; }
            else floor = std::min (aday, floor + (float) (3.0 * hopS));
            if (valid && smooth <= floor + 6.0f) floorMin = std::min (floorMin, floor);
        }
    } iz2;
    long dijitalKare { 0 }, yakinKare { 0 };
    // sesli LF taban çizgisi (median yaklaşımı: yavaş izleyici)
    float lfBaseDb { -60.0f }; bool haveLfBase { false };
    // sibilant kalibrasyonu
    float sibCenterHz { 6000.0f }; double sibCenterAcc { 0.0 }; int sibEvents { 0 };
    // sayaçlar
    long roomFrames { 0 }, voicedFrames { 0 }; int sentences { 0 };
    // koşular
    Run sib, plo, bre, sen;
    int sentenceGapFrames { 7 }, sentenceGap { 0 };
    int minSib { 1 }, maxSib { 19 }, minBre { 4 }, maxBre { 28 }, maxPlo { 4 };
    long lastFrame { -1 };
    float prevCentroid { 0.0f };
};

} // namespace sweax
