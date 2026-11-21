#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <memory>
#include "ScalePads.h"
#include "PrismeSkin.h"
#include "Spectrogram.h"
#include "Scope3D.h"
#include "PrismeLookAndFeel.h"
#include "RecorderView.h"

class PrismeProcessor;
class PrismeEditor;

// Calque de contenu : porte tous les enfants + tout le dessin, mis a l'echelle
// uniformement par l'editeur (1670x941 logique). Formes et proportions intactes.
struct PrismeContent : public juce::Component
{
    PrismeEditor& owner;
    explicit PrismeContent(PrismeEditor& o) : owner(o) {}
    void paint(juce::Graphics& g) override;
    // Decor "au-dessus" des controles (skin Post Apo seulement). N'intercepte
    // aucun clic : paintOverChildren ne gere que le dessin.
    void paintOverChildren(juce::Graphics& g) override;
    // Clics tableau SAMPLES : mouseDown = selection/boutons, mouseDrag = glisser
    // la prise vers le DAW, mouseUp = fin du geste.
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    bool dragArmed = false;          // un glisser est en attente de seuil
    int dragRow = -1;                // indice de ligne concernee
    juce::Point<float> dragStartPos; // position du mouseDown (seuil 8 px)
};

class PrismeEditor : public juce::AudioProcessorEditor,
                     private juce::Timer,
                     private juce::Slider::Listener,
                     private juce::ValueTree::Listener,
                     private juce::AsyncUpdater
{
public:
    explicit PrismeEditor(PrismeProcessor&);
    ~PrismeEditor() override;

    void paint(juce::Graphics&) override;
    void paintContent(juce::Graphics&);
    void paintContentOver(juce::Graphics&);
    void paintLiveWaveWindow(juce::Graphics&); // fenetre d'onde en direct du panneau SAMPLES
    void resized() override;
    // Appele par PrismeContent::mouseDown (calque de contenu). Public pour
    // la meme raison que paintContent : le calque delegue ses entrees souris.
    void handleSampleRowClickAt(int x, int y);
    // Indice de ligne du tableau SAMPLES sous (x,y), a condition que la
    // position soit GLISSABLE : ligne valide ET hors des boutons PLAY/STOP et
    // SUPPRIMER (ces deux la restent des clics, jamais des glissements).
    // Retourne -1 sinon.
    int sampleRowForDrag(int x, int y) const;
    // Glisse une ligne du tableau vers le DAW : copie locale refermee
    // (PrismeDragDrop), puis glisser systeme. Ne modifie jamais l'original.
    void dragSampleRowToDAW(int row);

private:
    void timerCallback() override;
    void sliderValueChanged(juce::Slider* slider) override;
    static juce::String formatFreq(double hz);
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    void handleAsyncUpdate() override;
    void setActivePreset(int menuId);
    PrismeProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    HardwareLookAndFeel hwLAF;
    PrismeContent content;
    RecorderView recorderView; // vue RECORDER : recouvre content, meme taille
    juce::TextButton recorderBtn; // bascule dans la bande superieure
    bool showingRecorder = false;
    void showRecorder(bool show);
    juce::ComponentBoundsConstrainer constrainer;
    juce::ResizableCornerComponent cornerResizer;

    // Panneau accords
    juce::ComboBox rootBox, scaleBox, octaveBox;
    juce::Slider givreSlider; // fader dry/wet horizontal a la place du bouton
    juce::Label givreLabel;
    ScalePads pads; // pastilles de gamme (clic = tonique)
    Spectrogram spectro; // analyse visuelle (bande superieure, thread UI uniquement)
    Scope3D scope3d; // oscilloscope 3D (panneau SPECTRUM, thread UI uniquement)
    int lastPadsRoot = -1, lastPadsScale = -1;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAtt, scaleAtt, octaveAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> givreAtt;

    // Panneau arp
    juce::ToggleButton arpOnBtn, arpSyncBtn;
    juce::TextButton undoBtn, redoBtn; // historique des reglages
    juce::ToggleButton autoKeyBtn; // AUTO KEY : tonalite detectee sur l'entree
    juce::ComboBox arpPatternBox, arpDivBox, arpOctBox, arpFxBox, arpFxBoxB, arpFxBoxC, arpRouteBox;
    juce::Label arpTitleLabel, swingLabel, arpFxLabel1, arpFxLabel2, arpFxMixLabel, arpRouteLabel;
    juce::Slider swingSlider, arpFxKnob1, arpFxKnob2, arpFxMixKnob;
    juce::TextButton fxSlotBtn0, fxSlotBtn1, fxSlotBtn2; // 1 bouton rouge par menu
    int fxSlotSel = 0;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> arpOnAtt, arpSyncAtt, autoKeyAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> arpPatternAtt, arpDivAtt, arpOctAtt, arpFxAtt, arpFxAttB, arpFxAttC, arpRouteAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> swingAtt, arpFxMixAtt;
    int lastFxType = -1;

    // Panneau mouvement
    juce::ToggleButton movSyncBtn;
    juce::ComboBox movDivBox;
    juce::Label movTitleLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> movSyncAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> movDivAtt;

    // Panneau EQ graphique 4 bandes (etage de sortie)
    juce::Slider eqSliders[4];
    juce::Label eqLabels[4];
    juce::Label eqTitleLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqAtts[4];

    // Fader de selection de frequence (bande d'effet, 3 valeurs)
    juce::Slider bandFader;
    juce::Label bandTitleLabel, bandHiLabel, bandLoLabel;
    bool updatingBandFader = false;
    bool updatingFxKnobs = false;

    // Tranche laterale : faders MIX + BLUR + taille (lies aux parametres)
    juce::Slider mixFader, blurFader, tailleSlider;
    juce::Label mixTitleLabel, blurTitleLabel, wetLabel, dryLabel, blurWetLabel, blurDryLabel, tailleLabel;
    juce::Slider mirPredelaySlider, mirColorSlider, mirWidthSlider, mirDecaySlider;
    juce::Label mirPredelayLabel, mirColorLabel, mirWidthLabel, mirDecayLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAtt, blurAtt, tailleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mirPredelayAtt, mirColorAtt, mirWidthAtt, mirDecayAtt;

    // CRYSTALLIZE : 1 potard dans la rangee FX de l'arpege (aucun controle deplace)
    juce::Slider cryoKnob;
    juce::Label cryoLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cryoAtt;

    // 3 LED de cible CRYSTALLIZE : 1 clic pour allumer, jamais deux ensemble.
    // Toutes eteintes = le Crystallize ne s'applique a rien (parametre 0).
    // Ordre de haut en bas = 1 "no arp", 2 "arp", 3 "all" (voir le parametre).
    // La LED allumee suit les couleurs du skin actif (PrismeLookAndFeel).
    juce::ToggleButton cryoLed[3];
    juce::Label cryoLedLabel[3]; // libelle a droite de chaque LED
    int lastCryoTarget = -1; // -1 = jamais lu : force le 1er rafraichissement

    // Sliders (les 4 derniers vivent dans le panneau mouvement)
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    std::vector<std::unique_ptr<Knob>> knobs;

    juce::ComboBox presetBox;
    juce::TextEditor presetNameEditor;
    juce::TextButton presetSaveBtn, presetDeleteBtn, randomBtn;
    juce::ComboBox skinBox; // selecteur de skin (haut gauche, ne deplace rien)
    juce::Label skinLabel;
    PrismeSkin activeSkin = PrismeSkins::crystal();
    juce::TextButton slotABtn, slotBBtn, slotCopyBtn; // A/B : comparer 2 reglages
    juce::ValueTree slotA, slotB;
    int activeSlot = 0;
    juce::Label titleLabel, footerLabel;

    // ---- Panneau SAMPLES (UI principale, format horizontal) ----
    // Tableau des prises reelles (proc.takeList). Chaque ligne porte un
    // PLAY/STOP (pre-ecoute, takePreviewAt) et un SUPPRIMER (takeRemoveAt).
    // L'info de variation (seed / intensite / mesures) s'affiche dans la
    // cellule dediee. Lecture seule sinon : rien n'est simule.
    struct SampleLine
    {
        int number = 0;
        juce::String name;
        double durationSec = 0.0;
        std::vector<float> peaks;
        bool selected = false;
        bool fileMissing = false;
        bool isVariation = false;   // vient d'un RANDOMIZE
        uint32_t seed = 0;          // seed de variation (0 = prise originale)
        int intensityPct = 0;       // intensite en %
        int varBars = 0;            // mesures de la variation
        bool previewing = false;    // ON actuellement (pre-ecoute en cours)
        juce::File file;            // chemin reel : sert au glisser vers le DAW
    };
    std::vector<SampleLine> sampleLines;   // copie legere de proc.takeList
    int sampleCacheCount = -1;             // cache anti-recopie (membres, pas static)
    int sampleCacheSel = -2;
    int sampleCachePreview = -2;           // numero en pre-ecoute (pour PLAY/STOP)
    int sampleConfirmIdx = -1;             // ligne armee pour suppression (2e clic = acte)
    void rebuildSampleLines();             // recopie takeList -> sampleLines

    // Barre de transport en haut du panneau SAMPLES (memes actions que le
    // Recorder, appellees sur les vraies methodes du processeur : rien n'est
    // simule). REC arme puis demarre, STOP arrete, RANDOMIZE/REROLL lancent
    // une variation depuis la prise selectionnee.
    juce::TextButton sampleRecBtn { "REC" };
    juce::TextButton sampleStopBtn { "STOP" };
    juce::TextButton sampleRandomBtn { "RANDOMIZE" };
    juce::TextButton sampleRerollBtn { "REROLL" };
    // LED de liaison RANDOMIZE / REROLL (section SAMPLES) : allumee, chaque
    // variation met un effet vedette en avant, qui avance d'un clic a l'autre.
    juce::ToggleButton sampleLinkLed;
    int activePreset = -1;      // -1 = manuel/personnalise ; sinon ID du menu (1-4 usine, 100+ user)
    bool applyingPreset = false;
    juce::StringArray userPresetNames;
    juce::Array<juce::File> userPresetFiles;

    void addKnob(const juce::String& paramID, const juce::String& name);
    void applyPreset(int index, int root, int scale, int oct, float decay, float attack, float bright, float irid,
                     float shimmer, float spread, float mix);
    void randomizeSound();
    void onPresetChosen();
    void refreshPresetList();
    void saveUserPreset();
    void deleteSelectedUserPreset();
    void refreshFxKnobs();
    void selectSlot(int slot);
    void refreshSlotColours();
    void copySlotToOther();
    void applySkin(int index);
    static juce::File getUserPresetDir();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PrismeEditor)
};
