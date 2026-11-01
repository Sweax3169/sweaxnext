#pragma once
// SWEAX NEXT — Analysis/CaptureStore.h
// Worker tarafı yakalama deposu (mimari §3.1): host hızında MONO ham float, PROJE ZAMANI anahtarlı.
//  • Segment = kesintisiz proje-zaman aralığı. Yeni blok:
//      – var olan segmentin aralığına düşüyorsa (loop/seek ile yeniden geliş) ÜZERİNE YAZILIR → kanıt sayılmaz,
//        son geçiş öncekini değiştirir (test: 8 s × 10 loop → depo 8 s kalır);
//      – son segmentin ucuna bitişikse eklenir;
//      – aksi halde (seek/gap) yeni segment açılır (gapBefore).
//  • Kapasite = 100 s (90 s analiz tavanı + 10 s pay). Dolunca yeni segment kabul edilmez (full());
//    var olan aralıkların üzerine yazma devam eder. "En bilgilendirici cümle" ile yer açma → dedektörler (Faz 2 sonu).
//  • Mono seçim (MonoSelector): korelasyon ortalaması < 0.3 → yüksek RMS'li kanal; aksi halde (L+R)/2. Histerezisli.
//  • DAW projesine ASLA kaydedilmez. Yalnız worker iş parçacığı dokunur (tahsis prepare'da).
#include "CaptureFifo.h"
#include <vector>
#include <cstdint>

namespace sweax
{

class MonoSelector
{
public:
    enum class Mode { Mix, Left, Right };
    void reset() noexcept { avgCorr = 1.0f; mode = Mode::Mix; seen = 0; }
    // başlığa göre modu güncelle (yavaş ortalama + histerezis), sonra mono üret
    Mode update (const CaptureChunkHeader& h) noexcept;
    void render (const CaptureChunkHeader& h, const float* planar, float* mono) const noexcept;
    Mode currentMode() const noexcept { return mode; }
private:
    float avgCorr { 1.0f };
    Mode mode { Mode::Mix };
    int seen { 0 };
};

class CaptureStore
{
public:
    struct Segment { int64_t projectStart { 0 }; int64_t storeStart { 0 }; int64_t length { 0 }; };
    struct Ingest
    {
        bool accepted { false };
        bool overwrite { false };      // var olan aralığın (bir kısmı) üzerine yazıldı
        bool gapBefore { false };      // önceki bloktan kopuk (drop/seek/generation) → STFT/VAD sıfırla
        int64_t storeStart { -1 };     // yazılan ilk örnek (depo koordinatı)
        int64_t storeLength { 0 };
    };

    void prepare (double sampleRate, double maxSeconds = 100.0);
    void clear();

    Ingest ingest (const CaptureChunkHeader& h, const float* mono);

    double sampleRate() const noexcept { return sr; }
    const float* data() const noexcept { return buf.data(); }
    int64_t size() const noexcept { return used; }                     // dolu örnek sayısı
    int64_t capacity() const noexcept { return (int64_t) buf.size(); }
    bool full() const noexcept { return used >= (int64_t) buf.size(); }
    double seconds() const noexcept { return (double) used / sr; }
    const std::vector<Segment>& segments() const noexcept { return segs; }
    int64_t projectToStore (int64_t projectTime) const noexcept;       // -1 = yakalanmadı
    uint32_t lastGeneration() const noexcept { return lastGen; }
    uint64_t totalDroppedBlocks() const noexcept { return droppedTotal; }

private:
    double sr { 48000.0 };
    std::vector<float> buf;
    int64_t used { 0 };
    std::vector<Segment> segs;
    bool haveLast { false };
    int64_t lastProjectEnd { 0 };
    uint32_t lastGen { 0 };
    uint64_t droppedTotal { 0 };
};

} // namespace sweax
