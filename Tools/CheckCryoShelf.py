import math, cmath

def highshelf(sr, f0, gdb):
    A = 10 ** (gdb / 40)
    w0 = 2 * math.pi * f0 / sr
    cw, sw = math.cos(w0), math.sin(w0)
    al = sw / 2 * math.sqrt(2.0)
    a0 = (A + 1) - (A - 1) * cw + 2 * math.sqrt(A) * al
    b = [A * ((A + 1) + (A - 1) * cw + 2 * math.sqrt(A) * al) / a0,
         -2 * A * ((A - 1) + (A + 1) * cw) / a0,
         A * ((A + 1) + (A - 1) * cw - 2 * math.sqrt(A) * al) / a0]
    a = [1, 2 * ((A - 1) - (A + 1) * cw) / a0,
         ((A + 1) - (A - 1) * cw - 2 * math.sqrt(A) * al) / a0]
    return b, a

def H(b, a, f, sr):
    z = cmath.exp(1j * 2 * math.pi * f / sr)
    num = sum(bi * z ** -k for k, bi in enumerate(b))
    den = sum(ai * z ** -k for k, ai in enumerate(a))
    return num / den

def poles(a):
    b, c = a[1], a[2]
    d = b * b - 4 * c
    r1 = (-b + cmath.sqrt(d)) / 2
    r2 = (-b - cmath.sqrt(d)) / 2
    return abs(r1), abs(r2)

sr = 44100
b, a = highshelf(sr, 7500, 9.0)
print('stabilite:', max(poles(a)) < 1.0)
for f, expect in [(1000, 0.0), (7500, 4.5), (12000, 9.0), (18000, 9.0)]:
    g = 20 * math.log10(abs(H(b, a, f, sr)))
    print('f=%5d -> %+.2f dB (attendu %+.1f) %s' % (f, g, expect, 'OK' if abs(g - expect) < 0.7 else 'PROBLEME'))
