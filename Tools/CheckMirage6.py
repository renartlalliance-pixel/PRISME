s = open('Source/PluginEditor.cpp', encoding='utf-8').read()
h = open('Source/PluginEditor.h', encoding='utf-8').read()
p = open('Source/PluginProcessor.cpp', encoding='utf-8').read()

# 5 nouveaux controles lies aux 5 nouveaux parametres
pairs = [('mirPredelaySlider', 'mirpredelay'), ('mirColorSlider', 'mircolor'),
         ('mirWidthSlider', 'mirwidth'), ('mirDecaySlider', 'mirdecay'),
         ('mirFreezeBtn', 'mirfreeze')]
for slider, param in pairs:
    assert slider in s and slider in h, slider
    assert '"' + param + '"' in s and '"' + param + '"' in p, param
print('5 reglages Mirage lies OK')

# Positions dans la tranche (652..864) et panneau (52..850)
for b in [(652, 640, 64, 64), (726, 640, 64, 64), (800, 640, 64, 64),
          (652, 726, 64, 64), (726, 726, 64, 64), (800, 744, 64, 24)]:
    x, y, w, hh = b
    assert x >= 652 and x + w <= 864 and y >= 52 and y + hh <= 850, b
print('positions tranche OK')
print('MIRAGE x6 VERIFIE')
