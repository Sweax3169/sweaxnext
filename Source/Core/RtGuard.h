#pragma once
// SWEAX NEXT — Core/RtGuard.h
// Gerçek zamanlı bölge koruması: processBlock giriş/çıkışında kapsam açılır; kapsam içindeyken
// global operator new/delete çağrılırsa ihlal sayacı artar (Debug/test derlemesinde ayrıca assert).
// Mimari §2.1 kural 1'in ÖLÇÜLEBİLİR hâli: "processBlock'ta tahsis yok" bir testtir, bir umut değil.
//
// Etkinleştirme: SWEAX_RT_GUARD=1 derleme tanımı (CMake: Debug ve SweaxTests). Release eklentide
// override yoktur; kapsam çağrıları o zaman boş inline'dır.
#include <cstdint>

namespace sweax::rt
{
    void enter() noexcept;                 // bu iş parçacığında RT kapsamına gir
    void exit() noexcept;
    bool active() noexcept;                // bu iş parçacığı RT kapsamında mı
    uint64_t violations() noexcept;        // proses geneli ihlal sayacı
    void resetViolations() noexcept;

    struct Scope
    {
        Scope() noexcept { enter(); }
        ~Scope() noexcept { exit(); }
        Scope (const Scope&) = delete;
        Scope& operator= (const Scope&) = delete;
    };
}
