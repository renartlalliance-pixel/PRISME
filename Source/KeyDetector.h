#pragma once
// Detection de tonalite (majeur/mineur) pour l'AUTO KEY de PRISME.
// Methode : chromagramme (FFT 8192, Hann) moyenne dans le temps, correle avec
// les 24 profils de tonalite de Krumhansl-Kessler (Krumhansl 1990).
// THREAD MESSAGE UNIQUEMENT : jamais appele depuis processBlock.
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <algorithm>

struct PrismeKeyDetector
{
    static constexpr int fftOrder = 13;
    static constexpr int fftSize = 1 << fftOrder;

    struct Result
    {
        int root = 0;        // 0..11 (C..B)
        bool minor = false;
        float corr = 0.0f;   // correlation du meilleur candidat (-1..1)
        float margin = 0.0f; // ecart avec le 2e meilleur (confiance)
    };

    PrismeKeyDetector()
        : fft(fftOrder),
          ring((size_t) fftSize, 0.0f),
          fftData((size_t) (2 * fftSize), 0.0f),
          window((size_t) fftSize, 0.0f)
    {
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi
                                                          * (float) i / (float) (fftSize - 1)));
        reset();
    }

    void reset()
    {
        std::fill(ring.begin(), ring.end(), 0.0f);
        wpos = 0;
        filled = 0;
        hasAvg = false;
        for (auto& c : avg) c = 0.0f;
    }

    void pushSamples(const float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) wpos] = x[i];
            wpos = (wpos + 1) & (fftSize - 1);
        }
        filled = std::min(filled + n, fftSize);
    }

    // Analyse la fenetre la plus recente. false = pas assez de signal (silence
    // ou fenetre pas encore pleine) : le chroma moyen n'est alors pas modifie.
    bool analyse(double sampleRate, Result& out)
    {
        if (filled < fftSize || sampleRate < 8000.0)
            return false;

        double energy = 0.0;
        for (int j = 0; j < fftSize; ++j)
        {
            float s = ring[(size_t) ((wpos + j) & (fftSize - 1))];
            energy += (double) s * (double) s;
            fftData[(size_t) j] = s * window[(size_t) j];
        }
        if (std::sqrt(energy / (double) fftSize) < 0.001) // < -60 dBFS : silence
            return false;
        std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);

        fft.performFrequencyOnlyForwardTransform(fftData.data());

        // 100 Hz..4 kHz : en dessous, 8192 points ne separent pas les demi-tons
        float frame[12] = {};
        const double binHz = sampleRate / (double) fftSize;
        const int kMin = (int) std::ceil(100.0 / binHz);
        const int kMax = std::min((int) std::floor(4000.0 / binHz), fftSize / 2);
        float sum = 0.0f;
        for (int k = kMin; k <= kMax; ++k)
        {
            double midi = 69.0 + 12.0 * std::log2(((double) k * binHz) / 440.0);
            int pc = (((int) std::lround(midi)) % 12 + 12) % 12;
            float v = std::sqrt(fftData[(size_t) k]); // compression : evite qu'une basse ecrase tout
            frame[pc] += v;
            sum += v;
        }
        if (sum <= 1.0e-6f)
            return false;
        for (auto& c : frame) c /= sum;

        // Moyenne exponentielle (~3 s a 10 analyses/s) : la tonalite ne saute pas a chaque note
        if (! hasAvg)
        {
            for (int i = 0; i < 12; ++i) avg[i] = frame[i];
            hasAvg = true;
        }
        else
        {
            for (int i = 0; i < 12; ++i) avg[i] = avg[i] * 0.97f + frame[i] * 0.03f;
        }

        // Profils Krumhansl-Kessler
        static const float majP[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
                                        2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
        static const float minP[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
                                        2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

        float best = -2.0f, second = -2.0f;
        int bestRoot = 0;
        bool bestMinor = false;
        for (int minor = 0; minor < 2; ++minor)
        {
            const float* p = minor ? minP : majP;
            for (int r = 0; r < 12; ++r)
            {
                float c = pearson(r, p);
                if (c > best)
                {
                    second = best;
                    best = c; bestRoot = r; bestMinor = (minor == 1);
                }
                else if (c > second)
                {
                    second = c;
                }
            }
        }
        out.root = bestRoot;
        out.minor = bestMinor;
        out.corr = best;
        out.margin = best - second;
        return true;
    }

private:
    // Correlation de Pearson entre le chroma (decale de r) et un profil
    float pearson(int r, const float* p) const
    {
        float mx = 0.0f, my = 0.0f;
        for (int i = 0; i < 12; ++i) { mx += avg[(i + r) % 12]; my += p[i]; }
        mx /= 12.0f; my /= 12.0f;
        float sxy = 0.0f, sxx = 0.0f, syy = 0.0f;
        for (int i = 0; i < 12; ++i)
        {
            float dx = avg[(i + r) % 12] - mx;
            float dy = p[i] - my;
            sxy += dx * dy; sxx += dx * dx; syy += dy * dy;
        }
        float d = std::sqrt(sxx * syy);
        return d > 1.0e-9f ? sxy / d : 0.0f;
    }

    juce::dsp::FFT fft;
    std::vector<float> ring, fftData, window;
    int wpos = 0, filled = 0;
    bool hasAvg = false;
    float avg[12] = {};
};
