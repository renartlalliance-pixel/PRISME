#pragma once
// Blur : petite reverb diffuse (style Freeverb) qui transforme les resonances
// nettes en nappe floue. Fader dry/wet : 0 = resonances intactes, 1 = wash.
// Implementation autonome partagee plugin / rendu offline.
#include <vector>
#include <cmath>
#include <algorithm>

struct BlurComb
{
    std::vector<float> buf;
    int size = 1, pos = 0;
    float lp = 0.0f;       // etat passe-bas dans la boucle (damping)
    float feedback = 0.8f;
    float damp = 0.3f;

    void init(int numSamples)
    {
        size = std::max(8, numSamples);
        buf.assign((size_t) size, 0.0f);
        pos = 0; lp = 0.0f;
    }

    inline float process(float x)
    {
        float y = buf[(size_t) pos];
        lp = damp * lp + (1.0f - damp) * y;
        float fb = std::clamp(lp * feedback, -2.0f, 2.0f);
        buf[(size_t) pos] = std::clamp(x + fb, -2.0f, 2.0f);
        if (++pos >= size) pos = 0;
        return y;
    }
};

struct BlurAllpass
{
    std::vector<float> buf;
    int size = 1, pos = 0;
    float feedback = 0.5f;

    void init(int numSamples)
    {
        size = std::max(8, numSamples);
        buf.assign((size_t) size, 0.0f);
        pos = 0;
    }

    inline float process(float x)
    {
        float y = buf[(size_t) pos];
        float out = -x + y;
        buf[(size_t) pos] = std::clamp(x + y * feedback, -2.0f, 2.0f);
        if (++pos >= size) pos = 0;
        return out;
    }
};

class BlurReverb
{
public:
    void setSampleRate(double sr)
    {
        sampleRate = sr > 0 ? sr : 44100.0;
        limAtt = 1.0f - std::exp(-1.0f / (float)(0.005 * sampleRate));
        limRel = 1.0f - std::exp(-1.0f / (float)(0.40 * sampleRate));
        double k = sampleRate / 44100.0;
        const int combL[4] = { 1116, 1188, 1277, 1356 };
        const int combR[4] = { 1139, 1211, 1300, 1379 };
        for (int i = 0; i < 4; ++i)
        {
            combsL[i].init((int) (combL[i] * k));
            combsR[i].init((int) (combR[i] * k));
        }
        const int ap[2] = { 556, 441 };
        for (int i = 0; i < 2; ++i)
        {
            allL[i].init((int) (ap[i] * k));
            allR[i].init((int) (ap[i] * k));
            allL[i].feedback = allR[i].feedback = 0.5f;
        }
        int preSize = (int) (sampleRate * 0.3) + 16;
        preL.assign((size_t) preSize, 0.0f);
        preR.assign((size_t) preSize, 0.0f);
        preBufSize = preSize;
        prePos = 0;
        setSize(roomSize);
    }

    // Taille 0..1 -> base de feedback 0.55..0.95
    void setSize(float t)
    {
        roomSize = std::clamp(t, 0.0f, 1.0f);
        updateFeedback();
    }

    // Pre-delay 0..0.25 s : la nappe arrive apres les transitoires
    void setPreDelay(float seconds)
    {
        preDelaySec = std::clamp(seconds, 0.0f, 0.25f);
        preLen = std::min((int) (preDelaySec * sampleRate), preBufSize - 1);
    }

    // Couleur 0..1 : 0 = sombre, 1 = brillant (damping dans la boucle)
    void setColor(float c)
    {
        color = std::clamp(c, 0.0f, 1.0f);
        float damp = 0.65f * (1.0f - color) + 0.05f;
        for (int i = 0; i < 4; ++i)
            combsL[i].damp = combsR[i].damp = damp;
    }

    // Decay : multiplicateur de tenue (1.0 = inchangé), 0.2 court, 3.0 immense
    void setDecayMul(float d)
    {
        decayMul = std::clamp(d, 0.2f, 3.0f);
        updateFeedback();
    }

    // Largeur stereo 0..1.5 (0 = mono, 1 = normal)
    void setWidth(float w) { width = std::clamp(w, 0.0f, 1.5f); }

    void reset()
    {
        for (int i = 0; i < 4; ++i)
        {
            combsL[i].pos = combsR[i].pos = 0;
            combsL[i].lp = combsR[i].lp = 0.0f;
            std::fill(combsL[i].buf.begin(), combsL[i].buf.end(), 0.0f);
            std::fill(combsR[i].buf.begin(), combsR[i].buf.end(), 0.0f);
        }
        for (int i = 0; i < 2; ++i)
        {
            allL[i].pos = allR[i].pos = 0;
            std::fill(allL[i].buf.begin(), allL[i].buf.end(), 0.0f);
            std::fill(allR[i].buf.begin(), allR[i].buf.end(), 0.0f);
        }
        limEnv = 0.0f;
        prePos = 0;
        std::fill(preL.begin(), preL.end(), 0.0f);
        std::fill(preR.begin(), preR.end(), 0.0f);
    }

    inline void process(float inL, float inR, float& outL, float& outR)
    {
        // Pre-delay (tampon circulaire simple, sans interpolation)
        if (preLen > 0)
        {
            preL[(size_t) prePos] = inL;
            preR[(size_t) prePos] = inR;
            int rp = prePos - preLen;
            if (rp < 0) rp += preBufSize;
            inL = preL[(size_t) rp];
            inR = preR[(size_t) rp];
            if (++prePos >= preBufSize) prePos = 0;
        }
        float l = 0.0f, r = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            l += combsL[i].process(inL);
            r += combsR[i].process(inR);
        }
        l *= 0.25f; r *= 0.25f;
        for (int i = 0; i < 2; ++i)
        {
            l = allL[i].process(l);
            r = allR[i].process(r);
        }
        // Largeur : mid/side (0 = mono)
        float mid = (l + r) * 0.5f;
        float side = (l - r) * 0.5f * width;
        outL = mid + side;
        outR = mid - side;

        // Etage de gain : le Gel accumule de l'energie en continu ; ce limiteur
        // doux maintient la nappe sous le seuil (transparent en dessous).
        float peak = std::fabs(outL) > std::fabs(outR) ? std::fabs(outL) : std::fabs(outR);
        float k = (peak > limEnv) ? limAtt : limRel;
        limEnv += (peak - limEnv) * k;
        if (limEnv > 0.5f)
        {
            float g = 0.5f / (limEnv + 1e-6f);
            outL *= g;
            outR *= g;
        }
        // Plafond doux anti-cretes (les transitoires plus rapides que l'attaque)
        outL = 0.65f * std::tanh(outL / 0.65f);
        outR = 0.65f * std::tanh(outR / 0.65f);
    }

private:
    void updateFeedback()
    {
        float base = 0.55f + 0.40f * roomSize;
        float fb = 1.0f - (1.0f - base) / decayMul;
        fb = std::clamp(fb, 0.10f, 0.985f);
        for (int i = 0; i < 4; ++i)
        {
            combsL[i].feedback = fb;
            combsR[i].feedback = fb;
        }
    }

    double sampleRate = 44100.0;
    float roomSize = 0.6f;
    float color = 0.6f;
    float decayMul = 1.0f;
    float width = 1.0f;
    float preDelaySec = 0.0f;
    int preLen = 0;
    int prePos = 0;
    int preBufSize = 1;
    std::vector<float> preL, preR;
    // Etage de gain (Gel) : enveloppe de crete + cible 0.5
    float limEnv = 0.0f;
    float limAtt = 0.05f;
    float limRel = 0.0005f;
    BlurComb combsL[4], combsR[4];
    BlurAllpass allL[2], allR[2];
};
