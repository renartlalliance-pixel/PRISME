# Guide d'intégration — PRISME / Soviet Analog

## Principes

1. Conserver les ressources sous `Assets/Skins/SovietAnalog/` ou le répertoire de ressources déjà employé par le projet.
2. Utiliser le SVG pour les contours et géométries nets ; utiliser les PNG pour les textures raster.
3. Centraliser les couleurs via `PALETTE.json` et/ou les constantes du LookAndFeel. Ne pas coder les couleurs en dur à chaque composant.
4. Dessiner les textes et valeurs de paramètres avec le framework ; ne pas les inclure dans les bitmaps.
5. Faire tourner le pointeur du potentiomètre autour de son centre : utiliser `knob_*_base.svg` puis superposer `knob_pointer_*.svg` avec une transformation pivotée par valeur normalisée.
6. Pour les faders, afficher `fader_track_*` comme fond puis positionner `fader_handle_*` en fonction de la valeur.
7. Pour les boutons et LEDs, sélectionner la variante correspondant à l'état réel.
8. Utiliser `spectrum_grid.svg` comme arrière-plan uniquement ; dessiner la courbe du spectre depuis l'analyse audio du plugin sur le thread graphique. Ne jamais utiliser `waveform_reference.svg` comme donnée dynamique.
9. Mettre à jour le skin sans réinitialiser les paramètres sonores ou toucher au DSP.

## Intégration JUCE indicative

- Pour les PNG, `juce::ImageCache::getFromFile(...)` peut servir au chargement si le mécanisme de ressources du projet le permet.
- Pour SVG, utiliser `juce::Drawable::createFromSVG(...)` ou convertir les SVG en `juce::Drawable` selon la version de JUCE déjà utilisée.
- Précharger/cache les ressources plutôt que de relire les fichiers pendant `paint()`.
- Ne jamais faire d'allocation lourde, de lecture de disque ou de rendu UI dans le thread audio.
- Conserver les hitboxes et événements de tous les contrôles.

Cette note est indicative : vérifier la version du framework et les conventions réelles du projet avant d'intégrer.
