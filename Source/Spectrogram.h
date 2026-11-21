#pragma once
// Spectre rempli style "screen" : courbe d'energie lisse + remplissage degrade
// + couche echo, dans la bande superieure (880x52). Thread audio : RIEN ici.
// Thread UI : pushFromFifo() + update() + draw(), pilotes par le timer.
// Couleurs 100 % issues du skin actif.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

class Spectrogram
{
public:
    static constexpr int FftOrder = 10;
    static constexpr int FftSize = 1 << FftOrder; // 1024
    static constexpr int Cols = 220;

    Spectrogram()
        : fft(FftOrder),
          window((size_t) FftSize, juce::dsp::WindowingFunction<float>::hann, false)
    {
        levels.fill(0.0f);
        echo.fill(0.0f);
    }

    void setSampleRate(double sr)
    {
        if (sr > 0 && sr != sampleRate)
        {
            sampleRate = sr;
            staged.clear();
            stageStart = 0;
            levels.fill(0.0f);
            echo.fill(0.0f);
        }
    }

    void setColors(juce::Colour deep, juce::Colour mid, juce::Colour hot)
    {
        colDeep = deep;
        colMid = mid;
        colHot = hot;
    }

    // Lit le FIFO du processeur (thread UI uniquement). Jette le trop-plein.
    void pushFromFifo(juce::AbstractFifo& fifo, std::vector<float>& buf)
    {
        int ready = fifo.getNumReady();
        if (ready > 4096) // en retard : on saute l'ancien, on garde le frais
        {
            int s1, n1, s2, n2;
            fifo.prepareToRead(ready - 2048, s1, n1, s2, n2);
            fifo.finishedRead(n1 + n2);
            ready = fifo.getNumReady();
        }
        while (ready >= FftSize && (int) staged.size() - stageStart < 4096)
        {
            int s1, n1, s2, n2;
            fifo.prepareToRead(FftSize, s1, n1, s2, n2);
            for (int i = 0; i < n1; ++i) staged.push_back(buf[(size_t)(s1 + i)]);
            for (int i = 0; i < n2; ++i) staged.push_back(buf[(size_t)(s2 + i)]);
            fifo.finishedRead(n1 + n2);
            ready = fifo.getNumReady();
        }
        if (stageStart > 8192) // compacte rarement
        {
            staged.erase(staged.begin(), staged.begin() + stageStart);
            stageStart = 0;
        }
    }

    // Consomme au plus 2 FFT par tick. Rend vrai si contenu neuf.
    bool update()
    {
        bool fresh = false;
        for (int k = 0; k < 2; ++k)
        {
            if ((int) staged.size() - stageStart < FftSize)
                break;
            for (int i = 0; i < FftSize; ++i)
                work[(size_t) i] = staged[(size_t)(stageStart + i)];
            stageStart += FftSize;
            window.multiplyWithWindowingTable(work.data(), (size_t) FftSize);
            fft.performFrequencyOnlyForwardTransform(work.data());
            analyseFrame();
            fresh = true;
        }
        return fresh;
    }

    void draw(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float W = area.getWidth();
        float H = area.getHeight();
        float base = H - 2.0f;
        float top = 6.0f;
        float span = base - top;

        auto colX = [&](int c) { return area.getX() + (float) c * W / (float)(Cols - 1); };

        // Couche echo (trainee, discrète)
        {
            juce::Path e;
            e.startNewSubPath(area.getX(), base);
            for (int c = 0; c < Cols; ++c)
                e.lineTo(colX(c), base - echo[(size_t) c] * span);
            e.lineTo(area.getX() + W, base);
            e.closeSubPath();
            g.setColour(colMid.withAlpha(0.14f));
            g.fillPath(e);
        }
        // Remplissage principal (degrade vertical)
        {
            juce::Path p;
            p.startNewSubPath(area.getX(), base);
            for (int c = 0; c < Cols; ++c)
                p.lineTo(colX(c), base - levels[(size_t) c] * span);
            p.lineTo(area.getX() + W, base);
            p.closeSubPath();
            juce::ColourGradient grad(colMid.withAlpha(0.55f), 0.0f, top,
                                      colMid.withAlpha(0.0f), 0.0f, base, false);
            g.setGradientFill(grad);
            g.fillPath(p);
        }
        // Arete lumineuse
        {
            juce::Path edge;
            for (int c = 0; c < Cols; ++c)
            {
                float x = colX(c);
                float y = base - levels[(size_t) c] * span;
                if (c == 0) edge.startNewSubPath(x, y);
                else edge.lineTo(x, y);
            }
            g.setColour(colHot);
            g.strokePath(edge, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        }
    }

private:
    void analyseFrame()
    {
        for (int c = 0; c < Cols; ++c)
        {
            // Echelle log 40 Hz -> 20 kHz, max sur 3 bins (pics francs)
            double f = 40.0 * std::pow(500.0, (double) c / (double)(Cols - 1));
            int bin = (int) std::round(f * FftSize / sampleRate);
            float mag = 0.0f;
            for (int b = bin - 1; b <= bin + 1; ++b)
            {
                int bb = std::clamp(b, 1, FftSize / 2 - 1);
                mag = std::max(mag, work[(size_t) bb]);
            }
            float db = 20.0f * std::log10(mag * 4.0f / (float) FftSize + 1e-7f);
            float t = std::clamp((db + 84.0f) / 84.0f, 0.0f, 1.0f);
            float prev = levels[(size_t) c];
            levels[(size_t) c] = prev + (t - prev) * (t > prev ? 0.6f : 0.12f);
            float pe = echo[(size_t) c];
            echo[(size_t) c] = pe + (levels[(size_t) c] - pe) * 0.05f;
        }
    }

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::vector<float> staged;
    int stageStart = 0;
    // 2x : performFrequencyOnlyForwardTransform ecrit size flottants APRES l'entree
    std::array<float, FftSize * 2> work = {};
    std::array<float, Cols> levels = {};
    std::array<float, Cols> echo = {};
    double sampleRate = 44100.0;
    juce::Colour colDeep = juce::Colour(0xff123a6a);
    juce::Colour colMid = juce::Colour(0xff35e0ff);
    juce::Colour colHot = juce::Colour(0xffd8f4ff);
};
