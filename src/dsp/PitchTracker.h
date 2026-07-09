#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// Monophonic pitch tracker (McLeod Pitch Method / normalized square difference).
// Runs at BASE rate on the clean input and aims the feedback bloom at the played
// note. NSDF + first-peak-above-threshold avoids the octave errors plain
// autocorrelation makes; parabolic interpolation gives sub-cent accuracy across
// the guitar range (verified E2..E5). getFrequency() returns 0 when unvoiced.
class PitchTracker
{
public:
    void prepare(double sampleRate)
    {
        fs     = sampleRate;
        minLag = std::max(2, (int) (fs / 1000.0));      // up to ~1000 Hz
        maxLag = std::min(W - 2, (int) (fs / 70.0));    // down to ~70 Hz
        hist.assign((size_t) W, 0.0f);
        scratch.assign((size_t) W, 0.0f);
        prefixE.assign((size_t) W + 1, 0.0f);
        nsdf.assign((size_t) maxLag + 2, 0.0f);
        widx = 0; hopCtr = 0; freqHz = 0.0f; conf = 0.0f;
    }

    void process(const float* mono, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            hist[(size_t) widx] = mono[i];
            widx = (widx + 1) % W;
            if (++hopCtr >= hop) { hopCtr = 0; analyze(); }
        }
    }

    float getFrequency()  const { return freqHz; }   // 0 == unvoiced
    float getConfidence() const { return conf; }

private:
    void analyze()
    {
        // unwrap the ring (oldest..newest) into a contiguous window
        for (int k = 0; k < W; ++k)
            scratch[(size_t) k] = hist[(size_t) ((widx + k) % W)];

        double E = 0.0;
        prefixE[0] = 0.0f;
        for (int i = 0; i < W; ++i) { E += (double) scratch[i] * scratch[i]; prefixE[i + 1] = (float) E; }
        if (E < 1e-6) { conf = 0.0f; freqHz = 0.0f; return; }   // silence -> unvoiced

        float maxPeak = 0.0f;
        for (int tau = minLag; tau <= maxLag; ++tau)
        {
            double acf = 0.0;
            for (int i = 0; i < W - tau; ++i) acf += (double) scratch[i] * scratch[i + tau];
            const double m = (double) prefixE[W - tau] + (E - (double) prefixE[tau]);
            const float v = m > 1e-9 ? (float) (2.0 * acf / m) : 0.0f;
            nsdf[(size_t) tau] = v;
            maxPeak = std::max(maxPeak, v);
        }

        // first local maximum at/above 0.8*strongest peak (rejects the sub-octave)
        const float thresh = 0.8f * maxPeak;
        int chosen = -1;
        for (int tau = minLag + 1; tau < maxLag; ++tau)
            if (nsdf[tau] > 0.0f && nsdf[tau] >= nsdf[tau - 1] && nsdf[tau] > nsdf[tau + 1]
                && nsdf[tau] >= thresh)
            { chosen = tau; break; }

        if (chosen < 0 || maxPeak < voicedThresh) { conf = maxPeak; freqHz = 0.0f; return; }

        const float a = nsdf[chosen - 1], b = nsdf[chosen], c = nsdf[chosen + 1];
        const float den   = a - 2.0f * b + c;
        const float shift = std::abs(den) > 1e-9f ? 0.5f * (a - c) / den : 0.0f;
        const float f     = (float) fs / ((float) chosen + shift);

        conf = b;
        // jump on a new note, lightly smooth otherwise to kill jitter
        if (freqHz <= 0.0f || std::abs(f - freqHz) > 0.15f * freqHz) freqHz = f;
        else                                                         freqHz += 0.35f * (f - freqHz);
    }

    double fs = 44100.0;
    static constexpr int   W            = 2048;
    static constexpr int   hop          = 256;
    static constexpr float voicedThresh = 0.6f;
    int   minLag = 44, maxLag = 630, widx = 0, hopCtr = 0;
    float freqHz = 0.0f, conf = 0.0f;
    std::vector<float> hist, scratch, prefixE, nsdf;
};
