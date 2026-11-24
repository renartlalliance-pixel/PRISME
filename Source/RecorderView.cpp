#include "RecorderView.h"
#include "PluginProcessor.h"
#include "PrismeDragDrop.h"

RecorderView::RecorderView(PrismeProcessor& p) : proc(p)
{
    titleLabel.setText("PRISME  -  RECORDER", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    stateLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    stateLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(stateLabel);

    elapsedLabel.setFont(juce::Font(30.0f, juce::Font::bold));
    elapsedLabel.setJustificationType(juce::Justification::centredLeft);
    elapsedLabel.setText("00:00.0", juce::dontSendNotification);
    addAndMakeVisible(elapsedLabel);

    tempoLabel.setFont(juce::Font(13.0f));
    tempoLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(tempoLabel);

    waveTitleLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    waveTitleLabel.setJustificationType(juce::Justification::centredLeft);
    waveTitleLabel.setText("Spectre 3D - sortie finale", juce::dontSendNotification);
    addAndMakeVisible(waveTitleLabel);

    mixTitleLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    mixTitleLabel.setJustificationType(juce::Justification::centred);
    mixTitleLabel.setText("E F F E C T   M I X", juce::dontSendNotification);
    addAndMakeVisible(mixTitleLabel);

    // 8 potentiomètres compacts (style pro : corps + repere + valeur).
    // Attaches aux memes IDs APVTS que l'interface principale.
    struct MixDef { const char* id; const char* name; };
    static const MixDef table[] = {
        { "mix", "Mix" }, { "freezemix", "Givre" },
        { "blurmix", "Mirage" }, { "arpfxmix", "FX Arp" },
        { "crystallize", "Cristal" }, { "iridescence", "Iris" },
        { "movdepth", "Mouvmt" }, { "shimmer", "Vivant" },
    };
    auto& apvts = proc.getAPVTS();
    for (int i = 0; i < numMixKnobs; ++i)
    {
        mixKnobs[i].setSliderStyle(juce::Slider::RotaryVerticalDrag);
        mixKnobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
        addAndMakeVisible(mixKnobs[i]);
        mixLabels[i].setText(table[i].name, juce::dontSendNotification);
        mixLabels[i].setJustificationType(juce::Justification::centred);
        mixLabels[i].setFont(juce::Font(11.0f, juce::Font::bold));
        addAndMakeVisible(mixLabels[i]);
        mixAtts[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, table[i].id, mixKnobs[i]);
    }

    // ---- ACCES A LA PARTIE RECORDER RETIRE ----
    // Les 8 potards EFFECT MIX restent affiches mais sont gris et
    // inutilisables. Leurs valeurs et les parametres APVTS ne changent pas :
    // aucun DSP, aucun ID de parametre touche.
    for (int i = 0; i < numMixKnobs; ++i)
        mixKnobs[i].setEnabled(false);

    recBtn.setButtonText("REC");
    recBtn.onClick = [this]
    {
        int st = proc.recorder.state.load();
        if (st == RecorderEngine::Idle)
            proc.recArmFromUI();
        else if (st == RecorderEngine::WaitingBar)
            proc.recForceStartFromUI(); // 2e appui = demarrage immediat
        refresh();
    };
    addAndMakeVisible(recBtn);

    stopBtn.setButtonText("STOP");
    stopBtn.onClick = [this]
    {
        proc.recStopFromUI();
        refresh();
    };
    addAndMakeVisible(stopBtn);

    nowBtn.setButtonText("START NOW");
    nowBtn.setTooltip("Secours : demarre immediatement sans attendre la mesure");
    nowBtn.onClick = [this]
    {
        proc.recForceStartFromUI();
        refresh();
    };
    addAndMakeVisible(nowBtn);

    backBtn.setButtonText("BACK TO PRISME");
    backBtn.onClick = [this] { if (onBack) onBack(); };
    addAndMakeVisible(backBtn);
    // Seule sortie de la vue : masquee ET inutilisable, comme demande.
    backBtn.setVisible(false);
    backBtn.setEnabled(false);

    fileLabel.setFont(juce::Font(12.0f));
    fileLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(fileLabel);

    folderLabel.setFont(juce::Font(12.0f));
    folderLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(folderLabel);

    browseBtn.setButtonText("Dossier...");    browseBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser>(
            "Dossier des prises PRISME", proc.recorder.getDestFolder());
        auto flags = juce::FileBrowserComponent::openMode
                   | juce::FileBrowserComponent::canSelectDirectories;
        chooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            auto dir = fc.getResult();
            if (dir != juce::File())
                proc.recorder.setDestFolder(dir);
            refresh();
        });
    };
    addAndMakeVisible(browseBtn);

    hintLabel.setFont(juce::Font(12.0f));
    hintLabel.setJustificationType(juce::Justification::topLeft);
    hintLabel.setText(
        "REC arme au temps 1 suivant (transport DAW). "
        "Sans transport : demarrage immediat (SYNC OFF). "
        "BACK TO PRISME ne coupe jamais la prise.",
        juce::dontSendNotification);
    addAndMakeVisible(hintLabel);

    // LOOP STACK : historique des prises au-dessus du spectrogramme.
    stackTitleLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    stackTitleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(stackTitleLabel);
    stackViewport.setViewedComponent(&stackList, false);
    stackViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(stackViewport);
    stackEmptyLabel.setFont(juce::Font(12.0f, juce::Font::italic));
    stackEmptyLabel.setJustificationType(juce::Justification::centred);
    stackEmptyLabel.setText("Aucune loop - REC puis STOP",
                           juce::dontSendNotification);
    addAndMakeVisible(stackEmptyLabel);

    // RANDOMIZE LAB : longueur + INTENSITY + SEED sur la meme rangee que
    // RANDOMIZE, toujours visibles (c'est le jeu : 10 % subtil -> 95 % extreme).
    varBarsLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    varBarsLabel.setJustificationType(juce::Justification::centredRight);
    varBarsLabel.setText("Mesures", juce::dontSendNotification);
    addAndMakeVisible(varBarsLabel);
    varBarsBox.addItem("4", 4);
    varBarsBox.addItem("8", 8);
    varBarsBox.setSelectedId(8, juce::dontSendNotification); // defaut : 8 mesures
    addAndMakeVisible(varBarsBox);

    varIntLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    varIntLabel.setJustificationType(juce::Justification::centredRight);
    varIntLabel.setText("INTENSITY", juce::dontSendNotification);
    addAndMakeVisible(varIntLabel);
    intensitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    intensitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 46, 20);
    intensitySlider.setRange(0.0, 100.0, 1.0);
    intensitySlider.setValue(40.0, juce::dontSendNotification); // CREATIVE
    intensitySlider.setTooltip("0-20 subtil, 21-45 creatif, 46-75 experimental, 76-100 extreme");
    addAndMakeVisible(intensitySlider);

    varSeedLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    varSeedLabel.setJustificationType(juce::Justification::centredRight);
    varSeedLabel.setText("SEED", juce::dontSendNotification);
    addAndMakeVisible(varSeedLabel);
    seedEditor.setInputRestrictions(6, "0123456789");
    seedEditor.setTooltip("Vide = seed auto. Tape une seed pour rejouer un plan exact");
    seedEditor.onTextChange = [this]
    {
        // Frappe manuelle = rejeu exact voulu (plus de tirage auto).
        if (! settingSeedGui)
            seedAuto = false;
    };
    seedEditor.onReturnKey = [this] { startVariationFromUI(false); };
    addAndMakeVisible(seedEditor);

    randomizeBtn.setButtonText("RANDOMIZE");
    randomizeBtn.onClick = [this] { startVariationFromUI(false); };
    addAndMakeVisible(randomizeBtn);
    rerollBtn.setButtonText("REROLL");
    rerollBtn.setTooltip("Nouvelle seed + nouvelle variation depuis la meme source");
    rerollBtn.onClick = [this] { startVariationFromUI(true); };
    addAndMakeVisible(rerollBtn);

    varStatusLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    varStatusLabel.setJustificationType(juce::Justification::centredLeft);
    varStatusLabel.setText("READY", juce::dontSendNotification);
    addAndMakeVisible(varStatusLabel);

    rebuildStack();

    applySkin(skin);
}

void RecorderView::TakeRow::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(selected ? rowSkin.panelBottom.brighter(0.14f)
                         : juce::Colour(0xff0d0f14));
    g.fillRect(b);
    if (selected)
    {
        g.setColour(rowSkin.accent);
        g.drawRect(b, 1.5f);
    }
    // Mini-forme d'onde (pics de la prise, couleurs du skin).
    float x0 = 448.0f + (float) indentPx;
    float x1 = (float) getWidth() - 104.0f;
    if (x1 > x0 + 20.0f && ! peaks.empty())
    {
        float midY = (float) h * 0.5f;
        g.setColour((fileMissing ? rowSkin.textDim : rowSkin.accent).withAlpha(0.9f));
        int n = (int) peaks.size();
        for (int x = (int) x0; x < (int) x1; ++x)
        {
            int c = (x - (int) x0) * n / ((int) (x1 - x0));
            c = juce::jlimit(0, n - 1, c);
            float v = juce::jlimit(0.0f, 1.0f, peaks[(size_t) c]);
            float y = v * (midY - 3.0f);
            g.drawLine((float) x, midY - y, (float) x, midY + y + 1.0f, 1.0f);
        }
    }
    // Separateur fin.
    g.setColour(rowSkin.panelEdge.withAlpha(0.5f));
    g.drawLine(0.0f, (float) h - 0.5f, (float) getWidth(), (float) h - 0.5f);
}

void RecorderView::TakeRow::resized()
{
    int w = getWidth();
    numLabel.setBounds(4, 0, 36, h);
    nameLabel.setBounds(44 + indentPx, 0, 196 - indentPx, h);
    durLabel.setBounds(244, 0, 58, h);
    bpmLabel.setBounds(306, 0, 58, h);
    barsLabel.setBounds(368, 0, 58, h);
    playBtn.setBounds(w - 96, 3, 28, 28); // triangle lecture/pause
    stopBtn.setBounds(w - 64, 3, 28, 28); // carre stop
    delBtn.setBounds(w - 32, 3, 28, 28);  // croix suppression
}

void RecorderView::TakeRow::mouseDown(const juce::MouseEvent& e)
{
    dragStartPos = e.position;
    // Le drag ne s'arme QUE sur la forme d'onde (le clic ailleurs = selection).
    dragArmed = waveZone().contains(e.position);
    if (onSelect) onSelect(takeNumber);
}

void RecorderView::TakeRow::mouseDrag(const juce::MouseEvent& e)
{
    // Seuil : un simple clic (selection/preecoute) ne glisse jamais.
    if (! dragArmed || (e.position - dragStartPos).getDistanceFromOrigin() < 8.0f)
        return;
    dragArmed = false;
    if (confirming) // annule une demande X plutot que glisser
    {
        confirming = false;
        repaint();
        return;
    }
    juce::File f = onFileForDrag ? onFileForDrag(takeNumber) : juce::File();
    if (f == juce::File() || ! f.existsAsFile())
        return; // manquant ou non finalise : pas de glissement, pas de surprise
    // Copie locale refermee avant le glisser (canMoveFiles=false) :
    // l'original reste intact, et le DAW recoit un fichier qu'il peut
    // librement reecrire. Duree/sr/canneaux reels sont ceux du vrai WAV.
    const juce::File drag = PrismeDragDrop::makeDraggableCopy(f);
    juce::DragAndDropContainer::performExternalDragDropOfFiles(
        juce::StringArray(drag.getFullPathName()), false, this, nullptr);
}

void RecorderView::TakeRow::mouseUp(const juce::MouseEvent&)
{
    dragArmed = false;
}

void RecorderView::TakeRow::mouseMove(const juce::MouseEvent& e)
{
    // Curseur main uniquement sur la forme d'onde (zone de drag).
    setMouseCursor(waveZone().contains(e.position)
        ? juce::MouseCursor::DraggingHandCursor
        : juce::MouseCursor::NormalCursor);
}

int RecorderView::takeIndexByNumber(int number) const
{
    for (size_t i = 0; i < proc.takeList.size(); ++i)
        if (proc.takeList[i].number == number)
            return (int) i;
    return -1;
}

void RecorderView::rebuildStack()
{
    rows.clear();
    for (size_t i = 0; i < proc.takeList.size(); ++i)
    {
        const auto& t = proc.takeList[i];
        auto row = std::make_unique<TakeRow>();
        row->takeNumber = t.number;
        row->indentPx = t.isVariation ? 16 : 0;
        row->peaks = t.peaks;
        row->fileMissing = t.fileMissing || ! t.file.existsAsFile();
        row->selected = (proc.selectedTake == (int) i);
        row->playing = (previewNumber == t.number) && proc.preview.isPlaying();
        row->rowSkin = skin;
        for (auto* lb : { &row->numLabel, &row->nameLabel, &row->durLabel,
                          &row->bpmLabel, &row->barsLabel })
        {
            lb->setFont(juce::Font(11.5f));
            lb->setJustificationType(juce::Justification::centredLeft);
            lb->setColour(juce::Label::textColourId, skin.textMain);
            row->addAndMakeVisible(lb);
        }
        row->numLabel.setText(juce::String::formatted("%02d", t.number),
                              juce::dontSendNotification);
        row->nameLabel.setText(t.name + (row->fileMissing ? "  (manquant)" : ""),
                               juce::dontSendNotification);
        row->durLabel.setText(formatElapsed(t.durationSec), juce::dontSendNotification);
        row->bpmLabel.setText(t.bpmText, juce::dontSendNotification);
        row->barsLabel.setText(t.barsText, juce::dontSendNotification);
        row->durLabel.setColour(juce::Label::textColourId, skin.textDim);
        row->bpmLabel.setColour(juce::Label::textColourId, skin.textDim);
        row->barsLabel.setColour(juce::Label::textColourId, skin.textDim);
        row->playBtn.setButtonText(row->playing ? "II" : ">");
        row->playBtn.setEnabled(! row->fileMissing);
        row->playBtn.setColour(juce::TextButton::buttonColourId, skin.buttonBg);
        row->playBtn.setColour(juce::TextButton::textColourOffId, skin.buttonText);
        row->stopBtn.setButtonText("[]");
        row->stopBtn.setEnabled(row->playing);
        row->stopBtn.setColour(juce::TextButton::buttonColourId, skin.buttonBg);
        row->stopBtn.setColour(juce::TextButton::textColourOffId, skin.buttonText);
        row->delBtn.setButtonText("X");
        row->delBtn.setColour(juce::TextButton::buttonColourId, skin.buttonBg);
        row->delBtn.setColour(juce::TextButton::textColourOffId, skin.buttonText);
        row->addAndMakeVisible(row->playBtn);
        row->addAndMakeVisible(row->stopBtn);
        row->addAndMakeVisible(row->delBtn);
        row->onSelect = [this](int n)
        {
            int idx = takeIndexByNumber(n);
            if (idx >= 0)
            {
                proc.selectedTake = idx;
                for (auto& r : rows) // changer de ligne annule une demande X
                    r->confirming = false;
                refreshRowStates();
            }
        };
        // Suppression en 2 temps (sans modale) : 1er clic X = demande
        // ("Liste" / "WAV?"), 2e clic = acte. Le fichier ne part qu'a "WAV?"
        // (vers la corbeille systeme, recuperable). Jamais de surprise.
        row->onPlay = [this](int n)
        {
            int idx = takeIndexByNumber(n);
            if (idx < 0)
                return;
            for (auto& r : rows)
            {
                if (r->takeNumber == n && r->confirming)
                {
                    r->confirming = false; // "Liste" = retire l'entree, WAV garde
                    removeEntryByNumber(n, false);
                    return;
                }
                r->confirming = false;
            }
            togglePreviewByNumber(n);
        };
        row->onStop = [this](int n)
        {
            if (previewNumber == n)
            {
                proc.takeStopPreview();
                previewNumber = -1;
                refreshRowStates();
            }
        };
        row->onDelete = [this](int n)
        {
            int idx = takeIndexByNumber(n);
            if (idx < 0)
                return;
            for (auto& r : rows)
            {
                if (r->takeNumber == n && r->confirming)
                {
                    r->confirming = false; // "WAV?" = corbeille + retiree
                    removeEntryByNumber(n, true);
                    return;
                }
                r->confirming = false;
            }
            if (idx >= 0 && (size_t) idx < rows.size())
            {
                rows[(size_t) idx]->confirming = true; // 1er clic = demande
                refreshRowStates();
            }
        };
        row->playBtn.onClick = [rowPtr = row.get()] { if (rowPtr->onPlay) rowPtr->onPlay(rowPtr->takeNumber); };
        row->stopBtn.onClick = [rowPtr = row.get()] { if (rowPtr->onStop) rowPtr->onStop(rowPtr->takeNumber); };
        row->delBtn.onClick = [rowPtr = row.get()] { if (rowPtr->onDelete) rowPtr->onDelete(rowPtr->takeNumber); };
        // Fichier reel pour le glisser vers le DAW (vide = pas de glissement).
        // La liste ne contient que des prises finalisees ; on re-verifie ici
        // le cas d'un fichier supprime a la main entre-temps.
        row->onFileForDrag = [this](int n) -> juce::File
        {
            int idx = takeIndexByNumber(n);
            if (idx < 0 || idx >= (int) proc.takeList.size())
                return {};
            const auto& te = proc.takeList[(size_t) idx];
            if (! te.file.existsAsFile())
                return {};
            return te.file;
        };
        stackList.addAndMakeVisible(row.get());
        rows.push_back(std::move(row));
    }
    int w = juce::jmax(600, stackViewport.getWidth() - ((int) rows.size() * TakeRow::h > stackViewport.getHeight() ? stackViewport.getScrollBarThickness() : 0));
    stackList.setSize(w, (int) rows.size() * TakeRow::h);
    for (size_t i = 0; i < rows.size(); ++i)
        rows[i]->setBounds(0, (int) i * TakeRow::h, w, TakeRow::h);
    stackTitleLabel.setText(
        "LOOP STACK - " + juce::String((int) rows.size())
        + " prise(s) - glisser la forme d'onde vers le DAW",
        juce::dontSendNotification);
    stackEmptyLabel.setVisible(rows.empty());
}

void RecorderView::refreshRowStates()
{
    bool canPreview = proc.recorder.state.load() == RecorderEngine::Idle;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        auto& r = rows[i];
        r->selected = (proc.selectedTake == (int) i);
        r->playing = (previewNumber == r->takeNumber) && proc.preview.isPlaying();
        // Mode confirmation X : les boutons deviennent "Liste" / "WAV?".
        r->playBtn.setButtonText(r->confirming ? "Liste" : (r->playing ? "II" : ">"));
        r->delBtn.setButtonText(r->confirming ? "WAV?" : "X");
        r->playBtn.setEnabled(canPreview && ! r->fileMissing);
        r->stopBtn.setEnabled(r->playing);
        r->repaint();
    }
}

void RecorderView::togglePreviewByNumber(int number)
{
    int idx = takeIndexByNumber(number);
    if (idx < 0)
        return;
    proc.takeStopPreview(); // repart d'un etat sur (bascule propre)
    proc.takePreviewAt(idx);
    previewNumber = proc.preview.isPlaying() ? number : -1;
    if (previewNumber >= 0)
        proc.selectedTake = idx;
    refreshRowStates();
}

void RecorderView::removeEntryByNumber(int number, bool deleteFile)
{
    int idx = takeIndexByNumber(number); // re-resout (la liste a pu bouger)
    if (idx < 0)
        return;
    if (previewNumber == number)
    {
        proc.takeStopPreview();
        previewNumber = -1;
    }
    proc.takeRemoveAt(idx, deleteFile);
    rebuildStack();
    refresh();
}

void RecorderView::startVariationFromUI(bool reroll)
{
    // RANDOMIZE = toujours une seed fraiche, sauf seed tapee a la main
    // (rejeu exact voulu : meme seed + meme config = meme plan).
    // REROLL = toujours une seed fraiche.
    juce::String txt = seedEditor.getText().trim();
    uint32_t seed = 0;
    if (reroll || seedAuto || txt.isEmpty())
    {
        seed = (uint32_t) juce::Random::getSystemRandom().nextInt();
        if (seed == 0) seed = 1; // 0 = reserve (auto cote moteur)
        settingSeedGui = true;
        seedEditor.setText(juce::String(seed), juce::dontSendNotification);
        settingSeedGui = false;
        seedAuto = true;
    }
    else
    {
        seed = (uint32_t) txt.getIntValue();
        if (seed == 0) // "0" tape = auto (0 reserve cote moteur)
        {
            seed = (uint32_t) juce::Random::getSystemRandom().nextInt();
            if (seed == 0) seed = 1;
            settingSeedGui = true;
            seedEditor.setText(juce::String(seed), juce::dontSendNotification);
            settingSeedGui = false;
            seedAuto = true;
        }
    }
    int bars = varBarsBox.getSelectedId();
    if (bars != 4) bars = 8; // variations calees sur 4 ou 8 mesures
    proc.variationStart(proc.selectedTake, bars,
                        (int) intensitySlider.getValue(), seed, false);
    refresh();
}

juce::String RecorderView::formatElapsed(double seconds)
{
    if (seconds < 0) seconds = 0;
    int m = (int) (seconds / 60.0);
    int s = (int) std::fmod(seconds, 60.0);
    int t = (int) std::fmod(seconds * 10.0, 10.0);
    return juce::String::formatted("%02d:%02d.%d", m, s, t);
}

void RecorderView::updateLabels(int st, double elapsedSec, bool fallback)
{
    juce::String txt = RecorderEngine::stateName(st);
    if (st == RecorderEngine::WaitingBar && fallback)
        txt = "WAITING FOR BAR";
    else if ((st == RecorderEngine::Recording) && fallback)
        txt += " (SYNC OFF)";
    if (proc.recorder.autoStopped.load() && st == RecorderEngine::Recording)
        txt += " - FULL";
    if (txt != cachedStateText)
    {
        cachedStateText = txt;
        stateLabel.setText(txt, juce::dontSendNotification);
        // Couleur d'etat : rouge en prise, ambre en attente/sauvegarde.
        if (st == RecorderEngine::Recording)
            stateLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff5e5e));
        else if (st == RecorderEngine::WaitingBar || st == RecorderEngine::Saving)
            stateLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffc36b));
        else
            stateLabel.setColour(juce::Label::textColourId, skin.textDim);
    }
    elapsedLabel.setText(formatElapsed(elapsedSec), juce::dontSendNotification);

    double bpm = proc.hostBpmCache.load();
    int num = proc.hostSigNum.load();
    int den = proc.hostSigDen.load();
    bool playing = proc.hostPlaying.load();
    bool hasTr = proc.hostHasTransport.load();
    juce::String tt;
    if (hasTr)
        tt = juce::String::formatted("%.1f BPM  %d/%d  %s", bpm, num, den,
                                     playing ? "PLAY" : "STOP");
    else
        tt = "Tempo : -- (hors DAW)";
    tempoLabel.setText(tt, juce::dontSendNotification);

    juce::String fn = proc.recorder.getLastFileName();
    int saved = proc.recorder.savedSamples.load();
    if (st == RecorderEngine::Saving)
        fileLabel.setText("Sauvegarde WAV en cours...", juce::dontSendNotification);
    else if (fn.isNotEmpty() && saved > 0)
        fileLabel.setText("WAV : " + fn, juce::dontSendNotification);
    else if (st == RecorderEngine::Recording || st == RecorderEngine::WaitingBar)
        fileLabel.setText("WAV : (sera nomme a STOP)", juce::dontSendNotification);
    else
        fileLabel.setText("WAV : aucune prise", juce::dontSendNotification);

    folderLabel.setText("Dossier : " + proc.recorder.getDestFolder().getFullPathName(),
                        juce::dontSendNotification);

    recBtn.setEnabled(st == RecorderEngine::Idle || st == RecorderEngine::WaitingBar);
    nowBtn.setEnabled(st == RecorderEngine::WaitingBar);
    stopBtn.setEnabled(st == RecorderEngine::Recording || st == RecorderEngine::WaitingBar);
}

void RecorderView::refresh()
{
    // Finalisation auto si capacite max atteinte (thread message : autorise).
    if (proc.recorder.autoStopped.load()
        && proc.recorder.state.load() == RecorderEngine::Recording)
    {
        proc.recStopFromUI();
    }
    // Ajout auto apres finalisation confirmee (WAV valide).
    // La miniature est figee a partir des buffers stables (etat Idle).
    if (proc.takeConsumeFresh() >= 0)
        rebuildStack();
    // Variation terminee : insertion sous sa source + statut.
    if (proc.variationConsumeDone() >= 0)
        rebuildStack();
    juce::String vs = proc.variationStatusText();
    if (vs != cachedVarStatus)
    {
        cachedVarStatus = vs;
        varStatusLabel.setText(vs, juce::dontSendNotification);
        int vjs = proc.varJobState.load();
        if (vjs == 4)
            varStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff5e5e));
        else if (vjs == 1 || vjs == 2)
            varStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffc36b));
        else if (vjs == 3)
            varStatusLabel.setColour(juce::Label::textColourId, skin.accentHi);
        else
            varStatusLabel.setColour(juce::Label::textColourId, skin.textDim);
    }
    bool vbusy = proc.varJobState.load() == 1 || proc.varJobState.load() == 2;
    randomizeBtn.setEnabled(! vbusy);
    rerollBtn.setEnabled(! vbusy);
    // Fin naturelle de la pre-ecoute : retombe l'indicateur.
    if (previewNumber >= 0 && ! proc.preview.isPlaying())
    {
        previewNumber = -1;
        refreshRowStates();
    }
    // Relief 3D : draine le FIFO seulement quand cette vue est visible.
    // (Le spectrogramme 2D de PRISME draine quand c'est lui qui est visible.)
    if (isVisible())
    {
        spectro3d.setSampleRate(proc.getSampleRate());
        spectro3d.pushFromFifo(proc.spectrumFifo, proc.spectrumBuf);
        spectro3d.update();
    }
    int st = proc.recorder.state.load();
    double sr = proc.recorder.sampleRateHz.load();
    if (sr <= 0) sr = 44100.0;
    double elapsed = 0.0;
    if (st == RecorderEngine::Recording || st == RecorderEngine::WaitingBar)
        elapsed = (double) proc.recorder.writeIndex.load() / sr;
    else
        elapsed = (double) proc.recorder.savedSamples.load() / sr;
    bool fb = proc.recorder.fallbackUsed.load();
    updateLabels(st, elapsed, fb);
    // Etats des lignes : seulement si l'etat prise/lecture a change.
    bool pv = proc.preview.isPlaying();
    if (st != lastRowRecState || pv != lastRowPreview)
    {
        lastRowRecState = st;
        lastRowPreview = pv;
        refreshRowStates();
    }

    float pk = juce::jmax(proc.recorder.peakL.load(), proc.recorder.peakR.load());
    if (pk >= meterLevel)
        meterLevel = pk;
    else
        meterLevel = meterLevel * 0.85f + pk * 0.15f;
    if (st != RecorderEngine::Recording)
        meterLevel *= 0.9f;
    updateLiveWave(st);
    repaint();
}

void RecorderView::applySkin(const PrismeSkin& sk)
{
    skin = sk;
    setLookAndFeel(nullptr);
    titleLabel.setColour(juce::Label::textColourId, sk.textMain);
    elapsedLabel.setColour(juce::Label::textColourId, sk.readoutText);
    tempoLabel.setColour(juce::Label::textColourId, sk.textDim);
    fileLabel.setColour(juce::Label::textColourId, sk.textMain);
    folderLabel.setColour(juce::Label::textColourId, sk.textDim);
    hintLabel.setColour(juce::Label::textColourId, sk.textDim);
    waveTitleLabel.setColour(juce::Label::textColourId, sk.sectionTitle);
    mixTitleLabel.setColour(juce::Label::textColourId, sk.sectionTitle);
    stackTitleLabel.setColour(juce::Label::textColourId, sk.sectionTitle);
    stackEmptyLabel.setColour(juce::Label::textColourId, sk.textDim);
    for (auto& r : rows)
    {
        r->rowSkin = sk;
        for (auto* lb : { &r->numLabel, &r->nameLabel })
            lb->setColour(juce::Label::textColourId, sk.textMain);
        for (auto* lb : { &r->durLabel, &r->bpmLabel, &r->barsLabel })
            lb->setColour(juce::Label::textColourId, sk.textDim);
        r->playBtn.setColour(juce::TextButton::buttonColourId, sk.buttonBg);
        r->playBtn.setColour(juce::TextButton::textColourOffId, sk.buttonText);
        r->stopBtn.setColour(juce::TextButton::buttonColourId, sk.buttonBg);
        r->stopBtn.setColour(juce::TextButton::textColourOffId, sk.buttonText);
        r->delBtn.setColour(juce::TextButton::buttonColourId, sk.buttonBg);
        r->delBtn.setColour(juce::TextButton::textColourOffId, sk.buttonText);
        r->repaint();
    }
    // Potentiomètres compacts : repere contraste + valeur lisible.
    for (int i = 0; i < numMixKnobs; ++i)
    {
        mixLabels[i].setColour(juce::Label::textColourId, sk.textMain);
        mixKnobs[i].setColour(juce::Slider::rotarySliderFillColourId, sk.accent);
        mixKnobs[i].setColour(juce::Slider::rotarySliderOutlineColourId, sk.trackBg);
        mixKnobs[i].setColour(juce::Slider::thumbColourId, sk.accentHi);
        mixKnobs[i].setColour(juce::Slider::textBoxTextColourId, sk.readoutText);
        mixKnobs[i].setColour(juce::Slider::textBoxBackgroundColourId, sk.readoutBg);
    }
    // Relief 3D suit le skin actif (fond graphite fixe).
    spectro3d.setColors(sk.accentDeep, sk.accent, sk.accentHi);
    for (auto* b : { &recBtn, &stopBtn, &nowBtn, &backBtn, &browseBtn })
    {
        b->setColour(juce::TextButton::buttonColourId, sk.buttonBg);
        b->setColour(juce::TextButton::textColourOffId, sk.buttonText);
        b->setColour(juce::TextButton::textColourOnId, sk.buttonActiveText);
    }
    recBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8a1f2e));
    recBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    // VAR LAB : memes couleurs que le reste, INTENSITY lisible en permanence.
    varBarsLabel.setColour(juce::Label::textColourId, sk.textMain);
    varIntLabel.setColour(juce::Label::textColourId, sk.textMain);
    varSeedLabel.setColour(juce::Label::textColourId, sk.textMain);
    varBarsBox.setColour(juce::ComboBox::backgroundColourId, sk.comboBg);
    varBarsBox.setColour(juce::ComboBox::textColourId, sk.textMain);
    intensitySlider.setColour(juce::Slider::trackColourId, sk.trackBg);
    intensitySlider.setColour(juce::Slider::thumbColourId, sk.accent);
    intensitySlider.setColour(juce::Slider::textBoxTextColourId, sk.readoutText);
    intensitySlider.setColour(juce::Slider::textBoxBackgroundColourId, sk.readoutBg);
    seedEditor.setColour(juce::TextEditor::backgroundColourId, sk.comboBg);
    seedEditor.setColour(juce::TextEditor::textColourId, sk.textMain);
    seedEditor.setColour(juce::TextEditor::outlineColourId, sk.panelEdge);
    seedEditor.setColour(juce::TextEditor::focusedOutlineColourId, sk.accent);
    seedEditor.setColour(juce::CaretComponent::caretColourId, sk.accent);
    for (auto* b : { &randomizeBtn, &rerollBtn })
    {
        b->setColour(juce::TextButton::buttonColourId, sk.buttonBg);
        b->setColour(juce::TextButton::textColourOffId, sk.buttonText);
        b->setColour(juce::TextButton::textColourOnId, sk.buttonActiveText);
    }
    randomizeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1f5a8a));
    randomizeBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    backBtn.setColour(juce::TextButton::buttonColourId, sk.buttonActive);
    backBtn.setColour(juce::TextButton::textColourOffId, sk.buttonActiveText);
    cachedStateText = "__reset__";
    repaint();
}

void RecorderView::paint(juce::Graphics& g)
{
    g.fillAll(skin.windowBg);
    // Bandeau irise identique a PRISME (coherence visuelle).
    juce::ColourGradient grad(juce::Colour(0xffff5e8a), 0, 0,
                              juce::Colour(0xff7df9ff), 880.0f, 0, false);
    grad.addColour(0.25, juce::Colour(0xffffc36b));
    grad.addColour(0.5, juce::Colour(0xff9d7bff));
    grad.addColour(0.75, juce::Colour(0xff5effc3));
    g.setGradientFill(grad);
    g.fillRect(0, 0, 880, 6);

    spectro3d.draw(g, juce::Rectangle<float>(24.0f, 258.0f, 832.0f, 228.0f));
    paintMeter(g, juce::Rectangle<float>(24.0f, 490.0f, 832.0f, 10.0f));

    // Forme d'onde en direct de la prise en cours (sous le texte d'aide)
    {
        juce::Rectangle<float> a(24.0f, 901.0f, 832.0f, 34.0f);
        g.setColour(skin.trackBg);
        g.fillRoundedRectangle(a, 4.0f);
        const float mid = a.getCentreY();
        const float half = a.getHeight() * 0.5f - 2.0f;
        g.setColour(skin.accent);
        for (int i = 0; i < liveCols; ++i)
        {
            const float v = juce::jlimit(0.0f, 1.0f, livePk[(size_t) i]);
            if (v <= 0.001f) continue;
            g.fillRect(a.getX() + (float) i, mid - v * half, 1.0f, 2.0f * v * half);
        }
        g.setColour(skin.panelEdge);
        g.drawRoundedRectangle(a, 4.0f, 1.0f);
    }
}

void RecorderView::updateLiveWave(int st)
{
    const bool starting = (st == RecorderEngine::Recording && liveLastSt != RecorderEngine::Recording);
    if (st == RecorderEngine::WaitingBar || starting)
    {
        std::fill(livePk.begin(), livePk.end(), 0.0f);
        liveIdx = 0;
        liveAcc = 0.0f;
        liveAccN = 0;
    }
    liveLastSt = st;
    if (st != RecorderEngine::Recording)
        return; // apres STOP : la derniere prise reste affichee

    const int w = proc.recorder.writeIndex.load();
    const float* L = proc.recorder.dataL();
    const float* R = proc.recorder.dataR();
    if (L == nullptr || R == nullptr || w > proc.recorder.getCapacity())
        return;
    if (w < liveIdx)
        liveIdx = 0;
    double sr = proc.recorder.sampleRateHz.load();
    if (sr <= 0.0) sr = 44100.0;
    const int spc = juce::jmax(1, (int) (sr * 10.0 / (double) liveCols));
    while (liveIdx < w) // seulement les nouveaux echantillons (cout negligeable)
    {
        liveAcc = juce::jmax(liveAcc, juce::jmax(std::fabs(L[liveIdx]), std::fabs(R[liveIdx])));
        ++liveIdx;
        if (++liveAccN >= spc)
        {
            std::move(livePk.begin() + 1, livePk.end(), livePk.begin());
            livePk.back() = liveAcc;
            liveAcc = 0.0f;
            liveAccN = 0;
        }
    }
}


void RecorderView::paintMeter(juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour(skin.trackBg);
    g.fillRoundedRectangle(area, 4.0f);
    float v = juce::jlimit(0.0f, 1.0f, meterLevel);
    if (v > 0.001f)
    {
        juce::Colour c = v > 0.95f ? juce::Colour(0xffff5e5e) : skin.accent;
        g.setColour(c);
        g.fillRoundedRectangle(area.withWidth(area.getWidth() * v), 4.0f);
    }
    g.setColour(skin.panelEdge);
    g.drawRoundedRectangle(area, 4.0f, 1.0f);
}

void RecorderView::resized()
{
    // Meme espace logique 880x970 que le contenu PRISME.
    // Bandeau : PRISME + RECORDER + statut + BACK.
    titleLabel.setBounds(146, 8, 588, 30);
    backBtn.setBounds(740, 12, 124, 26);
    // Haut : RANDOMIZE + REROLL + mesures + INTENSITY sur une rangee.
    randomizeBtn.setBounds(24, 44, 100, 26);
    rerollBtn.setBounds(130, 44, 60, 26);
    varBarsLabel.setBounds(196, 44, 50, 26);
    varBarsBox.setBounds(250, 44, 56, 26);
    varIntLabel.setBounds(312, 44, 66, 26);
    intensitySlider.setBounds(378, 44, 170, 26);
    // Ligne statut : titre stack + etat prise + tempo.
    stackTitleLabel.setBounds(24, 70, 400, 20);
    stateLabel.setBounds(430, 70, 180, 20);
    tempoLabel.setBounds(616, 70, 240, 20);
    // LOOP STACK : 4 lignes visibles (34 px), defilement au-dela.
    stackViewport.setBounds(24, 92, 832, 142);
    stackEmptyLabel.setBounds(24, 92, 832, 142);
    {
        int w = juce::jmax(600, stackViewport.getWidth() - ((int) rows.size() * TakeRow::h > stackViewport.getHeight() ? stackViewport.getScrollBarThickness() : 0));
        stackList.setSize(w, juce::jmax(1, (int) rows.size()) * TakeRow::h);
        for (size_t i = 0; i < rows.size(); ++i)
            rows[i]->setBounds(0, (int) i * TakeRow::h, w, TakeRow::h);
    }
    waveTitleLabel.setBounds(24, 238, 400, 18);
    // Spectrogramme 3D + metre dessines dans paint (24,258,832,228).
    // EFFECT MIX : 2 rangees de 4 potentiomètres compacts.
    mixTitleLabel.setBounds(24, 504, 832, 20);
    const int kw = 150, kh = 76, gap = 77;
    for (int i = 0; i < numMixKnobs; ++i)
    {
        int row = i / 4, col = i % 4;
        int x = 24 + col * (kw + gap);
        int y = 528 + row * 96;
        mixKnobs[i].setBounds(x, y, kw, kh);
        mixLabels[i].setBounds(x, y + kh, kw, 16);
    }
    // Bas : SEED + statut (le reste a demenage en haut).
    varSeedLabel.setBounds(24, 722, 44, 30);
    seedEditor.setBounds(72, 722, 70, 30);
    varStatusLabel.setBounds(152, 722, 704, 30);
    // Zone enregistrement : commandes + duree + fichier (decales sous le lab).
    recBtn.setBounds(24, 758, 140, 52);
    stopBtn.setBounds(172, 758, 140, 52);
    nowBtn.setBounds(320, 758, 140, 52);
    elapsedLabel.setBounds(480, 758, 376, 52);
    fileLabel.setBounds(24, 818, 832, 20);
    folderLabel.setBounds(24, 840, 620, 20);
    browseBtn.setBounds(652, 838, 204, 26);
    hintLabel.setBounds(24, 866, 832, 32);
}
