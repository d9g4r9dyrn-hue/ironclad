#pragma once
#include <JuceHeader.h>
#include <vector>

// Schroeder all-pass used to add dispersion (the metallic "chirp" that makes a
// spring reverb read as a spring rather than a room).
class SchroederAP
{
public:
    void prepare(int lenSamples, float gain)
    {
        buf.assign((size_t) std::max(1, lenSamples), 0.0f);
        idx = 0; g = gain;
    }
    void clear() { std::fill(buf.begin(), buf.end(), 0.0f); idx = 0; }
    float process(float x)
    {
        const float bo = buf[(size_t) idx];
        const float y  = -g * x + bo;
        buf[(size_t) idx] = x + g * bo;
        idx = (idx + 1) % (int) buf.size();
        return y;
    }
private:
    std::vector<float> buf;
    int idx = 0;
    float g = 0.5f;
};

// Multi-mode reverb: JUCE's Freeverb for the room/plate/hall tail, with a
// front-end dispersion chain engaged for Spring. Processed 100% wet on a scratch
// copy and mixed with the dry signal, so the dry stays pristine.
class ReverbFx
{
public:
    enum Mode { Spring = 0, Plate = 1, Room = 2, Hall = 3 };

    void prepare(double sampleRate, int blockSize, int numChannels)
    {
        fs = sampleRate;
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, (juce::uint32) juce::jmax(1, numChannels) };
        rv.prepare(spec);
        wetBuf.setSize(juce::jmax(1, numChannels), blockSize);

        // Dispersion chain (short, prime-ish lengths) for the spring character.
        const int lens[kNumAP] = { 37, 53, 79, 113, 151 };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kNumAP; ++i)
                springAP[c][i].prepare(lens[i] + c * 7, 0.62f);
        clear();
    }

    void clear()
    {
        rv.reset();
        for (auto& ch : springAP) for (auto& ap : ch) ap.clear();
    }

    void setParameters(int mode, float size, float damp, float mix)
    {
        mode_ = mode; wet_ = juce::jlimit(0.0f, 1.0f, mix);

        // Per-mode voicing, then SIZE/DAMP nudge it.
        float room = 0.5f, damping = 0.5f, width = 0.9f;
        switch (mode)
        {
            case Spring: room = 0.42f; damping = 0.34f; width = 0.55f; break;
            case Plate:  room = 0.70f; damping = 0.22f; width = 1.00f; break;
            case Room:   room = 0.52f; damping = 0.50f; width = 0.85f; break;
            case Hall:   room = 0.90f; damping = 0.60f; width = 1.00f; break;
        }
        params.roomSize = juce::jlimit(0.0f, 1.0f, room + 0.35f * (size - 0.5f));
        params.damping  = juce::jlimit(0.0f, 1.0f, damping + 0.6f * (damp - 0.5f));
        params.width    = width;
        params.wetLevel = 1.0f;   // we do the dry/wet mix ourselves
        params.dryLevel = 0.0f;
        params.freezeMode = 0.0f;
        rv.setParameters(params);
    }

    void processBlock(float* L, float* R, int n)
    {
        if (wet_ <= 0.0001f) return;

        wetBuf.setSize(2, n, false, false, true);
        auto* wl = wetBuf.getWritePointer(0);
        auto* wr = wetBuf.getWritePointer(1);
        for (int i = 0; i < n; ++i) { wl[i] = L[i]; wr[i] = R[i]; }

        if (mode_ == Spring)
            for (int i = 0; i < n; ++i)
            {
                for (int a = 0; a < kNumAP; ++a) { wl[i] = springAP[0][a].process(wl[i]); wr[i] = springAP[1][a].process(wr[i]); }
            }

        juce::dsp::AudioBlock<float> block(wetBuf);
        block = block.getSubBlock(0, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        rv.process(ctx);

        for (int i = 0; i < n; ++i)
        {
            L[i] = L[i] * (1.0f - wet_) + wl[i] * wet_;
            R[i] = R[i] * (1.0f - wet_) + wr[i] * wet_;
        }
    }

private:
    static constexpr int kNumAP = 5;
    double fs = 44100.0;
    juce::dsp::Reverb rv;
    juce::dsp::Reverb::Parameters params;
    juce::AudioBuffer<float> wetBuf;
    SchroederAP springAP[2][kNumAP];
    int   mode_ = Room;
    float wet_ = 0.0f;
};
