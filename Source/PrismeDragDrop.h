#pragma once
// Copie locale d'une prise AVANT de la glisser vers le DAW.
// Utilise par le tableau SAMPLES (interface principale) et par la vue Recorder.
//
// POURQUOI :
//  Les prises sont enregistrees dans "Documents/Prisme Takes", qui se trouve
//  dans OneDrive sur cette machine. Glisser le chemin d'origine faisait que
//  FL Studio ne pouvait pas reecrire le fichier : "Impossible d'appliquer les
//  effets precalculs". On glisse donc une copie locale, refermee par PRISME
//  avant de la proposer au DAW.
//
// REGLES :
//  - Thread GUI uniquement (appele depuis mouseDrag). Jamais le thread audio.
//  - Le nom d'origine est conserve : le DAW affiche le meme nom de sample.
//  - Aucune suppression. Si la copie precedente est encore ouverte par le DAW,
//    on number la suivante (_1, _2, ...) au lieu d'effacer.
//  - L'original n'est jamais ecrit ni modifie.
#include <juce_core/juce_core.h>

namespace PrismeDragDrop
{
    inline juce::File makeDraggableCopy (const juce::File& src)
    {
        if (! src.existsAsFile())
            return juce::File();

        const juce::File dir = juce::File::getSpecialLocation (
            juce::File::tempDirectory).getChildFile ("PrismeDrag");
        if (! dir.createDirectory())
            return src; // dossier local indisponible : on glisse l'original

        const juce::String ext  = src.getFileExtension();
        const juce::String base = src.getFileNameWithoutExtension();
        const juce::int64 size  = src.getSize();

        for (int attempt = 0; attempt < 100; ++attempt)
        {
            const juce::String name = (attempt == 0
                ? src.getFileName()
                : base + "_" + juce::String (attempt) + ext);
            const juce::File dst = dir.getChildFile (name);
            if (dst == src)
                continue; // garde-fou : jamais d'ecriture sur l'original

            // Troncature explicite, sans deleteFile(). Si le DAW tient la copie
            // precedente, l'ouverture echoue et on passe au nom suivant.
            auto out = dst.createOutputStream();
            if (out == nullptr)
                continue;

            auto in = src.createInputStream();
            if (in == nullptr)
                return src;

            const juce::int64 written = out->writeFromInputStream (*in, -1);
            out.reset();   // fermeture et flush avant la verification
            in.reset();

            if (written == size && dst.getSize() == size)
                return dst;
        }
        return src; // dernier recours : le chemin d'origine
    }
}
