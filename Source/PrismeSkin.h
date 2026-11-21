#pragma once
// Systeme de skins centralise : toutes les couleurs de l'interface viennent d'ici.
// Le DSP ne connait pas ce fichier. 4 skins : CRYSTAL (defaut), COSMIC, MONOLITH, TOXIC.
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

struct PrismeSkin
{
    juce::String name;
    // Fond + panneaux
    juce::Colour windowBg, panelTop, panelBottom, panelEdge, bevelHi;
    juce::Colour screwHi, screwLo;
    // Texte
    juce::Colour textMain, textDim, sectionTitle;
    // Texte sur widgets sombres (combos, editeurs). Par defaut = textMain ;
    // seule une peau a fond clair (Soviet) le diverge vers du clair.
    juce::Colour textOnWidget;
    // Accent (arcs, LEDs, curseurs, pastilles allumees)
    juce::Colour accent, accentHi, accentDeep;
    // Potards
    juce::Colour knobBodyHi, knobBodyLo, knobNeedle, knobCap;
    // Bague lumineuse autour du potard (independante de l'accent general).
    // Par defaut = accent ; une seule skin peut la diverger (ex. Soviet v5 orange).
    juce::Colour knobRing, knobRingHi;
    // Texture raster embarquee (pack Assets/Skins/SovietAnalog).
    // false = tout reste dessine par code (skins historiques, inchanges).
    bool hasTextures = false;
    bool metalV2 = false; // Phase 1 texture correction (patina background, reduced opacities)
    // Afficheurs de valeurs
    juce::Colour readoutText, readoutBg;
    // LEDs etat eteint
    juce::Colour ledOffHi, ledOffLo;
    // Curseurs (rails + capots)
    juce::Colour trackBg, trackEdge, thumbHi, thumbLo;
    // Combos + boutons
    juce::Colour comboBg, comboArrow;
    juce::Colour buttonBg, buttonText, buttonActive, buttonActiveText;
    // ---- POST APO (inutilises par les autres skins : valeurs par defaut) ----
    // diy = true : modules de couleurs differentes, fond en grille, decor bricole,
    // oscilloscope vert. Tout est visuel (voir PrismeDiy.h).
    bool diy = false;
    juce::Colour modKey, modArp, modMirageTop, modMirageBot, modSpectrum, modSamples;
    juce::Colour scopeDeep, scopeMid, scopeHot;   // oscilloscope / formes d'onde
    // ---- NIGHTCITY (inutilise par les autres skins : valeur par defaut) ----
    // neon = true : contours a halo, bagues de potards cyan/magenta, rails de
    // faders lumineux, titre en degrade. Tout est visuel (aucun DSP).
    bool neon = false;
    // ---- KAY<3 (inutilise par les autres skins : valeur par defaut) ----
    // kay = true : le langage graphique de la maquette KAY<3 (cadres arrondis
    // a deux tons cyan/magenta, anneau cyan autour des potards, boutons
    // "fond nuit + liseré", pastilles magenta). Tout est visuel (aucun DSP).
    bool kay = false;
    // ---- ANALOG03 (inutilise par les autres skins : valeur par defaut) ----
    // analog03 = true : fond bois sombre walnut, plaques ivoire vissees,
    // spectre vert sur fond noir, grille verte dans SAMPLES, accents rouges
    // soviétiques. Tout est visuel (aucun DSP).
    bool analog03 = false;
    // ---- ANALOG01 (inutilise par les autres skins : valeur par defaut) ----
    // analog01 = true : acier brossi platine, panneaux graphite, anneaux
    // potards bleus LED, cadre SPECTRUM + panneau MOUVEMENT en metal bleu,
    // grille oscilloscope bleue dans SAMPLES. Tout est visuel (aucun DSP).
    bool analog01 = false;
};

namespace PrismeSkins
{
    inline PrismeSkin crystal()
    {
        PrismeSkin s;
        s.name = "Crystal";
        s.windowBg = juce::Colour(0xff07090f);
        s.panelTop = juce::Colour(0xff101725); s.panelBottom = juce::Colour(0xff0a0e18);
        s.panelEdge = juce::Colour(0xff2a3a55); s.bevelHi = juce::Colour(0xff5a6c8a);
        s.screwHi = juce::Colour(0xff9aa0aa); s.screwLo = juce::Colour(0xff33363c);
        s.textMain = juce::Colour(0xffeef4ff); s.textDim = juce::Colour(0xff8a93b8);
        s.textOnWidget = s.textMain;
        s.sectionTitle = juce::Colour(0xffbfe9ff);
        s.accent = juce::Colour(0xff7df9ff); s.accentHi = juce::Colour(0xffd8fbff);
        s.accentDeep = juce::Colour(0xff2a7a9a);
        s.knobRing = s.accent; s.knobRingHi = s.accentHi;
        s.knobBodyHi = juce::Colour(0xff1c2740); s.knobBodyLo = juce::Colour(0xff0b1020);
        s.knobNeedle = juce::Colour(0xffd8fbff); s.knobCap = juce::Colour(0xff05070d);
        s.readoutText = juce::Colour(0xff7df9ff); s.readoutBg = juce::Colour(0xff05070d);
        s.ledOffHi = juce::Colour(0xff3a4a5a); s.ledOffLo = juce::Colour(0xff121a24);
        s.trackBg = juce::Colour(0xff0a0d14); s.trackEdge = juce::Colour(0xff3a4a5e);
        s.thumbHi = juce::Colour(0xffc8ccd4); s.thumbLo = juce::Colour(0xff6f747e);
        s.comboBg = juce::Colour(0xff0d1322);
        s.comboArrow = juce::Colour(0xffc9e8f5);
        s.buttonBg = juce::Colour(0xff16202f); s.buttonText = juce::Colours::white;
        s.buttonActive = juce::Colour(0xff7df9ff); s.buttonActiveText = juce::Colour(0xff06121a);
        return s;
    }

    inline PrismeSkin cosmic()
    {
        PrismeSkin s;
        s.name = "Cosmic";
        s.windowBg = juce::Colour(0xff0d0714);
        s.panelTop = juce::Colour(0xff1c1030); s.panelBottom = juce::Colour(0xff120a20);
        s.panelEdge = juce::Colour(0xff4a2a66); s.bevelHi = juce::Colour(0xff7a5a9a);
        s.screwHi = juce::Colour(0xffa08ab8); s.screwLo = juce::Colour(0xff38304a);
        s.textMain = juce::Colour(0xfff5e9ff); s.textDim = juce::Colour(0xff9a86c2);
        s.textOnWidget = s.textMain;
        s.sectionTitle = juce::Colour(0xfff0a6ff);
        s.accent = juce::Colour(0xffff4fd8); s.accentHi = juce::Colour(0xffffd2f2);
        s.accentDeep = juce::Colour(0xff8a1a72);
        s.knobRing = s.accent; s.knobRingHi = s.accentHi;
        s.knobBodyHi = juce::Colour(0xff2a1545); s.knobBodyLo = juce::Colour(0xff120a20);
        s.knobNeedle = juce::Colour(0xffffd2f2); s.knobCap = juce::Colour(0xff0d0714);
        s.readoutText = juce::Colour(0xffff4fd8); s.readoutBg = juce::Colour(0xff0d0714);
        s.ledOffHi = juce::Colour(0xff4a3a56); s.ledOffLo = juce::Colour(0xff1c1424);
        s.trackBg = juce::Colour(0xff120a18); s.trackEdge = juce::Colour(0xff4a3560);
        s.thumbHi = juce::Colour(0xffd8c8e0); s.thumbLo = juce::Colour(0xff7a6a88);
        s.comboBg = juce::Colour(0xff170d26);
        s.comboArrow = juce::Colour(0xfff0d2f5);
        s.buttonBg = juce::Colour(0xff241430); s.buttonText = juce::Colours::white;
        s.buttonActive = juce::Colour(0xffff4fd8); s.buttonActiveText = juce::Colour(0xff1c0716);
        return s;
    }

    inline PrismeSkin monolith()
    {
        PrismeSkin s;
        s.name = "Monolith";
        s.windowBg = juce::Colour(0xff101012);
        s.panelTop = juce::Colour(0xff232326); s.panelBottom = juce::Colour(0xff17171a);
        s.panelEdge = juce::Colour(0xff3a3a40); s.bevelHi = juce::Colour(0xff5a5a60);
        s.screwHi = juce::Colour(0xff9aa0aa); s.screwLo = juce::Colour(0xff33363c);
        s.textMain = juce::Colour(0xffeceef2); s.textDim = juce::Colour(0xff8b8e96);
        s.textOnWidget = s.textMain;
        s.sectionTitle = juce::Colour(0xffe2e4e9);
        s.accent = juce::Colour(0xffc9ccd4); s.accentHi = juce::Colour(0xffffffff);
        s.accentDeep = juce::Colour(0xff6a6e76);
        s.knobRing = s.accent; s.knobRingHi = s.accentHi;
        s.knobBodyHi = juce::Colour(0xff33363c); s.knobBodyLo = juce::Colour(0xff141518);
        s.knobNeedle = juce::Colour(0xffffffff); s.knobCap = juce::Colour(0xff0c0c0e);
        s.readoutText = juce::Colour(0xffe8eaee); s.readoutBg = juce::Colour(0xff0c0c0e);
        s.ledOffHi = juce::Colour(0xff4a4e56); s.ledOffLo = juce::Colour(0xff1d1f23);
        s.trackBg = juce::Colour(0xff0c0c0e); s.trackEdge = juce::Colour(0xff3c3c42);
        s.thumbHi = juce::Colour(0xffe2e4e9); s.thumbLo = juce::Colour(0xff7a7d85);
        s.comboBg = juce::Colour(0xff151517);
        s.comboArrow = juce::Colour(0xffd5d7dc);
        s.buttonBg = juce::Colour(0xff232326); s.buttonText = juce::Colours::white;
        s.buttonActive = juce::Colour(0xffe2e4e9); s.buttonActiveText = juce::Colour(0xff141518);
        return s;
    }

    inline PrismeSkin toxic()
    {
        PrismeSkin s;
        s.name = "Toxic";
        s.windowBg = juce::Colour(0xff060a06);
        s.panelTop = juce::Colour(0xff0f1c10); s.panelBottom = juce::Colour(0xff090f0a);
        s.panelEdge = juce::Colour(0xff1e4a26); s.bevelHi = juce::Colour(0xff3a6a44);
        s.screwHi = juce::Colour(0xff8aa890); s.screwLo = juce::Colour(0xff2a3a2e);
        s.textMain = juce::Colour(0xffe9ffee); s.textDim = juce::Colour(0xff7fa88a);
        s.textOnWidget = s.textMain;
        s.sectionTitle = juce::Colour(0xffa8ffc0);
        s.accent = juce::Colour(0xff4dff5e); s.accentHi = juce::Colour(0xffd2ffd8);
        s.accentDeep = juce::Colour(0xff1a7a2a);
        s.knobRing = s.accent; s.knobRingHi = s.accentHi;
        s.knobBodyHi = juce::Colour(0xff14301c); s.knobBodyLo = juce::Colour(0xff070d08);
        s.knobNeedle = juce::Colour(0xffd2ffd8); s.knobCap = juce::Colour(0xff050805);
        s.readoutText = juce::Colour(0xff4dff5e); s.readoutBg = juce::Colour(0xff050805);
        s.ledOffHi = juce::Colour(0xff3a4a3e); s.ledOffLo = juce::Colour(0xff101814);
        s.trackBg = juce::Colour(0xff070d08); s.trackEdge = juce::Colour(0xff2a4a30);
        s.thumbHi = juce::Colour(0xffc8d8cc); s.thumbLo = juce::Colour(0xff6a7a70);
        s.comboBg = juce::Colour(0xff0b140c);
        s.comboArrow = juce::Colour(0xffd2e8d8);
        s.buttonBg = juce::Colour(0xff122416); s.buttonText = juce::Colours::white;
        s.buttonActive = juce::Colour(0xff4dff5e); s.buttonActiveText = juce::Colour(0xff061206);
        return s;
    }

    inline PrismeSkin kay3()
    {
        PrismeSkin s;
        s.name = "KAY<3";
        // Neon noir : contours cyan, reflets bleus et touches magenta.
        s.windowBg = juce::Colour(0xff05091a);
        s.panelTop = juce::Colour(0xff07132b); s.panelBottom = juce::Colour(0xff030817);
        s.panelEdge = juce::Colour(0xff28e1ff); s.bevelHi = juce::Colour(0xff2878dc);
        s.screwHi = juce::Colour(0xff78a0c8); s.screwLo = juce::Colour(0xff101c3a);
        s.textMain = juce::Colour(0xffd7f0ff); s.textDim = juce::Colour(0xff78a0c8);
        s.textOnWidget = s.textMain;
        s.sectionTitle = juce::Colour(0xff28e1ff);
        s.accent = juce::Colour(0xff28e1ff); s.accentHi = juce::Colour(0xffff46e1);
        s.accentDeep = juce::Colour(0xff2878dc);
        s.knobRing = juce::Colour(0xff28e1ff); s.knobRingHi = juce::Colour(0xffff46e1);
        s.knobBodyHi = juce::Colour(0xff101d3b); s.knobBodyLo = juce::Colour(0xff030611);
        s.knobNeedle = juce::Colour(0xffd7f0ff); s.knobCap = juce::Colour(0xff05091a);
        s.readoutText = juce::Colour(0xff28e1ff); s.readoutBg = juce::Colour(0xff020611);
        s.ledOffHi = juce::Colour(0xff284064); s.ledOffLo = juce::Colour(0xff0b1025);
        s.trackBg = juce::Colour(0xff020611); s.trackEdge = juce::Colour(0xff2878dc);
        s.thumbHi = juce::Colour(0xff28e1ff); s.thumbLo = juce::Colour(0xff2878dc);
        s.comboBg = juce::Colour(0xff030817); s.comboArrow = juce::Colour(0xff28e1ff);
        s.buttonBg = juce::Colour(0xff08132c); s.buttonText = juce::Colour(0xffd7f0ff);
        s.buttonActive = juce::Colour(0xff28e1ff); s.buttonActiveText = juce::Colour(0xff05091a);
        // KAY<3 : langage graphique de la maquette (cadres deux tons, anneau
        // cyan sur les potards, boutons fond nuit + liseré, pastilles magenta).
        s.kay = true;
        return s;
    }

    inline PrismeSkin nightCity()
    {
        // NIGHTCITY - base « Neon » : meme palette que KAY<3, sans le decor.
        // C'est le skin de base ; KAY<3 en est la variante decoree.
        // Le rendu specifique (halo, bagues, rails lumineux, titre degrade)
        // est active par le drapeau neon, traite dans LookAndFeel et l'editeur.
        PrismeSkin s = kay3();
        s.name = "NightCity";
        s.neon = true;
        s.kay  = false;   // NIGHTCITY garde son style propre (chanfreins, equerres)
        // Degrade de panneau un peu plus clair : donne du volume sans eclaircir le fond.
        s.panelTop    = juce::Colour(0xff0b1b3a);
        s.panelBottom = juce::Colour(0xff03081a);
        return s;
    }

    inline PrismeSkin mix()
    {
        PrismeSkin s;
        s.name = "Mix";
        s.windowBg = juce::Colour(0xff0b0c0f);
        s.panelTop = juce::Colour(0xff17191e); s.panelBottom = juce::Colour(0xff101215);
        s.panelEdge = juce::Colour(0xff2e323a); s.bevelHi = juce::Colour(0xff4e525b);
        s.screwHi = juce::Colour(0xff9aa0aa); s.screwLo = juce::Colour(0xff33363c);
        s.textMain = juce::Colour(0xffe8eaf0); s.textDim = juce::Colour(0xff9aa0ae);
        s.textOnWidget = s.textMain;
        s.sectionTitle = juce::Colour(0xff7df9ff);
        s.accent = juce::Colour(0xff35e0ff); s.accentHi = juce::Colour(0xffd8f4ff);
        s.accentDeep = juce::Colour(0xff2a3a8a);
        s.knobRing = s.accent; s.knobRingHi = s.accentHi;
        s.knobBodyHi = juce::Colour(0xff23262c); s.knobBodyLo = juce::Colour(0xff101215);
        s.knobNeedle = juce::Colour(0xffffffff); s.knobCap = juce::Colour(0xff0a0b0d);
        s.readoutText = juce::Colour(0xff7df9ff); s.readoutBg = juce::Colour(0xff08090c);
        s.ledOffHi = juce::Colour(0xff3e444c); s.ledOffLo = juce::Colour(0xff14161a);
        s.trackBg = juce::Colour(0xff0a0b0e); s.trackEdge = juce::Colour(0xff363b44);
        s.thumbHi = juce::Colour(0xffd5d9e0); s.thumbLo = juce::Colour(0xff71757d);
        s.comboBg = juce::Colour(0xff121418);
        s.comboArrow = juce::Colour(0xffc8e4ee);
        s.buttonBg = juce::Colour(0xff181b20); s.buttonText = juce::Colours::white;
        s.buttonActive = juce::Colour(0xff35e0ff); s.buttonActiveText = juce::Colour(0xff061318);
        return s;
    }

    inline PrismeSkin mineprisme()
    {
        // SOVIET ANALOG — refonte visuelle (paquet Soviet_Analog_v5).
        // Cible : metal beige/khaki patine, texte sombre sur metal clair,
        // bagues de potards et accents orange vif (#FF7A1A).
        // Les textures raster du pack sont embarquees (hasTextures = true) ;
        // les 6 autres skins restent 100 % dessines par code.
        PrismeSkin s;
        s.name = "mineprisme";
        s.hasTextures = true;
        // Fond de fenetre : brun tres sombre (presque noir), comme la maquette.
        s.windowBg = juce::Colour(0xff180f07);
        // Metal rouille : brun chaud mi-fonce, biseau plus clair (patine).
        s.panelTop = juce::Colour(0xff8f6f48); s.panelBottom = juce::Colour(0xff64492c);
        s.panelEdge = juce::Colour(0xff2c1d0d); s.bevelHi = juce::Colour(0xffb58c55);
        s.screwHi = juce::Colour(0xff9c8258); s.screwLo = juce::Colour(0xff20160a);
        // Texte grave dans le metal (etiquettes de potards) : brun tres fonce.
        s.textMain = juce::Colour(0xff231706);
        // Texte sur widgets sombres (combos, editeurs, cellules)
        s.textOnWidget = juce::Colour(0xffe8d6b4);
        s.textDim = juce::Colour(0xff6a5232);
        s.sectionTitle = juce::Colour(0xff5e1c08);
        s.accent = juce::Colour(0xffff7a1a); s.accentHi = juce::Colour(0xffffc48a);
        s.accentDeep = juce::Colour(0xff7a1c0e);
        // v5 ORANGE KNOBS : bague lumineuse des potards en orange vif.
        s.knobRing = juce::Colour(0xffff7a1a); s.knobRingHi = juce::Colour(0xffffc48a);
        s.knobBodyHi = juce::Colour(0xff2a2018); s.knobBodyLo = juce::Colour(0xff080604);
        s.knobNeedle = juce::Colour(0xffff7a1a); s.knobCap = juce::Colour(0xff080604);
        s.readoutText = juce::Colour(0xffff9a3a); s.readoutBg = juce::Colour(0xff140f08);
        s.ledOffHi = juce::Colour(0xff453a2a); s.ledOffLo = juce::Colour(0xff161009);
        s.trackBg = juce::Colour(0xff241a10); s.trackEdge = juce::Colour(0xff2c1d0d);
        s.thumbHi = juce::Colour(0xffb08a56); s.thumbLo = juce::Colour(0xff5e462c);
        s.comboBg = juce::Colour(0xff140f08);
        s.comboArrow = juce::Colour(0xffff9a3a);
        s.buttonBg = juce::Colour(0xff241a10); s.buttonText = juce::Colour(0xffe8d6b4);
        s.buttonActive = juce::Colour(0xffff7a1a); s.buttonActiveText = juce::Colour(0xff1a1208);
        return s;
    }

    inline PrismeSkin analog2()
    {
        // ANALOG2 — clone de mineprisme avec correction de texture (Phase 1).
        // Fond patine bronze, panneaux avec patine 0.20, scratches 0.08.
        PrismeSkin s = mineprisme();
        s.name = "analog2";
        s.metalV2 = true;
        return s;
    }

    inline PrismeSkin postApo()
    {
        // POST APO - variante bricolee du Soviet Analog : modules colles de
        // travers (menthe / bleu glacier / orange / gris-vert / bordeaux), fond
        // en grille de metal rouille, cadres noirs epais, oscilloscope vert.
        // On part de Soviet Analog (memes potards a bague orange, memes textures)
        // puis on surcharge ce qui change.
        PrismeSkin s = mineprisme();
        s.name = "Post Apo";
        s.diy = true;
        s.windowBg = juce::Colour(0xff1a130d);
        s.panelEdge = juce::Colour(0x66000000);
        s.textMain = juce::Colour(0xff1c140a);          // sur plaques claires
        s.textDim = juce::Colour(0xff4d3f2c);
        s.textOnWidget = juce::Colour(0xffeadcc0);      // sur grille / bordeaux / ecrans
        s.sectionTitle = juce::Colour(0xff4a1606);
        s.modKey = juce::Colour(0xff8ccdb4);            // menthe
        s.modArp = juce::Colour(0xff566f9e);            // bleu glacier
        s.modMirageTop = juce::Colour(0xffffb242);      // orange degrade
        s.modMirageBot = juce::Colour(0xffba4216);
        s.modSpectrum = juce::Colour(0xff5c7062);       // gris-vert
        s.modSamples = juce::Colour(0xff742236);        // bordeaux
        s.scopeDeep = juce::Colour(0xff0f4a2a);         // oscilloscope vert phosphore
        s.scopeMid = juce::Colour(0xff46ff78);
        s.scopeHot = juce::Colour(0xffd8ffe0);
        return s;
    }

    inline PrismeSkin analog03()
    {
        // ANALOG03 - Soviet Analog Ivory & Green.
        // Chassis bois sombre walnut, plaques ivoire vissees,
        // spectre vert sur fond noir, grille verte SAMPLES,
        // accents rouges sovietiques, potards noirs a repere vert.
        PrismeSkin s;
        s.name = "analog03";
        s.hasTextures = true;
        s.analog03 = true;
        // Fond : bois sombre walnut
        s.windowBg = juce::Colour(0xff3B2116);
        // Panneaux : metal ivoire/gris clair vieilli
        s.panelTop = juce::Colour(0xffC7C1B2); s.panelBottom = juce::Colour(0xff4C4A43);
        s.panelEdge = juce::Colour(0xff171A17); s.bevelHi = juce::Colour(0xffE0DACC);
        s.screwHi = juce::Colour(0xff9B988D); s.screwLo = juce::Colour(0xff141412);
        // Texte : ivoire
        s.textMain = juce::Colour(0xffE8E0CC);
        s.textOnWidget = juce::Colour(0xffE8E0CC);
        s.textDim = juce::Colour(0xff72BC83);
        s.sectionTitle = juce::Colour(0xffA72D27);
        // Accents : rouge sovietique + vert indicateurs
        s.accent = juce::Colour(0xffA72D27); s.accentHi = juce::Colour(0xffD4443E);
        s.accentDeep = juce::Colour(0xff6D211F);
        // Potards noirs, bague vert, aiguille verte
        s.knobRing = juce::Colour(0xff28F05A); s.knobRingHi = juce::Colour(0xff72BC83);
        s.knobBodyHi = juce::Colour(0xff1E211E); s.knobBodyLo = juce::Colour(0xff050907);
        s.knobNeedle = juce::Colour(0xff28F05A); s.knobCap = juce::Colour(0xff030503);
        // Afficheurs : vert sur noir
        s.readoutText = juce::Colour(0xff28F05A); s.readoutBg = juce::Colour(0xff050907);
        s.ledOffHi = juce::Colour(0xff1E211E); s.ledOffLo = juce::Colour(0xff050907);
        // Faders : rails noirs, poignees rouge fonce
        s.trackBg = juce::Colour(0xff0F110F); s.trackEdge = juce::Colour(0xff171A17);
        s.thumbHi = juce::Colour(0xffA72D27); s.thumbLo = juce::Colour(0xff6D211F);
        // Combos + boutons
        s.comboBg = juce::Colour(0xff0F110F);
        s.comboArrow = juce::Colour(0xff28F05A);
        s.buttonBg = juce::Colour(0xff1E211E); s.buttonText = juce::Colour(0xffE8E0CC);
        s.buttonActive = juce::Colour(0xffA72D27); s.buttonActiveText = juce::Colour(0xffE8E0CC);
        // Oscilloscope / spectre : vert phosphore
        s.scopeDeep = juce::Colour(0xff0A1F10);
        s.scopeMid = juce::Colour(0xff28F05A);
        s.scopeHot = juce::Colour(0xffC0FFD0);
        return s;
    }

    inline PrismeSkin analog01()
    {
        // ANALOG01 - Platine / Graphite / Bleu.
        // Acier brossi platine, panneaux graphite, potards noirs a anneaux
        // bleus LED, cadre SPECTRUM et panneau MOUVEMENT en metal bleu,
        // grille oscilloscope bleue dans SAMPLES.
        PrismeSkin s;
        s.name = "analog01";
        s.hasTextures = true;
        s.analog01 = true;
        // Fond : acier brossi platine / argent froid (texture metal brossi)
        s.windowBg = juce::Colour(0xff747D82);
        // Panneaux : graphite / noir charbon, avec relief
        s.panelTop = juce::Colour(0xff2A323A); s.panelBottom = juce::Colour(0xff14181D);
        s.panelEdge = juce::Colour(0xff0E1216); s.bevelHi = juce::Colour(0xff4A555E);
        s.screwHi = juce::Colour(0xff9AA4AC); s.screwLo = juce::Colour(0xff0A0D10);
        // Texte : bleu pale, lisible sur graphite
        s.textMain = juce::Colour(0xffC8D3DC);
        s.textOnWidget = juce::Colour(0xffC8D3DC);
        s.textDim = juce::Colour(0xff8A97A3);
        s.sectionTitle = juce::Colour(0xff7FB8D8);
        // Accents : bleu LED
        s.accent = juce::Colour(0xff00A8FF); s.accentHi = juce::Colour(0xff26B8FF);
        s.accentDeep = juce::Colour(0xff005A8A);
        // Potards noirs, anneaux et aiguilles bleus LED
        s.knobRing = juce::Colour(0xff00A8FF); s.knobRingHi = juce::Colour(0xff26B8FF);
        s.knobBodyHi = juce::Colour(0xff23272B); s.knobBodyLo = juce::Colour(0xff05070A);
        s.knobNeedle = juce::Colour(0xff26B8FF); s.knobCap = juce::Colour(0xff03050A);
        // Afficheurs : bleu sur noir-bleu
        s.readoutText = juce::Colour(0xff26B8FF); s.readoutBg = juce::Colour(0xff020C16);
        s.ledOffHi = juce::Colour(0xff23272B); s.ledOffLo = juce::Colour(0xff05070A);
        // Faders : rails sombres, poignees metal platine
        s.trackBg = juce::Colour(0xff0E1216); s.trackEdge = juce::Colour(0xff20272C);
        s.thumbHi = juce::Colour(0xffA6ACAF); s.thumbLo = juce::Colour(0xff747D82);
        // Combos + boutons
        s.comboBg = juce::Colour(0xff14181D);
        s.comboArrow = juce::Colour(0xff00A8FF);
        s.buttonBg = juce::Colour(0xff20272C); s.buttonText = juce::Colour(0xffC8D3DC);
        s.buttonActive = juce::Colour(0xff00A8FF); s.buttonActiveText = juce::Colour(0xff020C16);
        // Oscilloscope / spectre : bleu sur noir-bleu
        s.scopeDeep = juce::Colour(0xff02101E);
        s.scopeMid = juce::Colour(0xff00A8FF);
        s.scopeHot = juce::Colour(0xffB8E0FF);
        return s;
    }

    inline std::vector<PrismeSkin> all()
    {
        // L'ordre compte : l'indice est memorise dans l'etat ("uiSkin").
        // Les nouveaux skins se rajoutent TOUJOURS a la fin.
        return { crystal(), cosmic(), monolith(), toxic(), mix(), mineprisme(), analog2(), kay3(), postApo(), nightCity(), analog03(), analog01() };
    }

    inline std::vector<juce::String> names()
    {
        std::vector<juce::String> n;
        for (auto& s : all()) n.push_back(s.name);
        return n;
    }
}
