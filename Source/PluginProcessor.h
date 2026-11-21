#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <atomic>
#include "ResonatorBank.h"
#include "EQ4Band.h"
#include "BlurReverb.h"
#include "ArpFX.h"
#include "Crystallize.h"
#include "OutputLimiter.h"
#include "Scales.h"
#include "KeyDetector.h"
#include "RecorderEngine.h"
#include "VariationLab.h"

class PrismeProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    PrismeProcessor();
    ~PrismeProcessor() override;

    const juce::String getName() const override { return "Prisme"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    juce::UndoManager& getUndoManager() { return undoManager; } // UNDO / REDO des reglages
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Prise d'analyse pour le spectrogramme (thread audio -> UI, lock-free).
    // Le DSP ne lit jamais ces membres : aucun effet sur le son.
    // REGLE : taille fixee a la construction, jamais retouchee (ni en prepare).
    static constexpr int spectrumFifoSize = 8192;
    juce::AbstractFifo spectrumFifo { spectrumFifoSize };
    std::vector<float> spectrumBuf = std::vector<float>((size_t) spectrumFifoSize, 0.0f);

    // Prise pour l'oscilloscope (thread audio -> UI, lock-free).
    // MEME regle : taille fixee a la construction, jamais retouchee.
    // UN item = UNE paire [L, R] (2 floats) : les deux canaux restent
    // toujours alignes, quelle que soit la taille du buffer audio.
    // Seul consommateur : l'editeur. Le DSP n'ecrit qu'ici.
    static constexpr int scopeFifoSize = 16384;
    juce::AbstractFifo scopeFifo { scopeFifoSize };
    std::vector<float> scopeBuf = std::vector<float>((size_t) (2 * scopeFifoSize), 0.0f);

    // Enregistreur (prise WAV). Vit dans le processeur : independant de la vue.
    // L'UI peut etre masquee (BACK TO PRISME) : la capture continue.
    RecorderEngine recorder;
    // Pre-ecoute du LOOP STACK : additive, jamais dans les parametres.
    TakePreview preview;

    // Historique des prises (thread message uniquement ; survit a la
    // fermeture/reouverture de l'editeur). Fichiers jamais supprimes
    // automatiquement : seule l'entree peut etre retiree (fichier sur
    // confirmation explicite).
    struct TakeEntry
    {
        int number = 0;                 // 01, 02... (jamais reutilise)
        juce::File file;
        juce::String name;
        double durationSec = 0.0;
        juce::String bpmText;           // "120.0" ou "--"
        juce::String barsText;          // "~4.0" ou "--"
        std::vector<float> peaks;       // miniature (120 cretes 0..1)
        bool fileMissing = false;
        // Variation RANDOMIZE (0 = prise originale).
        bool isVariation = false;
        int parentNumber = -1;          // numero de la source
        uint32_t seed = 0;
        int intensityPct = 0;
        int varBars = 0;
        double tempoUsed = 0.0;         // 0 = 120 par defaut (signale)
    };
    std::vector<TakeEntry> takeList;
    int selectedTake = -1;              // index dans takeList, -1 = aucune
    int nextTakeNumber = 1;
    static constexpr int maxTakes = 32;
    static constexpr int takePeaks = 120;

    // Cache transport hote (audio -> UI, atomiques seuls). Jamais de verrou.
    std::atomic<double> hostBpmCache { 120.0 };
    std::atomic<double> hostPpqCache { -1.0 };
    std::atomic<int> hostSigNum { 4 };
    std::atomic<int> hostSigDen { 4 };
    std::atomic<bool> hostPlaying { false };
    std::atomic<bool> hostHasTransport { false };

    // Helpers UI (thread message) : ne touchent ni au DSP ni aux presets.
    void recArmFromUI();
    bool recStopFromUI();
    void recForceStartFromUI();
    void recCancelFromUI();
    // LOOP STACK (thread message) : ajout auto apres finalisation WAV.
    // Retourne l'index ajoute, ou -1 si rien a ajouter.
    int takeConsumeFresh();
    void takeRemoveAt(int index, bool deleteFile);
    void takePreviewAt(int index); // bascule lecture/stop, additive douce
    void takeStopPreview();
    // Index de la prise REELLEMENT en pre-ecoute (-1 = aucune), thread message.
    // Source unique pour l'UI : l'etat "en lecture" ne se deduit plus de
    // selectedTake, qui est une SELECTION et non une LECTURE.
    std::atomic<int> previewTakeIndex { -1 };
    // RANDOMIZE WAVE : generation hors ligne sur thread de travail.
    // Etats : 0 READY, 1 GENERATING, 2 RENDERING, 3 COMPLETE, 4 ERROR.
    std::atomic<int> varJobState { 0 };
    std::atomic<double> varJobProgress { 0.0 };
    // LED de liaison RANDOMIZE / REROLL (section SAMPLES) : allumee, chaque
    // variation met un effet vedette en avant et l'effet avance d'un clic a
    // l'autre. Memoire seule : pas dans les presets, eteinte au chargement.
    std::atomic<bool> variationLinked { false };
    // useOneShotRotation : false = jamais d'effet vedette (VAR LAB). Le panneau
    // SAMPLES utilise le defaut (true) et suit alors la LED de liaison.
    void variationStart(int sourceIndex, int bars, int intensityPct, uint32_t seed,
                        bool useOneShotRotation = true);
    // Consomme une variation terminee (l'insere sous sa source).
    // Retourne l'index insere, ou -1.
    int variationConsumeDone();
    juce::String variationStatusText() const; // thread message

private:
    // Prochain effet vedette du mode "one shot" (0..7) : avance d'un cran a
    // chaque variation lancee depuis le panneau SAMPLES. Thread message.
    int linkedNextKind = 0;
    juce::UndoManager undoManager; // declare AVANT apvts (ordre d'initialisation)
    juce::AudioProcessorValueTreeState apvts;

    PrismResonatorBank resonator;       // voie dry (decay normal)
    PrismResonatorBank freezeBank;      // voie wet gelee (T60 ~30 s)
    ArpFX::Engine arpFXA, arpFXB, arpFXC; // 3 inserts en serie sur la session arpegee
    Crystallize cryo;                   // embellisseur 1 potard, parallele a la chaine
    bool freezeWasActive = false;
    juce::LinearSmoothedValue<float> smoothFreeze { 0.0f };
    juce::LinearSmoothedValue<float> smoothFx1 { 0.35f };
    juce::LinearSmoothedValue<float> smoothFx2 { 0.35f };
    juce::LinearSmoothedValue<float> smoothFxB1 { 0.35f };
    juce::LinearSmoothedValue<float> smoothFxB2 { 0.35f };
    juce::LinearSmoothedValue<float> smoothFxC1 { 0.35f };
    juce::LinearSmoothedValue<float> smoothFxC2 { 0.35f };
    juce::LinearSmoothedValue<float> smoothFxMix { 1.0f };
    juce::LinearSmoothedValue<float> smoothCryo { 0.0f };
    PrismeEQ4Band eqL, eqR; // etage EQ 4 bandes en sortie (post-mix)
    BlurReverb blur;        // reverb diffuse : resonances nettes -> nappe
    PrismeBandLimiter bandLimiter;  // zone de frequences excitant l'effet (voie L)
    PrismeBandLimiter bandLimiterR; // idem, voie R (dry/wet par bande)
    // Fin de chaine : limiteur de cretes + gain match (avant le potard Sortie)
    // puis plafond absolu (apres le potard). Aucune latence ajoutee.
    PrismeOutputLimiter outLimiter;
    juce::LinearSmoothedValue<float> smoothBlur { 0.3f };
    juce::LinearSmoothedValue<float> smoothBlurSize { 0.65f };
    juce::LinearSmoothedValue<float> smoothMirColor { 0.6f };
    juce::LinearSmoothedValue<float> smoothMirWidth { 1.0f };

    // Smoothing gains pour eviter les clics
    juce::LinearSmoothedValue<float> smoothMix { 0.5f };
    juce::LinearSmoothedValue<float> smoothInGain { 1.0f };
    juce::LinearSmoothedValue<float> smoothOutGain { 1.0f };
    juce::LinearSmoothedValue<float> smoothReso { 1.0f };

    int lastRoot = -1, lastScale = -1, lastOct = -1;
    int quietSamples = 0; // thread audio : duree (echantillons) d'entree ET sortie silencieuses
    std::vector<int> tuningScratch = std::vector<int>((size_t) PrismResonatorBank::maxVoices, 0); // capacite fixe : pas d'allocation audio

    // --- AUTO KEY (analyse sur le thread message, jamais dans processBlock) ---
    // L'audio thread ne fait qu'ecrire l'entree mono dans ce FIFO, et seulement si AUTO est actif.
    static constexpr int keyFifoSize = 32768;
    juce::AbstractFifo keyFifo { keyFifoSize };
    std::vector<float> keyBuf = std::vector<float>((size_t) keyFifoSize, 0.0f);
    PrismeKeyDetector keyDet;
    bool keyActive = false;
    int keyCand = -1, keyStable = 0, keyApplied = -1;
    void timerCallback() override;

    void updateTuningIfNeeded();

    // --- Arpegiateur + Attaque ---
    int arpStep = 0;          // pas courant (0..K-1)
    int arpDir = 1;           // direction pour le motif Haut-Bas
    long long arpTick = 0;    // compteur de pas (parite pour le swing)
    double arpSamplesToNext = 0.0; // echantillons avant le prochain pas
    int lastArpK = -1, lastArpPattern = -1;
    bool lastArpOn = false;
    float peakEnv = 0.0f;     // enveloppe crete entree (transients designer)
    float slowEnv = 0.0f;     // enveloppe lente (temps d'attaque)
    float arpTargets[12] = { 1,1,1,1,1,1,1,1,1,1,1,1 };

    double getHostBpm() const;
    void advanceArpStep(int K, int pattern);
    void updateArp(int numSamples, double sampleRate);
    void updateRecorderTransport(int numSamples); // audio : cache hote + sync mesure
    double lastBarPhase = -1.0;      // audio uniquement : phase mesure precedente
    bool barPhaseInit = false;       // audio uniquement
    std::atomic<bool> resetBarPhase { false }; // message -> audio : re-arme sync

    // Job RANDOMIZE (ecrit par le thread de travail AVANT COMPLETE/ERROR ;
    // lu par l'UI apres. Message protege : jamais touche par l'audio).
    struct PendingVariation
    {
        TakeEntry entry;
        juce::String error;
        juce::String statusDetail;
    };
    PendingVariation pendingVariation;
    // Fermeture propre : le thread RANDOMIZE s'arrete et est attendu (destructeur).
    std::atomic<bool> shuttingDown { false };
    std::atomic<int> varJobsActive { 0 };
    mutable juce::CriticalSection varMessageLock;
    juce::String varMessage; // detail d'etat/erreur pour l'UI

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PrismeProcessor)
};
