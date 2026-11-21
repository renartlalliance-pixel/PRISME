s = open('Source/PluginEditor.cpp', encoding='utf-8').read()
h = open('Source/PluginEditor.h', encoding='utf-8').read()
body = s.split('void PrismeEditor::randomizeSound()')[1].split('void PrismeEditor::')[0]
assert 'randomBtn' in h and 'randomizeSound();' in h
assert 'randomBtn.setBounds(553, topY, 83, 26)' in s
for v in ['"mix"', '"ingain"', '"outgain"']:
    assert v not in body, v
print('Randomize OK : bouton place, volumes intacts')
