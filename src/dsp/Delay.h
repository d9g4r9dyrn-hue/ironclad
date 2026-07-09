#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

// Stereo delay with feedback tone-shaping and optional ping-pong. Runs at base
// rate, post-amp. A one-pole low-pass in the feedback path darkens the repeats
// (analog-ish), and the read time is smoothed so time/tempo changes don't click.
// The processor passes the delay time already resolved to samples (free ms or
// tempo-synced), so this class stays host-agnostic.
class StereoDelay
{
public:
    void prepare(double sampleRate, double maxSeconds = 2.0)
    {
        fs     = sampleRate;
        maxLen = (int) (maxSeconds * fs) + 4;
        for (auto& b : buf) b.assign((size_t) maxLen, 0.0f);
        writePos = 0;
        smTime   = 0.0f;
        primed   = false;
        clear();
    }

    void clear()
    {
        for (auto& b : buf) std::fill(b.begin(), b.end(), 0.0f);
        lpState[0] = lpState[1] = 0.0f;
    }

    void setParameters(float timeSamples, float feedback, float toneHz, float mix, bool pingpong)
    {
        tgtTime = std::clamp(timeSamples, 1.0f, (float) (maxLen - 2));
        if (! primed) { smTime = tgtTime; primed = true; }  // no ramp-in on first set
        fb      = std::clamp(feedback, 0.0f, 0.95f);
        lpCoef  = std::exp(-2.0f * kPi * std::clamp(toneHz, 200.0f, 18000.0f) / (float) fs);
        wet     = std::clamp(mix, 0.0f, 1.0f);
        ping    = pingpong;
    }

    void processBlock(float* L, float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            smTime += 0.0008f * (tgtTime - smTime);          // glide time changes
            const float dl = readInterp(0, smTime);
            const float dr = readInterp(1, smTime);

            // darken the feedback path (one-pole low-pass)
            lpState[0] = dl + lpCoef * (lpState[0] - dl);
            lpState[1] = dr + lpCoef * (lpState[1] - dr);

            const float inL = L[i], inR = R[i];
            buf[0][(size_t) writePos] = inL + fb * (ping ? lpState[1] : lpState[0]);
            buf[1][(size_t) writePos] = inR + fb * (ping ? lpState[0] : lpState[1]);
            writePos = (writePos + 1) % maxLen;

            L[i] = inL * (1.0f - wet) + dl * wet;
            R[i] = inR * (1.0f - wet) + dr * wet;
        }
    }

private:
    float readInterp(int ch, float delaySamples)
    {
        float rp = (float) writePos - delaySamples;
        while (rp < 0.0f) rp += (float) maxLen;
        const int i0 = (int) rp;
        const float fr = rp - (float) i0;
        const int i1 = (i0 + 1) % maxLen;
        return buf[(size_t) ch][(size_t) i0] * (1.0f - fr) + buf[(size_t) ch][(size_t) i1] * fr;
    }

    double fs = 44100.0;
    int    maxLen = 88204, writePos = 0;
    std::vector<float> buf[2];
    float  tgtTime = 22050.0f, smTime = 0.0f, fb = 0.0f, lpCoef = 0.0f, wet = 0.0f;
    float  lpState[2] { 0.0f, 0.0f };
    bool   ping = false, primed = false;
    static constexpr float kPi = 3.14159265358979f;
};
