#pragma once
#include <cmath>
#include <algorithm>

// Phaser: a cascade of first-order all-pass stages whose break frequency is swept
// by an LFO, summed with the dry signal so the moving all-pass phase creates a set
// of sweeping notches. Feedback deepens the notches. 6 stages -> 3 notch pairs, the
// classic lush phaser. The L/R LFOs are 90 deg apart for stereo width. Base rate,
// post-amp.
class Phaser
{
public:
    void prepare(double sampleRate)
    {
        fs = sampleRate;
        phase = 0.0;
        clear();
    }
    void clear()
    {
        for (auto& s : stateL) s = 0.0f;
        for (auto& s : stateR) s = 0.0f;
        lastL = lastR = 0.0f;
    }

    void setParameters(float rateHz, float depth01, float feedback01, float mix)
    {
        phaseInc = twoPi * std::clamp(rateHz, 0.02f, 8.0f) / (float) fs;
        depth    = std::clamp(depth01, 0.0f, 1.0f);
        fb       = std::clamp(feedback01, 0.0f, 0.9f);
        wet      = std::clamp(mix, 0.0f, 1.0f);
    }

    void processBlock(float* L, float* R, int n)
    {
        if (wet <= 0.0001f) return;
        for (int i = 0; i < n; ++i)
        {
            phase += phaseInc;
            if (phase >= twoPi) phase -= twoPi;

            const float aL = sweepCoef(0.5f + 0.5f * std::sin((float) phase));
            const float aR = sweepCoef(0.5f + 0.5f * std::sin((float) phase + halfPi));

            L[i] = processChannel(L[i], aL, stateL, lastL);
            R[i] = processChannel(R[i], aR, stateR, lastR);
        }
    }

private:
    // LFO 0..1 -> a log sweep of the all-pass break frequency (fMin..fMax), then to
    // the one-multiply all-pass coefficient. depth scales how far the sweep travels.
    float sweepCoef(float lfo01) const
    {
        const float mod = 0.5f + depth * (lfo01 - 0.5f);       // narrow the travel by depth
        const float fc  = fMin * std::pow(fMax / fMin, mod);   // log-spaced sweep
        const float t   = std::tan(pi * std::min(fc, (float) fs * 0.49f) / (float) fs);
        return (t - 1.0f) / (t + 1.0f);                        // pole at -a, |a|<1 stable
    }

    float processChannel(float x, float a, float* state, float& last)
    {
        float v = x + fb * last;                                // feedback around the cascade
        for (int s = 0; s < kStages; ++s)
        {
            const float y = a * v + state[s];                   // one-multiply all-pass
            state[s] = v - a * y;
            v = y;
        }
        last = v;
        return x * (1.0f - wet) + v * wet;
    }

    static constexpr int kStages = 6;
    double fs = 44100.0, phase = 0.0;
    float phaseInc = 0.0f, depth = 1.0f, fb = 0.0f, wet = 0.0f;
    float stateL[kStages] { 0 }, stateR[kStages] { 0 };
    float lastL = 0.0f, lastR = 0.0f;
    static constexpr float fMin = 250.0f, fMax = 2200.0f;      // sweep range
    static constexpr float twoPi = 6.28318530718f, halfPi = 1.57079632679f, pi = 3.14159265359f;
};
