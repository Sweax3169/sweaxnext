// SWEAX NEXT — Tests/Analysis/CaptureTest.cpp
// Yakalama taşıyıcısı: tüm blok atma + droppedBefore; depo: loop üzerine yazma, seek segmenti, kapasite.
#include "Cati.h"
#include "Analysis/CaptureFifo.h"
#include "Analysis/CaptureStore.h"
#include "Core/RtGuard.h"
#include <vector>
#include <thread>
#include <atomic>

SWX_TEST (capture_fifo_blok_atma_ve_meta)
{
    sweax::CaptureFifo f; f.prepare (48000.0, 512, 0.05);            // ~2400 örnek + 512 → birkaç blok
    std::vector<float> L (512), R (512);
    for (int i = 0; i < 512; ++i) { L[(size_t) i] = 0.5f * (float) std::sin (0.1 * i); R[(size_t) i] = -L[(size_t) i]; }
    const float* ch[2] = { L.data(), R.data() };
    int kabul = 0;
    {
        sweax::rt::Scope rt;                                            // ses tarafı: tahsis yok
        for (int b = 0; b < 20; ++b) if (f.push (ch, 2, 512, (int64_t) b * 512, true)) ++kabul;
    }
    SWX_BEKLE_EQ (sweax::rt::violations(), 0u);
    SWX_BEKLE (kabul >= 3 && kabul < 20);                              // taşma: bloklar bütün olarak atıldı
    SWX_BEKLE (f.pendingDropped() > 0);
    // tüketici: kabul edilenler eksiksiz ve sıralı
    std::vector<float> dst (1024);
    sweax::CaptureChunkHeader h; int okunan = 0;
    while (f.pop (h, dst.data(), 512))
    {
        SWX_BEKLE_EQ (h.numSamples, 512); SWX_BEKLE_EQ ((int) h.numChannels, 2);
        SWX_BEKLE_EQ (h.playhead, (int64_t) okunan * 512);
        SWX_BEKLE_EQ (h.droppedBefore, 0u);                            // atılanlar sona düştü, öncekiler kesintisiz
        SWX_BEKLE_YAKIN (dst[10], L[10], 1e-7); SWX_BEKLE_YAKIN (dst[512 + 10], R[10], 1e-7);
        SWX_BEKLE_YAKIN (h.correlation, -1.0f, 1e-5f);                 // R = −L
        SWX_BEKLE_YAKIN (h.rms[0], h.rms[1], 1e-6f);
        ++okunan;
    }
    SWX_BEKLE_EQ (okunan, kabul);
    // atmadan sonra ilk başarılı push droppedBefore taşır
    SWX_BEKLE (f.push (ch, 2, 512, 999, true));
    SWX_BEKLE (f.pop (h, dst.data(), 512));
    SWX_BEKLE (h.droppedBefore > 0);
    SWX_BEKLE_EQ (h.playhead, (int64_t) 999);
    // playhead yok → LIVE + iç sayaç
    SWX_BEKLE (f.push (ch, 1, 512, -1, false));
    SWX_BEKLE (f.pop (h, dst.data(), 512));
    SWX_BEKLE ((h.flags & sweax::CaptureChunkHeader::LIVE) != 0);
    SWX_BEKLE_EQ ((int) h.numChannels, 1);
    SWX_BEKLE_YAKIN (h.correlation, 1.0f, 1e-6f);
}

SWX_TEST (capture_fifo_iki_is_parcacigi)
{
    sweax::CaptureFifo f; f.prepare (48000.0, 256, 0.5);
    const int BLOK = 400;
    std::vector<float> src (256); for (int i = 0; i < 256; ++i) src[(size_t) i] = (float) i;
    const float* ch[1] = { src.data() };
    std::thread uretici ([&] { for (int b = 0; b < BLOK; ++b) { while (! f.push (ch, 1, 256, (int64_t) b * 256, true)) std::this_thread::yield(); } });
    std::vector<float> dst (512); sweax::CaptureChunkHeader h; int64_t beklenen = 0; bool bozuk = false; int say = 0;
    while (say < BLOK)
    {
        if (! f.pop (h, dst.data(), 256)) { std::this_thread::yield(); continue; }
        // üretici dolu FIFO'da aynı bloğu yeniden dener → droppedBefore > 0 olabilir (gerçek kullanımda blok gider);
        // bütünlük ölçütü: sıra ve içerik
        if (h.playhead != beklenen || dst[100] != 100.0f) bozuk = true;
        beklenen += 256; ++say;
    }
    uretici.join();
    SWX_BEKLE (! bozuk); SWX_BEKLE_EQ (say, BLOK);
}

namespace
{
    sweax::CaptureChunkHeader hdr (int64_t ph, int n, uint32_t gen = 0, uint32_t dropped = 0, bool playing = true)
    {
        sweax::CaptureChunkHeader h; h.playhead = ph; h.numSamples = n; h.numChannels = 1; h.generation = gen;
        h.droppedBefore = dropped; h.isPlaying = playing ? 1 : 0; return h;
    }
}

SWX_TEST (capture_store_loop_ustune_yazar_seek_segment_acar)
{
    sweax::CaptureStore s; s.prepare (1000.0, 20.0);                    // 1 kHz "örnekleme", 20 s kapasite
    std::vector<float> blk (100);
    // 8 s loop × 10 geçiş, her geçişte değer = geçiş numarası
    for (int pass = 1; pass <= 10; ++pass)
        for (int b = 0; b < 80; ++b)
        {
            std::fill (blk.begin(), blk.end(), (float) pass);
            const auto r = s.ingest (hdr ((int64_t) b * 100, 100), blk.data());
            SWX_BEKLE (r.accepted);
            if (pass > 1) SWX_BEKLE (r.overwrite);
            if (pass > 1 && b == 0) SWX_BEKLE (r.gapBefore);           // loop başı = zaman sıçraması
            if (b > 0) SWX_BEKLE (! r.gapBefore);
        }
    SWX_BEKLE_EQ (s.size(), (int64_t) 8000);                           // depo 8 s kaldı, 80 s değil
    SWX_BEKLE_EQ ((int) s.segments().size(), 1);
    SWX_BEKLE_YAKIN (s.data()[4321], 10.0f, 0.0f);                     // son geçiş öncekini değiştirdi
    SWX_BEKLE_EQ (s.projectToStore (4321), (int64_t) 4321);
    // seek → yeni segment, gap
    std::fill (blk.begin(), blk.end(), 77.0f);
    auto r = s.ingest (hdr (50000, 100), blk.data());
    SWX_BEKLE (r.accepted && r.gapBefore && ! r.overwrite);
    SWX_BEKLE_EQ ((int) s.segments().size(), 2);
    SWX_BEKLE_EQ (s.projectToStore (50050), (int64_t) 8050);
    SWX_BEKLE_EQ (s.projectToStore (30000), (int64_t) -1);
    // bitişik devam → segment uzar
    r = s.ingest (hdr (50100, 100), blk.data());
    SWX_BEKLE (r.accepted && ! r.gapBefore);
    SWX_BEKLE_EQ (s.segments().back().length, (int64_t) 200);
    // drop → gap işareti
    r = s.ingest (hdr (50200, 100, 0, 3), blk.data());
    SWX_BEKLE (r.gapBefore); SWX_BEKLE_EQ (s.totalDroppedBlocks(), (uint64_t) 3);
    // duran host → kabul edilmez
    r = s.ingest (hdr (50300, 100, 0, 0, false), blk.data());
    SWX_BEKLE (! r.accepted);
    // generation değişimi → gap
    r = s.ingest (hdr (50300, 100, 1), blk.data());
    SWX_BEKLE (r.accepted && r.gapBefore);
    // kapasite: 20 s dolunca yeni segment reddedilir, üzerine yazma sürer
    for (int b = 0; b < 200; ++b) s.ingest (hdr (100000 + (int64_t) b * 100, 100, 1), blk.data());
    SWX_BEKLE (s.full());
    SWX_BEKLE_EQ (s.size(), (int64_t) 20000);
    r = s.ingest (hdr (900000, 100, 1), blk.data());
    SWX_BEKLE (! r.accepted);
    std::fill (blk.begin(), blk.end(), 5.0f);
    r = s.ingest (hdr (0, 100, 1), blk.data());
    SWX_BEKLE (r.accepted && r.overwrite);
    SWX_BEKLE_YAKIN (s.data()[50], 5.0f, 0.0f);
}

SWX_TEST (capture_mono_secici)
{
    sweax::MonoSelector m; m.reset();
    sweax::CaptureChunkHeader h; h.numChannels = 2; h.numSamples = 4800; h.rms[0] = 0.1f; h.rms[1] = 0.3f;
    h.correlation = 0.95f;
    for (int i = 0; i < 20; ++i) m.update (h);
    SWX_BEKLE (m.currentMode() == sweax::MonoSelector::Mode::Mix);
    h.correlation = 0.05f;                                             // gerçek stereo → yüksek RMS'li kanal (R)
    for (int i = 0; i < 40; ++i) m.update (h);
    SWX_BEKLE (m.currentMode() == sweax::MonoSelector::Mode::Right);
    float L[4] = { 1, 2, 3, 4 }, R[4] = { 10, 20, 30, 40 }; float planar[8]; std::copy (L, L + 4, planar); std::copy (R, R + 4, planar + 4);
    h.numSamples = 4; float mono[4]; m.render (h, planar, mono);
    SWX_BEKLE_YAKIN (mono[2], 30.0f, 0.0f);
    h.correlation = 0.35f; h.numSamples = 4800;                        // histerezis: 0.3–0.4 arası mod değişmez
    for (int i = 0; i < 40; ++i) m.update (h);
    SWX_BEKLE (m.currentMode() == sweax::MonoSelector::Mode::Right);
    h.correlation = 0.9f; for (int i = 0; i < 40; ++i) m.update (h);
    SWX_BEKLE (m.currentMode() == sweax::MonoSelector::Mode::Mix);
    h.numSamples = 4; m.render (h, planar, mono);
    SWX_BEKLE_YAKIN (mono[2], 16.5f, 0.0f);
}
