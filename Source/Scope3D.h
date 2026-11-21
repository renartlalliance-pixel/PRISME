#pragma once
// ---- Oscilloscope 3D temps reel (panneau SPECTRUM) ----------------------
// Affiche la forme d'onde du signal TRAITE par PRISME. Le prelevage est fait
// dans processBlock APRES le limiteur tanh et les gains de sortie : c'est
// exactement ce que le plugin produit, jamais un signal simule.
//
// Transfert thread audio -> thread UI (lock-free) :
//   * juce::AbstractFifo + tampon pre-alloue a la construction.
//   * Thread audio : ECRITURE seule. Aucune allocation, aucun mutex, aucun
//     appel GUI. Si le FIFO est plein, l'echantillon est jete (jamais de
//     blocage) : meme regle que le spectrogramme existant.
//   * Thread UI : lecture + dessin uniquement. Deja pre-alloue ici.
//   * Un seul consommateur : l'editeur. La vue RECORDER n'y touche pas.
//   * UN item = UNE paire [L, R] : les deux canaux ne peuvent pas se
//     desynchroniser, quoi qu'il arrive aux tailles de buffer.
//
// Le relief 3D est un empilement de TRACES REELLES : chaque ligne est la
// forme d'onde du signal sur une fenetre d' `frameLen` echantillons, les
// lignes plus anciennes sont reculees, retrecies et estompees (perspective).
// Aucune donnee n'est fabriquee : seuls des echantillons arrives du DSP
// sont dessines.
//
// Thread UI uniquement. Aucun member n'est lu ni ecrit par le thread audio.
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <cfloat>

class Scope3D
{
public:
    static constexpr int Cols = 96;  // points par trace
    static constexpr int Rows = 3;   // profondeur (traces empilees)
    // 3 et non 8 : chaque bande gagne le triple de hauteur. Sur le panneau
    // de 164 px, le pas fait ~50,7 px au lieu de 19, donc le trace en direct
    // occupe ~61 px au lieu de ~23 px. Le relief 3D reste lisible.
    static constexpr int RingSize = 8192; // tampon circulaire d'histo
    static constexpr int DefaultFrameLen = 1024;
    static constexpr float kFloor = 0.02f; // echelle minimale : evite de
                                           // amplifier le bruit en plein ecran

    Scope3D() { reset(); }

    // Reinitialise tout en etat "silence" : lignes centrales stables.
    void reset()
    {
        ringL.assign((size_t) RingSize, 0.0f);
        ringR.assign((size_t) RingSize, 0.0f);
        histMinL.assign((size_t) (Rows * Cols), 0.0f);
        histMaxL.assign((size_t) (Rows * Cols), 0.0f);
        histMinR.assign((size_t) (Rows * Cols), 0.0f);
        histMaxR.assign((size_t) (Rows * Cols), 0.0f);
        scrMinL.assign((size_t) Cols, 0.0f);
        scrMaxL.assign((size_t) Cols, 0.0f);
        scrMinR.assign((size_t) Cols, 0.0f);
        scrMaxR.assign((size_t) Cols, 0.0f);

        ringWrite = 0;
        sinceFrame = 0;
        head = 0;
        frameLen = DefaultFrameLen;
        sampleRate = 0.0;
        seen = 0;
        peakHold = kFloor;
        lastTracePeak = 0.0f;
    }

    // Une trace couvre ~25 ms quel que soit le taux d'echantillonnage :
    // le relief reste lisible de 44,1 a 192 kHz. sinceFrame remis a zero
    // pour ne jamais construire avec un frameLen qui vient de changer.
    void setSampleRate(double sr)
    {
        if (sr <= 0.0 || sr == sampleRate)
            return;
        sampleRate = sr;
        int fl = (int) std::lround(sr * 0.025);
        if (fl < Cols * 2) fl = Cols * 2;
        if (fl > RingSize / 4) fl = RingSize / 4;
        frameLen = fl;
        sinceFrame = 0;
    }

    void setColors(juce::Colour deep, juce::Colour mid, juce::Colour hot)
    {
        colDeep = deep;
        colMid = mid;
        colHot = hot;
    }

    // Draine le FIFO vers le tampon circulaire. Thread UI uniquement.
    // `buf` contient des paires [L, R] : 2 floats par item.
    void pushFromFifo(juce::AbstractFifo& fifo, std::vector<float>& buf)
    {
        const int avail = fifo.getNumReady();
        if (avail <= 0)
            return;

        int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
        fifo.prepareToRead(avail, s1, n1, s2, n2);

        auto pull = [this, &buf](int start, int num)
        {
            for (int i = 0; i < num; ++i)
            {
                const int item = start + i;
                ringL[(size_t) ringWrite] = buf[(size_t) (2 * item)];
                ringR[(size_t) ringWrite] = buf[(size_t) (2 * item + 1)];
                ringWrite = (ringWrite + 1) % RingSize;
            }
        };
        pull(s1, n1);
        pull(s2, n2);

        const int done = n1 + n2;
        if (done > 0)
        {
            fifo.finishedRead(done);
            seen += done;
            sinceFrame += done;
        }
    }

    // Construit les nouvelles traces. Rend vrai si le contenu est neuf
    // (l'appelant doit alors repeindre le panneau).
    bool update()
    {
        // Trop de retard : on ne garde que la fenetre utile. Le curseur
        // `ringWrite - sinceFrame` reste alors toujours valide.
        if (sinceFrame > frameLen * Rows)
            sinceFrame = frameLen * Rows;

        bool fresh = false;
        while (sinceFrame >= frameLen)
        {
            buildTrace(ringWrite - sinceFrame);
            pushHistory();
            sinceFrame -= frameLen;
            fresh = true;
        }
        if (fresh)
            updateScale();
        return fresh;
    }

    // Verifications (journal / tests) : le thread UI lit ces membres.
    long long samplesSeen() const { return seen; }
    float observedPeak() const { return lastTracePeak; }
    float displayScale() const { return peakHold; }

    // Acces en lecture pour les tests automatises uniquement.
    // Jamais appeles par le rendu ni par le thread audio (cout nul).
    int frameLength() const { return frameLen; }
    float newestTraceMax(bool left) const
    {
        const size_t o = (size_t) (head * Cols);
        const std::vector<float>& v = left ? histMaxL : histMaxR;
        float m = -FLT_MAX;
        for (int c = 0; c < Cols; ++c)
            if (v[o + (size_t) c] > m) m = v[o + (size_t) c];
        return m;
    }
    float newestTraceMin(bool left) const
    {
        const size_t o = (size_t) (head * Cols);
        const std::vector<float>& v = left ? histMinL : histMinR;
        float m = FLT_MAX;
        for (int c = 0; c < Cols; ++c)
            if (v[o + (size_t) c] < m) m = v[o + (size_t) c];
        return m;
    }

    void draw(juce::Graphics& g, juce::Rectangle<float> area) const
    {
        const float W = area.getWidth();
        const float H = area.getHeight();
        if (W < 8.0f || H < 8.0f)
            return;

        const float padT = 6.0f;
        const float padB = 6.0f;
        const float padX = 6.0f;
        const float rowStep = (H - padT - padB) / (float) Rows;
        // 0.60 et non 0.62 : deux bandes voisines se chevauchent d'environ
        // 10 px, ce qui laisse gagner de la hauteur de pic sans deborder.
        const float halfH = rowStep * 0.60f;
        const float cx = area.getX() + W * 0.5f;
        const float halfW0 = (W - padX * 2.0f) * 0.5f;
        const float depth = 0.22f; // retrecissement vers l'arriere, volontairement
                                   // faible : le relief vient surtout des decales
                                   // horizontaux et du degrade. Ca laisse les
                                   // 3 ondes se chevaucher d'environ 1 a 7 px.

        // ---- Grille discrete (fond sombre conserve) ----
        g.setColour(colDeep.withAlpha(0.50f));
        for (int j = 0; j < Rows; ++j)
        {
            const float yc = area.getY() + padT + (float) (j + 0.5f) * rowStep;
            g.drawHorizontalLine((int) yc, area.getX(), area.getX() + W);
        }
        for (int i = 1; i < 8; ++i)
        {
            const float x = area.getX() + (float) i * W / 8.0f;
            g.drawVerticalLine((int) x, area.getY() + padT, area.getY() + H - padB);
        }

        // ---- Ligne centrale : stable au silence, trace en direct dessus ----
        const float ycLive = area.getY() + padT + (float) (Rows - 0.5f) * rowStep;
        g.setColour(colMid.withAlpha(0.70f));
        g.drawHorizontalLine((int) ycLive, area.getX(), area.getX() + W);

        // ---- Traces, de l'arriere (ancien, estompe) vers l'avant ----
        const float scale = peakHold > 1.0e-6f ? 1.0f / peakHold : 1.0f;

        for (int j = 0; j < Rows; ++j)
        {
            // head = index de la trace la plus recente.
            const int slot = (head - (Rows - 1 - j) + Rows * 2) % Rows;
            const float d = (float) (Rows - 1 - j) / (float) (Rows - 1); // 0 = recent
            const float s = 1.0f - depth * d;
            const float a = 0.16f + 0.84f * (1.0f - d);
            const float yc = area.getY() + padT + (float) (j + 0.5f) * rowStep;
            const float hw = halfW0 * s;
            const float hh = halfH * s;
            const size_t o = (size_t) (slot * Cols);
            const bool live = (j == Rows - 1);

            // Leger decale horizontal par couche : la bande du haut part vers
            // la droite, celle du milieu vers la gauche, la trace en direct
            // reste centree. Le decalage croise le retrecissement en profondeur
            // -> effet de ruban tourne.
            const float kShift = 18.0f;
            const float rowShift = (j == 0 ? kShift : (j == 1 ? -kShift : 0.0f));

            // Leger degrade de couleur entre les couches : fonce (lointain)
            // -> accent (milieu) -> clair (en direct).
            // Les deux canaux restent legerement differents : L en clair,
            // R un ton plus fonce.
            const float ct = (Rows > 1) ? (float) j / (float) (Rows - 1) : 1.0f;
            const juce::Colour rowCol = (ct < 0.5f)
                ? colDeep.interpolatedWith(colMid, ct * 2.0f)
                : colMid.interpolatedWith(colHot, (ct - 0.5f) * 2.0f);
            const juce::Colour colL = rowCol;
            const juce::Colour colR = rowCol.darker(0.22f);

            auto yAt = [&](float v)
            {
                const float n = v * scale;
                const float cl = n > 1.0f ? 1.0f : (n < -1.0f ? -1.0f : n);
                return yc - cl * hh;
            };

            // Geometrie commune + les 4 bords de l'enveloppe, en pixels ecran.
            std::array<float, Cols> xs, mxL, mnL, mxR, mnR;
            for (int c = 0; c < Cols; ++c)
            {
                const float u = (float) c / (float) (Cols - 1);
                xs[(size_t) c] = cx + rowShift + (u - 0.5f) * 2.0f * hw;
                mxL[(size_t) c] = yAt(histMaxL[o + (size_t) c]);
                mnL[(size_t) c] = yAt(histMinL[o + (size_t) c]);
                mxR[(size_t) c] = yAt(histMaxR[o + (size_t) c]);
                mnR[(size_t) c] = yAt(histMinR[o + (size_t) c]);
            }

            // Corps de la trace : la couleur porte le canal (R accent,
            // L accent clair). Inchange par rapport au fond sombre.
            auto fillBand = [&](const float* top, const float* bot, juce::Colour col)
            {
                juce::Path p;
                p.startNewSubPath(xs[0], top[0]);
                for (int c = 1; c < Cols; ++c)
                    p.lineTo(xs[(size_t) c], top[(size_t) c]);
                for (int c = Cols - 1; c >= 0; --c)
                    p.lineTo(xs[(size_t) c], bot[(size_t) c]);
                p.closeSubPath();
                g.setColour(col.withAlpha(a * 0.24f));
                g.fillPath(p);
            };
            fillBand(mxR.data(), mnR.data(), colR);
            fillBand(mxL.data(), mnL.data(), colL);

            // ---- Faisceau a luminosite variable (PDM) ----
            // Transposition directe de la technique du depot GitHub :
            // la luminosite suit le temps de sejour du faisceau. Ici l'axe
            // temps avance a vitesse constante, donc c'est la pente du signal
            // qui determine la longueur parcourue par segment :
            //   * signal plat  -> segment court -> sejour long  -> CLAIR
            //   * transitoire  -> segment long  -> sejour court -> TERNE
            // 100 % derive de la derivee du signal reel. Aucune geometrie
            // fabriquee, aucun contenu simule.
            const float thick = 1.05f + 0.55f * s;
            strokeDensity(g, xs.data(), mxR.data(), Cols, colR, a, thick);
            strokeDensity(g, xs.data(), mxL.data(), Cols, colL, a, thick);
            if (live) // trace en direct : les deux bords de l'enveloppe
            {
                strokeDensity(g, xs.data(), mnR.data(), Cols, colR, a, thick);
                strokeDensity(g, xs.data(), mnL.data(), Cols, colL, a, thick);
            }
        }
    }

    // ---- Luminosite par densite de sejour (PDM) ----
    // Chaque segment est classe dans un palier selon sa longueur ; tous les
    // segments d'un palier sont traces d'un SEUL coup (peu d'appels graphiques).
    // Les paliers vides sont sautes : sur un signal doux, une seule ou deux
    // valeurs de luminosite sortent reellement.
    static void strokeDensity(juce::Graphics& g,
                              const float* xs,
                              const float* ys,
                              int n,
                              juce::Colour base,
                              float depthAlpha,
                              float thickness)
    {
        if (n < 2)
            return;

        constexpr int kLevels = 6;           // paliers, comme un DAC a 6 bits
        constexpr float kMinLit = 0.28f;     // le plus terne, jamais eteint

        juce::Path buckets[(size_t) kLevels];
        bool used[(size_t) kLevels] = {};
        int last = -1;

        for (int i = 1; i < n; ++i)
        {
            const float ddx = xs[i] - xs[i - 1];
            const float ddy = ys[i] - ys[i - 1];
            const float len = std::sqrt(ddx * ddx + ddy * ddy);

            // 1.0 = segment nul (trait plat : sejour maximal -> plus clair)
            float den = (len > 1.0e-4f) ? (std::fabs(ddx) / len) : 1.0f;
            if (den > 1.0f) den = 1.0f;
            if (den < 0.0f) den = 0.0f;

            int lvl = (int) (den * (float) kLevels);
            if (lvl >= kLevels) lvl = kLevels - 1;

            // Un nouveau palier ouvre un sous-trace au point precedent :
            // les bords de paliers voisins se touchent, sans raccourci.
            if (lvl != last)
            {
                buckets[(size_t) lvl].startNewSubPath(xs[i - 1], ys[i - 1]);
                last = lvl;
            }
            buckets[(size_t) lvl].lineTo(xs[i], ys[i]);
            used[(size_t) lvl] = true;
        }

        for (int lvl = 0; lvl < kLevels; ++lvl)
        {
            if (! used[(size_t) lvl])
                continue;
            const float t = ((float) lvl + 0.5f) / (float) kLevels;
            const float lit = kMinLit + (1.0f - kMinLit) * t;
            g.setColour(base.withAlpha(depthAlpha * lit));
            g.strokePath(buckets[(size_t) lvl],
                         juce::PathStrokeType(thickness * (0.70f + 0.55f * lit),
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
        }
    }

private:
    // Une trace = min/max par colonne sur les frameLen derniers echantillons.
    // Min/max conserve la forme des transitoires (rien n'est lissé).
    void buildTrace(int oldest)
    {
        for (int c = 0; c < Cols; ++c)
        {
            scrMinL[(size_t) c] = FLT_MAX;
            scrMaxL[(size_t) c] = -FLT_MAX;
            scrMinR[(size_t) c] = FLT_MAX;
            scrMaxR[(size_t) c] = -FLT_MAX;
        }

        int idx = ((oldest % RingSize) + RingSize) % RingSize;
        int col = 0;
        int boundary = frameLen / Cols;
        float peak = 0.0f;

        for (int s = 0; s < frameLen; ++s)
        {
            while (col < Cols - 1 && s >= boundary)
            {
                ++col;
                boundary = ((col + 1) * frameLen) / Cols;
            }

            const float vl = ringL[(size_t) idx];
            const float vr = ringR[(size_t) idx];

            if (vl < scrMinL[(size_t) col]) scrMinL[(size_t) col] = vl;
            if (vl > scrMaxL[(size_t) col]) scrMaxL[(size_t) col] = vl;
            if (vr < scrMinR[(size_t) col]) scrMinR[(size_t) col] = vr;
            if (vr > scrMaxR[(size_t) col]) scrMaxR[(size_t) col] = vr;

            const float al = std::fabs(vl);
            const float ar = std::fabs(vr);
            if (al > peak) peak = al;
            if (ar > peak) peak = ar;

            if (++idx >= RingSize) idx = 0;
        }

        lastTracePeak = peak;
    }

    void pushHistory()
    {
        head = (head - 1 + Rows) % Rows;
        const size_t o = (size_t) (head * Cols);
        for (int c = 0; c < Cols; ++c)
        {
            histMinL[o + (size_t) c] = scrMinL[(size_t) c];
            histMaxL[o + (size_t) c] = scrMaxL[(size_t) c];
            histMinR[o + (size_t) c] = scrMinR[(size_t) c];
            histMaxR[o + (size_t) c] = scrMaxR[(size_t) c];
        }
    }

    // Auto-echelle : attaque immediate (jamais d'ecretage a l'arrivee d'un
    // transitoire) + retour lent (pas de scintillement de l'echelle).
    // On ne descend JAMAIS sous le plus grand pic encore present dans
    // l'historique : sans cela, quand le signal retombe, l'echelle se
    // resserre et les couches anciennes (plus lumineuses) sont rognees.
    void updateScale()
    {
        float p = lastTracePeak;
        for (int k = 0; k < Rows; ++k)
        {
            const size_t o = (size_t) (k * Cols);
            for (int c = 0; c < Cols; ++c)
            {
                const float v[4] = { std::fabs(histMaxL[o + (size_t) c]),
                                     std::fabs(histMinL[o + (size_t) c]),
                                     std::fabs(histMaxR[o + (size_t) c]),
                                     std::fabs(histMinR[o + (size_t) c]) };
                for (float f : v)
                    if (f > p) p = f;
            }
        }

        if (p > peakHold)
            peakHold = p;                                  // attaque immediate
        else
            peakHold = p + (peakHold - p) * 0.96f;         // retour lent
        if (peakHold < kFloor)
            peakHold = kFloor;
    }

    // ---- Donnees UI (jamais touchees par le thread audio) ----
    std::vector<float> ringL, ringR; // tampon circulaire d'entree
    std::vector<float> histMinL, histMaxL, histMinR, histMaxR; // Rows * Cols
    std::vector<float> scrMinL, scrMaxL, scrMinR, scrMaxR;     // Cols

    int ringWrite = 0;   // position d'ecriture dans le ring
    int sinceFrame = 0;  // echantillons recus depuis la derniere trace
    int head = 0;        // slot de la trace la plus recente
    int frameLen = DefaultFrameLen;
    double sampleRate = 0.0;

    long long seen = 0;          // total d'echantillons recus (verification)
    float peakHold = kFloor;     // echelle verticale courante
    float lastTracePeak = 0.0f;  // crete de la derniere trace construite

    juce::Colour colDeep { 0xff1c6f8c };
    juce::Colour colMid { 0xff7df9ff };
    juce::Colour colHot { 0xffd9fbff };
};
