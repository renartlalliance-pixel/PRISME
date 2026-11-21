// Test : entree constante -> l'enveloppe de sortie doit suivre le LFO de mouvement.
#include <cstdio>
#include <cmath>
#include <vector>
#include "../Source/ResonatorBank.h"
#include "../Source/Scales.h"

int main()
{
    const int sr = 44100;
    PrismResonatorBank bank;
    bank.setSampleRate(sr);
    bank.setTuning(PrismeScales::buildTuning(0, 0, 1, 12)); // C Major
    bank.setDecay(1.0, false);
    bank.setBrightness(0.7f);
    bank.setShimmer(0.0f); // pas de shimmer pour isoler le mouvement
    bank.setSpread(0.0f);
    bank.setEmphSmooth(0.005);

    // Bruit blanc constant comme excitation (pas de trous rythmiques)
    srand(42);
    const int n = sr * 6;
    double sum = 0, sumSq = 0;
    // Deux passes : sans mouvement puis avec (0.5 Hz, force 1, forme 0.1)
    for (int pass = 0; pass < 2; ++pass)
    {
        bank.reset();
        bank.resetEmphasis();
        if (pass == 0) bank.setMovement(0.5f, 0.0f, 0.1f, 1.0f);
        else bank.setMovement(0.5f, 1.0f, 0.1f, 1.0f);
        // Chauffe 1s
        for (int i = 0; i < sr; ++i)
        {
            float nz = (float)(rand() % 2000 - 1000) / 1000.0f * 0.5f;
            float a, b; bank.processSample(nz, a, b);
        }
        // Mesure RMS par fenetre de 100ms sur 5s
        const int win = sr / 10;
        double mn = 1e9, mx = 0;
        std::vector<double> env;
        for (int k = 0; k < n; k += win)
        {
            double e = 0;
            for (int i = 0; i < win; ++i)
            {
                float nz = (float)(rand() % 2000 - 1000) / 1000.0f * 0.5f;
                float a, b; bank.processSample(nz, a, b);
                float m = (a + b) * 0.5f;
                e += m * m;
            }
            e = sqrt(e / win);
            env.push_back(e);
            if (e < mn) mn = e;
            if (e > mx) mx = e;
        }
        printf("pass %d (depth %s): min=%.4f max=%.4f ratio=%.1f\n",
               pass, pass ? "1.0" : "0.0", mn, mx, mx / (mn + 1e-9));
    }
    return 0;
}
