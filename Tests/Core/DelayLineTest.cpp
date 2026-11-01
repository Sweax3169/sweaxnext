// SWEAX NEXT — Tests/Core/DelayLineTest.cpp
#include "Cati.h"
#include "Core/DelayLine.h"
#include "Core/ParamIds.h"
#include <string>

SWX_TEST (delayline_tam_ornek_gecikme)
{
    sweax::DelayLine d; d.prepare (512, 37);
    float x[256]; for (int i = 0; i < 256; ++i) x[i] = (float) i;
    float y[256];
    d.process (x, y, 256);
    bool ok = true;
    for (int i = 0; i < 37; ++i) ok = ok && y[i] == 0.0f;
    for (int i = 37; i < 256; ++i) ok = ok && y[i] == (float) (i - 37);
    SWX_BEKLE (ok);
    // sıfır gecikme = geçiş
    sweax::DelayLine z; z.prepare (16, 0);
    z.process (x, y, 256);
    SWX_BEKLE (y[0] == 0.0f && y[255] == 255.0f);
    // klemp
    sweax::DelayLine k; k.prepare (10, 99);
    SWX_BEKLE_EQ (k.getDelay(), 10);
}

SWX_TEST (paramids_tablo_tutarliligi)
{
    using namespace sweax::params;
    // kimlikler benzersiz ve "swx." ile başlar
    bool benzersiz = true, onek = true;
    for (size_t i = 0; i < TABLO.size(); ++i)
    {
        onek = onek && TABLO[i].id.substr (0, 4) == "swx.";
        for (size_t j = i + 1; j < TABLO.size(); ++j) if (TABLO[i].id == TABLO[j].id) benzersiz = false;
        SWX_BEKLE (TABLO[i].min <= TABLO[i].def && TABLO[i].def <= TABLO[i].max);
        SWX_BEKLE (! TABLO[i].ad.empty());
    }
    SWX_BEKLE (benzersiz);
    SWX_BEKLE (onek);
    SWX_BEKLE_EQ ((int) TABLO.size(), (int) SAYI);
    // EQ bant indeksleme
    SWX_BEKLE (TABLO[(size_t) eqBant (0, 2)].id == "swx.eq.b1.hz");
    SWX_BEKLE (TABLO[(size_t) eqBant (5, 0)].id == "swx.eq.b6.on");
    // makro etiketli parametrelerde nötr değer aralık içinde
    for (auto& t : TABLO)
        if (t.makro != Makro::Yok) SWX_BEKLE (t.min <= t.notr && t.notr <= t.max);
}
