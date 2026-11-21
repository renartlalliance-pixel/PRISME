src = open('Source/PluginEditor.cpp', encoding='utf-8').read()
src_h = open('Source/PluginEditor.h', encoding='utf-8').read()

checks = [
    'presetBox.onChange', 'addSectionHeading("User preset")',
    'getUserPresetDir', 'userApplicationDataDirectory',
    '"*.prisme"', 'replaceState(loaded)',
    'replaceWithText', 'presetNameEditor.onReturnKey' if 'onReturnKey' in src else 'presetNameEditor',
    'deleteSelectedUserPreset', 'deleteFile()',
    'setTextWhenNothingSelected',
    'presetVerre' not in src and 'presetAurore' not in src,
]
for c in checks:
    assert c if isinstance(c, bool) else c in src, c
for token in ['presetBox', 'presetNameEditor', 'presetSaveBtn', 'presetDeleteBtn',
              'refreshPresetList', 'saveUserPreset', 'onPresetChosen']:
    assert token in src_h, token
assert 'presetVerre' not in src_h and 'refreshPresetColours' not in src_h
print('MENU PRESETS + SAVE VERIFIES :', len(checks) + 7, 'points OK')
