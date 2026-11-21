#pragma once
// Vue RECORDER : remplace temporairement l'affichage PRISME dans la meme
// fenetre (memes dimensions, meme conteneur). Aucun onglet, aucune fenetre
// flottante. Le moteur vit dans le processeur : masquer cette vue ne coupe
// jamais la capture.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>
#include "PrismeSkin.h"
#include "Spectro3D.h"

class PrismeProcessor;

class RecorderView : public juce::Component
{
public:
    explicit RecorderView(PrismeProcessor& p);
    ~RecorderView() override = default;

    // Appeles par l'editeur (thread message, ~30 Hz).
    void refresh();
    void applySkin(const PrismeSkin& sk);

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onBack;

    // Pour l'ecran principal (panneau SAMPLES) : pics de la prise en direct (10 s glissantes).
    const std::vector<float>& livePeaks() const { return livePk; }

private:
    PrismeProcessor& proc;
    PrismeSkin skin = PrismeSkins::crystal();

    juce::Label titleLabel, stateLabel, elapsedLabel, tempoLabel;
    juce::Label fileLabel, folderLabel, hintLabel, waveTitleLabel, mixTitleLabel;
    juce::Label stackTitleLabel, stackEmptyLabel;
    juce::TextButton recBtn, stopBtn, nowBtn, backBtn, browseBtn;
    // RANDOMIZE LAB : meme rangee que RANDOMIZE, INTENSITY toujours visible.
    juce::Label varBarsLabel, varIntLabel, varSeedLabel, varStatusLabel;
    juce::TextButton randomizeBtn, rerollBtn;
    juce::ComboBox varBarsBox;
    juce::Slider intensitySlider;
    juce::TextEditor seedEditor;
    bool seedAuto = true;       // true = seed affichee tiree auto (rejouer = retirer)
    bool settingSeedGui = false; // garde anti-boucle pendant setText()
    juce::Viewport stackViewport;
    juce::Component stackList; // lignes TakeRow, hauteur = n * TakeRow::h

    // EFFECT MIX : 8 potentiomètres compacts lies aux VRAIS parametres APVTS
    // (memes IDs que l'interface principale -> synchro automatique).
    // Limite documentee : 4 vrais crossfades dry/wet (mix, freezemix,
    // blurmix, arpfxmix) + 4 amounts wet/depth (crystallize, iridescence,
    // movdepth, shimmer). Aucun doublon, aucun controle decoratif.
    static constexpr int numMixKnobs = 8;
    juce::Slider mixKnobs[numMixKnobs];
    juce::Label mixLabels[numMixKnobs];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAtts[numMixKnobs];

    Spectro3D spectro3d; // relief spectral (sortie finale, thread UI seul)

    // Ligne du LOOP STACK : compacte (34 px), mini-forme d'onde, > / X.
    // Les fichiers ne sont jamais supprimes sans confirmation explicite.
    struct TakeRow : public juce::Component
    {
        static constexpr int h = 34;
        int takeNumber = 0;
        int indentPx = 0; // variations : decalees sous leur source
        juce::Label numLabel, nameLabel, durLabel, bpmLabel, barsLabel;
        juce::TextButton playBtn, stopBtn, delBtn;
        std::vector<float> peaks;
        bool selected = false;
        bool fileMissing = false;
        bool playing = false;
        bool confirming = false; // 1er clic X = demande, 2e = acte
        juce::Point<float> dragStartPos; // seuil anti-declenchement accidentel
        bool dragArmed = false; // vrai seulement si presse sur la forme d'onde
        // Zone de depart du drag : la mini-forme d'onde elle-meme.
        juce::Rectangle<float> waveZone() const
        {
            float x0 = 448.0f + (float) indentPx;
            float x1 = (float) getWidth() - 104.0f;
            if (x1 < x0 + 20.0f) return {};
            return { x0, 0.0f, x1 - x0, (float) h };
        }
        PrismeSkin rowSkin = PrismeSkins::crystal();
        std::function<void(int)> onSelect, onPlay, onStop, onDelete; // numero de prise
        std::function<juce::File(int)> onFileForDrag; // fichier valide ou vide
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;
        void mouseMove(const juce::MouseEvent&) override;
    };
    std::vector<std::unique_ptr<TakeRow>> rows;
    int previewNumber = -1; // numero en pre-ecoute, -1 = aucune

    void rebuildStack();   // reconstruit les lignes (ajout/suppression/select)
    void refreshRowStates(); // selection + lecture + skins, sans reconstruire
    void togglePreviewByNumber(int number);
    void removeEntryByNumber(int number, bool deleteFile);
    void startVariationFromUI(bool reroll); // lit la rangee VAR LAB -> job
    int takeIndexByNumber(int number) const;

    float meterLevel = 0.0f;

    // Forme d'onde EN DIRECT de la prise (10 s glissantes), calculee sur le thread UI
    static constexpr int liveCols = 832;
    std::vector<float> livePk = std::vector<float>((size_t) liveCols, 0.0f);
    int liveIdx = 0, liveAccN = 0, liveLastSt = 0;
    float liveAcc = 0.0f;
    void updateLiveWave(int st);
    juce::String cachedStateText, cachedVarStatus;
    int lastRowRecState = -1;
    bool lastRowPreview = false;
    std::unique_ptr<juce::FileChooser> chooser;

    static juce::String formatElapsed(double seconds);
    void updateLabels(int st, double elapsedSec, bool fallback);
    void paintMeter(juce::Graphics& g, juce::Rectangle<float> area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecorderView)
};
