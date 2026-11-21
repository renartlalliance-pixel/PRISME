#pragma once
// RANDOMIZE WAVE : plan de variation precalcule + rendu hors ligne.
//
// Principe central : le plan complet (evenements sur grille musicale) est
// construit AVANT le rendu, avec une seed. Meme source + meme config +
// meme seed + meme version moteur = meme plan.
//
// Moyens reels reutilises (constat etape A) :
//   - ArpFX::Engine (type Delay, autonome plugin/offline) pour le delay
//     synchronise au tempo (k1 calcule depuis la division musicale).
//   - BlurReverb (autonome plugin/offline) pour la reverb.
//   - Glitch/reverse = operations de buffer (pas de DSP requis).
// Limites documentees (pas de faux-semblant) :
//   - AUCUN pitch shifter temps-reel n'existe dans PRISME : implemente ici
//     un pitch granulaire offline qui preserve la duree (correct a +/-12 st,
//     artefacts possibles aux extremes, signales dans le resume du plan).
//   - AUCUN formant shifter independant n'existe : le "formant" est une
//     coloration resonante (2 pics deplaces independamment du pitch),
//     honnetement etiquetee comme telle, jamais comme du LPC.
// Tout le rendu tourne sur thread de travail (jamais audio, jamais UI).
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <cstdint>
#include <random>
#include <string>

namespace VarLab
{
    // 2 : la palette est reduite de 1 ou 2 effets par generation (tires de la
    //     seed). La version 1 utilisait toujours les 8 effets.
    inline constexpr int engineVersion = 2;

    enum class Kind { Glitch, Reverse, Pitch, Formant, Delay, Reverb, Grain, Shimmer };

    inline const char* kindName(Kind k)
    {
        switch (k)
        {
            case Kind::Glitch:  return "glitch";
            case Kind::Reverse: return "reverse";
            case Kind::Pitch:   return "pitch";
            case Kind::Formant: return "formant";
            case Kind::Delay:   return "delay";
            case Kind::Reverb:  return "reverb";
            case Kind::Grain:   return "grain";
            case Kind::Shimmer: return "shimmer";
        }
        return "?";
    }

    struct Grid
    {
        double bpm = 120.0;
        int sigNum = 4, sigDen = 4;
        int bars = 8;
        double sr = 44100.0;
        double beatsPerBar() const
        {
            return (double) juce::jlimit(1, 16, sigNum) * 4.0
                 / (double) juce::jlimit(1, 32, sigDen);
        }
        double totalBeats() const { return beatsPerBar() * (double) bars; }
        double beatSec() const { return 60.0 / bpm; }
        int totalSamples() const
        {
            return (int) (totalBeats() * beatSec() * sr);
        }
    };

    struct Event
    {
        Kind kind = Kind::Glitch;
        double startBeats = 0.0; // quantifie sur la grille
        double lenBeats = 1.0;
        float p[4] = { 0, 0, 0, 0 }; // parametres selon kind (voir moteur)
        uint32_t aux = 0; // divers deterministe (ex : permutation du shuffle)
        // Glitch : p0=subdiv(beats), p1=repetitions, p2=xfade(0..1),
        //          p3=mode (0 = stutter/repeat, 1 = shuffle/rearrange).
        // Reverse : p0 inutilise.
        // Pitch : p0=demi-tons (-12..+12), p1=mix(0..1).
        // Formant : p0=deplacement(st, independant du pitch), p1=gain pics.
        // Delay : p0=division(beats : 1, .5, .75, .25, 1.5), p1=feedback(0..0.85),
        //         p2=wet(0..1), p3=spill(beats).
        // Reverb : p0=size(0..1), p1=decayMul(0.5..2.5), p2=wet(0..1),
        //          p3=spill(beats).
        // Grain (granulaire leger) : p0=grain(ms 30..80), p1=scatter(beats),
        //         p2=mix(0..1). Duree preservee, diffusion douce.
        // Shimmer (leger) : p0=mix(0..1, bas), p1=tail(beats). Octave +12
        //         en nappe qui s'eteint, jamais envahissant.
    };

    struct Plan
    {
        Grid grid;
        uint32_t seed = 0;
        int intensity = 50; // 0..100
        std::vector<Event> events;
        // Palette reduite de la generation : le bit k vaut 1 quand l'effet
        // (int) Kind k a ete retire au hasard pour TOUTE cette generation.
        // 1 ou 2 bits a 1. Tire de la seed : reproductible.
        uint8_t mutedMask = 0;
        // Effet vedette de la generation (mode "one shot" de la LED SAMPLES) :
        // 0..7 = cet effet domine le plan ; -1 = aucun (tirage au hasard).
        int featuredKind = -1;
        juce::String summary; // resume lisible (calibrage, limites)
        bool tempoDefaulted = false; // true = 120 utilise faute de transport
        // exactBars = coupe net a la grille (pas de queue) : le fichier fait
        // EXACTEMENT bars mesures, pret a poser sur la timeline sans stretch.
        // Les spills sont rognes a la fin + fondu de sortie (80 ms).
        bool exactBars = false;
    };

    // Construit le plan complet (rapide, appelable thread message).
    // Ne touche a aucun fichier : pur calcul determine par la seed.
    // featuredKind : 0..7 = mode "one shot" (un effet vedette domine la
    // generation et n'est jamais retire) ; -1 = tirage au hasard (defaut).
    Plan buildPlan(const Grid& grid, uint32_t seed, int intensity,
                   int featuredKind = -1);

    // Resultat du rendu (rempli par le thread de travail).
    struct RenderResult
    {
        bool ok = false;
        juce::String error;
        std::vector<float> outL, outR;
        int outSamples = 0;
        double outSr = 44100.0;
        double tailSec = 0.0; // queue conservee au-dela de la duree musicale
        std::vector<float> peaks; // miniature 120 cretes
    };

    // Rend le plan a partir de la source (thread de travail uniquement).
    // La source est tuilee (bouclee) sur la grille ; jamais modifiee.
    RenderResult renderPlan(const Plan& plan,
                            const float* srcL, const float* srcR, int srcLen,
                            std::atomic<double>* progress,
                            const std::atomic<bool>* cancel = nullptr);
}
