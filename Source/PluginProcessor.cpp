#include "PluginProcessor.h"
#include "PluginEditor.h"

PrismeProcessor::PrismeProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, &undoManager, "PrismeParams", createParameterLayout())
{
    startTimer(500); // AUTO KEY : veille lente tant que AUTO est coupe
}

PrismeProcessor::~PrismeProcessor()
{
    stopTimer();
    // Stoppe puis attend le thread RANDOMIZE avant de detruire les membres.
    shuttingDown.store(true);
    for (int i = 0; i < 1000 && varJobsActive.load() > 0; ++i)
        juce::Thread::sleep(5);
}

juce::AudioProcessorEditor* PrismeProcessor::createEditor()
{
    return new PrismeEditor(*this);
}

juce::AudioProcessorValueTreeState::ParameterLayout PrismeProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto rootNames = PrismeScales::getRootNames();
    juce::StringArray rootArr;
    for (auto& n : rootNames) rootArr.add(n);

    auto scaleNames = PrismeScales::getScaleNames();
    juce::StringArray scaleArr;
    for (auto& n : scaleNames) scaleArr.add(n);

    juce::StringArray octArr;
    for (auto& n : PrismeScales::getOctaveNames()) octArr.add(n);

    params.push_back(std::make_unique<juce::AudioParameterChoice>("root", "Tonique", rootArr, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("scale", "Gamme / Accord", scaleArr, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("octave", "Octave", octArr, 1));
    params.push_back(std::make_unique<juce::AudioParameterBool>("autokey", "Auto Key", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "decay", "Resonance (Decay)",
        juce::NormalisableRange<float>(0.2f, 8.0f, 0.01f, 0.55f), 2.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "attack", "Attaque",
        juce::NormalisableRange<float>(0.005f, 2.0f, 0.001f, 0.5f), 0.02f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "brightness", "Couleur",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "iridescence", "Iridescence",
        juce::NormalisableRange<float>(0.0f, 1.5f, 0.001f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "shimmer", "Vivant (Shimmer)",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "spread", "Largeur",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.8f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mix", "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ingain", "Entree dB",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "outgain", "Sortie dB",
        juce::NormalisableRange<float>(-24.0f, 0.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "freezemix", "Givre (dry/wet)",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    juce::StringArray arpPatArr;
    for (auto& n : PrismeScales::getArpPatternNames()) arpPatArr.add(n);
    juce::StringArray arpDivArr;
    for (auto& n : PrismeScales::getArpDivNames()) arpDivArr.add(n);

    {
        juce::StringArray routeArr;
        routeArr.add("Arp");     // chaine FX sur l'arpege seul (comportement d'origine)
        routeArr.add("Accords"); // sur toute la resonance : accords + arpege
        routeArr.add("Tout");    // sur le wet complet, apres Mirage
        params.push_back(std::make_unique<juce::AudioParameterChoice>("arpfxroute", "Cible FX", routeArr, 0));
    }
    params.push_back(std::make_unique<juce::AudioParameterBool>("arpon", "Arp On", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("arppattern", "Motif Arp", arpPatArr, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>("arpsync", "Arp Sync Tempo", true));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("arpdiv", "Division Arp", arpDivArr, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "arprate", "Taux Arp (Hz)",
        juce::NormalisableRange<float>(0.5f, 16.0f, 0.01f, 0.5f), 4.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "arpswing", "Swing Arp",
        juce::NormalisableRange<float>(0.0f, 0.75f, 0.001f), 0.0f));

    juce::StringArray arpOctArr;
    arpOctArr.add("1");
    arpOctArr.add("2");
    arpOctArr.add("3");
    params.push_back(std::make_unique<juce::AudioParameterChoice>("arpoctaves", "Octaves Arp", arpOctArr, 0));

    juce::StringArray arpFxArr;
    arpFxArr.add("Off");
    arpFxArr.add("Delay");
    arpFxArr.add("Chorus");
    arpFxArr.add("Filter");
    arpFxArr.add("Crusher");
    arpFxArr.add("Reverse");
    params.push_back(std::make_unique<juce::AudioParameterChoice>("arpfxtype", "FX Arp", arpFxArr, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "arpfx1", "FX Arp 1",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "arpfx2", "FX Arp 2",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));
    juce::StringArray arpFxBArr;
    arpFxBArr.add("Off");
    arpFxBArr.add("Delay");
    arpFxBArr.add("Chorus");
    arpFxBArr.add("Filter");
    arpFxBArr.add("Crusher");
    arpFxBArr.add("Reverse");
    params.push_back(std::make_unique<juce::AudioParameterChoice>("arpfxtypeB", "FX Arp B", arpFxBArr, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "arpfxB1", "FX Arp B1",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "arpfxB2", "FX Arp B2",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));
    juce::StringArray arpFxCArr;
    arpFxCArr.add("Off");
    arpFxCArr.add("Delay");
    arpFxCArr.add("Chorus");
    arpFxCArr.add("Filter");
    arpFxCArr.add("Crusher");
    arpFxCArr.add("Reverse");
    params.push_back(std::make_unique<juce::AudioParameterChoice>("arpfxtypeC", "FX Arp C", arpFxCArr, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "arpfxC1", "FX Arp C1",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "arpfxC2", "FX Arp C2",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "arpfxmix", "Mix FX Arp",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "crystallize", "Crystallize",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    // Cible de CRYSTALLIZE, pilotee par les 3 LED (mutuellement exclusives).
    // L'ordre suit celui des LED de haut en bas : 1 "no arp" (tout sauf
    // l'arpege), 2 "arp" (arpege seul), 3 "all" (tout l'effet).
    // 0 "Aucune" = les 3 LED eteintes. Defaut 3 = "all". Memorise dans les presets.
    juce::StringArray cryoTgtArr;
    cryoTgtArr.add("Aucune");
    cryoTgtArr.add("Tout sauf arpege");
    cryoTgtArr.add("Arpege");
    cryoTgtArr.add("Tout");
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "cryotarget", "Cible Crystallize", cryoTgtArr, 3));

    juce::StringArray movDivArr;
    for (auto& n : PrismeScales::getArpDivNames()) movDivArr.add(n);

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "movrate", "Vitesse Mouvement (Hz)",
        juce::NormalisableRange<float>(0.05f, 20.0f, 0.01f, 0.45f), 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "movdepth", "Force Mouvement",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "movshape", "Forme Mouvement",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "movunison", "Mouvement Unisson",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("movsync", "Mouvement Sync Tempo", true));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("movdiv", "Division Mouvement", movDivArr, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "eqlow", "EQ Graves (dB)",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "eqlowmid", "EQ Bas-Mediums (dB)",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "eqhighmid", "EQ Haut-Mediums (dB)",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "eqhigh", "EQ Aigus (dB)",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "blurmix", "Mirage (dry/wet)",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "blursize", "Taille Mirage",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.65f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mirpredelay", "Mirage Pre-Delay (s)",
        juce::NormalisableRange<float>(0.0f, 0.25f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mircolor", "Mirage Couleur",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.6f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mirwidth", "Mirage Largeur",
        juce::NormalisableRange<float>(0.0f, 1.5f, 0.001f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mirdecay", "Mirage Decay (x)",
        juce::NormalisableRange<float>(0.2f, 3.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "bandlow", "Bande Bas (Hz)",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "bandhigh", "Bande Haut (Hz)",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.3f), 20000.0f));

    return { params.begin(), params.end() };
}

void PrismeProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    recorder.setSampleRate(sampleRate);
    preview.stop(); // changement de contexte audio : pas de preview stale
    resonator.setSampleRate(sampleRate);
    resonator.reset();
    freezeBank.setSampleRate(sampleRate);
    freezeBank.reset();
    freezeWasActive = false;
    arpFXA.setSampleRate(sampleRate);
    arpFXA.reset();
    arpFXB.setSampleRate(sampleRate);
    arpFXB.reset();
    arpFXC.setSampleRate(sampleRate);
    arpFXC.reset();
    cryo.setSampleRate(sampleRate);
    cryo.reset();
    blur.setSampleRate(sampleRate);
    blur.reset();
    quietSamples = 0;
    bandLimiter.reset();
    bandLimiterR.reset();
    bandLimiter.lastLo = bandLimiter.lastHi = -1.0f;
    eqL.reset();
    eqR.reset();
    for (int i = 0; i < 4; ++i) { eqL.lastDb[i] = 999.0f; eqR.lastDb[i] = 999.0f; }
    outLimiter.reset(sampleRate);
    lastRoot = lastScale = lastOct = -1;
    updateTuningIfNeeded();

    arpStep = 0;
    arpDir = 1;
    arpTick = 0;
    arpSamplesToNext = 0.0;
    lastArpK = lastArpPattern = -1;
    lastArpOn = false;
    peakEnv = slowEnv = 0.0f;
    for (int i = 0; i < 12; ++i) arpTargets[i] = 1.0f;
    resonator.resetEmphasis();
    freezeBank.resetEmphasis();

    smoothMix.reset(sampleRate, 0.05);
    smoothInGain.reset(sampleRate, 0.02);
    smoothOutGain.reset(sampleRate, 0.02);
    smoothReso.reset(sampleRate, 0.05);
    smoothBlur.reset(sampleRate, 0.05);
    smoothBlurSize.reset(sampleRate, 0.05);
    smoothMirColor.reset(sampleRate, 0.05);
    smoothMirWidth.reset(sampleRate, 0.05);
    smoothFreeze.reset(sampleRate, 0.08);
    smoothFx1.reset(sampleRate, 0.03);
    smoothFx2.reset(sampleRate, 0.03);
    smoothFxB1.reset(sampleRate, 0.03);
    smoothFxB2.reset(sampleRate, 0.03);
    smoothFxC1.reset(sampleRate, 0.03);
    smoothFxC2.reset(sampleRate, 0.03);
    smoothFxMix.reset(sampleRate, 0.05);
    smoothCryo.reset(sampleRate, 0.05);
}

bool PrismeProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void PrismeProcessor::updateTuningIfNeeded()
{
    auto* pRoot = apvts.getRawParameterValue("root");
    auto* pScale = apvts.getRawParameterValue("scale");
    auto* pOct = apvts.getRawParameterValue("octave");
    if (pRoot == nullptr || pScale == nullptr || pOct == nullptr) return;

    int r = (int) pRoot->load();
    int s = (int) pScale->load();
    int o = (int) pOct->load();
    if (r != lastRoot || s != lastScale || o != lastOct)
    {
        lastRoot = r; lastScale = s; lastOct = o;
        // Sans allocation (thread audio) : table de gammes statique + tampon pre-dimensionne
        PrismeScales::buildTuningInto(tuningScratch, r, s, o, PrismResonatorBank::maxVoices);
        resonator.setTuning(tuningScratch);
        freezeBank.setTuning(tuningScratch);
    }
}

void PrismeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    updateTuningIfNeeded();
    updateRecorderTransport(buffer.getNumSamples()); // cache hote + bascule WAITING -> RECORDING (sans toucher au DSP)

    float decay = apvts.getRawParameterValue("decay")->load();
    float attack = apvts.getRawParameterValue("attack")->load();
    float bright = apvts.getRawParameterValue("brightness")->load();
    float irid = apvts.getRawParameterValue("iridescence")->load();
    float shim = apvts.getRawParameterValue("shimmer")->load();
    float spread = apvts.getRawParameterValue("spread")->load();
    float mix = apvts.getRawParameterValue("mix")->load();
    float inDb = apvts.getRawParameterValue("ingain")->load();
    float outDb = apvts.getRawParameterValue("outgain")->load();
    float freezeMix = apvts.getRawParameterValue("freezemix")->load();

    resonator.setDecay((double) decay, false);
    resonator.setBrightness(bright);
    resonator.setShimmer(shim);
    resonator.setSpread(spread);
    // L'attaque pilote aussi le fondu des pas d'arpege : pincements -> houles
    double emphSmooth = (double) juce::jlimit(0.005f, 0.5f, attack * 0.15f + 0.004f);
    resonator.setEmphSmooth(emphSmooth);
    // Voie gelee : meme couleur, sustain quasi-infini
    freezeBank.setDecay(30.0, false);
    freezeBank.setBrightness(bright);
    freezeBank.setShimmer(shim);
    freezeBank.setSpread(spread);
    freezeBank.setEmphSmooth(emphSmooth);

    // Mouvement : vitesse libre (Hz) ou synchronisee au tempo hote
    {
        float movDepth = apvts.getRawParameterValue("movdepth")->load();
        float movShape = apvts.getRawParameterValue("movshape")->load();
        float movUnison = apvts.getRawParameterValue("movunison")->load();
        bool movSync = apvts.getRawParameterValue("movsync")->load() > 0.5f;
        float movHz;
        if (movSync)
        {
            int movDiv = (int) apvts.getRawParameterValue("movdiv")->load();
            double beat = 60.0 / getHostBpm();
            movHz = (float)(1.0 / (beat * PrismeScales::getArpDivMult(movDiv)));
        }
        else
        {
            movHz = apvts.getRawParameterValue("movrate")->load();
        }
        resonator.setMovement(movHz, movDepth, movShape, movUnison);
        freezeBank.setMovement(movHz, movDepth, movShape, movUnison);
    }

    const int numSamples = buffer.getNumSamples();
    updateArp(numSamples, getSampleRate());

    // Bande d'effet : ce qui excite les resonateurs (20-20k = transparent)
    float bandLo = apvts.getRawParameterValue("bandlow")->load();
    float bandHi = apvts.getRawParameterValue("bandhigh")->load();
    if (PrismeBandLimiter::needsUpdate(bandLimiter.lastLo, bandLimiter.lastHi, bandLo, bandHi))
    {
        double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
        bandLimiter.configure(sr, bandLo, bandHi);
        bandLimiterR.configure(sr, bandLo, bandHi);
    }
    bool bandActive = ! PrismeBandLimiter::isBypass(bandLo, bandHi);
    const bool autoKeyOn = apvts.getRawParameterValue("autokey")->load() > 0.5f;

    // Etage EQ 4 bandes (post-mix) : recalcule seulement si reglage change
    float eqDbLow = apvts.getRawParameterValue("eqlow")->load();
    float eqDbLowMid = apvts.getRawParameterValue("eqlowmid")->load();
    float eqDbHighMid = apvts.getRawParameterValue("eqhighmid")->load();
    float eqDbHigh = apvts.getRawParameterValue("eqhigh")->load();
    if (PrismeEQ4Band::needsUpdate(eqL.lastDb, eqDbLow, eqDbLowMid, eqDbHighMid, eqDbHigh))
    {
        double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
        eqL.configure(sr, eqDbLow, eqDbLowMid, eqDbHighMid, eqDbHigh);
        eqR.configure(sr, eqDbLow, eqDbLowMid, eqDbHighMid, eqDbHigh);
    }
    bool eqActive = std::fabs(eqDbLow) > 0.05f || std::fabs(eqDbLowMid) > 0.05f
                 || std::fabs(eqDbHighMid) > 0.05f || std::fabs(eqDbHigh) > 0.05f;

    // Coeffs du transient designer (attaque douce)
    bool useShaper = attack > 0.008f;
    double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
    float kFast = 1.0f - std::exp(-1.0f / (0.0002f * (float) sr));
    float kSlow = 1.0f - std::exp(-1.0f / (attack * (float) sr));
    float kRel  = 1.0f - std::exp(-1.0f / (0.25f * (float) sr));

    float inG = std::pow(10.0f, inDb / 20.0f);
    float outG = std::pow(10.0f, outDb / 20.0f);

    smoothMix.setTargetValue(mix);
    smoothInGain.setTargetValue(inG);
    smoothOutGain.setTargetValue(outG);
    smoothReso.setTargetValue(irid);
    smoothBlur.setTargetValue(apvts.getRawParameterValue("blurmix")->load());
    smoothBlurSize.setTargetValue(apvts.getRawParameterValue("blursize")->load());
    smoothMirColor.setTargetValue(apvts.getRawParameterValue("mircolor")->load());
    smoothMirWidth.setTargetValue(apvts.getRawParameterValue("mirwidth")->load());
    blur.setPreDelay(apvts.getRawParameterValue("mirpredelay")->load());
    blur.setDecayMul(apvts.getRawParameterValue("mirdecay")->load());
    smoothFreeze.setTargetValue(freezeMix);
    int fxType = (int) apvts.getRawParameterValue("arpfxtype")->load();
    int fxTypeB = (int) apvts.getRawParameterValue("arpfxtypeB")->load();
    int fxTypeC = (int) apvts.getRawParameterValue("arpfxtypeC")->load();
    const int fxRoute = (int) apvts.getRawParameterValue("arpfxroute")->load(); // 0 Arp, 1 Accords, 2 Tout
    const int cryoTarget = (int) apvts.getRawParameterValue("cryotarget")->load();
    smoothFx1.setTargetValue(apvts.getRawParameterValue("arpfx1")->load());
    smoothFx2.setTargetValue(apvts.getRawParameterValue("arpfx2")->load());
    smoothFxB1.setTargetValue(apvts.getRawParameterValue("arpfxB1")->load());
    smoothFxB2.setTargetValue(apvts.getRawParameterValue("arpfxB2")->load());
    smoothFxC1.setTargetValue(apvts.getRawParameterValue("arpfxC1")->load());
    smoothFxC2.setTargetValue(apvts.getRawParameterValue("arpfxC2")->load());
    smoothFxMix.setTargetValue(apvts.getRawParameterValue("arpfxmix")->load());
    smoothCryo.setTargetValue(apvts.getRawParameterValue("crystallize")->load());
    // LFO stereo interne au Crystallize : 1/4 = un cycle par temps hote.
    // Aucun parametre ajoute, aucun controle dans l'interface.
    cryo.setBeatHz(getHostBpm() / 60.0);
    // Front montant du Givre : capture propre (pas de residu stale)
    bool freezeActive = freezeMix > 0.001f;
    if (freezeActive && ! freezeWasActive)
        freezeBank.reset();
    // Front descendant : nettoie pour la prochaine capture
    if (! freezeActive && freezeWasActive)
        freezeBank.reset();
    freezeWasActive = freezeActive;

    const int numInCh = getTotalNumInputChannels();
    const int numOutCh = getTotalNumOutputChannels();
    const int bufCh = buffer.getNumChannels();

    // Securite : nettoie les canaux de sortie en trop
    for (int ch = numInCh; ch < bufCh; ++ch)
        buffer.clear(ch, 0, numSamples);

    const float* inL = buffer.getReadPointer(0);
    const float* inR = (bufCh > 1) ? buffer.getReadPointer(1) : buffer.getReadPointer(0);
    float* outL = buffer.getWritePointer(0);
    float* outR = (bufCh > 1) ? buffer.getWritePointer(1) : nullptr;

    // VEILLE : entree ET sortie silencieuses depuis plus d'1 s -> tout le DSP est
    // saute (zero CPU quand PRISME n'est pas utilise). Se reveille au 1er signal.
    float inPeak = 0.0f;
    for (int n = 0; n < numSamples; ++n)
    {
        inPeak = juce::jmax(inPeak, std::fabs(inL[n]));
        if (bufCh > 1) inPeak = juce::jmax(inPeak, std::fabs(inR[n]));
    }
    const bool inputQuiet = inPeak < 1.0e-6f;
    const bool idleSkip = inputQuiet && quietSamples >= (int) sr;
    if (idleSkip)
        buffer.clear();

    // Copie dry (avec inGain) car on ecrase le buffer
    // On traite en place avec buffers temporaires implicites : lire d'abord
    for (int n = 0; n < (idleSkip ? 0 : numSamples); ++n)
    {
        float ig = smoothInGain.getNextValue();
        float dryL = inL[n] * ig;
        float dryR = (bufCh > 1 ? inR[n] : inL[n]) * ig;

        float monoExc = (dryL + dryR) * 0.5f;

        // AUTO KEY : entree mono (avant effet) vers le FIFO d'analyse. Lock-free,
        // seulement si AUTO est actif ; si plein, l'echantillon est jete.
        if (autoKeyOn)
        {
            int ks1, kz1, ks2, kz2;
            keyFifo.prepareToWrite(1, ks1, kz1, ks2, kz2);
            if (kz1 > 0)
            {
                keyBuf[(size_t) ks1] = monoExc;
                keyFifo.finishedWrite(1);
            }
        }

        // Dry/wet PAR BANDE : la bande definit ou l'effet mord. Dans la bande,
        // le mix dry/wet s'applique ; hors bande, le dry passe intact (jamais
        // coupe). dryOut = dry - dryIn : la somme redonne toujours le dry exact.
        float dryInL = dryL, dryInR = dryR;
        float dryOutL = 0.0f, dryOutR = 0.0f;
        if (bandActive)
        {
            dryInL = bandLimiter.process(dryL);
            dryInR = bandLimiterR.process(dryR);
            dryOutL = dryL - dryInL;
            dryOutR = dryR - dryInR;
            monoExc = (dryInL + dryInR) * 0.5f;
        }

        // Transient designer : attaque courte = pincements percussifs intacts,
        // attaque longue = transitoires adoucies, houles lentes.
        float exc = monoExc;
        if (useShaper)
        {
            float ax = std::fabs(monoExc);
            peakEnv += (ax - peakEnv) * (ax > peakEnv ? kFast : kRel);
            slowEnv += (ax - slowEnv) * (ax > slowEnv ? kSlow : kRel);
            float g = slowEnv / (peakEnv + 1e-4f);
            exc = monoExc * juce::jlimit(0.0f, 1.0f, g);
        }

        float wetL = 0.0f, wetR = 0.0f;
        float arpL = 0.0f, arpR = 0.0f;
        resonator.processSample(exc, wetL, wetR, &arpL, &arpR);

        // Givre : crossfade voie normale -> voie gelee (rise/fall adoucis)
        {
            float fg = smoothFreeze.getNextValue();
            if (fg > 0.0005f)
            {
                float fL = 0.0f, fR = 0.0f, fArpL = 0.0f, fArpR = 0.0f;
                freezeBank.processSample(exc, fL, fR, &fArpL, &fArpR);
                // Maquillage x2 : la voie gelee accumule peu sur transitoires epars
                fL *= 2.0f; fR *= 2.0f; fArpL *= 2.0f; fArpR *= 2.0f;
                wetL = wetL * (1.0f - fg) + fL * fg;
                wetR = wetR * (1.0f - fg) + fR * fg;
                arpL = arpL * (1.0f - fg) + fArpL * fg;
                arpR = arpR * (1.0f - fg) + fArpR * fg;
            }
        }

        // Corps = "tout sauf l'arpege". Calcule AVANT que le FX remplace
        // l'arpege dans le mix : c'est l'une des 3 cibles de CRYSTALLIZE.
        const float bodyL = wetL - arpL;
        const float bodyR = wetR - arpR;

        // FX en chaine A -> B -> C puis dry/wet (Off = bypass). La CIBLE est choisie :
        //   0 Arp : arpege seul (d'origine) | 1 Accords : toute la resonance | 2 Tout : wet apres Mirage
        // Les lisseurs avancent d'un pas par echantillon quelle que soit la cible.
        const float sFx1 = smoothFx1.getNextValue(), sFx2 = smoothFx2.getNextValue();
        const float sFxB1 = smoothFxB1.getNextValue(), sFxB2 = smoothFxB2.getNextValue();
        const float sFxC1 = smoothFxC1.getNextValue(), sFxC2 = smoothFxC2.getNextValue();
        const float fm = smoothFxMix.getNextValue();
        auto runFxChain = [&](float inL, float inR, float& outFxL, float& outFxR)
        {
            float aL = 0.0f, aR = 0.0f, bL = 0.0f, bR = 0.0f;
            arpFXA.process(inL, inR, aL, aR, fxType, sFx1, sFx2);
            arpFXB.process(aL, aR, bL, bR, fxTypeB, sFxB1, sFxB2);
            arpFXC.process(bL, bR, outFxL, outFxR, fxTypeC, sFxC1, sFxC2);
        };
        if (fxRoute == 0)
        {
            float fx3L = 0.0f, fx3R = 0.0f;
            runFxChain(arpL, arpR, fx3L, fx3R);
            wetL = (wetL - arpL) + (arpL * (1.0f - fm) + fx3L * fm);
            wetR = (wetR - arpR) + (arpR * (1.0f - fm) + fx3R * fm);
        }
        else if (fxRoute == 1)
        {
            float fx3L = 0.0f, fx3R = 0.0f;
            runFxChain(wetL, wetR, fx3L, fx3R);
            wetL = wetL * (1.0f - fm) + fx3L * fm;
            wetR = wetR * (1.0f - fm) + fx3R * fm;
        }

        // CRYSTALLIZE en parallele de la chaine. La cible est choisie par les
        // 3 LED : 0 Aucune (rien), 1 no arp (tout sauf l'arpege), 2 arp, 3 all.
        // A 0 % le delta est nul : niveau et son inchanges.
        {
            const float cm = smoothCryo.getNextValue(); // avance toujours : pas d'a-coup
            if (cryoTarget != 0)
            {
                float cL = 0.0f, cR = 0.0f;
                const float tgtL = (cryoTarget == 3) ? wetL : (cryoTarget == 2) ? arpL : bodyL;
                const float tgtR = (cryoTarget == 3) ? wetR : (cryoTarget == 2) ? arpR : bodyR;
                cryo.process(tgtL, tgtR, cL, cR, cm);
                wetL += (cL - tgtL);
                wetR += (cR - tgtR);
            }
        }

        float m = smoothMix.getNextValue();
        float rg = smoothReso.getNextValue();
        float og = smoothOutGain.getNextValue();

        wetL *= rg;
        wetR *= rg;

        // Blur : reverb diffuse sur les resonances (net -> nappe)
        {
            float bm = smoothBlur.getNextValue();
            blur.setSize(smoothBlurSize.getNextValue());
            blur.setColor(smoothMirColor.getNextValue());
            blur.setWidth(smoothMirWidth.getNextValue());
            float vL = 0.0f, vR = 0.0f;
            blur.process(wetL, wetR, vL, vR);
            wetL = wetL * (1.0f - bm) + vL * bm;
            wetR = wetR * (1.0f - bm) + vR * bm;
        }

        // Cible "Tout" : la chaine FX traite le wet complet, apres Mirage
        if (fxRoute == 2)
        {
            float fx3L = 0.0f, fx3R = 0.0f;
            runFxChain(wetL, wetR, fx3L, fx3R);
            wetL = wetL * (1.0f - fm) + fx3L * fm;
            wetR = wetR * (1.0f - fm) + fx3R * fm;
        }

        // Soft-clip leger sur le wet pour garder du "luxe" sans agressivite
        wetL = std::tanh(wetL * 0.9f);
        wetR = std::tanh(wetR * 0.9f);

        float yL = dryOutL + dryInL * (1.0f - m) + wetL * m;
        float yR = dryOutR + dryInR * (1.0f - m) + wetR * m;

        if (eqActive)
        {
            yL = eqL.process(yL);
            yR = eqR.process(yR);
        }

        // Fin de chaine : le potard Sortie (plage -24..0 dB : il ne peut que
        // REDUIRE) est precede du limiteur et suivi du plafond. Le limiteur tient
        // le signal interne a -1.5 dBFS ; comme le potard ne remonte jamais
        // au-dessus de 0 dB, la sortie ne peut plus ecraser. Le plafond ne sert
        // donc plus que de filet pour les transitoires plus rapides que l'attaque.

        // 1) Limiteur de cretes stereo-lie. Sous le seuil : gain exactement 1,
        //    le signal est inchange. Remplace l'ancien tanh final, qui colorait
        //    deja les sons faibles (pas de seuil ni d'attaque/relachement).
        outLimiter.processLimiter(yL, yR);

        // 2) Gain match : le niveau interne suit le niveau ENTRE dans le plugin,
        //    pour que l'effet ne change pas le niveau. Applique AVANT le potard.
        outLimiter.processGainMatch(yL, yR, dryL, dryR);

        // 3) Etage de sortie : attenuateur de 0 a -24 dB (0 dB = maximum).
        yL *= og;
        yR *= og;

        // 4) Plafond absolu post-potard : filet de securite borne, transparent
        //    sous sa genouillere (-0.8 dBFS), donc inactif en usage normal.
        yL = PrismeOutputLimiter::ceiling(yL);
        yR = PrismeOutputLimiter::ceiling(yR);

        outL[n] = yL;
        if (outR != nullptr)
            outR[n] = yR;
        else
            outL[n] = (yL + yR) * 0.5f;

        // NB : le prelevement spectre/oscilloscope se fait PLUS BAS (deuxieme
        // passe, apres la pre-ecoute) pour que le cadre SPECTRUM bouge aussi
        // quand une prise enregistree est jouee.
    }

    // Suivi du silence (pour la veille) : sortie finale du bloc
    {
        float outPk = 0.0f;
        for (int n = 0; n < numSamples; ++n)
        {
            outPk = juce::jmax(outPk, std::fabs(outL[n]));
            if (outR != nullptr) outPk = juce::jmax(outPk, std::fabs(outR[n]));
        }
        if (inputQuiet && outPk < 1.0e-6f)
            quietSamples = juce::jmin(quietSamples + numSamples, 1 << 30);
        else
            quietSamples = 0;
    }

    // Capture recorder : sortie finale post-gains, un appel par bloc.
    // Jamais de fichier ni d'allocation ici (buffers pre-alloues).
    if (recorder.state.load() == RecorderEngine::Recording)
    {
        const float* recL = buffer.getReadPointer(0);
        const float* recR = (bufCh > 1) ? buffer.getReadPointer(1) : nullptr;
        recorder.writeSamples(recL, recR, numSamples);
    }
    else
    {
        // Hors prise : niveau a zero pour que le metre retombe.
        recorder.peakL.store(0.0f);
        recorder.peakR.store(0.0f);
    }

    // Pre-ecoute LOOP STACK : additive douce post-DSP. Desactivee pendant
    // une prise (l'UI l'interdit + l'armement la coupe). Pas d'alloc, pas
    // de verrou : un atomique + addition.
    if (preview.isPlaying())
    {
        float* pvL = buffer.getWritePointer(0);
        float* pvR = (bufCh > 1) ? buffer.getWritePointer(1) : nullptr;
        preview.renderAdd(pvL, pvR, numSamples);
    }

    // Prelevement spectre + oscilloscope : DEUXIEME PASSE sur la sortie FINALE,
    // donc APRES la pre-ecoute. Le cadre SPECTRUM (au-dessus de SAMPLES) bouge
    // ainsi quand une prise enregistree est jouee, et pas seulement sur le DSP.
    // Memes regles : atomes lock-free uniquement, aucune allocation, aucun appel
    // GUI, et l'echantillon est jete si le FIFO est plein (jamais de blocage).
    {
        const float* finL = buffer.getReadPointer(0);
        const float* finR = (bufCh > 1) ? buffer.getReadPointer(1) : finL;
        for (int n = 0; n < numSamples; ++n)
        {
            const float tapL = finL[n];
            const float tapR = finR[n];
            int st1, sz1, st2, sz2;

            spectrumFifo.prepareToWrite(1, st1, sz1, st2, sz2);
            if (sz1 > 0)
            {
                spectrumBuf[(size_t) st1] = (tapL + tapR) * 0.5f;
                spectrumFifo.finishedWrite(1);
            }

            scopeFifo.prepareToWrite(1, st1, sz1, st2, sz2);
            if (sz1 > 0)
            {
                scopeBuf[(size_t) (2 * st1)] = tapL;
                scopeBuf[(size_t) (2 * st1 + 1)] = tapR;
                scopeFifo.finishedWrite(1);
            }
        }
    }
}

double PrismeProcessor::getHostBpm() const
{
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition(); pos.hasValue())
        {
            if (auto bpm = pos->getBpm(); bpm.hasValue())
            {
                double b = *bpm;
                if (b > 20.0 && b < 300.0)
                    return b;
            }
        }
    }
    return 120.0;
}

void PrismeProcessor::advanceArpStep(int K, int pattern)
{
    if (K <= 1) { arpStep = 0; return; }
    switch (pattern)
    {
        case 0: // Haut
            arpStep = (arpStep + 1) % K;
            break;
        case 1: // Bas
            arpStep = (arpStep - 1 + K) % K;
            break;
        case 2: // Haut-Bas (ping-pong)
            arpStep += arpDir;
            if (arpStep >= K) { arpStep = K - 2 >= 0 ? K - 2 : 0; arpDir = -1; }
            else if (arpStep < 0) { arpStep = K > 1 ? 1 : 0; arpDir = 1; }
            break;
        case 3: // Aleatoire (evite la repetition immediate)
        {
            int next = std::rand() % K;
            if (K > 1 && next == arpStep)
                next = (next + 1 + (std::rand() % (K - 1))) % K;
            arpStep = next;
            break;
        }
        case 4: // Accord : pas de pas, toutes les voix actives
        default:
            break;
    }
}

void PrismeProcessor::updateArp(int numSamples, double sampleRate)
{
    bool arpOn = apvts.getRawParameterValue("arpon")->load() > 0.5f;
    int pattern = (int) apvts.getRawParameterValue("arppattern")->load();
    int scaleIdx = (int) apvts.getRawParameterValue("scale")->load();
    int tones = PrismeScales::getScaleToneCount(scaleIdx);
    tones = juce::jlimit(1, PrismResonatorBank::maxVoices, tones);
    int octaves = (int) apvts.getRawParameterValue("arpoctaves")->load() + 1;
    int K = juce::jlimit(1, PrismResonatorBank::maxVoices, tones * octaves);

    if (! arpOn)
    {
        if (lastArpOn)
        {
            for (int i = 0; i < 12; ++i) arpTargets[i] = 1.0f;
            resonator.setVoiceTargets(arpTargets, 12);
            freezeBank.setVoiceTargets(arpTargets, 12);
            resonator.setArpSplitActive(false);
            freezeBank.setArpSplitActive(false);
            lastArpOn = false;
        }
        return;
    }

    // (Re)initialisation sur activation ou changement de gamme/motif
    if (! lastArpOn || K != lastArpK || pattern != lastArpPattern)
    {
        arpStep = (pattern == 1) ? K - 1 : 0; // Bas commence en haut
        arpDir = 1;
        arpTick = 0;
        arpSamplesToNext = 0.0;
        lastArpK = K;
        lastArpPattern = pattern;
        lastArpOn = true;
    }

    bool sync = apvts.getRawParameterValue("arpsync")->load() > 0.5f;
    double stepSamples;
    if (sync)
    {
        int div = (int) apvts.getRawParameterValue("arpdiv")->load();
        double beat = 60.0 / getHostBpm();
        stepSamples = sampleRate * beat * PrismeScales::getArpDivMult(div);
    }
    else
    {
        float rate = apvts.getRawParameterValue("arprate")->load();
        rate = juce::jlimit(0.5f, 16.0f, rate);
        stepSamples = sampleRate / (double) rate;
    }
    stepSamples = juce::jlimit(64.0, sampleRate * 8.0, stepSamples);

    // Avance les pas qui tombent dans ce bloc (cibles lissees par la banque).
    // Swing : pas pairs allonges (1+s), impairs raccourcis (1-s) ; le cycle
    // de 2 pas garde une duree constante, donc pas de derive du tempo.
    float swing = juce::jlimit(0.0f, 0.75f, apvts.getRawParameterValue("arpswing")->load());
    arpSamplesToNext -= (double) numSamples;
    int guard = 0;
    while (arpSamplesToNext <= 0.0 && guard < 64)
    {
        advanceArpStep(K, pattern);
        double len = stepSamples;
        if (swing > 0.001)
            len *= ((arpTick % 2) == 0) ? (1.0 + swing) : (1.0 - swing);
        arpSamplesToNext += len;
        ++arpTick;
        ++guard;
    }

    constexpr float bed = PrismResonatorBank::kArpBed; // lit harmonique des voix non jouees
    for (int i = 0; i < 12; ++i)
    {
        if (pattern == 4)
            arpTargets[i] = 1.0f;
        else
            arpTargets[i] = ((i % K) == arpStep) ? 1.0f : bed;
    }
    resonator.setVoiceTargets(arpTargets, 12);
    freezeBank.setVoiceTargets(arpTargets, 12);
    // Separation nappe/session seulement quand l'arp selectionne vraiment
    bool split = arpOn && pattern != 4;
    resonator.setArpSplitActive(split);
    freezeBank.setArpSplitActive(split);
}

void PrismeProcessor::updateRecorderTransport(int numSamples)
{
    // Cache transport hote (lectures playhead non bloquantes) + gestion
    // WAITING FOR BAR. Ne touche jamais au DSP ni aux parametres.
    double bpm = 120.0;
    double ppq = -1.0;
    int sigNum = 4, sigDen = 4;
    bool playing = false, hasTransport = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition(); pos.hasValue())
        {
            hasTransport = true;
            if (auto b = pos->getBpm(); b.hasValue())
            {
                double v = *b;
                if (v > 20.0 && v < 300.0)
                    bpm = v;
            }
            if (auto q = pos->getPpqPosition(); q.hasValue())
                ppq = *q;
            playing = pos->getIsPlaying();
            if (auto ts = pos->getTimeSignature(); ts.hasValue())
            {
                sigNum = juce::jlimit(1, 16, (int) ts->numerator);
                sigDen = juce::jlimit(1, 32, (int) ts->denominator);
            }
        }
    }
    hostBpmCache.store(bpm);
    hostPpqCache.store(ppq);
    hostSigNum.store(sigNum);
    hostSigDen.store(sigDen);
    hostPlaying.store(playing);
    hostHasTransport.store(hasTransport);

    if (resetBarPhase.exchange(false))
    {
        barPhaseInit = false;
        lastBarPhase = -1.0;
    }

    if (recorder.state.load() != RecorderEngine::WaitingBar)
        return;

    // Secours : pas de transport exploitable -> demarrage immediat, UI informee.
    if (! hasTransport || ppq < 0.0 || ! playing)
    {
        recorder.forceStart(); // marque fallbackUsed = true
        barPhaseInit = false;
        return;
    }

    // Sync mesure : temps en noires par mesure = num * 4 / den.
    double beatsPerBar = (double) sigNum * 4.0 / (double) juce::jmax(1, sigDen);
    if (beatsPerBar <= 0.0)
    {
        recorder.forceStart();
        barPhaseInit = false;
        return;
    }
    double barPos = ppq / beatsPerBar;
    double frac = barPos - std::floor(barPos);
    const double srHz = getSampleRate();
    if (srHz <= 0.0 || bpm <= 0.0)
    {
        recorder.forceStart();
        barPhaseInit = false;
        return;
    }
    // Position exacte du temps 1 : on regarde si la prochaine mesure tombe DANS
    // ce bloc et on demarre a cet echantillon (plus de retard d'un bloc).
    const double barsPerSample = (bpm / 60.0) / (srHz * beatsPerBar);
    if (frac < 1.0e-9) // le bloc commence pile sur un temps 1
    {
        recorder.notifyBarSyncAt(0);
        return;
    }
    const double samplesToNext = (1.0 - frac) / barsPerSample;
    if (samplesToNext < (double) numSamples)
        recorder.notifyBarSyncAt((int) std::ceil(samplesToNext));
}

void PrismeProcessor::recArmFromUI()
{
    takeStopPreview(); // la pre-ecoute ne survit pas a une nouvelle prise
    resetBarPhase.store(true);
    recorder.armWithTransport(hostBpmCache.load(), hostSigNum.load(),
                              hostSigDen.load(), hostHasTransport.load());
}

bool PrismeProcessor::recStopFromUI()
{
    return recorder.requestStopAndSave();
}

void PrismeProcessor::recForceStartFromUI()
{
    recorder.forceStart();
}

void PrismeProcessor::recCancelFromUI()
{
    recorder.cancelArm();
}

int PrismeProcessor::takeConsumeFresh()
{
    // N'ajoute que sur finalisation confirmee (freshTake = WAV valide).
    if (! recorder.freshTake.exchange(false))
        return -1;
    const PrismeTakeInfo& info = recorder.lastTake;
    if (info.samples <= 0 || ! info.file.existsAsFile())
        return -1;
    TakeEntry e;
    e.number = nextTakeNumber++;
    e.file = info.file;
    e.name = info.name;
    e.durationSec = info.sr > 0 ? (double) info.samples / info.sr : 0.0;
    if (info.bpm > 0.0 && ! info.fallback)
    {
        e.bpmText = juce::String(info.bpm, 1);
        double beatsPerBar = (double) info.sigNum * 4.0 / (double) juce::jmax(1, info.sigDen);
        double bars = e.durationSec * info.bpm / 60.0 / beatsPerBar;
        e.barsText = "~" + juce::String(bars, 1);
    }
    else
    {
        e.bpmText = "--";
        e.barsText = "--";
    }
    // Miniature a partir des buffers (stables : etat Idle, pas d'ecriture
    // audio). 120 cretes max-abs, thread message uniquement.
    e.peaks.assign((size_t) takePeaks, 0.0f);
    const float* L = recorder.dataL();
    const float* R = recorder.dataR();
    int valid = juce::jlimit(0, recorder.getCapacity(), info.samples);
    if (L != nullptr && R != nullptr && valid > 0)
    {
        for (int c = 0; c < takePeaks; ++c)
        {
            int i0 = (int) ((int64_t) c * valid / takePeaks);
            int i1 = (int) ((int64_t) (c + 1) * valid / takePeaks);
            if (i1 <= i0) i1 = i0 + 1;
            float pk = 0.0f;
            for (int i = i0; i < i1 && i < valid; ++i)
                pk = juce::jmax(pk, std::fabs((L[i] + R[i]) * 0.5f));
            e.peaks[(size_t) c] = juce::jlimit(0.0f, 1.0f, pk);
        }
    }
    takeList.push_back(std::move(e));
    if ((int) takeList.size() > maxTakes)
        takeList.erase(takeList.begin()); // fichier conserve, entree retiree
    selectedTake = (int) takeList.size() - 1;
    return selectedTake;
}

void PrismeProcessor::takeRemoveAt(int index, bool deleteFile)
{
    if (index < 0 || index >= (int) takeList.size())
        return;
    if (deleteFile)
        takeList[(size_t) index].file.moveToTrash();
    takeStopPreview();
    takeList.erase(takeList.begin() + index);
    if (selectedTake >= (int) takeList.size())
        selectedTake = (int) takeList.size() - 1;
}

void PrismeProcessor::takePreviewAt(int index)
{
    if (index < 0 || index >= (int) takeList.size())
        return;
    // Pas de pre-ecoute pendant une prise (la capture reste prioritaire).
    int rs = recorder.state.load();
    if (rs == RecorderEngine::Recording || rs == RecorderEngine::WaitingBar
        || rs == RecorderEngine::Saving)
        return;
    // Meme prise deja en train de jouer : arret (bascule PLAY/STOP).
    if (preview.isPlaying() && previewTakeIndex.load() == index)
    {
        takeStopPreview();
        return;
    }
    // Autre prise (ou aucune) : on arrete d'abord, puis on lance celle-ci.
    // Sans cet arret, isPlaying() faisait toujours basculer sur l'ARRET :
    // cliquer une 2e ligne n'executait jamais la prise visee.
    preview.stop();
    previewTakeIndex.store(-1);

    const auto& e = takeList[(size_t) index];
    if (! e.file.existsAsFile())
        return;
    if (preview.loadFile(e.file, getSampleRate() > 0 ? getSampleRate() : 44100.0))
    {
        selectedTake = index;
        preview.start();
        previewTakeIndex.store(index);
    }
}

void PrismeProcessor::takeStopPreview()
{
    preview.stop();
    previewTakeIndex.store(-1);
}

void PrismeProcessor::variationStart(int sourceIndex, int bars, int intensityPct, uint32_t seed,
                                     bool useOneShotRotation)
{
    int js = varJobState.load();
    if (js == 1 || js == 2)
        return; // un rendu est deja en cours
    if (sourceIndex < 0 || sourceIndex >= (int) takeList.size())
    {
        juce::ScopedLock sl(varMessageLock);
        varMessage = "Selectionne d'abord une loop dans le STACK.";
        varJobState.store(4);
        return;
    }
    bars = (bars == 4) ? 4 : 8; // variations calees sur 4 ou 8 mesures
    intensityPct = juce::jlimit(0, 100, intensityPct);
    if (seed == 0) // 0 = auto : tire une seed et l'enregistre (reproductible)
        seed = (uint32_t) juce::Random::getSystemRandom().nextInt();
    const TakeEntry src = takeList[(size_t) sourceIndex]; // copie (fichier seul)
    if (! src.file.existsAsFile())
    {
        juce::ScopedLock sl(varMessageLock);
        varMessage = "Source illisible : fichier introuvable.";
        varJobState.store(4);
        return;
    }
    // LED de liaison (section SAMPLES) : effet vedette qui avance d'un cran a
    // chaque clic. Hors de ce panneau (VAR LAB), jamais de vedette.
    int featured = -1;
    if (useOneShotRotation && variationLinked.load())
    {
        featured = linkedNextKind;
        linkedNextKind = (linkedNextKind + 1) % 8;
    }
    // Tempo : transport live du DAW si dispo, sinon 120 SIGNALE (jamais muet).
    bool hasTr = hostHasTransport.load();
    double bpm = hasTr ? hostBpmCache.load() : 0.0;
    bool defaulted = ! (hasTr && bpm > 20.0 && bpm < 300.0);
    if (defaulted) bpm = 120.0;
    int sigNum = hostSigNum.load(), sigDen = hostSigDen.load();
    int varNum = nextTakeNumber++; // numero reserve tout de suite (fichier unique)
    {
        juce::ScopedLock sl(varMessageLock);
        varMessage = "Plan seed " + juce::String(seed) + "...";
    }
    varJobProgress.store(0.0);
    varJobState.store(1); // GENERATING
    varJobsActive.fetch_add(1);
    juce::Thread::launch([this, src, bars, intensityPct, seed, bpm, sigNum, sigDen,
                          defaulted, varNum, featured]
    {
        struct JobGuard
        {
            std::atomic<int>& c;
            ~JobGuard() { c.fetch_sub(1); } // dernier acces a this (tous les return)
        } jobGuard { varJobsActive };
        // 1. Charge la source (WAV). L'original n'est ouvert qu'en lecture.
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(
            src.file.createInputStream().release(), true));
        if (reader == nullptr || reader->lengthInSamples <= 0
            || reader->numChannels <= 0)
        {
            juce::ScopedLock sl(varMessageLock);
            varMessage = "Source illisible (WAV requis).";
            varJobState.store(4);
            return;
        }
        double fileSr = reader->sampleRate > 0 ? reader->sampleRate : 44100.0;
        int srcLen = (int) std::min<int64_t>(reader->lengthInSamples,
                                             (int64_t) (fileSr * 600.0));
        std::vector<float> sL((size_t) srcLen), sR((size_t) srcLen);
        {
            juce::AudioBuffer<float> ab(2, srcLen);
            if (! reader->read(&ab, 0, srcLen, 0, true, true))
            {
                juce::ScopedLock sl(varMessageLock);
                varMessage = "Lecture de la source impossible.";
                varJobState.store(4);
                return;
            }
            const float* rL = ab.getReadPointer(0);
            const float* rR = ab.getNumChannels() > 1 ? ab.getReadPointer(1) : rL;
            for (int i = 0; i < srcLen; ++i)
            {
                sL[(size_t) i] = rL[i];
                sR[(size_t) i] = rR[i];
            }
        }
        // 2. Plan AVANT le rendu (meme seed + meme config = meme plan).
        VarLab::Grid grid;
        grid.bpm = bpm;
        grid.sigNum = sigNum;
        grid.sigDen = sigDen;
        grid.bars = bars;
        grid.sr = fileSr;
        VarLab::Plan plan = VarLab::buildPlan(grid, seed, intensityPct, featured);
        plan.tempoDefaulted = defaulted;
        plan.exactBars = true; // fichier pile sur la grille, sans stretch
        varJobProgress.store(0.05);
        varJobState.store(2); // RENDERING
        // 3. Rendu offline (progres 0.05 -> 1.0).
        juce::String renderError;
        VarLab::RenderResult rr = VarLab::renderPlan(
            plan, sL.data(), sR.data(), srcLen, nullptr, &shuttingDown);
        if (! rr.ok)
        {
            juce::ScopedLock sl(varMessageLock);
            varMessage = rr.error.isNotEmpty() ? rr.error : "Rendu impossible.";
            varJobState.store(4);
            return;
        }
        varJobProgress.store(0.95);
        // 4. Nouveau WAV (jamais l'original) + entree sous la source.
        juce::String vname = juce::String::formatted("Prisme_VAR%02d_from%02d_int%d_seed%u.wav",
            varNum, src.number, intensityPct, seed);
        juce::File out = src.file.getSiblingFile(vname);
        out.getParentDirectory().createDirectory();
        bool ok = false;
        if (rr.outSamples > 0)
        {
            if (auto* fos = out.createOutputStream().release())
            {
                std::unique_ptr<juce::FileOutputStream> fosPtr(fos);
                fosPtr->setPosition(0);
                fosPtr->truncate();
                if (auto* w = fmt.createWriterFor(fosPtr.get(), rr.outSr, 2, 16, {}, 0))
                {
                    fosPtr.release();
                    juce::AudioBuffer<float> ab(2, rr.outSamples);
                    ab.copyFrom(0, 0, rr.outL.data(), rr.outSamples);
                    ab.copyFrom(1, 0, rr.outR.data(), rr.outSamples);
                    ok = w->writeFromAudioSampleBuffer(ab, 0, rr.outSamples);
                    delete w;
                }
            }
        }
        if (! ok)
        {
            juce::ScopedLock sl(varMessageLock);
            varMessage = "Ecriture du WAV impossible.";
            varJobState.store(4);
            return;
        }
        TakeEntry e;
        e.number = varNum;
        e.file = out;
        e.name = juce::String::formatted("VAR %02d (de %02d) %d%% s%u",
            varNum, src.number, intensityPct, seed);
        e.durationSec = (double) rr.outSamples / rr.outSr;
        e.bpmText = defaulted ? juce::String((int) bpm) + "!" : juce::String(bpm, 1);
        e.barsText = juce::String(bars); // exact : pas de queue, cale grille
        e.peaks.assign(rr.peaks.begin(), rr.peaks.end());
        e.isVariation = true;
        e.parentNumber = src.number;
        e.seed = seed;
        e.intensityPct = intensityPct;
        e.varBars = bars;
        e.tempoUsed = defaulted ? 0.0 : bpm;
        pendingVariation.entry = std::move(e);
        {
            juce::ScopedLock sl(varMessageLock);
            varMessage = plan.summary;
        }
        varJobProgress.store(1.0);
        varJobState.store(3); // COMPLETE
    });
}

int PrismeProcessor::variationConsumeDone()
{
    if (varJobState.load() != 3)
        return -1;
    TakeEntry e = pendingVariation.entry; // copie (thread message)
    // Insertion juste sous la source, apres ses variations existantes.
    int at = (int) takeList.size();
    for (size_t i = 0; i < takeList.size(); ++i)
    {
        if (takeList[i].number == e.parentNumber)
        {
            at = (int) i + 1;
            while (at < (int) takeList.size() && takeList[(size_t) at].isVariation
                   && takeList[(size_t) at].parentNumber == e.parentNumber)
                ++at;
            break;
        }
    }
    takeList.insert(takeList.begin() + at, std::move(e));
    if ((int) takeList.size() > maxTakes)
        takeList.erase(takeList.begin()); // fichier conserve
    selectedTake = at;
    varJobState.store(0);
    varJobProgress.store(0.0);
    return at;
}

juce::String PrismeProcessor::variationStatusText() const
{
    int js = varJobState.load();
    juce::ScopedLock sl(varMessageLock);
    int pct = (int) std::round(varJobProgress.load() * 100.0);
    switch (js)
    {
        case 1: return "GENERATING";
        case 2: return "RENDERING " + juce::String(pct) + "%";
        case 3: return "COMPLETE";
        case 4: return "ERROR - " + varMessage;
        default: return varMessage.isNotEmpty() ? varMessage : "READY";
    }
}

void PrismeProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PrismeProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        undoManager.clearUndoHistory(); // l'historique d'avant le chargement n'a plus de sens
    }
}

// ---------------------------------------------------------------------------
// AUTO KEY : thread message. FFT + chroma + correlation Krumhansl-Kessler.
// ---------------------------------------------------------------------------
void PrismeProcessor::timerCallback()
{
    const bool on = apvts.getRawParameterValue("autokey")->load() > 0.5f;
    const int wanted = on ? 100 : 500;
    if (getTimerInterval() != wanted)
        startTimer(wanted);

    if (! on)
    {
        keyActive = false; // plus aucun calcul : on ne fait que surveiller le bouton
        return;
    }

    if (! keyActive) // AUTO vient d'etre active : analyse repartie de zero
    {
        keyActive = true;
        keyDet.reset();
        keyCand = -1;
        keyStable = 0;
        keyApplied = -1;
    }

    int guard = 0;
    for (int n = keyFifo.getNumReady(); n > 0 && guard < 8; n = keyFifo.getNumReady(), ++guard)
    {
        int s1, z1, s2, z2;
        keyFifo.prepareToRead(n, s1, z1, s2, z2);
        if (z1 > 0) keyDet.pushSamples(keyBuf.data() + s1, z1);
        if (z2 > 0) keyDet.pushSamples(keyBuf.data() + s2, z2);
        keyFifo.finishedRead(z1 + z2);
    }

    PrismeKeyDetector::Result r;
    if (! keyDet.analyse(getSampleRate(), r))
        return; // silence ou fenetre pas pleine

    if (r.corr < 0.55f || r.margin < 0.02f)
    {
        keyCand = -1;
        keyStable = 0;
        return;
    }
    const int cand = r.root + (r.minor ? 12 : 0);
    if (cand == keyCand) ++keyStable;
    else { keyCand = cand; keyStable = 1; }

    if (keyStable < 20 || cand == keyApplied)
        return;
    keyApplied = cand;

    auto setChoice = [this](const char* id, int idx)
    {
        if (auto* p = apvts.getParameter(id))
        {
            const float v = p->convertTo0to1((float) idx);
            if (std::abs(p->getValue() - v) > 1.0e-4f)
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost(v);
                p->endChangeGesture();
            }
        }
    };
    setChoice("root", r.root);
    setChoice("scale", r.minor ? 1 : 0); // 0 = Major, 1 = Minor Naturel
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PrismeProcessor();
}
