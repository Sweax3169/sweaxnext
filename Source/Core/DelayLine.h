#pragma once
// SWEAX NEXT — Core/DelayLine.h
// Sabit tam-örnek gecikme hattı (tahsis yalnız prepare'da).
// Kullanım: lookahead'li modüllerin kuru yolu, kapalı modülün gecikme hattı (delay-compensated
// bypass), A/B kuru tamponu. Gecikme çalışırken DEĞİŞTİRİLMEZ (latency = f(sr, mod) kuralı);
// yalnız prepare'da ayarlanır. Çok kanal: kanal başına bir örnek.
#include <vector>
#include <algorithm>
#include <cstring>

namespace sweax
{

class DelayLine
{
public:
    // maxDelay: en büyük olası gecikme (constructor/prepare'da worst-case); delay: aktif gecikme
    void prepare (int maxDelaySamples, int delaySamples)
    {
        maxDelay = std::max (0, maxDelaySamples);
        buffer.assign ((size_t) maxDelay + 1, 0.0f);
        setDelay (delaySamples);
        reset();
    }
    void setDelay (int d) noexcept { delay = std::clamp (d, 0, maxDelay); }
    int  getDelay() const noexcept { return delay; }
    void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); write = 0; }

    // delay == 0 iken de geçmiş YAZILIR (okuma = yazılan örnek): gecikme daha sonra artarsa halka gerçek geçmişi
    // içerir — kısayol sürümünde 0 → D geçişi D örnek boyunca sıfır/bayat değer basıyordu (inceleme bulgusu; kazanç
    // gecikme hatlarında dB taşındığından bu doğrudan duyulur bir sıçramaydı).
    inline float processSample (float x) noexcept
    {
        const int n = (int) buffer.size();
        buffer[(size_t) write] = x;
        int read = write - delay;
        if (read < 0) read += n;
        const float y = buffer[(size_t) read];
        if (++write >= n) write = 0;
        return y;
    }
    void process (float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i) data[i] = processSample (data[i]);
    }
    // in → out, in bozulmaz
    void process (const float* in, float* out, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i) out[i] = processSample (in[i]);
    }

private:
    std::vector<float> buffer;
    int maxDelay { 0 }, delay { 0 }, write { 0 };
};

} // namespace sweax
