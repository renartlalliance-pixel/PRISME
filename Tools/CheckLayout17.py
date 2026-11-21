import re, sys

src = open('Source/PluginEditor.cpp', encoding='utf-8').read()
src_h = open('Source/PluginEditor.h', encoding='utf-8').read()

# 1. setSize final 800x960 unique
ms = [m.start() for m in re.finditer(r'setSize\(800, 960\)', src)]
print('setSize(800,960):', len(ms), 'occurrence(s)')
assert len(ms) == 1

# 2. addKnob : 13 au total, plus de Mix
knobs = re.findall(r'addKnob\("([^"]+)"', src)
print('knobs:', len(knobs), knobs)
assert len(knobs) == 13 and 'mix' not in knobs

# 3. fader dans la tranche (652,86,132,792)
for token in ['mixFader.setBounds(675, 132, 86, 640)',
              'mixTitleLabel.setBounds(652, 92, 132, 20)',
              'wetLabel.setBounds(652, 114, 132, 16)',
              'dryLabel.setBounds(652, 774, 132, 16)',
              'SliderAttachment>(apvts, "mix", mixFader)']:
    assert token in src, token
print('fader tranche OK')

# 4. boucle mouvement sur indices 9..12
assert 'size_t i = 9; i < knobs.size() && i < 13' in src
assert '(i - 9)' in src
print('indices mouvement OK')

# 5. header : membres fader + eq + mouvement intacts
for token in ['mixFader', 'mixTitleLabel', 'wetLabel', 'dryLabel', 'mixAtt',
              'eqSliders[4]', 'eqAtts[4]', 'movSyncBtn', 'movDivAtt']:
    assert token in src_h, token
print('header OK')

# 6. EQ en bas : panneau (16,728,628,150), sliders y=756
assert 'fillRoundedRectangle(16, 728, 628, 150' in src
assert 'eqSliders[i].setBounds(x, 756, eqW, eqH)' in src
print('EQ bas OK')
print('LAYOUT v1.7 VERIFIE par Python')
