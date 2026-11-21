s = open('Source/PluginEditor.cpp', encoding='utf-8').read()
h = open('Source/PluginEditor.h', encoding='utf-8').read()
l = open('Source/PrismeLookAndFeel.h', encoding='utf-8').read()
for t in ['slotABtn', 'slotBBtn', 'slotCopyBtn', 'selectSlot', 'refreshSlotColours',
          'copySlotToOther', 'slotA = apvts.copyState()', 'replaceState((slot == 0 ? slotA : slotB)']:
    assert t in s, t
for t in ['slotABtn', 'slotBBtn', 'slotCopyBtn', 'slotA, slotB', 'activeSlot', 'selectSlot(']:
    assert t in h, t
assert '0xffff7a6b' in s  # corail emplacement actif
assert 'base.brighter(0.10f)' in l  # LAF respecte la teinte du bouton
print('A/B VERIFIE : 13 points OK')
