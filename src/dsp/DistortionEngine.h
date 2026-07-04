#pragma once
#include "Biquad.h"
#include "Waveshaper.h"
#include <algorithm>
#include <cmath>

struct DistortionParams
{
    int   type     = 0;         // Waveshaper::Mode
    float drive    = 0.5f;      // 0..1
    float bass     = 0.0f;      // dB, -12..+12
    float mid      = 0.0f;      // dB, -12..+12
    float treble   = 0.0f;      // dB, -12..+12
    float presence = 0.5f;      // 0..1 -> post high-shelf brightness
    float tight    = 80.0f;     // Hz, pre-drive high-pass (low-cut)
    float highCut  = 20000.0f;  // Hz, post-chain low-pass (high-cut); 20k ~= open
    float mix      = 1.0f;      // 0..1 dry/wet
    float output   = 1.0f;      // 0..2 wet makeup gain, applied pre-mix
    float level    = 1.0f;      // linear master trim, applied post-mix
    bool  gate     = false;     // input noise gate on/off
};

// Stereo-linked noise gate placed ahead of the drive stage, so hiss is silenced
// before the waveshaper amplifies it. Hysteresis (separate open/close thresholds)
// keeps it from chattering on signals hovering right at the threshold, and the
// gain ramp (fast attack, slow release) keeps transitions click-free.
class NoiseGate
{
public:
    void prepare(double sampleRate)
    {
        fs         = sampleRate;
        envRelCoef = std::exp(-1.0f / (float) (0.010 * fs)); // 10 ms envelope decay
        attCoef    = 1.0f - std::exp(-1.0f / (float) (0.002 * fs)); //   2 ms open ramp
        relCoef    = 1.0f - std::exp(-1.0f / (float) (0.120 * fs)); // 120 ms close ramp
        clear();
    }

    void clear() { env = 0.0f; gainState = 1.0f; open = true; }

    void setEnabled(bool e) { enabled = e; }

    // linkedAbs = max(|L|, |R|); returns the gain to apply to both channels.
    float processGain(float linkedAbs)
    {
        if (! enabled)
            return 1.0f;

        env = std::max(linkedAbs, env * envRelCoef);

        if (open) { if (env < closeThresh) open = false; }
        else      { if (env > openThresh)  open = true;  }

        const float target = open ? 1.0f : 0.0f;
        const float coef   = (target > gainState) ? attCoef : relCoef;
        gainState += (target - gainState) * coef;
        return gainState;
    }

private:
    double fs = 44100.0;
    bool   enabled = false;
    bool   open = true;
    float  env = 0.0f, gainState = 1.0f;
    float  envRelCoef = 0.0f, attCoef = 1.0f, relCoef = 1.0f;
    static constexpr float openThresh  = 0.0056f; // ~ -45 dBFS
    static constexpr float closeThresh = 0.0018f; // ~ -55 dBFS
};

// Amp-style distortion chain, per channel:
//   tight low-cut -> input gain -> waveshaper -> DC block -> tone stack
//   (bass/mid/treble) -> presence -> cabinet low-pass -> high-cut
// then wet makeup gain, dry/wet mix, and master level. A stereo-linked noise
// gate sits in front of the whole thing. Stereo is two independent channel chains.
class IroncladEngine
{
public:
    void prepare(double sampleRate, int /*maxBlockSize*/)
    {
        sr = sampleRate;
        for (auto& c : chans)
        {
            c.tightHP.setSampleRate(sr);
            c.bass.setSampleRate(sr);
            c.mid.setSampleRate(sr);
            c.treble.setSampleRate(sr);
            c.presence.setSampleRate(sr);
            c.cab.setSampleRate(sr);
            c.highCut.setSampleRate(sr);
        }
        gate.prepare(sr);
        clear();
    }

    void clear()
    {
        for (auto& c : chans)
        {
            c.tightHP.clear();
            c.bass.clear();
            c.mid.clear();
            c.treble.clear();
            c.presence.clear();
            c.cab.clear();
            c.highCut.clear();
            c.dc.clear();
        }
        gate.clear();
    }

    void setParameters(const DistortionParams& p)
    {
        params = p;

        const float tight = std::clamp(p.tight, 20.0f, 500.0f);
        const float hiCut = std::clamp(p.highCut, 1000.0f, 20000.0f);
        // Presence knob (0..1) maps to a 0..+9 dB high shelf around 4.5 kHz.
        const float presDb = p.presence * 9.0f;

        for (auto& c : chans)
        {
            c.tightHP.setHighPass(tight, 0.707f);
            c.bass.setLowShelf(120.0f, p.bass);
            c.mid.setPeak(750.0f, 0.7f, p.mid);
            c.treble.setHighShelf(3000.0f, p.treble);
            c.presence.setHighShelf(4500.0f, presDb);
            // Fixed cabinet-style roll-off tames the fizz above the guitar range.
            c.cab.setLowPass(9000.0f, 0.707f);
            // User high-cut sits after the cab for a final treble tailoring.
            c.highCut.setLowPass(hiCut, 0.707f);
        }

        gate.setEnabled(p.gate);
        gain = Waveshaper::driveGain(p.type, p.drive);
    }

    void processBlock(float* left, float* right, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = gate.processGain(std::max(std::abs(left[i]), std::abs(right[i])));
            left[i]  = processSample(chans[0], left[i]  * g);
            right[i] = processSample(chans[1], right[i] * g);
        }
    }

private:
    struct Channel
    {
        Biquad   tightHP, bass, mid, treble, presence, cab, highCut;
        DCBlocker dc;
    };

    float processSample(Channel& c, float dry)
    {
        float x = c.tightHP.process(dry);
        x *= gain;
        x = Waveshaper::shape(params.type, x);
        x = c.dc.process(x);
        x = c.bass.process(x);
        x = c.mid.process(x);
        x = c.treble.process(x);
        x = c.presence.process(x);
        x = c.cab.process(x);
        x = c.highCut.process(x);

        const float wet = x * params.output;
        const float out = dry * (1.0f - params.mix) + wet * params.mix;
        return out * params.level;
    }

    Channel chans[2];
    NoiseGate gate;
    DistortionParams params;
    float  gain = 1.0f;
    double sr   = 44100.0;
};
