#pragma once
// Habillage "POST APO" : modules de couleurs differentes colles de travers,
// fond en grille de metal rouille, cadres noirs epais, oscilloscope vert et
// decor bricole (scotch, plaques, etiquettes, compteurs, cables, lampe).
//
// 100 % visuel : aucun DSP, aucun parametre, aucun composant interactif.
// Tout est dessine en code dans le repere logique 1670 x 941 de l'editeur.
// Seule image embarquee : le fond de grille (PrismeTextures::fondGrille()).
// Si elle est absente, le fond reste uni : rien ne plante.
// Appele uniquement depuis le thread GUI.
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "PrismeSkin.h"
#include "PrismeTextures.h"

namespace PrismeDiy
{
    enum class Module { Mint, Blue, Grille, Orange, GrisVert, Bordeaux };

    // Ombre portee legere SANS flou (quelques traits translucides) : le decor est
    // redessine a chaque repaint, un flou d'image serait trop couteux.
    inline void softShadow(juce::Graphics& g, const juce::Path& p, float dx, float dy,
                           float spread, float alpha)
    {
        juce::Path s(p);
        s.applyTransform(juce::AffineTransform::translation(dx, dy));
        for (int i = 4; i >= 1; --i)
        {
            g.setColour(juce::Colours::black.withAlpha(alpha * 0.25f));
            g.strokePath(s, juce::PathStrokeType(spread * (float) i * 0.5f));
        }
        g.setColour(juce::Colours::black.withAlpha(alpha * 0.6f));
        g.fillPath(s);
    }

    // ---------------------------------------------------------------- fond
    inline void paintBackdrop(juce::Graphics& g, const PrismeSkin& sk)
    {
        g.fillAll(sk.windowBg);
        const juce::Image& img = PrismeTextures::fondGrille();
        if (img.isValid())
            g.drawImage(img, juce::Rectangle<float>(0.0f, 0.0f, 1670.0f, 941.0f),
                        juce::RectanglePlacement::stretchToFit);
        g.setColour(juce::Colours::black.withAlpha(0.10f));
        g.fillRect(0.0f, 0.0f, 1670.0f, 941.0f);
    }

    // Petite tuile de grain (poussiere, eclats, rouille), generee une seule fois.
    inline const juce::Image& noiseTile()
    {
        static const juce::Image img = []
        {
            juce::Image im(juce::Image::ARGB, 128, 128, true);
            juce::Random rnd(4242);
            for (int yy = 0; yy < 128; ++yy)
                for (int xx = 0; xx < 128; ++xx)
                {
                    const float v = rnd.nextFloat();
                    if (v < 0.30f)
                        im.setPixelAt(xx, yy, juce::Colours::black.withAlpha(0.04f + 0.10f * rnd.nextFloat()));
                    else if (v > 0.88f)
                        im.setPixelAt(xx, yy, juce::Colours::white.withAlpha(0.04f + 0.08f * rnd.nextFloat()));
                    else if (v > 0.855f)
                        im.setPixelAt(xx, yy, juce::Colour(0xff8a4a1c).withAlpha(0.16f));
                }
            return im;
        }();
        return img;
    }

    inline void moduleColours(const PrismeSkin& sk, Module m, juce::Colour& top, juce::Colour& bot)
    {
        switch (m)
        {
            case Module::Mint:     top = sk.modKey.brighter(0.10f);      bot = sk.modKey.darker(0.22f);      break;
            case Module::Blue:     top = sk.modArp.brighter(0.08f);      bot = sk.modArp.darker(0.25f);      break;
            case Module::Orange:   top = sk.modMirageTop;                bot = sk.modMirageBot;              break;
            case Module::GrisVert: top = sk.modSpectrum.brighter(0.10f); bot = sk.modSpectrum.darker(0.25f); break;
            case Module::Bordeaux: top = sk.modSamples.brighter(0.10f);  bot = sk.modSamples.darker(0.30f);  break;
            case Module::Grille:   top = juce::Colours::black.withAlpha(0.10f);
                                   bot = juce::Colours::black.withAlpha(0.28f); break;
        }
    }

    // Plaque d'un module : legerement inclinee (tiltDeg), ombre portee, degrade,
    // grain, arete sombre + reflet. Les controles restent droits par-dessus.
    inline void paintModulePlate(juce::Graphics& g, const PrismeSkin& sk, Module m,
                                 int x, int y, int w, int h, float tiltDeg)
    {
        const juce::Rectangle<float> r((float) x, (float) y, (float) w, (float) h);
        const juce::AffineTransform rot = juce::AffineTransform::rotation(
            juce::degreesToRadians(tiltDeg), r.getCentreX(), r.getCentreY());

        juce::Path plate;
        plate.addRoundedRectangle(r.expanded(3.0f), 14.0f);
        plate.applyTransform(rot);

        softShadow(g, plate, 3.0f, 5.0f, 8.0f, 0.55f);

        juce::Colour top, bot;
        moduleColours(sk, m, top, bot);
        juce::ColourGradient grad(top, 0.0f, r.getY(), bot, 0.0f, r.getBottom(), false);
        g.setGradientFill(grad);
        g.fillPath(plate);

        if (m != Module::Grille)
        {
            const juce::Image& tile = noiseTile();
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(plate, juce::AffineTransform());
            for (int yy = y - 4; yy < y + h + 4; yy += tile.getHeight())
                for (int xx = x - 4; xx < x + w + 4; xx += tile.getWidth())
                    g.drawImageAt(tile, xx, yy);
        }

        g.setColour(juce::Colour(0xff15110e));
        g.strokePath(plate, juce::PathStrokeType(3.0f));
        juce::Path inner;
        inner.addRoundedRectangle(r.expanded(0.5f), 12.0f);
        inner.applyTransform(rot);
        g.setColour(juce::Colours::white.withAlpha(0.20f));
        g.strokePath(inner, juce::PathStrokeType(1.0f));
    }

    // -------------------------------------------------------------- vis
    // kind : 0 laiton, 1 chrome, 2 noire, 3 zinc (vis volontairement dependarillees)
    inline void screw(juce::Graphics& g, float cx, float cy, float r, int kind)
    {
        juce::Colour hi, lo;
        switch (((kind % 4) + 4) % 4)
        {
            case 0:  hi = juce::Colour(0xffe0c27a); lo = juce::Colour(0xff7a5c26); break;
            case 1:  hi = juce::Colour(0xfff0f2f6); lo = juce::Colour(0xff6c727c); break;
            case 2:  hi = juce::Colour(0xff55555c); lo = juce::Colour(0xff121214); break;
            default: hi = juce::Colour(0xffb6bcb2); lo = juce::Colour(0xff555a52); break;
        }
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillEllipse(cx - r + 1.0f, cy - r + 1.5f, r * 2.0f, r * 2.0f);
        juce::ColourGradient grad(hi, cx - r * 0.5f, cy - r * 0.5f, lo, cx + r * 0.7f, cy + r * 0.7f, false);
        g.setGradientFill(grad);
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour(juce::Colour(0xff101012));
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);
        const float a = (float) (((int) (cx * 7.0f + cy * 13.0f + (float) kind * 31.0f)) % 180) * 0.0174533f;
        g.setColour(juce::Colour(0xff0e0e10));
        g.drawLine(cx - std::cos(a) * r * 0.7f, cy - std::sin(a) * r * 0.7f,
                   cx + std::cos(a) * r * 0.7f, cy + std::sin(a) * r * 0.7f, 1.4f);
        if (kind % 2 == 0)
            g.drawLine(cx - std::cos(a + 1.5708f) * r * 0.7f, cy - std::sin(a + 1.5708f) * r * 0.7f,
                       cx + std::cos(a + 1.5708f) * r * 0.7f, cy + std::sin(a + 1.5708f) * r * 0.7f, 1.4f);
    }

    // ------------------------------------------------ cadre noir encastre
    inline void paintBezel(juce::Graphics& g, juce::Rectangle<float> inner, float thick)
    {
        const juce::Rectangle<float> outer = inner.expanded(thick);
        juce::Path ring;
        ring.setUsingNonZeroWinding(false);
        ring.addRoundedRectangle(outer, 10.0f);
        ring.addRoundedRectangle(inner, 5.0f);
        softShadow(g, ring, 2.0f, 3.0f, 5.0f, 0.50f);
        g.setColour(juce::Colour(0xff0f0f12));
        g.fillPath(ring);
        g.setColour(juce::Colour(0xff4a4c54));
        g.drawRoundedRectangle(outer, 10.0f, 1.5f);
        g.setColour(juce::Colours::white.withAlpha(0.30f));
        g.drawLine(outer.getX() + 12.0f, outer.getY() + 2.5f, outer.getRight() - 12.0f, outer.getY() + 2.5f, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.60f));
        g.drawRoundedRectangle(inner, 5.0f, 1.5f);
        const float k = thick * 0.5f;
        screw(g, outer.getX() + k + 2.0f,     outer.getY() + k + 2.0f,      3.6f, 0);
        screw(g, outer.getRight() - k - 2.0f, outer.getY() + k + 2.0f,      3.6f, 1);
        screw(g, outer.getX() + k + 2.0f,     outer.getBottom() - k - 2.0f, 3.6f, 2);
        screw(g, outer.getRight() - k - 2.0f, outer.getBottom() - k - 2.0f, 3.6f, 3);
    }

    // -------------------------------------------------------- texte feutre
    // Lettres majuscules legerement decalees et penchees. Pas de mesure de
    // police : avance fixe, volontairement (texte decoratif).
    inline void markerText(juce::Graphics& g, const juce::String& text, float x, float y,
                           float size, juce::Colour col, float angleDeg)
    {
        juce::Graphics::ScopedSaveState ss(g);
        g.addTransform(juce::AffineTransform::rotation(juce::degreesToRadians(angleDeg), x, y));
        g.setFont(juce::Font(size, juce::Font::bold | juce::Font::italic));
        juce::Random rnd(text.hashCode());
        float cx = x;
        for (int i = 0; i < text.length(); ++i)
        {
            const juce::String ch = text.substring(i, i + 1);
            const bool space = (ch == " ");
            const float adv = size * (space ? 0.45f : 0.64f);
            if (! space)
            {
                juce::Graphics::ScopedSaveState s2(g);
                const float jy = (rnd.nextFloat() - 0.5f) * size * 0.14f;
                const float ja = (rnd.nextFloat() - 0.5f) * 0.16f;
                g.addTransform(juce::AffineTransform::rotation(ja, cx + adv * 0.5f, y + jy));
                g.setColour(col);
                g.drawText(ch, juce::Rectangle<float>(cx, y + jy - size * 0.6f, adv, size * 1.2f),
                           juce::Justification::centred, false);
            }
            cx += adv;
        }
    }

    // -------------------------------------------------------------- scotch
    inline void tape(juce::Graphics& g, float cx, float cy, float w, float h, float angleDeg,
                     juce::Colour col, const juce::String& text = {})
    {
        const juce::AffineTransform rot = juce::AffineTransform::rotation(juce::degreesToRadians(angleDeg), cx, cy);
        juce::Random rnd((int) (cx * 3.0f + cy * 5.0f));
        const float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f, y0 = cy - h * 0.5f, y1 = cy + h * 0.5f;

        juce::Path p;                                  // bouts dechires
        p.startNewSubPath(x0 + rnd.nextFloat() * 3.0f, y0);
        p.lineTo(x1 - rnd.nextFloat() * 3.0f, y0);
        const int teeth = 6;
        for (int i = 1; i <= teeth; ++i)
            p.lineTo(x1 - rnd.nextFloat() * 5.0f, y0 + (y1 - y0) * (float) i / (float) teeth);
        p.lineTo(x0 + rnd.nextFloat() * 3.0f, y1);
        for (int i = teeth - 1; i >= 1; --i)
            p.lineTo(x0 + rnd.nextFloat() * 5.0f, y0 + (y1 - y0) * (float) i / (float) teeth);
        p.closeSubPath();
        p.applyTransform(rot);

        softShadow(g, p, 2.0f, 3.0f, 3.0f, 0.45f);
        g.setColour(col);
        g.fillPath(p);
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(p, juce::AffineTransform());
            g.addTransform(rot);
            for (float yy = y0 + 1.0f; yy < y1; yy += 2.0f)
            {
                g.setColour(((int) yy % 4 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha(0.06f));
                g.drawLine(x0, yy, x1, yy, 1.0f);
            }
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.drawLine(x0, y0 + 1.5f, x1, y0 + 1.5f, 1.2f);
            g.setColour(juce::Colours::black.withAlpha(0.25f));
            g.drawLine(x0, y1 - 1.5f, x1, y1 - 1.5f, 1.2f);
        }
        if (text.isNotEmpty())
        {
            const juce::Point<float> o = juce::Point<float>(x0 + 8.0f, cy + h * 0.22f).transformedBy(rot);
            markerText(g, text, o.x, o.y, h * 0.62f, juce::Colour(0xff15151a), angleDeg);
        }
    }

    // ------------------------------------------- plaque de metal recuperee
    // metal : 0 laiton, 1 galvanise
    inline void plate(juce::Graphics& g, float cx, float cy, float w, float h, float angleDeg,
                      int metal, const juce::String& text)
    {
        const juce::AffineTransform rot = juce::AffineTransform::rotation(juce::degreesToRadians(angleDeg), cx, cy);
        const juce::Rectangle<float> r(cx - w * 0.5f, cy - h * 0.5f, w, h);
        juce::Path p;
        p.addRoundedRectangle(r, 3.0f);
        p.applyTransform(rot);
        softShadow(g, p, 2.0f, 3.0f, 4.0f, 0.50f);
        const juce::Colour a = (metal == 0) ? juce::Colour(0xffc09a56) : juce::Colour(0xffa4adb4);
        const juce::Colour b = (metal == 0) ? juce::Colour(0xff7d6030) : juce::Colour(0xff626a72);
        juce::ColourGradient grad(a, 0.0f, r.getY(), b, 0.0f, r.getBottom(), false);
        g.setGradientFill(grad);
        g.fillPath(p);
        {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion(p, juce::AffineTransform());
            g.addTransform(rot);
            for (float yy = r.getY() + 2.0f; yy < r.getBottom(); yy += 2.0f)
            {
                g.setColour(((int) yy % 4 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha(0.07f));
                g.drawLine(r.getX(), yy, r.getRight(), yy, 1.0f);
            }
        }
        g.setColour(juce::Colour(0xff14120f));
        g.strokePath(p, juce::PathStrokeType(1.5f));
        const float sx = w * 0.5f - 6.0f, sy = h * 0.5f - 6.0f;
        const float sxs[4] = { -sx, sx, -sx, sx }, sys[4] = { -sy, -sy, sy, sy };
        for (int i = 0; i < 4; ++i)
        {
            const juce::Point<float> q = juce::Point<float>(cx + sxs[i], cy + sys[i]).transformedBy(rot);
            screw(g, q.x, q.y, 2.6f + (float) (i % 2) * 0.8f, (metal + i) % 4);
        }
        if (text.isNotEmpty())
        {
            const juce::Point<float> o = juce::Point<float>(cx - w * 0.22f, cy + h * 0.12f).transformedBy(rot);
            markerText(g, text, o.x, o.y, h * 0.58f, juce::Colour(0xff2a1e0a), angleDeg);
        }
    }

    // ------------------------------------------------------- etiquette papier
    inline void sticker(juce::Graphics& g, float cx, float cy, float w, float h, float angleDeg,
                        const juce::String& text)
    {
        const juce::AffineTransform rot = juce::AffineTransform::rotation(juce::degreesToRadians(angleDeg), cx, cy);
        const float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f, y0 = cy - h * 0.5f, y1 = cy + h * 0.5f;
        juce::Path p;
        p.startNewSubPath(x0, y0);
        p.lineTo(x1, y0);
        p.lineTo(x1, y1 - h * 0.32f);
        p.lineTo(x1 - w * 0.12f, y1);
        p.lineTo(x0, y1);
        p.closeSubPath();
        p.applyTransform(rot);
        softShadow(g, p, 2.0f, 3.0f, 3.0f, 0.45f);
        juce::ColourGradient grad(juce::Colour(0xffe8d9a6), 0.0f, y0, juce::Colour(0xffcdbb80), 0.0f, y1, false);
        g.setGradientFill(grad);
        g.fillPath(p);
        g.setColour(juce::Colour(0xff7a6638));
        g.strokePath(p, juce::PathStrokeType(1.0f));
        const juce::Point<float> o = juce::Point<float>(x0 + w * 0.16f, cy + h * 0.18f).transformedBy(rot);
        markerText(g, text, o.x, o.y, h * 0.58f, juce::Colour(0xff2b2116), angleDeg);
        tape(g, cx, y0 + 1.0f, w * 0.42f, 8.0f, angleDeg + 2.0f, juce::Colour(0xb0d8cfae));
    }

    // -------------------------------------------- compteur mecanique (decor)
    // Chiffres fixes : purement decoratif, jamais une valeur du plugin.
    inline void counter(juce::Graphics& g, float x, float y, const juce::String& digits,
                        float cellW, float cellH)
    {
        const int n = digits.length();
        const juce::Rectangle<float> box(x, y, cellW * (float) n + 8.0f, cellH + 8.0f);
        juce::Path boxPath;
        boxPath.addRoundedRectangle(box, 5.0f);
        softShadow(g, boxPath, 1.0f, 2.0f, 3.0f, 0.50f);
        g.setColour(juce::Colour(0xff101012));
        g.fillRoundedRectangle(box, 5.0f);
        g.setColour(juce::Colour(0xff5f626a));
        g.drawRoundedRectangle(box, 5.0f, 1.2f);
        g.setFont(juce::Font(cellH * 0.9f, juce::Font::bold));
        for (int i = 0; i < n; ++i)
        {
            const juce::Rectangle<float> c(x + 4.0f + cellW * (float) i, y + 4.0f, cellW, cellH);
            g.setColour(juce::Colour(0xff222226));
            g.fillRect(c.reduced(1.0f, 0.0f));
            g.setColour(juce::Colour(0xffeee8d6));
            g.drawText(digits.substring(i, i + 1), c, juce::Justification::centred, false);
            g.setColour(juce::Colour(0xff5a5a60));
            g.drawLine(c.getX(), c.getY(), c.getX(), c.getBottom(), 1.0f);
        }
        g.setColour(juce::Colours::white.withAlpha(0.09f));
        g.fillRect(x + 4.0f, y + 4.0f, cellW * (float) n, cellH * 0.42f);
    }

    // ------------------------------------------------------------- cables
    inline void cable(juce::Graphics& g, float x0, float y0, float x1, float y1,
                      float sag, float thick, juce::Colour col)
    {
        juce::Path p;
        p.startNewSubPath(x0, y0);
        p.cubicTo(x0, y0 + sag * 0.6f, x1, y1 - sag, x1, y1);
        juce::Path sh(p);
        sh.applyTransform(juce::AffineTransform::translation(3.0f, 4.0f));
        g.setColour(juce::Colours::black.withAlpha(0.40f));
        g.strokePath(sh, juce::PathStrokeType(thick + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(col);
        g.strokePath(p, juce::PathStrokeType(thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path hl(p);
        hl.applyTransform(juce::AffineTransform::translation(-thick * 0.22f, -thick * 0.22f));
        g.setColour(col.brighter(0.7f).withAlpha(0.55f));
        g.strokePath(hl, juce::PathStrokeType(thick * 0.28f));
        // fiche jack au bout
        g.setColour(juce::Colour(0xffb4b6bc));
        g.fillRoundedRectangle(x1 - 4.0f, y1 - 1.0f, 8.0f, 18.0f, 2.0f);
        g.setColour(juce::Colour(0xff3a3c42));
        g.drawRoundedRectangle(x1 - 4.0f, y1 - 1.0f, 8.0f, 18.0f, 2.0f, 1.0f);
        g.setColour(juce::Colour(0xffe2e4ea));
        g.fillRect(x1 - 1.5f, y1 + 17.0f, 3.0f, 9.0f);
    }

    // ------------------------------------------------------ lampe baladeuse
    inline void lamp(juce::Graphics& g, float x, float topY, float cableLen)
    {
        const float by = topY + cableLen + 14.0f;
        g.setColour(juce::Colour(0xff4a4a50));
        g.fillRoundedRectangle(x - 9.0f, topY - 2.0f, 18.0f, 4.0f, 2.0f);
        g.setColour(juce::Colour(0xff17171a));
        g.drawLine(x, topY, x, by - 14.0f, 2.2f);
        g.setColour(juce::Colour(0xff2a2a2e));
        g.fillRoundedRectangle(x - 6.0f, by - 14.0f, 12.0f, 10.0f, 2.0f);
        juce::ColourGradient glow(juce::Colour(0xffffd78a).withAlpha(0.40f), x, by,
                                  juce::Colour(0xffffa030).withAlpha(0.0f), x + 46.0f, by, true);
        g.setGradientFill(glow);
        g.fillEllipse(x - 46.0f, by - 46.0f, 92.0f, 92.0f);
        g.setColour(juce::Colour(0xfffff0b8));
        g.fillEllipse(x - 10.0f, by - 10.0f, 20.0f, 20.0f);
        g.setColour(juce::Colour(0xff222226));
        for (int k = -2; k <= 2; ++k)
            g.drawLine(x + (float) k * 4.0f, by - 12.0f, x + (float) k * 2.0f, by + 12.0f, 1.2f);
        g.drawEllipse(x - 12.0f, by - 12.0f, 24.0f, 24.0f, 1.4f);
        g.drawLine(x - 11.0f, by, x + 11.0f, by, 1.2f);
    }

    // ----------------------------------------------- rouille et filigrane
    // Coulures sous les vis : deterministes (graine fixe), un trait par pas.
    inline void rustStreaks(juce::Graphics& g, float x, float y, int count, int seed, float len)
    {
        juce::Random rnd(seed);
        for (int i = 0; i < count; ++i)
        {
            const float sx = x + (rnd.nextFloat() - 0.5f) * 18.0f;
            const float L = len * (0.5f + 0.5f * rnd.nextFloat());
            const float wd = 1.5f + 2.0f * rnd.nextFloat();
            for (float t = 0.0f; t < L; t += 3.0f)
            {
                g.setColour(juce::Colour(0xff9a4a1c).withAlpha(0.38f * (1.0f - t / L)));
                g.drawLine(sx + std::sin(t * 0.11f + (float) i) * 1.2f, y + t,
                           sx + std::sin((t + 3.0f) * 0.11f + (float) i) * 1.2f, y + t + 3.0f, wd);
            }
        }
    }

    inline void gearOutline(juce::Graphics& g, float cx, float cy, float R, int teeth, float phase, juce::Colour col)
    {
        juce::Path p;
        for (int i = 0; i < teeth * 4; ++i)
        {
            const float a = phase + (float) i * 6.2831853f / (float) (teeth * 4);
            const float rr = (i % 4 == 1 || i % 4 == 2) ? R + 8.0f : R;
            const float px = cx + rr * std::cos(a), py = cy + rr * std::sin(a);
            if (i == 0) p.startNewSubPath(px, py); else p.lineTo(px, py);
        }
        p.closeSubPath();
        g.setColour(col);
        g.strokePath(p, juce::PathStrokeType(1.2f));
        g.drawEllipse(cx - R * 0.7f, cy - R * 0.7f, R * 1.4f, R * 1.4f, 1.0f);
        g.drawEllipse(cx - R * 0.25f, cy - R * 0.25f, R * 0.5f, R * 0.5f, 1.0f);
        for (int i = 0; i < 6; ++i)
        {
            const float a = phase + (float) i * 1.0471976f;
            g.drawLine(cx + R * 0.25f * std::cos(a), cy + R * 0.25f * std::sin(a),
                       cx + R * 0.7f * std::cos(a), cy + R * 0.7f * std::sin(a), 1.0f);
        }
    }

    // Plan technique imprime sur la plaque (dessine SOUS les controles).
    inline void blueprint(juce::Graphics& g, juce::Rectangle<float> area)
    {
        const juce::Colour ink(0xff1a0f08);
        juce::Graphics::ScopedSaveState ss(g);
        g.reduceClipRegion(area.toNearestInt());
        const juce::Colour c = ink.withAlpha(0.34f);
        const float cx1 = area.getX() + area.getWidth() * 0.42f, cy1 = area.getY() + area.getHeight() * 0.68f;
        gearOutline(g, cx1, cy1, 84.0f, 18, 0.10f, c);
        gearOutline(g, cx1 + 168.0f, cy1 - 26.0f, 52.0f, 11, 0.35f, c);
        gearOutline(g, area.getX() + 92.0f, cy1 + 22.0f, 40.0f, 9, 0.20f, c);
        g.setColour(c);
        for (float x = cx1 - 110.0f; x < cx1 + 110.0f; x += 22.0f)
            g.drawLine(x, cy1, x + 12.0f, cy1, 1.0f);
        for (float y = cy1 - 110.0f; y < cy1 + 110.0f; y += 22.0f)
            g.drawLine(cx1, y, cx1, y + 12.0f, 1.0f);
        const float dy = area.getBottom() - 22.0f;
        g.drawLine(cx1 - 100.0f, dy, cx1 + 100.0f, dy, 1.2f);
        g.drawLine(cx1 - 100.0f, dy - 6.0f, cx1 - 100.0f, dy + 6.0f, 1.2f);
        g.drawLine(cx1 + 100.0f, dy - 6.0f, cx1 + 100.0f, dy + 6.0f, 1.2f);
        // trace de circuit a angles droits
        juce::Path tr;
        tr.startNewSubPath(area.getRight() - 90.0f, area.getY() + 120.0f);
        tr.lineTo(area.getRight() - 90.0f, area.getY() + 170.0f);
        tr.lineTo(area.getRight() - 50.0f, area.getY() + 170.0f);
        tr.lineTo(area.getRight() - 50.0f, area.getY() + 300.0f);
        g.strokePath(tr, juce::PathStrokeType(1.4f));
    }

    // Couche SOUS les controles : filigrane + coulures de rouille.
    inline void paintUnderlay(juce::Graphics& g)
    {
        blueprint(g, juce::Rectangle<float>(16.0f, 58.0f, 644.0f, 400.0f));
        rustStreaks(g, 30.0f, 66.0f, 3, 11, 70.0f);
        rustStreaks(g, 646.0f, 66.0f, 3, 12, 90.0f);
        rustStreaks(g, 684.0f, 66.0f, 2, 13, 60.0f);
        rustStreaks(g, 1020.0f, 66.0f, 3, 14, 80.0f);
        rustStreaks(g, 1060.0f, 66.0f, 2, 15, 50.0f);
        rustStreaks(g, 1640.0f, 66.0f, 3, 16, 90.0f);
        rustStreaks(g, 1060.0f, 294.0f, 2, 17, 60.0f);
        rustStreaks(g, 1640.0f, 294.0f, 3, 18, 110.0f);
        rustStreaks(g, 30.0f, 474.0f, 2, 19, 50.0f);
        rustStreaks(g, 30.0f, 682.0f, 3, 20, 70.0f);
        rustStreaks(g, 684.0f, 494.0f, 3, 21, 90.0f);
    }

    // Couche AU-DESSUS des controles : seulement dans des zones libres.
    // Rien ici n'intercepte la souris (paintOverChildren ne gere pas les clics).
    inline void paintOverlay(juce::Graphics& g)
    {
        // cables qui pendent du bandeau, dans l'interstice entre colonnes
        cable(g, 658.0f, 52.0f, 656.0f, 150.0f, 18.0f, 4.5f, juce::Colour(0xff1a1a1d));
        cable(g, 664.0f, 52.0f, 667.0f, 188.0f, -16.0f, 4.5f, juce::Colour(0xff4a1d17));
        cable(g, 670.0f, 52.0f, 664.0f, 222.0f, 20.0f, 4.0f, juce::Colour(0xff1d2a1f));
        tape(g, 664.0f, 98.0f, 40.0f, 12.0f, 82.0f, juce::Colour(0xff9ea4ac));

        // scotch a cheval sur les interstices entre modules
        tape(g, 664.0f, 442.0f, 72.0f, 16.0f, -8.0f, juce::Colour(0xff9ea4ac), "NE PAS TOUCHER");
        tape(g, 1350.0f, 282.0f, 132.0f, 16.0f, 2.0f, juce::Colour(0xffe2d646), "ATTENTION");

        // plaques recuperees, vis dependarillees
        plate(g, 24.0f, 56.0f, 42.0f, 26.0f, -5.0f, 0, "V2");
        plate(g, 1628.0f, 874.0f, 72.0f, 22.0f, 2.0f, 1, "ok");

        // etiquettes papier dans des zones libres du panneau KEY / SCALE
        sticker(g, 150.0f, 143.0f, 70.0f, 30.0f, -4.0f, "R-07");
        sticker(g, 592.0f, 142.0f, 62.0f, 28.0f, 4.0f, "TEST 3");

        // compteurs mecaniques (decor)
        counter(g, 1290.0f, 59.0f, "0417", 20.0f, 14.0f);
        counter(g, 470.0f, 826.0f, "034", 22.0f, 16.0f);

        // lampe baladeuse accrochee au bandeau du module SPECTRUM
        lamp(g, 1440.0f, 58.0f, 12.0f);
    }

    // ---------------------------------------------------------- oscilloscope
    // Quadrillage + axe central (a dessiner SOUS la trace).
    inline void oscGraticule(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col,
                             float stepX, int divY)
    {
        g.setColour(col.withAlpha(0.10f));
        for (float x = r.getX() + stepX; x < r.getRight(); x += stepX)
            g.drawLine(x, r.getY(), x, r.getBottom(), 1.0f);
        for (int i = 1; i < divY; ++i)
        {
            const float yy = r.getY() + r.getHeight() * (float) i / (float) divY;
            g.setColour(col.withAlpha(i * 2 == divY ? 0.22f : 0.10f));
            g.drawLine(r.getX(), yy, r.getRight(), yy, 1.0f);
        }
    }
    // Lignes de balayage (a dessiner PAR-DESSUS la trace).
    inline void oscScanlines(juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour(juce::Colours::black.withAlpha(0.16f));
        for (float yy = r.getY(); yy < r.getBottom(); yy += 2.0f)
            g.drawLine(r.getX(), yy, r.getRight(), yy, 1.0f);
    }

    // --------------------------------------------------- potard depareille
    // Remplace SEULEMENT le dessin du potard : aucune valeur, aucun parametre.
    inline void drawVintageKnob(juce::Graphics& g, float cx, float cy, float radius,
                                float angle, bool chrome)
    {
        g.setColour(juce::Colours::black.withAlpha(0.40f));
        g.fillEllipse(cx - radius + 2.0f, cy - radius + 3.0f, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0xff0e0e10));
        g.fillEllipse(cx - radius - 1.5f, cy - radius - 1.5f, radius * 2.0f + 3.0f, radius * 2.0f + 3.0f);
        const juce::Colour hi = chrome ? juce::Colour(0xfff0f3f8) : juce::Colour(0xfff4eddb);
        const juce::Colour lo = chrome ? juce::Colour(0xff5c626c) : juce::Colour(0xff8c7c5a);
        juce::ColourGradient grad(hi, cx - radius * 0.5f, cy - radius * 0.55f, lo, cx + radius * 0.8f, cy + radius * 0.8f, false);
        g.setGradientFill(grad);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(0xff17171a).withAlpha(0.8f));
        for (int i = 0; i < 40; ++i)                       // cannelures
        {
            const float a = (float) i * 6.2831853f / 40.0f;
            g.drawLine(cx + std::cos(a) * (radius - 4.0f), cy + std::sin(a) * (radius - 4.0f),
                       cx + std::cos(a) * (radius - 0.5f), cy + std::sin(a) * (radius - 0.5f), 1.0f);
        }
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.drawEllipse(cx - radius * 0.62f, cy - radius * 0.62f, radius * 1.24f, radius * 1.24f, 1.2f);
        const float dx = std::sin(angle), dy = -std::cos(angle);
        g.setColour(chrome ? juce::Colour(0xffe0382c) : juce::Colour(0xff1d1a16));
        g.drawLine(cx + dx * radius * 0.18f, cy + dy * radius * 0.18f,
                   cx + dx * radius * 0.86f, cy + dy * radius * 0.86f, 3.4f);
    }
}
