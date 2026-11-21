import re
base = 'C:/PrismeBuild/build/_deps/juce-src/modules/juce_dsp/frequency/'
src = open(base + 'juce_FFT.cpp', encoding='utf-8').read()

def show(func):
    lines = src.split('\n')
    started = False
    depth = 0
    began = False
    for l in lines:
        if not started and func in l and ('(' in l):
            started = True
        if started:
            print(l[:100])
            depth += l.count('{') - l.count('}')
            if '{' in l:
                began = True
            if began and depth == 0:
                break

print('########## performFrequencyOnlyForwardTransform')
show('performFrequencyOnlyForwardTransform')
