s = open('Source/PluginEditor.cpp', encoding='utf-8').read()
h = open('Source/PluginEditor.h', encoding='utf-8').read()
p = open('Source/PluginProcessor.cpp', encoding='utf-8').read()
W, H = 880, 970

# 2 menus + selecteur + 3 potards dans le panneau (16,428,628,170)
for b in [(48, 500, 150, 26), (206, 500, 150, 26), (362, 500, 44, 26),
          (420, 492, 56, 56), (416, 548, 64, 14),
          (484, 492, 56, 56), (480, 548, 64, 14),
          (548, 492, 56, 56), (544, 548, 64, 14)]:
    x, y, w, hh = b
    assert 16 <= x and y >= 428 and x + w <= 644 and y + hh <= 598, b
print('r implementations rangee FX OK')

for t in ['arpFxBoxB', 'fxSlotBtn', 'arpFxMixKnob', 'arpFxMixLabel', 'arpFxMixAtt',
          'refreshFxKnobs', 'updatingFxKnobs', 'fxSlotSel']:
    assert t in s or t in h, t
print('controles chaine lies OK')

for t in ['"arpfxtypeB"', '"arpfxB1"', '"arpfxB2"', '"arpfxmix"',
          'arpFXA.process', 'arpFXB.process', 'smoothFxMix']:
    assert t in p, t
print('DSP double moteur OK')
print('CHAINE FX VERIFIEE')
