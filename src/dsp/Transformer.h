#pragma once
#include <cmath>
#include <algorithm>

// Output-transformer model: core saturation + magnetic HYSTERESIS.
//
// A real transformer's B-H curve is a loop with finite width - the flux for a
// rising drive follows a different path than for a falling drive, so the transfer
// function depends on the direction of travel and on recent state (memory). We
// model that with a direction-of-travel bias shifted into a bounded saturation:
// rising input uses a curve shifted one way, falling the other, tracing a real
// hysteresis loop. Because the output is always a bounded tanh (no growing internal
// feedback), it is unconditionally stable - unlike a full Jiles-Atherton solve,
// which is the "genuinely hard, can-blow-up" model this deliberately avoids.
class Transformer
{
public:
    void prepare(double sampleRate) { fs = sampleRate; clear(); }
    void clear() { xPrev = 0.0f; dirSm = 0.0f; env = 0.0f; }

    void setAmount(float a)   // 0..1
    {
        amount   = std::clamp(a, 0.0f, 1.0f);
        drive    = 1.0f + amount * 0.45f;   // gentle core saturation (mild peak rounding)
        width    = amount * 0.10f;          // hysteresis loop half-width (the "memory")
        invDrive = 1.0f / drive;            // keep small-signal ~unity
    }

    float process(float x)
    {
        const float dx = x - xPrev;
        xPrev = x;
        // Smoothed direction of travel: the loop needs the sign of dB/dt, but a raw
        // sign chatters at zero-crossings and (with a dead-zone) settles to NO
        // direction in silence, so the loop relaxes to zero instead of a DC offset.
        const float dirT = (dx > 1.0e-5f) ? 1.0f : (dx < -1.0e-5f ? -1.0f : 0.0f);
        dirSm += 0.04f * (dirT - dirSm);
        // Fade the hysteresis bias in with level: at low signal (clean tones) the
        // direction bias otherwise adds a faint high-freq chatter. Real transformer
        // hysteresis is negligible until the core is driven anyway.
        env += (std::abs(x) > env ? 0.02f : 0.001f) * (std::abs(x) - env);
        const float lvl = std::min(env * 12.0f, 1.0f);   // ~full above ~-18 dBFS
        return std::tanh(drive * (x - width * dirSm * lvl)) * invDrive;
    }

private:
    double fs = 44100.0;
    float amount = 0.0f, drive = 1.0f, width = 0.0f, invDrive = 1.0f;
    float xPrev = 0.0f, dirSm = 0.0f, env = 0.0f;
};
