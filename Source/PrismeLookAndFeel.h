#pragma once
// Skin HARDWARE : metal brosse, potards massifs, afficheurs, LEDs, combos encastres.
// Toutes les couleurs viennent du PrismeSkin actif (Crystal/Cosmic/Monolith/Toxic).
// 100 % visuel, aucun DSP.
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "PrismeSkin.h"
#include "PrismeTextures.h"
#include "PrismeDiy.h"

struct HardwareLookAndFeel : public juce::LookAndFeel_V4
{
    PrismeSkin skin = PrismeSkins::crystal();

    juce::Image materialDial, materialFader, panelGrain;

    void setSkin(const PrismeSkin& s)
    {
        skin = s;
        int size = 0;
        const char* data = s.materialResource != nullptr
            ? PrismeAssets::getNamedResource(s.materialResource, size) : nullptr;
        const auto sheet = PrismeTextures::load(data, size);
        materialDial = sheet.getClippedImage({ 0, 0, 256, 256 });
        materialFader = sheet.getClippedImage({ 256, 0, 96, 44 });
        panelGrain = sheet.getClippedImage({ 256, 48, 64, 64 });
    }

    // ---- NIGHTCITY : coin coupe en biais (chanfrein a 45 deg).
    static juce::Path chamferRect(juce::Rectangle<float> r, float c)
    {
        c = juce::jlimit(0.0f, juce::jmin(r.getWidth(), r.getHeight()) * 0.45f, c);
        juce::Path p;
        p.startNewSubPath(r.getX() + c,     r.getY());
        p.lineTo(r.getRight() - c,          r.getY());
        p.lineTo(r.getRight(),              r.getY() + c);
        p.lineTo(r.getRight(),              r.getBottom() - c);
        p.lineTo(r.getRight() - c,          r.getBottom());
        p.lineTo(r.getX() + c,              r.getBottom());
        p.lineTo(r.getX(),                  r.getBottom() - c);
        p.lineTo(r.getX(),                  r.getY() + c);
        p.closeSubPath();
        return p;
    }

    // ---- KAY<3 : cadre arrondi a deux tons (cyan en haut, magenta en bas).
    // Dessine un halo doux, puis le trait en degrade. Aucun DSP, dessin seul.
    static void kayNeonFrame(juce::Graphics& g, juce::Rectangle<float> r,
                             float radius, const PrismeSkin& sk, bool glow)
    {
        if (glow)
        {
            g.setColour(sk.accent.withAlpha(0.10f));
            g.drawRoundedRectangle(r.expanded(2.0f), radius + 2.0f, 4.0f);
        }
        juce::ColourGradient gr(sk.accent,   r.getX(), r.getY(),
                                sk.accentHi, r.getX(), r.getBottom(), false);
        g.setGradientFill(gr);
        g.drawRoundedRectangle(r, radius, 1.4f);
    }

    // --- Potard rotatif massif ---
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        float cx = (float) x + (float) width * 0.5f;
        float cy = (float) y + (float) height * 0.5f;
        float radius = std::min((float) width, (float) height) * 0.5f - 4.0f;
        if (radius < 4.0f) return;

        float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        if (materialDial.isValid())
        {
            const float r = radius * 0.85f;
            g.setColour(skin.sectionTitle.withAlpha(0.8f));
            for (int i = 0; i <= 10; ++i)
            {
                const float a = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * i / 10.0f;
                const float dx = std::sin(a), dy = -std::cos(a);
                g.drawLine(cx + dx * radius * 0.92f, cy + dy * radius * 0.92f,
                           cx + dx * radius, cy + dy * radius, 1.0f);
            }
            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.fillEllipse(cx-r, cy-r+2.0f, 2.0f*r, 2.0f*r);
            g.setColour(juce::Colours::white);
            const auto transform = juce::AffineTransform::translation(-128.0f, -128.0f)
                .scaled(r / 128.0f).rotated(angle).translated(cx, cy);
            g.drawImageTransformed(materialDial, transform);
            return;
        }

        if (skin.crystalFaceplate)
        {
            const float face = radius * 0.78f;
            auto radialLine = [&](float a, float from, float to, float thickness)
            {
                const float dx = std::sin(a), dy = -std::cos(a);
                g.drawLine(cx + dx * from, cy + dy * from,
                           cx + dx * to, cy + dy * to, thickness);
            };
            g.setColour(skin.sectionTitle.withAlpha(0.65f));
            for (int i = 0; i <= 10; ++i)
            {
                const float tick = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * (float) i / 10.0f;
                radialLine(tick, radius * (i % 5 == 0 ? 0.85f : 0.92f), radius, 1.0f);
            }

            const juce::Rectangle<float> cap(cx - face, cy - face, face * 2.0f, face * 2.0f);
            g.setColour(juce::Colours::black.withAlpha(0.24f));
            g.fillEllipse(cap.translated(0.0f, 2.0f).expanded(1.0f));
            g.setColour(juce::Colours::white);
            g.drawImage(PrismeTextures::crystalDial(), cap);
            g.setColour(skin.accent.withAlpha(0.22f));
            radialLine(angle, face * 0.40f, face * 0.83f, 5.0f);
            g.setColour(skin.accent);
            radialLine(angle, face * 0.40f, face * 0.83f, 2.6f);
            return;
        }

        // ---- KAY<3 : halo cyan doux derriere le potard (maquette) ----
        if (skin.kay)
        {
            g.setColour(skin.accent.withAlpha(0.09f));
            g.fillEllipse(cx - radius - 6.0f, cy - radius - 6.0f,
                          (radius + 6.0f) * 2.0f, (radius + 6.0f) * 2.0f);
            g.setColour(skin.accent.withAlpha(0.15f));
            g.fillEllipse(cx - radius - 2.5f, cy - radius - 2.5f,
                          (radius + 2.5f) * 2.0f, (radius + 2.5f) * 2.0f);
        }

        // ---- NIGHTCITY : potard neon (halo cyan, bagues cyan + liseret magenta) ----
        if (skin.neon)
        {
            auto arc = [&](float from, float to, float thick, juce::Colour col)
            {
                juce::Path p;
                const int steps = 48;
                for (int i = 0; i <= steps; ++i)
                {
                    const float a  = from + (to - from) * (float) i / (float) steps;
                    const float px = cx + std::sin(a) * radius;
                    const float py = cy - std::cos(a) * radius;
                    if (i == 0) p.startNewSubPath(px, py);
                    else        p.lineTo(px, py);
                }
                g.setColour(col);
                g.strokePath(p, juce::PathStrokeType(thick, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
            };

            // Halo exterieur : deux passes, du plus large au plus serre
            g.setColour(skin.accent.withAlpha(0.07f));
            g.fillEllipse(cx - radius - 7.0f, cy - radius - 7.0f,
                          (radius + 7.0f) * 2.0f, (radius + 7.0f) * 2.0f);
            g.setColour(skin.accent.withAlpha(0.13f));
            g.fillEllipse(cx - radius - 3.5f, cy - radius - 3.5f,
                          (radius + 3.5f) * 2.0f, (radius + 3.5f) * 2.0f);

            // Corps sombre (degrade du skin)
            juce::ColourGradient body(skin.knobBodyHi, cx, cy - radius,
                                      skin.knobBodyLo, cx, cy + radius, false);
            g.setGradientFill(body);
            g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.07f));
            g.fillEllipse(cx - radius * 0.72f, cy - radius * 0.82f,
                          radius * 1.05f, radius * 0.50f);

            // Piste complete (cyan tres sombre)
            arc(rotaryStartAngle, rotaryEndAngle, 3.5f, skin.accent.withAlpha(0.20f));
            // Valeur : halo cyan, trait cyan, liseret magenta
            arc(rotaryStartAngle, angle, 6.0f, skin.accent.withAlpha(0.22f));
            arc(rotaryStartAngle, angle, 3.2f, skin.accent);
            arc(rotaryStartAngle, angle, 1.4f, skin.accentHi);

            // Aiguille + point lumineux
            const float dx = std::sin(angle), dy = -std::cos(angle);
            const float r0 = radius * 0.20f, r1 = radius * 0.78f;
            g.setColour(skin.knobNeedle.withAlpha(0.85f));
            g.drawLine(cx + dx * r0, cy + dy * r0, cx + dx * r1, cy + dy * r1, 1.6f);
            g.setColour(skin.accentHi.withAlpha(0.30f));
            g.fillEllipse(cx + dx * r1 - 4.5f, cy + dy * r1 - 4.5f, 9.0f, 9.0f);
            g.setColour(juce::Colours::white);
            g.fillEllipse(cx + dx * r1 - 1.7f, cy + dy * r1 - 1.7f, 3.4f, 3.4f);
            return;
        }

        // ---- POST APO : potards "recuperes" (bakelite / chrome), dessin seul ----
        // Marques par la propriete "diyKnob" posee dans PrismeEditor::applySkin.
        if (skin.diy)
        {
            const juce::var kind = slider.getProperties()["diyKnob"];
            if (kind.isString())
            {
                PrismeDiy::drawVintageKnob(g, cx, cy, radius - 2.0f, angle, kind.toString() == "chrome");
                return;
            }
        }

        // ---- Variante TEXTUREE (skin.hasTextures) : corps en PNG + pointeur pivoTE ----
        if (skin.hasTextures && PrismeTextures::knobBase().isValid())
        {
            const juce::Image& base = PrismeTextures::knobBase();
            // Corps du potard : PNG centré, inscrit dans le cercle (le PNG est carré)
            g.drawImage(base, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f,
                        0, 0, base.getWidth(), base.getHeight());

            // Bague orange : arc de valeur par-dessus le PNG (couleur du skin)
            auto strokeArcT = [&](float a0, float a1, float thick)
            {
                juce::Path p;
                int steps = 40;
                for (int i = 0; i <= steps; ++i)
                {
                    float a = a0 + (a1 - a0) * (float) i / (float) steps;
                    float px = cx + std::sin(a) * radius;
                    float py = cy - std::cos(a) * radius;
                    if (i == 0) p.startNewSubPath(px, py);
                    else p.lineTo(px, py);
                }
                g.strokePath(p, juce::PathStrokeType(thick, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
            };
            g.setColour(skin.knobRing.withAlpha(0.22f));
            strokeArcT(rotaryStartAngle, rotaryEndAngle, 5.0f);
            g.setColour(skin.knobRing.withAlpha(0.55f));
            strokeArcT(rotaryStartAngle, angle, 6.0f);
            g.setColour(skin.knobRingHi);
            strokeArcT(rotaryStartAngle, angle, 3.0f);

            // Pointeur PNG : image orientee vers le haut, pivotee autour du centre.
            const juce::Image& ptr = PrismeTextures::knobPointer();
            if (ptr.isValid())
            {
                juce::Graphics::ScopedSaveState save(g);
                // Le pointeur du PNG pointe vers le haut (-Y) ; l'angle JUCE est
                // mesure depuis le haut dans le sens horaire, d'ou la meme conversion.
                g.addTransform(juce::AffineTransform::rotation(angle, cx, cy));
                const float pw = radius * 1.7f, ph = radius * 1.7f;
                g.drawImage(ptr, cx - pw * 0.5f, cy - ph * 0.5f, pw, ph,
                            0, 0, ptr.getWidth(), ptr.getHeight());
            }
            return;
        }

        // Bague externe sombre
        g.setColour(skin.panelBottom);
        g.fillEllipse(cx - radius - 2.0f, cy - radius - 2.0f, (radius + 2.0f) * 2.0f, (radius + 2.0f) * 2.0f);
        // Corps metal (degrade vertical)
        juce::ColourGradient grad(skin.knobBodyHi, cx, cy - radius, skin.knobBodyLo, cx, cy + radius, false);
        g.setGradientFill(grad);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
        // Reflet superieur
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.fillEllipse(cx - radius * 0.75f, cy - radius * 0.85f, radius * 1.1f, radius * 0.55f);
        // Cabochon central
        g.setColour(skin.knobCap);
        g.fillEllipse(cx - radius * 0.28f, cy - radius * 0.28f, radius * 0.56f, radius * 0.56f);

        // ---- KAY<3 : anneau cyan complet + arc de valeur magenta + aiguille
        // blanche tres fine (fideles a la maquette). ----
        if (skin.kay)
        {
            auto kArc = [&](float a0, float a1, float thick, juce::Colour col)
            {
                juce::Path p;
                const int steps = 64;
                for (int i = 0; i <= steps; ++i)
                {
                    const float a  = a0 + (a1 - a0) * (float) i / (float) steps;
                    const float px = cx + std::sin(a) * radius;
                    const float py = cy - std::cos(a) * radius;
                    if (i == 0) p.startNewSubPath(px, py);
                    else        p.lineTo(px, py);
                }
                g.setColour(col);
                g.strokePath(p, juce::PathStrokeType(thick, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
            };
            // Piste complete : anneau cyan fin
            kArc(rotaryStartAngle, rotaryEndAngle, 4.5f, skin.accent.withAlpha(0.28f));
            kArc(rotaryStartAngle, rotaryEndAngle, 2.2f, skin.accent);
            // Valeur : arc magenta par-dessus
            kArc(rotaryStartAngle, angle, 7.0f, skin.accentHi.withAlpha(0.25f));
            kArc(rotaryStartAngle, angle, 3.0f, skin.accentHi);
            // Aiguille blanche fine
            const float kdx = std::sin(angle), kdy = -std::cos(angle);
            const float kr0 = radius * 0.18f, kr1 = radius * 0.90f;
            g.setColour(skin.knobNeedle.withAlpha(0.95f));
            g.drawLine(cx + kdx * kr0, cy + kdy * kr0,
                       cx + kdx * kr1, cy + kdy * kr1, 1.4f);
            return;
        }

        // Arc de progression (piste sombre + valeur accent)
        auto strokeArc = [&](float a0, float a1, float thick)
        {
            juce::Path p;
            int steps = 40;
            for (int i = 0; i <= steps; ++i)
            {
                float a = a0 + (a1 - a0) * (float) i / (float) steps;
                float px = cx + std::sin(a) * radius;
                float py = cy - std::cos(a) * radius;
                if (i == 0) p.startNewSubPath(px, py);
                else p.lineTo(px, py);
            }
            g.strokePath(p, juce::PathStrokeType(thick, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        };
        g.setColour(skin.panelBottom);
        strokeArc(rotaryStartAngle, rotaryEndAngle, 4.5f);
        // Bague lumineuse : couleur dediee (knobRing), tombe sur accent par defaut.
        g.setColour(skin.knobRing.withAlpha(0.30f));
        strokeArc(rotaryStartAngle, angle, 8.0f);
        g.setColour(skin.knobRingHi);
        strokeArc(rotaryStartAngle, angle, 4.0f);

        // Aiguille fine + point lumineux
        float dx = std::sin(angle), dy = -std::cos(angle);
        float r0 = radius * 0.30f, r1 = radius * 0.88f;
        g.setColour(skin.knobNeedle);
        g.drawLine(cx + dx * r0, cy + dy * r0, cx + dx * r1, cy + dy * r1, 2.0f);
        g.setColour(skin.knobRingHi);
        g.fillEllipse(cx + dx * r1 - 2.5f, cy + dy * r1 - 2.5f, 5.0f, 5.0f);
    }

    // --- Curseur vertical : rail encastre + capot metal ---
    int getSliderThumbRadius (juce::Slider&) override { return 12; }
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical && style != juce::Slider::ThreeValueVertical)
        {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0, 0, style, slider);
            return;
        }

        // ---- NIGHTCITY : rail lumineux (rempli magenta en haut -> cyan en bas) ----
        if (skin.neon && !materialFader.isValid())
        {
            const float ncx    = (float) x + (float) width * 0.5f;
            const float railW  = 7.0f;
            const float ntop   = (float) y + 3.0f;
            const float nbot   = (float) y + (float) height - 3.0f;
            const juce::Rectangle<float> rail(ncx - railW * 0.5f, ntop, railW, nbot - ntop);

            // Halo du rail
            g.setColour(skin.accent.withAlpha(0.10f));
            g.fillPath(chamferRect(rail.expanded(3.5f), 4.0f));

            // Fond du rail
            g.setColour(juce::Colour(0xff050b1c));
            g.fillPath(chamferRect(rail, 3.0f));
            g.setColour(skin.accentDeep.withAlpha(0.60f));
            g.strokePath(chamferRect(rail, 3.0f), juce::PathStrokeType(1.0f));

            // Partie remplie (de la position courante vers le bas)
            const float vTop = juce::jlimit(ntop, nbot, sliderPos);
            if (nbot - vTop > 1.0f)
            {
                juce::ColourGradient vg(skin.accentHi, ncx, vTop, skin.accent, ncx, nbot, false);
                g.setGradientFill(vg);
                g.fillPath(chamferRect(rail.withTop(vTop), 3.0f));
            }

            auto drawNeonCap = [&](float cy)
            {
                const float cw = (float) width - 8.0f, ch = 11.0f;
                g.setColour(skin.accent.withAlpha(0.22f));
                g.fillPath(chamferRect({ ncx - cw * 0.5f - 2.0f, cy - ch * 0.5f - 2.0f,
                                         cw + 4.0f, ch + 4.0f }, 4.0f));
                juce::ColourGradient cap(skin.accent, ncx, cy - ch * 0.5f,
                                         skin.accentDeep, ncx, cy + ch * 0.5f, false);
                g.setGradientFill(cap);
                g.fillPath(chamferRect({ ncx - cw * 0.5f, cy - ch * 0.5f, cw, ch }, 4.0f));
                g.setColour(juce::Colours::white.withAlpha(0.55f));
                g.drawLine(ncx - cw * 0.5f + 3.0f, cy, ncx + cw * 0.5f - 3.0f, cy, 1.2f);
            };

            if (style == juce::Slider::ThreeValueVertical)
            {
                auto nYOf = [&](double v)
                {
                    return nbot - (float) slider.valueToProportionOfLength(v) * (nbot - ntop);
                };
                drawNeonCap(nYOf(slider.getMinValue()));
                drawNeonCap(nYOf(slider.getMaxValue()));
            }
            else
            {
                drawNeonCap(sliderPos);
            }
            return;
        }

        // ---- KAY<3 : rail fin cyan, rempli du haut jusqu'a la poignee ----
        if (skin.kay && !materialFader.isValid())
        {
            const float kcx     = (float) x + (float) width * 0.5f;
            const float kRailW  = 6.0f;
            const float ktop    = (float) y + 3.0f;
            const float kbot    = (float) y + (float) height - 3.0f;
            const juce::Rectangle<float> krail(kcx - kRailW * 0.5f, ktop, kRailW, kbot - ktop);

            g.setColour(juce::Colour(0xff04081a));
            g.fillRoundedRectangle(krail, 3.0f);
            g.setColour(skin.accent.withAlpha(0.45f));
            g.drawRoundedRectangle(krail, 3.0f, 1.0f);

            // Remplissage : du haut du rail jusqu'a la poignee
            const float kTop = juce::jlimit(ktop, kbot, sliderPos);
            if (kTop - ktop > 1.0f)
            {
                g.setGradientFill(juce::ColourGradient(skin.accent, kcx, ktop,
                                                       skin.accentDeep, kcx, kTop, false));
                g.fillRoundedRectangle(krail.withBottom(kTop), 3.0f);
            }

            auto drawKayCap = [&](float cy)
            {
                const float cw = (float) width - 4.0f, ch = 10.0f;
                g.setGradientFill(juce::ColourGradient(skin.accent, kcx, cy - ch * 0.5f,
                                                       skin.accentDeep, kcx, cy + ch * 0.5f, false));
                g.fillRoundedRectangle(kcx - cw * 0.5f, cy - ch * 0.5f, cw, ch, 2.5f);
                g.setColour(juce::Colours::white.withAlpha(0.55f));
                g.drawLine(kcx - cw * 0.5f + 3.0f, cy, kcx + cw * 0.5f - 3.0f, cy, 1.0f);
            };

            if (style == juce::Slider::ThreeValueVertical)
            {
                auto kYOf = [&](double v)
                {
                    return kbot - (float) slider.valueToProportionOfLength(v) * (kbot - ktop);
                };
                drawKayCap(kYOf(slider.getMinValue()));
                drawKayCap(kYOf(slider.getMaxValue()));
            }
            else
            {
                drawKayCap(sliderPos);
            }
            return;
        }

        float cx = (float) x + (float) width * 0.5f;
        float railW = 10.0f;
        float top = (float) y + 2.0f, bottom = (float) y + (float) height - 2.0f;
        if (skin.crystalFaceplate || materialFader.isValid())
        {
            g.setColour(skin.sectionTitle.withAlpha(0.55f));
            for (int i = 0; i <= 10; ++i)
            {
                const float tickY = top + (bottom - top) * (float) i / 10.0f;
                const float edge = cx - railW * 0.5f - 4.0f;
                g.drawLine(edge - (i == 5 ? 8.0f : 4.0f), tickY, edge, tickY, 1.0f);
            }
        }
        // Rail encastre
        g.setColour(skin.trackBg);
        g.fillRoundedRectangle(cx - railW * 0.5f, top, railW, bottom - top, 4.0f);
        g.setColour(skin.trackEdge);
        g.drawRoundedRectangle(cx - railW * 0.5f, top, railW, bottom - top, 4.0f, 1.0f);

        auto drawCap = [&](float cy)
        {
            float cw = (float) width - 6.0f, ch = 13.0f;
            if (skin.crystalFaceplate || materialFader.isValid())
            {
                g.setColour(juce::Colours::white);
                g.drawImage(materialFader.isValid() ? materialFader : PrismeTextures::crystalFader(),
                            juce::Rectangle<float>(cx - cw * 0.5f, cy - ch * 0.5f, cw, ch),
                            juce::RectanglePlacement::stretchToFit);
                return;
            }
            juce::ColourGradient cap(skin.thumbHi, cx, cy - ch * 0.5f,
                                     skin.thumbLo, cx, cy + ch * 0.5f, false);
            g.setGradientFill(cap);
            g.fillRoundedRectangle(cx - cw * 0.5f, cy - ch * 0.5f, cw, ch, 3.0f);
            g.setColour(skin.panelBottom);
            g.drawRoundedRectangle(cx - cw * 0.5f, cy - ch * 0.5f, cw, ch, 3.0f, 1.0f);
            g.setColour(skin.accent);
            g.fillRect(cx - cw * 0.5f + 3.0f, cy - 1.0f, cw - 6.0f, 2.0f);
        };

        if (style == juce::Slider::ThreeValueVertical)
        {
            // Zone de bande eclairee entre les deux curseurs (min en bas, max en haut)
            auto yOf = [&](double v)
            {
                return bottom - (float) slider.valueToProportionOfLength(v) * (bottom - top);
            };
            float yMin = yOf(slider.getMinValue());
            float yMax = yOf(slider.getMaxValue());
            float ya = std::min(yMin, yMax), yb = std::max(yMin, yMax);
            g.setColour(skin.accent.withAlpha(0.28f));
            g.fillRoundedRectangle(cx - railW * 0.5f + 1.0f, ya, railW - 2.0f, yb - ya, 3.0f);
            drawCap(yMin);
            drawCap(yMax);
        }
        else
        {
            drawCap(sliderPos);
        }
    }

    // --- Toggle : LED + etiquette ---
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool /*highlighted*/, bool /*down*/) override
    {
        bool on = button.getToggleState();
        float d = 13.0f;
        float cy = (float) button.getHeight() * 0.5f;
        if (on)
        {
            // NIGHTCITY : halo plus large autour de la LED allumee
            if (skin.neon)
            {
                g.setColour(skin.accent.withAlpha(0.14f));
                g.fillEllipse(2.0f - 7.0f, cy - d * 0.5f - 7.0f, d + 14.0f, d + 14.0f);
            }
            g.setColour(skin.accent.withAlpha(0.30f));
            g.fillEllipse(2.0f - 3.0f, cy - d * 0.5f - 3.0f, d + 6.0f, d + 6.0f);
            juce::ColourGradient led(skin.accentHi, 4.0f, cy - 4.0f, skin.accentDeep, 4.0f, cy + 4.0f, false);
            g.setGradientFill(led);
            g.fillEllipse(2.0f, cy - d * 0.5f, d, d);
        }
        else
        {
            juce::ColourGradient led(skin.ledOffHi, 4.0f, cy - 4.0f, skin.ledOffLo, 4.0f, cy + 4.0f, false);
            g.setGradientFill(led);
            g.fillEllipse(2.0f, cy - d * 0.5f, d, d);
        }
        g.setColour(skin.panelBottom);
        g.drawEllipse(2.0f, cy - d * 0.5f, d, d, 1.0f);

        g.setColour(button.findColour(juce::ToggleButton::textColourId));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText(button.getButtonText(), 20, 0, button.getWidth() - 20, button.getHeight(),
                   juce::Justification::centredLeft);
    }

    // --- Combo encastre ---
    void drawComboBox(juce::Graphics& g, int width, int height, bool /*isDown*/,
                      int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                      juce::ComboBox& box) override
    {
        // ---- KAY<3 : fond nuit + liseré cyan fin (maquette) ----
        if (skin.kay)
        {
            juce::Rectangle<float> nr(0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
            juce::ColourGradient cg(juce::Colour(0xff0a1430), 0.0f, nr.getY(),
                                    juce::Colour(0xff04081a), 0.0f, nr.getBottom(), false);
            g.setGradientFill(cg);
            g.fillRoundedRectangle(nr, 5.0f);
            g.setColour(skin.accent.withAlpha(0.16f));
            g.drawRoundedRectangle(nr.expanded(1.5f), 6.5f, 3.0f);
            g.setColour(skin.accent.withAlpha(0.85f));
            g.drawRoundedRectangle(nr, 5.0f, 1.2f);
            const float kax = (float) width - 16.0f, kay2 = (float) height * 0.5f;
            juce::Path karrow;
            karrow.addTriangle(kax - 5.0f, kay2 - 3.0f, kax + 5.0f, kay2 - 3.0f, kax, kay2 + 4.0f);
            g.setColour(skin.accent);
            g.fillPath(karrow);
            juce::ignoreUnused(box);
            return;
        }

        // ---- NIGHTCITY : combo a cotes plats + leger degrade vertical ----
        if (skin.neon)
        {
            juce::Rectangle<float> nr(0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
            juce::ColourGradient ng(juce::Colour(0xff0a1730), 0.0f, nr.getY(),
                                    juce::Colour(0xff03091a), 0.0f, nr.getBottom(), false);
            g.setGradientFill(ng);
            g.fillPath(chamferRect(nr, 6.0f));
            g.setColour(skin.accent.withAlpha(0.16f));
            g.strokePath(chamferRect(nr.expanded(2.5f), 8.0f), juce::PathStrokeType(2.0f));
            g.setColour(skin.accent.withAlpha(0.75f));
            g.strokePath(chamferRect(nr, 6.0f), juce::PathStrokeType(1.0f));
            float nax = (float) width - 16.0f, nay = (float) height * 0.5f;
            juce::Path narrow;
            narrow.addTriangle(nax - 5.0f, nay - 3.0f, nax + 5.0f, nay - 3.0f, nax, nay + 4.0f);
            g.setColour(skin.accent);
            g.fillPath(narrow);
            juce::ignoreUnused(box);
            return;
        }

        juce::Rectangle<float> r(0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(juce::Colour(0xff000000));
        g.drawRoundedRectangle(r, 5.0f, 1.0f);
        g.setColour(skin.bevelHi);
        g.drawLine(4.0f, 1.5f, (float) width - 4.0f, 1.5f, 1.0f);
        // Fleche
        float ax = (float) width - 16.0f, ay = (float) height * 0.5f;
        juce::Path arrow;
        arrow.addTriangle(ax - 5.0f, ay - 3.0f, ax + 5.0f, ay - 3.0f, ax, ay + 4.0f);
        g.setColour(skin.comboArrow);
        g.fillPath(arrow);
    }

    // --- Affichage de valeur (boite de texte des curseurs) ---
    // KAY<3 : fond nuit + liseré cyan + texte cyan, comme la maquette.
    // Les autres etiquettes du plugin ne sont pas touchees.
    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        const bool isSliderReadout = (dynamic_cast<juce::Slider*>(label.getParentComponent()) != nullptr);
        if (skin.kay && isSliderReadout)
        {
            const juce::Rectangle<float> r(0.0f, 0.0f,
                                           (float) label.getWidth(), (float) label.getHeight());
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0a1430), 0.0f, r.getY(),
                                                   juce::Colour(0xff04081a), 0.0f, r.getBottom(),
                                                   false));
            g.fillRoundedRectangle(r, 3.5f);
            g.setColour(skin.accent.withAlpha(0.75f));
            g.drawRoundedRectangle(r.reduced(0.5f), 3.5f, 1.1f);
            g.setColour(skin.readoutText);
            g.setFont(label.getFont());
            g.drawFittedText(label.getText(), label.getLocalBounds(),
                             juce::Justification::centred, 1);
            return;
        }
        juce::LookAndFeel_V4::drawLabel(g, label);
    }

    // --- Bouton metal (teinte derivee de buttonColourId : A/B actif = corail) ---
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& bg,
                              bool highlighted, bool down) override
    {
        auto r = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);
        juce::Colour base = bg.withAlpha(1.0f);

        // ---- KAY<3 : bouton "fond nuit + liseré" (cyan ou magenta selon le
        // bouton) ; les boutons marques kaySolid sont remplis pleins. ----
        if (skin.kay)
        {
            if (base.getAlpha() < 40) base = skin.buttonBg;
            const bool solid   = (bool) button.getProperties()["kaySolid"];
            const bool neutral = base.getPerceivedBrightness() < 0.32f;
            const juce::Colour line = neutral ? skin.accent : base;

            if (solid)
            {
                g.setColour(base);
                g.fillRoundedRectangle(r, 5.0f);
                g.setColour(base.brighter(0.35f).withAlpha(0.55f));
                g.drawRoundedRectangle(r, 5.0f, 1.0f);
            }
            else
            {
                juce::ColourGradient cg(juce::Colour(0xff0a1430), 0.0f, r.getY(),
                                        juce::Colour(0xff04081a), 0.0f, r.getBottom(), false);
                g.setGradientFill(cg);
                g.fillRoundedRectangle(r, 5.0f);
                g.setColour(line.withAlpha(0.16f));
                g.drawRoundedRectangle(r.expanded(1.5f), 6.5f, 3.0f);
                g.setColour(line.withAlpha(0.85f));
                g.drawRoundedRectangle(r, 5.0f, 1.2f);
            }
            juce::ignoreUnused(highlighted, down);
            return;
        }

        // ---- NIGHTCITY : bouton a cotes plats + leger degrade ----
        if (skin.neon)
        {
            if (base.getAlpha() < 40) base = skin.buttonBg;   // boutons "neutres"
            g.setColour(skin.accent.withAlpha(0.16f));
            g.strokePath(chamferRect(r.expanded(2.0f), 8.0f), juce::PathStrokeType(2.0f));
            juce::ColourGradient ngrad(down ? base.darker(0.10f) : base.brighter(0.18f), 0.0f, r.getY(),
                                       base.darker(0.22f), 0.0f, r.getBottom(), false);
            g.setGradientFill(ngrad);
            g.fillPath(chamferRect(r, 6.0f));
            g.setColour(skin.accent.withAlpha(0.60f));
            g.strokePath(chamferRect(r, 6.0f), juce::PathStrokeType(1.0f));
            g.setColour(juce::Colours::white.withAlpha(0.10f));
            g.drawLine(r.getX() + 7.0f, r.getY() + 1.5f, r.getRight() - 7.0f, r.getY() + 1.5f, 1.0f);
            juce::ignoreUnused(highlighted);
            return;
        }

        juce::Colour top = down ? base.darker(0.10f) : base.brighter(0.10f);
        juce::Colour bot = base.darker(0.18f);
        if (highlighted && ! down) top = top.brighter(0.08f);
        juce::ColourGradient grad(top, 0.0f, 0.0f, bot, 0.0f, r.getHeight(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(juce::Colour(0xff0c0d0f));
        g.drawRoundedRectangle(r, 5.0f, 1.0f);
        g.setColour(top.brighter(0.25f));
        g.drawLine(r.getX() + 5.0f, r.getY() + 1.0f, r.getRight() - 5.0f, r.getY() + 1.0f, 1.0f);
        juce::ignoreUnused(button);
    }
};
