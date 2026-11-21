#pragma once
// Limiteur de sortie PRISME : la toute fin de la chaine, en 3 etages.
//
//   1. LIMITEUR DE CRETES stereo-lie, place AVANT le potard Sortie.
//      Sous le seuil, le gain vaut exactement 1 : le signal est inchange.
//      Au-dessus, il reduit avec une attaque courte et un relachement long.
//      Detecteur sur l'ENTREE et gain commun aux 2 canaux : l'image stereo ne
//      bouge pas. Pas de look-ahead : AUCUNE latence a declarer au DAW.
//   2. GAIN MATCH, place AVANT le potard Sortie : le niveau sortant suit le
//      niveau ENTRE dans le plugin (mesures RMS ~500 ms, correction lente
//      plafonnee a 6 dB/s pour ne pas pomper). Applique avant le potard, qui
//      garde donc toute son autorite.
//   3. PLAFOND ABSOLU, place APRES le potard Sortie : borne mathematique de la
//      sortie. Transparent bit a bit sous la genouillere : il ne touche que ce
//      qui depasse. C'est lui qui protege le DAW, quoi que fasse le potard.
//
// Remplace l'ancien "tanh" final, qui colorait deja les sons faibles (pas de
// seuil, pas d'attaque/relachement, aucune compensation de niveau).
//
// Aucune dependance JUCE, aucune allocation, aucun verrou : testable en console
// (Tools/TestLimiter.cpp) et utilisable tel quel dans le thread audio.
#include <algorithm>
#include <cmath>

class PrismeOutputLimiter
{
public:
    // Seuils. Marge volontaire avant 0 dBFS.
    static constexpr float kThresh  = 0.8414f;   // -1.5 dBFS (limiteur)
    static constexpr float kKnee    = 0.9120f;   // -0.8 dBFS (debut du plafond)
    static constexpr float kCeiling = 0.9661f;   // -0.3 dBFS (plafond absolu)
    static constexpr float kMinGain = 0.0625f;   // -24 dB : plancher de reduction
    static constexpr float kMinMatch = 0.2512f;  // -12 dB : bornes du gain match
    static constexpr float kMaxMatch = 3.9811f;  // +12 dB
    static constexpr float kMaxSlewDb = 6.0f;    // vitesse maximale (dB/s)

    void reset(double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        aAtt   = coef(0.001);    // attaque 1 ms
        aRel   = coef(0.120);    // relachement 120 ms
        aRms   = coef(0.500);    // fenetre RMS 500 ms
        holdMax = (int) (sr * 0.025);   // maintien 25 ms (couvre jusqu'a 40 Hz)
        slewUp = std::pow(10.0f, (float) (kMaxSlewDb / 20.0 / sr)) - 1.0f;
        env       = 0.0f;
        hold      = 0;
        inRms     = 0.0f;
        outRms    = 0.0f;
        matchGain = 1.0f;
        gr        = 1.0f;
    }

    // Etage 1 : limiteur de cretes stereo-lie.
    // Le maintien est indispensable : sans lui l'enveloppe redescend ENTRE deux
    // cretes (surtout sur les basses) et le gain remonte juste avant la crete
    // suivante, ce qui laissait passer la crete au-dessus du seuil.
    void processLimiter(float& l, float& r)
    {
        const float pk = std::max(std::fabs(l), std::fabs(r));

        if (pk > env)
        {
            env += (pk - env) * aAtt;   // montee douce (1 ms)
            hold = holdMax;             // le maintien repart a chaque crete
        }
        else if (hold > 0)
        {
            --hold;                     // maintien : pas de chute entre 2 cretes
        }
        else
        {
            env += (pk - env) * aRel;   // relachement (120 ms)
        }

        const float g = (env > kThresh) ? std::max(kMinGain, kThresh / env) : 1.0f;
        gr = g;
        l *= g;
        r *= g;
    }

    // Etage 2 : gain match (inL/inR = entree du plugin, avant tout traitement).
    void processGainMatch(float& l, float& r, float inL, float inR)
    {
        inRms  += (inL * inL + inR * inR - inRms)  * aRms;
        outRms += (l * l + r * r - outRms) * aRms;

        // Silence : on ne poursuit pas du bruit de fond, le gain reste en place.
        if (inRms > 1e-10f && outRms > 1e-10f)
        {
            float want = std::clamp(std::sqrt(inRms / outRms), kMinMatch, kMaxMatch);
            if (want > matchGain) matchGain = std::min(want, matchGain * (1.0f + slewUp));
            else                  matchGain = std::max(want, matchGain * (1.0f - slewUp));
        }

        l *= matchGain;
        r *= matchGain;
    }

    // Etage 3 : plafond absolu, a appeler APRES le potard Sortie.
    // Sous la genouillere : identique bit a bit. Au-dessus : courbe douce bornee
    // par kCeiling (la sortie ne peut jamais l'atteindre, encore moins la passer).
    static float ceiling(float x)
    {
        const float a = std::fabs(x);
        if (a <= kKnee) return x;
        const float over = kCeiling - kKnee;
        const float y = kKnee + over * std::tanh((a - kKnee) / over);
        return x < 0.0f ? -y : y;
    }

    float gainReduction() const { return gr; }        // 1 = ne limite pas
    float match() const { return matchGain; }         // gain de compensation

private:
    float coef(double secondes) const
    {
        return 1.0f - (float) std::exp(-1.0 / (secondes * sr));
    }

    double sr = 44100.0;
    float aAtt = 0.02f, aRel = 0.0002f, aRms = 0.00005f, slewUp = 0.0f;
    int holdMax = 1102, hold = 0;
    float env = 0.0f, inRms = 0.0f, outRms = 0.0f, matchGain = 1.0f, gr = 1.0f;
};
