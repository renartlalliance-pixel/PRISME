import re
s = open('Source/PluginEditor.cpp', encoding='utf-8').read()
h = open('Source/PluginEditor.h', encoding='utf-8').read()

# 1. Tous les enfants vont dans content (sauf content + cornerResizer eux-memes)
direct = [l.strip() for l in s.split('\n')
          if re.match(r'^\s+addAndMakeVisible\(', l)]
assert direct == ['addAndMakeVisible(content);', 'addAndMakeVisible(cornerResizer);'], direct
n_content = s.count('content.addAndMakeVisible(')
assert n_content >= 40, n_content
assert 'addAndMakeVisible(cornerResizer)' in s
print('reparentage :', n_content, 'enfants dans content OK')

# 2. resized() : echelle uniforme + centrage + coin
body = s.split('void PrismeEditor::resized()')[1].split('\n}\n')[0]
for t in ['designW = 880, designH = 970', 'AffineTransform::scale(s)',
          'content.setBounds(ox, oy, designW, designH)',
          'cornerResizer.setBounds(getWidth() - 18, getHeight() - 18, 16, 16)',
          'jlimit(0.25f, 4.0f, s)']:
    assert t in body, t
print('resized() : echelle + centrage + coin OK')

# 3. Constrainer : min 440x485, max 1760x1940, ratio 880/970
for t in ['constrainer.setSizeLimits(440, 485, 1760, 1940)',
          'constrainer.setFixedAspectRatio(880.0 / 970.0)',
          'setSize(880, 970);']:
    assert t in s, t
print('constrainer + taille initiale OK')

# 4. paintContent existe, paint() editeur = fond seul
assert 'void PrismeEditor::paintContent(juce::Graphics& g)' in s
paint_body = s.split('void PrismeEditor::paint(juce::Graphics& g)')[1].split('void PrismeEditor::paintContent')[0]
assert 'fillAll(activeSkin.windowBg)' in paint_body
assert 'fillRoundedRectangle' not in paint_body
print('separation paint / paintContent OK')

# 5. Init list : content + corner branches
assert 'content(*this), cornerResizer(this, &constrainer)' in s
assert 'PrismeContent content;' in h or 'PrismeContent content' in h
print('membres + init OK')
print('RESIZE VERIFIE')
