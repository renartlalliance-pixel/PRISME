#pragma once
#include <string>
#include <vector>
#include <array>
#include <cmath>

namespace PrismeScales
{
    inline std::vector<std::string> getRootNames()
    {
        return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    }

    struct ScaleDef
    {
        std::string name;
        std::vector<int> intervals; // semitones from root
    };

    // Table construite UNE fois (thread-safe) : aucune allocation ensuite.
    inline const std::vector<ScaleDef>& getScalesRef()
    {
        static const std::vector<ScaleDef> scales = {
            { "Major",            { 0, 2, 4, 5, 7, 9, 11 } },
            { "Minor Naturel",    { 0, 2, 3, 5, 7, 8, 10 } },
            { "Dorian",           { 0, 2, 3, 5, 7, 9, 10 } },
            { "Phrygien",         { 0, 1, 3, 5, 7, 8, 10 } },
            { "Lydien",           { 0, 2, 4, 6, 7, 9, 11 } },
            { "Mixolydien",       { 0, 2, 4, 5, 7, 9, 10 } },
            { "Penta Majeur",     { 0, 2, 4, 7, 9 } },
            { "Penta Mineur",     { 0, 3, 5, 7, 10 } },
            { "Accord Maj7",      { 0, 4, 7, 11 } },
            { "Accord Min7",      { 0, 3, 7, 10 } },
            { "Accord Dom7",      { 0, 4, 7, 10 } },
            { "Accord Majeur",    { 0, 4, 7 } },
            { "Accord Mineur",    { 0, 3, 7 } },
            { "Diminue",          { 0, 3, 6 } },
            { "Augmente",         { 0, 4, 8 } },
            { "Accord Sus2",      { 0, 2, 7 } },
            { "Accord Sus4",      { 0, 5, 7, 12 } },
            { "Accord Add9",      { 0, 4, 7, 14 } },
            { "Accord Six",       { 0, 4, 7, 9 } },
            { "Octave + Quinte",  { 0, 7, 12 } },
            { "Chromatique",      { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
        };
        return scales;
    }

    inline std::vector<ScaleDef> getScales() { return getScalesRef(); }

    inline std::vector<std::string> getScaleNames()
    {
        std::vector<std::string> names;
        for (auto& s : getScales()) names.push_back(s.name);
        return names;
    }

    inline std::vector<std::string> getOctaveNames()
    {
        return { "-1 Grave", "0 Medium", "+1 Aigu" };
    }

    inline std::vector<std::string> getArpPatternNames()
    {
        return { "Haut", "Bas", "Haut-Bas", "Aleatoire", "Accord" };
    }

    inline std::vector<std::string> getArpDivNames()
    {
        return { "1/2", "1/4", "1/8", "1/16", "1/8T", "1/16T" };
    }

    // Multiplicateur de noire pour chaque division rythmique
    inline double getArpDivMult(int divIndex)
    {
        switch (divIndex)
        {
            case 0: return 2.0;
            case 1: return 1.0;
            case 2: return 0.5;
            case 3: return 0.25;
            case 4: return 1.0 / 3.0;
            case 5: return 1.0 / 6.0;
            default: return 0.5;
        }
    }

    inline int getScaleToneCount(int scaleIndex)
    {
        auto scales = getScales();
        if (scaleIndex < 0 || scaleIndex >= (int) scales.size())
            return 7;
        return (int) scales[(size_t) scaleIndex].intervals.size();
    }

    // Construit la liste de notes MIDI pour les resonateurs.
    // baseMidi = 48 (C3) + root + octaveShift*12
    // Remplit jusqu'a maxVoices en montant dans la gamme sur plusieurs octaves.
    inline std::vector<int> buildTuning(int root, int scaleIndex, int octaveShiftIndex, int maxVoices = 12)
    {
        auto scales = getScales();
        if (scaleIndex < 0 || scaleIndex >= (int) scales.size())
            scaleIndex = 0;

        auto intervals = scales[(size_t) scaleIndex].intervals;
        int octaveShift = octaveShiftIndex - 1; // 0->-1, 1->0, 2->+1
        int baseMidi = 48 + root + octaveShift * 12;

        std::vector<int> notes;
        notes.reserve((size_t) maxVoices);
        int scaleSize = (int) intervals.size();
        for (int i = 0; i < maxVoices; ++i)
        {
            int degree = i % scaleSize;
            int oct = i / scaleSize;
            int midi = baseMidi + intervals[(size_t) degree] + oct * 12;
            // Securite : reste dans 24..96 (C1..C7)
            while (midi > 96) midi -= 12;
            while (midi < 24) midi += 12;
            notes.push_back(midi);
        }
        return notes;
    }

    // Variante temps reel : remplit `notes` sans allocation (capacite >= maxVoices
    // reservee par l'appelant). Meme resultat que buildTuning.
    inline void buildTuningInto(std::vector<int>& notes, int root, int scaleIndex,
                                int octaveShiftIndex, int maxVoices = 12)
    {
        const auto& scales = getScalesRef();
        if (scaleIndex < 0 || scaleIndex >= (int) scales.size())
            scaleIndex = 0;
        const auto& intervals = scales[(size_t) scaleIndex].intervals;
        int octaveShift = octaveShiftIndex - 1;
        int baseMidi = 48 + root + octaveShift * 12;
        notes.clear();
        int scaleSize = (int) intervals.size();
        for (int i = 0; i < maxVoices; ++i)
        {
            int degree = i % scaleSize;
            int oct = i / scaleSize;
            int midi = baseMidi + intervals[(size_t) degree] + oct * 12;
            while (midi > 96) midi -= 12;
            while (midi < 24) midi += 12;
            notes.push_back(midi);
        }
    }

    inline double midiToFreq(int midi)
    {
        return 440.0 * std::pow(2.0, (midi - 69) / 12.0);
    }
}
