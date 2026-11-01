// SWEAX NEXT — Core/RtGuard.cpp
#include "RtGuard.h"
#include <atomic>
#include <cstdlib>
#include <new>

namespace
{
    thread_local int rtDepth = 0;
    std::atomic<uint64_t> rtViolations { 0 };
}

namespace sweax::rt
{
    void enter() noexcept { ++rtDepth; }
    void exit() noexcept { if (rtDepth > 0) --rtDepth; }
    bool active() noexcept { return rtDepth > 0; }
    uint64_t violations() noexcept { return rtViolations.load (std::memory_order_relaxed); }
    void resetViolations() noexcept { rtViolations.store (0, std::memory_order_relaxed); }
}

#if defined (SWEAX_RT_GUARD) && SWEAX_RT_GUARD
// Global tahsis kancaları: RT kapsamında tahsis = ihlal. Tahsisi ENGELLEMEZ (davranışı değiştirmez),
// yalnız sayar; testler sayacı kontrol eder, Debug'da assert'e çevrilebilir.
namespace
{
    inline void kaydet() noexcept
    {
        if (rtDepth > 0) rtViolations.fetch_add (1, std::memory_order_relaxed);
    }
}
void* operator new (std::size_t n)
{
    kaydet();
    if (void* p = std::malloc (n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t n)
{
    kaydet();
    if (void* p = std::malloc (n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new (std::size_t n, const std::nothrow_t&) noexcept { kaydet(); return std::malloc (n ? n : 1); }
void* operator new[] (std::size_t n, const std::nothrow_t&) noexcept { kaydet(); return std::malloc (n ? n : 1); }
void operator delete (void* p) noexcept { kaydet(); std::free (p); }
void operator delete[] (void* p) noexcept { kaydet(); std::free (p); }
void operator delete (void* p, std::size_t) noexcept { kaydet(); std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { kaydet(); std::free (p); }
#endif
