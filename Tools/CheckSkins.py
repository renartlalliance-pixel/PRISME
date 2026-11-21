import re

ed = open('Source/PluginEditor.cpp', encoding='utf-8').read()
eh = open('Source/PluginEditor.h', encoding='utf-8').read()
laf = open('Source/PrismeLookAndFeel.h', encoding='utf-8').read()
sk = open('Source/PrismeSkin.h', encoding='utf-8').read()
pads = open('Source/ScalePads.h', encoding='utf-8').read()

# 1. 4 skins aux accents distincts
for name in ['crystal', 'cosmic', 'monolith', 'toxic']:
    assert name + '()' in sk, name
accents = re.findall(r's\.accent = juce::Colour\((0xff[0-9a-f]{6})\)', sk)
assert len(accents) == 4 and len(set(accents)) == 4, accents
print('4 skins distincts OK')

# 2. LAF : plus d'accent ambre code en dur.
# Restent acceptes : noir pur (contours neutres), blanc/transparent (reflets).
for old in ['0xffffb648', '0xffffcf7d', '0xff9a5f16', '0xff4a4e56', '0xff545963',
            '0xffc9cdd6', '0xff1a1b1e', '0xff0b0c0e', '0xff0a0b0d', '0xff484c55',
            '0xffc8ccd4', '0xff6f747e', '0xff4a3f77', '0xff3f7a8c']:
    assert old not in laf, old
assert 'setSkin' in laf and 'PrismeSkin skin' in laf
print('LAF 100% skin OK')

# 3. Editeur : anciens fonds/bordures de panneaux partis.
# Restent acceptes : 2 valeurs de creation (ecrasees par applySkin avant affichage).
ed2 = ed.replace('presetNameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2a2e4d));', '')
ed2 = ed2.replace('presetSaveBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2f5d4c));', '')
for old in ['0xff141624', '0xff2a2e4d', '0xff3a3f66', '0xff2f5d4c', '0xff0a0b14',
            '0xff2e3138', '0xff1e2024', '0xff5a5f68', '0xfff2f4f8']:
    assert old not in ed2, old
print('fonds panneaux centralises OK')

# 4. applySkin couvre chaque composant nomme de l'inventaire
body = ed.split('void PrismeEditor::applySkin')[1].split('\n}\n')[0]
targets = ['rootBox', 'scaleBox', 'octaveBox', 'arpPatternBox', 'arpDivBox', 'arpOctBox',
           'arpFxBox', 'arpFxBoxB', 'arpFxBoxC', 'movDivBox', 'presetBox', 'skinBox',
           'arpTitleLabel', 'movTitleLabel', 'eqTitleLabel', 'mixTitleLabel', 'blurTitleLabel',
           'bandTitleLabel', 'titleLabel', 'footerLabel', 'skinLabel',
           'wetLabel', 'dryLabel', 'blurWetLabel', 'blurDryLabel', 'bandHiLabel', 'bandLoLabel',
           'givreLabel', 'swingLabel', 'tailleLabel', 'mirPredelayLabel', 'mirColorLabel',
           'mirWidthLabel', 'mirDecayLabel', 'arpFxLabel1', 'arpFxLabel2', 'arpFxMixLabel', 'cryoLabel',
           'mixFader', 'blurFader', 'bandFader', 'givreSlider', 'swingSlider',
           'presetNameEditor', 'presetSaveBtn', 'presetDeleteBtn', 'randomBtn',
           'arpOnBtn', 'arpSyncBtn', 'movSyncBtn', 'pads.setColors', 'repaint()']
for t in targets:
    assert t in body, t
print('applySkin couvre', len(targets), 'cibles OK')

# 5. Persistance hors params audio + garde-fous
assert 'apvts.state.setProperty("uiSkin"' in ed
assert 'getProperty("uiSkin", 0)' in ed
assert 'if (property.toString() == "uiSkin")' in ed
assert ed.count('setProperty("uiSkin", sk, nullptr)') >= 2  # slots + presets
print('persistance + gardes OK')
print('SKINS VERIFIES')
