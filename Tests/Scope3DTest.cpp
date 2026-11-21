// ---- Tests automatises de l'oscilloscope PRISME (Scope3D) -----------------
// Objectif : verifier le chemin de donnees et les regles demandees, SANS
// preter a confusion sur ce qui est ou n'est pas teste.
//
// Ce programme n'execute PAS le moteur audio de PRISME : il injecte des
// echantillons connus dans le meme type de FIFO que celui du processeur,
// puis relit ce que l'oscilloscope en a fait. Il couvre donc le transfert
// thread audio -> thread UI, la stéréo, les transitoires, le silence,
// l'auto-echelle et les tailles de buffer variees.
// Le rendu est teste en mode tete (Image logicielle), sans fenetre.
//
// Execution : PrismeScopeTest.exe   (code retour 0 = tous les tests passes)
#include "Scope3D.h"

#include <cstdio>
#include <cmath>
#include <vector>

static int gChecks = 0;
static int gFails = 0;

static void check(bool ok, const char* what)
{
    ++gChecks;
    if (!ok)
        ++gFails;
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
}

static void section(const char* name)
{
    std::printf("\n== %s ==\n", name);
}

static bool near(float a, float b, float tol)
{
    return std::fabs(a - b) <= tol;
}

// Reproduit exactement l'ecriture du processeur : un item = une paire [L, R].
struct Driver
{
    juce::AbstractFifo fifo { 16384 };
    std::vector<float> buf = std::vector<float>(2 * 16384, 0.0f);
    int dropped = 0;

    bool pushOne(float l, float r)
    {
        int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
        fifo.prepareToWrite(1, s1, n1, s2, n2);
        if (n1 <= 0)
        {
            ++dropped; // comportement prevu : on jette, on ne bloque jamais
            return false;
        }
        buf[(size_t) (2 * s1)] = l;
        buf[(size_t) (2 * s1 + 1)] = r;
        fifo.finishedWrite(1);
        return true;
    }

    // Remplit n echantillons avec une fonction f(i).
    template <typename F>
    void pushN(int n, F&& f)
    {
        for (int i = 0; i < n; ++i)
            pushOne(f(i), f(i));
    }

    void drain(Scope3D& s) { s.pushFromFifo(fifo, buf); }
};

// Identique a Driver mais avec des canaux differents (test d'alignement).
struct SplitDriver
{
    juce::AbstractFifo fifo { 16384 };
    std::vector<float> buf = std::vector<float>(2 * 16384, 0.0f);

    void pushN(int n, float l, float r)
    {
        for (int i = 0; i < n; ++i)
        {
            int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
            fifo.prepareToWrite(1, s1, n1, s2, n2);
            if (n1 <= 0)
                continue;
            buf[(size_t) (2 * s1)] = l;
            buf[(size_t) (2 * s1 + 1)] = r;
            fifo.finishedWrite(1);
        }
    }

    void drain(Scope3D& s) { s.pushFromFifo(fifo, buf); }
};

int main()
{
    std::printf("PRISME - tests de l'oscilloscope Scope3D\n");

    // ---------------------------------------------------------------- T1
    section("T1 - silence : ligne centrale stable, echelle au plancher");
    {
        Scope3D s;
        Driver d;
        d.pushN(4096, [](int) { return 0.0f; });
        d.drain(s);
        const bool fresh = s.update();

        check(fresh, "des traces sont construites meme en silence");
        check(s.samplesSeen() == 4096, "tous les echantillons ont traverse le FIFO");
        check(near(s.newestTraceMax(true), 0.0f, 1.0e-6f), "trace L = 0 (ligne plate)");
        check(near(s.newestTraceMin(true), 0.0f, 1.0e-6f), "trace L min = 0");
        check(near(s.newestTraceMax(false), 0.0f, 1.0e-6f), "trace R = 0");
        check(near(s.displayScale(), 0.02f, 1.0e-6f),
              "echelle bloquee au plancher (pas d'amplification du bruit)");
    }

    // ---------------------------------------------------------------- T2
    section("T2 - signal continu : auto-echelle sans ecrantage");
    {
        Scope3D s;
        Driver d;
        const double w = 2.0 * 3.14159265358979 / 64.0;
        d.pushN(8192, [&](int i)
                { return 0.5f * (float) std::sin(w * (double) i); });
        d.drain(s);
        s.update();

        check(near(s.observedPeak(), 0.5f, 0.02f), "crete mesuree ~ 0.5");
        check(s.displayScale() >= s.observedPeak() - 1.0e-4f,
              "echelle >= crete : aucun ecrantage");
        check(s.displayScale() <= s.observedPeak() * 1.05f,
              "echelle resserree sur le signal : reste lisible");
    }

    // ---------------------------------------------------------------- T3
    section("T3 - stero : canaux alignes et non permutes");
    {
        Scope3D s;
        SplitDriver d;
        // Fenetre 1 : L=+0.60, R=-0.30
        d.pushN(1024, 0.60f, -0.30f);
        d.drain(s);
        s.update();
        // Fenetre 2 (= une trace entiere) : L=+0.10, R=-0.05
        d.pushN(1024, 0.10f, -0.05f);
        d.drain(s);
        s.update();

        check(near(s.newestTraceMax(true), 0.10f, 1.0e-3f),
              "la trace la plus recente porte les valeurs de L (fenetre 2)");
        check(near(s.newestTraceMin(false), -0.05f, 1.0e-3f),
              "la trace la plus recente porte les valeurs de R (fenetre 2)");
        check(near(s.newestTraceMax(true), 0.10f, 1.0e-3f)
                  && !near(s.newestTraceMax(true), 0.60f, 1.0e-3f),
              "les canaux ne sont ni permutes ni decalés d'une fenetre");
    }

    // ---------------------------------------------------------------- T4
    section("T4 - transitoire court conserve");
    {
        Scope3D s;
        Driver d;
        // 1023 echantillons nuls + 1 impulsion de crete 1.0
        d.pushN(1024, [](int i) { return i == 700 ? 1.0f : 0.0f; });
        d.drain(s);
        const bool fresh = s.update();

        check(fresh, "une trace est construite");
        check(s.observedPeak() >= 0.999f,
              "l'impulsion est detectee (pas de lissage qui l'efface)");
        check(s.newestTraceMax(true) >= 0.999f, "le min/max par colonne la garde");
        check(s.displayScale() >= 0.999f, "l'echelle s'adapte immediatement");
    }

    // ---------------------------------------------------------------- T5
    section("T5 - tailles de buffer audio variees (chunks de 7 a 4096)");
    {
        const int chunkSizes[] = { 7, 64, 512, 4096 };
        float peaks[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        for (int k = 0; k < 4; ++k)
        {
            Scope3D s;
            Driver d;
            const int total = 4096;
            const int chunk = chunkSizes[k];
            for (int i = 0; i < total; i += chunk)
            {
                const int n = (i + chunk <= total) ? chunk : (total - i);
                for (int j = 0; j < n; ++j)
                {
                    const int idx = i + j;
                    // 3800 tombe dans la DERNIERE fenetre de 1024 echantillons
                    // ([3072..4096[) : c'est elle que observedPeak() rapporte.
                    const float v = (idx == 3800) ? 0.75f
                                                  : 0.0f;
                    d.pushOne(v, v);
                }
                d.drain(s); // le plugin draine a chaque tick
            }
            s.update();
            peaks[k] = s.observedPeak();
        }

        check(near(peaks[0], 0.75f, 1.0e-3f) && near(peaks[1], 0.75f, 1.0e-3f)
                  && near(peaks[2], 0.75f, 1.0e-3f) && near(peaks[3], 0.75f, 1.0e-3f),
              " resultat identique quelle que soit la taille de bloc");
    }

    // ---------------------------------------------------------------- T6
    section("T6 - frequences d'echantillonnage variees");
    {
        const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
        bool allOk = true;
        bool lenOk = true;

        for (double sr : rates)
        {
            Scope3D s;
            s.setSampleRate(sr);
            Driver d;
            d.pushN(4096, [](int i)
                    { return 0.3f * (float) std::sin(0.05 * (double) i); });
            d.drain(s);
            const bool fresh = s.update();

            if (! fresh) allOk = false;
            if (s.frameLength() < Scope3D::Cols * 2
                || s.frameLength() > Scope3D::RingSize / 4) lenOk = false;
            if (s.displayScale() < 0.02f) allOk = false;
        }

        check(allOk, "traces construites et echelle valide a tous les taux");
        check(lenOk, "fenetre de trace toujours dans les bornes [192 .. 2048]");
    }

    // ---------------------------------------------------------------- T7
    section("T7 - debordement du FIFO : jamais de blocage");
    {
        Scope3D s;
        Driver d;
        int written = 0;
        for (int i = 0; i < 40000; ++i) // >> capacite (16384)
            if (d.pushOne(0.4f, 0.4f)) ++written;

        check(written <= 16384, "ecriture bornee par la capacite du FIFO");
        check(d.dropped > 0, "l'excédent est jete, pas bufferise (pas de blocage)");
        d.drain(s);
        s.update();
        check(s.samplesSeen() == (long long) written,
              "l'UI n'a lu que ce qui a ete effectivement ecrit");
    }

    // ---------------------------------------------------------------- T8
    section("T8 - rendu en mode tete (sans fenetre)");
    {
        bool ok = true;
        try
        {
            Scope3D s;
            Driver d;
            d.pushN(4096, [](int i)
                    { return 0.6f * (float) std::sin(0.08 * (double) i); });
            d.drain(s);
            s.update();

            juce::Image img(juce::Image::RGB, 576, 164, true);
            juce::Graphics g(img);
            s.draw(g, juce::Rectangle<float>(0.0f, 0.0f, 576.0f, 164.0f));
            // Dessine aussi avec des dimensions degenerées : ne doit pas planter.
            s.draw(g, juce::Rectangle<float>(0.0f, 0.0f, 4.0f, 4.0f));
        }
        catch (...)
        {
            ok = false;
        }
        check(ok, "draw() s'execute sans exception (y compris zone trop petite)");
    }

    // ---------------------------------------------------------------- T9
    section("T9 - aucun contenu simule : reinitialisation = silence");
    {
        Scope3D s;
        Driver d;
        d.pushN(4096, [](int) { return 0.8f; });
        d.drain(s);
        s.update();
        const bool hadSignal = s.observedPeak() > 0.5f;

        s.reset();
        d.drain(s);
        s.update();

        check(hadSignal, "signal pris en compte avant reset");
        check(near(s.newestTraceMax(true), 0.0f, 1.0e-6f),
              "apres reset, retour strict au silence");
        check(near(s.displayScale(), 0.02f, 1.0e-6f),
              "apres reset, echelle de nouveau au plancher");
    }

    std::printf("\n---------------------------------------------\n");
    std::printf("%d verifications, %d echec(s)\n", gChecks, gFails);
    std::printf("%s\n", gFails == 0 ? "TOUS LES TESTS PASSENT" : "ECHEC");
    return gFails == 0 ? 0 : 1;
}
