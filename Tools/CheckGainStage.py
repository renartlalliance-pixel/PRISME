s = open('Source/PluginProcessor.cpp', encoding='utf-8').read()
h = open('Source/PluginEditor.h', encoding='utf-8').read()
e = open('Source/PluginEditor.cpp', encoding='utf-8').read()
b = open('Source/BlurReverb.h', encoding='utf-8').read()

for token in ['mirfreeze', 'mirFreeze', 'setFreeze', 'frozen']:
    assert token not in s and token not in h and token not in e and token not in b, token
print('Gel supprime partout OK')

# Sortie = tanh PUIS gain (le potard definit le niveau)
i_tanh = s.find('yL = std::tanh(yL);')
i_gain = s.find('yL *= og;', i_tanh)
assert 0 < i_tanh < i_gain, 'ordre tanh->gain'
print('Etage Sortie (tanh puis gain) OK')
print('GAIN STAGING VERIFIE')
