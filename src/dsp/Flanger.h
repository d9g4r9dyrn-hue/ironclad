#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// Flanger: a short modulated delay (roughly 1-7 ms) with regeneration (feedback),
// producing the classic sweeping comb-filter "jet" sound. Shorter delay + feedback
// is what separates it from the chorus. The L/R LFOs are 90 deg apart for width.
// Runs at base rate, post-amp.
class Flanger
{
public:
    void prepare(double sampleRate)
    {
        fs     = sampleRate;
        maxLen = (int) (0.012 * fs) + 4;        // 12 ms line (headroom over the 7 ms max sweep)
        for (auto& b : buf) b.assign((size_t) maxLen, 0.0f);
        writePos = 0; phase = 0.0;
        clear();
    }
    void clear() { for (auto& b : buf) std::fill(b.begin(), b.end(), 0.0f); }

    void setParameters(float rateHz, float depth01, float feedback01, float mix)
    {
        phaseInc = twoPi * std::clamp(rateHz, 0.02f, 8.0f) / (float) fs;
        const float d = std::clamp(depth01, 0.0f, 1.0f);
        centerS  = 0.0018f * (float) fs;                    // ~1.8 ms centre
        depthS   = d * 0.0045f * (float) fs;                // up to ~4.5 ms swing
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
            const float dl = read(0, centerS + depthS * (0.5f + 0.5f * std::sin((float) phase)));
            const float dr = read(1, centerS + depthS * (0.5f + 0.5f * std::sin((float) phase + halfPi)));

            // Regeneration: feed the delayed output back into the line (bounded).
            buf[0][(size_t) writePos] = std::clamp(L[i] + dl * fb, -4.0f, 4.0f);
            buf[1][(size_t) writePos] = std::clamp(R[i] + dr * fb, -4.0f, 4.0f);
            writePos = (writePos + 1) % maxLen;

            L[i] = L[i] * (1.0f - wet) + dl * wet;
            R[i] = R[i] * (1.0f - wet) + dr * wet;
        }
    }

private:
    float read(int ch, float delayS)
    {
        float rp = (float) writePos - delayS;
        while (rp < 0.0f) rp += (float) maxLen;
        const int i0 = (int) rp;
        const float fr = rp - (float) i0;
        const int i1 = (i0 + 1) % maxLen;
        return buf[(size_t) ch][(size_t) i0] * (1.0f - fr) + buf[(size_t) ch][(size_t) i1] * fr;
    }

    double fs = 44100.0, phase = 0.0;
    int maxLen = 532, writePos = 0;
    std::vector<float> buf[2];
    float phaseInc = 0.0f, depthS = 0.0f, centerS = 0.0f, fb = 0.0f, wet = 0.0f;
    static constexpr float twoPi = 6.28318530718f, halfPi = 1.57079632679f;
};
