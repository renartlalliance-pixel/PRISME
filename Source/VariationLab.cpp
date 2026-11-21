// RANDOMIZE WAVE : planificateur + rendu hors ligne (thread de travail).
// Ne touche jamais au thread audio ni a l'UI. Reutilise ArpFX::Engine
// (delay) et BlurReverb (reverb) en instances fraiches : meme son que le
// plugin, zero nouveau DSP temporel. Glitch/reverse = buffer. Pitch =
// granulaire offline (duree preservee). "Formant" = coloration resonante
// deplacee independamment du pitch (honnete : pas du LPC, voir .h).
#include "VariationLab.h"
#include "ArpFX.h"
#include "BlurReverb.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace VarLab
{
namespace
{
    struct PeakEQ
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        void set(double sr, double f0, double Q, double gainDB)
        {
            double A = std::pow(10.0, gainDB / 40.0);
            double w0 = 6.283185307179586 * std::min(std::max(f0, 20.0), sr * 0.45) / sr;
            double cw = std::cos(w0), sw = std::sin(w0);
            double alpha = sw / (2.0 * std::max(0.1, Q));
            double a0 = 1.0 + alpha / A;
            b0 = (float) ((1.0 + alpha * A) / a0);
            b1 = (float) ((-2.0 * cw) / a0);
            b2 = (float) ((1.0 - alpha * A) / a0);
            a1 = (float) ((-2.0 * cw) / a0);
            a2 = (float) ((1.0 - alpha / A) / a0);
            x1 = x2 = y1 = y2 = 0.0f;
        }
        inline float process(float x)
        {
            float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
    };

    // Fondus sin² aux bords d'un segment (anti-clic), en place.
    void edgeFades(std::vector<float>& b, int off, int len, int fade)
    {
        fade = std::min(fade, len / 2);
        if (fade <= 0) return;
        for (int i = 0; i < fade; ++i)
        {
            float g = std::sin(1.5707963f * (float) i / (float) fade);
            g *= g;
            b[(size_t) (off + i)] *= g;
            b[(size_t) (off + len - 1 - i)] *= g;
        }
    }

    // Pitch granulaire offline, duree preservee (ratio > 1 = plus aigu).
    void pitchGrain(const float* in, float* out, int len, double sr, float semitones)
    {
        double ratio = std::pow(2.0, (double) semitones / 12.0);
        int W = (int) std::min<double>(2048.0, std::max<double>(256.0, len / 4.0));
        int H = std::max(64, W / 4);
        std::vector<float> win((size_t) W);
        for (int n = 0; n < W; ++n)
        {
            float w = 0.5f - 0.5f * std::cos(6.2831853f * (float) n / (float) W);
            win[(size_t) n] = w;
        }
        std::vector<float> acc((size_t) len, 0.0f), wsum((size_t) len, 0.0f);
        for (int m = 0; ; ++m)
        {
            int outPos = m * H;
            if (outPos >= len) break;
            double readPos = (double) outPos * ratio;
            for (int n = 0; n < W; ++n)
            {
                int o = outPos + n - W / 2;
                if (o < 0 || o >= len) continue;
                double rp = readPos + (n - W / 2);
                int r0 = (int) std::floor(rp);
                float fr = (float) (rp - r0);
                float v = 0.0f;
                if (r0 >= 0 && r0 < len - 1)
                    v = in[r0] * (1.0f - fr) + in[r0 + 1] * fr;
                else if (r0 >= 0 && r0 < len)
                    v = in[r0];
                acc[(size_t) o] += v * win[(size_t) n];
                wsum[(size_t) o] += win[(size_t) n];
            }
        }
        for (int i = 0; i < len; ++i)
            out[i] = wsum[(size_t) i] > 1e-4f ? acc[(size_t) i] / wsum[(size_t) i] : 0.0f;
        (void) sr;
    }
} // namespace

Plan buildPlan(const Grid& grid, uint32_t seed, int intensity, int featuredKind)
{
    Plan plan;
    plan.grid = grid;
    plan.seed = seed;
    plan.intensity = juce::jlimit(0, 100, intensity);
    // Mode "one shot" : un effet vedette (0..7), ou -1 pour le tirage au hasard.
    plan.featuredKind = (featuredKind >= 0 && featuredKind < 8) ? featuredKind : -1;
    const int featured = plan.featuredKind;
    float t = (float) plan.intensity / 100.0f;
    std::mt19937 rng(seed);
    auto ur = [&](float a, float b)
    {
        return a + (b - a) * (float) (rng() / (double) rng.max());
    };

    double bpb = grid.beatsPerBar();
    double total = grid.totalBeats();
    // Subdivisions candidates (en temps) : mesure -> double-croche.
    std::vector<double> subs = { bpb, bpb / 2.0, 1.0, 0.5, 0.25 };
    // Poids : le fin arrive avec l'intensite.
    auto subWeight = [&](double s)
    {
        if (s >= bpb) return 0.5f + 0.5f * (1.0f - t);
        if (s >= 1.0) return 0.8f;
        if (s >= 0.5) return 0.4f + 0.8f * t;
        return 0.1f + 1.2f * t;
    };
    auto pickSub = [&]() -> double
    {
        float sum = 0.0f;
        for (auto s : subs) sum += subWeight(s);
        float r = ur(0.0f, sum);
        for (auto s : subs)
        {
            r -= subWeight(s);
            if (r <= 0) return std::min(s, bpb);
        }
        return 1.0;
    };
    static const Kind all[8] = { Kind::Glitch, Kind::Reverse, Kind::Pitch,
                                 Kind::Formant, Kind::Delay, Kind::Reverb,
                                 Kind::Grain, Kind::Shimmer };
    // Poids de base par effet : chacun sa courbe (pas de simple multiplication).
    // Boost creatif : glitch encore + (x1.6), reverse/pitch x1.3,
    // delay un peu +, grain leger + shimmer leger en appoint.
    auto baseWeight = [&](Kind k) -> float
    {
        switch (k)
        {
            case Kind::Glitch:  return (0.10f + 0.30f * t) * 1.6f;
            case Kind::Reverse: return (0.05f + 0.20f * t) * 1.3f;
            case Kind::Pitch:   return (0.10f + 0.20f * t) * 1.3f;
            case Kind::Formant: return t < 0.20f ? 0.0f : 0.02f + 0.15f * t;
            case Kind::Delay:   return 0.18f + 0.12f * t;
            case Kind::Reverb:  return 0.15f + 0.05f * t;
            case Kind::Grain:   return 0.08f + 0.12f * t;
            case Kind::Shimmer: return 0.05f + 0.10f * t;
        }
        return 0.1f;
    };
    // Palette reduite : a CHAQUE generation, 1 ou 2 effets sont retires au
    // hasard et ne jouent pas du tout. Les retires reviennent sur les
    // generations suivantes : la variete vient de cette rotation de palette.
    // Le tirage sort de la seed, donc "meme seed = meme plan" reste vrai.
    bool muted[8] = { false, false, false, false, false, false, false, false };
    {
        // On ne retire que parmi les effets REELLEMENT disponibles : un effet de
        // poids nul (formant sous 20 % d'intensite) ne compte pas comme retire.
        int pool[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        int nPool = 0;
        for (int k = 0; k < 8; ++k)
            if (baseWeight(all[k]) > 0.0f && k != featured) // vedette jamais retiree
                pool[nPool++] = k;
        int nMute = (rng() % 2u) ? 2 : 1;                // 1 ou 2 selon le tirage
        nMute = std::min(nMute, std::max(0, nPool - 5)); // garde au moins 5 effets
        for (int n = 0; n < nMute; ++n)
        {
            int idx = (int) (rng() % (uint32_t) nPool);
            muted[pool[idx]] = true;
            pool[idx] = pool[--nPool];                   // tirage sans remise
        }
        for (int k = 0; k < 8; ++k)
            if (muted[k])
                plan.mutedMask |= (uint8_t) (1u << k);
    }
    // Amplitude du mode "one shot" : vedette nettement remontee (plancher pour
    // qu'elle domine de la meme facon quelle que soit sa courbe), autres temperes.
    constexpr float kFeaturedBoost = 10.0f;
    constexpr float kFeaturedFloor = 2.0f;
    constexpr float kOtherDamp = 0.35f;
    // Poids final : la palette retire d'abord, puis la vedette est mise en avant.
    auto kindWeight = [&](Kind k) -> float
    {
        if (muted[(int) k])
            return 0.0f;
        float w = baseWeight(k);
        if (featured >= 0)
            w = ((int) k == featured) ? std::max(w * kFeaturedBoost, kFeaturedFloor)
                                      : w * kOtherDamp;
        return w;
    };
    auto pickKind = [&]() -> Kind
    {
        float sum = 0.0f;
        for (auto k : all) sum += kindWeight(k);
        float r = ur(0.0f, sum);
        for (auto k : all)
        {
            r -= kindWeight(k);
            if (r <= 0) return k;
        }
        return Kind::Delay;
    };

    int counts[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    for (int m = 0; m < grid.bars; ++m)
    {
        // Arc de structure : respiration (x0.35) -> plein (x1.0-1.3) -> sortie.
        // Contraste renforce pour des randoms qui se distinguent vraiment.
        float q = (float) m * 4.0f / (float) std::max(1, grid.bars);
        float zone = (q < 1.0f) ? 0.35f : (q < 2.0f ? 1.0f : (q < 3.0f ? 1.3f : 0.7f));
        float density = (0.5f + 2.9f * t) * zone;
        int n = (int) density + (ur(0.0f, 1.0f) < density - (int) density ? 1 : 0);
        n = std::min(n, 6);
        for (int e = 0; e < n; ++e)
        {
            double sub = pickSub();
            int steps = std::max(1, (int) std::round(bpb / sub));
            double start = m * bpb + (rng() % (uint32_t) steps) * sub;
            int maxMult = (int) std::max(1.0, (m * bpb + bpb - start) / sub);
            double len = sub * (1 + (int) (rng() % (uint32_t) std::min(maxMult, 6)));
            Event ev;
            ev.kind = pickKind();
            ev.startBeats = start;
            ev.lenBeats = len;
            switch (ev.kind)
            {
                case Kind::Glitch:
                {
                    double gsub = t > 0.6f ? 0.125 : 0.25; // 32e / 16e
                    ev.p[0] = (float) gsub;
                    ev.p[1] = (float) std::max(2, (int) std::round(len / gsub));
                    ev.p[2] = 0.5f;
                    // Rearrangement : melange l'ordre des sous-fragments
                    // (1 fois sur 3 environ, plus a forte intensite).
                    if (ur(0.0f, 1.0f) < 0.25f + 0.35f * t)
                    {
                        ev.p[3] = 1.0f;
                        ev.aux = rng(); // permutation determinee (reproductible)
                    }
                    break;
                }
                case Kind::Reverse:
                    // Reverse plus longs : etire jusqu'a 2x la subdivision.
                    ev.lenBeats = std::min(len * 2.0, (double) maxMult * sub);
                    break;
                case Kind::Pitch:
                {
                    // Plages elargies : plus de variation qu'avant, cap +/-12 st
                    // (au-dela = artefacts excessifs, documente).
                    static const int tiers[][6] = {
                        { -2, -1, 1, 2, -3, 3 },       // subtle (legerement ouvert)
                        { -5, -2, -1, 1, 3, 5 },       // creative
                        { -7, -5, -3, 3, 7, 12 },     // experimental
                        { -12, -7, -5, 5, 7, 12 },    // extreme
                    };
                    int tier = t < 0.2f ? 0 : (t < 0.45f ? 1 : (t < 0.75f ? 2 : 3));
                    int pick = tiers[tier][rng() % 6];
                    if (pick == 0) pick = (rng() % 2) ? 2 : -2;
                    ev.p[0] = (float) pick;
                    ev.p[1] = 0.85f + 0.15f * t;
                    break;
                }
                case Kind::Formant:
                {
                    float range = 2.0f + 5.0f * t; // +/-st recus
                    ev.p[0] = ur(-range, range);
                    ev.p[1] = 0.3f + 0.6f * t;    // gain des pics
                    break;
                }
                case Kind::Delay:
                {
                    static const double divs[5] = { 1.0, 0.5, 0.75, 0.25, 1.5 };
                    double dv = divs[rng() % 5];
                    ev.p[0] = (float) dv;
                    ev.p[1] = 0.15f + 0.60f * t;  // feedback
                    ev.p[2] = 0.15f + 0.40f * t;  // wet
                    ev.p[3] = 1.0f + 1.0f * t;    // spill (temps)
                    break;
                }
                case Kind::Reverb:
                {
                    ev.p[0] = 0.35f + 0.50f * t;  // size
                    ev.p[1] = 0.8f + 1.4f * t;    // decayMul
                    ev.p[2] = 0.15f + 0.45f * t;  // wet
                    ev.p[3] = 1.0f + 2.0f * t;    // spill (temps)
                    break;
                }
                case Kind::Grain:
                {
                    // Granulaire leger : petits grains diffuses, mix contenu.
                    ev.p[0] = 30.0f + 50.0f * t;  // grain ms
                    ev.p[1] = 0.25f + 0.25f * t;  // scatter (temps)
                    ev.p[2] = 0.20f + 0.30f * t;  // mix
                    ev.aux = rng();               // offsets determines
                    break;
                }
                case Kind::Shimmer:
                {
                    // Shimmer leger : octave +12 en nappe, jamais envahissant.
                    ev.p[0] = 0.07f + 0.15f * t;  // mix (contenu mais present)
                    ev.p[1] = 1.0f + 1.0f * t;    // tail (temps)
                    break;
                }
            }
            plan.events.push_back(ev);
            counts[(int) ev.kind]++;
        }
    }
    // Tri + anti-chevauchement SELECTIF : les destructifs (glitch, reverse,
    // pitch, grain) restent exclusifs entre eux (lisibilite), mais les
    // couleurs (formant, delay, reverb, shimmer) peuvent les recouvrir :
    // les combinaisons donnent des randoms qui se distinguent vraiment.
    auto isStructural = [](Kind k)
    {
        return k == Kind::Glitch || k == Kind::Reverse
            || k == Kind::Pitch || k == Kind::Grain;
    };
    std::sort(plan.events.begin(), plan.events.end(),
              [](const Event& a, const Event& b) { return a.startBeats < b.startBeats; });
    for (size_t i = 0; i < plan.events.size(); ++i)
    {
        if (! isStructural(plan.events[i].kind))
        {
            // Couleur : garde sa longueur (bornee a la grille + fin propre).
            plan.events[i].lenBeats = std::min(plan.events[i].lenBeats,
                                               total - 0.125 - plan.events[i].startBeats);
            continue;
        }
        double endNext = total - 0.125; // fin propre par defaut
        for (size_t j = i + 1; j < plan.events.size(); ++j)
        {
            if (isStructural(plan.events[j].kind))
            {
                endNext = plan.events[j].startBeats;
                break;
            }
        }
        double maxLen = endNext - plan.events[i].startBeats;
        plan.events[i].lenBeats = std::min(plan.events[i].lenBeats, maxLen);
    }
    plan.events.erase(
        std::remove_if(plan.events.begin(), plan.events.end(),
                       [](const Event& e) { return e.lenBeats < 0.125; }),
        plan.events.end());

    // Les effets retires sont annonces en tete du resume : c'est la seule trace
    // lisible de la palette du tirage (le fichier ne porte que la seed).
    juce::String mutedTxt;
    if (featured >= 0)
        mutedTxt = juce::String("one shot ") + juce::String(kindName(all[featured]));
    for (int k = 0; k < 8; ++k)
        if (muted[k])
            mutedTxt += juce::String(mutedTxt.isEmpty() ? "sans " : ", sans ")
                      + juce::String(kindName(all[k]));
    plan.summary = (mutedTxt.isNotEmpty() ? mutedTxt + " - " : juce::String())
        + juce::String::formatted(
        "%d evts (glitch %d, reverse %d, pitch %d, formant %d, delay %d, reverb %d,"
        " grain %d, shimmer %d)"
        " - pitch granulaire offline +/-12 st max, formant = coloration (pas LPC)",
        (int) plan.events.size(), counts[0], counts[1], counts[2],
        counts[3], counts[4], counts[5], counts[6], counts[7]);
    return plan;
}

RenderResult renderPlan(const Plan& plan,
                        const float* srcL, const float* srcR, int srcLen,
                        std::atomic<double>* progress,
                        const std::atomic<bool>* cancel)
{
    RenderResult rr;
    auto setProg = [&](double v) { if (progress) progress->store(v); };
    double sr = plan.grid.sr;
    if (sr <= 0 || srcLen <= 0 || srcL == nullptr || srcR == nullptr)
    {
        rr.error = "Source illisible (buffers vides).";
        return rr;
    }
    double beatSamp = plan.grid.beatSec() * sr;
    int musical = plan.grid.totalSamples();
    bool hasTime = false;
    for (auto& e : plan.events)
        if (e.kind == Kind::Delay || e.kind == Kind::Reverb
            || e.kind == Kind::Shimmer) { hasTime = true; break; }
    // exactBars : fichier pile sur la grille (drop sans stretch).
    rr.tailSec = plan.exactBars ? 0.0 : (hasTime ? 2.0 : 0.15);
    int total = musical + (int) (rr.tailSec * sr);
    rr.outL.assign((size_t) total, 0.0f);
    rr.outR.assign((size_t) total, 0.0f);
    // Base : source tuilee (bouclee) sur la grille. L'original n'est pas touche.
    for (int i = 0; i < total; ++i)
    {
        int s = i % srcLen;
        rr.outL[(size_t) i] = srcL[s];
        rr.outR[(size_t) i] = srcR[s];
    }
    setProg(0.15);

    for (size_t ei = 0; ei < plan.events.size(); ++ei)
    {
        if (cancel != nullptr && cancel->load()) // fermeture du plugin
        {
            rr.error = "Annule.";
            return rr;
        }
        const Event& e = plan.events[ei];
        int a = (int) (e.startBeats * beatSamp);
        int len = (int) (e.lenBeats * beatSamp);
        a = std::clamp(a, 0, total - 1);
        len = std::clamp(len, 64, total - a);
        if (len < 64) continue;

        if (e.kind == Kind::Glitch)
        {
            int sub = std::max(64, (int) (e.p[0] * beatSamp));
            sub = std::min(sub, len);
            // Cliche du segment (la permutation lit pendant qu'on ecrit).
            std::vector<float> segL(len), segR(len);
            for (int i = 0; i < len; ++i)
            {
                segL[(size_t) i] = rr.outL[(size_t) (a + i)];
                segR[(size_t) i] = rr.outR[(size_t) (a + i)];
            }
            int xf = std::min(96, sub / 2);
            if (e.p[3] > 0.5f)
            {
                // Rearrangement : permute les sous-fragments (seed du plan).
                int nfrag = std::max(1, len / sub);
                std::vector<int> order((size_t) nfrag);
                for (int k = 0; k < nfrag; ++k) order[(size_t) k] = k;
                std::mt19937 shuf(e.aux);
                for (int k = nfrag - 1; k > 0; --k)
                {
                    std::uniform_int_distribution<int> d(0, k);
                    std::swap(order[(size_t) k], order[(size_t) d(shuf)]);
                }
                for (int i = 0; i < len; ++i)
                {
                    int k = std::min(i / sub, nfrag - 1);
                    int j = i - k * sub;
                    int src = order[(size_t) k] * sub + j;
                    src = std::min(src, len - 1);
                    float vL = segL[(size_t) src];
                    float vR = segR[(size_t) src];
                    if (j < xf && k > 0) // raccord entre fragments permutes
                    {
                        float t2 = (float) j / (float) xf;
                        int psrc = std::min(order[(size_t) (k - 1)] * sub + sub - xf + j,
                                            len - 1);
                        vL = segL[(size_t) psrc] * (1.0f - t2) + vL * t2;
                        vR = segR[(size_t) psrc] * (1.0f - t2) + vR * t2;
                    }
                    rr.outL[(size_t) (a + i)] = vL;
                    rr.outR[(size_t) (a + i)] = vR;
                }
            }
            else
            {
                // Stutter : repete le premier sous-fragment.
                for (int i = 0; i < len; ++i)
                {
                    int k = i % sub;
                    float vL = segL[(size_t) k], vR = segR[(size_t) k];
                    if (k < xf && i >= sub) // raccord : fondu queue->tete
                    {
                        float t2 = (float) k / (float) xf;
                        vL = segL[(size_t) (sub - xf + k)] * (1.0f - t2) + vL * t2;
                        vR = segR[(size_t) (sub - xf + k)] * (1.0f - t2) + vR * t2;
                    }
                    rr.outL[(size_t) (a + i)] = vL;
                    rr.outR[(size_t) (a + i)] = vR;
                }
            }
            // Anti-clic : le glitch REMPLACE la source. Sans fondu, le bord
            // d'attaque et le bord de sortie font un saut d'un seul
            // echantillon, ce qui s'entend comme un clic. Meme longueur que
            // reverse/pitch/formant : 256 ech. (~6 ms a 44,1 kHz).
            edgeFades(rr.outL, a, len, 256);
            edgeFades(rr.outR, a, len, 256);
        }
        else if (e.kind == Kind::Reverse)
        {
            std::vector<float> tmpL(len), tmpR(len);
            for (int i = 0; i < len; ++i)
            {
                tmpL[(size_t) i] = rr.outL[(size_t) (a + len - 1 - i)];
                tmpR[(size_t) i] = rr.outR[(size_t) (a + len - 1 - i)];
            }
            edgeFades(tmpL, 0, len, 256);
            edgeFades(tmpR, 0, len, 256);
            for (int i = 0; i < len; ++i)
            {
                rr.outL[(size_t) (a + i)] = tmpL[(size_t) i];
                rr.outR[(size_t) (a + i)] = tmpR[(size_t) i];
            }
        }
        else if (e.kind == Kind::Pitch)
        {
            std::vector<float> inL(len), inR(len), oL(len), oR(len);
            for (int i = 0; i < len; ++i)
            {
                inL[(size_t) i] = rr.outL[(size_t) (a + i)];
                inR[(size_t) i] = rr.outR[(size_t) (a + i)];
            }
            pitchGrain(inL.data(), oL.data(), len, sr, e.p[0]);
            pitchGrain(inR.data(), oR.data(), len, sr, e.p[0]);
            edgeFades(oL, 0, len, 256);
            edgeFades(oR, 0, len, 256);
            float m = juce::jlimit(0.0f, 1.0f, e.p[1]);
            for (int i = 0; i < len; ++i)
            {
                rr.outL[(size_t) (a + i)] = inL[(size_t) i] * (1.0f - m) + oL[(size_t) i] * m;
                rr.outR[(size_t) (a + i)] = inR[(size_t) i] * (1.0f - m) + oR[(size_t) i] * m;
            }
        }
        else if (e.kind == Kind::Formant)
        {
            PeakEQ q1L, q1R, q2L, q2R;
            double sh = (double) e.p[0];
            float g = 3.0f + 9.0f * juce::jlimit(0.0f, 1.0f, e.p[1]);
            q1L.set(sr, 800.0 * std::pow(2.0, sh / 12.0), 2.0, g);
            q1R.set(sr, 800.0 * std::pow(2.0, sh / 12.0), 2.0, g);
            q2L.set(sr, 2400.0 * std::pow(2.0, sh / 12.0), 3.0, g * 0.7f);
            q2R.set(sr, 2400.0 * std::pow(2.0, sh / 12.0), 3.0, g * 0.7f);
            std::vector<float> tmpL(len), tmpR(len);
            for (int i = 0; i < len; ++i)
            {
                tmpL[(size_t) i] = q2L.process(q1L.process(rr.outL[(size_t) (a + i)]));
                tmpR[(size_t) i] = q2R.process(q1R.process(rr.outR[(size_t) (a + i)]));
            }
            edgeFades(tmpL, 0, len, 256);
            edgeFades(tmpR, 0, len, 256);
            for (int i = 0; i < len; ++i)
            {
                rr.outL[(size_t) (a + i)] = tmpL[(size_t) i];
                rr.outR[(size_t) (a + i)] = tmpR[(size_t) i];
            }
        }
        else if (e.kind == Kind::Delay)
        {
            double ms = (double) e.p[0] * plan.grid.beatSec() * 1000.0;
            ms = std::clamp(ms, 10.0, 1200.0);
            float k1 = (float) (std::log(ms / 10.0) / std::log(120.0));
            float k2 = juce::jlimit(0.0f, 0.85f, e.p[1]);
            float wet = juce::jlimit(0.0f, 1.0f, e.p[2]);
            int spill = std::min((int) (e.p[3] * beatSamp), total - (a + len));
            ArpFX::Engine fx;
            fx.setSampleRate(sr);
            fx.reset();
            for (int i = 0; i < len + spill; ++i)
            {
                float dryL = (i < len) ? rr.outL[(size_t) (a + i)] : 0.0f;
                float dryR = (i < len) ? rr.outR[(size_t) (a + i)] : 0.0f;
                float oL = 0, oR = 0;
                fx.process(dryL, dryR, oL, oR, 1, k1, k2);
                float wL = (oL - dryL) * wet; // le moteur ajoute dry + d*0.5
                float wR = (oR - dryR) * wet;
                if (i < len)
                {
                    rr.outL[(size_t) (a + i)] = dryL + wL;
                    rr.outR[(size_t) (a + i)] = dryR + wR;
                }
                else
                {
                    rr.outL[(size_t) (a + i)] += wL;
                    rr.outR[(size_t) (a + i)] += wR;
                }
            }
        }
        else if (e.kind == Kind::Reverb)
        {
            int spill = std::min((int) (e.p[3] * beatSamp), total - (a + len));
            BlurReverb vb;
            vb.setSampleRate(sr);
            vb.setSize(e.p[0]);
            vb.setColor(0.6f);
            vb.setDecayMul(0.5f + (float) e.p[1]);
            vb.setWidth(1.0f);
            vb.setPreDelay(0.0f);
            vb.reset();
            float wet = juce::jlimit(0.0f, 1.0f, e.p[2]);
            for (int i = 0; i < len + spill; ++i)
            {
                float dryL = (i < len) ? rr.outL[(size_t) (a + i)] : 0.0f;
                float dryR = (i < len) ? rr.outR[(size_t) (a + i)] : 0.0f;
                float oL = 0, oR = 0;
                vb.process(dryL, dryR, oL, oR);
                rr.outL[(size_t) (a + i)] += oL * wet;
                rr.outR[(size_t) (a + i)] += oR * wet;
            }
        }
        else if (e.kind == Kind::Grain)
        {
            // Granulaire leger : grains Hann diffuses autour de leur place,
            // duree preservee, mix contenu. Deterministe (seed du plan).
            int gw = std::clamp((int) (e.p[0] * 0.001 * sr), 256, len);
            int hop = std::max(64, gw / 2);
            int scatter = (int) (e.p[1] * beatSamp);
            std::mt19937 grng(e.aux);
            std::uniform_int_distribution<int> off(-scatter, scatter);
            std::vector<float> accL(len, 0.0f), accR(len, 0.0f),
                               wsum(len, 0.0f);
            std::vector<float> win((size_t) gw);
            for (int n = 0; n < gw; ++n)
                win[(size_t) n] = 0.5f - 0.5f
                    * std::cos(6.2831853f * (float) n / (float) gw);
            for (int m = 0; ; ++m)
            {
                int o = m * hop;
                if (o >= len) break;
                int gsrc = std::clamp(o + off(grng), 0, len - 1);
                for (int n = 0; n < gw; ++n)
                {
                    int oo = o + n - gw / 2;
                    if (oo < 0 || oo >= len) continue;
                    int ss = std::clamp(gsrc + n - gw / 2, 0, len - 1);
                    accL[(size_t) oo] += rr.outL[(size_t) (a + ss)] * win[(size_t) n];
                    accR[(size_t) oo] += rr.outR[(size_t) (a + ss)] * win[(size_t) n];
                    wsum[(size_t) oo] += win[(size_t) n];
                }
            }
            float m2 = juce::jlimit(0.0f, 1.0f, e.p[2]);
            for (int i = 0; i < len; ++i)
            {
                float g2 = wsum[(size_t) i] > 1e-4f
                    ? accL[(size_t) i] / wsum[(size_t) i] : 0.0f;
                float gR = wsum[(size_t) i] > 1e-4f
                    ? accR[(size_t) i] / wsum[(size_t) i] : 0.0f;
                rr.outL[(size_t) (a + i)] = rr.outL[(size_t) (a + i)] * (1.0f - m2) + g2 * m2;
                rr.outR[(size_t) (a + i)] = rr.outR[(size_t) (a + i)] * (1.0f - m2) + gR * m2;
            }
        }
        else if (e.kind == Kind::Shimmer)
        {
            // Shimmer leger : octave +12 en nappe qui s'eteint (mix bas).
            std::vector<float> inL(len), inR(len), oL(len), oR(len);
            for (int i = 0; i < len; ++i)
            {
                inL[(size_t) i] = rr.outL[(size_t) (a + i)];
                inR[(size_t) i] = rr.outR[(size_t) (a + i)];
            }
            pitchGrain(inL.data(), oL.data(), len, sr, 12.0f);
            pitchGrain(inR.data(), oR.data(), len, sr, 12.0f);
            float wet = juce::jlimit(0.0f, 0.5f, e.p[0]);
            int spill = std::min((int) (e.p[1] * beatSamp), total - (a + len));
            int tailSrc = std::min(len, (int) (0.5 * beatSamp));
            for (int i = 0; i < len + spill; ++i)
            {
                float env;
                float sL2, sR2;
                if (i < len)
                {
                    env = 1.0f;
                    sL2 = oL[(size_t) i];
                    sR2 = oR[(size_t) i];
                }
                else
                {
                    int j = i - len;
                    env = std::exp(-3.0f * (float) j / (float) std::max(1, spill));
                    int k = len - tailSrc + (j % std::max(1, tailSrc));
                    sL2 = oL[(size_t) k];
                    sR2 = oR[(size_t) k];
                }
                rr.outL[(size_t) (a + i)] += sL2 * wet * env;
                rr.outR[(size_t) (a + i)] += sR2 * wet * env;
            }
        }
        setProg(0.15 + 0.75 * (double) (ei + 1) / (double) std::max<size_t>(1, plan.events.size()));
    }

    // Gain staging leger : la variation vise le niveau moyen de la source, avec
    // une correction bornee (kStageMaxDb de chaque cote). But : deux variations
    // du meme morceau ne sautent pas de volume d'un clic a l'autre (mode un
    // effet vedette, et variations classiques). Applique AVANT le soft-clip :
    // le clip absorbe le depassement au lieu de reduire tout le signal, donc la
    // correction tient. Deterministe : fonction des buffers uniquement.
    {
        constexpr double kStageMaxDb = 4.0;
        double sumSrc = 0.0;
        for (int i = 0; i < srcLen; ++i)
            sumSrc += (double) srcL[(size_t) i] * srcL[(size_t) i]
                    + (double) srcR[(size_t) i] * srcR[(size_t) i];
        double sumOut = 0.0;
        for (int i = 0; i < total; ++i)
            sumOut += (double) rr.outL[(size_t) i] * rr.outL[(size_t) i]
                    + (double) rr.outR[(size_t) i] * rr.outR[(size_t) i];
        const double rmsSrc = std::sqrt(sumSrc / (double) std::max(1, srcLen * 2));
        const double rmsOut = std::sqrt(sumOut / (double) std::max(1, total * 2));
        if (rmsSrc > 1.0e-6 && rmsOut > 1.0e-6)
        {
            const double lo = std::pow(10.0, -kStageMaxDb / 20.0);
            const double hi = std::pow(10.0,  kStageMaxDb / 20.0);
            const double g = std::max(lo, std::min(hi, rmsSrc / rmsOut));
            if (std::fabs(g - 1.0) > 1.0e-4)
                for (int i = 0; i < total; ++i)
                {
                    rr.outL[(size_t) i] = (float) ((double) rr.outL[(size_t) i] * g);
                    rr.outR[(size_t) i] = (float) ((double) rr.outR[(size_t) i] * g);
                }
        }
    }

    // Securite master : fondu d'entree, decroissance de fin (queues),
    // soft-clip + normalisation 0.89. Jamais de saturation sauvage.
    int fadeIn = std::min((int) (0.005 * sr), total / 4);
    int fadeOut = std::min((int) (0.080 * sr), total / 4);
    for (int i = 0; i < fadeIn; ++i)
    {
        float g = std::sin(1.5707963f * (float) i / (float) fadeIn);
        rr.outL[(size_t) i] *= g * g;
        rr.outR[(size_t) i] *= g * g;
    }
    for (int i = 0; i < fadeOut; ++i)
    {
        float g = std::sin(1.5707963f * (float) i / (float) fadeOut);
        g *= g;
        rr.outL[(size_t) (total - 1 - i)] *= g;
        rr.outR[(size_t) (total - 1 - i)] *= g;
    }
    float peak = 1e-6f;
    for (int i = 0; i < total; ++i)
    {
        rr.outL[(size_t) i] = std::tanh(rr.outL[(size_t) i]);
        rr.outR[(size_t) i] = std::tanh(rr.outR[(size_t) i]);
        peak = std::max({ peak, std::fabs(rr.outL[(size_t) i]),
                          std::fabs(rr.outR[(size_t) i]) });
    }
    if (peak > 0.89f)
    {
        float g = 0.89f / peak;
        for (int i = 0; i < total; ++i)
        {
            rr.outL[(size_t) i] *= g;
            rr.outR[(size_t) i] *= g;
        }
    }
    rr.outSamples = total;
    rr.outSr = sr;
    // Miniature 120 cretes.
    rr.peaks.assign(120, 0.0f);
    for (int c = 0; c < 120; ++c)
    {
        int i0 = c * total / 120, i1 = (c + 1) * total / 120;
        float pk = 0.0f;
        for (int i = i0; i < std::min(i1, total); ++i)
            pk = std::max(pk, std::fabs((rr.outL[(size_t) i] + rr.outR[(size_t) i]) * 0.5f));
        rr.peaks[(size_t) c] = std::min(1.0f, pk);
    }
    setProg(1.0);
    rr.ok = true;
    return rr;
}
} // namespace VarLab
