// Test Delay : ping-pong alterne L/R, repetitions adoucies, pas d'explosion.
#include <cstdio>
#include <cmath>
#include <vector>
#include "../Source/ArpFX.h"

int main()
{
    const double sr = 44100.0;
    ArpFX::Engine fx;
    fx.setSampleRate(sr);
    fx.reset();

    // Impulsion a gauche seule, delay ~110 ms (k1=0.5), fb 0.45
    const int n = (int)(sr * 1.5);
    std::vector<float> oL(n, 0.0f), oR(n, 0.0f);
    float peak = 0;
    for (int i = 1; i < n; ++i) // impulsion a i=1, mesure des i=2 (pas le dry lui-meme)
    {
        float in = (i == 1) ? 1.0f : 0.0f;
        float a, b;
        fx.process(in, 0.0f, a, b, 1, 0.5f, 0.5f);
        oL[(size_t)i] = a; oR[(size_t)i] = b;
        if (i >= 2) { peak = std::max(peak, std::fabs(a)); peak = std::max(peak, std::fabs(b)); }
    }
    auto windowPeak = [&](const std::vector<float>& v, int a, int b)
    {
        float m = 0;
        for (int i = a; i < b && i < n; ++i) m = std::max(m, std::fabs(v[(size_t)i]));
        return m;
    };
    // Premier echo a gauche vers 110 ms, puis reponse a droite
    float earlyL = windowPeak(oL, 3000, 7000);
    float earlyR = windowPeak(oR, 3000, 7000);
    float lateL = windowPeak(oL, 20000, 30000);
    float lateR = windowPeak(oR, 20000, 30000);
    printf("echo1: L=%.3f R=%.3f | tard: L=%.3f R=%.3f | peak=%.2f\n",
           earlyL, earlyR, lateL, lateR, peak);

    // Douceur : contenu HF relatif plus faible tard que tot (passe-bas boucle)
    auto hfRatio = [&](const std::vector<float>& v, int a, int b)
    {
        double lo = 0, hi = 0;
        float y = 0;
        for (int i = a; i < b && i < n; ++i)
        {
            y += 0.248f * (v[(size_t)i] - y);
            double h = v[(size_t)i] - y;
            lo += (double)(y * y); hi += h * h;
        }
        return hi / (lo + 1e-9);
    };
    double rEarly = hfRatio(oL, 3000, 7000);
    double rLate = hfRatio(oL, 20000, 30000) + hfRatio(oR, 20000, 30000);
    printf("HF tot=%.3f tard=%.3f\n", rEarly, rLate);

    bool pingpong = earlyL > 0.1f && earlyL > earlyR * 2.0f && (lateL + lateR) > 0.001f;
    bool doux = rLate < rEarly;
    bool stable = peak < 0.99f;
    printf("pingpong=%s douceur=%s stable=%s -> %s\n",
           pingpong ? "OK" : "PROBLEME", doux ? "OK" : "PROBLEME",
           stable ? "OK" : "PROBLEME",
           (pingpong && doux && stable) ? "OK" : "PROBLEME");
    return (pingpong && doux && stable) ? 0 : 1;
}
