import re

p = 'Tools/RenderDemo.cpp'
s = open(p, encoding='utf-8').read()

# Apres arpRate (float), inserer arpOct=1, arpSwing=0.0 SAUF lignes deja OK (t, b1)
fixes = [
    ('true, 0, 6.0f, 0.8f,', 'true, 0, 6.0f, 1, 0.0f, 0.8f,'),
    ('false, 0, 4.0f, 0.8f,', 'false, 0, 4.0f, 1, 0.0f, 0.8f,'),
    ('false, 0, 4.0f, 0.5f,', 'false, 0, 4.0f, 1, 0.0f, 0.5f,'),
    ('false, 0, 4.0f, 3.0f,', 'false, 0, 4.0f, 1, 0.0f, 3.0f,'),
]
total = 0
for a, b in fixes:
    n = s.count(a)
    s = s.replace(a, b)
    total += n
    print(a, '->', n, 'fois')
open(p, 'w', encoding='utf-8').write(s)
print('total:', total)
