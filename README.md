# PRISME — Résonateur couleur irisé

Transforme le son brut en quelque chose de **tonal, mélodique et vivant**.

Comme la lumière traversant un prisme, ce qui entre proprement ressort fracturé en
harmoniques cachées, tons chatoyants et textures vives et colorées.

## Principe

- 12 résonateurs en peigne (`comb filters`) accordés sur une **tonique + gamme / accord**
- Chaque résonateur génère une couche harmonique luxuriante
- `Decay` = temps de résonance (T60), `Couleur` = amortissement HF dans la boucle
- `Vivant` = modulation lente d'amplitude + micro-désaccordage (effet chatoyant)
- `Iridescence` = niveau des résonateurs, `Largeur` = stéréo, `Mix` = dry/wet
- `Givre (Freeze)` = sustain quasi-infini (T60 = 30 s)

Envoyez batterie, voix, bruit, guitare : Prisme révèle les harmoniques cachées.

## Paramètres

| Paramètre | Description |
|-----------|-------------|
| Tonique | C, C#, D ... B |
| Gamme / Accord | 21 choix : Major, Minor, Dorian, Phrygien, Lydien, Mixolydien, Penta Maj/Min, Maj7, Min7, Dom7, Majeur, Mineur, Diminue, Augmente, Sus2, Sus4, Add9, Six, Oct+Quinte, Chromatique |
| Octave | -1 Grave / 0 Medium / +1 Aigu |
| Resonance (Decay) | 0.2 – 8.0 s |
| Attaque | 0.005 – 2.0 s : courte = pincements percussifs, longue = houles lentes |
| Couleur | 0 sombre → 1 brillant |
| Iridescence | 0 – 1.5 niveau résonateurs |
| Vivant | 0 – 1 modulation |
| Largeur | 0 mono → 1 large stéréo |
| Mix | 0 dry → 1 full wet |
| Entrée / Sortie | -12 à +12 dB |
| Givre (dry/wet) | Fader horizontal : 0 = résonance normale, 1 = nappe gelée (double banque, sustain ~30 s, capture propre) |
| EQ Graves / Bas-Med / Haut-Med / Aigus | 150 Hz, 800 Hz, 3 kHz, 8 kHz — ±12 dB, 0 = bypass |

## Égaliseur (v1.5, en bas)

4 curseurs en bas de la fenêtre, style graphique
(Graves 150 Hz, Bas-Med 800 Hz, Haut-Med 3 kHz, Aigus 8 kHz) :
- ±12 dB, 0 = transparent (bypassé, unity bit-parfait)
- Vérifié : +6/−6 dB mesurés aux centres, pôles stables

## Bande d'effet (v2.5, fader double depuis v2.6)

**Fader vertical double** dans la tranche de droite (curseurs Bas/Haut, 20 Hz – 20 kHz, échelle log) :
seule cette zone excite les résonateurs — hors bande, silence (le dry passe toujours).
20–20k = transparent. Vérifié : 10× plus d'aigus, 2.3× moins de graves en bande haute.

## CRYSTALLIZE (v4.7) — 1 potard, section ARPEGE

Parallèle à la chaîne FX, arpège uniquement : High Shelf 7.5 kHz (+9 dB max),
saturation douce normalisée (drive 1→4), micro-chorus 0.18 Hz (0→2.8 ms).
Loi : discret sous 20 %, audible à 50 %, cristallin à 100 % sans agressivité.
Vérifié : bypass exact à 0, paliers progressifs, stable 44.1/48/96 kHz, mono OK.

## Tranche MIX / MIRAGE / BANDE (v1.7 + v2.4, Mirage x6 depuis v3.8)

Tranche de droite : faders **MIX**, **MIRAGE** (dry/wet), **BANDE** (double curseur) + zone **Mirage** :
**Taille**, **Pre-Del** (0-250 ms), **Couleur** (sombre-brillant), **Largeur** (0-1.5, 0 = mono),
**Decay** (x0.2-x3), **Gel** (sustain infini + étage de gain : nappe tenue sous 0.6, queue audible). Mesuré : RT60 x2 en decay x2, gel sans fin.

## Arpégiateur (v1.1, swing + octaves v3.9, FX v4.0)

Parcourt les notes de l'accord actif et met en avant chaque voix résonante à tour de rôle :
- **Motifs** : Haut, Bas, Haut-Bas (ping-pong), Aléatoire, Accord (toutes voix, pas de cycle)
- **Swing** 0-75 % : pas pairs allongés, impairs raccourcis, cycle constant (pas de dérive)
- **Octaves** 1-3 : l'arpège monte sur N octaves de l'accord
- **FX arp** (menu + 2 potards, libellés dynamiques) : Off, Delay (Temps/Feedback,
  ping-pong croche pointée + douceur passe-bas + shimmer d'octave léger),
  Chorus (Taux/Profondeur), Filtre (Coupure/Résonance), Crusher (Bits/Drive),
  Reverse simple (Temps/Mix, notes arpégées inversées, sans répétitions).
  Ne touche que la session arpégée (nappe intacte, vérifié par A/B).
- **Chaîne FX v4.1 puis v4.2** : 3 slots en série (3 menus empilés, sans noms),
  1 **bouton rouge** à côté : chaque clic passe au slot suivant affiché sur les 2 potards
  (rangée active surlignée), 3e potard **Mix** dry/wet global. Vérifié : Delay->Crusher->Chorus audible.
- **Rythme libre** : Taux Arp 0.5 – 16 Hz (bouton Sync éteint)
- **Sync tempo** : 1/2, 1/4, 1/8, 1/16, 1/8T, 1/16T sur le BPM de l'hôte (120 BPM par défaut en Standalone)
- Les octaves suivent leur classe de hauteur ; les voix non jouées gardent un lit à 15 %
- L'**Attaque** pilote aussi le fondu des pas : pincements → houles

## Mouvement (v1.2)

Modulation d'amplitude continue appliquée à la résonance :
- **Vitesse** : 0.05 – 20 Hz en libre, ou sync tempo (1/2 → 1/16T sur BPM hôte)
- **Force** : 0 = transparent, montez pour creuser la modulation
- **Forme** : 0 = respiration douce et organique (sinus), 1 = rafales rythmiques nettes (carré adouci)
- **Unisson** : potard 0 → 1, de phases réparties (nappe évolutive) à une seule impulsion motrice
- Sync tempo optionnel (1/2 → 1/16T sur BPM hôte)

## Skins — 12 disponibles

Sélecteur **Skin** en haut à gauche (aucun contrôle déplacé). 1 clic = toute
l'interface change immédiatement (fonds, panneaux, potards, LEDs, pastilles...).
Le choix est mémorisé dans le projet (propriété `uiSkin` hors paramètres audio) :
presets, slots A/B, automation et son jamais affectés. DSP intact.

- **Originaux** : Crystal, Cosmic, Monolith, Toxic
- **Ajoutés** : KAY&lt;3, NightCity, Mix, mineprisme, analog2, Post Apo, analog03, analog01

Les textures vivent dans `Assets/Skins/`.

## Spectrogramme + skin MIX (v5.2-MIX)

Bande 0..52 : courbe spectrale remplie (220 colonnes, log 40Hz-20kHz) + arête
lumineuse + couche écho, dégradé vertical aux couleurs du skin. Attaque rapide,
retombée douce. FFT 1024 + Hann, repeint partiel seul.
Skin MIX : graphite/cyan/bleu/violet. Timer UI 33 ms. Audio intact (prise lock-free).

## Enregistreur + panneau SAMPLES

Colonne de droite : **SPECTRUM** en haut, **SAMPLES** en bas.

### Bouton RECORDER (haut de fenêtre)

Ouvre la vue Recorder — même fenêtre, mêmes dimensions, elle recouvre le contenu.
**BACK TO PRISME** revient à l'interface principale. La capture continue même
masquée : l'UI n'est qu'un affichage.

- **REC** : 1er appui = armement, 2e appui = démarrage immédiat
- Sans **START NOW** : le démarrage attend le **début de la mesure suivante**
  (sync hôte ; état moteur `WAITING FOR BAR`)
- **STOP** : arrêt puis écriture du WAV
- Durée bornée : **10 à 600 s**
- États moteur : `IDLE` → `WAITING FOR BAR` → `RECORDING` → `SAVING`

### LOOP STACK

Historique des prises : **4 lignes visibles** (défilement au-delà), miniature
d'onde sur 120 crêtes, boutons `>` (lecture) / `X` (suppression).
**32 prises maximum.** Une prise finie apparaît automatiquement.

### Panneau SAMPLES (interface principale)

- **Barre de transport** : `REC`, `STOP`, `RANDOMIZE`, LED de liaison, `REROLL`
- **Une ligne par prise** : numéro · nom · durée · cellule **VARIATION**
  (`R<seed> I<intensité>%`) · forme d'onde · **PLAY/STOP** · **DEL**
- **PLAY/STOP** : lance ou arrête la pré-écoute **de cette ligne**. La prise en
  lecture affiche `STOP`, les autres `PLAY`. Clic sur une autre ligne : la
  première s'arrête, la nouvelle démarre.
- **DEL** : 1er clic arme (`OK?`), 2e clic supprime l'entrée **et le fichier**
- **Glisser une ligne vers le DAW** : exporte la prise. Le glisser s'arrête
  avant les colonnes PLAY/STOP et DEL, pour que ces boutons restent des clics
  purs. L'original n'est jamais déplacé.
- Pré-écoute **additive** (post-DSP), plafonnée à **60 s**, impossible pendant
  une prise en cours.

### RANDOMIZE / REROLL — variation

Produisent une **variation** de la prise sélectionnée : rendu hors ligne sur un
thread de travail, puis insérée sous sa source.

- Depuis le **panneau SAMPLES** : 8 mesures, intensité 60 %, seed aléatoire.
  Les deux boutons y produisent le même résultat.
- Depuis **VAR LAB** (vue Recorder) : mesures **4 ou 8** au choix, seed éditable
  (réjeu exact si vous tapez une valeur).
- **Seed `0`** = auto : tirée et enregistrée, donc reproductible.
- **LED de liaison** (panneau SAMPLES) : allumée → chaque clic met un effet
  **vedette** en avant et l'effet avance d'un cran ; éteinte → tirage au hasard.
  Cet effet vedette ne s'applique que depuis ce panneau, jamais depuis VAR LAB.

## Fenêtre redimensionnable (v5.1)

Tirez les bords ou le coin bas-droit : le contenu est mis à l'échelle
uniformément (880×970 logique). Potards ronds, textes nets, rien ne sort.
Min 440×485, max 1760×1940, ratio verrouillé (centré sinon). Design et son inchangés.

## Pastilles de gamme (v1.8, style piano depuis v2.1)

Dièses en haut (C# D# F# G# A#), naturelles en bas (C D E F G A B), corail selon la gamme + tonique (tonique = anneau blanc).
**Clic sur une pastille = nouvelle tonique**, les résonateurs se réaccordent. Suivi auto via timer (presets, combos, automation).

## Presets : menu + sauvegarde (v2.2 boutons lumineux, v2.3 menu)

Menu déroulant en bas : 4 presets usine (Verre Clair, Aurore, Cathédrale, Fracture)
puis section **User preset**. Pour sauver : tape un nom dans le champ, **Sauver**
(ou Entrée) — le preset apparaît dans le menu. **X** supprime le user preset
sélectionné. Fichiers `.prisme` dans `%APPDATA%/Atelier Prisme/Prisme/UserPresets`.
Tout réglage manuel fait revenir le menu sur "Choisir un preset...".

- **Verre Clair** : C Major, decay court, brillant — touche délicate
- **Aurore** : F Penta Majeur, nappe évolutive
- **Cathédrale** : G Minor, decay 6.5 s — espace immense
- **Fracture** : D Chromatique, extrême, brillant + shimmer max

## Installation

**Le VST3 est déjà installé** : `C:\Program Files\Common Files\VST3\Prisme.vst3`
(le build l'installe automatiquement, voir « Build développeur » ci-dessous).

### Installeur Windows — `Installer/Prisme.iss` (Inno Setup 6)

- VST3 → `C:\Program Files\Common Files\VST3\Prisme.vst3` (obligatoire)
- Standalone + Lisez-moi → `Program Files\Atelier Prisme\Prisme` (optionnel)
- Désinstalleur inclus

> ⚠️ **`Release/Prisme-Setup-v3.2.exe` est périmé** (13/10) : il installe
> l'ancien VST3 et **écraserait les corrections récentes**. Régénérez-le avec
> Inno Setup 6 depuis `Installer/Prisme.iss` avant de l'utiliser.

## Build développeur

Ce dépôt est la **source de référence** du plugin installé.

> ⚠️ Ne compilez **jamais** depuis un chemin OneDrive : l'accent de
> `Projet par défaut` fait crasher `juceaide`. Ce dépôt est sur le Bureau,
> hors OneDrive — c'est voulu.

```powershell
cd C:\Users\Utilisateur\Desktop\prisme\PrismeMix
cmake --build build_postapo --config Release
```

La compilation **installe automatiquement** le VST3 :
`-- Installing: C:\Program Files\Common Files\VST3\Prisme.vst3`

Sortie :
- `build_postapo\Prisme_artefacts\Release\VST3\Prisme.vst3` → VST3 installé
- `build_postapo\Prisme_artefacts\Release\Standalone\Prisme.exe` → autonome, pour tester sans DAW

Premier configure, si le répertoire de build n'existe pas encore :
```powershell
cmake -S . -B build_postapo -G "Visual Studio 17 2022" -A x64
```

JUCE 8.0.6 est récupéré automatiquement via FetchContent (connexion internet requise au premier configure).

> Les dossiers `build/` et `build_postapo/` (~1 Go) sont **exclus de git**
> par `.gitignore` : ils se régénèrent, jamais à committer.

## Test rapide sans DAW

1. Lancez `Prisme.exe` (Standalone)
2. Sélectionnez votre interface audio / casque
3. Parlez, jouez un sample de batterie, ou passez un bruit blanc
4. Montez `Mix` et `Iridescence`, changez de gamme

## Idées d'utilisation

- Batterie → nappes mélodiques accordées au morceau
- Voix parlée → chœur spectral
- Bruit / field recording → texture vivante tonale
- Avec `Givre` : figez un transient en drone, puis changez de gamme en live

---
Atelier Prisme — JUCE 8 / VST3 / Standalone — source suivie sous git
