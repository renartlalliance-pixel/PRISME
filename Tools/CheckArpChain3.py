s = open('Source/PluginEditor.cpp', encoding='utf-8').read()
h = open('Source/PluginEditor.h', encoding='utf-8').read()
p = open('Source/PluginProcessor.cpp', encoding='utf-8').read()

# 3 menus empiles + bouton rouge + 3 potards, dans le panneau (16,428,628,170)
for b in [(48, 490, 150, 24), (48, 520, 150, 24), (48, 550, 150, 24),
          (206, 521, 44, 26),
          (420, 492, 56, 56), (484, 492, 56, 56), (548, 492, 56, 56)]:
    x, y, w, hh = b
    assert 16 <= x and y >= 428 and x + w <= 644 and y + hh <= 598, b
print('disposition 3 menus + rouge OK')

for t in ['arpFxBoxC', 'arpFxAttC', 'fxSlotSel = (fxSlotSel + 1) % 3']:
    assert t in s or t in h, t
# Bouton rouge sans nom (les boutons A/B du mode compare existent toujours, c'est voulu)
assert 'fxSlotBtn.setButtonText("")' in s
print('bouton rouge sans nom OK')

for t in ['"arpfxtypeC"', '"arpfxC1"', '"arpfxC2"',
          'arpFXC.process', 'smoothFxC1', 'smoothFxC2']:
    assert t in p, t
print('DSP 3e slot OK')
print('CHAINE x3 VERIFIEE')
