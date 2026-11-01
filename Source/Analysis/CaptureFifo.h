#pragma once
// SWEAX NEXT — Analysis/CaptureFifo.h
// Ses iş parçacığı → analiz iş parçacığı yakalama taşıyıcısı (mimari §2.2, §3.1).
//  • Üretici (ses): push() blok başına başlık + planar örnekler yazar. Yer yoksa TÜM BLOK atılır
//    (tek tek örnek değil) ve droppedBefore sayacı bir sonraki başarılı başlığa işlenir → worker
//    STFT örtüşmesini/VAD histerezisini sıfırlar ve gap işaretler.
//  • Başlık ayrı FIFO'da, örnekler ayrı FIFO'da; örnekler ÖNCE yazılır, başlık SONRA (release) →
//    tüketici başlığı görünce örnekleri kesin okuyabilir.
//  • Blok başına L/R RMS + korelasyon burada (ses iş parçacığında) hesaplanır; tahsis yok, kilit yok.
#include "../Core/Fifo.h"
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace sweax
{

struct CaptureChunkHeader
{
    uint32_t generation { 0 };      // reset/seek/prepare sayacı — değişince worker akış durumunu sıfırlar
    uint32_t droppedBefore { 0 };   // bu bloktan önce atılan blok sayısı (0 = kesintisiz)
    int64_t  playhead { -1 };       // proje örnek zamanı (host vermiyorsa iç sayaç, flags&LIVE)
    int32_t  numSamples { 0 };
    int16_t  numChannels { 1 };
    uint8_t  isPlaying { 0 };
    uint8_t  flags { 0 };
    float    rms[2] { 0.0f, 0.0f }; // kanal RMS (lineer)
    float    correlation { 1.0f };  // L/R normalize korelasyon (mono → 1)

    enum Flags : uint8_t { LIVE = 1 };
};

class CaptureFifo
{
public:
    static constexpr int MAX_CHANNELS = 2;

    // hazırlık (ses iş parçacığı dışında): seconds kadar tampon + başlık sayısı
    void prepare (double sampleRate, int maxBlock, double seconds = 2.0)
    {
        const size_t samples = (size_t) std::max (1.0, sampleRate * seconds) * MAX_CHANNELS + (size_t) maxBlock * MAX_CHANNELS;
        sampleFifo.prepare (samples);
        headerFifo.prepare ((size_t) std::max (64.0, sampleRate * seconds / 32.0));
        droppedBlocks = 0;
        liveCounter = 0;
    }

    void bumpGeneration() noexcept { ++generation; }
    uint32_t currentGeneration() const noexcept { return generation; }

    // ÜRETİCİ (ses): tüm blok ya girer ya atılır. playhead < 0 → iç sayaç (LIVE).
    bool push (const float* const* channels, int numChannels, int numSamples, int64_t playhead, bool isPlaying) noexcept
    {
        if (numSamples <= 0 || numChannels <= 0) return false;
        const int ch = std::min (numChannels, MAX_CHANNELS);
        CaptureChunkHeader h;
        h.generation = generation;
        h.numSamples = numSamples;
        h.numChannels = (int16_t) ch;
        h.isPlaying = isPlaying ? 1 : 0;
        if (playhead < 0) { h.playhead = liveCounter; h.flags |= CaptureChunkHeader::LIVE; }
        else h.playhead = playhead;
        liveCounter += numSamples;

        // meta: RMS + korelasyon
        double s0 = 0.0, s1 = 0.0, s01 = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            const double a = channels[0][i];
            s0 += a * a;
            if (ch > 1) { const double b = channels[1][i]; s1 += b * b; s01 += a * b; }
        }
        h.rms[0] = (float) std::sqrt (s0 / numSamples);
        h.rms[1] = ch > 1 ? (float) std::sqrt (s1 / numSamples) : h.rms[0];
        h.correlation = ch > 1 ? (float) ((s0 > 1e-20 && s1 > 1e-20) ? s01 / std::sqrt (s0 * s1) : 1.0) : 1.0f;

        const size_t need = (size_t) numSamples * (size_t) ch;
        const size_t freeS = sampleFifo.capacity() - sampleFifo.size();
        const size_t freeH = headerFifo.capacity() - headerFifo.size();
        if (freeS < need || freeH < 1)
        {
            ++droppedBlocks;
            return false;                    // tüm blok atıldı
        }
        h.droppedBefore = droppedBlocks;
        droppedBlocks = 0;
        for (int c = 0; c < ch; ++c) sampleFifo.push (channels[c], (size_t) numSamples);
        headerFifo.push (&h, 1);
        return true;
    }

    // TÜKETİCİ (worker): bir blok okur; dst planar (ch0[n], ch1[n]) ve en az maxSamples*2 kapasiteli olmalı.
    // Sığmayan blok (numSamples > maxSamples) atılır ve false döner (nadir; worker maxBlock ile hazırlanır).
    bool pop (CaptureChunkHeader& h, float* dst, int maxSamples) noexcept
    {
        if (headerFifo.pop (&h, 1) != 1) return false;
        const size_t need = (size_t) h.numSamples * (size_t) h.numChannels;
        if (h.numSamples > maxSamples)
        {
            float sink[256];
            size_t left = need;
            while (left > 0) { const size_t got = sampleFifo.pop (sink, std::min<size_t> (left, 256)); if (got == 0) break; left -= got; }
            return false;
        }
        size_t got = 0;
        while (got < need) { const size_t g = sampleFifo.pop (dst + got, need - got); if (g == 0) break; got += g; }
        return got == need;
    }

    size_t pendingBlocks() const noexcept { return headerFifo.size(); }
    uint32_t pendingDropped() const noexcept { return droppedBlocks; }   // yalnız üretici tarafında anlamlı

private:
    SpscFifo<float> sampleFifo;
    SpscFifo<CaptureChunkHeader> headerFifo;
    uint32_t generation { 0 };
    uint32_t droppedBlocks { 0 };   // yalnız ses iş parçacığı
    int64_t liveCounter { 0 };      // yalnız ses iş parçacığı
};

} // namespace sweax
