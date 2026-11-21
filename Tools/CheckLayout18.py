import re

src = open('Source/PluginEditor.cpp', encoding='utf-8').read()
W, H = 800, 1000

def rect(name, x, y, w, h):
    assert x >= 0 and y >= 0 and x + w <= W and y + h <= H, (name, x, y, w, h)

# Panneaux (doivent matcher paint())
panels = {'accords': (16, 86, 628, 422), 'arp': (16, 518, 628, 110),
          'mouvement': (16, 638, 628, 122), 'eq': (16, 770, 628, 150),
          'strip': (652, 86, 132, 834)}
for name, (x, y, w, h) in panels.items():
    assert f'fillRoundedRectangle({x}, {y}, {w}, {h}' in src, name
    rect('panel-' + name, x, y, w, h)

def inside(b, outer):
    x, y, w, h = b
    ox, oy, ow, oh = panels[outer]
    assert x >= ox and y >= oy and x + w <= ox + ow and y + h <= oy + oh, (b, outer)

# Pastilles : 12 x d28 + 11 x gap10 = 446 <= 470, dans accords
assert 'pads.setGeometry(95, 130, 470, 48, 28)' in src
inside((95, 130, 470, 48), 'accords')

# Knobs accords : startY 182, 2 rangees
for i in range(9):
    r, c = i // 5, i % 5
    off = 58 if r == 1 else 0
    inside((30 + c * 116 + off, 182 + r * 124, 108, 112), 'accords')
# Knobs mouvement : y 666
for i in range(4):
    inside((30 + i * 104, 666, 96, 86), 'mouvement')
# EQ : y 798
for i in range(4):
    inside((160 + i * 90, 798, 70, 112), 'eq')
# Fader dans strip
for b in [(652, 92, 132, 20), (652, 114, 132, 16), (675, 132, 86, 640),
          (652, 774, 132, 16), (652, 794, 132, 22)]:
    inside(b, 'strip')
# Presets + footer
for b in [(48, 934, 130, 24), (186, 934, 130, 24), (324, 934, 130, 24),
          (462, 934, 130, 24), (0, 970, 800, 20)]:
    rect('preset/footer', *b)
# setSize final + timer + callback clic
assert 'setSize(800, 1000)' in src
assert 'pads.onPadClicked' in src and 'startTimer(120)' in src
assert 'stopTimer()' in src and 'timerCallback()' in src
print('LAYOUT v1.8 VERIFIE : 800x1000, pastilles + tout tient')
