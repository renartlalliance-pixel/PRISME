#pragma once
// FX reserve a la session arpegee : Delay, Chorus, Filtre, Crusher (+ Off).
// 2 potards generiques k1/k2 (0..1), interpretes selon le type.
// Module autonome partage plugin / rendu offline.
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>

namespace ArpFX
{
    inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    // --- Correspondances potards -> unites (utilisees par le DSP et l'affichage) ---
    inline double delayMs(double k)      { return 10.0 * std::pow(120.0, clamp01((float) k)); } // 10..1200 ms
    inline double delayFb(double k)      { return clamp01((float) k) * 0.9; }                     // 0..0.9
    inline double chorusRate(double k)   { return 0.05 * std::pow(160.0, clamp01((float) k)); }   // 0.05..8 Hz
    inline double chorusDepthMs(double k){ return clamp01((float) k) * 8.0; }                     // 0..8 ms
    inline double filterCutoff(double k) { return 100.0 * std::pow(180.0, clamp01((float) k)); }  // 100..18000 Hz
    inline double filterQ(double k)      { return 0.5 + clamp01((float) k) * 7.5; }               // 0.5..8
    inline int    crusherBits(double k)  { return 2 + (int) std::round(clamp01((float) k) * 14.0); } // 2..16
    inline double crusherDrive(double k) { return 1.0 + clamp01((float) k) * 9.0; }               // x1..x10

    inline std::string knobName(int type, int knob)
    {
        // type : 0 Off, 1 Delay, 2 Chorus, 3 Filtre, 4 Crusher, 5 Reverse
        static const char* names[6][2] = {
            { "--", "--" }, { "Temps", "Feedback" }, { "Taux", "Profondeur" },
            { "Coupure", "Resonance" }, { "Bits", "Drive" }, { "Temps", "Mix" }
        };
        if (type < 0 || type > 5 || (knob != 0 && knob != 1)) return "--";
        return names[type][knob];
    }

    struct DelayLine
    {
        std::vector<float> buf;
        int size = 1, pos = 0;
        void init(int n)
        {
            size = std::max(8, n);
            buf.assign((size_t) size, 0.0f);
            pos = 0;
        }
        inline float read(double delaySamples) const
        {
            double rp = (double) pos - delaySamples;
            while (rp < 0) rp += size;
            int i0 = (int) rp;
            int i1 = i0 + 1;
            if (i1 >= size) i1 -= size;
            if (i0 >= size) i0 -= size;
            float frac = (float)(rp - (int) rp);
            return buf[(size_t) i0] * (1.0f - frac) + buf[(size_t) i1] * frac;
        }
        inline void write(float x)
        {
            buf[(size_t) pos] = std::clamp(x, -2.0f, 2.0f);
            if (++pos >= size) pos = 0;
        }
    };

    struct BiquadLP
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        void reset() { x1 = x2 = y1 = y2 = 0.0f; }
        void configure(double sr, double f0, double Q)
        {
            double w0 = 6.283185307179586 * std::min(f0, sr * 0.45) / sr;
            double cw = std::cos(w0), sw = std::sin(w0);
            double alpha = sw / (2.0 * std::max(0.1, Q));
            double a0 = 1.0 + alpha;
            b0 = (float)(((1.0 - cw) / 2.0) / a0);
            b1 = (float)((1.0 - cw) / a0);
            b2 = (float)(((1.0 - cw) / 2.0) / a0);
            a1 = (float)((-2.0 * cw) / a0);
            a2 = (float)((1.0 - alpha) / a0);
        }
        inline float process(float x)
        {
            float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
    };

    class Engine
    {
    public:
        void setSampleRate(double sr)
        {
            sampleRate = sr > 0 ? sr : 44100.0;
            int maxD = (int)(sampleRate * 1.5) + 16;
            dlyL.init(maxD);
            dlyR.init(maxD);
            choL.init((int)(sampleRate * 0.1) + 16);
            choR.init((int)(sampleRate * 0.1) + 16);
            int maxR = (int)(sampleRate * 1.2) + 16;
            revL.init(maxR);
            revR.init(maxR);
            lastCut = -1.0;
            lastQ = -1.0;
        }

        void reset()
        {
            for (auto* d : { &dlyL, &dlyR, &choL, &choR, &revL, &revR })
            {
                std::fill(d->buf.begin(), d->buf.end(), 0.0f);
                d->pos = 0;
            }
            lpL.reset(); lpR.reset();
            choPhase = 0.0f;
            revPlay = 0;
            revEnd = 0;
            dmpL = dmpR = 0.0f;
            shMeanL = shMeanR = 0.0f;
            lastCut = -1.0;
            lastQ = -1.0;
        }

        // type 0=Off(bypass), 1=Delay, 2=Chorus, 3=Filtre, 4=Crusher, 5=Reverse
        inline void process(float inL, float inR, float& outL, float& outR,
                            int type, float k1, float k2)
        {
            switch (type)
            {
                case 1: // Ping-pong stereo : droite a 0.75x (croche pointee)
                {
                    double dSampL = delayMs(k1) * 0.001 * sampleRate;
                    double dSampR = dSampL * 0.75;
                    double fb = delayFb(k2);
                    float dL = dlyL.read(dSampL);
                    float dR = dlyR.read(dSampR);
                    // Adoucissement : passe-bas + shimmer d'octave leger (redresse)
                    dmpL += 0.35f * (dL - dmpL);
                    dmpR += 0.35f * (dR - dmpR);
                    shMeanL += 0.001f * (std::fabs(dL) - shMeanL);
                    shMeanR += 0.001f * (std::fabs(dR) - shMeanR);
                    float shL = (std::fabs(dL) - shMeanL) * 0.15f;
                    float shR = (std::fabs(dR) - shMeanR) * 0.15f;
                    float wL = dmpL * 0.92f + shL;
                    float wR = dmpR * 0.92f + shR;
                    dlyL.write(inL + wR * (float) fb); // rebonds croises
                    dlyR.write(inR + wL * (float) fb);
                    outL = inL + dL * 0.5f;
                    outR = inR + dR * 0.5f;
                    break;
                }
                case 2:
                {
                    double rate = chorusRate(k1);
                    double depth = chorusDepthMs(k2) * 0.001 * sampleRate;
                    double center = 0.020 * sampleRate;
                    choPhase += (float)(6.2831853 * rate / sampleRate);
                    if (choPhase > 6.2831853f) choPhase -= 6.2831853f;
                    float mL = std::sin(choPhase);
                    float mR = std::sin(choPhase + 1.5707963f);
                    float wL = choL.read(center + mL * depth);
                    float wR = choR.read(center + mR * depth);
                    choL.write(inL + wL * 0.25f);
                    choR.write(inR + wR * 0.25f);
                    outL = inL * 0.7f + wL * 0.7f;
                    outR = inR * 0.7f + wR * 0.7f;
                    break;
                }
                case 3:
                {
                    double cut = filterCutoff(k1);
                    double q = filterQ(k2);
                    if (cut != lastCut || q != lastQ)
                    {
                        lpL.configure(sampleRate, cut, q);
                        lpR.configure(sampleRate, cut, q);
                        lastCut = cut; lastQ = q;
                    }
                    outL = lpL.process(inL);
                    outR = lpR.process(inR);
                    break;
                }
                case 4:
                {
                    int bits = crusherBits(k1);
                    double drive = crusherDrive(k2);
                    float n = (float)(1 << std::min(bits, 16));
                    auto crush = [&](float x)
                    {
                        float d = std::tanh(x * (float) drive);
                        return std::round(d * n) / n;
                    };
                    outL = crush(inL);
                    outR = crush(inR);
                    break;
                }
                case 5: // Reverse : fenetre relue a l'envers, avec enveloppe
                {
                    int W = (int)(sampleRate * 0.05 * std::pow(20.0, clamp01(k1)));
                    W = std::clamp(W, 32, revL.size - 1);
                    // Fin de fenetre memorisee UNE fois au debut de chaque fenetre.
                    // Sans ca, pos et revPlay avancent ensemble : l'index de lecture
                    // reste fige et l'effet relit le meme echantillon (maintien).
                    if (revPlay == 0) revEnd = revL.pos;
                    int iL = revEnd - 1 - revPlay;
                    if (iL < 0) iL += revL.size;
                    int iR = revEnd - 1 - revPlay;
                    if (iR < 0) iR += revR.size;
                    float rL = revL.buf[(size_t) iL];
                    float rR = revR.buf[(size_t) iR];
                    revL.write(inL);
                    revR.write(inR);
                    // Enveloppe : montee LONGUE sur presque toute la fenetre (elle
                    // accentue l'effet reverse), puis descente COURTE sur la fin.
                    // Les deux bouts tombent a zero : pas de clic a la recirculation.
                    const int fadeOut = std::min(W / 4, (int)(sampleRate * 0.005));
                    const int fadeInEnd = W - fadeOut;
                    float env;
                    if (revPlay < fadeInEnd)
                        env = (float) revPlay / (float) std::max(1, fadeInEnd - 1);
                    else
                        env = (float) (W - 1 - revPlay) / (float) std::max(1, fadeOut - 1);
                    if (++revPlay >= W) revPlay = 0;
                    const float mx = clamp01(k2);
                    outL = inL * (1.0f - mx) + rL * (mx * env);
                    outR = inR * (1.0f - mx) + rR * (mx * env);
                    break;
                }
                default: // Off : bypass transparent
                    outL = inL;
                    outR = inR;
                    break;
            }
        }

    private:
        double sampleRate = 44100.0;
        DelayLine dlyL, dlyR, choL, choR, revL, revR;
        BiquadLP lpL, lpR;
        float choPhase = 0.0f;
        float dmpL = 0.0f, dmpR = 0.0f;       // passe-bas boucle delay
        float shMeanL = 0.0f, shMeanR = 0.0f; // moyenne redressee (shimmer sans DC)
        int revPlay = 0;
        int revEnd = 0; // fin de la fenetre en cours (Reverse)
        double lastCut = -1.0, lastQ = -1.0;
    };
}
