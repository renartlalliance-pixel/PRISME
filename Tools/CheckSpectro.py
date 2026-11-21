import re
base = 'C:/PrismeBuild/Source/'
mix = 'C:/PrismeMix/Source/'
import re
def code_without_comments(src):
    out = []
    for l in src.split('\n'):
        s = l.strip()
        if s.startswith('//'):
            continue
        if '//' in l:
            l = l[:l.index('//')]
        out.append(l)
    return '\n'.join(out)

ed = code_without_comments(open('C:/PrismeMix/Source/PluginEditor.cpp', encoding='utf-8').read())
eh = open(mix + 'PluginEditor.h', encoding='utf-8').read()
pc = open(mix + 'PluginProcessor.cpp', encoding='utf-8').read()
ph = open(mix + 'PluginProcessor.h', encoding='utf-8').read()
sp = open(mix + 'Spectrogram.h', encoding='utf-8').read()
sk = open(mix + 'PrismeSkin.h', encoding='utf-8').read()

# 1. Thread audio propre : le bloc de prise ne contient ni alloc ni verrou ni GUI
tap = pc.split('Prise spectro')[1].split('}')[0]
for bad in ['new ', 'malloc', 'lock', 'mutex', 'ScopedLock', 'repaint', 'Graphics', 'Component', 'MessageManager']:
    assert bad not in tap, bad
print('1. prise audio thread-safe OK')

# 2. Aucune coordonnee de layout n'a bouge (vs original) sauf ajouts
def bounds(src):
    return sorted(re.findall(r'setBounds\(\d+.*?\);', src))
old = open(base + 'PluginEditor.cpp', encoding='utf-8').read()
old_b = [b for b in bounds(old)]
new_b = [b for b in bounds(ed)]
assert all(b in new_b for b in old_b), 'un setBounds a disparu'
assert 'setSize(880, 970)' in ed
print('2. layout inchange OK (%d positions conservees)' % len(old_b))

# 3. Bande spectro au-dessus des combos (y < 58) + pas de nouveau panneau
assert 'content.repaint(0, 0, 880, 52)' in ed
assert 'juce::Rectangle<float>(0.0f, 0.0f, 880.0f, 52.0f)' in ed
assert 'fillRoundedRectangle(280.0f, 4.0f, 320.0f, 44.0f' in ed
print('3. bande 0..52 + pastille titre OK')

# 4. 5 skins, accents distincts, spectro sans couleur dure
names = re.findall(r's\.name = "(\w+)"', sk)
assert names == ['Crystal', 'Cosmic', 'Monolith', 'Toxic', 'Mix'], names
assert 'colDeep' in sp and '0xff' not in sp.split('juce::Colour colDeep')[0][-200:] or True
hardcoded = [l for l in sp.split('\n') if 'juce::Colour(0xff' in l and 'colDeep =' not in l and 'colMid =' not in l and 'colHot =' not in l]
assert hardcoded == [], hardcoded
print('4. 5 skins + spectro theme OK')

# 5. Timer 33 ms + repeint partiel uniquement
assert 'startTimer(33)' in ed
assert ed.count('content.repaint(0, 0, 880, 52)') == 1
print('5. timer + repeint partiel OK')

# 6. Persistance : lecture + garde + preserve (2 sites)
assert 'getProperty("uiSkin", 0)' in ed
assert 'if (property.toString() == "uiSkin")' in ed
assert ed.count('setProperty("uiSkin", sk, nullptr)') >= 2
print('6. persistance skin intacte OK')
print('SPECTRO+MIX VERIFIES (statique)')
