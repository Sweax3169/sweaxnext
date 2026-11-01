// SWEAX NEXT — Analysis/CaptureStore.cpp
#include "CaptureStore.h"
#include <algorithm>
#include <cstring>

namespace sweax
{

MonoSelector::Mode MonoSelector::update (const CaptureChunkHeader& h) noexcept
{
    if (h.numChannels < 2) { mode = Mode::Mix; return mode; }
    // ~1 s zaman sabiti (blok sayısına göre değil; örnek sayısına göre)
    const float alpha = std::min (1.0f, (float) h.numSamples / 48000.0f);
    if (seen == 0) avgCorr = h.correlation; else avgCorr += alpha * (h.correlation - avgCorr);
    ++seen;
    if (mode == Mode::Mix) { if (avgCorr < 0.3f) mode = (h.rms[0] >= h.rms[1]) ? Mode::Left : Mode::Right; }
    else if (avgCorr > 0.4f) mode = Mode::Mix;
    return mode;
}

void MonoSelector::render (const CaptureChunkHeader& h, const float* planar, float* mono) const noexcept
{
    const int n = h.numSamples;
    if (h.numChannels < 2) { std::memcpy (mono, planar, (size_t) n * sizeof (float)); return; }
    const float* L = planar; const float* R = planar + n;
    switch (mode)
    {
        case Mode::Left:  std::memcpy (mono, L, (size_t) n * sizeof (float)); break;
        case Mode::Right: std::memcpy (mono, R, (size_t) n * sizeof (float)); break;
        default: for (int i = 0; i < n; ++i) mono[i] = 0.5f * (L[i] + R[i]); break;
    }
}

void CaptureStore::prepare (double sampleRate, double maxSeconds)
{
    sr = sampleRate;
    buf.assign ((size_t) std::max (1.0, sampleRate * maxSeconds), 0.0f);
    segs.reserve (256);
    clear();
}

void CaptureStore::clear()
{
    used = 0; segs.clear(); haveLast = false; lastProjectEnd = 0; droppedTotal = 0;
}

int64_t CaptureStore::projectToStore (int64_t t) const noexcept
{
    for (const auto& s : segs)
        if (t >= s.projectStart && t < s.projectStart + s.length) return s.storeStart + (t - s.projectStart);
    return -1;
}

CaptureStore::Ingest CaptureStore::ingest (const CaptureChunkHeader& h, const float* mono)
{
    Ingest r;
    const int64_t n = h.numSamples;
    if (n <= 0) return r;
    droppedTotal += h.droppedBefore;
    const bool broken = (! haveLast) || h.droppedBefore > 0 || h.generation != lastGen || h.playhead != lastProjectEnd;
    r.gapBefore = broken;
    lastGen = h.generation;
    if (! h.isPlaying && ! (h.flags & CaptureChunkHeader::LIVE)) { haveLast = false; return r; }   // duran host: yakalama yok

    // 1) var olan segmentle örtüşme → üzerine yaz (loop/seek). Örtüşen kısım yazılır; taşan kısım aşağıda ele alınır.
    int64_t p = h.playhead; int64_t off = 0;
    for (auto& s : segs)
    {
        if (p >= s.projectStart && p < s.projectStart + s.length)
        {
            const int64_t inSeg = p - s.projectStart;
            const int64_t can = std::min (n - off, s.length - inSeg);
            std::memcpy (buf.data() + s.storeStart + inSeg, mono + off, (size_t) can * sizeof (float));
            r.accepted = true; r.overwrite = true;
            if (r.storeStart < 0) r.storeStart = s.storeStart + inSeg;
            r.storeLength += can;
            off += can; p += can;
            // segment deponun ucundaysa ve blok taşıyorsa segment uzatılır
            if (off < n && s.storeStart + s.length == used)
            {
                const int64_t room = std::min (n - off, (int64_t) buf.size() - used);
                if (room > 0)
                {
                    std::memcpy (buf.data() + used, mono + off, (size_t) room * sizeof (float));
                    s.length += room; used += room; off += room; p += room; r.storeLength += room;
                }
            }
            break;
        }
    }
    // 2) bitişik ekleme (son segmentin ucu)
    if (off < n && ! segs.empty())
    {
        auto& s = segs.back();
        if (p == s.projectStart + s.length && s.storeStart + s.length == used)
        {
            const int64_t room = std::min (n - off, (int64_t) buf.size() - used);
            if (room > 0)
            {
                std::memcpy (buf.data() + used, mono + off, (size_t) room * sizeof (float));
                if (r.storeStart < 0) r.storeStart = used;
                s.length += room; used += room; r.storeLength += room; off += room; p += room; r.accepted = true;
            }
        }
    }
    // 3) yeni segment (seek/gap) — kapasite varsa
    if (off < n)
    {
        const int64_t room = std::min (n - off, (int64_t) buf.size() - used);
        if (room > 0)
        {
            Segment s; s.projectStart = p; s.storeStart = used; s.length = room;
            std::memcpy (buf.data() + used, mono + off, (size_t) room * sizeof (float));
            if (r.storeStart < 0) r.storeStart = used;
            segs.push_back (s); used += room; r.storeLength += room; off += room; p += room; r.accepted = true;
            r.gapBefore = true;
        }
    }
    haveLast = true;
    lastProjectEnd = h.playhead + n;     // proje zamanı akmaya devam eder (kapasite dolsa da)
    return r;
}

} // namespace sweax
