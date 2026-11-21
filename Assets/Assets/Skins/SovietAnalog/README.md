# PRISME Soviet Analog — Graphics Asset Pack

Version: 1.0.0  
Format: SVG vector + PNG raster/texture  
Style: industrial Soviet-inspired analog hardware, aged metal, graphite, muted khaki and red/amber indicators.

## Contenu

- `Backgrounds/`: fond principal texturé.
- `Textures/`: métal brossé, métal sombre, patine, rayures transparentes, grain, lignes et grille.
- `Panels/`: plaques, header, contours, vis et rivets.
- `Knobs/`: corps de potentiomètres (sans rotation intégrée), pointeurs séparés et exemples statiques.
- `Sliders/`: rails verticaux/horizontaux et poignées séparées.
- `Buttons/`: boutons, switches on/off.
- `Indicators/`: LEDs rouge, ambre, bleue, verte et éteinte.
- `Meters/`: indicateur VU et aperçu du spectre.
- `Spectrum/`: fond/grille et exemple de waveform. Les données spectrales dynamiques doivent être dessinées par le code à partir de l'audio réel.
- `Icons/`: pictogrammes en SVG et PNG.
- `Preview/`: aperçu global et catalogue HTML.
- `Documentation/`: manifeste, guide d'intégration, palette et specimen typographique.
- `Reference/`: capture originale fournie pour référence.

## Formats et utilisation

Les SVG sont à privilégier pour les formes qui doivent rester nettes à différentes tailles. Les PNG servent aux textures, superpositions et rendus raster. Les PNG des composants vectoriels sont des aperçus pratiques ; ils ne remplacent pas les SVG sources.

Les corps de potentiomètres et leurs pointeurs sont séparés pour permettre de faire pivoter le pointeur depuis le code. De même, les rails et poignées de fader sont séparés. Les labels, valeurs, graduations dynamiques et courbes de spectre ne doivent pas être aplatis dans des images.

## Important

Ce pack est une recréation visuelle basée sur la capture de référence, et non une extraction de calques source originaux (la capture ne contient pas ces calques). Les composants sont des ressources de design ; leur branchement dans PRISME demande une intégration au `LookAndFeel` / renderer propre au projet. Aucun moteur audio n'est modifié par ce pack.

## Aperçu

Ouvrir `Preview/index.html` dans un navigateur ou consulter `Preview/PRISME_SovietAnalog_Interface_Preview.png`.
