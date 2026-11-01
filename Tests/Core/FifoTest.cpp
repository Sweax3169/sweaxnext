// SWEAX NEXT — Tests/Core/FifoTest.cpp
#include "Cati.h"
#include "Core/Fifo.h"
#include "Core/Smoother.h"
#include <thread>
#include <numeric>

SWX_TEST (fifo_sirali_yaz_oku)
{
    sweax::SpscFifo<float> f (1000);
    SWX_BEKLE_EQ (f.capacity(), (size_t) 1024);
    float in[300]; std::iota (in, in + 300, 0.0f);
    SWX_BEKLE_EQ (f.push (in, 300), (size_t) 300);
    float out[300] {};
    SWX_BEKLE_EQ (f.pop (out, 300), (size_t) 300);
    bool ayni = true;
    for (int i = 0; i < 300; ++i) ayni = ayni && out[i] == in[i];
    SWX_BEKLE (ayni);
    SWX_BEKLE_EQ (f.pop (out, 10), (size_t) 0);
}

SWX_TEST (fifo_sarma_ve_tasma)
{
    sweax::SpscFifo<float> f (8); // kapasite 8
    float in[6] = { 1, 2, 3, 4, 5, 6 };
    f.push (in, 6);
    float out[4];
    f.pop (out, 4);                 // head=4, tail=6 → sarma bölgesine yazılacak
    float in2[6] = { 7, 8, 9, 10, 11, 12 };
    const size_t yazilan = f.push (in2, 6); // boş yer = 8-2 = 6 → hepsi sığar (sarmalı)
    SWX_BEKLE_EQ (yazilan, (size_t) 6);
    float in3[3] = { 13, 14, 15 };
    SWX_BEKLE_EQ (f.push (in3, 3), (size_t) 0); // dolu → atılır
    SWX_BEKLE_EQ (f.droppedCount(), (uint64_t) 3);
    float out2[8];
    SWX_BEKLE_EQ (f.pop (out2, 8), (size_t) 8);
    SWX_BEKLE (out2[0] == 5 && out2[1] == 6 && out2[2] == 7 && out2[7] == 12);
}

SWX_TEST (fifo_iki_is_parcacigi_veri_butunlugu)
{
    sweax::SpscFifo<uint32_t> f (4096);
    const uint32_t N = 2'000'000;
    std::thread uretici ([&] {
        uint32_t v = 0; uint32_t blok[64];
        while (v < N)
        {
            const uint32_t n = std::min<uint32_t> (64, N - v);
            for (uint32_t i = 0; i < n; ++i) blok[i] = v + i;
            uint32_t yaz = 0;
            while (yaz < n) yaz += (uint32_t) f.push (blok + yaz, n - yaz); // bekleme yok, döngü
            v += n;
        }
    });
    uint32_t beklenen = 0; bool sirali = true; uint32_t out[128];
    while (beklenen < N)
    {
        const size_t n = f.pop (out, 128);
        for (size_t i = 0; i < n; ++i) { if (out[i] != beklenen) sirali = false; ++beklenen; }
    }
    uretici.join();
    SWX_BEKLE (sirali);
    SWX_BEKLE_EQ (beklenen, N);
}

SWX_TEST (smoother_dogrusal_ve_db)
{
    sweax::LinearSmoother s; s.prepare (48000.0, 10.0); // 480 örnek
    s.setCurrentAndTarget (0.0f); s.setTarget (1.0f);
    for (int i = 0; i < 240; ++i) s.next();
    SWX_BEKLE_YAKIN (s.getCurrent(), 0.5f, 0.01f);
    s.skip (1000);
    SWX_BEKLE (! s.isSmoothing());
    SWX_BEKLE_YAKIN (s.getCurrent(), 1.0f, 1e-6f);

    sweax::DbGainSmoother g; g.prepare (48000.0, 20.0);
    g.setCurrentAndTargetDb (0.0f); g.setTargetDb (-12.0f);
    float son = 1.0f, once = 1.0f; bool monoton = true;
    for (int i = 0; i < 960; ++i) { son = g.nextLinear(); if (son > once + 1e-7f) monoton = false; once = son; }
    SWX_BEKLE (monoton);
    SWX_BEKLE_YAKIN (son, std::pow (10.0f, -12.0f / 20.0f), 1e-4f);

    sweax::OnePoleSmoother p; p.prepare (48000.0, 10.0);
    p.setCurrentAndTarget (0.0f); p.setTarget (1.0f);
    for (int i = 0; i < 480; ++i) p.next();          // 1 tau → 1-e^-1
    SWX_BEKLE_YAKIN (p.getCurrent(), 0.632f, 0.01f);
}
