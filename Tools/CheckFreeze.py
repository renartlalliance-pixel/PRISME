src = open('Source/PluginProcessor.cpp', encoding='utf-8').read()
src_h = open('Source/PluginProcessor.h', encoding='utf-8').read()
ed = open('Source/PluginEditor.cpp', encoding='utf-8').read()
ed_h = open('Source/PluginEditor.h', encoding='utf-8').read()

# Les deux banques suivent les memes reglages
for meth in ['setTuning(notes)', 'setBrightness(bright)', 'setShimmer(shim)',
             'setSpread(spread)', 'setEmphSmooth(emphSmooth)',
             'setMovement(movHz, movDepth, movShape, movUnison)',
             'setVoiceTargets(arpTargets, 12)', 'setSampleRate(sampleRate)',
             'resetEmphasis()', 'reset()']:
    assert ('resonator.' + meth in src) and ('freezeBank.' + meth in src), meth
print('double banque synchronisee OK')

assert '"freezemix"' in src and '"freeze"' not in src
assert 'smoothFreeze' in src_h and 'freezeWasActive' in src_h
assert 'freezeBank.reset()' in src
print('params + cycle de vie OK')

assert 'givreSlider' in ed_h and 'givreLabel' in ed_h and 'givreAtt' in ed_h
assert 'freezeBtn' not in ed_h and 'freezeBtn' not in ed
assert 'LinearHorizontal' in ed and 'givreLabel.setBounds(438' in ed
assert 'onPadClicked' in ed and 'startTimer(120)' in ed
print('UI fader Givre + pastilles OK')
print('GIVRE DRY/WET VERIFIE')
