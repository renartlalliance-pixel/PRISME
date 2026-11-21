#pragma once
// Spectrogramme 3D en perspective (vue RECORDER) : X = frequence (log),
// Y = temps (defilement), Z = energie (relief). Rendu 2.5D en perspective
// avec JUCE uniquement (aucune dependance 3D materielle) : rangees
// spectrales reculees vers l'horizon, cretes en relief, grille discrete.
// Thread audio : RIEN ici. Thread UI : pushFromFifo() + update() + draw(),
// pilotes par le timer de l'editeur. Couleurs 100 % issues du skin actif.
// Consomme le MEME FIFO que le spectrogramme 2D : un seul des deux doit
// drainer a la fois (l'editeur arbitre selon la vue visible).
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>
#include <cmath>

class Spectro3D
{
public:
    static constexpr int FftOrder = 10;
    static constexpr int FftSize = 1 << FftOrder; // 1024
    static constexpr int Bins = 64;  // colonnes log 40 Hz -> 18 kHz
    static constexpr int Rows = 44;  // historique (~1 s a 48 kHz)

    Spectro3D()
        : fft(FftOrder),
          window((size_t) FftSize, juce::dsp::WindowingFunction<float>::hann, false)
    {
        staged.reserve(8192);
        for (auto& r : history) r.fill(0.0f);
    }

    void setSampleRate(double sr)
    {
        if (sr > 0 && sr != sampleRate)
        {
            sampleRate = sr;
            staged.clear();
            stageStart = 0;
            for (auto& r : history) r.fill(0.0f);
            head = 0;
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
        if (ready > 4096)
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
            for (int i = 0; i < n1; ++i) staged.push_back(buf[(size_t) (s1 + i)]);
            for (int i = 0; i < n2; ++i) staged.push_back(buf[(size_t) (s2 + i)]);
            fifo.finishedRead(n1 + n2);
            ready = fifo.getNumReady();
        }
        if (stageStart > 8192)
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
                work[(size_t) i] = staged[(size_t) (stageStart + i)];
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
        // Fond graphite fixe (les cretes portent les couleurs du skin).
        g.setColour(juce::Colour(0xff0a0c10));
        g.fillRoundedRectangle(area, 8.0f);

        float cx = area.getCentreX();
        float baseY = area.getBottom() - 10.0f;
        float depthH = area.getHeight() - 30.0f;
        float halfW = area.getWidth() * 0.5f - 8.0f;

        // Grille discrete : lignes de profondeur + verticales log.
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        for (int r = 0; r < Rows; r += 6)
        {
            float d = (float) r / (float) (Rows - 1);
            float y = baseY - d * depthH;
            float s = 1.0f / (1.0f + d * 0.9f);
            g.drawLine(cx - halfW * s, y, cx + halfW * s, y, 1.0f);
        }
        for (int c = 0; c < Bins; c += 8)
        {
            float fx = (float) c / (float) (Bins - 1);
            float x = cx + (fx - 0.5f) * 2.0f * halfW;
            g.drawLine(x, baseY, cx + (fx - 0.5f) * 2.0f * halfW * 0.52f,
                       baseY - depthH, 1.0f);
        }

        // Cretes : anciennes d'abord (fond), recentes en avant-plan.
        for (int r = Rows - 1; r >= 0; --r)
        {
            const auto& row = history[(size_t) ((head + r) % Rows)];
            float d = (float) r / (float) (Rows - 1);
            float s = 1.0f / (1.0f + d * 0.9f);
            float yB = baseY - d * depthH;
            float ridgeH = (area.getHeight() * 0.42f) * s;
            float alpha = 0.28f + 0.72f * (1.0f - d);
            juce::Colour fill = colMid.withAlpha(0.35f * alpha)
                                    .interpolatedWith(colDeep, d * 0.7f);

            juce::Path p;
            float x0 = cx - halfW * s;
            p.startNewSubPath(x0, yB);
            for (int c = 0; c < Bins; ++c)
            {
                float fx = (float) c / (float) (Bins - 1);
                float x = cx + (fx - 0.5f) * 2.0f * halfW * s;
                p.lineTo(x, yB - row[(size_t) c] * ridgeH);
            }
            p.lineTo(cx + halfW * s, yB);
            p.closeSubPath();
            g.setColour(fill);
            g.fillPath(p);

            // Arete lumineuse sur les rangees avant (cout maitrise).
            if (r < 8)
            {
                juce::Path edge;
                for (int c = 0; c < Bins; ++c)
                {
                    float fx = (float) c / (float) (Bins - 1);
                    float x = cx + (fx - 0.5f) * 2.0f * halfW * s;
                    float y = yB - row[(size_t) c] * ridgeH;
                    if (c == 0) edge.startNewSubPath(x, y);
                    else edge.lineTo(x, y);
                }
                g.setColour(colHot.withAlpha(alpha));
                g.strokePath(edge, juce::PathStrokeType(1.2f,
                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            // Pastilles des pics forts (accents).
            if (r < 12)
            {
                g.setColour(colHot.withAlpha(0.85f * alpha));
                for (int c = 0; c < Bins; ++c)
                {
                    if (row[(size_t) c] > 0.85f)
                    {
                        float fx = (float) c / (float) (Bins - 1);
                        float x = cx + (fx - 0.5f) * 2.0f * halfW * s;
                        g.fillEllipse(x - 1.5f, yB - row[(size_t) c] * ridgeH - 1.5f,
                                      3.0f, 3.0f);
                    }
                }
            }
        }
        g.setColour(juce::Colour(0xff2a3a55).withAlpha(0.6f));
        g.drawRoundedRectangle(area, 8.0f, 1.0f);
    }

private:
    void analyseFrame()
    {
        std::array<float, Bins> row = {};
        for (int c = 0; c < Bins; ++c)
        {
            double f = 40.0 * std::pow(450.0, (double) c / (double) (Bins - 1));
            int bin = (int) std::round(f * FftSize / sampleRate);
            float mag = 0.0f;
            for (int b = bin - 1; b <= bin + 1; ++b)
            {
                int bb = std::clamp(b, 1, FftSize / 2 - 1);
                mag = std::max(mag, work[(size_t) bb]);
            }
            float db = 20.0f * std::log10(mag * 4.0f / (float) FftSize + 1e-7f);
            row[(size_t) c] = std::clamp((db + 84.0f) / 84.0f, 0.0f, 1.0f);
        }
        // Lissage attaque/retombe (stable, sans sauts) + push historique.
        auto& prev = history[(size_t) head];
        for (int c = 0; c < Bins; ++c)
        {
            float t = row[(size_t) c], p = prev[(size_t) c];
            row[(size_t) c] = p + (t - p) * (t > p ? 0.6f : 0.12f);
        }
        head = (head + 1) % Rows;
        history[(size_t) head] = row;
    }

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::vector<float> staged;
    int stageStart = 0;
    // 2x : performFrequencyOnlyForwardTransform ecrit size flottants APRES l'entree
    std::array<float, FftSize * 2> work = {};
    std::array<std::array<float, Bins>, Rows> history;
    int head = 0;
    double sampleRate = 44100.0;
    juce::Colour colDeep = juce::Colour(0xff2a7a9a);
    juce::Colour colMid = juce::Colour(0xff7df9ff);
    juce::Colour colHot = juce::Colour(0xffd8fbff);
};
