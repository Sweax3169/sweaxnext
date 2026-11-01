#pragma once
// SWEAX NEXT — Core/Smoother.h
// Parametre yumuşatıcıları (gerçek zamanlı, tahsissiz).
//  • LinearSmoother   : sabit süreli doğrusal rampa (kazanç, mix, eşik gibi lineer alanlar)
//  • OnePoleSmoother  : üstel (RC) yumuşatma; zaman sabiti ms
//  • DbGainSmoother   : dB alanında doğrusal rampa, çıkış lineer kazanç (algısal olarak eşit adım)
// Hepsi: setTarget() her blokta çağrılabilir; next() örnek başına; skip() blok atlamak için.
#include <cmath>
#include <algorithm>

namespace sweax
{

class LinearSmoother
{
public:
    void prepare (double sampleRate, double rampMs) noexcept
    {
        rampSamples = std::max (1, (int) std::lround (sampleRate * rampMs * 0.001));
        stepsLeft = 0;
    }
    void setCurrentAndTarget (float v) noexcept { current = target = v; step = 0.0f; stepsLeft = 0; }
    void setTarget (float v) noexcept
    {
        if (v == target) return;
        target = v;
        stepsLeft = rampSamples;
        step = (target - current) / (float) rampSamples;
    }
    float next() noexcept
    {
        if (stepsLeft > 0)
        {
            current += step;
            if (--stepsLeft == 0) current = target;
        }
        return current;
    }
    void skip (int n) noexcept
    {
        if (stepsLeft <= 0) return;
        const int k = std::min (n, stepsLeft);
        current += step * (float) k;
        stepsLeft -= k;
        if (stepsLeft == 0) current = target;
    }
    bool isSmoothing() const noexcept { return stepsLeft > 0; }
    float getCurrent() const noexcept { return current; }
    float getTarget() const noexcept { return target; }

private:
    float current { 0.0f }, target { 0.0f }, step { 0.0f };
    int rampSamples { 1 }, stepsLeft { 0 };
};

class OnePoleSmoother
{
public:
    void prepare (double sampleRate, double timeConstantMs) noexcept
    {
        const double tau = std::max (1.0e-4, timeConstantMs * 0.001);
        coeff = (float) std::exp (-1.0 / (tau * sampleRate));
    }
    void setCurrentAndTarget (float v) noexcept { current = target = v; }
    void setTarget (float v) noexcept { target = v; }
    float next() noexcept
    {
        current = target + coeff * (current - target);
        // hedefin 1e-6 yakınına gelince kilitle (denormal + sonsuz yaklaşma)
        if (std::fabs (current - target) < 1.0e-6f) current = target;
        return current;
    }
    void skip (int n) noexcept
    {
        for (int i = 0; i < n && current != target; ++i) next();
    }
    bool isSmoothing() const noexcept { return current != target; }
    float getCurrent() const noexcept { return current; }
    float getTarget() const noexcept { return target; }

private:
    float current { 0.0f }, target { 0.0f }, coeff { 0.0f };
};

// dB alanında rampa: kazanç değişimi kulağa her adımda eşit gelir (lineer rampa +12 dB'de
// sondan başa doğru hızlanır, bu yüzden tercih edilmez).
class DbGainSmoother
{
public:
    void prepare (double sampleRate, double rampMs) noexcept { db.prepare (sampleRate, rampMs); }
    void setCurrentAndTargetDb (float v) noexcept { db.setCurrentAndTarget (v); cachedLin = dbToLin (v); }
    void setTargetDb (float v) noexcept { db.setTarget (v); }
    float nextLinear() noexcept
    {
        if (! db.isSmoothing()) return cachedLin;
        const float d = db.next();
        cachedLin = dbToLin (d);
        return cachedLin;
    }
    void skip (int n) noexcept { db.skip (n); cachedLin = dbToLin (db.getCurrent()); }
    bool isSmoothing() const noexcept { return db.isSmoothing(); }
    float getCurrentDb() const noexcept { return db.getCurrent(); }

    static float dbToLin (float d) noexcept { return std::pow (10.0f, d * 0.05f); }

private:
    LinearSmoother db;
    float cachedLin { 1.0f };
};

} // namespace sweax
