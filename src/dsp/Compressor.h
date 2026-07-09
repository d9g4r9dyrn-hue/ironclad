#pragma once
#include <cmath>
#include <algorithm>

// Stereo-linked feed-forward compressor / limiter with a soft knee, log-domain
// attack/release, makeup gain, and a parallel MIX. Ratio sweeps from gentle glue
// (2:1) up to brickwall limiting (20:1+). Parallel mix blends the compressed
// signal back with the dry so you get "loud softs / soft louds" without crushing
// transients. Placed on the output bus (after the FX).
class Compressor
{
public:
    void prepare(double sampleRate)
    {
        fs = sampleRate;
        setTimes(10.0f, 120.0f);
        env = 0.0f;
    }
    void clear() { env = 0.0f; }

    void setParameters(float threshDb, float ratio, float attackMs, float releaseMs,
                       float makeupDb, float mix)
    {
        thr    = threshDb;
        ratioR = std::max(1.0f, ratio);
        setTimes(attackMs, releaseMs);
        makeup = std::pow(10.0f, makeupDb / 20.0f);
        wet    = std::clamp(mix, 0.0f, 1.0f);
    }

    void processBlock(float* L, float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float side = std::max(std::abs(L[i]), std::abs(R[i]));
            const float dB   = 20.0f * std::log10(std::max(side, 1.0e-6f));

            // soft-knee static gain reduction (dB, >= 0)
            float grDb = 0.0f;
            const float over = dB - thr;
            if      (over >= kHalfKnee) grDb = over * (1.0f - 1.0f / ratioR);
            else if (over > -kHalfKnee)                                   // inside the knee
            {
                const float x = over + kHalfKnee;                        // 0 .. knee
                grDb = (1.0f - 1.0f / ratioR) * (x * x) / (2.0f * kKnee);
            }

            // attack when clamping down harder, release when backing off
            env += (grDb > env ? atk : rel) * (grDb - env);

            const float g = std::pow(10.0f, -env / 20.0f) * makeup;
            L[i] = L[i] * (1.0f - wet) + (L[i] * g) * wet;
            R[i] = R[i] * (1.0f - wet) + (R[i] * g) * wet;
        }
    }

private:
    void setTimes(float aMs, float rMs)
    {
        atk = 1.0f - std::exp(-1.0f / (float) (0.001 * std::max(0.05f, aMs) * fs));
        rel = 1.0f - std::exp(-1.0f / (float) (0.001 * std::max(1.0f,  rMs) * fs));
    }
    double fs = 44100.0;
    float thr = -12.0f, ratioR = 4.0f, makeup = 1.0f, wet = 1.0f;
    float atk = 0.0f, rel = 0.0f, env = 0.0f;
    static constexpr float kKnee = 6.0f, kHalfKnee = 3.0f;
};
