#pragma once
// SWEAX NEXT — Chain/ZincirSirasi.h
// M3.5: preset başına ÖZEL zincir sırası (sahip: "her biri için özel sıralama").
//  • GATE her zaman başta; FX kuyruğu (DBL → DLY → REV → WID), çıkış trimi ve limiter her zaman sonda.
//  • Aradaki EKLEME modülleri swx.pos.* parametreleriyle (0..15) sıralanır; eşit konumda varsayılan sıra korunur
//    (kararlı sıralama) → eski oturumlar (tüm konumlar varsayılan) birebir eski zinciri çalar.
//  • Toplam gecikme sıradan bağımsızdır (toplam); sıra değişimi RT'de tahsissizdir.
#include "../Core/ParamIds.h"
#include <array>
#include <cstdint>

namespace sweax
{

// M3.5: Yumusat (spektral rezonans), Formant (spektral zarf), TonEq (preset ton EQ'su) — SONA eklendi: eski oturumların
// konum parametreleri (swx.pos.*) aynı anlamı taşır; yeni modüllerin varsayılan konumları 12/13/14 (kapalı gelir).
enum class Ek : uint8_t { Tune, Eq, Lev, CompA, Ds, CompB, Glow, Sat, Dirt, Air, Ds2, Layer, Yumusat, Formant, TonEq };
constexpr int EK_SAYISI = 15;
using Sira = std::array<uint8_t, EK_SAYISI>;

inline constexpr std::array<params::Id, EK_SAYISI> EK_KONUM_PARAM = {
    params::PosTune, params::PosEq, params::PosLev, params::PosCompA, params::PosDs, params::PosCompB,
    params::PosGlow, params::PosSat, params::PosDirt, params::PosAir, params::PosDs2, params::PosLayer,
    params::PosSoo, params::PosFrm, params::PosTeq
};

constexpr Sira varsayilanSira() noexcept
{
    Sira s {};
    for (int i = 0; i < EK_SAYISI; ++i) s[(size_t) i] = (uint8_t) i;
    return s;
}

// jenerik preset sırası (M3.5): ton EQ düzeltici EQ'nun hemen ardında — M3.4'te stil bantları düzeltici EQ'nun içindeydi;
// doğrusal iki EQ art arda = eski tek EQ (kimlik bozulmaz). Varsayılan KONUMLAR (eski oturum uyumu) varsayilanSira'dır.
constexpr Sira jenerikSira() noexcept
{
    return Sira { (uint8_t) Ek::Tune, (uint8_t) Ek::Eq, (uint8_t) Ek::TonEq, (uint8_t) Ek::Lev, (uint8_t) Ek::CompA, (uint8_t) Ek::Ds,
                  (uint8_t) Ek::CompB, (uint8_t) Ek::Glow, (uint8_t) Ek::Sat, (uint8_t) Ek::Dirt, (uint8_t) Ek::Air, (uint8_t) Ek::Ds2,
                  (uint8_t) Ek::Layer, (uint8_t) Ek::Yumusat, (uint8_t) Ek::Formant };
}

// konumlar (Ek indeksine göre) → işleme sırası; kararlı ekleme sıralaması
inline Sira siraHesapla (const std::array<float, EK_SAYISI>& konum) noexcept
{
    Sira s = varsayilanSira();
    for (int i = 1; i < EK_SAYISI; ++i)
    {
        const uint8_t e = s[(size_t) i];
        int j = i - 1;
        while (j >= 0 && konum[s[(size_t) j]] > konum[e]) { s[(size_t) (j + 1)] = s[(size_t) j]; --j; }
        s[(size_t) (j + 1)] = e;
    }
    return s;
}

// işleme sırası → konum değerleri (plan yayını / preset)
inline std::array<float, EK_SAYISI> siraKonumlari (const Sira& s) noexcept
{
    std::array<float, EK_SAYISI> k {};
    for (int i = 0; i < EK_SAYISI; ++i) k[s[(size_t) i]] = (float) i;
    return k;
}

// geçerli bir permütasyon mu (her Ek tam bir kez)
inline bool siraGecerli (const Sira& s) noexcept
{
    std::array<bool, EK_SAYISI> gor {};
    for (auto e : s)
    {
        if (e >= EK_SAYISI || gor[e]) return false;
        gor[e] = true;
    }
    return true;
}

} // namespace sweax
