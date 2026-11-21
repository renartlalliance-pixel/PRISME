src = open('Source/PluginEditor.cpp', encoding='utf-8').read()
src_h = open('Source/PluginEditor.h', encoding='utf-8').read()
W, H = 880, 910

def rect(name, x, y, w, h):
    assert x >= 0 and y >= 0 and x + w <= W and y + h <= H, (name, x, y, w, h)

# Panneaux gauche revenus a 628 + tranche elargie 212
for t in ['fillRoundedRectangle(16, 52, 628, 366', 'fillRoundedRectangle(16, 428, 628, 110',
          'fillRoundedRectangle(16, 548, 628, 122', 'fillRoundedRectangle(16, 680, 628, 150',
          'fillRoundedRectangle(652, 52, 212, 798']:
    assert t in src, t
print('panneaux OK')

# Potards NON agrandis (108/96 comme avant)
assert 'int knobW = 108, knobH = 96;' in src
assert 'int movKnobW = 96, movKnobH = 72;' in src
print('potards taille origine OK')

# Anciens potards de bande supprimes
assert 'bandLowSlider' not in src and 'bandHighSlider' not in src
assert 'bandLowSlider' not in src_h
print('anciens potards bande supprimes OK')

# Fader 3 valeurs + synchro
for t in ['ThreeValueVertical', 'setSkewFactorFromMidPoint(800.0)',
          'setMinAndMaxValues', 'sliderValueChanged',
          'bandFader.setBounds(798, 96, 58, 600)',
          'bandTitleLabel.setBounds(792, 58, 70, 20)',
          'bandHiLabel.setBounds(792, 80, 70, 14)',
          'bandLoLabel.setBounds(792, 698, 70, 14)',
          'setSize(880, 910)']:
    assert t in src, t
rect('bandFader', 798, 96, 58, 600)
print('fader de bande OK')
print('LAYOUT v2.6 VERIFIE : 880x910, fader double, potards intacts')
