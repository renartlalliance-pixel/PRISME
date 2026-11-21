import re

p = 'Source/PluginEditor.cpp'
lines = open(p, encoding='utf-8').read().split('\n')
n = 0
for i, l in enumerate(lines):
    m = re.match(r'^(\s+)addAndMakeVisible\((.*)$', l)
    if m and not m.group(2).startswith('content)'):
        lines[i] = m.group(1) + 'content.addAndMakeVisible(' + m.group(2)
        n += 1
open(p, 'w', encoding='utf-8').write('\n'.join(lines))
print('rediriges:', n)
