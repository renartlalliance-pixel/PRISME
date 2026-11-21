#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Scales.h"
#include "ArpFX.h"
#include "PrismeTextures.h"
#include "PrismeDiy.h"
#include "PrismeDragDrop.h"

PrismeEditor::PrismeEditor(PrismeProcessor& p)
    : AudioProcessorEditor(p), proc(p), apvts(p.getAPVTS()),
      content(*this), recorderView(p), cornerResizer(this, &constrainer)
{
    setLookAndFeel(&hwLAF);
    addAndMakeVisible(content);
    // NOTE : setSize() est volontairement a la FIN du constructeur :
    // il declenche resized(), qui a besoin que tous les composants existent.

    // Titre
    titleLabel.setText("P R I S M E", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(32.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    content.addAndMakeVisible(titleLabel);

    // Selecteur de skin : en haut a gauche, aucun controle deplace
    skinLabel.setText("Skin", juce::dontSendNotification);
    skinLabel.setJustificationType(juce::Justification::centredRight);
    skinLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8a8fa8));
    skinLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    content.addAndMakeVisible(skinLabel);
    // mineprisme est masque du menu deroulant. L'ID de chaque entree reste
    // "index du skin + 1" : selectedId - 1 donne donc toujours le bon index,
    // aucun skin memorise (uiSkin) n'est decale.
    for (int i = 0; i < (int) PrismeSkins::all().size(); ++i)
    {
        if (PrismeSkins::all()[i].name == "mineprisme")
            continue;
        skinBox.addItem(PrismeSkins::all()[i].name, i + 1);
    }
    skinBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1d2e));
    skinBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    skinBox.onChange = [this]
    {
        int idx = skinBox.getSelectedId() - 1;
        if (idx < 0 || idx >= (int) PrismeSkins::all().size()) return;
        applySkin(idx);
        apvts.state.setProperty("uiSkin", idx, nullptr); // hors params audio
    };
    content.addAndMakeVisible(skinBox);

    // Bascule RECORDER : bande superieure, aucun controle deplace.
    recorderBtn.setButtonText("RECORDER");
    recorderBtn.onClick = [this] { showRecorder(true); };
    content.addAndMakeVisible(recorderBtn);
    // ---- ACCES A LA PARTIE RECORDER RETIRE ----
    // Entree masquee et inutilisable : la partie RECORDER n'est plus
    // joignable. Le handler reste pose mais showRecorder() refuse l'ouverture.
    recorderBtn.setVisible(false);
    recorderBtn.setEnabled(false);
    recorderBtn.onClick = nullptr;

    // Vue RECORDER : meme fenetre, memes dimensions, recouvre le contenu.
    // Masquee par defaut ; la capture vit dans le processeur.
    // NOTE : addAndMakeVisible rend visible -> masquer APRES l'ajout.
    recorderView.onBack = [this] { showRecorder(false); };
    content.addAndMakeVisible(recorderView);
    recorderView.setVisible(false);

    // --- Panneau accords ---
    auto rootNames = PrismeScales::getRootNames();
    for (size_t i = 0; i < rootNames.size(); ++i)
        rootBox.addItem(rootNames[i], (int)i + 1);
    auto scaleNames = PrismeScales::getScaleNames();
    for (size_t i = 0; i < scaleNames.size(); ++i)
        scaleBox.addItem(scaleNames[i], (int)i + 1);
    auto octNames = PrismeScales::getOctaveNames();
    for (size_t i = 0; i < octNames.size(); ++i)
        octaveBox.addItem(octNames[i], (int)i + 1);

    for (auto* cb : { &rootBox, &scaleBox, &octaveBox, &arpPatternBox, &arpDivBox, &arpOctBox, &arpFxBox, &arpFxBoxB, &arpFxBoxC, &arpRouteBox })
    {
        cb->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1d2e));
        cb->setColour(juce::ComboBox::textColourId, juce::Colours::white);
        content.addAndMakeVisible(*cb);
    }

    rootAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "root", rootBox);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "scale", scaleBox);
    octaveAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "octave", octaveBox);

    // Givre : fader dry/wet horizontal (0 = normal, 1 = gele)
    givreLabel.setText("Givre", juce::dontSendNotification);
    givreLabel.setJustificationType(juce::Justification::centredRight);
    givreLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    givreLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    content.addAndMakeVisible(givreLabel);

    givreSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    givreSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    givreSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff7df9ff));
    givreSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff2c2f4a));
    content.addAndMakeVisible(givreSlider);
    givreAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "freezemix", givreSlider);

    // Randomize : melange les parametres de son (volumes preserves)
    randomBtn.setButtonText("Randomize");
    randomBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff23263d));
    randomBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    randomBtn.onClick = [this] { randomizeSound(); };
    content.addAndMakeVisible(randomBtn);

    // ---- Barre de transport du panneau SAMPLES ----
    // Mêmes appels que la vue Recorder (armement puis démarrage, arrêt,
    // variation depuis la prise sélectionnée). Aucune fonction simulée.
    sampleRecBtn.setTooltip("Armer l'enregistrement (2e appui = demarrage immediat)");
    sampleRecBtn.onClick = [this]
    {
        int st = proc.recorder.state.load();
        if (st == RecorderEngine::Idle)
            proc.recArmFromUI();
        else if (st == RecorderEngine::WaitingBar)
            proc.recForceStartFromUI();
        recorderView.refresh();
    };
    content.addAndMakeVisible(sampleRecBtn);

    sampleStopBtn.setTooltip("Arreter l'enregistrement");
    sampleStopBtn.onClick = [this]
    {
        proc.recStopFromUI();
        recorderView.refresh();
    };
    content.addAndMakeVisible(sampleStopBtn);

    // Variation : reutilise la logique du Recorder (seed/mesures/intensite).
    auto startVariationFromPanel = [this](bool reroll)
    {
        if (proc.selectedTake < 0 || proc.selectedTake >= (int) proc.takeList.size())
            return;                                   // rien a varier
        juce::Random rnd;
        uint32_t seed = (uint32_t) rnd.nextInt();
        if (seed == 0) seed = 1;                      // 0 = reserve (auto moteur)
        int bars = 8;                                 // defaut si non précisé
        proc.variationStart(proc.selectedTake, bars, 60, seed);
        (void) reroll;
        recorderView.refresh();
    };
    sampleRandomBtn.setTooltip("Nouvelle variation depuis la prise selectionnee");
    sampleRandomBtn.onClick = [this, startVariationFromPanel] { startVariationFromPanel(false); };
    content.addAndMakeVisible(sampleRandomBtn);

    sampleRerollBtn.setTooltip("Nouvelle seed + nouvelle variation depuis la meme source");
    sampleRerollBtn.onClick = [this, startVariationFromPanel] { startVariationFromPanel(true); };
    content.addAndMakeVisible(sampleRerollBtn);

    // LED de liaison RANDOMIZE / REROLL : allumee, chaque variation met un
    // effet vedette en avant et l'effet avance d'un clic a l'autre (one shot).
    // Eteinte, les effets sont tires au hasard. Etat en memoire du plugin.
    sampleLinkLed.setButtonText("");
    sampleLinkLed.setTooltip("Lier RANDOMIZE et REROLL : chaque variation met un effet "
                             "vedette en avant, et l'effet change a chaque clic");
    sampleLinkLed.onClick = [this]
    {
        proc.variationLinked.store(sampleLinkLed.getToggleState());
    };
    content.addAndMakeVisible(sampleLinkLed);

    // Pastilles de gamme : clic = nouvelle tonique, la gamme se reaccorde
    content.addAndMakeVisible(pads);
    pads.onPadClicked = [this](int pc)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("root")))
            p->setValueNotifyingHost((float) pc / (float) (p->choices.size() - 1));
    };
    startTimer(33); // ~30 Hz : pads, labels, bande + spectrogramme (repeint partiel seul)

    // --- Knobs : 10 potards (2 rangees de 5) ---
    addKnob("decay", "Resonance");
    addKnob("attack", "Attaque");
    addKnob("brightness", "Couleur");
    addKnob("iridescence", "Iridescence");
    addKnob("shimmer", "Vivant");
    addKnob("spread", "Largeur");
    addKnob("ingain", "Entree");
    addKnob("outgain", "Sortie");
    addKnob("arprate", "Taux Arp");
    addKnob("movrate", "Vitesse");
    addKnob("movdepth", "Force");
    addKnob("movshape", "Forme");
    addKnob("movunison", "Unisson");

    // --- Panneau arp ---
    arpTitleLabel.setText("A R P E G E", juce::dontSendNotification);
    arpTitleLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    arpTitleLabel.setJustificationType(juce::Justification::centred);
    arpTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd8b25c));
    content.addAndMakeVisible(arpTitleLabel);

    auto patNames = PrismeScales::getArpPatternNames();
    for (size_t i = 0; i < patNames.size(); ++i)
        arpPatternBox.addItem(patNames[i], (int)i + 1);
    auto divNames = PrismeScales::getArpDivNames();
    for (size_t i = 0; i < divNames.size(); ++i)
        arpDivBox.addItem(divNames[i], (int)i + 1);
    arpOctBox.addItem("1", 1);
    arpOctBox.addItem("2", 2);
    arpOctBox.addItem("3", 3);
    arpOctBox.setTooltip("Octaves parcourues par l'arpege");

    arpOnBtn.setButtonText("Arp On");
    arpSyncBtn.setButtonText("Sync Tempo");
    autoKeyBtn.setButtonText("Auto");
    autoKeyBtn.setTooltip("Detecte la tonalite de l'entree (majeur/mineur) et regle Tonique + Gamme");
    for (auto* tb : { &arpOnBtn, &arpSyncBtn, &autoKeyBtn })
    {
        tb->setColour(juce::ToggleButton::textColourId, juce::Colours::white);
        tb->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffffc36b));
        content.addAndMakeVisible(*tb);
    }
    arpOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "arpon", arpOnBtn);
    autoKeyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "autokey", autoKeyBtn);
    arpSyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "arpsync", arpSyncBtn);
    arpPatternAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "arppattern", arpPatternBox);
    arpDivAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "arpdiv", arpDivBox);
    arpOctAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "arpoctaves", arpOctBox);

    arpFxBox.addItem("Off", 1);
    arpFxBox.addItem("Delay", 2);
    arpFxBox.addItem("Chorus", 3);
    arpFxBox.addItem("Filter", 4);
    arpFxBox.addItem("Crusher", 5);
    arpFxBox.addItem("Reverse", 6);
    arpFxAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "arpfxtype", arpFxBox);

    undoBtn.setButtonText("Undo");
    redoBtn.setButtonText("Redo");
    undoBtn.setTooltip("Annuler le dernier reglage");
    redoBtn.setTooltip("Refaire le reglage annule");
    undoBtn.onClick = [this] { proc.getUndoManager().undo(); };
    redoBtn.onClick = [this] { proc.getUndoManager().redo(); };
    content.addAndMakeVisible(undoBtn);
    content.addAndMakeVisible(redoBtn);

    arpRouteLabel.setText("Cible FX", juce::dontSendNotification);
    arpRouteLabel.setJustificationType(juce::Justification::centred);
    arpRouteLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    arpRouteLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    arpRouteLabel.setInterceptsMouseClicks(false, false);
    content.addAndMakeVisible(arpRouteLabel);

    arpRouteBox.addItem("Arp", 1);
    arpRouteBox.addItem("Accords", 2);
    arpRouteBox.addItem("Tout", 3);
    arpRouteBox.setTooltip("Cible des FX : Arp = arpege seul, Accords = toute la resonance, Tout = apres Mirage");
    arpRouteAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "arpfxroute", arpRouteBox);

    arpFxBoxB.addItem("Off", 1);
    arpFxBoxB.addItem("Delay", 2);
    arpFxBoxB.addItem("Chorus", 3);
    arpFxBoxB.addItem("Filter", 4);
    arpFxBoxB.addItem("Crusher", 5);
    arpFxBoxB.addItem("Reverse", 6);
    arpFxAttB = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "arpfxtypeB", arpFxBoxB);

    arpFxBoxC.addItem("Off", 1);
    arpFxBoxC.addItem("Delay", 2);
    arpFxBoxC.addItem("Chorus", 3);
    arpFxBoxC.addItem("Filter", 4);
    arpFxBoxC.addItem("Crusher", 5);
    arpFxBoxC.addItem("Reverse", 6);
    arpFxAttC = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "arpfxtypeC", arpFxBoxC);

    // Bouton rouge unique : cycle le slot affiche sur les potards (sans nom)
    // 1 bouton rouge par menu : selectionne le slot affiche sur les potards
    juce::TextButton* slotBtns[3] = { &fxSlotBtn0, &fxSlotBtn1, &fxSlotBtn2 };
    for (int i = 0; i < 3; ++i)
    {
        slotBtns[i]->setButtonText("");
        slotBtns[i]->setColour(juce::TextButton::buttonColourId, juce::Colour(0xffc03028));
        slotBtns[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        slotBtns[i]->onClick = [this, i]
        {
            fxSlotSel = i;
            refreshFxKnobs();
        };
        content.addAndMakeVisible(*slotBtns[i]);
    }

    auto styleArpFxKnob = [this](juce::Slider& s, juce::Label& lb)
    {
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setRange(0.0, 1.0, 0.001); // OBLIGATOIRE : sans attachement, la plage par defaut bloque l'aiguille
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffffc36b));
        s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2c2f4a));
        s.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        content.addAndMakeVisible(s);
        lb.setJustificationType(juce::Justification::centred);
        lb.setColour(juce::Label::textColourId, juce::Colours::white);
        lb.setFont(juce::Font(10.0f, juce::Font::bold));
        content.addAndMakeVisible(lb);
    };
    styleArpFxKnob(arpFxKnob1, arpFxLabel1);
    styleArpFxKnob(arpFxKnob2, arpFxLabel2);
    styleArpFxKnob(arpFxMixKnob, arpFxMixLabel);
    arpFxMixLabel.setText("Mix", juce::dontSendNotification);
    // Potards 1/2 manuels : ils ecrivent dans le slot A ou B selon fxSlotSel
    arpFxKnob1.addListener(this);
    arpFxKnob2.addListener(this);
    arpFxMixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "arpfxmix", arpFxMixKnob);
    refreshFxKnobs();

    swingLabel.setText("Swing", juce::dontSendNotification);
    swingLabel.setJustificationType(juce::Justification::centredRight);
    swingLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    swingLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    content.addAndMakeVisible(swingLabel);
    swingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    swingSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    swingSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffffc36b));
    swingSlider.setColour(juce::Slider::trackColourId, juce::Colour(0xff2c2f4a));
    content.addAndMakeVisible(swingSlider);
    swingAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "arpswing", swingSlider);

    // --- Panneau mouvement ---
    movTitleLabel.setText("M O U V E M E N T", juce::dontSendNotification);
    movTitleLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    movTitleLabel.setJustificationType(juce::Justification::centred);
    movTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd8b25c));
    content.addAndMakeVisible(movTitleLabel);

    auto movDivNames = PrismeScales::getArpDivNames();
    for (size_t i = 0; i < movDivNames.size(); ++i)
        movDivBox.addItem(movDivNames[i], (int)i + 1);
    movDivBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1d2e));
    movDivBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    content.addAndMakeVisible(movDivBox);

    movSyncBtn.setButtonText("Sync Tempo");
    movSyncBtn.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    movSyncBtn.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff5effc3));
    content.addAndMakeVisible(movSyncBtn);
    movSyncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "movsync", movSyncBtn);
    movDivAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "movdiv", movDivBox);

    // --- Menu presets : usine + section "User preset" + sauvegarde ---
    presetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1d2e));
    presetBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    presetBox.setTextWhenNothingSelected("Choisir un preset...");
    content.addAndMakeVisible(presetBox);

    presetNameEditor.setTextToShowWhenEmpty("Nom du preset...", juce::Colour(0xff8a8fa8));
    presetNameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a1d2e));
    presetNameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    presetNameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2a2e4d));
    presetNameEditor.onReturnKey = [this] { saveUserPreset(); };
    content.addAndMakeVisible(presetNameEditor);

    presetSaveBtn.setButtonText("Sauver");
    presetSaveBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2f5d4c));
    presetSaveBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    presetSaveBtn.onClick = [this] { saveUserPreset(); };
    content.addAndMakeVisible(presetSaveBtn);

    presetDeleteBtn.setButtonText("X");
    presetDeleteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a2e35));
    presetDeleteBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    presetDeleteBtn.onClick = [this] { deleteSelectedUserPreset(); };
    content.addAndMakeVisible(presetDeleteBtn);

    refreshPresetList();
    presetBox.onChange = [this] { onPresetChosen(); };

    // A/B : deux versions du son, switch + copie
    slotABtn.setButtonText("A");
    slotBBtn.setButtonText("B");
    slotCopyBtn.setButtonText("Copier");
    slotABtn.onClick = [this] { selectSlot(0); };
    slotBBtn.onClick = [this] { selectSlot(1); };
    slotCopyBtn.onClick = [this] { copySlotToOther(); };
    content.addAndMakeVisible(slotABtn);
    content.addAndMakeVisible(slotBBtn);
    content.addAndMakeVisible(slotCopyBtn);
    slotA = apvts.copyState();
    slotB = apvts.copyState();
    activeSlot = 0;
    refreshSlotColours();
    apvts.state.addListener(this);

    // --- Panneau EQ graphique 4 bandes ---
    eqTitleLabel.setText("E Q", juce::dontSendNotification);
    eqTitleLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    eqTitleLabel.setJustificationType(juce::Justification::centred);
    eqTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd8b25c));
    content.addAndMakeVisible(eqTitleLabel);

    const char* eqIds[4] = { "eqlow", "eqlowmid", "eqhighmid", "eqhigh" };
    const char* eqNames[4] = { "Graves 150", "Bas-Med 800", "Haut-Med 3k", "Aigus 8k" };
    for (int i = 0; i < 4; ++i)
    {
        eqSliders[i].setSliderStyle(juce::Slider::LinearVertical);
        eqSliders[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
        eqSliders[i].setColour(juce::Slider::trackColourId, juce::Colour(0xff2c2f4a));
        eqSliders[i].setColour(juce::Slider::thumbColourId, juce::Colour(0xff9d7bff));
        eqSliders[i].setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffffcf7d));
        eqSliders[i].setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff101114));
        content.addAndMakeVisible(eqSliders[i]);

        eqLabels[i].setText(eqNames[i], juce::dontSendNotification);
        eqLabels[i].setJustificationType(juce::Justification::centred);
        eqLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xffb9bec8));
        eqLabels[i].setFont(juce::Font(12.0f, juce::Font::bold));
        content.addAndMakeVisible(eqLabels[i]);

        eqAtts[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, eqIds[i], eqSliders[i]);
    }

    // Fader de selection de frequence : double curseur Bas/Haut (20 Hz - 20 kHz)
    bandTitleLabel.setText("BANDE", juce::dontSendNotification);
    bandTitleLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    bandTitleLabel.setJustificationType(juce::Justification::centred);
    bandTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd8b25c));
    content.addAndMakeVisible(bandTitleLabel);

    for (auto* lb : { &bandHiLabel, &bandLoLabel })
    {
        lb->setJustificationType(juce::Justification::centred);
        lb->setColour(juce::Label::textColourId, juce::Colours::white);
        lb->setFont(juce::Font(11.0f, juce::Font::bold));
        content.addAndMakeVisible(*lb);
    }

    bandFader.setSliderStyle(juce::Slider::ThreeValueVertical);
    bandFader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    bandFader.setRange(20.0, 20000.0, 1.0);
    bandFader.setSkewFactorFromMidPoint(1200.0);
    bandFader.setColour(juce::Slider::trackColourId, juce::Colour(0xff2c2f4a));
    bandFader.setColour(juce::Slider::thumbColourId, juce::Colour(0xffffc36b));
    bandFader.addListener(this);
    content.addAndMakeVisible(bandFader);

    footerLabel.setText("Force a 0 = transparent  -  Forme 0 = respiration douce, 1 = rafales nettes  -  Unisson = une seule impulsion",
                        juce::dontSendNotification);
    footerLabel.setFont(juce::Font(12.0f));
    footerLabel.setJustificationType(juce::Justification::centred);
    footerLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8a8fa8));
    // --- Tranche laterale : faders MIX + BLUR + taille ---
    mixTitleLabel.setText("MIX", juce::dontSendNotification);
    mixTitleLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    mixTitleLabel.setJustificationType(juce::Justification::centred);
    mixTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd8b25c));
    content.addAndMakeVisible(mixTitleLabel);

    blurTitleLabel.setText("MIRAGE", juce::dontSendNotification);
    blurTitleLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    blurTitleLabel.setJustificationType(juce::Justification::centred);
    blurTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd8b25c));
    content.addAndMakeVisible(blurTitleLabel);

    wetLabel.setText("WET", juce::dontSendNotification);
    dryLabel.setText("DRY", juce::dontSendNotification);
    blurWetLabel.setText("WET", juce::dontSendNotification);
    blurDryLabel.setText("DRY", juce::dontSendNotification);
    for (auto* lb : { &wetLabel, &dryLabel, &blurWetLabel, &blurDryLabel })
    {
        lb->setJustificationType(juce::Justification::centred);
        lb->setColour(juce::Label::textColourId, juce::Colour(0xff8a8fa8));
        lb->setFont(juce::Font(11.0f, juce::Font::bold));
        content.addAndMakeVisible(*lb);
    }

    auto styleFader = [](juce::Slider& s, juce::Colour thumb)
    {
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 18);
        s.setColour(juce::Slider::trackColourId, juce::Colour(0xff2c2f4a));
        s.setColour(juce::Slider::thumbColourId, thumb);
        s.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffffcf7d));
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff101114));
        s.textFromValueFunction = [](double v) { return juce::String((int) std::round(v * 100.0)) + " %"; };
        s.valueFromTextFunction = [](const juce::String& t) { return juce::jlimit(0.0, 1.0, t.getDoubleValue() / 100.0); };
    };
    styleFader(mixFader, juce::Colour(0xff7df9ff));
    content.addAndMakeVisible(mixFader);
    mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mix", mixFader);

    styleFader(blurFader, juce::Colour(0xff9d7bff));
    content.addAndMakeVisible(blurFader);
    blurAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "blurmix", blurFader);

    tailleSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    tailleSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    tailleSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff9d7bff));
    tailleSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2c2f4a));
    tailleSlider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    content.addAndMakeVisible(tailleSlider);
    tailleLabel.setText("Taille", juce::dontSendNotification);
    tailleLabel.setJustificationType(juce::Justification::centred);
    tailleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffb9bec8));
    tailleLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    content.addAndMakeVisible(tailleLabel);
    tailleAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "blursize", tailleSlider);

    // Rangee Mirage : 4 mini-potards + Gel (meme style, sans afficheur)
    auto styleMirKnob = [this](juce::Slider& s, juce::Label& lb, const char* txt)
    {
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff9d7bff));
        s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2c2f4a));
        s.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        content.addAndMakeVisible(s);
        lb.setText(txt, juce::dontSendNotification);
        lb.setJustificationType(juce::Justification::centred);
        lb.setColour(juce::Label::textColourId, juce::Colour(0xffb9bec8));
        lb.setFont(juce::Font(11.0f, juce::Font::bold));
        content.addAndMakeVisible(lb);
    };
    styleMirKnob(mirPredelaySlider, mirPredelayLabel, "Pre-Del");
    mirPredelayAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mirpredelay", mirPredelaySlider);
    styleMirKnob(mirColorSlider, mirColorLabel, "Couleur");
    mirColorAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mircolor", mirColorSlider);
    styleMirKnob(mirWidthSlider, mirWidthLabel, "Largeur");
    mirWidthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mirwidth", mirWidthSlider);
    styleMirKnob(mirDecaySlider, mirDecayLabel, "Decay");
    mirDecayAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mirdecay", mirDecaySlider);

    // CRYSTALLIZE : meme style que les potards FX voisins, attache au parametre
    cryoKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    cryoKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    cryoKnob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff7df9ff));
    cryoKnob.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2c2f4a));
    cryoKnob.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    content.addAndMakeVisible(cryoKnob);
    cryoLabel.setText("Crystallize", juce::dontSendNotification);
    cryoLabel.setJustificationType(juce::Justification::centred);
    cryoLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    cryoLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    content.addAndMakeVisible(cryoLabel);
    cryoAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "crystallize", cryoKnob);

    // 3 LED de cible CRYSTALLIZE, empilees a droite du potard, chacune suivie
    // de son libelle. 1 clic allume la LED ; les autres s'eteignent. Re-cliquer
    // sur la LED allumee eteint les 3 (cible "Aucune"). Aucun potard ajoute.
    // Ordre de haut en bas : no arp, arp, all.
    {
        static const char* cryoLedTxt[3] = { "no arp", "arp", "all" };
        static const char* cryoTip[3] = { "Crystallize sur tout sauf l'arpege",
                                          "Crystallize sur l'arpege seulement",
                                          "Crystallize sur tout l'effet" };
        for (int i = 0; i < 3; ++i)
        {
            cryoLed[i].setButtonText("");
            cryoLed[i].setTooltip(cryoTip[i]);
            cryoLed[i].onClick = [this, i]
            {
                auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("cryotarget"));
                if (p == nullptr) return;
                const int cur = (int) apvts.getRawParameterValue("cryotarget")->load();
                const int next = (cur == i + 1) ? 0 : (i + 1); // re-clic = tout eteint
                p->setValueNotifyingHost((float) next / (float) (p->choices.size() - 1));
            };
            content.addAndMakeVisible(cryoLed[i]);

            cryoLedLabel[i].setText(cryoLedTxt[i], juce::dontSendNotification);
            cryoLedLabel[i].setJustificationType(juce::Justification::centredLeft);
            cryoLedLabel[i].setColour(juce::Label::textColourId, juce::Colours::white);
            cryoLedLabel[i].setFont(juce::Font(9.0f, juce::Font::bold));
            cryoLedLabel[i].setInterceptsMouseClicks(false, false); // ne gene pas le clic
            content.addAndMakeVisible(cryoLedLabel[i]);
        }
    }

    content.addAndMakeVisible(footerLabel);

    // Skin memorise (propriete hors params audio), applique avant affichage.
    // Valeur par defaut 2 = MONOLITH : utilisee UNIQUEMENT quand aucune
    // peau n'a ete memorisee. Un projet ayant deja enregistre une peau
    // conserve la sienne (le defaut ne s'applique jamais par-dessus).
    int skinIdx = juce::jlimit(0, (int) PrismeSkins::all().size() - 1,
                               (int) apvts.state.getProperty("uiSkin", 2));
    skinBox.setSelectedId(skinIdx + 1, juce::dontSendNotification);
    applySkin(skinIdx);

    // Redimensionnement : ratio verrouille 1670x941 (format horizontal 16:9),
    // min 835x471, max 3340x1882. Le contenu est mis a l'echelle uniformement.
    constrainer.setSizeLimits(835, 471, 3340, 1882);
    constrainer.setFixedAspectRatio(1670.0 / 941.0);
    addAndMakeVisible(cornerResizer);

    setSize(1670, 941);
    // Etat initial garanti : PRISME seul (jamais les deux vues superposees).
    showRecorder(false);
}

PrismeEditor::~PrismeEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
    apvts.state.removeListener(this);
    cancelPendingUpdate();
}

void PrismeEditor::timerCallback()
{
    auto* pRoot = apvts.getRawParameterValue("root");
    auto* pScale = apvts.getRawParameterValue("scale");
    if (pRoot == nullptr || pScale == nullptr) return;
    int r = (int) pRoot->load();
    int s = (int) pScale->load();
    if (r != lastPadsRoot || s != lastPadsScale)
    {
        lastPadsRoot = r;
        lastPadsScale = s;
        auto scales = PrismeScales::getScales();
        if (s >= 0 && s < (int) scales.size())
            pads.setScale(r, scales[(size_t) s].intervals);
    }
    // Libelles des potards FX selon le type du slot selectionne + positions
    auto* pFxA = apvts.getRawParameterValue("arpfxtype");
    auto* pFxB = apvts.getRawParameterValue("arpfxtypeB");
    auto* pFxC = apvts.getRawParameterValue("arpfxtypeC");
    if (pFxA != nullptr && pFxB != nullptr && pFxC != nullptr)
    {
        int fx = (int) (fxSlotSel == 0 ? pFxA->load() : (fxSlotSel == 1 ? pFxB->load() : pFxC->load()));
        if (fx != lastFxType)
        {
            lastFxType = fx;
            arpFxLabel1.setText(ArpFX::knobName(fx, 0), juce::dontSendNotification);
            arpFxLabel2.setText(ArpFX::knobName(fx, 1), juce::dontSendNotification);
        }
        refreshFxKnobs();
    }
    // 3 LED de cible CRYSTALLIZE : reflet du parametre (donc des presets).
    if (auto* pT = apvts.getRawParameterValue("cryotarget"))
    {
        const int t = (int) pT->load();
        if (t != lastCryoTarget)
        {
            lastCryoTarget = t;
            for (int i = 0; i < 3; ++i)
                cryoLed[i].setToggleState(t == i + 1, juce::dontSendNotification);
        }
    }
    // LED de liaison RANDOMIZE/REROLL : reflet de l'etat du processeur (memoire
    // seule, donc toujours eteinte au chargement du plugin).
    {
        const bool liaison = proc.variationLinked.load();
        if (sampleLinkLed.getToggleState() != liaison)
            sampleLinkLed.setToggleState(liaison, juce::dontSendNotification);
    }
    // Le fader de bande suit les parametres, sauf pendant un drag (sinon ca se bat)
    if (! updatingBandFader && ! bandFader.isMouseOverOrDragging(false))
    {
        auto* pLo = apvts.getRawParameterValue("bandlow");
        auto* pHi = apvts.getRawParameterValue("bandhigh");
        if (pLo != nullptr && pHi != nullptr)
        {
            updatingBandFader = true;
            bandFader.setMinAndMaxValues((double) pLo->load(), (double) pHi->load(),
                                         juce::dontSendNotification);
            bandHiLabel.setText(formatFreq((double) pHi->load()), juce::dontSendNotification);
            bandLoLabel.setText(formatFreq((double) pLo->load()), juce::dontSendNotification);
            updatingBandFader = false;
        }
    }
    // Spectrogramme 2D : draine le FIFO seulement quand PRISME est visible.
    // (La vue RECORDER draine pour le relief 3D ; un seul consommateur.)
    if (! showingRecorder)
    {
        spectro.setSampleRate(proc.getSampleRate());
        spectro.pushFromFifo(proc.spectrumFifo, proc.spectrumBuf);
        if (spectro.update())
            content.repaint(0, 0, 1670, 52);

        // Oscilloscope 3D : FIFO dedie, donc aucun conflit avec spectro.
        // Le panneau SPECTRUM (y 58..278) n'etait jamais repeint par le
        // timer -> d'ou l'image figee. On repeint ce rectangle explicitement.
        scope3d.setSampleRate(proc.getSampleRate());
        scope3d.pushFromFifo(proc.scopeFifo, proc.scopeBuf);
        if (scope3d.update())
        {
            content.repaint(1046, 58, 608, 220);
            // Verification reelle (une seule fois) : prouve que des donnees
            // issues du DSP arrivent bien au panneau. Rien n'est simule.
            static bool scopeLogged = false;
            if (! scopeLogged)
            {
                scopeLogged = true;
                FILE* f = fopen("C:\\Temp\\prisme_boot.log", "a");
                if (f != nullptr)
                {
                    fprintf(f, "scope: %lld echantillons recus, crete=%.5f, echelle=%.5f\n",
                            (long long) scope3d.samplesSeen(),
                            (double) scope3d.observedPeak(),
                            (double) scope3d.displayScale());
                    fclose(f);
                }
            }
        }
    }
    // Panneau SAMPLES (UI principale) : suit la liste reelle des prises.
    rebuildSampleLines();
    content.repaint(1046, 300, 608, 566);
    // Recorder : suit l'etat meme quand la vue est masquee (prise en fond).
    // Finalise aussi la prise si la capacite max est atteinte.
    recorderView.refresh();
}

juce::String PrismeEditor::formatFreq(double hz)
{
    if (hz >= 1000.0)
        return juce::String(hz / 1000.0, 1) + "k";
    return juce::String((int) std::round(hz));
}

// Copie legere de proc.takeList vers sampleLines (panneau SAMPLES principal).
// Ne modifie rien : lecture seule. Recopie uniquement si la liste a change
// (taille ou selection) pour eviter une allocation a chaque tick.
void PrismeEditor::rebuildSampleLines()
{
    int count = (int) proc.takeList.size();
    int sel   = proc.selectedTake;
    // Index LISIBLE cote moteur : on ne le deduit plus de selectedTake, car
    // une SELECTION n'est pas une LECTURE. Fin naturelle de la pre-ecoute :
    // isPlaying() faux -> -1, le bouton repasse de lui meme sur PLAY.
    int prev  = proc.preview.isPlaying() ? proc.previewTakeIndex.load() : -1;
    if (count == sampleCacheCount && sel == sampleCacheSel && prev == sampleCachePreview)
        return;
    sampleCacheCount = count;
    sampleCacheSel   = sel;
    sampleCachePreview = prev;

    sampleLines.clear();
    sampleLines.reserve((size_t) count);
    for (int i = 0; i < count; ++i)
    {
        const auto& t = proc.takeList[(size_t) i];
        SampleLine sl;
        sl.number       = t.number;
        sl.name         = t.name;
        sl.durationSec  = t.durationSec;
        sl.peaks        = t.peaks;          // 120 cretes, ~0.5 ko
        sl.selected     = (sel == i);
        sl.fileMissing  = t.fileMissing;
        sl.isVariation  = t.isVariation;
        sl.seed         = t.seed;
        sl.intensityPct = t.intensityPct;
        sl.varBars      = t.varBars;
        sl.previewing   = (prev == i);
        sl.file         = t.file;            // chemin reel, pour le glisser
        sampleLines.push_back(std::move(sl));
    }
}

// Clic dans le tableau SAMPLES. Convertit (x,y) logiques en ligne + zone :
//  - zone PLAY/STOP  : bascule la pre-ecoute (takePreviewAt, verbe au moteur)
//  - zone SUPPR   : 1er clic arme, 2e clic confirme (takeRemoveAt)
//  - ailleurs     : selection de la prise (utile pour RANDOMIZE/REROLL)
// Geometrie du tableau SAMPLES, partagee entre le dessin (paintContent) et la
// hit-box des clics. Definie avant les deux usages pour rester la source unique.
namespace SampleTable
{
    constexpr int listX = 1062;   // marge gauche des lignes
    constexpr int listY = 362;    // premiere ligne (sous la barre de transport)
    constexpr int listW = 576;    // largeur d'une ligne
    constexpr int rowH  = 50;     // hauteur d'une ligne (40 -> 50 : onde agrandie)
    // Colonnes (relatives a listX) : bouts PLAY/STOP et SUPPRIMER a droite.
    constexpr int playStopRelX = 494;  // x relatif du bouton PLAY/STOP
    constexpr int delRelX   = 538;  // x relatif du bouton SUPPRIMER
    constexpr int btnW      = 40;   // largeur commune des 2 bouts
    constexpr int maxRows   = 10;   // lignes visibles (interieur 344..864 : 10 x 50)
    // Colonnes de texte et miniature d'onde. Le nom, la duree et la cellule
    // VARIATION cedent de la largeur a l'onde : l'info de variation reste
    // lisible dans sa cellule dediee, et le nom est tronque proprement.
    constexpr int nameRelX = 34;    // nom de la prise
    constexpr int nameRelW = 92;
    constexpr int durRelX  = 130;   // duree
    constexpr int varRelX  = 166;   // cellule VARIATION (seed / intensite)
    constexpr int varRelW  = 68;
    constexpr int waveRelX = 242;   // debut de l'onde (fin = playStopRelX - 6)
}

void PrismeEditor::handleSampleRowClickAt(int x, int y)
{
    using namespace SampleTable;
    if (x < listX || x >= listX + listW) return;
    int row = (y - listY) / rowH;
    if (y < listY || row < 0 || row >= (int) sampleLines.size() || row >= maxRows)
        return;

    const int idx = row;
    const int relX = x - listX;

    // Zone SUPPRIMER (a droite) : confirmation en 2 clics.
    if (relX >= delRelX)
    {
        if (sampleConfirmIdx == idx)
        {
            proc.takeRemoveAt(idx, true);   // suppression reelle (fichier inclus)
            sampleConfirmIdx = -1;
            sampleCacheCount = -1;          // force la recopie au prochain tick
            recorderView.refresh();
        }
        else
        {
            sampleConfirmIdx = idx;         // 1er clic = armement
        }
        content.repaint(listX, listY, listW, rowH * maxRows);
        return;
    }

    // Zone PLAY/STOP : bascule la pre-ecoute.
    if (relX >= playStopRelX)
    {
        proc.takePreviewAt(idx);
        sampleConfirmIdx = -1;
        recorderView.refresh();
        content.repaint(listX, listY, listW, rowH * maxRows);
        return;
    }

    // Ailleurs : selection de la prise (memorable pour RANDOMIZE/REROLL).
    sampleConfirmIdx = -1;
    proc.selectedTake = idx;
    recorderView.refresh();
    content.repaint(listX, listY, listW, rowH * maxRows);
}

// Ligne glissable sous (x,y). Le glisser s'arrete AVANT la colonne PLAY/STOP :
// ces deux boutons restent des clics purs, sinon un glisser declencherait une
// pre-ecoute ou une suppression par accident.
int PrismeEditor::sampleRowForDrag(int x, int y) const
{
    using namespace SampleTable;
    if (x < listX || x >= listX + playStopRelX)
        return -1;
    if (y < listY)
        return -1;

    const int row = (y - listY) / rowH;
    if (row < 0 || row >= maxRows || row >= (int) sampleLines.size())
        return -1;
    if (sampleLines[(size_t) row].fileMissing)
        return -1; // fichier absent : rien a glisser
    return row;
}

// Glisse une ligne du tableau vers le DAW. Copie locale refermee, puis
// glisser systeme (canMoveFiles=false : l'original n'est jamais bouge).
// Bloquant pendant tout le geste (boucle modale) : thread GUI uniquement.
void PrismeEditor::dragSampleRowToDAW(int row)
{
    if (row < 0 || row >= (int) sampleLines.size())
        return;

    const juce::File f = sampleLines[(size_t) row].file;
    if (f == juce::File() || ! f.existsAsFile())
        return;

    const juce::File drag = PrismeDragDrop::makeDraggableCopy(f);
    if (drag == juce::File())
        return; // copie impossible : pas de glissement, pas de surprise

    juce::DragAndDropContainer::performExternalDragDropOfFiles(
        juce::StringArray(drag.getFullPathName()), false, &content, nullptr);
}

void PrismeEditor::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &bandFader && ! updatingBandFader)
    {
        updatingBandFader = true;
        double lo = bandFader.getMinValue();
        double hi = bandFader.getMaxValue();
        if (auto* p = apvts.getParameter("bandlow"))
            p->setValueNotifyingHost(p->convertTo0to1((float) lo));
        if (auto* p = apvts.getParameter("bandhigh"))
            p->setValueNotifyingHost(p->convertTo0to1((float) hi));
        bandHiLabel.setText(formatFreq(hi), juce::dontSendNotification);
        bandLoLabel.setText(formatFreq(lo), juce::dontSendNotification);
        updatingBandFader = false;
    }
    // Potards FX partages : ecriture dans le slot selectionne (0/1/2)
    if ((slider == &arpFxKnob1 || slider == &arpFxKnob2) && ! updatingFxKnobs)
    {
        updatingFxKnobs = true;
        juce::String id = (slider == &arpFxKnob1)
            ? (fxSlotSel == 0 ? "arpfx1" : (fxSlotSel == 1 ? "arpfxB1" : "arpfxC1"))
            : (fxSlotSel == 0 ? "arpfx2" : (fxSlotSel == 1 ? "arpfxB2" : "arpfxC2"));
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1((float) slider->getValue()));
        updatingFxKnobs = false;
    }
}

void PrismeEditor::refreshFxKnobs()
{
    // Recale les 2 potards sur le slot selectionne (sans notifier)
    // + surlignage discret de la rangee active (sans nom)
    if (arpFxKnob1.isMouseOverOrDragging(false) || arpFxKnob2.isMouseOverOrDragging(false))
        return;
    updatingFxKnobs = true;
    juce::String id1 = fxSlotSel == 0 ? "arpfx1" : (fxSlotSel == 1 ? "arpfxB1" : "arpfxC1");
    juce::String id2 = fxSlotSel == 0 ? "arpfx2" : (fxSlotSel == 1 ? "arpfxB2" : "arpfxC2");
    if (auto* p = apvts.getRawParameterValue(id1))
        arpFxKnob1.setValue((double) p->load(), juce::dontSendNotification);
    if (auto* p = apvts.getRawParameterValue(id2))
        arpFxKnob2.setValue((double) p->load(), juce::dontSendNotification);
    juce::ComboBox* boxes[3] = { &arpFxBox, &arpFxBoxB, &arpFxBoxC };
    for (int i = 0; i < 3; ++i)
        boxes[i]->setColour(juce::ComboBox::backgroundColourId,
                            i == fxSlotSel ? activeSkin.panelBottom.interpolatedWith(activeSkin.accent, 0.3f)
                                           : activeSkin.comboBg);
    // KAY<3 : la pastille du slot actif est plus lumineuse (maquette).
    if (activeSkin.kay)
    {
        juce::TextButton* pads[3] = { &fxSlotBtn0, &fxSlotBtn1, &fxSlotBtn2 };
        for (int i = 0; i < 3; ++i)
            pads[i]->setColour(juce::TextButton::buttonColourId,
                               i == fxSlotSel ? activeSkin.accentHi
                                              : activeSkin.accentHi.withMultipliedBrightness(0.55f));
    }
    updatingFxKnobs = false;
}

void PrismeEditor::applySkin(int index)
{
    auto all = PrismeSkins::all();
    activeSkin = all[(size_t) juce::jlimit(0, (int) all.size() - 1, index)];
    const PrismeSkin& sk = activeSkin;
    hwLAF.setSkin(sk);

    // Combos
    juce::ComboBox* boxes[] = { &rootBox, &scaleBox, &octaveBox, &arpPatternBox, &arpDivBox,
        &arpOctBox, &arpFxBox, &arpFxBoxB, &arpFxBoxC, &arpRouteBox, &movDivBox, &presetBox, &skinBox };
    for (auto* cb : boxes)
    {
        cb->setColour(juce::ComboBox::backgroundColourId, sk.comboBg);
        cb->setColour(juce::ComboBox::textColourId, sk.textMain);
    }
    skinLabel.setColour(juce::Label::textColourId, sk.textDim);

    // Potards : etiquettes + afficheurs
    for (auto& k : knobs)
    {
        k->label.setColour(juce::Label::textColourId, sk.textMain);
        k->slider.setColour(juce::Slider::textBoxTextColourId, sk.readoutText);
        k->slider.setColour(juce::Slider::textBoxBackgroundColourId, sk.readoutBg);
    }
    // EQ
    for (int i = 0; i < 4; ++i)
    {
        eqLabels[i].setColour(juce::Label::textColourId, sk.textMain);
        eqSliders[i].setColour(juce::Slider::textBoxTextColourId, sk.readoutText);
        eqSliders[i].setColour(juce::Slider::textBoxBackgroundColourId, sk.readoutBg);
        eqSliders[i].setColour(juce::Slider::trackColourId, sk.trackBg);
        eqSliders[i].setColour(juce::Slider::thumbColourId, sk.accent);
    }
    // Titres
    titleLabel.setColour(juce::Label::textColourId, sk.textMain);
    // NIGHTCITY : le titre "P R I S M E" est dessine en degrade dans
    // paintContent (halo + gradient), donc le label est masque pour ce skin.
    titleLabel.setVisible(! sk.neon);
    // KAY<3 : le titre est remplace par le logo graffiti dessine dans paintContent.
    if (sk.kay) titleLabel.setVisible(false);
    // Meme chose pour le titre du panneau ARP : pour NIGHTCITY c'est paintContent
    // qui dessine les trois titres A R P / A R P F X / A R P E G G E.
    arpTitleLabel.setVisible(! sk.neon);
    footerLabel.setColour(juce::Label::textColourId, sk.textDim);
    arpTitleLabel.setColour(juce::Label::textColourId, sk.sectionTitle);
    movTitleLabel.setColour(juce::Label::textColourId, sk.sectionTitle);
    eqTitleLabel.setColour(juce::Label::textColourId, sk.sectionTitle);
    mixTitleLabel.setColour(juce::Label::textColourId, sk.sectionTitle);
    blurTitleLabel.setColour(juce::Label::textColourId, sk.sectionTitle);
    bandTitleLabel.setColour(juce::Label::textColourId, sk.sectionTitle);
    // Faders + curseurs lineaires
    juce::Slider* lin[] = { &mixFader, &blurFader, &bandFader, &givreSlider, &swingSlider };
    for (auto* s : lin)
    {
        s->setColour(juce::Slider::trackColourId, sk.trackBg);
        s->setColour(juce::Slider::thumbColourId, sk.accent);
        s->setColour(juce::Slider::textBoxTextColourId, sk.readoutText);
        s->setColour(juce::Slider::textBoxBackgroundColourId, sk.readoutBg);
    }
    // Etiquettes diverses
    juce::Label* labs[] = { &wetLabel, &dryLabel, &blurWetLabel, &blurDryLabel, &bandHiLabel,
        &bandLoLabel, &givreLabel, &swingLabel, &tailleLabel, &mirPredelayLabel, &mirColorLabel,
        &mirWidthLabel, &mirDecayLabel, &arpFxLabel1, &arpFxLabel2, &arpFxMixLabel, &arpRouteLabel, &cryoLabel };
    for (auto* lb : labs)
        lb->setColour(juce::Label::textColourId, sk.textMain);
    juce::ToggleButton* togs[] = { &arpOnBtn, &arpSyncBtn, &movSyncBtn, &autoKeyBtn };
    for (auto* tb : togs)
        tb->setColour(juce::ToggleButton::textColourId, sk.textMain);
    // Ligne presets + A/B
    presetNameEditor.setColour(juce::TextEditor::backgroundColourId, sk.comboBg);
    presetNameEditor.setColour(juce::TextEditor::textColourId, sk.textMain);
    presetNameEditor.setColour(juce::TextEditor::outlineColourId, sk.panelEdge);
    presetNameEditor.setColour(juce::TextEditor::focusedOutlineColourId, sk.accent);
    presetNameEditor.setColour(juce::CaretComponent::caretColourId, sk.accent);
    presetSaveBtn.setColour(juce::TextButton::buttonColourId, sk.buttonBg);
    presetSaveBtn.setColour(juce::TextButton::textColourOffId, sk.buttonText);
    randomBtn.setColour(juce::TextButton::buttonColourId, sk.buttonBg);
    randomBtn.setColour(juce::TextButton::textColourOffId, sk.buttonText);
    undoBtn.setColour(juce::TextButton::buttonColourId, sk.buttonBg);
    undoBtn.setColour(juce::TextButton::textColourOffId, sk.buttonText);
    redoBtn.setColour(juce::TextButton::buttonColourId, sk.buttonBg);
    redoBtn.setColour(juce::TextButton::textColourOffId, sk.buttonText);
    // Barre de transport du panneau SAMPLES
    sampleRecBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8a1f2e));
    sampleRecBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    sampleStopBtn.setColour(juce::TextButton::buttonColourId, sk.buttonBg);
    sampleStopBtn.setColour(juce::TextButton::textColourOffId, sk.buttonText);
    sampleRandomBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f5a8a));
    sampleRandomBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    sampleRerollBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f5a8a));
    sampleRerollBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    presetDeleteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a2e35));
    presetDeleteBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);

    // ---- KAY<3 : boutons "fond nuit + liseré" (maquette) ----
    if (sk.kay)
    {
        // Boutons neutres : liseré cyan, texte cyan.
        juce::TextButton* neutres[] = { &presetSaveBtn, &sampleStopBtn, &sampleRandomBtn,
                                        &sampleRerollBtn, &randomBtn, &recorderBtn };
        for (auto* b : neutres)
        {
            b->setColour(juce::TextButton::buttonColourId, sk.buttonBg);
            b->setColour(juce::TextButton::textColourOffId, sk.accent);
        }
        // REC : liseré magenta, texte magenta (le bouton n'est pas rempli).
        sampleRecBtn.setColour(juce::TextButton::buttonColourId, sk.accentHi);
        sampleRecBtn.setColour(juce::TextButton::textColourOffId, sk.accentHi.brighter(0.30f));
        // X (supprimer) : magenta plein, texte sombre.
        presetDeleteBtn.setColour(juce::TextButton::buttonColourId, sk.accentHi);
        presetDeleteBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff12061a));
        presetDeleteBtn.getProperties().set("kaySolid", true);
        // Pastilles des slots ARP FX : magenta (le slot actif est plus lumineux).
        juce::TextButton* pads[3] = { &fxSlotBtn0, &fxSlotBtn1, &fxSlotBtn2 };
        for (int i = 0; i < 3; ++i)
        {
            pads[i]->setColour(juce::TextButton::buttonColourId,
                               i == fxSlotSel ? sk.accentHi
                                              : sk.accentHi.withMultipliedBrightness(0.55f));
            pads[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        }
    }
    else
    {
        presetDeleteBtn.getProperties().remove("kaySolid");
        // Restaure la couleur d'origine des pastilles ARP FX pour les autres skins.
        juce::TextButton* pads[3] = { &fxSlotBtn0, &fxSlotBtn1, &fxSlotBtn2 };
        for (int i = 0; i < 3; ++i)
        {
            pads[i]->setColour(juce::TextButton::buttonColourId, juce::Colour(0xffc03028));
            pads[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        }
    }
    refreshSlotColours();
    // Pastilles
    pads.setColors(sk.accent, sk.accentHi, sk.panelBottom.brighter(0.08f));
    // Post Apo : spectrogramme et oscilloscope en vert phosphore (champs scope*).
    // ANALOG01 : spectre / oscilloscope bleus (champs scope*) sur fond bleu nuit.
    const bool useScope = sk.diy || sk.analog03 || sk.analog01;
    // ANALOG03 : spectre vert phosphore
    spectro.setColors(useScope ? sk.scopeDeep : sk.accentDeep, useScope ? sk.scopeMid : sk.accent,
                      useScope ? sk.scopeHot : sk.accentHi); // BISECTION (re-actif pour trace)
    scope3d.setColors(useScope ? sk.scopeDeep : sk.accentDeep, useScope ? sk.scopeMid : sk.accent,
                      useScope ? sk.scopeHot : sk.accentHi); // oscilloscope : meme charte
    // ---- POST APO : potards depareilles + texte clair sur grille / ecrans ----
    // Dessin seulement : les parametres et leurs attachements ne changent pas.
    // Ordre des potards (voir addKnob) : 6 = Entree, 12 = Unisson.
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& props = knobs[i]->slider.getProperties();
        if (sk.diy && i == 6)        props.set("diyKnob", "bakelite");
        else if (sk.diy && i == 12)  props.set("diyKnob", "chrome");
        else                         props.remove("diyKnob");
        knobs[i]->slider.repaint();
    }
    if (sk.diy)
    {
        const juce::Colour light = sk.textOnWidget;
        for (auto* cb : boxes)
            cb->setColour(juce::ComboBox::textColourId, light);
        for (size_t i = 9; i < knobs.size(); ++i)          // panneau MOVEMENT (sur la grille)
            knobs[i]->label.setColour(juce::Label::textColourId, light);
        for (int i = 0; i < 4; ++i)
            eqLabels[i].setColour(juce::Label::textColourId, light);   // panneau EQ (grille)
        movTitleLabel.setColour(juce::Label::textColourId, light);
        eqTitleLabel.setColour(juce::Label::textColourId, light);
        movSyncBtn.setColour(juce::ToggleButton::textColourId, light);
        titleLabel.setColour(juce::Label::textColourId, light);
        footerLabel.setColour(juce::Label::textColourId, light.withAlpha(0.75f));
        skinLabel.setColour(juce::Label::textColourId, light);
        presetNameEditor.setColour(juce::TextEditor::textColourId, light);
    }
    // Recorder : memes couleurs que le skin actif + bouton de bascule.
    recorderBtn.setColour(juce::TextButton::buttonColourId, sk.buttonBg);
    recorderBtn.setColour(juce::TextButton::textColourOffId, sk.buttonText);
    recorderView.applySkin(sk);

    resized();   // NIGHTCITY : la disposition des controles depend du skin (zone ARP)
    repaint();
    content.repaint();
}

void PrismeEditor::showRecorder(bool show)
{
    // ---- ACCES A LA PARTIE RECORDER RETIRE ----
    // L'utilisateur ne veut plus y acceder : aucune ouverture possible.
    // Seule la fermeture reste autorisee (etat initial a la construction).
    if (show)
        return;

    // Bascule complete : meme fenetre, memes dimensions, meme conteneur.
    // On masque/affiche les enfants de content, sans les deplacer ni les
    // detruire. Les parametres audio, l'arpege et les presets sont intacts.
    // La capture continue en arriere-plan (moteur dans le processeur).
    showingRecorder = show;
    for (int i = 0; i < content.getNumChildComponents(); ++i)
    {
        auto* c = content.getChildComponent(i);
        if (c == &recorderView)
            c->setVisible(show);
        else if (c == &recorderBtn)
            c->setVisible(false);   // entree retiree : jamais affiche
        else
            c->setVisible(! show);
    }
    if (show)
        recorderView.refresh();
    else
    {
        // NIGHTCITY : ces deux libelles sont remplaces par un dessin dans
        // paintContent ; la boucle ci-dessus les aurait rendus visibles.
        // KAY<3 : le titre est le logo graffiti dessine dans paintContent.
        const bool dessine = activeSkin.neon || activeSkin.kay;
        titleLabel.setVisible(! dessine);
        arpTitleLabel.setVisible(! dessine);
    }
    content.repaint();
}

void PrismeEditor::addKnob(const juce::String& paramID, const juce::String& name)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    k->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    k->slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff7df9ff));
    k->slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2c2f4a));
    k->slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    k->slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffffcf7d));
    k->slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff101114));
    content.addAndMakeVisible(k->slider);

    k->label.setText(name, juce::dontSendNotification);
    k->label.setJustificationType(juce::Justification::centred);
    k->label.setColour(juce::Label::textColourId, juce::Colour(0xffb9bec8));
    k->label.setFont(juce::Font(12.0f, juce::Font::bold));
    content.addAndMakeVisible(k->label);

    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, k->slider);
    knobs.push_back(std::move(k));
}

void PrismeEditor::applyPreset(int index, int root, int scale, int oct, float decay, float attack, float bright,
                               float irid, float shimmer, float spread, float mix)
{
    applyingPreset = true;
    auto setParam = [this](const juce::String& id, float v)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(v));
    };
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("root")))
        p->setValueNotifyingHost((float)root / (float)(p->choices.size() - 1));
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("scale")))
        p->setValueNotifyingHost((float)scale / (float)(p->choices.size() - 1));
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("octave")))
        p->setValueNotifyingHost((float)oct / 2.0f);

    setParam("decay", decay);
    setParam("attack", attack);
    setParam("brightness", bright);
    setParam("iridescence", irid);
    setParam("shimmer", shimmer);
    setParam("spread", spread);
    setParam("mix", mix);
    setParam("freezemix", 0.0f);
    setParam("arpswing", 0.0f);
    setParam("crystallize", 0.0f);
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("arpoctaves")))
        p->setValueNotifyingHost(0.0f);
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("arpfxtype")))
        p->setValueNotifyingHost(0.0f);
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("arpfxtypeB")))
        p->setValueNotifyingHost(0.0f);
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("arpfxtypeC")))
        p->setValueNotifyingHost(0.0f);
    if (auto* p = apvts.getParameter("arpon"))
        p->setValueNotifyingHost(0.0f);
    applyingPreset = false;
    setActivePreset(index);
}

void PrismeEditor::setActivePreset(int menuId)
{
    activePreset = menuId;
    presetBox.setSelectedId(menuId, juce::dontSendNotification);
}

void PrismeEditor::randomizeSound()
{
    // Melange les parametres de son. Volumes preserves : mix, ingain, outgain.
    // Pas de garde applyingPreset : le menu preset repasse sur "Choisir...".
    // NOTE : reference vers le generateur partage (pas de copie :
    // une copie repartirait du meme etat et donnerait les memes valeurs).
    juce::Random& r = juce::Random::getSystemRandom();
    auto setParam = [this](const juce::String& id, float v)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(v));
    };
    auto setChoice = [this](const juce::String& id, int index)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(id)))
            p->setValueNotifyingHost((float) index / (float)(p->choices.size() - 1));
    };
    setChoice("root", r.nextInt(12));
    setChoice("scale", r.nextInt((int) PrismeScales::getScales().size()));
    setChoice("octave", r.nextInt(3));
    setParam("decay", 0.5f + r.nextFloat() * 6.5f);
    setParam("attack", 0.005f + r.nextFloat() * r.nextFloat() * 1.5f);
    setParam("brightness", 0.1f + r.nextFloat() * 0.9f);
    setParam("iridescence", 0.3f + r.nextFloat() * 1.2f);
    setParam("shimmer", r.nextFloat());
    setParam("spread", r.nextFloat());
    setParam("freezemix", 0.0f);
    setChoice("arppattern", r.nextInt(5));
    setChoice("arpdiv", r.nextInt(6));
    setParam("arprate", 1.0f + r.nextFloat() * 9.0f);
    if (auto* p = apvts.getParameter("arpon"))
        p->setValueNotifyingHost(r.nextFloat() < 0.3f ? 1.0f : 0.0f);
    if (r.nextFloat() < 0.4f)
    {
        setChoice("arpfxtype", 1 + r.nextInt(5));
        setParam("arpfx1", r.nextFloat());
        setParam("arpfx2", r.nextFloat());
    }
    else
    {
        setChoice("arpfxtype", 0);
    }
    if (r.nextFloat() < 0.4f)
    {
        setChoice("arpfxtypeB", 1 + r.nextInt(5));
        setParam("arpfxB1", r.nextFloat());
        setParam("arpfxB2", r.nextFloat());
    }
    else
    {
        setChoice("arpfxtypeB", 0);
    }
    if (r.nextFloat() < 0.4f)
    {
        setChoice("arpfxtypeC", 1 + r.nextInt(5));
        setParam("arpfxC1", r.nextFloat());
        setParam("arpfxC2", r.nextFloat());
    }
    else
    {
        setChoice("arpfxtypeC", 0);
    }
    setParam("arpfxmix", 0.5f + r.nextFloat() * 0.5f);
    if (r.nextFloat() < 0.35f)
        setParam("crystallize", 0.15f + r.nextFloat() * 0.75f);
    else
        setParam("crystallize", 0.0f);
    setParam("movrate", 0.1f + r.nextFloat() * 8.0f);
    setParam("movdepth", r.nextFloat() * 0.9f);
    setParam("movshape", r.nextFloat());
    setParam("movunison", r.nextFloat());
    setChoice("movdiv", r.nextInt(6));
    setParam("eqlow", -6.0f + r.nextFloat() * 12.0f);
    setParam("eqlowmid", -6.0f + r.nextFloat() * 12.0f);
    setParam("eqhighmid", -6.0f + r.nextFloat() * 12.0f);
    setParam("eqhigh", -6.0f + r.nextFloat() * 12.0f);
    setParam("blurmix", r.nextFloat() * 0.7f);
    setParam("blursize", 0.3f + r.nextFloat() * 0.7f);
}

juce::File PrismeEditor::getUserPresetDir()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Atelier Prisme").getChildFile("Prisme").getChildFile("UserPresets");
}

void PrismeEditor::refreshPresetList()
{
    int keepSelected = presetBox.getSelectedId();
    presetBox.clear(juce::dontSendNotification);
    presetBox.addItem("Verre Clair", 1);
    presetBox.addItem("Aurore", 2);
    presetBox.addItem("Cathedrale", 3);
    presetBox.addItem("Fracture", 4);
    presetBox.addSeparator();
    presetBox.addSectionHeading("User preset");

    userPresetNames.clear();
    userPresetFiles.clear();
    auto dir = getUserPresetDir();
    if (dir.isDirectory())
    {
        juce::Array<juce::File> files;
        dir.findChildFiles(files, juce::File::findFiles, false, "*.prisme");
        files.sort();
        for (int i = 0; i < files.size(); ++i)
        {
            userPresetFiles.add(files[i]);
            userPresetNames.add(files[i].getFileNameWithoutExtension());
            presetBox.addItem(files[i].getFileNameWithoutExtension(), 100 + i);
        }
    }
    // Restaure la selection si elle existe encore
    if (keepSelected >= 1 && keepSelected <= 4)
        presetBox.setSelectedId(keepSelected, juce::dontSendNotification);
    else if (keepSelected >= 100 && keepSelected - 100 < userPresetFiles.size())
        presetBox.setSelectedId(keepSelected, juce::dontSendNotification);
}

void PrismeEditor::onPresetChosen()
{
    int id = presetBox.getSelectedId();
    if (id >= 1 && id <= 4)
    {
        static const struct { int root, scale, oct; float decay, attack, bright, irid, shimmer, spread, mix; } fac[4] = {
            { 0, 0, 1, 1.2f, 0.010f, 0.85f, 0.8f, 0.20f, 0.70f, 0.45f },
            { 5, 6, 1, 3.2f, 0.150f, 0.65f, 1.1f, 0.55f, 0.90f, 0.60f },
            { 7, 1, 0, 6.5f, 0.600f, 0.40f, 1.2f, 0.30f, 0.85f, 0.70f },
            { 2, 20, 1, 2.0f, 0.008f, 1.0f, 1.4f, 0.80f, 1.0f, 0.75f },
        };
        auto& f = fac[id - 1];
        applyPreset(id, f.root, f.scale, f.oct, f.decay, f.attack, f.bright, f.irid, f.shimmer, f.spread, f.mix);
    }
    else if (id >= 100 && id - 100 < userPresetFiles.size())
    {
        applyingPreset = true;
        int sk = apvts.state.getProperty("uiSkin", 2);
        std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(userPresetFiles[id - 100]));
        if (xml != nullptr)
        {
            juce::ValueTree loaded = juce::ValueTree::fromXml(*xml);
            if (loaded.isValid() && loaded.hasType(apvts.state.getType()))
                apvts.replaceState(loaded);
        }
        apvts.state.setProperty("uiSkin", sk, nullptr);
        applyingPreset = false;
        setActivePreset(id);
    }
}

void PrismeEditor::saveUserPreset()
{
    juce::String name = presetNameEditor.getText().trim();
    if (name.isEmpty())
        name = "Sans nom";
    // Nom de fichier sur : retire les caracteres interdits
    juce::String safe = name;
    for (auto c : { '\\', '/', ':', '*', '?', '"', '<', '>', '|' })
        safe = safe.replaceCharacter(c, '_');
    safe = safe.trim();
    if (safe.isEmpty())
        safe = "Sans nom";

    auto dir = getUserPresetDir();
    dir.createDirectory();
    auto file = dir.getChildFile(safe + ".prisme");

    applyingPreset = true;
    std::unique_ptr<juce::XmlElement> xml(apvts.copyState().createXml());
    bool ok = (xml != nullptr) && file.replaceWithText(xml->toString());
    applyingPreset = false;

    if (ok)
    {
        presetNameEditor.clear();
        refreshPresetList();
        // Selectionne le preset qu'on vient de sauver
        int idx = userPresetNames.indexOf(safe);
        if (idx >= 0)
            setActivePreset(100 + idx);
    }
}

void PrismeEditor::deleteSelectedUserPreset()
{
    int id = presetBox.getSelectedId();
    if (id >= 100 && id - 100 < userPresetFiles.size())
    {
        userPresetFiles[id - 100].deleteFile();
        activePreset = -1;
        refreshPresetList();
        presetBox.setSelectedId(0, juce::dontSendNotification);
    }
}

void PrismeEditor::selectSlot(int slot)
{
    if (slot == activeSlot || (slot != 0 && slot != 1))
        return;
    // Sauve le son actuel dans l'emplacement qu'on quitte, charge l'autre.
    // Le skin est preserve (preference UI, pas parametre audio).
    int sk = apvts.state.getProperty("uiSkin", 2);
    if (activeSlot == 0) slotA = apvts.copyState();
    else slotB = apvts.copyState();
    activeSlot = slot;
    apvts.replaceState((slot == 0 ? slotA : slotB).createCopy());
    apvts.state.setProperty("uiSkin", sk, nullptr);
    refreshSlotColours();
}

void PrismeEditor::refreshSlotColours()
{
    juce::TextButton* buttons[2] = { &slotABtn, &slotBBtn };
    for (int i = 0; i < 2; ++i)
    {
        bool active = (i == activeSlot);
        buttons[i]->setColour(juce::TextButton::buttonColourId,
                              active ? activeSkin.buttonActive : activeSkin.buttonBg);
        buttons[i]->setColour(juce::TextButton::textColourOffId,
                              active ? activeSkin.buttonActiveText : activeSkin.buttonText);
        // KAY<3 : le slot actif est rempli plein (le LookAndFeel lit cette propriete).
        buttons[i]->getProperties().set("kaySolid", active && activeSkin.kay);
    }
    slotCopyBtn.setColour(juce::TextButton::buttonColourId, activeSkin.buttonBg);
    slotCopyBtn.setColour(juce::TextButton::textColourOffId,
                          activeSkin.kay ? activeSkin.accent : activeSkin.buttonText);
}

void PrismeEditor::copySlotToOther()
{
    // Fige le son actuel (live) dans l'autre emplacement
    if (activeSlot == 0) slotB = apvts.copyState();
    else slotA = apvts.copyState();
}

void PrismeEditor::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& property)
{
    // La preference de skin ne salit jamais la selection de preset
    if (property.toString() == "uiSkin")
        return;
    // Tout changement manuel eteint le preset actif (sauf pendant son application)
    if (! applyingPreset)
        triggerAsyncUpdate();
}

void PrismeEditor::handleAsyncUpdate()
{
    // Reglage manuel : le menu revient sur "Choisir un preset..."
    activePreset = -1;
    presetBox.setSelectedId(0, juce::dontSendNotification);
}

void PrismeContent::paint(juce::Graphics& g)
{
    static_cast<PrismeEditor&>(owner).paintContent(g);
}

void PrismeContent::paintOverChildren(juce::Graphics& g)
{
    static_cast<PrismeEditor&>(owner).paintContentOver(g);
}

void PrismeEditor::paintContentOver(juce::Graphics& g)
{
    // Decor bricole au-dessus des controles (zones libres uniquement).
    // Autres skins : aucun effet.
    if (activeSkin.diy && ! showingRecorder)
        PrismeDiy::paintOverlay(g);

    if (! showingRecorder)
        paintLiveWaveWindow(g);
}

// Fenetre d'onde EN DIRECT dans le panneau SAMPLES : sous la derniere prise (ou sous
// "Aucune prise enregistree"). Si la liste est pleine, elle s'affiche en bas uniquement
// pendant un enregistrement. Donnees : pics calcules par la vue Recorder (meme source).
void PrismeEditor::paintLiveWaveWindow(juce::Graphics& g)
{
    using namespace SampleTable;
    const int st = proc.recorder.state.load();
    const bool waiting = (st == RecorderEngine::WaitingBar);
    const bool recording = (st == RecorderEngine::Recording);
    const bool active = waiting || recording;

    const int rows = juce::jmin((int) sampleLines.size(), maxRows);
    const int h = 64, bottom = 858;
    int y = (rows == 0) ? (listY + 52) : (listY + rows * rowH + 8);
    if (y + h > bottom)
    {
        if (! active)
            return;              // liste pleine : fenetre seulement pendant le REC
        y = bottom - h;          // superposee en bas pendant l'enregistrement
    }

    const juce::Rectangle<float> a((float) listX, (float) y, (float) listW, (float) h);
    const juce::Colour tDim = activeSkin.diy ? activeSkin.textOnWidget.withAlpha(0.65f) : activeSkin.textDim;
    const juce::Colour hot(0xffff5e5e);

    g.setColour(juce::Colours::black.withAlpha(active ? 0.60f : 0.35f));
    g.fillRoundedRectangle(a, 6.0f);
    g.setColour(recording ? hot : activeSkin.accent.withAlpha(active ? 0.9f : 0.45f));
    g.drawRoundedRectangle(a, 6.0f, 1.2f);

    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.setColour(recording ? hot : tDim);
    g.drawText(recording ? "REC  -  FORME D'ONDE EN DIRECT"
                         : (waiting ? "EN ATTENTE DU TEMPS 1 DE LA MESURE..."
                                    : "FORME D'ONDE EN DIRECT"),
               (int) a.getX() + 10, (int) a.getY() + 4, (int) a.getWidth() - 20, 12,
               juce::Justification::centredLeft);

    const juce::Rectangle<float> wv = a.reduced(8.0f, 0.0f).withTrimmedTop(18.0f).withTrimmedBottom(6.0f);
    const float mid = wv.getCentreY(), half = wv.getHeight() * 0.5f;
    g.setColour(tDim.withAlpha(0.35f));
    g.drawHorizontalLine((int) mid, wv.getX(), wv.getRight());

    const auto& pk = recorderView.livePeaks();
    const int n = (int) pk.size();
    if (n > 0)
    {
        g.setColour(recording ? hot : activeSkin.accent);
        const float colW = wv.getWidth() / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const float v = juce::jlimit(0.0f, 1.0f, pk[(size_t) i]);
            if (v <= 0.002f) continue;
            g.fillRect(wv.getX() + (float) i * colW, mid - v * half,
                       juce::jmax(1.0f, colW), juce::jmax(1.0f, 2.0f * v * half));
        }
    }
}

void PrismeContent::mouseDown(const juce::MouseEvent& e)
{
    auto& ed = static_cast<PrismeEditor&>(owner);

    // Le glisser s'arme UNIQUEMENT hors des boutons PLAY/STOP et SUPPRIMER :
    // un clic sur ces-la ne doit jamais partir en glisser.
    dragStartPos = e.position;
    dragRow      = ed.sampleRowForDrag(e.x, e.y);
    dragArmed    = (dragRow >= 0);

    // Comportement du clic, inchange : selection / pre-ecoute / suppression.
    ed.handleSampleRowClickAt(e.x, e.y);
}

void PrismeContent::mouseDrag(const juce::MouseEvent& e)
{
    if (! dragArmed)
        return;

    // Seuil : un simple clic ne glisse jamais (memes 8 px que le Recorder).
    if ((e.position - dragStartPos).getDistanceFromOrigin() < 8.0f)
        return;

    const int row = dragRow;
    dragArmed = false;  // avant l'appel : la boucle modale re-entrante est ignoree
    dragRow   = -1;

    static_cast<PrismeEditor&>(owner).dragSampleRowToDAW(row);
}

void PrismeContent::mouseUp(const juce::MouseEvent&)
{
    dragArmed = false;
    dragRow   = -1;
}

void PrismeEditor::paint(juce::Graphics& g)
{
    // Fond uni plein format (les panneaux vivent dans le calque mis a l'echelle)
    g.fillAll(activeSkin.windowBg);
}

void PrismeEditor::paintContent(juce::Graphics& g)
{
    // Tout le dessin historique vit ici, en coordonnees logiques 880x970.
    if (activeSkin.diy)
        PrismeDiy::paintBackdrop(g, activeSkin);   // grille rouillee + electronique
    else if (activeSkin.analog03)
    {
        // ANALOG03 : fond bois walnut realiste. Texture pleine page (1680x945)
        // dessinee UNE seule fois : aucun motif repetitif, aucun raccord visible.
        const juce::Image& bg = PrismeTextures::boisDark();
        if (bg.isValid())
            g.drawImage(bg, 0.0f, 0.0f, 1670.0f, 941.0f,
                        0, 0, bg.getWidth(), bg.getHeight());
        else
            g.fillAll(activeSkin.windowBg);
    }
    else if (activeSkin.analog01)
    {
        // ANALOG01 : acier bross platine / argent froid (reference).
        // 1. Degrade platine = la couleur de la maquette (#A6ACAF -> #6E767B).
        // 2. Grain bross : texture metal brossi tuilee a taille native
        //    (512x512, pas d'etirement) en transparence 0.55 : le grain se
        //    marie au degrade. Diff de raccord ~6/255 apres transparence,
        //    invisible a l'oeil -> aucun motif repetitif perceptible.
        juce::ColourGradient plat(juce::Colour(0xffA6ACAF), 0.0f, 0.0f,
                                  juce::Colour(0xff6E767B), 0.0f, 941.0f, false);
        g.setGradientFill(plat);
        g.fillAll();
        const juce::Image& brushed = PrismeTextures::metalBrushed();
        if (brushed.isValid())
        {
            g.setTiledImageFill(brushed, 0, 0, 0.55f);
            g.fillRect(0, 0, 1670, 941);
            g.setColour(juce::Colours::white);   // reset du pinceau
        }
    }
    else if (activeSkin.hasTextures)
    {
        if (activeSkin.metalV2)
        {
            // Fond metal martelé (panel_metal_large.png, seamless 1200x390)
            // Tuile a taille native pour preserver le relief martele.
            const juce::Image& bg = PrismeTextures::panelMetalLarge();
            if (bg.isValid())
            {
                const int tw = bg.getWidth(), th = bg.getHeight();
                for (int yy = 0; yy < 941; yy += th)
                    for (int xx = 0; xx < 1670; xx += tw)
                        g.drawImage(bg, (float) xx, (float) yy, (float) tw, (float) th,
                                    0, 0, tw, th);
            }
            else
                g.fillAll(activeSkin.windowBg);
        }
        else
        {
            // MINEPRISME : fond metal martelé (panel_metal_large.png, seamless)
            const juce::Image& bg = PrismeTextures::panelMetalLarge();
            if (bg.isValid())
            {
                const int tw = bg.getWidth(), th = bg.getHeight();
                for (int yy = 0; yy < 941; yy += th)
                    for (int xx = 0; xx < 1670; xx += tw)
                        g.drawImage(bg, (float) xx, (float) yy, (float) tw, (float) th,
                                    0, 0, tw, th);
            }
            else
                g.fillAll(activeSkin.windowBg);
        }
    }
    else
        g.fillAll(activeSkin.windowBg);
    // KAY<3 : fond grunge (eclaboussures + rayures) par-dessus le fond uni.
    if (activeSkin.kay)
    {
        juce::Random rng(0x5b2d4f1a);
        g.setColour(juce::Colour(0xff9a1f6e).withAlpha(0.06f));
        for (int i = 0; i < 20; ++i)
        {
            const float cx = rng.nextFloat() * 1670.0f;
            const float cy = rng.nextFloat() * 941.0f;
            const float r  = 5.0f + rng.nextFloat() * 30.0f;
            g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        }
        g.setColour(juce::Colour(0xff28e1ff).withAlpha(0.04f));
        for (int i = 0; i < 14; ++i)
        {
            const float cx = rng.nextFloat() * 1670.0f;
            const float cy = rng.nextFloat() * 941.0f;
            const float r  = 3.0f + rng.nextFloat() * 22.0f;
            g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        }
        g.setColour(juce::Colours::white.withAlpha(0.013f));
        for (int i = 0; i < 7; ++i)
        {
            const float yy = 60.0f + rng.nextFloat() * 820.0f;
            g.fillRect(0.0f, yy, 1670.0f, 1.0f);
        }
    }
    {
        static bool logged = false;
        if (! logged)
        {
            logged = true;
            FILE* f = fopen("C:\\Temp\\prisme_boot.log", "a");
            if (f != nullptr) { fputs("editor: paintContent\n", f); fclose(f); }
        }
    }

    // Spectrogramme bandeau haut (pleine largeur 1670)
    spectro.draw(g, juce::Rectangle<float>(0.0f, 0.0f, 1670.0f, 52.0f));
    // Pastille sombre sous le titre pour garder PRISME lisible
    g.setColour(juce::Colours::black.withAlpha(activeSkin.neon ? 0.72f : 0.55f));
    g.fillRoundedRectangle(640.0f, 4.0f, 390.0f, 44.0f, 10.0f);
    if (activeSkin.neon)
    {
        // NIGHTCITY : cadre lumineux + titre en degrade cyan -> magenta.
        // (Le label titre est masque pour ce skin : voir applySkin.)
        g.setColour(activeSkin.accent.withAlpha(0.18f));
        g.drawRoundedRectangle(640.0f, 4.0f, 390.0f, 44.0f, 10.0f, 4.0f);
        g.setColour(activeSkin.accent.withAlpha(0.55f));
        g.drawRoundedRectangle(640.0f, 4.0f, 390.0f, 44.0f, 10.0f, 1.2f);

        juce::GlyphArrangement ga;
        ga.addLineOfText(juce::Font(32.0f, juce::Font::bold), "P R I S M E", 0.0f, 0.0f);
        juce::Path titlePath;
        ga.createPath(titlePath);
        const auto tb = titlePath.getBounds();
        titlePath.applyTransform(juce::AffineTransform::translation(835.0f - tb.getCentreX(),
                                                                    26.0f - tb.getCentreY()));
        juce::DropShadow ds(activeSkin.accent.withAlpha(0.75f), 7, juce::Point<int>(0, 0));
        ds.drawForPath(g, titlePath);
        juce::ColourGradient tgrad(activeSkin.accent,    660.0f, 0.0f,
                                   activeSkin.accentHi, 1010.0f, 0.0f, false);
        g.setGradientFill(tgrad);
        g.fillPath(titlePath);
    }

    // Petit polygone ferme a partir d'un tableau de points
    auto poly = [](juce::Path& p, const juce::Point<float>* pts, int n)
    {
        if (n <= 0) return;
        p.startNewSubPath(pts[0]);
        for (int i = 1; i < n; ++i) p.lineTo(pts[i]);
        p.closeSubPath();
    };
    auto fillPoly = [&g, &poly](std::initializer_list<juce::Point<float>> list, juce::Colour col)
    {
        std::vector<juce::Point<float>> v(list);
        juce::Path p;
        poly(p, v.data(), (int) v.size());
        g.setColour(col);
        g.fillPath(p);
    };
    auto strokePoly = [&g, &poly](std::initializer_list<juce::Point<float>> list, juce::Colour col, float thick)
    {
        std::vector<juce::Point<float>> v(list);
        juce::Path p;
        poly(p, v.data(), (int) v.size());
        g.setColour(col);
        g.strokePath(p, juce::PathStrokeType(thick));
    };
    // Petit cluster de quartz bleu (aura aqua) de part et d'autre du titre
    auto drawCrystal = [&g, &poly](float cx, float baseY, float w, float h, float tiltDeg,
                            juce::Colour deep, juce::Colour mid, juce::Colour light)
    {
        (void) light;
        juce::AffineTransform t = juce::AffineTransform::translation(cx, baseY)
                                .rotated(tiltDeg * 3.14159265f / 180.0f);
        auto P = [&](float lx, float ly) { return juce::Point<float>(lx, ly).transformedBy(t); };

        // Halo
        g.setColour(juce::Colour(0xff7df9ff).withAlpha(0.10f));
        auto gc = juce::Point<float>(0.0f, -h * 0.5f).transformedBy(t);
        g.fillEllipse(gc.x - w * 1.3f, gc.y - h * 0.6f, w * 2.6f, h * 1.2f);

        // Corps : prisme hexagonal + pointe
        juce::Path body;
        {
            juce::Point<float> pts[5] = { P(-w * 0.5f, 0), P(w * 0.5f, 0), P(w * 0.5f, -h * 0.62f),
                                          P(0.0f, -h), P(-w * 0.5f, -h * 0.62f) };
            poly(body, pts, 5);
        }
        // Note: gradient 2 points (clair -> profond)
        juce::ColourGradient fill2(mid, P(0.0f, -h), deep, P(0.0f, 0.0f), false);
        g.setGradientFill(fill2);
        g.fillPath(body);
        // Facette gauche sombre + bande centrale claire
        juce::Path left;
        {
            juce::Point<float> pts[4] = { P(-w * 0.5f, 0), P(-w * 0.12f, 0), P(-w * 0.12f, -h * 0.72f), P(-w * 0.5f, -h * 0.62f) };
            poly(left, pts, 4);
        }
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillPath(left);
        juce::Path stripe;
        {
            juce::Point<float> pts[5] = { P(-w * 0.10f, 0), P(w * 0.16f, 0), P(w * 0.10f, -h * 0.66f), P(0.0f, -h), P(-w * 0.06f, -h * 0.68f) };
            poly(stripe, pts, 5);
        }
        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.fillPath(stripe);
        // Aretes blanches
        g.setColour(juce::Colours::white.withAlpha(0.75f));
        juce::Path edges;
        {
            juce::Point<float> pts[5] = { P(-w * 0.5f, 0), P(w * 0.5f, 0), P(w * 0.5f, -h * 0.62f),
                                          P(0.0f, -h), P(-w * 0.5f, -h * 0.62f) };
            poly(edges, pts, 5);
        }
        g.strokePath(edges, juce::PathStrokeType(1.1f));
        {
            juce::Point<float> t0 = P(0.0f, -2.0f), t1 = P(0.0f, -h + 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.drawLine(t0.x, t0.y, t1.x, t1.y, 1.0f);
        }
    };

    auto drawCluster = [&](float cx, float baseY, float s, bool mirror)
    {
        float m = mirror ? -1.0f : 1.0f;
        // Socle rocheux
        juce::Path rock;
        {
            juce::Point<float> pts[4] = { juce::Point<float>(cx - 34 * s, baseY), juce::Point<float>(cx + 34 * s, baseY),
                                          juce::Point<float>(cx + 22 * s, baseY - 9 * s), juce::Point<float>(cx - 22 * s, baseY - 9 * s) };
            poly(rock, pts, 4);
        }
        g.setColour(juce::Colour(0xff14161c));
        g.fillPath(rock);
        g.setColour(juce::Colour(0xff4a4e56));
        g.strokePath(rock, juce::PathStrokeType(1.0f));
        drawCrystal(cx - 22 * s * m, baseY - 6 * s, 13 * s, 20 * s, -14 * m, juce::Colour(0xff1e3f8f), juce::Colour(0xff3f7fd0), juce::Colour(0xffbfe6ff));
        drawCrystal(cx + 2 * s * m, baseY - 6 * s, 16 * s, 34 * s, -5 * m, juce::Colour(0xff1a3a80), juce::Colour(0xff2f6fc4), juce::Colour(0xff9fd8ff));
        drawCrystal(cx + 24 * s * m, baseY - 6 * s, 14 * s, 25 * s, 11 * m, juce::Colour(0xff24479a), juce::Colour(0xff4f96e0), juce::Colour(0xffcdf0ff));
    };

    // (clusters dessines en fin de paint, au premier plan)
    auto drawScrews = [&g, this](int x, int y, int w, int h)
    {
        // NIGHTCITY : panneaux sans vis, comme la maquette.
        if (activeSkin.neon) return;

        const int cx[4] = { x + 11, x + w - 11, x + 11, x + w - 11 };
        const int cy[4] = { y + 11, y + 11, y + h - 11, y + h - 11 };

        // Peau texturée : vis en PNG du pack (avec ombre portée discrete)
        if (activeSkin.hasTextures)
        {
            const juce::Image& sc = PrismeTextures::screw();
            if (sc.isValid())
            {
                const float sz = 13.0f;
                for (int i = 0; i < 4; ++i)
                {
                    const float sx = (float) cx[i] - sz * 0.5f;
                    const float sy = (float) cy[i] - sz * 0.5f;
                    g.setColour(juce::Colours::black.withAlpha(0.35f));
                    g.fillEllipse(sx + 1.0f, sy + 1.5f, sz, sz);
                    g.drawImage(sc, sx, sy, sz, sz, 0, 0, sc.getWidth(), sc.getHeight());
                }
                return;
            }
        }

        for (int i = 0; i < 4; ++i)
        {
            juce::ColourGradient sg(activeSkin.screwHi, (float) cx[i] - 3.0f, (float) cy[i] - 3.0f,
                                    activeSkin.screwLo, (float) cx[i] + 3.0f, (float) cy[i] + 3.0f, false);
            g.setGradientFill(sg);
            g.fillEllipse((float) cx[i] - 4.5f, (float) cy[i] - 4.5f, 9.0f, 9.0f);
            g.setColour(activeSkin.panelBottom);
            g.drawEllipse((float) cx[i] - 4.5f, (float) cy[i] - 4.5f, 9.0f, 9.0f, 1.0f);
            float a = 0.5f + (float) i * 0.9f;
            g.setColour(juce::Colour(0xff141518));
            g.drawLine((float) cx[i] - std::cos(a) * 3.4f, (float) cy[i] - std::sin(a) * 3.4f,
                       (float) cx[i] + std::cos(a) * 3.4f, (float) cy[i] + std::sin(a) * 3.4f, 1.6f);
        }
    };
    // Fond deja rempli en tete (le spectrogramme reste visible dessous)
    // Panneaux pilotes par le skin actif

    // Bandeau lumineux : gradient historique, ou cyan/bleu/violet pour les
    // skins neon (KAY<3 et NightCity partagent la meme base de couleurs).
    const bool neonBandeau = activeSkin.neon || activeSkin.name == "KAY<3";
    juce::ColourGradient grad(
        neonBandeau ? juce::Colour(0xff28e1ff) : juce::Colour(0xffff5e8a), 0, 0,
        neonBandeau ? juce::Colour(0xffff46e1) : juce::Colour(0xff7df9ff), 1670.0f, 0, false);
    if (neonBandeau)
        grad.addColour(0.50, juce::Colour(0xff2878dc));
    else
    {
        grad.addColour(0.25, juce::Colour(0xffffc36b));
        grad.addColour(0.5, juce::Colour(0xff9d7bff));
        grad.addColour(0.75, juce::Colour(0xff5effc3));
    }
    g.setGradientFill(grad);
    g.fillRect(0, 0, 1670, 6);

    // Triangle "prisme" discret en fond (centre horizontal = 835)
    juce::Path prism;
    prism.addTriangle(835.0f - 130, 910,
                      835.0f + 130, 910,
                      835.0f, 100);
    g.setColour(juce::Colour(0x14ffffff));
    g.fillPath(prism);
    g.setColour(juce::Colour(0x22ffffff));
    g.strokePath(prism, juce::PathStrokeType(1.5f));

    // ---- NIGHTCITY : coins chanfreines --------------------------------
    // Le coin est coupe en biais (diagonale a 45 deg) : ni arrondi, ni angle droit.
    auto chamferPath = [](float x, float y, float w, float h, float c)
    {
        c = juce::jlimit(0.0f, juce::jmin(w, h) * 0.45f, c);
        juce::Path p;
        p.startNewSubPath(x + c, y);
        p.lineTo(x + w - c, y);
        p.lineTo(x + w,     y + c);
        p.lineTo(x + w,     y + h - c);
        p.lineTo(x + w - c, y + h);
        p.lineTo(x + c,     y + h);
        p.lineTo(x,         y + h - c);
        p.lineTo(x,         y + c);
        p.closeSubPath();
        return p;
    };

    // Panneau metal brosse : degrade + stries + biseau haut (couleurs du skin)
    auto paintBrushedPanel = [&g, this, &chamferPath](int x, int y, int w, int h)
    {
        // NIGHTCITY : panneaux a coins chanfreines (coupe a 45 deg).
        const bool  chNeon  = activeSkin.neon;
        const float panelRad = 14.0f;
        juce::Path  panelPath = chNeon ? chamferPath((float) x, (float) y, (float) w, (float) h, 14.0f)
                                       : juce::Path();
        juce::ColourGradient mg(activeSkin.panelTop, 0.0f, (float) y,
                                activeSkin.panelBottom, 0.0f, (float) (y + h), false);
        if (! activeSkin.hasTextures)
        {
            g.setGradientFill(mg);
            if (chNeon)
                g.fillPath(panelPath);
            else
                g.fillRoundedRectangle((float) x, (float) y, (float) w, (float) h, panelRad);
        }
        else if (chNeon)
        {
            g.fillPath(panelPath);
        }
        else
        {
            g.fillRoundedRectangle((float) x, (float) y, (float) w, (float) h, panelRad);
        }

        g.saveState();
        juce::Path clip;
        if (chNeon)
            clip = panelPath;
        else
            clip.addRoundedRectangle((float) x, (float) y, (float) w, (float) h, panelRad);
        g.reduceClipRegion(clip);

        // SOVIET ANALOG (peau texturée) : on tile la patine du pack par-dessus
        // le degrade, avec un leger mode multiply pour garder le volume.
        if (activeSkin.analog01)
        {
            // ANALOG01 : panneaux graphite (noir charbon) avec relief.
            // Deux zones bleues en metal bleu selon la reference :
            //   SPECTRUM (1046,58,608x220) et MOUVEMENT (16,674,644x206).
            const bool blueZone =
                (x == 1046 && y == 58   && w == 608 && h == 220) ||
                (x == 16   && y == 674 && w == 644 && h == 206);
            const juce::Colour pTop = blueZone ? juce::Colour(0xff197EB7)
                                                : juce::Colour(0xff2A323A);
            const juce::Colour pBot = blueZone ? juce::Colour(0xff0D4A70)
                                                : juce::Colour(0xff14181D);
            const juce::Colour bevHi = blueZone ? juce::Colour(0xff5FBEEE)
                                                 : juce::Colour(0xff4A555E);
            const juce::Colour bevLo = blueZone ? juce::Colour(0xff052438)
                                                 : juce::Colour(0xff080B0E);
            // 1. Corps : degrade vertical graphite / metal bleu.
            juce::ColourGradient pg(pTop, (float) x, (float) y,
                                    pBot, (float) x, (float) (y + h), false);
            g.setGradientFill(pg);
            g.fillRoundedRectangle((float) x, (float) y, (float) w, (float) h, panelRad);
            // 2. Brossage fin : stries pseudo-aleatoires lissees. Pas de pas
            //    fixe -> aucun motif repetitif, aucun effet pixelise.
            juce::Random brush(0x2468ACE0u + (juce::int64) (x * 31 + y * 17));
            float smooth = 0.5f;
            for (int yy = y; yy < y + h; ++yy)
            {
                smooth = smooth * 0.80f + brush.nextFloat() * 0.20f;
                const float a = 0.005f + smooth * 0.013f;
                g.setColour(smooth > 0.5f ? juce::Colours::white.withAlpha(a)
                                          : juce::Colours::black.withAlpha(a * 0.9f));
                g.drawLine((float) x, (float) yy + 0.5f, (float) (x + w), (float) yy + 0.5f, 0.5f);
            }
            // 3. Reflet satine en haut (acier bross eclaire).
            juce::ColourGradient sh(juce::Colours::white.withAlpha(0.05f), 0.0f, (float) y,
                                    juce::Colours::transparentWhite, 0.0f,
                                    (float) y + (float) h * 0.22f, false);
            g.setGradientFill(sh);
            g.fillRect((float) x, (float) y, (float) w, (float) h * 0.22f);
            // 4. Biseau deux tons : arete claire en haut, sombre en bas.
            g.saveState();
            g.reduceClipRegion(juce::Rectangle<int>(x, y, w, h / 2 + 1));
            g.setColour(bevHi.withAlpha(0.45f));
            g.drawRoundedRectangle((float) x + 1.5f, (float) y + 1.5f,
                                   (float) w - 3.0f, (float) h - 3.0f, panelRad - 1.5f, 1.2f);
            g.restoreState();
            g.saveState();
            g.reduceClipRegion(juce::Rectangle<int>(x, y + h / 2, w, h / 2));
            g.setColour(bevLo.withAlpha(0.55f));
            g.drawRoundedRectangle((float) x + 1.5f, (float) y + 1.5f,
                                   (float) w - 3.0f, (float) h - 3.0f, panelRad - 1.5f, 1.2f);
            g.restoreState();
            // 5. Rivets de milieu de cote. La bordure 9-slice du pack est
            //    EXCLUE ici : panel_border.png est beige chaud (149,140,125),
            //    incompatible avec l'acier froid / le metal bleu de la
            //    reference. Le bord est donne par le biseau 2 tons (4.) et
            //    par panelOutline (trait panelEdge).
            const juce::Image& rivA = PrismeTextures::rivet();
            if (rivA.isValid())
            {
                const float rSz = 10.0f;
                const float pos[4][2] = {
                    { (float) (x + w/2) - rSz*0.5f, (float) y + 6.0f },
                    { (float) (x + w/2) - rSz*0.5f, (float) (y + h) - 6.0f - rSz },
                    { (float) x + 6.0f, (float) (y + h/2) - rSz*0.5f },
                    { (float) (x + w) - 6.0f - rSz, (float) (y + h/2) - rSz*0.5f }
                };
                for (int i = 0; i < 4; ++i)
                    g.drawImage(rivA, pos[i][0], pos[i][1], rSz, rSz,
                                0, 0, rivA.getWidth(), rivA.getHeight());
            }
        }
        // IMPORTANT : le else-if est obligatoire. Sans lui, la branche
        // else if (hasTextures) s'executerait AUSSI pour analog01 et
        // recouvrirait les panneaux graphite / metal bleu de la texture
        // panel_metal_large (bug constate en capture).
        else if (activeSkin.analog03)
        {
            // ANALOG03 : plaque ivoire/champagne, metal brossi fin, biseautee.
            // 1. Ombre portee douce et discrète (plaque vissee sur le bois).
            g.setColour(juce::Colours::black.withAlpha(0.30f));
            g.fillRoundedRectangle((float) x + 1.5f, (float) y + 3.0f, (float) w, (float) h, panelRad);
            g.setColour(juce::Colours::black.withAlpha(0.16f));
            g.fillRoundedRectangle((float) x + 0.8f, (float) y + 1.8f, (float) w, (float) h, panelRad);
            // 2. Corps : degrade vertical ivoire chaud -> gris champagne chaud.
            juce::ColourGradient ivoryGrad(
                juce::Colour(0xffE4DECE), (float) x, (float) y,
                juce::Colour(0xffA6A194), (float) x, (float) (y + h), false);
            g.setGradientFill(ivoryGrad);
            g.fillRoundedRectangle((float) x, (float) y, (float) w, (float) h, panelRad);
            // 3. Brossage fin : stries pseudo-aleatoires lissees, tres discrets.
            //    Pas de pas fixe -> aucun motif repetitif ni effet pixelise.
            juce::Random brush(0x13572468u + (juce::int64) (x * 31 + y * 17));
            float smooth = 0.5f;
            for (int yy = y; yy < y + h; ++yy)
            {
                smooth = smooth * 0.78f + brush.nextFloat() * 0.22f;
                const float a = 0.005f + smooth * 0.011f;
                g.setColour(smooth > 0.5f ? juce::Colours::white.withAlpha(a)
                                          : juce::Colours::black.withAlpha(a * 0.9f));
                g.drawLine((float) x, (float) yy + 0.5f, (float) (x + w), (float) yy + 0.5f, 0.5f);
            }
            // 4. Reflet satine doux en haut (metal brossi, eclaire comme le bois).
            juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.045f), 0.0f, (float) y,
                                       juce::Colours::transparentWhite, 0.0f, (float) y + (float) h * 0.20f, false);
            g.setGradientFill(sheen);
            g.fillRect((float) x, (float) y, (float) w, (float) h * 0.20f);
            // 5. Biseau : arete claire en haut/gauche, sombre en bas/droite.
            g.saveState();
            g.reduceClipRegion(juce::Rectangle<int>(x, y, w, h / 2 + 1));
            g.setColour(juce::Colour(0xfff8f2e4).withAlpha(0.50f));
            g.drawRoundedRectangle((float) x + 1.5f, (float) y + 1.5f,
                                   (float) w - 3.0f, (float) h - 3.0f, panelRad - 1.5f, 1.2f);
            g.restoreState();
            g.saveState();
            g.reduceClipRegion(juce::Rectangle<int>(x, y + h / 2, w, h / 2));
            g.setColour(juce::Colour(0xff4a463d).withAlpha(0.45f));
            g.drawRoundedRectangle((float) x + 1.5f, (float) y + 1.5f,
                                   (float) w - 3.0f, (float) h - 3.0f, panelRad - 1.5f, 1.2f);
            g.restoreState();
            // 6. Bordure metallique distincte (9-slice du pack).
            const juce::Image& border = PrismeTextures::panelBorder();
            if (border.isValid())
            {
                g.drawImage(border, (float) x, (float) y, (float) w, (float) h,
                            0, 0, border.getWidth(), border.getHeight());
            }
            // 7. Rivets discrets aux milieux des cotes (vis d'angle gerees par
            //    drawScrews avec screw_phillips.png : vis metalliques reels).
            const juce::Image& riv = PrismeTextures::rivet();
            if (riv.isValid())
            {
                const float rSz = 10.0f;
                const float positions[4][2] = {
                    { (float) (x + w/2) - rSz*0.5f, (float) y + 6.0f },
                    { (float) (x + w/2) - rSz*0.5f, (float) (y + h) - 6.0f - rSz },
                    { (float) x + 6.0f, (float) (y + h/2) - rSz*0.5f },
                    { (float) (x + w) - 6.0f - rSz, (float) (y + h/2) - rSz*0.5f }
                };
                for (int i = 0; i < 4; ++i)
                    g.drawImage(riv, positions[i][0], positions[i][1], rSz, rSz,
                                0, 0, riv.getWidth(), riv.getHeight());
            }
        }

        else if (activeSkin.hasTextures)
        {
            // 1. Base : panel_metal_large.png (seamless 1200x390) tuile a taille native
            //    pour preserver le relief martele sans deformation.
            const juce::Image& metal = PrismeTextures::panelMetalLarge();
            if (metal.isValid())
            {
                const int tw = metal.getWidth(), th = metal.getHeight();
                for (int yy = y; yy < y + h; yy += th)
                    for (int xx = x; xx < x + w; xx += tw)
                        g.drawImage(metal, (float) xx, (float) yy, (float) tw, (float) th,
                                    0, 0, tw, th);
            }
            // 2. Bordure de panneau (panel_border.png) etiree
            const juce::Image& border = PrismeTextures::panelBorder();
            if (border.isValid())
            {
                g.drawImage(border, (float) x, (float) y, (float) w, (float) h,
                            0, 0, border.getWidth(), border.getHeight());
            }
            // 3. Rivets aux milieux des cotes
            const juce::Image& riv = PrismeTextures::rivet();
            if (riv.isValid())
            {
                const float rSz = 10.0f;
                const float positions[4][2] = {
                    { (float) (x + w/2) - rSz*0.5f, (float) y + 6.0f },
                    { (float) (x + w/2) - rSz*0.5f, (float) (y + h) - 6.0f - rSz },
                    { (float) x + 6.0f, (float) (y + h/2) - rSz*0.5f },
                    { (float) (x + w) - 6.0f - rSz, (float) (y + h/2) - rSz*0.5f }
                };
                for (int i = 0; i < 4; ++i)
                    g.drawImage(riv, positions[i][0], positions[i][1], rSz, rSz,
                                0, 0, riv.getWidth(), riv.getHeight());
            }
        }
        else
        {
            // Stries horizontales fines (alu brosse), deterministes
            for (int yy = y; yy < y + h; yy += 2)
            {
                bool light = ((yy / 2) % 2) == 0;
                g.setColour(light ? juce::Colours::white.withAlpha(0.020f)
                                  : juce::Colours::black.withAlpha(0.035f));
                g.drawLine((float) x, (float) yy + 0.5f, (float) (x + w), (float) yy + 0.5f, 1.0f);
            }
        }
        // Reflet horizontal doux (uniquement pour les skins non-texturés)
        if (! activeSkin.hasTextures)
        {
            juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.05f), 0.0f, (float) y,
                                       juce::Colours::transparentWhite, 0.0f, (float) y + (float) h * 0.35f, false);
            g.setGradientFill(sheen);
            g.fillRect((float) x, (float) y, (float) w, (float) h * 0.35f);
        }
        g.restoreState();

        if (! activeSkin.hasTextures)
        {
            g.setColour(activeSkin.bevelHi);
            g.drawLine((float) x + 8.0f, (float) y + 1.0f, (float) x + (float) w - 8.0f, (float) y + 1.0f, 1.0f);
        }
    };

    // Post Apo : chaque module a sa propre plaque (couleur + legere inclinaison).
    // Autres skins : le panneau metal historique, inchange.
    auto paintModule = [&](PrismeDiy::Module m, float tiltDeg, int x, int y, int w, int h)
    {
        if (activeSkin.diy)
            PrismeDiy::paintModulePlate(g, activeSkin, m, x, y, w, h, tiltDeg);
        else
            paintBrushedPanel(x, y, w, h);
    };

    // ---- NIGHTCITY : cadre a double trait, comme la maquette.
    // Un contour exterieur fin (degrade cyan -> bleu) et, a l'interieur,
    // quatre equerres de coin (« L »). Les autres skins gardent le trait simple.
    auto neonBracket = [&g](float bx, float by, float sx, float sy, float r,
                            float lx, float ly)
    {
        // Cotes plats : l'equerre est un simple angle droit.
        if (r < 0.5f)
        {
            juce::Path q;
            q.startNewSubPath(bx, by + sy * ly);
            q.lineTo(bx, by);
            q.lineTo(bx + sx * lx, by);
            g.strokePath(q, juce::PathStrokeType(2.0f));
            return;
        }
        // NIGHTCITY : le coin de l'equerre est coupe en biais (chanfrein).
        juce::Path p;
        p.startNewSubPath(bx, by + sy * (r + ly));
        p.lineTo(bx, by + sy * r);
        p.lineTo(bx + sx * r, by);
        p.lineTo(bx + sx * (r + lx), by);
        g.strokePath(p, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    };

    // Cadre neon complet : halo + trait fin degrade + equerres de coin.
    // La longueur des equerres s'adapte aux petits cadres (pastille du selecteur de skin).
    auto neonFrame = [&](float fx, float fy, float fw, float fh)
    {
        const float sm = juce::jmin(fw, fh);
        const float ch = 13.0f;   // NIGHTCITY : chanfrein des coins
        // Halo doux (la maquette a un leger bloom sur le trait)
        g.setColour(activeSkin.accent.withAlpha(0.09f));
        g.strokePath(chamferPath(fx - 1.5f, fy - 1.5f, fw + 3.0f, fh + 3.0f, ch + 1.5f),
                     juce::PathStrokeType(6.0f));
        g.setColour(activeSkin.accent.withAlpha(0.14f));
        g.strokePath(chamferPath(fx - 0.75f, fy - 0.75f, fw + 1.5f, fh + 1.5f, ch + 0.75f),
                     juce::PathStrokeType(3.0f));
        // Contour fin, degrade du cyan (haut) au bleu profond (bas)
        juce::ColourGradient og(activeSkin.accent,   0.0f, fy,
                                activeSkin.accentDeep, 0.0f, fy + fh, false);
        g.setGradientFill(og);
        g.strokePath(chamferPath(fx, fy, fw, fh, ch), juce::PathStrokeType(1.2f));
        // Equerres de coin a l'interieur (coin coupe en biais, comme le cadre)
        const float ins = juce::jmin(10.0f, sm * 0.18f);
        const float rr  = juce::jmin(9.0f, sm * 0.15f);
        const float lx  = juce::jlimit(4.0f, 26.0f, fw * 0.5f - ins - rr - 1.0f);
        const float ly  = juce::jlimit(4.0f, 20.0f, fh * 0.5f - ins - rr - 1.0f);
        g.setColour(activeSkin.accent.withAlpha(0.95f));
        neonBracket(fx + ins,      fy + ins,       1.0f,  1.0f, rr, lx, ly);
        neonBracket(fx + fw - ins, fy + ins,      -1.0f,  1.0f, rr, lx, ly);
        neonBracket(fx + fw - ins, fy + fh - ins, -1.0f, -1.0f, rr, lx, ly);
        neonBracket(fx + ins,      fy + fh - ins,  1.0f, -1.0f, rr, lx, ly);
    };

    auto panelOutline = [&](int x, int y, int w, int h)
    {
        if (activeSkin.neon)
        {
            neonFrame((float) x, (float) y, (float) w, (float) h);
            return;
        }
        // KAY<3 : cadre arrondi a deux tons (cyan en haut, magenta en bas).
        if (activeSkin.kay)
        {
            HardwareLookAndFeel::kayNeonFrame(
                g, juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h),
                10.0f, activeSkin, true);
            return;
        }
        g.setColour(activeSkin.panelEdge);
        g.drawRoundedRectangle((float) x, (float) y, (float) w, (float) h, 14.0f, 1.0f);
    };

    // ---- NIGHTCITY : le selecteur de skin est dans son propre cadre (maquette) ----
    if (activeSkin.neon)
    {
        const juce::Rectangle<float> cadreSkin(10.0f, 8.0f, 182.0f, 38.0f);
        g.setColour(juce::Colour(0xff040a18));   // masque la bande derriere le selecteur
        g.fillPath(chamferPath(cadreSkin.getX(), cadreSkin.getY(),
                               cadreSkin.getWidth(), cadreSkin.getHeight(), 11.0f));
        neonFrame(cadreSkin.getX(), cadreSkin.getY(),
                  cadreSkin.getWidth(), cadreSkin.getHeight());
    }

    // ---- COLONNE GAUCHE (x 16..660) ----
    // Panneau accords : rangee accord + pads + 9 potards
    paintModule(PrismeDiy::Module::Mint, -0.4f, 16, 58, 644, 400);
    panelOutline(16, 58, 644, 400);
    if (activeSkin.neon)   // titre du premier panneau, comme la maquette
    {
        g.setColour(activeSkin.accent.withAlpha(0.90f));
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("K E Y   /   S C A L E", 16, 58, 644, 14,
                   juce::Justification::centred, false);
    }

    // Panneau arp
    if (activeSkin.neon)
    {
        // NIGHTCITY : la zone ARP est decoupee en trois cadres, comme la maquette.
        paintModule(PrismeDiy::Module::Blue, 0.3f, 16, 466, 644, 58);
        panelOutline(16, 466, 644, 58);
        paintModule(PrismeDiy::Module::Blue, 0.3f, 16, 530, 232, 116);
        panelOutline(16, 530, 232, 116);
        paintModule(PrismeDiy::Module::Blue, 0.3f, 252, 530, 408, 116);
        panelOutline(252, 530, 408, 116);
        g.setColour(activeSkin.accent.withAlpha(0.90f));
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("A R P", 16, 470, 644, 18, juce::Justification::centred, false);
        g.drawText("A R P   F X", 16, 534, 232, 16, juce::Justification::centred, false);
        g.drawText("A R P E G G E", 252, 534, 408, 16, juce::Justification::centred, false);
    }
    else
    {
        paintModule(PrismeDiy::Module::Blue, 0.3f, 16, 466, 644, 200);
        panelOutline(16, 466, 644, 200);
    }

    // Panneau mouvement (colonne gauche)
    paintModule(PrismeDiy::Module::Grille, -0.25f, 16, 674, 644, 206);
    panelOutline(16, 674, 644, 206);

    // ---- COLONNE CENTRE (x 668..1030) ----
    // Panneau Mirage : 3 faders + 5 potards
    paintModule(PrismeDiy::Module::Orange, 0.35f, 668, 58, 362, 420);
    panelOutline(668, 58, 362, 420);

    // Panneau EQ (bas de la colonne centre)
    paintModule(PrismeDiy::Module::Grille, 0.3f, 668, 486, 362, 394);
    panelOutline(668, 486, 362, 394);

    // ---- COLONNE DROITE (x 1046..1654) ----
    // Panneau Spectrum : cadre metal + interieur noir (afficheur)
    paintModule(PrismeDiy::Module::GrisVert, -0.3f, 1046, 58, 608, 220);
    panelOutline(1046, 58, 608, 220);
    {
        juce::Rectangle<float> inset(1062.0f, 88.0f, 576.0f, 172.0f);
        if (activeSkin.diy)
            PrismeDiy::paintBezel(g, inset, 8.0f);        // cadre noir epais encastre
        if (activeSkin.neon)
        {
            // NIGHTCITY : afficheur a cotes plats + leger degrade vertical
            juce::ColourGradient ig(juce::Colour(0xff07132b), 0.0f, inset.getY(),
                                    juce::Colour(0xff01040c), 0.0f, inset.getBottom(), false);
            g.setGradientFill(ig);
            g.fillPath(chamferPath(inset.getX(), inset.getY(),
                                   inset.getWidth(), inset.getHeight(), 8.0f));
            g.setColour(activeSkin.accent.withAlpha(0.14f));
            g.strokePath(chamferPath(inset.getX() - 2.0f, inset.getY() - 2.0f,
                                     inset.getWidth() + 4.0f, inset.getHeight() + 4.0f, 10.0f),
                         juce::PathStrokeType(4.0f));
            g.setColour(activeSkin.accent.withAlpha(0.55f));
            g.strokePath(chamferPath(inset.getX(), inset.getY(),
                                     inset.getWidth(), inset.getHeight(), 8.0f),
                         juce::PathStrokeType(1.2f));
        }
        else
        {
            // ANALOG01 : ecran noir-bleu (display_black_blue #020C16).
            g.setColour(activeSkin.analog01 ? juce::Colour(0xff020C16)
                        : activeSkin.analog03 ? juce::Colour(0xff050907)
                        : activeSkin.diy ? juce::Colour(0xff04100a) : juce::Colour(0xff0a0603));
            g.fillRoundedRectangle(inset, 6.0f);
        }
        if (activeSkin.analog01)
        {
            // ANALOG01 : grille d'instrument cyan tres discrete dans le spectre.
            g.setColour(juce::Colour(0xff00A8FF).withAlpha(0.20f));
            for (float gx = inset.getX(); gx <= inset.getRight(); gx += 48.0f)
                g.drawLine(gx, inset.getY(), gx, inset.getBottom(), 0.5f);
            for (float gy = inset.getY(); gy <= inset.getBottom(); gy += 34.0f)
                g.drawLine(inset.getX(), gy, inset.getRight(), gy, 0.5f);
        }
        if (activeSkin.analog03)
        {
            // ANALOG03 : grille fine verte dans le spectre
            g.setColour(juce::Colour(0xff124B2B).withAlpha(0.35f));
            for (float gx = inset.getX(); gx <= inset.getRight(); gx += 48.0f)
                g.drawLine(gx, inset.getY(), gx, inset.getBottom(), 0.5f);
            for (float gy = inset.getY(); gy <= inset.getBottom(); gy += 34.0f)
                g.drawLine(inset.getX(), gy, inset.getRight(), gy, 0.5f);
        }
        if (activeSkin.diy)                               // quadrillage d'oscilloscope
            PrismeDiy::oscGraticule(g, inset.reduced(4.0f), activeSkin.scopeMid, 64.0f, 6);
    }

    // Panneau Samples : cadre metal + interieur noir (tableau)
    paintModule(PrismeDiy::Module::Bordeaux, 0.25f, 1046, 286, 608, 594);
    panelOutline(1046, 286, 608, 594);
    {
        juce::Rectangle<float> inset(1062.0f, 344.0f, 576.0f, 520.0f);
        if (activeSkin.diy)
            PrismeDiy::paintBezel(g, inset, 8.0f);        // cadre noir epais encastre
        if (activeSkin.neon)
        {
            // NIGHTCITY : tableau a cotes plats + leger degrade vertical
            juce::ColourGradient ig(juce::Colour(0xff07132b), 0.0f, inset.getY(),
                                    juce::Colour(0xff01040c), 0.0f, inset.getBottom(), false);
            g.setGradientFill(ig);
            g.fillPath(chamferPath(inset.getX(), inset.getY(),
                                   inset.getWidth(), inset.getHeight(), 8.0f));
            g.setColour(activeSkin.accent.withAlpha(0.14f));
            g.strokePath(chamferPath(inset.getX() - 2.0f, inset.getY() - 2.0f,
                                     inset.getWidth() + 4.0f, inset.getHeight() + 4.0f, 10.0f),
                         juce::PathStrokeType(4.0f));
            g.setColour(activeSkin.accent.withAlpha(0.55f));
            g.strokePath(chamferPath(inset.getX(), inset.getY(),
                                     inset.getWidth(), inset.getHeight(), 8.0f),
                         juce::PathStrokeType(1.2f));
        }
        else
        {
            // ANALOG01 : fond d'ecran bleu nuit pour la grille oscilloscope.
            g.setColour(activeSkin.analog01 ? juce::Colour(0xff02101E)
                        : activeSkin.analog03 ? juce::Colour(0xff050907)
                        : activeSkin.diy ? juce::Colour(0xff04100a) : juce::Colour(0xff0a0603));
            g.fillRoundedRectangle(inset, 6.0f);
        }
        if (activeSkin.analog01)
        {
            // ANALOG01 : grille bleue lumineuse facon oscilloscope (reference).
            g.setColour(juce::Colour(0xff00A8FF).withAlpha(0.30f));
            for (float gx = inset.getX(); gx <= inset.getRight(); gx += 40.0f)
                g.drawLine(gx, inset.getY(), gx, inset.getBottom(), 0.6f);
            for (float gy = inset.getY(); gy <= inset.getBottom(); gy += 40.0f)
                g.drawLine(inset.getX(), gy, inset.getRight(), gy, 0.6f);
            // Points d'intersection plus lumineux (cyan LED).
            g.setColour(juce::Colour(0xff26B8FF).withAlpha(0.22f));
            for (float gx = inset.getX(); gx <= inset.getRight(); gx += 40.0f)
                for (float gy = inset.getY(); gy <= inset.getBottom(); gy += 40.0f)
                    g.fillRect(gx - 0.5f, gy - 0.5f, 1.5f, 1.5f);
        }
        if (activeSkin.analog03)
        {
            // ANALOG03 : grille verte lumineuse en fond du panneau SAMPLES
            g.setColour(juce::Colour(0xff124B2B).withAlpha(0.40f));
            for (float gx = inset.getX(); gx <= inset.getRight(); gx += 40.0f)
                g.drawLine(gx, inset.getY(), gx, inset.getBottom(), 0.6f);
            for (float gy = inset.getY(); gy <= inset.getBottom(); gy += 40.0f)
                g.drawLine(inset.getX(), gy, inset.getRight(), gy, 0.6f);
            // Points d'intersection plus lumineux
            g.setColour(juce::Colour(0xff28F05A).withAlpha(0.12f));
            for (float gx = inset.getX(); gx <= inset.getRight(); gx += 40.0f)
                for (float gy = inset.getY(); gy <= inset.getBottom(); gy += 40.0f)
                    g.fillRect(gx - 0.5f, gy - 0.5f, 1.5f, 1.5f);
        }
    }

    // Post Apo : filigrane (plan technique) et coulures, SOUS les controles
    if (activeSkin.diy)
        PrismeDiy::paintUnderlay(g);

    // Vis aux coins de chaque panneau
    drawScrews(16, 58, 644, 400);
    drawScrews(16, 466, 644, 200);
    drawScrews(16, 674, 644, 206);
    drawScrews(668, 58, 362, 420);
    drawScrews(668, 486, 362, 394);
    drawScrews(1046, 58, 608, 220);
    drawScrews(1046, 286, 608, 594);

    // Oscilloscope 3D reel dans le panneau de droite (depuis le signal DSP,
    // jamais simule). Il occupe tout le cadre noir du panneau SPECTRUM.
    scope3d.draw(g, juce::Rectangle<float>(1062.0f, 96.0f, 576.0f, 164.0f));
    if (activeSkin.diy)   // lignes de balayage par-dessus la trace
        PrismeDiy::oscScanlines(g, juce::Rectangle<float>(1062.0f, 96.0f, 576.0f, 164.0f));

    // NOTE : les graduations dB / frequences (echelle FFT) ont ete retirees
    // de CE cadre : elles decrivent le spectre, pas une forme d'onde dans le
    // temps, et laisser des etiquettes fausses serait trompeur.
    // Le spectrogramme lui-meme est TOUJOURS dessine dans la bande haute
    // (spectro.draw(...) du bandeau 1670x52) : rien n'a ete supprime la.

    // Entrees de panneau (titres de zone)
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.setColour(activeSkin.diy ? activeSkin.textOnWidget : activeSkin.sectionTitle);
    g.drawText("SPECTRUM", 1062, 74, 200, 16, juce::Justification::centredLeft);
    g.drawText("SAMPLES", 1062, 302, 200, 16, juce::Justification::centredLeft);

    // ---- Tableau des prises (panneau SAMPLES) : donnees reelles ----
    {
        using namespace SampleTable;
        // Post Apo : lignes sombres => texte clair. Autres skins : inchange.
        const juce::Colour tMain = activeSkin.diy ? activeSkin.textOnWidget : activeSkin.textMain;
        const juce::Colour tDim  = activeSkin.diy ? activeSkin.textOnWidget.withAlpha(0.65f) : activeSkin.textDim;
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.setColour(tDim);

        if (sampleLines.empty())
        {
            g.setFont(juce::Font(12.0f, juce::Font::plain));
            g.drawText("Aucune prise enregistree", listX, listY + 20, listW, 20,
                       juce::Justification::centred);
        }
        else
        {
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            for (size_t i = 0; i < sampleLines.size() && (int) i < maxRows; ++i)
            {
                const auto& sl = sampleLines[i];
                int y = listY + (int) i * rowH;

                // Fond de ligne (selection = accent)
                juce::Rectangle<float> rowArea((float) listX, (float) y,
                                               (float) listW, (float) rowH - 2.0f);
                g.setColour(sl.selected ? activeSkin.accent.withAlpha(0.22f)
                                        : juce::Colours::black.withAlpha(0.22f));
                g.fillRoundedRectangle(rowArea, 5.0f);
                if (sl.selected)
                {
                    g.setColour(activeSkin.accent);
                    g.drawRoundedRectangle(rowArea, 5.0f, 1.5f);
                }

                // Numero
                g.setColour(sl.selected ? activeSkin.accentHi : tMain);
                g.drawText(juce::String(sl.number).paddedLeft('0', 2),
                           listX + 6, y + 2, 26, rowH - 6,
                           juce::Justification::centredLeft);

                // Nom
                g.setColour(sl.fileMissing ? tDim : tMain);
                g.drawText(sl.name, listX + nameRelX, y + 2, nameRelW, rowH - 6,
                           juce::Justification::centredLeft, true);

                // Duree
                g.setColour(tDim);
                g.drawText(juce::String(sl.durationSec, 1) + "s",
                           listX + durRelX, y + 2, 32, rowH - 6,
                           juce::Justification::centredLeft);

                // Cellule VARIATION : seed + intensite + mesures (donnees reelles)
                juce::Rectangle<float> varCell((float) (listX + varRelX), (float) (y + 8),
                                               (float) varRelW, (float) rowH - 16.0f);
                g.setColour(sl.isVariation ? activeSkin.accent.withAlpha(0.14f)
                                           : juce::Colours::black.withAlpha(0.18f));
                g.fillRoundedRectangle(varCell, 3.0f);
                g.setColour(sl.isVariation ? activeSkin.accentHi : tDim);
                g.setFont(juce::Font(9.0f, juce::Font::plain));
                if (sl.isVariation)
                    g.drawText("R" + juce::String((int) (sl.seed % 1000)) + " I"
                                   + juce::String(sl.intensityPct) + "%",
                               varCell.toNearestInt(), juce::Justification::centred);
                else
                    g.drawText("-", varCell.toNearestInt(), juce::Justification::centred);
                g.setFont(juce::Font(10.0f, juce::Font::bold));

                // Mini-forme d'onde (pics reels) entre la cellule et les bouts
                float wx0 = (float) (listX + waveRelX);
                float wx1 = (float) (listX + playStopRelX - 6);
                float midY = (float) y + (float) rowH * 0.5f - 1.0f;
                if (wx1 > wx0 + 20.0f && ! sl.peaks.empty())
                {
                    const int n = (int) sl.peaks.size();
                    auto drawPeaks = [&](juce::Colour col, float thick)
                    {
                        g.setColour(col);
                        for (int x = (int) wx0; x < (int) wx1; ++x)
                        {
                            int c = (x - (int) wx0) * n / (int) (wx1 - wx0);
                            c = juce::jlimit(0, n - 1, c);
                            float v = juce::jlimit(0.0f, 1.0f, sl.peaks[(size_t) c]);
                            float dy = v * ((float) rowH * 0.5f - 4.0f);
                            g.drawLine((float) x, midY - dy, (float) x, midY + dy + 1.0f, thick);
                        }
                    };
                    if (activeSkin.diy && ! sl.fileMissing)
                    {
                        // Oscilloscope vert phosphore : quadrillage, halo, trace, balayage.
                        // Les pics restent ceux de la prise (donnees reelles).
                        const juce::Rectangle<float> scr(wx0, (float) y + 3.0f,
                                                         wx1 - wx0, (float) rowH - 8.0f);
                        PrismeDiy::oscGraticule(g, scr, activeSkin.scopeMid, 30.0f, 4);
                        drawPeaks(activeSkin.scopeMid.withAlpha(0.22f), 3.0f);
                        drawPeaks(activeSkin.scopeMid.withAlpha(0.95f), 1.0f);
                        PrismeDiy::oscScanlines(g, scr);
                    }
                    else
                        drawPeaks((sl.fileMissing ? tDim
                                                  : activeSkin.accent).withAlpha(0.9f), 1.0f);
                }

                // Bouton PLAY/STOP : etat reel de la pre-ecoute de CETTE ligne
                juce::Rectangle<float> playStop((float) (listX + playStopRelX), (float) (y + 8),
                                             (float) btnW, (float) rowH - 16.0f);
                g.setColour(sl.previewing ? activeSkin.accent
                                          : activeSkin.panelBottom.brighter(0.10f));
                g.fillRoundedRectangle(playStop, 3.0f);
                g.setColour(sl.previewing ? activeSkin.accentHi : tDim);
                g.setFont(juce::Font(9.0f, juce::Font::bold));
                g.drawText(sl.previewing ? "STOP" : "PLAY", playStop.toNearestInt(),
                           juce::Justification::centred);

                // Bouton SUPPRIMER : 2e clic pour confirmer
                juce::Rectangle<float> del((float) (listX + delRelX), (float) (y + 8),
                                           (float) btnW, (float) rowH - 16.0f);
                bool armed = (sampleConfirmIdx == (int) i);
                g.setColour(armed ? juce::Colour(0xff8a1f2e)
                                  : activeSkin.panelBottom.brighter(0.10f));
                g.fillRoundedRectangle(del, 3.0f);
                g.setColour(armed ? juce::Colours::white : tDim);
                g.drawText(armed ? "OK?" : "DEL", del.toNearestInt(),
                           juce::Justification::centred);
                g.setFont(juce::Font(10.0f, juce::Font::bold));
            }
        }
    }

    const bool isKay3 = activeSkin.name == "KAY<3";
    if (! isKay3)
    {
        // Cristaux historiques au premier plan, de part et d'autre du titre.
        drawCluster(585.0f, 46.0f, 1.0f, false);
        drawCluster(1085.0f, 46.0f, 1.0f, true);
    }
    else
    {
        // KAY<3 : elements decoratifs non interactifs. Les images sont mises
        // en cache par PrismeTextures et ne touchent jamais au thread audio.
        // ---- Bandeau : logo PRISME + mot KAY + barres de niveau ----
        // (La plaque de verre KAY reste invisible ; les personnages neon sont remis.)
        // Logo graffiti « PRISME », remonte dans le bandeau.
        g.setOpacity(0.92f);
        PrismeTextures::drawFit(g, PrismeTextures::kayGraffitiLogo(),
                                juce::Rectangle<float>(560.0f, 2.0f, 480.0f, 58.0f));
        // Mot « KAY » dans la bande noire du bandeau (a la place de la plaque).
        g.setFont(juce::Font(20.0f, juce::Font::bold));
        g.setColour(activeSkin.textDim);
        g.drawText("K A Y", 346.0f, 24.0f, 226.0f, 54.0f, juce::Justification::centred, false);
        // Barres de niveau cyan -> rose, a droite du bandeau.
        g.setOpacity(0.90f);
        PrismeTextures::drawFit(g, PrismeTextures::kayLevelBars(),
                                juce::Rectangle<float>(1360.0f, 40.0f, 270.0f, 52.0f));
        g.setOpacity(1.0f);

        // ---- Arc en ciel dans le coin bas-droit du panneau SAMPLES ----
        {
            const float rcx = 1640.0f;
            const float rcy = 870.0f;
            const float rOuter = 180.0f;  // 2x plus grand
            const float bandW  = 20.0f;
            juce::Colour rainbow[7] = {
                juce::Colour(0xffff0000), // red
                juce::Colour(0xffff7f00), // orange
                juce::Colour(0xffffff00), // yellow
                juce::Colour(0xff00ff00), // green
                juce::Colour(0xff0000ff), // blue
                juce::Colour(0xff4b0082), // indigo
                juce::Colour(0xff8b00ff)  // violet
            };
            for (int i = 0; i < 7; ++i)
            {
                const float r = rOuter - (float) i * bandW;
                juce::Path arc;
                const int steps = 40;
                for (int j = 0; j <= steps; ++j)
                {
                    const float t     = (float) j / (float) steps;
                    const float angle = t * juce::MathConstants<float>::halfPi;
                    const float px    = rcx - std::cos(angle) * r;
                    const float py    = rcy - std::sin(angle) * r;
                    if (j == 0) arc.startNewSubPath(px, py);
                    else        arc.lineTo(px, py);
                }
                // Effet neon : halo large derriere chaque bande
                g.setColour(rainbow[i].withAlpha(0.08f));
                g.strokePath(arc, juce::PathStrokeType(bandW * 2.5f,
                             juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                // Halo moyen
                g.setColour(rainbow[i].withAlpha(0.16f));
                g.strokePath(arc, juce::PathStrokeType(bandW * 1.6f,
                             juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                // Trait principal
                g.setColour(rainbow[i].withAlpha(0.35f));
                g.strokePath(arc, juce::PathStrokeType(bandW * 0.85f,
                             juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }

        // Coeur avec pensement (band-aid), haut-gauche du panneau KEY / SCALE, effet neon.
        {
            const float hx = 20.0f, hy = 56.0f, hr = 10.0f;

            // Halo neon (cyan + magenta) — plus fort pour la visibilite
            g.setColour(juce::Colour(0xff28e1ff).withAlpha(0.25f));
            g.fillEllipse(hx - hr - 8.0f, hy - hr - 8.0f, (hr + 8.0f) * 2.0f, (hr + 8.0f) * 2.0f);
            g.setColour(juce::Colour(0xffff46e1).withAlpha(0.18f));
            g.fillEllipse(hx - hr - 12.0f, hy - hr - 12.0f, (hr + 12.0f) * 2.0f, (hr + 12.0f) * 2.0f);

            // Corps du coeur : deux lobes + triangle
            juce::Path heart;
            heart.addEllipse(hx - hr * 1.15f, hy - hr, hr * 1.15f, hr * 1.15f);
            heart.addEllipse(hx, hy - hr, hr * 1.15f, hr * 1.15f);
            juce::Path tri;
            tri.startNewSubPath(hx - hr * 1.15f, hy + hr * 0.15f);
            tri.lineTo(hx + hr * 1.15f, hy + hr * 0.15f);
            tri.lineTo(hx, hy + hr * 1.7f);
            tri.closeSubPath();
            heart.addPath(tri);

            // Remplissage — plus clair pour la visibilite
            juce::ColourGradient body(juce::Colour(0xff3a2060), hx, hy - hr,
                                      juce::Colour(0xff151530), hx, hy + hr * 1.7f, false);
            g.setGradientFill(body);
            g.fillPath(heart);
            // Liseré neon cyan — plus epais et plus lumineux
            g.setColour(juce::Colour(0xff28e1ff).withAlpha(1.0f));
            g.strokePath(heart, juce::PathStrokeType(2.5f));

            // Pensement (band-aid) horizontal sur le coeur — plus grand et plus visible
            const float bw = hr * 2.2f, bh = 7.0f;
            const float bx = hx, by = hy + hr * 0.1f;
            juce::Rectangle<float> bandaid(bx - bw/2, by - bh/2, bw, bh);
            // Fond beige
            g.setColour(juce::Colour(0xfff5d0a9));
            g.fillRoundedRectangle(bandaid, 2.5f);
            // Pad central
            g.setColour(juce::Colour(0xfff0b888));
            g.fillRoundedRectangle(bandaid.reduced(5.0f, 1.5f), 1.5f);
            // Liseré neon magenta — plus epais
            g.setColour(juce::Colour(0xffff46e1).withAlpha(0.85f));
            g.drawRoundedRectangle(bandaid, 2.5f, 1.0f);
        }

        // Personnages neon : licorne filigrane derriere les potards, punk accroche
        // au bandeau, poupée au rebord bas du spectre.
        {
            g.saveState();
            juce::Path clip;
            clip.addRoundedRectangle(24.0f, 68.0f, 628.0f, 382.0f, 10.0f);
            g.reduceClipRegion(clip);
            g.setOpacity(0.42f);
            PrismeTextures::drawCover(g, PrismeTextures::kayUnicorn(),
                                      juce::Rectangle<float>(184.0f, 77.0f, 308.0f, 362.0f));
            g.restoreState();
        }
        g.setOpacity(0.96f);
        PrismeTextures::drawCover(g, PrismeTextures::kayPunk(),
                                  juce::Rectangle<float>(574.0f, 16.0f, 93.0f, 68.0f));
        {
            g.saveState();
            juce::Path clip;
            clip.addRectangle(1046.0f, 58.0f, 608.0f, 269.0f);
            g.reduceClipRegion(clip);
            g.setOpacity(0.95f);
            PrismeTextures::drawCover(g, PrismeTextures::kayDoll(),
                                      juce::Rectangle<float>(1276.0f, 210.0f, 108.0f, 68.0f));
            g.restoreState();
        }
        g.setOpacity(1.0f);
    }

    // Signature gravee dans la machine (meme graisse que le titre)
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.setColour(activeSkin.diy ? activeSkin.textOnWidget.withAlpha(0.55f) : activeSkin.textDim);
    g.drawText("by BloodyRedIndustrie", 1040, 13, 180, 26, juce::Justification::centredLeft);
    g.setColour(activeSkin.diy ? activeSkin.textOnWidget : activeSkin.textMain);
    g.drawText("by BloodyRedIndustrie", 1040, 12, 180, 26, juce::Justification::centredLeft);
}

void PrismeEditor::resized()
{
    // Mise a l'echelle uniforme du calque 880x970 : proportions intactes
    // (potards ronds, textes nets), centre si l'hote ignore le ratio.
    constexpr int designW = 1670, designH = 941;
    float s = std::min((float) getWidth() / (float) designW,
                       (float) getHeight() / (float) designH);
    s = juce::jlimit(0.25f, 4.0f, s);
    int ox = (int) (((float) getWidth() - (float) designW * s) * 0.5f);
    int oy = (int) (((float) getHeight() - (float) designH * s) * 0.5f);
    content.setTransform(juce::AffineTransform::scale(s));
    content.setBounds(ox, oy, designW, designH);
    cornerResizer.setBounds(getWidth() - 18, getHeight() - 18, 16, 16);

    constexpr int W = designW;
    titleLabel.setBounds(0, 8, W, 34);
    skinLabel.setBounds(16, 14, 36, 26);
    skinBox.setBounds(56, 14, 130, 26);
    undoBtn.setBounds(196, 14, 58, 26);
    redoBtn.setBounds(258, 14, 58, 26);
    recorderBtn.setBounds(1514, 14, 140, 26);
    recorderView.setBounds(0, 0, W, designH);

    // ---- COLONNE GAUCHE (x 16..660) : ACCORDS, ARP, MOVEMENT ----

    // Rangee accords (haut de la colonne)
    int topY = 72;
    rootBox.setBounds(28, topY, 90, 26);
    scaleBox.setBounds(126, topY, 160, 26);
    autoKeyBtn.setBounds(292, topY, 66, 26);
    octaveBox.setBounds(362, topY, 90, 26);
    givreLabel.setBounds(458, topY, 44, 26);
    givreSlider.setBounds(504, topY, 76, 26);
    randomBtn.setBounds(588, topY, 60, 26);

    // Knobs accords : 5 en rangee 1, 4 en rangee 2 (centree)
    int knobW = 112, knobH = 108;
    int startX = 28, startY = 178;
    int gapX = 12, rowGap = 24;
    for (size_t i = 0; i < knobs.size() && i < 9; ++i)
    {
        int row = (int)i / 5;
        int col = (int)i % 5;
        int x = startX + col * (knobW + gapX) + (row == 1 ? (knobW + gapX) / 2 : 0);
        int y = startY + row * (knobH + rowGap);
        knobs[i]->slider.setBounds(x, y, knobW, knobH);
        knobs[i]->label.setBounds(x, y + knobH, knobW, 16);
    }

    // Pastilles de gamme : dieses en haut, naturelles en bas (style piano)
    pads.setGeometry(284, 110, 240, 52, 24);

    // Rangee arp : On, Motif, Octaves, Sync, Division, Swing
    arpTitleLabel.setBounds(16, 470, 644, 20);
    arpOnBtn.setBounds(28, 494, 80, 26);
    arpPatternBox.setBounds(116, 494, 150, 26);
    arpOctBox.setBounds(274, 494, 70, 26);
    arpSyncBtn.setBounds(352, 494, 100, 26);
    arpDivBox.setBounds(460, 494, 70, 26);
    swingLabel.setBounds(538, 494, 44, 26);
    swingSlider.setBounds(586, 494, 62, 26);

    // Rangee FX arp : 3 menus empiles + bouton rouge + 3 potards + CRYSTALLIZE
    // NIGHTCITY decoupe la zone ARP en trois cadres : les controles du bas
    // descendent de 24 px pour laisser la place au titre de chaque cadre.
    // Les autres skins gardent la position d'origine (fxDy = 0).
    const int fxDy = activeSkin.neon ? 24 : 0;
    arpFxBox.setBounds(40, 528 + fxDy, 150, 24);
    arpFxBoxB.setBounds(40, 556 + fxDy, 150, 24);
    arpFxBoxC.setBounds(40, 584 + fxDy, 150, 24);
    // NIGHTCITY : pastille carree (24) au lieu du bouton large (44), comme la maquette.
    const int fxBtnW = activeSkin.neon ? 24 : 44;
    fxSlotBtn0.setBounds(198, 528 + fxDy, fxBtnW, 24);
    fxSlotBtn1.setBounds(198, 556 + fxDy, fxBtnW, 24);
    fxSlotBtn2.setBounds(198, 584 + fxDy, fxBtnW, 24);
    cryoKnob.setBounds(260, 530 + fxDy, 56, 56);
    cryoLabel.setBounds(256, 586 + fxDy, 64, 14);
    // 3 LED de cible : colonne libre a droite du potard Crystallize.
    // Chaque libelle (no arp / arp / all) suit sa LED sur la meme ligne.
    for (int i = 0; i < 3; ++i)
    {
        cryoLed[i].setBounds(320, 530 + fxDy + i * 24, 22, 22);
        cryoLedLabel[i].setBounds(344, 530 + fxDy + i * 24 + 4, 50, 14);
    }
    arpFxKnob1.setBounds(400, 530 + fxDy, 56, 56);
    arpFxLabel1.setBounds(396, 586 + fxDy, 64, 14);
    arpFxKnob2.setBounds(464, 530 + fxDy, 56, 56);
    arpFxLabel2.setBounds(460, 586 + fxDy, 64, 14);
    arpFxMixKnob.setBounds(528, 530 + fxDy, 56, 56);
    arpFxMixLabel.setBounds(524, 586 + fxDy, 64, 14);
    arpRouteBox.setBounds(592, 530 + fxDy, 64, 24); // cible des FX (Arp / Accords / Tout)
    arpRouteLabel.setBounds(588, 556 + fxDy, 72, 14);

    // Panneau mouvement : 4 potards + Sync/Division
    movTitleLabel.setBounds(16, 678, 644, 20);
    int movKnobW = 112, movKnobH = 88;
    for (size_t i = 9; i < knobs.size() && i < 13; ++i)
    {
        int x = 28 + (int)(i - 9) * (movKnobW + gapX);
        knobs[i]->slider.setBounds(x, 704, movKnobW, movKnobH);
        knobs[i]->label.setBounds(x, 704 + movKnobH, movKnobW, 14);
    }
    movSyncBtn.setBounds(28, 818, 150, 24);
    movDivBox.setBounds(186, 818, 100, 26);

    // ---- COLONNE CENTRE (x 668..1030) : MIRAGE, EQ ----

    // Tranche Mirage : 3 faders MIX + MIRAGE + BANDE, 5 potards dessous
    mixTitleLabel.setBounds(686, 66, 70, 18);
    blurTitleLabel.setBounds(756, 66, 70, 18);
    bandTitleLabel.setBounds(826, 66, 70, 18);
    wetLabel.setBounds(686, 86, 70, 14);
    blurWetLabel.setBounds(756, 86, 70, 14);
    bandHiLabel.setBounds(826, 86, 70, 14);
    mixFader.setBounds(686, 104, 58, 220);
    blurFader.setBounds(756, 104, 58, 220);
    bandFader.setBounds(826, 104, 58, 220);
    dryLabel.setBounds(686, 326, 70, 14);
    blurDryLabel.setBounds(756, 326, 70, 14);
    bandLoLabel.setBounds(826, 326, 70, 14);

    // 5 potards Mirage : Taille, Predelay, Color, Width, Decay
    int mirKnob = 64, mirGap = 4;
    int mirX = 680, mirY = 350;
    tailleSlider.setBounds(mirX + 0 * (mirKnob + mirGap), mirY, mirKnob, mirKnob);
    tailleLabel.setBounds(mirX + 0 * (mirKnob + mirGap), mirY + mirKnob, mirKnob, 14);
    mirPredelaySlider.setBounds(mirX + 1 * (mirKnob + mirGap), mirY, mirKnob, mirKnob);
    mirPredelayLabel.setBounds(mirX + 1 * (mirKnob + mirGap), mirY + mirKnob, mirKnob, 14);
    mirColorSlider.setBounds(mirX + 2 * (mirKnob + mirGap), mirY, mirKnob, mirKnob);
    mirColorLabel.setBounds(mirX + 2 * (mirKnob + mirGap), mirY + mirKnob, mirKnob, 14);
    mirWidthSlider.setBounds(mirX + 3 * (mirKnob + mirGap), mirY, mirKnob, mirKnob);
    mirWidthLabel.setBounds(mirX + 3 * (mirKnob + mirGap), mirY + mirKnob, mirKnob, 14);
    mirDecaySlider.setBounds(mirX + 4 * (mirKnob + mirGap), mirY, mirKnob, mirKnob);
    mirDecayLabel.setBounds(mirX + 4 * (mirKnob + mirGap), mirY + mirKnob, mirKnob, 14);

    // Panneau EQ (bas de la colonne centre) : 4 curseurs verticaux
    eqTitleLabel.setBounds(668, 490, 362, 20);
    int eqW = 76, eqH = 280, eqGap = 10;
    int eqStartX = 684;
    for (int i = 0; i < 4; ++i)
    {
        int x = eqStartX + i * (eqW + eqGap);
        eqSliders[i].setBounds(x, 518, eqW, eqH);
        eqLabels[i].setBounds(x - 10, 518 + eqH, eqW + 20, 16);
    }

    // ---- COLONNE DROITE (x 1046..1654) : SPECTRUM, SAMPLES ----
    // Barre de transport en haut du panneau SAMPLES (rec encadre).
    int trY = 324, trH = 26;
    sampleRecBtn.setBounds(1062, trY, 62, trH);
    sampleStopBtn.setBounds(1130, trY, 62, trH);
    sampleRandomBtn.setBounds(1204, trY, 104, trH);
    // LED de liaison : entre les deux boutons ; REROLL decale de 32 px.
    sampleLinkLed.setBounds(1318, trY, 20, trH);
    sampleRerollBtn.setBounds(1346, trY, 76, trH);

    // ---- BAS DE FENETRE : presets + signature ----
    int presetY = 888;
    presetBox.setBounds(40, presetY, 220, 24);
    presetNameEditor.setBounds(268, presetY, 120, 24);
    presetSaveBtn.setBounds(396, presetY, 70, 24);
    presetDeleteBtn.setBounds(474, presetY, 34, 24);
    slotABtn.setBounds(516, presetY, 34, 24);
    slotBBtn.setBounds(554, presetY, 34, 24);
    slotCopyBtn.setBounds(592, presetY, 52, 24);

    footerLabel.setBounds(0, 918, W, 20);
}
