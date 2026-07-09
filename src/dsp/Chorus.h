#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// Classic chorus: a short modulated delay per channel, with the L/R LFOs 90 deg
// apart for stereo width. Runs at base rate, post-amp.
class Chorus
{
public:
    void prepare(double sampleRate)
    {
        fs     = sampleRate;
        maxLen = (int) (0.05 * fs) + 4;         // 50 ms line
        for (auto& b : buf) b.assign((size_t) maxLen, 0.0f);
        writePos = 0; phase = 0.0;
        clear();
    }
    void clear() { for (auto& b : buf) std::fill(b.begin(), b.end(), 0.0f); }

    void setParameters(float rateHz, float depth01, float mix)
    {
        phaseInc = twoPi * std::clamp(rateHz, 0.05f, 8.0f) / (float) fs;
        depthS   = std::clamp(depth01, 0.0f, 1.0f) * 0.006f * (float) fs;   // up to 6 ms swing
        centerS  = 0.011f * (float) fs;                                     // 11 ms centre
        wet      = std::clamp(mix, 0.0f, 1.0f);
    }

    void processBlock(float* L, float* R, int n)
    {
        if (wet <= 0.0001f) return;
        for (int i = 0; i < n; ++i)
        {
            phase += phaseInc;
            if (phase >= twoPi) phase -= twoPi;
            const float dl = read(0, centerS + depthS * std::sin((float) phase));
            const float dr = read(1, centerS + depthS * std::sin((float) phase + halfPi));

            buf[0][(size_t) writePos] = L[i];
            buf[1][(size_t) writePos] = R[i];
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
    int maxLen = 2208, writePos = 0;
    std::vector<float> buf[2];
    float phaseInc = 0.0f, depthS = 0.0f, centerS = 0.0f, wet = 0.0f;
    static constexpr float twoPi = 6.28318530718f, halfPi = 1.57079632679f;
};
