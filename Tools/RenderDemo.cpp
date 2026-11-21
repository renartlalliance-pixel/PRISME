// Rendu offline de demos Prisme — reutilise le vrai DSP du plugin.
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <cstdlib>
#include "../Source/ResonatorBank.h"
#include "../Source/EQ4Band.h"
#include "../Source/BlurReverb.h"
#include "../Source/ArpFX.h"
#include "../Source/Crystallize.h"
#include "../Source/Scales.h"
#include <algorithm>

// Journal des pas d'arpege (sample, pas) pour le test de timing. Actif sur demande.
#include <utility>
static std::vector<std::pair<int,int>> g_arpLog;
static bool g_arpLogOn = false;

static void writeWav16(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = fopen(path.c_str(), "wb");
    int n = (int)L.size();
    int dataBytes = n * 2 * 2;
    auto w32 = [&](uint32_t v){ fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v){ fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f);
    w32(36 + dataBytes);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    w32(16); w16(1); w16(2);
    w32((uint32_t)sr); w32((uint32_t)(sr * 2 * 2)); w16(4); w16(16);
    fwrite("data", 1, 4, f);
    w32(dataBytes);
    for (int i = 0; i < n; ++i)
    {
        float a = std::max(-1.0f, std::min(1.0f, L[(size_t)i]));
        float b = std::max(-1.0f, std::min(1.0f, R[(size_t)i]));
        int16_t sa = (int16_t)(a * 32767.0f);
        int16_t sb = (int16_t)(b * 32767.0f);
        fwrite(&sa, 2, 1, f);
        fwrite(&sb, 2, 1, f);
    }
    fclose(f);
    printf("OK %s (%d samples)\n", path.c_str(), n);
}

static void genDrums(std::vector<float>& m, int sr)
{
    int n = (int)m.size();
    double bpm = 120.0, beat = 60.0 / bpm * sr;
    for (int i = 0; i < n; ++i)
    {
        double t = i / (double)sr;
        int step = (int)(i / (beat / 2)) % 8;
        double since = fmod(i, beat / 2) / sr;
        float s = 0;
        if (step % 4 == 0 && since < 0.25)
        {
            double f = 160.0 * exp(-since * 18.0) + 45.0;
            s += (float)(sin(6.2831 * f * since) * exp(-since * 12.0) * 0.9);
        }
        if (step % 2 == 0 && since < 0.05)
        {
            float nz = (float)(rand() % 2000 - 1000) / 1000.0f;
            s += nz * (float)exp(-since * 120.0) * 0.35f;
        }
        if (step == 2 || step == 6)
        {
            if (since < 0.18)
            {
                float nz = (float)(rand() % 2000 - 1000) / 1000.0f;
                s += nz * (float)exp(-since * 22.0) * 0.5f * (0.6f + 0.4f * (float)sin(6.2831 * 1800 * t));
            }
        }
        m[(size_t)i] = s * 0.8f;
    }
}

static void genNoise(std::vector<float>& mo, int sr)
{
    int n = (int)mo.size();
    for (int i = 0; i < n; ++i)
    {
        double t = i / (double)sr;
        double gate = (fmod(t, 1.0) < 0.3) ? 1.0 : 0.0;
        double env = gate * std::min(1.0, fmod(t, 1.0) * 40.0) * std::min(1.0, (0.3 - fmod(t, 1.0)) * 12.0 + 0.15);
        float nz = (float)(rand() % 2000 - 1000) / 1000.0f;
        mo[(size_t)i] = nz * (float)env * 0.6f;
    }
}

struct Preset { const char* name; int root, scale, oct; float decay, attack, bright, irid, shimmer, spread, mix;
                bool arpOn; int arpPattern; float arpRate;
                int arpOct = 1; float arpSwing = 0.0f;
                float movRate, movDepth, movShape, movUnison;
                float eqLow, eqLowMid, eqHighMid, eqHigh;
                float blurMix = 0.0f, blurSize = 0.65f;
                float bandLo = 20.0f, bandHi = 20000.0f;
                float freezeMix = 0.0f;
                int fxType = 0; float fxK1 = 0.35f, fxK2 = 0.35f;
                int fxTypeB = 0; float fxB1 = 0.35f, fxB2 = 0.35f; float fxMix = 1.0f;
                int fxTypeC = 0; float fxC1 = 0.35f, fxC2 = 0.35f;
                float cryoMix = 0.0f; float outDb = 0.0f; };

static void renderOne(PrismResonatorBank& bank, const std::vector<float>& in,
                      std::vector<float>& L, std::vector<float>& R, const Preset& p, int sr)
{
    auto notes = PrismeScales::buildTuning(p.root, p.scale, p.oct, 12);
    bank.reset();
    bank.resetEmphasis();
    bank.setTuning(notes);
    bank.setDecay(p.decay, false);
    bank.setBrightness(p.bright);
    bank.setShimmer(p.shimmer);
    bank.setSpread(p.spread);
    bank.setEmphSmooth(p.attack * 0.15 + 0.004);
    bank.setMovement(p.movRate, p.movDepth, p.movShape, p.movUnison);

    PrismeEQ4Band eqL, eqR;
    eqL.configure(sr, p.eqLow, p.eqLowMid, p.eqHighMid, p.eqHigh);
    eqR.configure(sr, p.eqLow, p.eqLowMid, p.eqHighMid, p.eqHigh);
    bool eqActive = (p.eqLow != 0 || p.eqLowMid != 0 || p.eqHighMid != 0 || p.eqHigh != 0);

    BlurReverb blur;
    blur.setSampleRate(sr);
    blur.reset();
    blur.setSize(p.blurSize);

    PrismeBandLimiter band;
    band.configure(sr, p.bandLo, p.bandHi);
    bool bandActive = ! PrismeBandLimiter::isBypass(p.bandLo, p.bandHi);

    // Double banque miroir du processeur : normale + gelee, crossfade Givre
    PrismResonatorBank freezeBank;
    freezeBank.setSampleRate(sr);
    freezeBank.reset();
    freezeBank.resetEmphasis();
    {
        auto notes = PrismeScales::buildTuning(p.root, p.scale, p.oct, 12);
        freezeBank.setTuning(notes);
    }
    freezeBank.setDecay(30.0, false);
    freezeBank.setBrightness(p.bright);
    freezeBank.setShimmer(p.shimmer);
    freezeBank.setSpread(p.spread);
    freezeBank.setEmphSmooth(p.attack * 0.15 + 0.004);

    // FX arp en chaine A -> B -> C + dry/wet (miroir processeur)
    ArpFX::Engine fxA, fxB, fxC;
    fxA.setSampleRate(sr); fxA.reset();
    fxB.setSampleRate(sr); fxB.reset();
    fxC.setSampleRate(sr); fxC.reset();

    // CRYSTALLIZE en parallele de la chaine (miroir processeur)
    Crystallize cryo;
    cryo.setSampleRate(sr);
    cryo.reset();
    fxA.setSampleRate(sr); fxA.reset();
    fxB.setSampleRate(sr); fxB.reset();
    fxC.setSampleRate(sr); fxC.reset();
    bank.setArpSplitActive(p.arpOn && p.arpPattern != 4);
    freezeBank.setArpSplitActive(p.arpOn && p.arpPattern != 4);

    bool useShaper = p.attack > 0.008f;
    float kFast = 1.0f - expf(-1.0f / (0.0002f * sr));
    float kSlow = 1.0f - expf(-1.0f / (p.attack * sr));
    float kRel  = 1.0f - expf(-1.0f / (0.25f * sr));
    float peakEnv = 0, slowEnv = 0;

    int n = (int)in.size();
    L.assign(n, 0); R.assign(n, 0);

    // Arp (miroir du processeur : K = tons x octaves, swing alterne) + journal
    int arpK = std::min(12, PrismeScales::getScaleToneCount(p.scale) * p.arpOct);
    if (arpK < 1) arpK = 1;
    double stepSamples = sr / p.arpRate;
    double toNext = 0;
    long long tick = 0;
    int step = 0, dir = 1;
    float targets[12];
    for (int i = 0; i < 12; ++i) targets[i] = 1.0f;

    for (int i = 0; i < n; ++i)
    {
        if (p.arpOn)
        {
            toNext -= 1.0;
            if (toNext <= 0)
            {
                if (p.arpPattern == 0) step = (step + 1) % arpK;
                else if (p.arpPattern == 2) { step += dir; if (step >= arpK) { step = arpK-2; dir=-1; } else if (step < 0) { step = 1; dir=1; } }
                else if (p.arpPattern == 3) step = rand() % arpK;
                double len = stepSamples;
                if (p.arpSwing > 0.001)
                    len *= ((tick % 2) == 0) ? (1.0 + p.arpSwing) : (1.0 - p.arpSwing);
                toNext += len;
                ++tick;
                if (g_arpLogOn) g_arpLog.push_back({ i, step });
                for (int v = 0; v < 12; ++v)
                    targets[v] = ((v % arpK) == step) ? 1.0f : 0.15f;
                bank.setVoiceTargets(targets, 12);
            }
        }
        float mono = in[(size_t)i];
        if (bandActive) mono = band.process(mono);
        float exc = mono;
        if (useShaper)
        {
            float ax = fabsf(mono);
            peakEnv += (ax - peakEnv) * (ax > peakEnv ? kFast : kRel);
            slowEnv += (ax - slowEnv) * (ax > slowEnv ? kSlow : kRel);
            float g = slowEnv / (peakEnv + 1e-4f);
            if (g > 1) g = 1; if (g < 0) g = 0;
            exc = mono * g;
        }
        float wetL = 0, wetR = 0, arpL = 0, arpR = 0;
        bank.processSample(exc, wetL, wetR, &arpL, &arpR);
        {
            float fxL = 0, fxR = 0, fx2L = 0, fx2R = 0, fx3L = 0, fx3R = 0;
            fxA.process(arpL, arpR, fxL, fxR, p.fxType, p.fxK1, p.fxK2);
            fxB.process(fxL, fxR, fx2L, fx2R, p.fxTypeB, p.fxB1, p.fxB2);
            fxC.process(fx2L, fx2R, fx3L, fx3R, p.fxTypeC, p.fxC1, p.fxC2);
            wetL = (wetL - arpL) + (arpL * (1 - p.fxMix) + fx3L * p.fxMix);
            wetR = (wetR - arpR) + (arpR * (1 - p.fxMix) + fx3R * p.fxMix);
        }
        if (p.freezeMix > 0.0005f)
        {
            float fL = 0, fR = 0, fArpL = 0, fArpR = 0;
            freezeBank.processSample(exc, fL, fR, &fArpL, &fArpR);
            fL *= 2.0f; fR *= 2.0f; fArpL *= 2.0f; fArpR *= 2.0f; // idem plugin : maquillage voie gelee
            wetL = wetL * (1 - p.freezeMix) + fL * p.freezeMix;
            wetR = wetR * (1 - p.freezeMix) + fR * p.freezeMix;
            arpL = arpL * (1 - p.freezeMix) + fArpL * p.freezeMix;
            arpR = arpR * (1 - p.freezeMix) + fArpR * p.freezeMix;
        }
        {
            float cL = 0, cR = 0;
            cryo.process(arpL, arpR, cL, cR, p.cryoMix);
            wetL += (cL - arpL);
            wetR += (cR - arpR);
        }
        wetL *= p.irid; wetR *= p.irid;
        if (p.blurMix > 0.001f)
        {
            float vL = 0, vR = 0;
            blur.process(wetL, wetR, vL, vR);
            wetL = wetL * (1 - p.blurMix) + vL * p.blurMix;
            wetR = wetR * (1 - p.blurMix) + vR * p.blurMix;
        }
        wetL = tanhf(wetL * 0.9f); wetR = tanhf(wetR * 0.9f);
        float yL = mono * (1 - p.mix) + wetL * p.mix;
        float yR = mono * (1 - p.mix) + wetR * p.mix;
        if (eqActive) { yL = eqL.process(yL); yR = eqR.process(yR); }
        // Etage Sortie miroir : tanh PUIS gain (le potard definit le niveau)
        float outG = powf(10.0f, p.outDb / 20.0f);
        L[(size_t)i] = tanhf(yL) * outG;
        R[(size_t)i] = tanhf(yR) * outG;
    }
}

int main()
{
    const int sr = 44100;
    const double dur = 6.0;
    const int n = (int)(sr * dur);

    std::vector<float> drums(n), noise(n);
    genDrums(drums, sr);
    genNoise(noise, sr);
    writeWav16("Release/Demo_Drums_DRY.wav", drums, drums, sr);
    writeWav16("Release/Demo_Noise_DRY.wav", noise, noise, sr);

    std::vector<Preset> presets = {
        { "VerreClair",  0, 0, 1, 1.2f, 0.010f, 0.85f, 0.8f, 0.20f, 0.70f, 0.45f, false, 0, 4.0f, 1, 0.0f, 0.8f, 0.0f, 0.2f, 1.0f, 0, 0, 0 },
        { "Aurore",      5, 6, 1, 3.2f, 0.150f, 0.65f, 1.1f, 0.55f, 0.90f, 0.60f, false, 0, 4.0f, 1, 0.0f, 0.8f, 0.0f, 0.2f, 1.0f, 0, 0, 0 },
        { "Cathedrale",  7, 1, 0, 6.5f, 0.600f, 0.40f, 1.2f, 0.30f, 0.85f, 0.70f, false, 0, 4.0f, 1, 0.0f, 0.8f, 0.0f, 0.2f, 1.0f, 0, 0, 0 },
        { "Fracture",    2, 20, 1, 2.0f, 0.008f, 1.0f, 1.4f, 0.80f, 1.0f, 0.75f, false, 0, 4.0f, 1, 0.0f, 0.8f, 0.0f, 0.2f, 1.0f, 0, 0, 0 },
    };

    PrismResonatorBank bank;
    bank.setSampleRate(sr);

    std::vector<float> L, R;
    for (auto& p : presets)
    {
        renderOne(bank, drums, L, R, p, sr);
        writeWav16(std::string("Release/Demo_Drums_") + p.name + ".wav", L, R, sr);
    }
    renderOne(bank, noise, L, R, presets[1], sr);
    writeWav16("Release/Demo_Noise_Aurore.wav", L, R, sr);

    // Nouveautes : arp + houle lente (attaque longue)
    Preset arpDemo = { "Arp", 0, 8, 1, 2.5f, 0.020f, 0.75f, 1.1f, 0.45f, 0.9f, 0.65f, true, 0, 6.0f, 1, 0.0f, 0.8f, 0.0f, 0.2f, 1.0f, 0, 0, 0 };
    renderOne(bank, drums, L, R, arpDemo, sr);
    writeWav16("Release/Demo_Drums_ArpMaj7.wav", L, R, sr);

    Preset swellDemo = { "Swell", 5, 6, 1, 4.0f, 0.900f, 0.60f, 1.1f, 0.40f, 0.9f, 0.70f, false, 0, 4.0f, 1, 0.0f, 0.8f, 0.0f, 0.2f, 1.0f, 0, 0, 0 };
    renderOne(bank, drums, L, R, swellDemo, sr);
    writeWav16("Release/Demo_Drums_Swell.wav", L, R, sr);

    // Mouvement : respiration douce en phases reparties, puis rafales unisson
    Preset breatheDemo = { "Respire", 5, 6, 1, 3.5f, 0.150f, 0.65f, 1.1f, 0.45f, 0.9f, 0.65f, false, 0, 4.0f, 1, 0.0f, 0.5f, 0.65f, 0.1f, 0.0f, 0, 0, 0 };
    renderOne(bank, drums, L, R, breatheDemo, sr);
    writeWav16("Release/Demo_Drums_Respire.wav", L, R, sr);

    Preset burstDemo = { "Rafales", 0, 8, 1, 2.5f, 0.020f, 0.75f, 1.1f, 0.40f, 0.9f, 0.65f, false, 0, 4.0f, 1, 0.0f, 3.0f, 0.9f, 0.85f, 1.0f, 0, 0, 0 };
    renderOne(bank, drums, L, R, burstDemo, sr);
    writeWav16("Release/Demo_Drums_Rafales.wav", L, R, sr);

    // EQ : meme base Aurore + courbe en V (+graves/aigus, mediums creuses)
    Preset eqDemo = { "EQ", 5, 6, 1, 3.2f, 0.150f, 0.65f, 1.1f, 0.55f, 0.90f, 0.60f, false, 0, 4.0f, 1, 0.0f, 0.8f, 0.0f, 0.2f, 1.0f, 6.0f, -4.0f, -4.0f, 6.0f };
    renderOne(bank, drums, L, R, eqDemo, sr);
    writeWav16("Release/Demo_Drums_EQ.wav", L, R, sr);

    // Blur : meme base Aurore + nappe diffuse (mix 0.7, taille 0.75)
    Preset blurDemo = { "Blur", 5, 6, 1, 3.2f, 0.150f, 0.65f, 1.1f, 0.55f, 0.90f, 0.60f, false, 0, 4.0f, 1, 0.0f, 0.8f, 0.0f, 0.2f, 1.0f, 0, 0, 0, 0, 0.7f, 0.75f };
    renderOne(bank, drums, L, R, blurDemo, sr);
    writeWav16("Release/Demo_Drums_Blur.wav", L, R, sr);

    // Bande : chromatique +1 octave en 100% wet, effet concentre sur les aigus (800-20k)
    Preset bandDemo = { "Bande", 2, 20, 2, 2.0f, 0.008f, 1.0f, 1.4f, 0.80f, 1.0f, 1.0f, false, 0, 4.0f, 1, 0.0f, 0.8f, 0.0f, 0.2f, 1.0f, 0, 0, 0, 0, 0.0f, 0.65f, 800.0f, 20000.0f };
    renderOne(bank, drums, L, R, bandDemo, sr);
    writeWav16("Release/Demo_Drums_Bande.wav", L, R, sr);

    // Reference 100% wet sans bande (pour mesurer ce que la bande retire)
    Preset wetRef = { "WetRef", 5, 6, 1, 3.2f, 0.150f, 0.65f, 1.1f, 0.55f, 0.90f, 1.0f, false, 0, 4.0f, 1, 0.0f, 0.8f, 0.0f, 0.2f, 1.0f, 0, 0, 0, 0 };
    renderOne(bank, drums, L, R, wetRef, sr);
    writeWav16("Release/Demo_Drums_WetRef.wav", L, R, sr);

    // Givre : meme base Aurore, fader a 0.8 (nappe gelee)
    Preset givreDemo = { "Givre", 5, 6, 1, 3.2f, 0.150f, 0.65f, 1.1f, 0.55f, 0.90f, 0.60f, false, 0, 4.0f, 1, 0.0f, 0.8f, 0.0f, 0.2f, 1.0f, 0, 0, 0, 0, 0.0f, 0.65f, 20.0f, 20000.0f, 0.8f };
    renderOne(bank, drums, L, R, givreDemo, sr);
    writeWav16("Release/Demo_Drums_Givre.wav", L, R, sr);

    // Test A/B gele : 6 s batterie + 4 s silence, meme seed.
    // Sans givre la queue s'eteint (T60 3.2 s), gelee elle sustaine (T60 30 s).
    {
        Preset b0 = { "B0", 5, 6, 1, 3.2f, 0.150f, 0.65f, 1.1f, 0.55f, 0.90f, 0.60f, false, 0, 4.0f, 1, 0.0f, 0.8f, 0.0f, 0.2f, 1.0f, 0, 0, 0, 0, 0.0f, 0.65f, 20.0f, 20000.0f, 0.0f };
        Preset b1 = b0; b1.freezeMix = 1.0f;
        std::vector<float> in10 = drums;
        in10.insert(in10.end(), (size_t)(sr * 4), 0.0f);
        std::vector<float> inN10 = noise;
        inN10.insert(inN10.end(), (size_t)(sr * 4), 0.0f);
        std::vector<float> L2, R2;
        srand(999); renderOne(bank, in10, L, R, b0, sr);
        srand(999); renderOne(bank, in10, L2, R2, b1, sr);
        auto tailRms = [](const std::vector<float>& v)
        {
            double e = 0; int c = 0;
            for (size_t i = v.size() - (size_t)(44100 * 2); i < v.size(); ++i) { e += v[i] * v[i]; ++c; }
            return std::sqrt(e / c);
        };
        double t0 = tailRms(L), t1 = tailRms(L2);
        double peak = 0; for (auto v : L2) peak = std::max<double>(peak, std::fabs(v));
        printf("GIVRE A/B batterie : queue sec=%.4f gele=%.4f ratio=%.1f peak=%.2f\n",
               t0, t1, t1 / (t0 + 1e-9), peak);
        srand(999); renderOne(bank, inN10, L, R, b0, sr);
        srand(999); renderOne(bank, inN10, L2, R2, b1, sr);
        double n0 = tailRms(L), n1 = tailRms(L2);
        printf("GIVRE A/B bruit    : queue sec=%.4f gele=%.4f ratio=%.1f -> %s\n",
               n0, n1, n1 / (n0 + 1e-9),
               (n1 > n0 * 3.0 && n1 > 0.005 && peak < 0.99) ? "OK" : "PROBLEME");
    }

    // Test A/B FX arp : meme seed, arp identique, Off vs Delay.
    // Le delay doit ajouter de l'energie (echoes) sans clipper.
    {
        Preset f0 = { "F0", 0, 8, 1, 2.5f, 0.020f, 0.75f, 1.1f, 0.45f, 0.9f, 0.65f, true, 0, 6.0f, 1, 0.0f, 0.8f, 0.0f, 0.2f, 1.0f, 0, 0, 0, 0, 0.0f, 0.65f, 20.0f, 20000.0f, 0.0f, 0, 0.35f, 0.35f };
        Preset f1 = f0; f1.fxType = 1; f1.fxK1 = 0.35f; f1.fxK2 = 0.4f;
        std::vector<float> L2, R2;
        srand(4242); renderOne(bank, drums, L, R, f0, sr);
        srand(4242); renderOne(bank, drums, L2, R2, f1, sr);
        writeWav16("Release/Demo_Drums_ArpFX.wav", L2, R2, sr);
        double diff = 0, peak = 0;
        for (size_t i = 0; i < L.size(); ++i)
        {
            double d = L2[i] - L[i];
            diff += d * d;
            peak = std::max(peak, (double) std::fabs(L2[i]));
        }
        diff = std::sqrt(diff / L.size());
        printf("ARP FX A/B : diff=%.4f peak=%.2f -> %s\n",
               diff, peak, (diff > 0.005 && peak < 0.99) ? "OK" : "PROBLEME");
    }
    // Test A/B chaine FX : meme seed, arp identique, Off/Off/Off vs Delay->Crusher->Chorus.
    {
        Preset c0 = { "C0", 0, 8, 1, 2.5f, 0.020f, 0.75f, 1.1f, 0.45f, 0.9f, 0.65f, true, 0, 6.0f, 1, 0.0f, 0.8f, 0.0f, 0.2f, 1.0f, 0, 0, 0, 0, 0.0f, 0.65f, 20.0f, 20000.0f, 0.0f, 0, 0.35f, 0.35f };
        Preset c1 = c0; c1.fxType = 1; c1.fxK1 = 0.35f; c1.fxK2 = 0.4f;
        c1.fxTypeB = 4; c1.fxB1 = 0.3f; c1.fxB2 = 0.3f;
        c1.fxTypeC = 5; c1.fxC1 = 0.3f; c1.fxC2 = 0.8f; c1.fxMix = 0.8f;
        std::vector<float> L2, R2;
        srand(777); renderOne(bank, drums, L, R, c0, sr);
        srand(777); renderOne(bank, drums, L2, R2, c1, sr);
        writeWav16("Release/Demo_Drums_ArpChain.wav", L2, R2, sr);
        double diff = 0, peak = 0;
        for (size_t i = 0; i < L.size(); ++i)
        {
            double d = L2[i] - L[i];
            diff += d * d;
            peak = std::max(peak, (double) std::fabs(L2[i]));
        }
        diff = std::sqrt(diff / L.size());
        printf("ARP CHAINE A/B : diff=%.4f peak=%.2f -> %s\n",
               diff, peak, (diff > 0.005 && peak < 0.99) ? "OK" : "PROBLEME");
    }
    // Attendu : pas visites 0..9 en boucle, intervalles alternes 1.5x / 0.5x,
    // et chaque cycle de 2 pas = 2 durees de pas (pas de derive).
    {
        Preset t = { "T", 0, 6, 1, 1.0f, 0.008f, 0.8f, 0.0f, 0.0f, 0.0f, 1.0f, true, 0, 6.0f, 2, 0.5f };
        std::vector<float> silence((size_t)(sr * 6), 0.0f);
        g_arpLog.clear();
        g_arpLogOn = true;
        renderOne(bank, silence, L, R, t, sr);
        g_arpLogOn = false;

        bool seqOk = true, swingOk = true, driftOk = true;
        int K = 10;
        double stepDur = (double) sr / 6.0;
        // 1. Sequence : (step+1)%10 a chaque pas (motif Haut, depart step=1)
        for (size_t i = 1; i < g_arpLog.size(); ++i)
        {
            int prev = g_arpLog[i - 1].second;
            int cur = g_arpLog[i].second;
            if (cur != (prev + 1) % K) { seqOk = false; break; }
        }
        // 2. Swing : intervalles alternes autour de 1.5x et 0.5x (+/-5 %)
        for (size_t i = 1; i + 1 < g_arpLog.size(); ++i)
        {
            double d1 = (double)(g_arpLog[i].first - g_arpLog[i - 1].first);
            double d2 = (double)(g_arpLog[i + 1].first - g_arpLog[i].first);
            double r1 = d1 / stepDur, r2 = d2 / stepDur;
            bool alt = (std::fabs(r1 - 1.5) < 0.08 && std::fabs(r2 - 0.5) < 0.08)
                    || (std::fabs(r1 - 0.5) < 0.08 && std::fabs(r2 - 1.5) < 0.08);
            if (! alt) { swingOk = false; break; }
            // 3. Pas de derive : somme de 2 pas consecutifs = 2.0x (+/-2 echantillons)
            if (std::fabs((d1 + d2) - 2.0 * stepDur) > 2.0) { driftOk = false; break; }
        }
        // K distinct visite sur les 10 premiers pas (apres le pas initial)
        bool rangeOk = false;
        if (g_arpLog.size() >= 11)
        {
            bool seen[10] = { false };
            for (size_t i = 1; i <= 10; ++i) seen[g_arpLog[i].second % 10] = true;
            rangeOk = true;
            for (int k = 0; k < 10; ++k) rangeOk = rangeOk && seen[k];
        }
        printf("ARP TIMING : %d pas, seq=%s swing=%s derive=%s etendue=%s -> %s\n",
               (int) g_arpLog.size(),
               seqOk ? "OK" : "PROBLEME", swingOk ? "OK" : "PROBLEME",
               driftOk ? "OK" : "PROBLEME", rangeOk ? "OK" : "PROBLEME",
               (seqOk && swingOk && driftOk && rangeOk) ? "OK" : "PROBLEME");
    }

    // Test paliers CRYSTALLIZE : 0/25/50/75/100 % (meme seed, base arp Maj7).
    // Attendu : 0 % inaudible vs base, diff strictement croissante, pas de clip.
    {
        Preset base = { "Base", 0, 8, 1, 2.5f, 0.020f, 0.75f, 1.1f, 0.45f, 0.9f, 0.65f, true, 0, 6.0f, 1, 0.0f, 0.8f, 0.0f, 0.2f, 1.0f, 0, 0, 0, 0, 0.0f, 0.65f, 20.0f, 20000.0f, 0.0f, 0, 0.35f, 0.35f };
        double prevDiff = -1.0;
        bool progOk = true, peakOk = true;
        for (int si = 0; si < 5; ++si)
        {
            float m = si * 0.25f;
            Preset pc = base; pc.cryoMix = m;
            std::vector<float> L2, R2;
            srand(2026); renderOne(bank, drums, L, R, base, sr);
            srand(2026); renderOne(bank, drums, L2, R2, pc, sr);
            if (si == 4)
                writeWav16("Release/Demo_Drums_Cryo.wav", L2, R2, sr);
            double diff = 0, peak = 0;
            for (size_t i = 0; i < L.size(); ++i)
            {
                double d = L2[i] - L[i];
                diff += d * d;
                peak = std::max(peak, (double) std::fabs(L2[i]));
            }
            diff = std::sqrt(diff / L.size());
            // Seuil 1e-4 (-80 dB) : inaudible ; un residu ~1e-6 existe (etat FP),
            // le point musical est "aucun changement audible a 0 %".
            bool okStep = (si == 0) ? (diff < 1e-4) : (diff > prevDiff);
            printf("  CRYO %3.0f %% : diff=%.7f peak=%.2f -> %s\n",
                   m * 100, diff, peak, okStep ? "OK" : "PROBLEME");
            if (! okStep) progOk = false;
            if (peak > 0.99) peakOk = false;
            prevDiff = diff;
        }
        printf("CRYO PALIERS : progressif=%s clip=%s -> %s\n",
               progOk ? "OK" : "PROBLEME", peakOk ? "OK" : "PROBLEME",
               (progOk && peakOk) ? "OK" : "PROBLEME");
    }

    // Test etage Sortie : 0 dB vs -6 dB (meme seed) -> RMS divise par ~2
    {
        Preset g0 = { "G0", 5, 6, 1, 3.2f, 0.150f, 0.65f, 1.1f, 0.55f, 0.90f, 0.60f, false, 0, 4.0f, 1, 0.0f, 0.8f, 0.0f, 0.2f, 1.0f, 0, 0, 0, 0, 0.0f, 0.65f, 20.0f, 20000.0f, 0.0f, 0, 0.35f, 0.35f };
        Preset g1 = g0; g1.outDb = -6.0f;
        std::vector<float> L2, R2;
        srand(31337); renderOne(bank, drums, L, R, g0, sr);
        srand(31337); renderOne(bank, drums, L2, R2, g1, sr);
        auto rmsAll = [](const std::vector<float>& v)
        {
            double e = 0;
            for (auto x : v) e += x * x;
            return std::sqrt(e / v.size());
        };
        double r0 = rmsAll(L), r1 = rmsAll(L2);
        double peak = 0; for (auto x : L) peak = std::max<double>(peak, std::fabs(x));
        printf("SORTIE 0/-6 dB : rms=%.4f/%.4f ratio=%.2f (attendu ~2.00) -> %s\n",
               r0, r1, r0 / (r1 + 1e-12), std::fabs(r0 / (r1 + 1e-12) - 2.0) < 0.1 ? "OK" : "PROBLEME");
    }

    printf("Termine : ecoutez Release/Demo_*.wav\n");
    return 0;
}
