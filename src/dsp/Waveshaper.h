#pragma once
#include <cmath>

// Amp-voiced waveshaping. Four voicings share the same tanh-family core but
// differ in pre-gain scaling, bias (even-harmonic asymmetry), and hardness of
// the clip. `drive` is 0..1 from the knob; each mode maps it to its own gain
// span so the four types feel distinct at the same knob position.
namespace Waveshaper
{
    enum Mode { Clean = 0, Crunch = 1, Lead = 2, Fuzz = 3 };

    // Soft asymmetric clip: tanh with a DC bias folded in for even harmonics.
    inline float softClip(float x, float bias)
    {
        return std::tanh(x + bias) - std::tanh(bias);
    }

    // Harder clip used by Lead/Fuzz - a cubic knee that flattens sooner than
    // tanh for a more compressed, saturated top.
    inline float hardClip(float x)
    {
        if (x >  1.0f) return  0.6666667f;
        if (x < -1.0f) return -0.6666667f;
        return x - (x * x * x) / 3.0f;
    }

    // Per-mode pre-gain applied to the drive knob (multiplicative on top of a
    // unity floor). Bigger span = more aggressive at full drive.
    inline float driveGain(int mode, float drive)
    {
        switch (mode)
        {
            case Clean:  return 1.0f + drive *  6.0f;
            case Crunch: return 1.0f + drive * 16.0f;
            case Lead:   return 1.0f + drive * 40.0f;
            case Fuzz:   return 1.0f + drive * 80.0f;
            default:     return 1.0f + drive *  6.0f;
        }
    }

    inline float shape(int mode, float x)
    {
        switch (mode)
        {
            case Clean:  return softClip(x, 0.0f);
            case Crunch: return softClip(x, 0.15f);
            case Lead:   return hardClip(x * 0.7f) * 1.2f;
            case Fuzz:   return hardClip(x + 0.25f) - hardClip(0.25f);
            default:     return softClip(x, 0.0f);
        }
    }

    // Power-amp saturation: a gentler, more symmetric stage that sits AFTER the
    // preamp voicing (shape). Push-pull output tubes largely cancel even harmonics
    // and compress smoothly, so this is a plain tanh - the character comes from the
    // preamp; this adds output-stage compression + a little more grind when pushed.
    inline float powerAmp(float x) { return std::tanh(x); }
}
