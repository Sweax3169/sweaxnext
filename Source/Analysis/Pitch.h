#pragma once
// SWEAX NEXT — Analysis/Pitch.h
// Temel frekans: YIN / CMND (de Cheveigné & Kawahara 2002) + tamsayı minimum çevresinde parabolik ara değer.
// Mimari §3.4: hedef ±3 cent (80–1000 Hz), oktav hatası <2 %; zero-crossing YOK. Pencere ≥ 2× en uzun periyot.
//  • analyze(x, n): n ≥ frameSize(). Dönüş: hz (0 = sessiz/tonal değil), periodicity = 1 − d'(τ) (0..1), voiced.
//  • ÖLÇÜLMÜŞ KARAR (Tests/Analysis/PitchTest + sonda, 44.1/48 kHz, 80–1000 Hz, 12 harmonik + formant zarfı):
//      – tamsayı d(τ) üzerinde parabolik: 30 dB SNR maks 0.64 cent, sapma ≈ 0; 15 dB SNR maks ≤ 5 cent, ort ≈ 1 cent
//      – 8× band-sınırlı (Kaiser-sinc polifaz) kesirli ızgara: 15 dB SNR'de 7–11 cent'e varan SİSTEMATİK sapma;
//        neden: her fazın gürültü kazancı Σh_p² farklı → d(τ) gürültü tabanı ızgara boyunca dalgalanır ve minimumu
//        yarım-örneğe çeker. Gürültü tabanı eşitleme (σ²W·(1−Σh²)) bunu yalnız kısmen giderdi (maks 9 cent).
//      – doğrusal ara değerli 8× ızgara: 1 kHz'de +0.3 örnek / −11 cent (en kötü).
//    Bu yüzden üretimde yalnız tamsayı-parabolik kullanılır; tüm gecikmelerin gürültü terimi özdeştir (2Wσ²).
//  • Oktav koruması: ilk eşik-altı çukur (YIN adım 4) + önceki kararlı perdeye süreklilik tercihi (pYIN ruhu, hafif).
//  • Tahsis yalnız prepare()'da; worker iş parçacığında çalışır (RT'de kullanılmaz).
#include <vector>
#include <cmath>

namespace sweax
{

class PitchTracker
{
public:
    struct Result { float hz { 0.0f }; float periodicity { 0.0f }; bool voiced { false }; };

    void prepare (double sampleRate, double minHz = 60.0, double maxHz = 1000.0, float threshold = 0.12f);
    void reset() noexcept { prevHz = 0.0f; stable = 0; }

    int frameSize() const noexcept { return W + maxLag; }         // gerekli örnek sayısı
    int hop() const noexcept { return W / 2; }
    Result analyze (const float* x, int n) noexcept;

    // yardımcı: cent farkı
    static float cents (double a, double b) noexcept { return (float) (1200.0 * std::log2 (a / b)); }

private:
    double refineLag (int tau) const noexcept;                     // parabolik (d[τ−1], d[τ], d[τ+1])
    double sr { 48000.0 };
    int W { 800 }, minLag { 48 }, maxLag { 800 };
    float thr { 0.12f };
    std::vector<double> d, cmnd;
    float prevHz { 0.0f };
    int stable { 0 };
};

} // namespace sweax
