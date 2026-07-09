#pragma once
#include <cmath>

// Direct-form-I biquad with RBJ "cookbook" coefficients. Used for the amp tone
// stack (bass/mid/treble), the presence shelf, the pre-drive tight low-cut, and
// the cabinet-taming low-pass. One instance per channel - filter state is not
// shareable across channels.
class Biquad
{
public:
    void setSampleRate(double sampleRate) { fs = sampleRate; }

    void setLowShelf (float freq, float gainDb)          { design(Shelf,  freq, 0.707f, gainDb, -1); }
    void setHighShelf(float freq, float gainDb)          { design(Shelf,  freq, 0.707f, gainDb, +1); }
    void setPeak     (float freq, float q, float gainDb) { design(Peak,   freq, q,      gainDb,  0); }
    void setLowPass  (float freq, float q = 0.707f)      { design(LowP,   freq, q,      0.0f,    0); }
    void setHighPass (float freq, float q = 0.707f)      { design(HighP,  freq, q,      0.0f,    0); }
    void setBandPass (float freq, float q)               { design(BandP,  freq, q,      0.0f,    0); }

    float process(float x)
    {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }

    void clear() { x1 = x2 = y1 = y2 = 0.0f; }

private:
    enum Kind { Shelf, Peak, LowP, HighP, BandP };

    // `shelfDir` is -1 for a low shelf, +1 for a high shelf, ignored otherwise.
    void design(Kind kind, float freq, float q, float gainDb, int shelfDir)
    {
        const double w0    = 2.0 * 3.14159265358979 * (double) freq / fs;
        const double cosw  = std::cos(w0);
        const double sinw  = std::sin(w0);
        const double alpha = sinw / (2.0 * (double) q);
        const double A     = std::pow(10.0, (double) gainDb / 40.0);

        double B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;

        if (kind == Peak)
        {
            B0 = 1 + alpha * A;
            B1 = -2 * cosw;
            B2 = 1 - alpha * A;
            A0 = 1 + alpha / A;
            A1 = -2 * cosw;
            A2 = 1 - alpha / A;
        }
        else if (kind == Shelf && shelfDir < 0) // low shelf
        {
            const double s = 2.0 * std::sqrt(A) * alpha;
            B0 =      A * ((A + 1) - (A - 1) * cosw + s);
            B1 =  2 * A * ((A - 1) - (A + 1) * cosw);
            B2 =      A * ((A + 1) - (A - 1) * cosw - s);
            A0 =          (A + 1) + (A - 1) * cosw + s;
            A1 =     -2 * ((A - 1) + (A + 1) * cosw);
            A2 =          (A + 1) + (A - 1) * cosw - s;
        }
        else if (kind == Shelf) // high shelf
        {
            const double s = 2.0 * std::sqrt(A) * alpha;
            B0 =      A * ((A + 1) + (A - 1) * cosw + s);
            B1 = -2 * A * ((A - 1) + (A + 1) * cosw);
            B2 =      A * ((A + 1) + (A - 1) * cosw - s);
            A0 =          (A + 1) - (A - 1) * cosw + s;
            A1 =      2 * ((A - 1) - (A + 1) * cosw);
            A2 =          (A + 1) - (A - 1) * cosw - s;
        }
        else if (kind == LowP)
        {
            B0 = (1 - cosw) / 2;
            B1 =  1 - cosw;
            B2 = (1 - cosw) / 2;
            A0 =  1 + alpha;
            A1 = -2 * cosw;
            A2 =  1 - alpha;
        }
        else if (kind == HighP)
        {
            B0 =  (1 + cosw) / 2;
            B1 = -(1 + cosw);
            B2 =  (1 + cosw) / 2;
            A0 =   1 + alpha;
            A1 =  -2 * cosw;
            A2 =   1 - alpha;
        }
        else // BandP (constant 0 dB peak gain)
        {
            B0 =  alpha;
            B1 =  0;
            B2 = -alpha;
            A0 =  1 + alpha;
            A1 = -2 * cosw;
            A2 =  1 - alpha;
        }

        const double inv = 1.0 / A0;
        b0 = (float) (B0 * inv);
        b1 = (float) (B1 * inv);
        b2 = (float) (B2 * inv);
        a1 = (float) (A1 * inv);
        a2 = (float) (A2 * inv);
    }

    double fs = 44100.0;
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
};

// One-sample DC blocker - the asymmetric shaping stages add a DC offset that
// would otherwise eat headroom and thump on bypass toggles.
class DCBlocker
{
public:
    float process(float x)
    {
        float y = x - x1 + R * y1;
        x1 = x;
        y1 = y;
        return y;
    }
    void clear() { x1 = 0.0f; y1 = 0.0f; }
private:
    static constexpr float R = 0.9975f;
    float x1 = 0.0f, y1 = 0.0f;
};
