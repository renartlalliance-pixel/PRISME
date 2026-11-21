s = open('Source/PluginProcessor.cpp', encoding='utf-8').read()
h = open('Source/PluginProcessor.h', encoding='utf-8').read()
e = open('Source/PluginEditor.cpp', encoding='utf-8').read()
eh = open('Source/PluginEditor.h', encoding='utf-8').read()

assert '"arpswing"' in s and '"arpoctaves"' in s
assert 'arpTick' in h
assert 'tones * octaves' in s
assert '(arpTick % 2) == 0' in s
for t in ['arpOctBox', 'arpOctAtt', 'swingSlider', 'swingAtt', 'swingLabel']:
    assert t in e and t in eh, t
assert 'arpOctBox.setBounds(270, 462, 66, 26)' in e
assert 'swingSlider.setBounds(568, 462, 68, 26)' in e
print('SWING + OCTAVES VERIFIES')
