#pragma once
// SWEAX NEXT — Core/Fifo.h
// Tek üretici / tek tüketici (SPSC), kilitsiz, sabit kapasiteli halka tampon.
// Kullanım: ses iş parçacığı (üretici) → analiz iş parçacığı (tüketici).
//
// GERÇEK ZAMANLI SÖZLEŞME
//  • push() asla beklemez, asla tahsis etmez. Yer yoksa fazlası ATILIR ve dropped sayacı artar;
//    ses akmaya devam eder (spec §86: worker geç kalırsa ses etkilenmez).
//  • Kapasite 2'nin kuvvetine yuvarlanır; indeksler maskeyle sarılır.
//  • Bellek sırası: üretici tail'i release ile yayınlar, tüketici acquire ile okur (ve tersi).
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cstring>

namespace sweax
{

template <typename T>
class SpscFifo
{
public:
    SpscFifo() = default;
    explicit SpscFifo (size_t minCapacity) { prepare (minCapacity); }

    // Yalnız hazırlık aşamasında (ses iş parçacığı dışında) çağrılır: tahsis burada.
    void prepare (size_t minCapacity)
    {
        size_t cap = 1;
        while (cap < std::max<size_t> (2, minCapacity)) cap <<= 1;
        buffer.assign (cap, T {});
        mask = cap - 1;
        head.store (0, std::memory_order_relaxed);
        tail.store (0, std::memory_order_relaxed);
        dropped.store (0, std::memory_order_relaxed);
    }

    size_t capacity() const noexcept { return buffer.size(); }

    // Tüketiciden bağımsız, yaklaşık doluluk (izleme için).
    size_t size() const noexcept
    {
        return tail.load (std::memory_order_acquire) - head.load (std::memory_order_acquire);
    }

    // ÜRETİCİ: n öğe yazar; sığmayanı atar. Dönüş: yazılan öğe sayısı.
    size_t push (const T* src, size_t n) noexcept
    {
        if (buffer.empty() || n == 0) return 0;
        const size_t t = tail.load (std::memory_order_relaxed);
        const size_t h = head.load (std::memory_order_acquire);
        const size_t free = buffer.size() - (t - h);
        const size_t toWrite = std::min (n, free);
        if (toWrite < n) dropped.fetch_add (n - toWrite, std::memory_order_relaxed);
        // iki parçalı kopya (sarma)
        const size_t start = t & mask;
        const size_t first = std::min (toWrite, buffer.size() - start);
        std::memcpy (buffer.data() + start, src, first * sizeof (T));
        if (toWrite > first)
            std::memcpy (buffer.data(), src + first, (toWrite - first) * sizeof (T));
        tail.store (t + toWrite, std::memory_order_release);
        return toWrite;
    }

    // TÜKETİCİ: en çok n öğe okur. Dönüş: okunan öğe sayısı.
    size_t pop (T* dst, size_t n) noexcept
    {
        if (buffer.empty() || n == 0) return 0;
        const size_t h = head.load (std::memory_order_relaxed);
        const size_t t = tail.load (std::memory_order_acquire);
        const size_t avail = t - h;
        const size_t toRead = std::min (n, avail);
        const size_t start = h & mask;
        const size_t first = std::min (toRead, buffer.size() - start);
        std::memcpy (dst, buffer.data() + start, first * sizeof (T));
        if (toRead > first)
            std::memcpy (dst + first, buffer.data(), (toRead - first) * sizeof (T));
        head.store (h + toRead, std::memory_order_release);
        return toRead;
    }

    // Tüketici tarafında sıfırlama (üretici o sırada push edebilir; yalnız sayaç anlamında güvenli).
    void clearFromConsumer() noexcept
    {
        head.store (tail.load (std::memory_order_acquire), std::memory_order_release);
    }

    uint64_t droppedCount() const noexcept { return dropped.load (std::memory_order_relaxed); }

private:
    std::vector<T> buffer;
    size_t mask { 0 };
    alignas (64) std::atomic<size_t> head { 0 };
    alignas (64) std::atomic<size_t> tail { 0 };
    std::atomic<uint64_t> dropped { 0 };
};

} // namespace sweax
