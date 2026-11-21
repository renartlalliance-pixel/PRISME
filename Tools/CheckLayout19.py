src = open('Source/PluginEditor.cpp', encoding='utf-8').read()
W, H = 800, 910

def rect(name, x, y, w, h):
    assert x >= 0 and y >= 0 and x + w <= W and y + h <= H, (name, x, y, w, h)

panels = {'accords': (16, 52, 628, 366), 'arp': (16, 428, 628, 110),
          'mouvement': (16, 548, 628, 122), 'eq': (16, 680, 628, 150),
          'strip': (652, 52, 132, 778)}
for name, (x, y, w, h) in panels.items():
    assert f'fillRoundedRectangle({x}, {y}, {w}, {h}' in src, name
    rect('panel-' + name, x, y, w, h)

def inside(b, outer):
    x, y, w, h = b
    ox, oy, ow, oh = panels[outer]
    assert x >= ox and y >= oy and x + w <= ox + ow and y + h <= oy + oh, (b, outer)

assert 'subLabel' not in src, 'sous-titre encore present'
inside((221, 96, 238, 52), 'accords')  # pastilles piano : dieses haut
for i in range(9):  # knobs accords des 168
    r, c = i // 5, i % 5
    off = 58 if r == 1 else 0
    inside((30 + c * 116 + off, 168 + r * 124, 108, 112), 'accords')
for i in range(4):
    inside((30 + i * 104, 576, 96, 86), 'mouvement')
for i in range(4):
    inside((160 + i * 90, 708, 70, 112), 'eq')
for b in [(652, 58, 132, 20), (652, 80, 132, 16), (675, 98, 86, 640),
          (652, 740, 132, 16), (652, 760, 132, 22)]:
    inside(b, 'strip')
for b in [(48, 844, 130, 24), (186, 844, 130, 24), (324, 844, 130, 24),
          (462, 844, 130, 24), (0, 880, 800, 20)]:
    rect('preset/footer', *b)
assert 'setSize(800, 910)' in src
assert 'pads.setGeometry(221, 96, 238, 52, 24)' in src
print('LAYOUT v2.1 VERIFIE : dieses en haut, naturelles en bas')
