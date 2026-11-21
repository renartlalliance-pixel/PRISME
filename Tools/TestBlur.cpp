// Test Blur : impulsion -> la queue doit decroitre doucement (reverb), pas exploser.
#include <cstdio>
#include <cmath>
#include <vector>
#include "../Source/BlurReverb.h"

static double measureRT(const BlurReverb& proto, double sr, int seconds)
{
    BlurReverb verb = proto;
    verb.reset();
    int n = (int)(sr * seconds);
    std::vector<double> env(n / 512, 0.0);
    double peak = 0;
    for (int i = 0; i < n; ++i)
    {
        float in = (i == 0) ? 1.0f : 0.0f;
        float a, b;
        verb.process(in, in, a, b);
        float m = (a + b) * 0.5f;
        int k = i / 512;
        env[(size_t)k] += (double)(m * m);
        if (std::fabs(m) > peak) peak = std::fabs(m);
    }
    for (auto& e : env) e = std::sqrt(e / 512.0);
    // Cherche le max puis la tombee a -60 dB APRES lui (ignore le pre-delay)
    size_t kMax = 0;
    double mx = 0;
    for (size_t k = 0; k < env.size(); ++k)
        if (env[k] > mx) { mx = env[k]; kMax = k; }
    double target = mx * 0.001;
    for (size_t k = kMax; k < env.size(); ++k)
        if (env[k] < target) return k * 512.0 / sr;
    return -1.0;
}

int main()
{
    const double sr = 44100.0;
    BlurReverb verb;
    verb.setSampleRate(sr);

    bool stable = true;
    for (float size : { 0.0f, 0.5f, 0.75f, 1.0f })
    {
        verb.setSize(size);
        double rt = measureRT(verb, sr, 12);
        // Stabilite : sortie bornee (pas d'explosion)
        BlurReverb v2 = verb; v2.reset();
        float mx = 0;
        for (int i = 0; i < (int)(sr * 12); ++i)
        {
            float in = (i == 0) ? 1.0f : 0.0f;
            float a, b; v2.process(in, in, a, b);
            if (std::fabs(a) > mx) mx = std::fabs(a);
            if (std::fabs(b) > mx) mx = std::fabs(b);
        }
        if (mx > 4.0f) stable = false;
        printf("size %.2f : RT60~%.2fs  peak=%.2f\n", size, rt, mx);
    }
    printf(stable ? "BLUR OK : stable, decay progressif\n" : "BLUR PROBLEME : instable\n");

    // Pre-delay : silence avant l'arrivee de la nappe
    {
        BlurReverb v; v.setSampleRate(sr); v.setSize(0.75f);
        v.setPreDelay(0.1f); v.reset();
        float mx = 0;
        for (int i = 0; i < 5000; ++i)
        {
            float in = (i == 0) ? 1.0f : 0.0f;
            float a, b; v.process(in, in, a, b);
            mx = std::max(mx, std::fabs(a));
        }
        printf("predelay 100ms : max sur 5000 premiers echantillons = %.4f -> %s\n",
               mx, mx < 0.01f ? "OK" : "PROBLEME");
        if (mx >= 0.01f) stable = false;
    }

    // Couleur : brillant = plus d'aigus que sombre (bruit continu)
    {
        auto tilt = [&](float color)
        {
            BlurReverb v; v.setSampleRate(sr); v.setSize(0.75f); v.setColor(color); v.reset();
            srand(7);
            double lo = 0, hi = 0; float yl = 0;
            for (int i = 0; i < (int)(sr * 2); ++i)
            {
                float nz = (float)(rand() % 2000 - 1000) / 1000.0f * 0.5f;
                float a, b; v.process(nz, nz, a, b);
                float m = (a + b) * 0.5f;
                yl += 0.05f * (m - yl); // passe-bas 1 pole
                float h = m - yl;
                lo += yl * yl; hi += h * h;
            }
            return hi / (lo + 1e-9);
        };
        double dark = tilt(0.0f), bright = tilt(1.0f);
        printf("couleur : sombre=%.3f brillant=%.3f -> %s\n",
               dark, bright, bright > dark * 1.3 ? "OK" : "PROBLEME");
        if (! (bright > dark * 1.3)) stable = false;
    }

    // Largeur 0 : L == R exactement
    {
        BlurReverb v; v.setSampleRate(sr); v.setSize(0.75f); v.setWidth(0.0f); v.reset();
        srand(7);
        double diff = 0;
        for (int i = 0; i < (int)(sr * 1); ++i)
        {
            float nz = (float)(rand() % 2000 - 1000) / 1000.0f * 0.5f;
            float a, b; v.process(nz, nz * 0.5f, a, b);
            diff += std::fabs(a - b);
        }
        printf("largeur 0 : diff L-R = %.6f -> %s\n", diff, diff < 1e-4 ? "OK" : "PROBLEME");
        if (diff >= 1e-4) stable = false;
    }

    // Decay x2 : RT60 plus long qu'a x1
    {
        BlurReverb v; v.setSampleRate(sr); v.setSize(0.75f);
        v.setDecayMul(1.0f);
        double rt1 = measureRT(v, sr, 12);
        v.setDecayMul(2.0f);
        double rt2 = measureRT(v, sr, 12);
        printf("decay : x1 -> %.2fs, x2 -> %.2fs -> %s\n", rt1, rt2, rt2 > rt1 * 1.5 ? "OK" : "PROBLEME");
        if (! (rt2 > rt1 * 1.5)) stable = false;
    }

    // Etage de gain : entree forte continue (decay long) -> plafond tenu, toujours audible
    {
        BlurReverb v; v.setSampleRate(sr); v.setSize(1.0f);
        v.setDecayMul(3.0f);
        v.reset();
        srand(99);
        double peak = 0, eTail = 0;
        int n = (int)(sr * 6);
        for (int i = 0; i < n; ++i)
        {
            float nz = (float)(rand() % 2000 - 1000) / 1000.0f * 0.9f;
            float a, b; v.process(nz, nz, a, b);
            if (i > n - (int)(sr * 2))
            {
                peak = std::max(peak, (double) std::fabs(a));
                eTail += a * a;
            }
        }
        double tailRms = std::sqrt(eTail / (sr * 2));
        printf("fort : pic tenu=%.2f queue=%.4f -> %s\n", peak, tailRms,
               (peak <= 0.75 && tailRms > 0.05) ? "OK" : "PROBLEME");
        if (! (peak <= 0.75 && tailRms > 0.05)) stable = false;
    }

    printf(stable ? "MIRAGE COMPLET OK\n" : "MIRAGE PROBLEME\n");
    return stable ? 0 : 1;
}
