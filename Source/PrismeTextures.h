#pragma once
// Cache des textures raster du skin SOVIET ANALOG (pack Assets/Skins/SovietAnalog).
// Les images sont embarquees via juce_add_binary_data (cible PrismeAssets).
//
// REGLES :
//  - Chargement paresseux, une seule fois, sur le premier appel (thread GUI).
//  - Jamais appele depuis le thread audio.
//  - Les skins sans texture (hasTextures == false) ne passent jamais ici.
#include <juce_graphics/juce_graphics.h>
#include "BinaryData.h"

namespace PrismeTextures
{
    inline juce::Image load (const void* data, int dataSize)
    {
        if (data == nullptr || dataSize <= 0) return {};
        return juce::ImageCache::getFromMemory (data, dataSize);
    }

    // -- Potard --
    inline const juce::Image& knobBase()
    {
        static const juce::Image img = load (PrismeAssets::knob_large_base_png,
                                             PrismeAssets::knob_large_base_pngSize);
        return img;
    }
    // Pointeur : oriente vers le HAUT dans le PNG, a pivoter autour du centre.
    inline const juce::Image& knobPointer()
    {
        static const juce::Image img = load (PrismeAssets::knob_pointer_amber_png,
                                             PrismeAssets::knob_pointer_amber_pngSize);
        return img;
    }

    // -- Faders --
    inline const juce::Image& faderTrack()
    {
        static const juce::Image img = load (PrismeAssets::fader_track_vertical_png,
                                             PrismeAssets::fader_track_vertical_pngSize);
        return img;
    }
    inline const juce::Image& faderHandle()
    {
        static const juce::Image img = load (PrismeAssets::fader_handle_vertical_png,
                                             PrismeAssets::fader_handle_vertical_pngSize);
        return img;
    }

    // -- LEDs (temoins) --
    inline const juce::Image& ledOn()    // rouge allume
    {
        static const juce::Image img = load (PrismeAssets::led_red_png,
                                             PrismeAssets::led_red_pngSize);
        return img;
    }
    inline const juce::Image& ledAmber()
    {
        static const juce::Image img = load (PrismeAssets::led_amber_png,
                                             PrismeAssets::led_amber_pngSize);
        return img;
    }
    inline const juce::Image& ledOff()
    {
        static const juce::Image img = load (PrismeAssets::led_off_png,
                                             PrismeAssets::led_off_pngSize);
        return img;
    }

    // -- Decor (vis, rivets, fond) --
    inline const juce::Image& screw()
    {
        static const juce::Image img = load (PrismeAssets::screw_phillips_png,
                                             PrismeAssets::screw_phillips_pngSize);
        return img;
    }
    inline const juce::Image& rivet()
    {
        static const juce::Image img = load (PrismeAssets::rivet_png,
                                             PrismeAssets::rivet_pngSize);
        return img;
    }
    // Texture de fond metal patine : raccordable, on la tile.
    inline const juce::Image& patina()
    {
        static const juce::Image img = load (PrismeAssets::texture_patina_png,
                                             PrismeAssets::texture_patina_pngSize);
        return img;
    }

    // ---- Bois warm walnut pour mineprisme (texture dédiée) ----
    inline const juce::Image& bois()
    {
        static const juce::Image img = load (PrismeAssets::texture_bois_png,
                                              PrismeAssets::texture_bois_pngSize);
        return img;
    }

    // ---- Bois sombre walnut pour analog03 (texture dediee) ----
    inline const juce::Image& boisDark()
    {
        static const juce::Image img = load (PrismeAssets::texture_bois_dark_png,
                                              PrismeAssets::texture_bois_dark_pngSize);
        return img;
    }

    // ---- Phase 1 : fond et panneaux mineprisme (Soviet Analog v5) ----

    inline const juce::Image& backgroundMain()
    {
        static const juce::Image img = load (PrismeAssets::background_main_png,
                                              PrismeAssets::background_main_pngSize);
        return img;
    }

    inline const juce::Image& metalDark()
    {
        static const juce::Image img = load (PrismeAssets::texture_metal_dark_png,
                                              PrismeAssets::texture_metal_dark_pngSize);
        return img;
    }

    inline const juce::Image& metalBrushed()
    {
        static const juce::Image img = load (PrismeAssets::texture_metal_brushed_png,
                                              PrismeAssets::texture_metal_brushed_pngSize);
        return img;
    }

    inline const juce::Image& scratchesOverlay()
    {
        static const juce::Image img = load (PrismeAssets::texture_scratches_overlay_png,
                                              PrismeAssets::texture_scratches_overlay_pngSize);
        return img;
    }

    inline const juce::Image& grain()
    {
        static const juce::Image img = load (PrismeAssets::texture_grain_png,
                                              PrismeAssets::texture_grain_pngSize);
        return img;
    }

    inline const juce::Image& panelMetalLarge()
    {
        static const juce::Image img = load (PrismeAssets::panel_metal_large_png,
                                              PrismeAssets::panel_metal_large_pngSize);
        return img;
    }

    inline const juce::Image& panelBorder()
    {
        static const juce::Image img = load (PrismeAssets::panel_border_png,
                                              PrismeAssets::panel_border_pngSize);
        return img;
    }

    inline const juce::Image& panelHeader()
    {
        static const juce::Image img = load (PrismeAssets::panel_header_png,
                                              PrismeAssets::panel_header_pngSize);
        return img;
    }

    inline const juce::Image& separator()
    {
        static const juce::Image img = load (PrismeAssets::separator_png,
                                              PrismeAssets::separator_pngSize);
        return img;
    }
    // Image 1670 x 941 (repere logique de l'editeur), embarquee en JPEG.
    inline const juce::Image& fondGrille()
    {
        static const juce::Image img = load (PrismeAssets::fond_grille_jpg,
                                             PrismeAssets::fond_grille_jpgSize);
        return img;
    }

    // ---- KAY<3 : PNG decoratifs du pack KAY3 (conserves : le paquet Post Apo
    // ---- les supprimait, ils sont indispensables au skin KAY<3) ----
    inline const juce::Image& kayPunk()
    {
        static const juce::Image img = load (PrismeAssets::punk_masque_ledge_png, PrismeAssets::punk_masque_ledge_pngSize);
        return img;
    }
    inline const juce::Image& kayDoll()
    {
        static const juce::Image img = load (PrismeAssets::doll_emo_goth_ledge_png, PrismeAssets::doll_emo_goth_ledge_pngSize);
        return img;
    }
    inline const juce::Image& kayUnicorn()
    {
        static const juce::Image img = load (PrismeAssets::licorne_tete_filigrane_png, PrismeAssets::licorne_tete_filigrane_pngSize);
        return img;
    }
    inline const juce::Image& kayPlaque()
    {
        static const juce::Image img = load (PrismeAssets::plaque_verre_kay_png, PrismeAssets::plaque_verre_kay_pngSize);
        return img;
    }

    // ---- KAY<3 : elements extraits de la maquette (step B) ----
    inline const juce::Image& kayGraffitiLogo()
    {
        static const juce::Image img = load (PrismeAssets::prisme_graffiti_logo_png,
                                             PrismeAssets::prisme_graffiti_logo_pngSize);
        return img;
    }
    inline const juce::Image& kayLevelBars()
    {
        static const juce::Image img = load (PrismeAssets::header_level_bars_png,
                                             PrismeAssets::header_level_bars_pngSize);
        return img;
    }

    // Dessine une image en couvrant exactement un rectangle (etirement propre).
    inline void drawCover (juce::Graphics& g, const juce::Image& img,
                           juce::Rectangle<float> area)
    {
        if (img.isNull() || area.isEmpty()) return;
        g.drawImage (img, area.getX(), area.getY(), area.getWidth(), area.getHeight(),
                     0, 0, img.getWidth(), img.getHeight());
    }

    // Dessine une image en tenant son ratio, centree dans le rectangle.
    inline void drawFit (juce::Graphics& g, const juce::Image& img,
                         juce::Rectangle<float> area)
    {
        if (img.isNull() || area.isEmpty()) return;
        const float iw = (float) img.getWidth();
        const float ih = (float) img.getHeight();
        const float s = juce::jmin (area.getWidth() / iw, area.getHeight() / ih);
        const float w = iw * s, h = ih * s;
        g.drawImage (img, area.getX() + (area.getWidth() - w) * 0.5f,
                         area.getY() + (area.getHeight() - h) * 0.5f, w, h,
                         0, 0, img.getWidth(), img.getHeight());
    }
}
