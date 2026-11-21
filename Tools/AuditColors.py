import re, glob

sites = []
for p in glob.glob('Source/Plugin*.cpp') + glob.glob('Source/PrismeLookAndFeel.h') + glob.glob('Source/ScalePads.h'):
    src = open(p, encoding='utf-8').read()
    colors = set(re.findall(r'0xff[0-9a-fA-F]{6}', src))
    sites.append((p, len(colors)))
    print(p, '-', len(colors), 'couleurs distinctes')
