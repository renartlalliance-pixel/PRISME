#!/usr/bin/env python3
# Pack graphique SOVIET ANALOG pour PRISME — generation 100 % originale (PIL/numpy).
# Sortie : Assets/Skins/SovietAnalog/ (arborescence propre, prete a zipper).
import os, math, random
import numpy as np
from PIL import Image, ImageDraw

random.seed(2077)
np.random.seed(2077)
ROOT = "C:/PrismeMix/Assets/Skins/SovietAnalog"
SUBS = ["Backgrounds", "Textures", "Panels", "Knobs", "Sliders", "Buttons",
        "Indicators", "Meters", "Spectrum", "Borders", "Typography",
        "Previews", "Documentation"]
for s in SUBS:
    os.makedirs(os.path.join(ROOT, s), exist_ok=True)

def hexrgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i+2], 16) for i in (0, 2, 4))

def save(img, *parts):
    p = os.path.join(ROOT, *parts)
    img.save(p)
    print("gen", os.path.relpath(p, ROOT), img.size)

def vgrad(w, h, top, bot):
    t = np.linspace(0, 1, h)[:, None, None]
    a = np.array(hexrgb(top), float)[None, None, :]
    b = np.array(hexrgb(bot), float)[None, None, :]
    return np.broadcast_to(a * (1 - t) + b * t, (h, w, 3))

def grain(arr, amt):
    n = np.random.normal(0, amt, arr.shape[:2])[..., None]
    return np.clip(arr + n, 0, 255)

def vignette(arr, strength=0.35):
    h, w, _ = arr.shape
    yy, xx = np.mgrid[0:h, 0:w].astype(float)
    d = np.sqrt(((xx - w/2)/(w/2))**2 + ((yy - h/2)/(h/2))**2) / np.sqrt(2)
    return arr * (1 - strength * np.clip(d, 0, 1))[..., None]

def speckle(img, n, color, alpha, rmin=1, rmax=2):
    d = ImageDraw.Draw(img, "RGBA")
    w, h = img.size
    for _ in range(n):
        x, y = random.randrange(w), random.randrange(h)
        r = random.randint(rmin, rmax)
        d.ellipse([x-r, y-r, x+r, y+r], fill=color + (alpha,))

def scratches(img, n, color=(20, 18, 16), alpha=40, maxlen=120):
    d = ImageDraw.Draw(img, "RGBA")
    w, h = img.size
    for _ in range(n):
        x, y = random.randrange(w), random.randrange(h)
        ang = random.uniform(0, math.pi)
        L = random.uniform(8, maxlen)
        d.line([x, y, x + L*math.cos(ang), y + L*math.sin(ang)],
               fill=color + (alpha,), width=1)

def brushed(arr, streak=6.0):
    h, w, _ = arr.shape
    rows = np.random.normal(0, streak, (h, 1, 1))
    out = arr + np.broadcast_to(rows, (h, w, 1))
    return np.clip(out, 0, 255)

# ---------- Backgrounds ----------
bg = vgrad(880, 970, "#232527", "#141517")
bg = brushed(bg, 5.0)
bg = grain(bg, 4.0)
bg = vignette(bg, 0.42)
save(Image.fromarray(bg.astype("uint8")), "Backgrounds", "bg_main_plate.png")

disp = vgrad(256, 64, "#0B0C0D", "#060708")
disp = grain(disp, 2.0)
save(Image.fromarray(disp.astype("uint8")), "Backgrounds", "bg_display_well.png")

# ---------- Textures (raccordables) ----------
t = np.ones((256, 256, 3)) * np.array(hexrgb("#8B9094"), float)
t = brushed(t, 9.0); t = grain(t, 5.0)
save(Image.fromarray(np.tile(t, (1, 1, 1)).astype("uint8")), "Textures", "tex_brushed_metal_tile.png")

t = np.ones((256, 256, 3)) * np.array(hexrgb("#4A4D42"), float)
t = grain(t, 7.0)
im = Image.fromarray(t.astype("uint8")); speckle(im, 500, (30, 32, 26), 50); scratches(im, 25)
save(im, "Textures", "tex_military_plate_tile.png")

t = np.ones((512, 512, 3)) * np.array(hexrgb("#3A3D40"), float)
t = brushed(t, 6.0); t = grain(t, 6.0)
im = Image.fromarray(t.astype("uint8")); speckle(im, 900, (90, 70, 46), 46); scratches(im, 120)
save(im, "Textures", "tex_patina_controlled_tile.png")

ov = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
speckle(ov, 1400, (96, 74, 48), 40); scratches(ov, 160, (12, 10, 8), 46, 200)
ov.save(os.path.join(ROOT, "Textures", "tex_patina_scratches_overlay.png"))
print("gen", "Textures/tex_patina_scratches_overlay.png", ov.size)

t = np.ones((128, 128, 3)) * np.array(hexrgb("#0A0B0C"), float)
t = grain(t, 2.5)
save(Image.fromarray(t.astype("uint8")), "Textures", "tex_dark_display_tile.png")

t = np.ones((64, 64, 3)) * np.array(hexrgb("#1E2023"), float)
t[::4, :, :] *= 0.82
save(Image.fromarray(t.astype("uint8")), "Textures", "tex_hlines_tile.png")

t = np.ones((512, 512, 3)) * np.array(hexrgb("#33363B"), float)
t = brushed(t, 5.0); t = grain(t, 5.0)
im = Image.fromarray(t.astype("uint8")); speckle(im, 700, (88, 70, 48), 40, 1, 3); scratches(im, 90)
d = ImageDraw.Draw(im)
d.rectangle([8, 8, 503, 503], outline=(16, 17, 19, 255), width=3)
d.line([8, 12, 503, 12], fill=(120, 124, 128, 90), width=1)
save(im, "Textures", "tex_industrial_panel_tile.png")

gr = (np.random.normal(128, 42, (256, 256))).clip(0, 255).astype("uint8")
ga = Image.fromarray(gr, "L")
ov = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
ov.putalpha(ga.point(lambda v: int(v * 0.10)))
ov.save(os.path.join(ROOT, "Textures", "tex_grain_overlay.png"))
print("gen", "Textures/tex_grain_overlay.png", ov.size)

# ---------- Panels ----------
def panel(w, h, top="#2E3134", bot="#222426", edge="#3F434A", bevel="#767B82"):
    img = Image.new("RGB", (w, h))
    a = vgrad(w, h, top, bot)
    a = grain(a, 3.0)
    img = Image.fromarray(a.astype("uint8"))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([1, 1, w-2, h-2], radius=8, outline=hexrgb(edge), width=2)
    d.line([6, 2, w-6, 2], fill=hexrgb(bevel), width=1)
    return img

save(panel(640, 420), "Panels", "panel_main.png")
save(panel(640, 200, "#2A2D30", "#1E2023"), "Panels", "panel_sub.png")

# ---------- Knobs ----------
def knob_body(size, hi="#3A3D42", lo="#17181B", cap="#0C0D0E"):
    c = size // 2
    yy, xx = np.mgrid[0:size, 0:size].astype(float)
    dist = np.sqrt((xx-c)**2 + (yy-c)**2) / (size/2)
    base = np.zeros((size, size, 3))
    t = np.clip(dist, 0, 1)[..., None]
    base = np.array(hexrgb(hi), float) * (1-t) + np.array(hexrgb(lo), float) * t
    ring = np.clip(1 - np.abs(dist-0.86)*9, 0, 1)[..., None]
    base = base * (1-ring*0.35) + np.array(hexrgb("#0A0B0C"), float) * (ring*0.35)
    img = Image.fromarray(np.clip(brushed(grain(base, 4.0), 4.0), 0, 255).astype("uint8"), "RGB")
    img.putalpha(Image.fromarray((255*(dist <= 1.0)).astype("uint8"), "L") if False else None)
    alpha = Image.new("L", (size, size), 0)
    ImageDraw.Draw(alpha).ellipse([0, 0, size-1, size-1], fill=255)
    img.putalpha(alpha)
    d = ImageDraw.Draw(img)
    d.ellipse([0, 0, size-1, size-1], outline=(10, 11, 12, 255), width=max(2, size//90))
    cr = int(size*0.14)
    d.ellipse([c-cr, c-cr, c+cr, c+cr], fill=hexrgb(cap)+(255,))
    d.ellipse([c-cr, c-cr-2, c+cr, c+cr-2], fill=(200, 204, 208, 70))
    return img

for name, sz in [("knob_L_body", 256), ("knob_M_body", 192), ("knob_S_body", 128)]:
    save(knob_body(sz), "Knobs", name + ".png")

ptr = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
d = ImageDraw.Draw(ptr)
d.line([128, 128-36, 128, 128-104], fill=(255, 208, 128, 255), width=7)
d.ellipse([128-9, 128-104-9, 128+9, 128-104+9], fill=(255, 224, 160, 255))
save(ptr, "Knobs", "knob_pointer_overlay.png")

ring = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
d = ImageDraw.Draw(ring)
d.arc([14, 14, 242, 242], start=135, end=405, fill=(232, 160, 32, 220), width=9)
save(ring, "Knobs", "knob_litring_overlay.png")

# ---------- Sliders ----------
rv = vgrad(20, 256, "#08090A", "#101114")
rv = grain(rv, 2.0)
ri = Image.fromarray(rv.astype("uint8"))
d = ImageDraw.Draw(ri)
d.rounded_rectangle([1, 1, 18, 254], radius=8, outline=(60, 62, 68))
save(ri, "Sliders", "rail_v_tile.png")

rh = vgrad(256, 20, "#08090A", "#101114").transpose(1, 0, 2).copy()
rh = grain(rh, 2.0)
ri = Image.fromarray(rh.astype("uint8"))
d = ImageDraw.Draw(ri)
d.rounded_rectangle([1, 1, 254, 18], radius=8, outline=(60, 62, 68))
save(ri, "Sliders", "rail_h_tile.png")

cap = Image.new("RGB", (100, 28))
a = vgrad(100, 28, "#9AA0A3", "#5A5E63")
cap = Image.fromarray(brushed(grain(a, 3.0), 4.0).astype("uint8"))
d = ImageDraw.Draw(cap)
d.rounded_rectangle([1, 1, 98, 26], radius=5, outline=(12, 13, 14), width=2)
d.line([8, 13, 92, 13], fill=(232, 160, 32), width=3)
d.line([8, 3, 92, 3], fill=(210, 214, 218), width=1)
save(cap, "Sliders", "cap_metal.png")

# ---------- Buttons ----------
def metal_btn(w, h, base, glow=None, text=None):
    img = Image.new("RGB", (w, h))
    a = vgrad(w, h, base[0], base[1])
    img = Image.fromarray(grain(a, 2.5).astype("uint8"))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([1, 1, w-2, h-2], radius=6, outline=(12, 13, 15), width=2)
    d.line([7, 2, w-7, 2], fill=(215, 218, 222), width=1)
    if glow:
        d.rounded_rectangle([1, 1, w-2, h-2], radius=6, outline=glow, width=2)
        d.line([7, h-4, w-7, h-4], fill=glow, width=2)
    return img

save(metal_btn(248, 52, ("#3A3D42", "#222426")), "Buttons", "btn_metal_idle.png")
save(metal_btn(248, 52, ("#45494F", "#282B2F")), "Buttons", "btn_metal_hover.png")
save(metal_btn(248, 52, ("#4A3A20", "#2A2118"), glow=(232, 160, 32)), "Buttons", "btn_metal_active.png")
im = metal_btn(248, 52, ("#2A2C2E", "#1C1D1F")).convert("L").convert("RGB")
save(im, "Buttons", "btn_metal_disabled.png")
save(metal_btn(192, 52, ("#7A2A1A", "#4A1A10"), glow=(255, 90, 42)), "Buttons", "btn_compact_red_idle.png")
save(metal_btn(192, 52, ("#A83A1E", "#5E2012"), glow=(255, 120, 70)), "Buttons", "btn_compact_red_active.png")
save(metal_btn(192, 104, ("#7A2A1A", "#3A140C"), glow=(255, 90, 42)), "Buttons", "btn_rec.png")

# ---------- Indicators ----------
def led(size, rgb, glow):
    c = size // 2
    yy, xx = np.mgrid[0:size, 0:size].astype(float)
    dist = np.sqrt((xx-c)**2 + (yy-c)**2) / (size/2)
    img = np.zeros((size, size, 4), dtype=float)
    core = np.clip(1 - dist*1.6, 0, 1)
    halo = np.clip(1 - dist, 0, 1) ** 2 * glow
    for ch in range(3):
        img[..., ch] = rgb[ch] * np.maximum(core, halo * 0.8)
    img[..., 3] = np.clip(np.maximum(core, halo), 0, 1) * 255
    return Image.fromarray(img.astype("uint8"), "RGBA")

save(led(32, (60, 62, 68), 0.0), "Indicators", "led_off.png")
save(led(32, (255, 90, 42), 1.0), "Indicators", "led_red.png")
save(led(32, (232, 160, 32), 1.0), "Indicators", "led_amber.png")
save(led(32, (90, 200, 120), 1.0), "Indicators", "led_green.png")

# ---------- Meters ----------
vu = Image.new("RGB", (256, 192))
a = vgrad(256, 192, "#141518", "#0A0B0C")
vu = Image.fromarray(grain(a, 2.0).astype("uint8"))
d = ImageDraw.Draw(vu)
d.rounded_rectangle([1, 1, 254, 190], radius=6, outline=(63, 67, 74), width=2)
cx, cy, R = 128, 170, 130
for i in range(21):
    ang = math.radians(-40 + i * 4)
    r0, r1 = R - (18 if i % 5 == 0 else 10), R - 2
    hot = i >= 16
    col = (200, 57, 31) if hot else (216, 213, 204)
    d.line([cx + r0*math.sin(ang), cy - r0*math.cos(ang),
            cx + r1*math.sin(ang), cy - r1*math.cos(ang)], fill=col, width=3 if i % 5 == 0 else 1)
d.arc([cx-R, cy-R, cx+R, cy+R], start=230, end=310, fill=(200, 57, 31), width=5)
save(vu, "Meters", "meter_vu_face.png")

nd = Image.new("RGBA", (16, 160), (0, 0, 0, 0))
d = ImageDraw.Draw(nd)
d.line([8, 158, 8, 6], fill=(230, 228, 220, 255), width=3)
d.ellipse([1, 150, 15, 164], fill=(40, 42, 45, 255))
save(nd, "Meters", "meter_needle.png")

# ---------- Spectrum ----------
sg = np.ones((128, 128, 3)) * np.array(hexrgb("#0A0B0C"), float)
sg = grain(sg, 2.0)
si = Image.fromarray(sg.astype("uint8"))
d = ImageDraw.Draw(si)
for x in range(0, 128, 16):
    d.line([x, 0, x, 128], fill=(255, 255, 255, 14), width=1)
for y in range(0, 128, 16):
    d.line([0, y, 128, y], fill=(255, 255, 255, 14), width=1)
save(si, "Spectrum", "spec_grid_tile.png")

leg = np.zeros((64, 256, 3))
deep, mid, hot = (np.array(hexrgb(c), float) for c in ("#5A1A0A", "#E8641E", "#FFD080"))
for y in range(256):
    t = y / 255
    c = deep * (1-t) + (mid if t < 0.5 else hot) * 0 if False else None
    c = deep * max(0, 1-t*2) + mid * (1-abs(t-0.5)*2) + hot * max(0, t*2-1)
    leg[:, y, :] = 0
leg = np.zeros((256, 64, 3))
for y in range(256):
    t = y / 255
    c = deep * max(0.0, 1-t*2) + mid * max(0.0, 1-abs(t-0.5)*2) + hot * max(0.0, t*2-1)
    leg[y, :, :] = c
save(Image.fromarray(np.clip(leg, 0, 255).astype("uint8")), "Spectrum", "spec_legend_gradient.png")

print("PNG OK")
