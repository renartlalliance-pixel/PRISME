#pragma once
// CRYSTALLIZE : embellisseur a 1 potard pour la session arpegee.
// Chaine exciter : saturation douce -> High Shelf (air) -> Low Shelf (bas
//                  adouci) -> micro-chorus -> LFO stereo 1/4 -> soft clipper.
// m = 0..1 (lisse en externe). A m=0, process() recopie l'entree a l'identique.
// Reutilise PrismeBiquad (EQ4Band) et ArpFX::Engine fige en chorus.
// Aucun parametre supplementaire : tout est dose par le seul potard m.
#include <cmath>
#include <algorithm>
#include "EQ4Band.h"
#include "ArpFX.h"

class Crystallize
{
public:
    void setSampleRate(double sr)
    {
        sampleRate = sr > 0 ? sr : 44100.0;
        chorus.setSampleRate(sampleRate);
        chorus.reset();
        lastShelfDb = 999.0f;
        lastLowDb = 999.0f;
        lfoPhase = 0.0;
    }

    void reset()
    {
        shelfL.reset();
        shelfR.reset();
        lowL.reset();
        lowR.reset();
        chorus.reset();
        lastShelfDb = 999.0f;
        lastLowDb = 999.0f;
        lfoPhase = 0.0;
    }

    // Cadence du LFO stereo : 1/4 = un cycle par temps.
    // Le tempo vient de l'hote (aucun controle dans l'interface).
    // Defaut 2.0 Hz = noire a 120 BPM.
    void setBeatHz(double hz)
    {
        beatHz = (hz > 0.05 && hz < 20.0) ? hz : 2.0;
    }

    // Loi de controle musicale (accentuee) :
    //  drive 1->7 (progression ^2), aigu +17 dB (^1.4), bas -6 dB,
    //  chorus 0->3.8 ms, LFO stereo 1/4 dose par le potard.
    // La partie traitee suit une RACINE CARREE de la dose : l'effet s'entend
    // des le quart du potard, alors qu'avec une loi lineaire il fallait
    // depasser 70 % pour l'entendre nettement. A m=1, le resultat est celui
    // de l'ancienne loi lineaire.
    // A m=0 : recopie exacte, bit a bit.
    inline void process(float inL, float inR, float& outL, float& outR, float m)
    {
        m = std::clamp(m, 0.0f, 1.0f);

        // Aigu releve (le "verre") et bas adouci (moins de boue harmonique).
        // Les deux suivent la dose : a m=0 les filtres sont neutres.
        const float shelfDb = kShelfMaxDb * std::pow(m, 1.4f);
        const float lowDb = kLowCutDb * m;
        if (std::fabs(shelfDb - lastShelfDb) > 0.02f || std::fabs(lowDb - lastLowDb) > 0.02f)
        {
            shelfL.setHighShelf(sampleRate, kShelfHz, shelfDb);
            shelfR.setHighShelf(sampleRate, kShelfHz, shelfDb);
            lowL.setLowShelf(sampleRate, kLowHz, lowDb);
            lowR.setLowShelf(sampleRate, kLowHz, lowDb);
            lastShelfDb = shelfDb;
            lastLowDb = lowDb;
        }

        const float drive = 1.0f + kDriveMax * m * m;
        const float norm = 1.0f / std::sqrt(drive); // compromis : details releves, cretes contenues
        const float dL = std::tanh(inL * drive) * norm;
        const float dR = std::tanh(inR * drive) * norm;
        const float sL = lowL.process(shelfL.process(dL));
        const float sR = lowR.process(shelfR.process(dR));
        float wL = 0.0f, wR = 0.0f;
        chorus.process(sL, sR, wL, wR, 2, kChorusK1, m * 0.35f); // micro-chorus fixe

        // Partie traitee : tanh borne les pics du chorus, puis la dose suit une
        // racine carree. A m=0 : exactement 0, la recopie reste bit a bit.
        float tL = std::tanh(wL) * std::sqrt(m);
        float tR = std::tanh(wR) * std::sqrt(m);

        // LFO stereo 1/4 : balance douce gauche/droite, un cycle par temps.
        // Normalise en energie (1/sqrt(1+pan^2)) : rien ne monte en niveau,
        // seule l'image stereo bouge.
        const float pan = kPanDepth * m * (float) std::sin(6.283185307179586 * lfoPhase);
        lfoPhase += beatHz / sampleRate;
        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;
        const float gpan = 1.0f / std::sqrt(1.0f + pan * pan);
        tL *= (1.0f + pan) * gpan;
        tR *= (1.0f - pan) * gpan;

        // Soft clipper de fin de chaine : transparent sous le genou, borne a 1.0.
        // Garantit que la balance LFO ne fait pas monter le son.
        tL = softClip(tL);
        tR = softClip(tR);

        // Melange sec/traite : a m=0 la recopie est exacte.
        outL = inL * (1.0f - m) + tL;
        outR = inR * (1.0f - m) + tR;
    }

private:
    static constexpr float kChorusK1 = 0.2524f; // ~0.18 Hz via chorusRate()
    static constexpr float kPanDepth = 0.30f;   // balance max a m=1 (petit LFO)
    static constexpr float kShelfMaxDb = 17.0f; // aigu max a m=1 (etait 13,0)
    static constexpr double kShelfHz = 6000.0;  // etait 7500 : plus de matiere
    static constexpr float kLowCutDb = -6.0f;   // bas adouci a m=1 (nouveau)
    static constexpr double kLowHz = 500.0;     // etait : rien
    static constexpr float kDriveMax = 6.0f;    // drive max 7 (etait 4,5)
    static constexpr float kSoftKnee = 0.5f;    // genou du clipper (etait 0,6)

    // Clipper doux continu : valeur ET pente continues au genou.
    // Sous le genou : transparent. Au-dela : borne douce vers 1.0.
    static inline float softClip(float x)
    {
        const float a = std::fabs(x);
        if (a <= kSoftKnee)
            return x;
        const float s = (x < 0.0f) ? -1.0f : 1.0f;
        const float reste = 1.0f - kSoftKnee;
        const float pente = 1.0f / reste; // pente 1 au genou : raccord sans cassure
        return s * (kSoftKnee + reste * std::tanh((a - kSoftKnee) * pente));
    }

    double sampleRate = 44100.0;
    double beatHz = 2.0;   // 1/4 a 120 BPM par defaut
    double lfoPhase = 0.0; // 0..1
    float lastShelfDb = 999.0f;
    float lastLowDb = 999.0f;
    PrismeBiquad shelfL, shelfR;
    PrismeBiquad lowL, lowR;
    ArpFX::Engine chorus;
};
