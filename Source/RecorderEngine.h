#pragma once
// Moteur d'enregistrement PRISME : capture la sortie finale, quantifiee au
// debut de mesure quand le DAW fournit le transport, avec secours immediat.
// Thread audio : writeSamples() seul (pas d'allocation, pas de verrou, pas de
// fichier, pas d'appel GUI). Thread message : arm(), forceStart(),
// requestStopAndSave(), refresh de l'UI par lecture d'atomiques.
// L'UI peut etre masquee (BACK TO PRISME) : le moteur vit dans le processeur.
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <vector>
#include <thread>
#include <cmath>

// Metadonnees d'une prise finalisee (thread message uniquement, sauf
// lastTake rempli par le thread de fond AVANT freshTake/state -> lecture
// UI apres observation de freshTake : ordre garanti par seq_cst).
struct PrismeTakeInfo
{
    juce::File file;
    juce::String name;
    int samples = 0;
    double sr = 44100.0;
    double bpm = 0.0;      // 0 = inconnu (pas de transport a l'armement)
    int sigNum = 4, sigDen = 4;
    bool fallback = true;
};

class RecorderEngine
{
public:
    enum State { Idle = 0, WaitingBar = 1, Recording = 2, Saving = 3 };

    static const char* stateName(int s)
    {
        switch (s)
        {
            case Idle:       return "IDLE";
            case WaitingBar: return "WAITING FOR BAR";
            case Recording:  return "RECORDING";
            case Saving:     return "SAVING";
            default:         return "?";
        }
    }

    std::atomic<int> state { Idle };
    std::atomic<int> writeIndex { 0 };      // echantillons valides (audio -> UI)
    std::atomic<int> savedSamples { 0 };    // derniers echantillons finalises
    std::atomic<float> peakL { 0.0f };      // crete du dernier bloc (audio -> UI)
    std::atomic<float> peakR { 0.0f };
    std::atomic<double> sampleRateHz { 44100.0 };
    std::atomic<bool> fallbackUsed { false }; // true = demarrage immediat (pas de sync)
    std::atomic<bool> freshTake { false };    // true = une prise vient d'etre sauvee
    std::atomic<bool> autoStopped { false };  // true = capacite max atteinte
    PrismeTakeInfo lastTake; // rempli par le thread de fond (voir ci-dessus)

    RecorderEngine()
    {
        destFolder = juce::File::getSpecialLocation(
            juce::File::userDocumentsDirectory).getChildFile("Prisme Takes");
    }

    // Fermeture du plugin : attend la fin de la sauvegarde WAV de fond
    // (sinon le thread ecrit dans un moteur deja detruit -> crash a la fermeture).
    std::atomic<int> activeJobs { 0 };
    ~RecorderEngine()
    {
        for (int i = 0; i < 1000 && activeJobs.load() > 0; ++i)
            juce::Thread::sleep(5);
    }

    // --- Thread message uniquement ---
    void setSampleRate(double sr)
    {
        if (sr > 0)
            sampleRateHz.store(sr);
    }

    void setMaxSeconds(int s)
    {
        maxSeconds = juce::jlimit(10, 600, s);
    }

    // Arme : pre-alloue (120 s stereo par defaut), puis attend la mesure.
    // Ne touche jamais au DSP existant. Memorise le transport du moment
    // (pour les metadonnees de la prise : tempo/mesures).
    void arm() { armWithTransport(0.0, 4, 4, false); }

    void armWithTransport(double bpm, int sigNum, int sigDen, bool hasTransport)
    {
        armBpm = (hasTransport && bpm > 20.0 && bpm < 300.0) ? bpm : 0.0;
        armSigNum = juce::jlimit(1, 16, sigNum);
        armSigDen = juce::jlimit(1, 32, sigDen);
        armHasTransport = hasTransport && armBpm > 0.0;
        double sr = sampleRateHz.load();
        if (sr <= 0) sr = 44100.0;
        const int cap = (int) (sr * (double) maxSeconds);
        bufL.assign((size_t) cap, 0.0f);
        bufR.assign((size_t) cap, 0.0f);
        capacity = cap;
        writeIndex.store(0);
        savedSamples.store(0);
        peakL.store(0.0f);
        peakR.store(0.0f);
        fallbackUsed.store(false);
        freshTake.store(false);
        autoStopped.store(false);
        stopRequested.store(false);
        state.store(WaitingBar);
    }

    // Secours : demarrage immediat si pas de transport (2e appui sur REC
    // ou transport absent). Thread message ou audio : atomique seul.
    void forceStart()
    {
        int s = state.load();
        if (s == WaitingBar)
        {
            fallbackUsed.store(true);
            startSkip = 0;
            writeIndex.store(0);
            state.store(Recording);
        }
    }

    // Thread audio : appele quand la mesure est detectee par le processeur.
    // Mesure detectee DANS le bloc courant : on demarre a l'echantillon exact
    // (skipSamples = position du temps 1 dans le bloc). Thread audio.
    void notifyBarSyncAt(int skipSamples)
    {
        if (state.load() == WaitingBar)
        {
            startSkip = skipSamples < 0 ? 0 : skipSamples;
            fallbackUsed.store(false);
            writeIndex.store(0);
            state.store(Recording);
        }
    }

    void notifyBarSync()
    {
        if (state.load() == WaitingBar)
        {
            startSkip = 0;
            fallbackUsed.store(false);
            writeIndex.store(0);
            state.store(Recording);
        }
    }

    void cancelArm()
    {
        int s = state.load();
        if (s == WaitingBar)
            state.store(Idle);
    }

    // Thread message (bouton STOP) : fige la capture au bloc suivant et
    // finalise le WAV sur un thread de fond. Retourne false si rien a sauver.
    bool requestStopAndSave()
    {
        int s = state.load();
        if (s != Recording && s != WaitingBar)
            return false;
        if (s == WaitingBar)
        {
            // Jamais demarre : aucun echantillon, pas de fichier.
            state.store(Idle);
            return false;
        }
        stopRequested.store(true);
        state.store(Saving);
        const int n = writeIndex.load();
        const double sr = sampleRateHz.load();
        const int cap = capacity;
        // Copie des handles pour le thread de fond (buffers stables : l'audio
        // n'ecrit plus car state != Recording).
        juce::File folder = destFolder;
        juce::String fname = buildTakeName();
        lastFileName = fname;
        pendingSaveFile = folder.getChildFile(fname);
        // Le dossier est cree sur le thread de fond (pas de blocage audio).
        activeJobs.fetch_add(1);
        juce::Thread::launch([this, n, sr, cap]
        {
            saveWavBackground(n, sr, cap);
            activeJobs.fetch_sub(1); // dernier acces a this
        });
        return true;
    }

    // Dossier de destination (thread message uniquement ; jamais lu par l'audio).
    void setDestFolder(const juce::File& f) { destFolder = f; }
    juce::File getDestFolder() const { return destFolder; }
    juce::String getLastFileName() const { return lastFileName; }
    juce::File getLastFile() const { return pendingSaveFile; }

    // --- Thread audio uniquement ---
    // Retourne true si l'enregistrement continue apres ce bloc.
    bool writeSamples(const float* l, const float* r, int n)
    {
        if (state.load() != Recording)
            return false;
        if (stopRequested.load())
            return false;
        if (startSkip > 0) // depart au temps 1 exact : saute le debut du bloc
        {
            const int s = startSkip < n ? startSkip : n;
            startSkip -= s;
            if (s >= n)
                return true;
            l += s;
            if (r != nullptr) r += s;
            n -= s;
        }
        int idx = writeIndex.load();
        if (idx >= capacity)
        {
            autoStopped.store(true);
            stopRequested.store(true);
            return false;
        }
        int room = capacity - idx;
        int todo = n < room ? n : room;
        float pkL = 0.0f, pkR = 0.0f;
        for (int i = 0; i < todo; ++i)
        {
            float a = l[i], b = (r != nullptr) ? r[i] : a;
            bufL[(size_t) (idx + i)] = a;
            bufR[(size_t) (idx + i)] = b;
            float aa = std::fabs(a), bb = std::fabs(b);
            if (aa > pkL) pkL = aa;
            if (bb > pkR) pkR = bb;
        }
        idx += todo;
        writeIndex.store(idx);
        peakL.store(pkL);
        peakR.store(pkR);
        if (todo < n)
        {
            // Capacite atteinte en plein bloc : fin propre, l'UI finalisera.
            autoStopped.store(true);
            stopRequested.store(true);
            return false;
        }
        return true;
    }

    // Lecture UI (thread message) : acces direct aux buffers pour la forme
    // d'onde. L'audio n'ecrit que sous writeIndex ; lire en dessous est sur
    // pour l'affichage (valeurs par defaut 0 au-dela).
    const float* dataL() const { return bufL.data(); }
    const float* dataR() const { return bufR.data(); }
    int getCapacity() const { return capacity; }

private:
    int startSkip = 0; // thread audio seul : echantillons a sauter au demarrage
    int maxSeconds = 120;
    int capacity = 0;
    std::vector<float> bufL, bufR;
    std::atomic<bool> stopRequested { false };
    juce::File destFolder;
    juce::String lastFileName;
    juce::File pendingSaveFile;
    // Transport memorise a l'armement (thread message -> thread de fond).
    double armBpm = 0.0;
    int armSigNum = 4, armSigDen = 4;
    bool armHasTransport = false;

    juce::String buildTakeName() const
    {
        juce::Time t = juce::Time::getCurrentTime();
        return "Prisme_Take_" + t.formatted("%Y-%m-%d_%H-%M-%S") + ".wav";
    }

    // Thread de fond uniquement : ecriture WAV, jamais sur le thread audio.
    void saveWavBackground(int n, double sr, int /*cap*/)
    {
        n = juce::jlimit(0, (int) bufL.size(), n);
        juce::File out = pendingSaveFile;
        out.getParentDirectory().createDirectory();
        bool ok = false;
        if (n > 0 && sr > 0)
        {
            juce::WavAudioFormat fmt;
            if (auto* fos = out.createOutputStream().release())
            {
                std::unique_ptr<juce::FileOutputStream> fosPtr(fos);
                fosPtr->setPosition(0);
                fosPtr->truncate();
                if (auto* w = fmt.createWriterFor(fosPtr.get(), sr, 2, 16, {}, 0))
                {
                    // Le writer prend possession du stream en cas de succes.
                    fosPtr.release();
                    juce::AudioBuffer<float> ab(2, n);
                    ab.copyFrom(0, 0, bufL.data(), n);
                    ab.copyFrom(1, 0, bufR.data(), n);
                    ok = w->writeFromAudioSampleBuffer(ab, 0, n);
                    delete w;
                }
            }
        }
        savedSamples.store(ok ? n : 0);
        freshTake.store(ok);
        if (ok)
        {
            lastTake.file = out;
            lastTake.name = out.getFileName();
            lastTake.samples = n;
            lastTake.sr = sr;
            lastTake.bpm = armBpm;
            lastTake.sigNum = armSigNum;
            lastTake.sigDen = armSigDen;
            lastTake.fallback = fallbackUsed.load() || ! armHasTransport;
        }
        stopRequested.store(false);
        state.store(Idle);
    }
};

// Pre-ecoute d'une prise (LOOP STACK) : lecture additive post-DSP.
// Thread message : loadFile()/stop(). Thread audio : renderAdd() seul
// (pas d'allocation, pas de verrou, pas de fichier). Ne touche jamais
// aux parametres du synthétiseur (gain propre, fondu anti-clic).
class TakePreview
{
public:
    std::atomic<bool> playing { false };

    // Charge un WAV (thread message). Reechantillonne si besoin vers sr.
    // Plafonne a 60 s. Retourne false si illisible.
    bool loadFile(const juce::File& f, double sr)
    {
        stop();
        dataL.clear();
        dataR.clear();
        length = 0;
        if (sr <= 0) sr = 44100.0;
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatReader> r(fmt.createReaderFor(
            f.createInputStream().release(), true));
        if (r == nullptr || r->lengthInSamples <= 0)
            return false;
        juce::int64 want = std::min<juce::int64>(r->lengthInSamples,
                                                (juce::int64) (sr * 60.0));
        juce::AudioBuffer<float> ab((int) std::min<juce::int64>(r->numChannels, 2),
                                    (int) want);
        if (! r->read(&ab, 0, (int) want, 0, true, true))
            return false;
        double ratio = r->sampleRate > 0 ? sr / r->sampleRate : 1.0;
        int outLen = (int) ((double) want * ratio);
        dataL.assign((size_t) outLen, 0.0f);
        dataR.assign((size_t) outLen, 0.0f);
        const float* sL = ab.getReadPointer(0);
        const float* sR = ab.getNumChannels() > 1 ? ab.getReadPointer(1) : sL;
        for (int i = 0; i < outLen; ++i)
        {
            double pos = (double) i / ratio;
            int i0 = (int) pos;
            float fr = (float) (pos - i0);
            int i1 = std::min(i0 + 1, (int) want - 1);
            i0 = std::clamp(i0, 0, (int) want - 1);
            dataL[(size_t) i] = sL[i0] + (sL[i1] - sL[i0]) * fr;
            dataR[(size_t) i] = sR[i0] + (sR[i1] - sR[i0]) * fr;
        }
        length = outLen;
        return outLen > 0;
    }

    void start()
    {
        if (length > 0)
        {
            playIndex.store(0);
            playing.store(true);
        }
    }

    void stop() { playing.store(false); }
    bool isPlaying() const { return playing.load(); }

    // Thread audio : ajoute la prise a la sortie (post-DSP, pre-gains UI).
    void renderAdd(float* L, float* R, int n)
    {
        if (! playing.load())
            return;
        int idx = playIndex.load();
        const int fade = 256;
        for (int i = 0; i < n; ++i)
        {
            if (idx >= length)
            {
                playing.store(false);
                break;
            }
            float g = 0.8f;
            if (idx < fade) g *= (float) idx / (float) fade;
            if (length - idx < fade) g *= (float) (length - idx) / (float) fade;
            L[i] += dataL[(size_t) idx] * g;
            if (R != nullptr) R[i] += dataR[(size_t) idx] * g;
            ++idx;
        }
        playIndex.store(idx);
        if (idx >= length)
            playing.store(false);
    }

private:
    std::vector<float> dataL, dataR;
    int length = 0;
    std::atomic<int> playIndex { 0 };
};
