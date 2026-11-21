s = open('Source/PluginEditor.cpp', encoding='utf-8').read()
h = open('Source/PluginEditor.h', encoding='utf-8').read()
p = open('Source/PluginProcessor.cpp', encoding='utf-8').read()
ph = open('Source/PluginProcessor.h', encoding='utf-8').read()

# Parametre hote stable + valeurs par defaut
assert '"crystallize"' in p
assert 'NormalisableRange<float>(0.0f, 1.0f' in p
assert 'smoothCryo' in ph
assert 'Crystallize cryo;' in ph
print('parametre + etat OK')

# UI : potard 56 px rangee FX, attache, label, presets, randomize
for t in ['cryoKnob', 'cryoLabel', 'cryoAtt', '"crystallize"']:
    assert t in s or t in h, t
assert 'cryoKnob.setBounds(268, 492, 56, 56)' in s
assert 'setParam("crystallize", 0.0f)' in s
assert 'setParam("crystallize", 0.15f' in s
print('UI + presets + randomize OK')
print('CABLAGE CRYSTALLIZE VERIFIE')
