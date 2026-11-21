#pragma once
// EQ graphique 4 bandes - implementation autonome (sans dependance), partagee
// entre le plugin et l'outil de rendu offline. Formules RBJ Audio EQ Cookbook.
// Bandes fixes : LowShelf 150 Hz / Peak 800 Hz Q1.0 / Peak 3 kHz Q1.0 / HighShelf 8 kHz.
#include <cmath>
#include <algorithm>

struct PrismeBiquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    void reset() { x1 = x2 = y1 = y2 = 0.0f; }

    // Etagere basse, S = 1 (pente douce). gainDb : -12..+12
    void setLowShelf(double sr, double f0, float gainDb)
    {
        double A = std::pow(10.0, gainDb / 40.0);
        double w0 = 6.283185307179586 * f0 / sr;
        double cw = std::cos(w0), sw = std::sin(w0);
        double alpha = sw / 2.0 * std::sqrt(2.0); // S = 1
        double a0 = (A + 1.0) + (A - 1.0) * cw + 2.0 * std::sqrt(A) * alpha;
        b0 = (float)((A * ((A + 1.0) - (A - 1.0) * cw + 2.0 * std::sqrt(A) * alpha)) / a0);
        b1 = (float)((2.0 * A * ((A - 1.0) - (A + 1.0) * cw)) / a0);
        b2 = (float)((A * ((A + 1.0) - (A - 1.0) * cw - 2.0 * std::sqrt(A) * alpha)) / a0);
        a1 = (float)((-2.0 * ((A - 1.0) + (A + 1.0) * cw)) / a0);
        a2 = (float)(((A + 1.0) + (A - 1.0) * cw - 2.0 * std::sqrt(A) * alpha) / a0);
    }

    // Etagere haute, S = 1. gainDb : -12..+12
    void setHighShelf(double sr, double f0, float gainDb)
    {
        double A = std::pow(10.0, gainDb / 40.0);
        double w0 = 6.283185307179586 * f0 / sr;
        double cw = std::cos(w0), sw = std::sin(w0);
        double alpha = sw / 2.0 * std::sqrt(2.0); // S = 1
        double a0 = (A + 1.0) - (A - 1.0) * cw + 2.0 * std::sqrt(A) * alpha;
        b0 = (float)((A * ((A + 1.0) + (A - 1.0) * cw + 2.0 * std::sqrt(A) * alpha)) / a0);
        b1 = (float)((-2.0 * A * ((A - 1.0) + (A + 1.0) * cw)) / a0);
        b2 = (float)((A * ((A + 1.0) + (A - 1.0) * cw - 2.0 * std::sqrt(A) * alpha)) / a0);
        a1 = (float)((2.0 * ((A - 1.0) - (A + 1.0) * cw)) / a0);
        a2 = (float)(((A + 1.0) - (A - 1.0) * cw - 2.0 * std::sqrt(A) * alpha) / a0);
    }

    // Cloche. Q = 1.0. gainDb : -12..+12
    void setPeak(double sr, double f0, double Q, float gainDb)
    {
        double A = std::pow(10.0, gainDb / 40.0);
        double w0 = 6.283185307179586 * f0 / sr;
        double cw = std::cos(w0), sw = std::sin(w0);
        double alpha = sw / (2.0 * Q);
        double a0 = 1.0 + alpha / A;
        b0 = (float)((1.0 + alpha * A) / a0);
        b1 = (float)((-2.0 * cw) / a0);
        b2 = (float)((1.0 - alpha * A) / a0);
        a1 = (float)((-2.0 * cw) / a0);
        a2 = (float)((1.0 - alpha / A) / a0);
    }

    inline float process(float x)
    {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }
};

class PrismeEQ4Band
{
public:
    static constexpr float kLowFreq = 150.0f;
    static constexpr float kLowMidFreq = 800.0f;
    static constexpr float kHighMidFreq = 3000.0f;
    static constexpr float kHighFreq = 8000.0f;

    void configure(double sr, float dbLow, float dbLowMid, float dbHighMid, float dbHigh)
    {
        bands[0].setLowShelf(sr, kLowFreq, dbLow);
        bands[1].setPeak(sr, kLowMidFreq, 1.0, dbLowMid);
        bands[2].setPeak(sr, kHighMidFreq, 1.0, dbHighMid);
        bands[3].setHighShelf(sr, kHighFreq, dbHigh);
        lastDb[0] = dbLow; lastDb[1] = dbLowMid; lastDb[2] = dbHighMid; lastDb[3] = dbHigh;
    }

    void reset()
    {
        for (auto& b : bands) b.reset();
    }

    inline float process(float x)
    {
        for (auto& b : bands) x = b.process(x);
        return x;
    }

    static bool needsUpdate(const float lastDb[4], float dbLow, float dbLowMid, float dbHighMid, float dbHigh)
    {
        const float eps = 0.01f;
        return std::fabs(lastDb[0] - dbLow) > eps || std::fabs(lastDb[1] - dbLowMid) > eps
            || std::fabs(lastDb[2] - dbHighMid) > eps || std::fabs(lastDb[3] - dbHigh) > eps;
    }

    float lastDb[4] = { 999.0f, 999.0f, 999.0f, 999.0f };

private:
    PrismeBiquad bands[4];
};

// Limiteur de bande : passe-bande (coupe-bas + coupe-haut, Q = 0.707)
// qui definit la zone de frequences excitant l'effet.
// 20 Hz - 20 kHz = transparent.
struct PrismeBandLimiter
{
    PrismeBiquad hp, lp;
    float lastLo = -1.0f, lastHi = -1.0f;

    void reset() { hp.reset(); lp.reset(); }

    void configure(double sr, float loHz, float hiHz)
    {
        loHz = std::clamp(loHz, 20.0f, 20000.0f);
        hiHz = std::clamp(hiHz, 20.0f, 20000.0f);
        if (loHz > hiHz) loHz = hiHz;
        setHighpass(sr, loHz);
        setLowpass(sr, hiHz);
        lastLo = loHz; lastHi = hiHz;
    }

    static bool needsUpdate(float lastLo, float lastHi, float loHz, float hiHz)
    {
        return std::fabs(lastLo - loHz) > 0.5f || std::fabs(lastHi - hiHz) > 0.5f;
    }

    static bool isBypass(float loHz, float hiHz)
    {
        return loHz <= 20.5f && hiHz >= 19999.0f;
    }

    inline float process(float x)
    {
        return lp.process(hp.process(x));
    }

private:
    void setHighpass(double sr, double f0)
    {
        double w0 = 6.283185307179586 * f0 / sr;
        // Garde-fou Nyquist : au-dela, passe-haut = transparent
        if (w0 >= 3.10) { hp.b0 = 1; hp.b1 = hp.b2 = hp.a1 = hp.a2 = 0; return; }
        double cw = std::cos(w0), sw = std::sin(w0);
        double alpha = sw / (2.0 * 0.7071067811865476);
        double b0 = (1.0 + cw) / 2.0, b1 = -(1.0 + cw), b2 = (1.0 + cw) / 2.0;
        double a0 = 1.0 + alpha, a1 = -2.0 * cw, a2 = 1.0 - alpha;
        hp.b0 = (float)(b0 / a0); hp.b1 = (float)(b1 / a0); hp.b2 = (float)(b2 / a0);
        hp.a1 = (float)(a1 / a0); hp.a2 = (float)(a2 / a0);
    }

    void setLowpass(double sr, double f0)
    {
        double w0 = 6.283185307179586 * f0 / sr;
        // Garde-fou Nyquist : au-dela, passe-bas = transparent
        if (w0 >= 3.10) { lp.b0 = 1; lp.b1 = lp.b2 = lp.a1 = lp.a2 = 0; return; }
        double cw = std::cos(w0), sw = std::sin(w0);
        double alpha = sw / (2.0 * 0.7071067811865476);
        double b0 = (1.0 - cw) / 2.0, b1 = 1.0 - cw, b2 = (1.0 - cw) / 2.0;
        double a0 = 1.0 + alpha, a1 = -2.0 * cw, a2 = 1.0 - alpha;
        lp.b0 = (float)(b0 / a0); lp.b1 = (float)(b1 / a0); lp.b2 = (float)(b2 / a0);
        lp.a1 = (float)(a1 / a0); lp.a2 = (float)(a2 / a0);
    }
};
