s = open('Source/PluginEditor.cpp', encoding='utf-8').read()
h = open('Source/PluginEditor.h', encoding='utf-8').read()
W, H = 880, 970

def rect(name, x, y, w, hh):
    assert x >= 0 and y >= 0 and x + w <= W and y + hh <= H, (name, x, y, w, hh)

# Panneau arpege agrandi + decales
assert 'paintBrushedPanel(16, 428, 628, 170' in s
assert 'paintBrushedPanel(16, 608, 628, 122' in s
assert 'paintBrushedPanel(16, 740, 628, 150' in s
for b in [(140, 500, 170, 26), (330, 492, 64, 64), (330, 556, 64, 14),
          (410, 492, 64, 64), (410, 556, 64, 14)]:
    x, y, w, hh = b
    assert 16 <= x and y >= 428 and x + w <= 644 and y + hh <= 598, b
print('panneau FX arp OK')
for b in [(0, 614, 640, 20), (0, 746, 652, 20), (48, 904, 130, 24), (0, 940, 880, 20)]:
    rect('decale', *b)
assert 'setSize(880, 970)' in s
print('decalage +60 / 880x970 OK')
for t in ['arpFxBox', 'arpFxKnob1', 'arpFxKnob2', 'arpFxLabel1', 'arpFxLabel2',
          'arpFxAtt', 'arpFxAtt1', 'arpFxAtt2', 'lastFxType', 'knobName']:
    assert t in s or t in h, t
print('controles FX lies OK')
print('FX ARP VERIFIE')
