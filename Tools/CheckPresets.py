src = open('Source/PluginEditor.cpp', encoding='utf-8').read()
src_h = open('Source/PluginEditor.h', encoding='utf-8').read()

checks = [
    ('applyPreset(int index', src),
    ('applyPreset(0, 0, 0, 1,', src),
    ('applyPreset(1, 5, 6, 1,', src),
    ('applyPreset(2, 7, 1, 0,', src),
    ('applyPreset(3, 2, 20, 1,', src),
    ('applyingPreset = true', src),
    ('applyingPreset = false', src),
    ('setActivePreset(index)', src),
    ('0xffff7a6b', src),  # corail actif
    ('triggerAsyncUpdate()', src),
    ('handleAsyncUpdate()', src),
    ('activePreset = -1', src),
    ('apvts.state.addListener(this)', src),
    ('apvts.state.removeListener(this)', src),
    ('cancelPendingUpdate()', src),
    ('stopTimer()', src),
    ('ValueTree::Listener', src_h),
    ('AsyncUpdater', src_h),
    ('activePreset = -1', src_h),
    ('applyingPreset', src_h),
]
for token, where in checks:
    assert token in where, token
print('PRESETS ACTIFS VERIFIES :', len(checks), 'points OK')
