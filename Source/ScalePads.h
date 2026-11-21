#pragma once
// Pastilles de gamme style piano : rangee du haut = dieses (C# D# F# G# A#),
// rangee du bas = naturelles (C D E F G A B), comme le screen.
// Allumees selon la gamme + tonique actives. Clic = change la tonique.
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>

class ScalePads : public juce::Component
{
public:
    std::function<void(int pitchClass)> onPadClicked;

    void setScale(int root, const std::vector<int>& intervals)
    {
        bool changed = (root != curRoot || intervals != curIntervals);
        curRoot = root;
        curIntervals = intervals;
        if (changed) repaint();
    }

    void setColors(juce::Colour lit, juce::Colour litHi, juce::Colour unlit)
    {
        colLit = lit;
        colLitHi = litHi;
        colUnlit = unlit;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        for (int i = 0; i < 12; ++i)
        {
            int pc;
            float cx, cy;
            padInfo(i, pc, cx, cy);
            float d = (float) padD;
            bool inScale = isInScale(pc);
            bool isRoot = (pc == curRoot);
            juce::Colour fill = inScale ? colLit : colUnlit;
            if (isRoot && inScale) fill = colLitHi;
            g.setColour(fill);
            g.fillEllipse(cx - d * 0.5f, cy - d * 0.5f, d, d);
            if (isRoot)
            {
                g.setColour(juce::Colours::white.withAlpha(0.85f));
                g.drawEllipse(cx - d * 0.5f, cy - d * 0.5f, d, d, 2.0f);
            }
            g.setColour(inScale ? juce::Colours::white.withAlpha(0.95f)
                                : juce::Colours::white.withAlpha(0.35f));
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText(noteName(pc), (int)(cx - 20), (int)(cy - 8), 40, 16,
                       juce::Justification::centred);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        int bestPc = -1;
        float bestDist = 1e9f;
        for (int i = 0; i < 12; ++i)
        {
            int pc;
            float cx, cy;
            padInfo(i, pc, cx, cy);
            float dist = std::hypot((float) e.x - cx, (float) e.y - cy);
            if (dist < bestDist) { bestDist = dist; bestPc = pc; }
        }
        if (bestPc >= 0 && bestDist < (float) padD && onPadClicked)
            onPadClicked(bestPc);
    }

    void setGeometry(int x, int y, int w, int h, int diameter)
    {
        padD = diameter;
        setBounds(x, y, w, h);
    }

private:
    int curRoot = 0;
    std::vector<int> curIntervals = { 0, 2, 4, 5, 7, 9, 11 };
    int padD = 24;
    juce::Colour colLit = juce::Colour(0xffff7a6b);
    juce::Colour colLitHi = juce::Colour(0xffff9d8f);
    juce::Colour colUnlit = juce::Colour(0xff23263d);

    // i 0..6 : blanches en bas ; i 7..11 : dieses en haut (dans les intervalles)
    void padInfo(int i, int& pc, float& cx, float& cy) const
    {
        static const int whites[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static const int sharps[5] = { 1, 3, 6, 8, 10 };
        static const int afterWhite[5] = { 0, 1, 3, 4, 5 };
        float d = (float) padD;
        float step = d + 10.0f; // C D E F G A B : 7 x 34 = 238 de large
        if (i < 7)
        {
            pc = whites[i];
            cx = (float) i * step + d * 0.5f;
            cy = 28.0f + d * 0.5f;
        }
        else
        {
            int k = i - 7;
            pc = sharps[k];
            cx = (float) afterWhite[k] * step + d + 5.0f; // centre de l'intervalle
            cy = d * 0.5f;
        }
    }

    bool isInScale(int pc) const
    {
        int rel = (pc - curRoot + 12) % 12;
        for (int iv : curIntervals)
            if (((iv % 12) + 12) % 12 == rel) return true;
        return false;
    }

    static juce::String noteName(int pc)
    {
        static const char* names[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        return names[pc];
    }
};
