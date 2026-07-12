#pragma once
#include "Biquad.h"
#include <algorithm>
#include <cmath>

// Guitar source model. Sits at the very FRONT of the amp chain (before the pickup
// RLC + tight low-cut), conditioning the raw input to approximate the electrical,
// spectral, and dynamic character of several electric-guitar archetypes. It is NOT
// a static EQ badge: because it changes input gain staging, the bass entering the
// clipper, the pickup resonance, and the feedback sensitivity, the SAME preset reacts
// differently per guitar - which is the whole point of exposing Guitar Type.
//
// Per type we bake a physical-ish profile: source output level, a pre-distortion
// high-pass (tightness), a low-mid bell (body vs. scoop), a pickup resonance peak,
// an upper-mid/presence shelf (pick definition), a top-end shelf (extension vs.
// humbucker roll-off), an optional mechanical body mode (semi-hollow bloom), and a
// feedback-sensitivity multiplier. A MODEL amount (0..1) blends the whole character
// in from neutral; OUTPUT/BODY/BRIGHT are user offsets on top.
class GuitarSource
{
public:
    enum Type { STypeSC = 0, SingleCutHB, ModernHB, TTypeSC, SemiHollow, P90, ExtendedRange };

    void setSampleRate(double fs)
    {
        hp.setSampleRate(fs); lowMid.setSampleRate(fs); res.setSampleRate(fs);
        presence.setSampleRate(fs); top.setSampleRate(fs); bodyRes.setSampleRate(fs);
        bodyShelf.setSampleRate(fs); brightShelf.setSampleRate(fs);
    }
    void clear()
    {
        hp.clear(); lowMid.clear(); res.clear(); presence.clear(); top.clear();
        bodyRes.clear(); bodyShelf.clear(); brightShelf.clear();
    }

    // type: archetype 0..6. model: 0..1 character depth. outputDb: source level trim.
    // body01/bright01: 0..1 user tone (0.5 = neutral).
    void setParameters(int type, float model, float outputDb, float body01, float bright01)
    {
        struct Prof {
            float inDb, hpHz, lmHz, lmDb, resHz, resQ, resDb,
                  presHz, presDb, topHz, topDb, bodyHz, bodyQ, bodyDb, fbSens;
        };
        // Bold enough that switching type is clearly audible (esp. at low/medium
        // drive, where distortion doesn't yet compress the spectral differences);
        // MODEL still blends the whole character from neutral.
        // HF boosts (resDb/presDb/topDb) and resonance Q eased vs. the first bold
        // pass: they were generating fizz into the clipper. The gain (inDb), body
        // (lmDb) and tightness (hp) that carry the archetype character are kept.
        // inDb  hp   lmHz  lmDb   resHz resQ  resDb presHz presDb topHz  topDb bodyHz bodyQ bodyDb fbSens
        static const Prof P[7] = {
            { -4.0f, 85, 400, -3.5f, 3600, 1.5f, 3.0f, 7500,  1.5f, 9000,  1.0f,  200, 1.0f, 0.0f, 0.95f }, // S-Type SC
            {  4.0f, 70, 280,  4.5f, 2200, 1.1f, 2.5f, 4800, -1.0f, 4200, -3.0f,  200, 1.0f, 0.0f, 1.20f }, // Single-Cut HB
            {  6.0f,110, 240, -3.5f, 1700, 1.1f, 1.5f, 2400,  2.5f, 6000, -2.0f,  200, 1.0f, 0.0f, 0.80f }, // Modern HB
            { -2.0f, 95, 320, -1.5f, 2800, 1.4f, 3.0f, 4200,  2.5f, 8500,  1.2f,  200, 1.0f, 0.0f, 0.95f }, // T-Type SC
            {  1.5f, 75, 190,  3.5f, 2500, 1.1f, 2.0f, 5000, -1.0f, 6500, -1.5f,  250, 2.6f, 4.0f, 1.50f }, // Semi-Hollow
            {  2.0f, 90, 480,  4.0f, 2900, 1.3f, 2.5f, 4200,  1.0f, 8000,  0.0f,  200, 1.0f, 0.0f, 1.10f }, // P-90
            {  5.0f, 60, 180, -4.5f, 1900, 1.1f, 2.0f, 3200,  1.8f, 5500, -1.5f,  200, 1.0f, 0.0f, 0.75f }, // Extended-Range
        };
        const Prof& pr = P[std::clamp(type, 0, 6)];
        const float m  = std::clamp(model, 0.0f, 1.0f);

        // MODEL blends character in from neutral: at 0 the source is essentially flat
        // (subsonic-only HP, no EQ colour, unity character gain), at 1 it is the full
        // archetype. OUTPUT/BODY/BRIGHT always apply so the user can trim any setting.
        inGain = std::pow(10.0f, (pr.inDb * m + std::clamp(outputDb, -12.0f, 12.0f)) / 20.0f);

        hp.setHighPass(30.0f + (pr.hpHz - 30.0f) * m, 0.707f);
        lowMid.setPeak(pr.lmHz, 0.8f, pr.lmDb * m);
        res.setPeak(pr.resHz, pr.resQ, pr.resDb * m);
        presence.setHighShelf(pr.presHz, pr.presDb * m);
        top.setHighShelf(pr.topHz, pr.topDb * m);
        bodyRes.setPeak(pr.bodyHz, pr.bodyQ, pr.bodyDb * m);

        // User tone offsets (type-independent, neutral at 0.5): +/-4 dB each.
        bodyShelf.setLowShelf(180.0f, (std::clamp(body01, 0.0f, 1.0f) - 0.5f) * 8.0f);
        brightShelf.setHighShelf(3500.0f, (std::clamp(bright01, 0.0f, 1.0f) - 0.5f) * 8.0f);

        fbSens = 1.0f + (pr.fbSens - 1.0f) * m;   // feedback sensitivity blended by MODEL
    }

    float process(float x)
    {
        x = hp.process(x);
        x = lowMid.process(x);
        x = bodyShelf.process(x);
        x = res.process(x);
        x = presence.process(x);
        x = brightShelf.process(x);
        x = top.process(x);
        x = bodyRes.process(x);
        return x * inGain;
    }

    float getFeedbackSens() const { return fbSens; }

private:
    Biquad hp, lowMid, res, presence, top, bodyRes, bodyShelf, brightShelf;
    float  inGain = 1.0f, fbSens = 1.0f;
};
