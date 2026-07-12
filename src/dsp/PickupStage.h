#pragma once
#include "Biquad.h"
#include <algorithm>

// Magnetic pickup + amp-input-impedance model.
//
// A guitar pickup is an RLC circuit: winding inductance/resistance + its own
// capacitance + the cable capacitance + the amp's input impedance form a damped
// resonator with a peak in the 2-6 kHz range, rolling off above it. That resonant
// peak is most of what makes a pickup sound like itself, and the AMP INPUT LOAD
// (impedance + cable) changes the peak's height/frequency audibly - a bright,
// high-impedance front end gives a sharper, higher peak; a loaded one damps it.
//
// Modeled as a resonant peak filter -> gentle roll-off, with a per-type output
// level (humbuckers/actives are hotter, so they drive the amp harder).
class PickupStage
{
public:
    enum Type { Single = 0, Humbucker = 1, Active = 2, Bass = 3 };

    void setSampleRate(double fs) { peak.setSampleRate(fs); roll.setSampleRate(fs); }
    void clear() { peak.clear(); roll.clear(); }

    // load 0..1 : amp/cable loading. High = bright, hi-Z (sharper, higher peak);
    // low = darker, loaded (damped peak, earlier roll-off).
    void setParameters(int type, float load)
    {
        // Peaks eased (was up to +4.5 dB Q2.0 @5 kHz): that sharp presence-region
        // resonance stacked with the guitar-source model and amplified the input
        // noise floor into audible hiss on sustained/decaying notes. The guitar
        // source now carries the pickup character, so this stays gentle.
        float fRes, q, peakDb, rollHz, g;
        switch (type)
        {
            case Humbucker: fRes = 2900.0f; q = 1.2f; peakDb = 2.0f; rollHz = 6500.0f;  g = 1.35f; break;
            case Active:    fRes = 6000.0f; q = 0.7f; peakDb = 1.0f; rollHz = 11000.0f; g = 1.50f; break;
            case Bass:      fRes = 1600.0f; q = 1.0f; peakDb = 2.0f; rollHz = 4500.0f;  g = 1.20f; break;
            default:        fRes = 5000.0f; q = 1.3f; peakDb = 2.5f; rollHz = 8500.0f;  g = 1.00f; break; // Single
        }
        const float l = std::clamp(load, 0.0f, 1.0f);
        fRes   *= 0.85f + 0.30f * l;   // load nudges the resonance up
        q      *= 0.60f + 0.90f * l;   // ...and sharpens it
        peakDb *= 0.70f + 0.60f * l;
        rollHz *= 0.70f + 0.60f * l;
        peak.setPeak(fRes, q, peakDb);
        roll.setLowPass(std::min(rollHz, 19000.0f), 0.707f);
        outGain = g;
    }

    float process(float x) { return roll.process(peak.process(x)) * outGain; }

private:
    Biquad peak, roll;
    float outGain = 1.0f;
};
