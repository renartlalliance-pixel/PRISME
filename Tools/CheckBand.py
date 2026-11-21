import math, cmath

def hp(sr, f0, Q=0.7071067811865476):
    w0 = 2 * math.pi * f0 / sr
    if w0 >= 3.10:
        return [1, 0, 0], [1, 0, 0]
    cw, sw = math.cos(w0), math.sin(w0)
    al = sw / (2 * Q)
    b0, b1, b2 = (1 + cw) / 2, -(1 + cw), (1 + cw) / 2
    a0, a1, a2 = 1 + al, -2 * cw, 1 - al
    return [b0 / a0, b1 / a0, b2 / a0], [1, a1 / a0, a2 / a0]

def lp(sr, f0, Q=0.7071067811865476):
    w0 = 2 * math.pi * f0 / sr
    if w0 >= 3.10:
        return [1, 0, 0], [1, 0, 0]
    cw, sw = math.cos(w0), math.sin(w0)
    al = sw / (2 * Q)
    b0, b1, b2 = (1 - cw) / 2, 1 - cw, (1 - cw) / 2
    a0, a1, a2 = 1 + al, -2 * cw, 1 - al
    return [b0 / a0, b1 / a0, b2 / a0], [1, a1 / a0, a2 / a0]

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
bh, ah = hp(sr, 200)
bl, al = lp(sr, 2000)
print('stabilite:', max(poles(ah)) < 1.0 and max(poles(al)) < 1.0)
for f in [40, 100, 200, 500, 1000, 2000, 4000, 10000]:
    tot = H(bh, ah, f, sr) * H(bl, al, f, sr)
    print('f=%5d -> %+.1f dB' % (f, 20 * math.log10(abs(tot))))
# bypass : 20-20000 doit etre ~plat
bh2, ah2 = hp(sr, 20)
bl2, al2 = lp(sr, 20000)
worst = 0
for f in [30, 100, 1000, 5000, 15000]:
    tot = H(bh2, ah2, f, sr) * H(bl2, al2, f, sr)
    worst = max(worst, abs(20 * math.log10(abs(tot))))
print('bypass ecart max: %.3f dB %s' % (worst, 'OK' if worst < 1.5 else 'PROBLEME'))
