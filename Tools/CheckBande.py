import wave, struct, math, sys

def load(p):
    w = wave.open(p, 'rb')
    n = w.getnframes()
    ch = w.getnchannels()
    s = struct.unpack('<' + 'h' * (n * ch), w.readframes(n))
    return [(s[i] + s[i + 1]) / 2 / 32767 for i in range(0, len(s), 2)]

def split_lo_hi(x, sr, fc=800.0):
    # Passe-bas un pole + complement (robuste, domaine temporel)
    k = 1.0 - math.exp(-2.0 * math.pi * fc / sr)
    lo, y = [], 0.0
    for v in x:
        y += k * (v - y)
        lo.append(y)
    hi = [v - l for v, l in zip(x, lo)]
    return lo, hi

def rms(x):
    return math.sqrt(sum(v * v for v in x) / len(x))

for name in ['Release/Demo_Drums_Bande.wav', 'Release/Demo_Drums_WetRef.wav']:
    x = load(name)
    seg = x[44100:44100 * 5]
    lo, hi = split_lo_hi(seg, 44100)
    peak = max(abs(v) for v in x)
    print('%s peak=%.3f graves=%.4f aigus=%.4f' % (name.split('/')[-1], peak, rms(lo), rms(hi)))

xb, xw = load('Release/Demo_Drums_Bande.wav'), load('Release/Demo_Drums_WetRef.wav')
rb = rms(xb[44100:44100 * 5])
print('bande audible:', 'OK' if rb > 0.005 else 'TROP FAIBLE')
