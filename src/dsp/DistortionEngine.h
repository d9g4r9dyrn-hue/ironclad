#pragma once
#include "Biquad.h"
#include "Waveshaper.h"
#include <algorithm>

struct DistortionParams
{
    int   type     = 0;      // Waveshaper::Mode
    float drive    = 0.5f;   // 0..1
    float bass     = 0.0f;   // dB, -12..+12
    float mid      = 0.0f;   // dB, -12..+12
    float treble   = 0.0f;   // dB, -12..+12
    float presence = 0.5f;   // 0..1 -> post high-shelf brightness
    float tight    = 80.0f;  // Hz, pre-drive high-pass
    float mix      = 1.0f;   // 0..1 dry/wet
    float output   = 1.0f;   // 0..2 output gain
};

// Amp-style distortion chain, per channel:
//   tight low-cut -> input gain -> waveshaper -> DC block -> tone stack
//   (bass/mid/treble) -> presence -> cabinet low-pass
// then dry/wet mix and output gain. Stereo is two independent channel chains.
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
        }
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
            c.dc.clear();
        }
    }

    void setParameters(const DistortionParams& p)
    {
        params = p;

        const float tight = std::clamp(p.tight, 20.0f, 500.0f);
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
        }

        gain = Waveshaper::driveGain(p.type, p.drive);
    }

    void processBlock(float* left, float* right, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            left[i]  = processSample(chans[0], left[i]);
            right[i] = processSample(chans[1], right[i]);
        }
    }

private:
    struct Channel
    {
        Biquad   tightHP, bass, mid, treble, presence, cab;
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

        float wet = x * params.output;
        return dry * (1.0f - params.mix) + wet * params.mix;
    }

    Channel chans[2];
    DistortionParams params;
    float  gain = 1.0f;
    double sr   = 44100.0;
};
