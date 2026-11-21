#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>

class PrismResonatorBank
{
public:
    static constexpr int maxVoices = 12;

    PrismResonatorBank()
    {
        voices.resize(maxVoices);
        for (auto& v : voices)
        {
            v.buf.assign(1, 0.0f);
            v.bufSize = 1;
        }
    }

    void setSampleRate(double sr)
    {
        sampleRate = sr > 0 ? sr : 44100.0;
        int newSize = (int)(sampleRate * 1.0) + 2048;
        newSize = std::max(newSize, 8192);
        for (auto& v : voices)
        {
            v.buf.assign((size_t)newSize, 0.0f);
            v.bufSize = newSize;
            v.writePos = 0;
            v.lp = 0.0f;
            v.emph = 1.0f;
            v.emphTarget = 1.0f;
        }
        emphCoeff = 1.0f - std::exp(-1.0f / (float)(emphSmoothSec * sampleRate));
        updateAllDelays();
        updateAllGains();
    }

    void setTuning(const std::vector<int>& midiNotes)
    {
        int n = std::min((int)midiNotes.size(), maxVoices);
        numActive = std::max(1, n);
        for (int i = 0; i < numActive; ++i)
        {
            voices[(size_t)i].midi = midiNotes[(size_t)i];
            voices[(size_t)i].freq = midiToFreq(midiNotes[(size_t)i]);
        }
        updateAllDelays();
        updateAllGains();
    }

    void setDecay(double t60seconds, bool freeze)
    {
        if (freeze)
            t60seconds = 30.0;
        t60 = std::clamp(t60seconds, 0.05, 30.0);
        updateAllGains();
    }

    // brightness 0..1 (0 sombre, 1 brillant)
    void setBrightness(float b)
    {
        brightness = std::clamp(b, 0.0f, 1.0f);
        // 0 -> lpCoeff 0.85 (sombre), 1 -> 0.0 (brillant)
        lpCoeff = (1.0f - brightness) * 0.85f;
    }

    // shimmer 0..1 : profondeur de modulation "vivant"
    void setShimmer(float s)
    {
        shimmer = std::clamp(s, 0.0f, 1.0f);
    }

    // spread 0..1 : largeur stereo
    void setSpread(float s)
    {
        spread = std::clamp(s, 0.0f, 1.0f);
    }

    // Temps de fondu (secondes) vers les gains de mise en avant des voix (arpege).
    // Court = pincements percussifs, long = houles lentes.
    void setEmphSmooth(double seconds)
    {
        emphSmoothSec = std::clamp(seconds, 0.002, 2.0);
        emphCoeff = 1.0f - std::exp(-1.0f / (float)(emphSmoothSec * sampleRate));
    }

    // Gains cibles par voix (0..1), utilises par l'arpegiateur.
    // n peut etre < maxVoices : les voix restantes gardent leur cible.
    void setVoiceTargets(const float* targets, int n)
    {
        n = std::min(n, maxVoices);
        for (int i = 0; i < n; ++i)
            voices[(size_t)i].emphTarget = std::clamp(targets[i], 0.0f, 1.0f);
    }

    void resetEmphasis()
    {
        for (auto& v : voices)
        {
            v.emph = 1.0f;
            v.emphTarget = 1.0f;
        }
    }

    // --- Mouvement : modulation d'amplitude continue de la resonance ---
    // rateHz : vitesse (0.05 = respiration lente, 20 = rafales rythmiques)
    // depth  : force 0..1 (0 = transparent)
    // shape  : forme 0..1 (0 = impulsion douce organique, 1 = rafales nettes)
    // unison : 0 = phases reparties (nappe evolutive), 1 = une seule impulsion motrice
    void setMovement(float rateHz, float depth, float shape, float unison)
    {
        movRateHz = std::clamp(rateHz, 0.0f, 40.0f);
        movDepth = std::clamp(depth, 0.0f, 1.0f);
        movShape = std::clamp(shape, 0.0f, 1.0f);
        movUnison = std::clamp(unison, 0.0f, 1.0f);
    }

    // Lit de l'arpege pour la separation nappe/session (meme valeur que le processeur)
    static constexpr float kArpBed = 0.15f;

    void setArpSplitActive(bool active) { arpSplitActive = active; }

    void reset()
    {
        for (auto& v : voices)
        {
            std::fill(v.buf.begin(), v.buf.end(), 0.0f);
            v.lp = 0.0f;
            v.emph = 1.0f;
            v.emphTarget = 1.0f;
            v.lfoPhase = (float)(rand() % 1000) / 1000.0f * 6.2831853f;
        }
        dcL_x1 = dcL_y1 = dcR_x1 = dcR_y1 = 0.0f;
        dcAL_x1 = dcAL_y1 = dcAR_x1 = dcAR_y1 = 0.0f;
        timeSamples = 0;
    }

    // Traite un echantillon mono -> stereo. Si arpL/arpR fournis (et split actif),
    // recoit en plus la part des voix mises en avant (session arpegee).
    inline void processSample(float monoIn, float& outL, float& outR,
                              float* arpL = nullptr, float* arpR = nullptr)
    {
        outL = 0.0f;
        outR = 0.0f;
        float arpSumL = 0.0f, arpSumR = 0.0f;
        bool doSplit = arpSplitActive && arpL != nullptr && arpR != nullptr;
        if (numActive <= 0)
        {
            if (arpL) *arpL = 0.0f;
            if (arpR) *arpR = 0.0f;
            return;
        }

        // Saturation douce sur l'excitation pour enrichir les harmoniques
        float exc = std::tanh(monoIn * 1.2f);

        float norm = 0.38f / std::sqrt((float)numActive);
        // LFO rate de base : 0.15 Hz -> 2.8 Hz selon shimmer
        float lfoRate = 0.15f + shimmer * 2.6f;
        float lfoInc = 6.2831853f * lfoRate / (float)sampleRate;

        // Mouvement : phase globale + forme d'onde (douce -> fracturee)
        float movInc = 6.2831853f * movRateHz / (float)sampleRate;
        movPhase += movInc;
        if (movPhase > 6.2831853f) movPhase -= 6.2831853f;
        float movK = 1.0f + 4.0f * movShape;
        float movNorm = std::tanh(movK); // > 0 toujours

        for (int i = 0; i < numActive; ++i)
        {
            Voice& v = voices[(size_t)i];

            // Modulation vivante : petite variation de longueur + amplitude
            float mod = std::sin(v.lfoPhase);
            v.lfoPhase += lfoInc * (1.0f + 0.23f * (float)i / (float)numActive);
            if (v.lfoPhase > 6.2831853f) v.lfoPhase -= 6.2831853f;

            float detune = 1.0f + shimmer * 0.0035f * mod;
            float dLen = v.delayLen * detune;
            dLen = std::clamp(dLen, 2.0f, (float)(v.bufSize - 4));

            float readPos = (float)v.writePos - dLen;
            while (readPos < 0) readPos += (float)v.bufSize;

            int i0 = (int)readPos;
            int i1 = i0 + 1;
            if (i1 >= v.bufSize) i1 -= v.bufSize;
            if (i0 >= v.bufSize) i0 -= v.bufSize;
            float frac = readPos - (float)((int)readPos);
            float delayed = v.buf[(size_t)i0] * (1.0f - frac) + v.buf[(size_t)i1] * frac;

            // Filtre passe-bas dans la boucle (couleur)
            v.lp = lpCoeff * v.lp + (1.0f - lpCoeff) * delayed;

            float fb = v.lp * v.gain;
            // Securite anti-explosion
            fb = std::clamp(fb, -2.0f, 2.0f);

            float toWrite = exc * v.drive + fb;
            toWrite = std::clamp(toWrite, -2.0f, 2.0f);
            v.buf[(size_t)v.writePos] = toWrite;
            v.writePos++;
            if (v.writePos >= v.bufSize) v.writePos = 0;

            // Sortie : le contenu resonant (delayed), module en amplitude
            float ampMod = 1.0f + shimmer * 0.35f * mod;
            // Fondu vers la cible d'arpege (evite les clics sur les pas)
            v.emph += (v.emphTarget - v.emph) * emphCoeff;
            float vOut = delayed * norm * ampMod * v.emph;

            // Mouvement : modulation d'amplitude (morphose reparti -> unisson)
            if (movDepth > 0.0005f)
            {
                float spread = 1.0f - movUnison;
                float offset = spread * ((float)i / (float)numActive) * 6.2831853f;
                float s = std::sin(movPhase + offset);
                float uni = 0.5f + 0.5f * (std::tanh(movK * s) / movNorm);
                vOut *= (1.0f - movDepth) + movDepth * uni;
            }

            // Pan equal-power
            float pan = v.panBase * spread; // -1..1
            float ang = (pan + 1.0f) * 0.78539816f; // 0..pi/2
            float gL = std::cos(ang);
            float gR = std::sin(ang);
            outL += vOut * gL;
            outR += vOut * gR;
            if (doSplit)
            {
                // 1 = voix jouee par l'arp, 0 = lit (permet nappe + session separees)
                float gate = std::clamp((v.emph - kArpBed) / (1.0f - kArpBed), 0.0f, 1.0f);
                arpSumL += vOut * gL * gate;
                arpSumR += vOut * gR * gate;
            }
        }

        // Anti-DC (bloqueur DC a 5 Hz env.)
        {
            float x = outL;
            float y = x - dcL_x1 + 0.999f * dcL_y1;
            dcL_x1 = x; dcL_y1 = y;
            outL = y;
            x = outR;
            y = x - dcR_x1 + 0.999f * dcR_y1;
            dcR_x1 = x; dcR_y1 = y;
            outR = y;
        }

        // Anti-DC separe pour le bus arpege
        if (arpL != nullptr && arpR != nullptr)
        {
            float x = arpSumL;
            float y = x - dcAL_x1 + 0.999f * dcAL_y1;
            dcAL_x1 = x; dcAL_y1 = y;
            *arpL = y;
            x = arpSumR;
            y = x - dcAR_x1 + 0.999f * dcAR_y1;
            dcAR_x1 = x; dcAR_y1 = y;
            *arpR = y;
        }

        timeSamples++;
    }

private:
    struct Voice
    {
        std::vector<float> buf;
        int bufSize = 1;
        int writePos = 0;
        float lp = 0.0f;
        float delayLen = 100.0f;
        float gain = 0.9f;   // feedback
        float freq = 440.0f;
        int midi = 69;
        float drive = 1.0f;
        float panBase = 0.0f;
        float lfoPhase = 0.0f;
        float emph = 1.0f;        // gain courant de mise en avant (arpege)
        float emphTarget = 1.0f;  // cible
    };

    static double midiToFreq(int m)
    {
        return 440.0 * std::pow(2.0, (m - 69) / 12.0);
    }

    void updateAllDelays()
    {
        for (int i = 0; i < maxVoices; ++i)
        {
            Voice& v = voices[(size_t)i];
            if (v.freq < 20.0) v.freq = 20.0;
            v.delayLen = (float)(sampleRate / v.freq);
            v.delayLen = std::clamp(v.delayLen, 2.0f, (float)(v.bufSize - 4));
        }
        updateAllGains();
        updatePans();
    }

    void updateAllGains()
    {
        for (int i = 0; i < maxVoices; ++i)
        {
            Voice& v = voices[(size_t)i];
            double D = (double)v.delayLen;
            // g pour T60 : g^(T60*sr/D) = 0.001
            double g = std::pow(10.0, -3.0 * D / (t60 * sampleRate));
            g = std::clamp(g, 0.0, 0.9995);
            v.gain = (float)g;
            // Drive legerement plus fort pour les aigus (compensation perceptive)
            double f = v.freq > 0 ? v.freq : 440.0;
            v.drive = (float)std::clamp(std::pow(f / 220.0, 0.15), 0.6, 1.6);
        }
        updatePans();
    }

    void updatePans()
    {
        int n = std::max(1, numActive);
        for (int i = 0; i < maxVoices; ++i)
        {
            if (n == 1) voices[(size_t)i].panBase = 0.0f;
            else
            {
                int idx = std::min(i, n - 1);
                voices[(size_t)i].panBase = ((float)idx / (float)(n - 1)) * 2.0f - 1.0f;
            }
        }
    }

    std::vector<Voice> voices;
    int numActive = 7;
    double sampleRate = 44100.0;
    double t60 = 2.5;
    float lpCoeff = 0.25f;
    float brightness = 0.7f;
    float shimmer = 0.35f;
    float spread = 0.8f;
    double emphSmoothSec = 0.012;
    float emphCoeff = 0.0017f;
    // Mouvement
    float movPhase = 0.0f;
    float movRateHz = 0.8f;
    float movDepth = 0.0f;
    float movShape = 0.2f;
    float movUnison = 1.0f;
    float dcL_x1 = 0, dcL_y1 = 0, dcR_x1 = 0, dcR_y1 = 0;
    float dcAL_x1 = 0, dcAL_y1 = 0, dcAR_x1 = 0, dcAR_y1 = 0;
    bool arpSplitActive = false;
    long long timeSamples = 0;
};
