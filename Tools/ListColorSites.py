import re
src = open('Source/PluginEditor.cpp', encoding='utf-8').read()
sites = {}
for m in re.finditer(r'(\w+)\.setColour\(juce::(\w+)::(\w+), ([^)]+)\)', src):
    key = (m.group(1), m.group(2) + '::' + m.group(3))
    sites.setdefault(key, 0)
    sites[key] += 1
for k in sorted(sites):
    print(k, sites[k])
