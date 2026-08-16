#!/usr/bin/env python3
"""Synthesize an art-deco-styled TV test card, 240x240, via PIL (no AI
image generation available). Rendered supersampled then downscaled for
clean anti-aliased edges once it hits the round 240x240 panel."""

import math
import os
from PIL import Image, ImageDraw, ImageFont

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SS = 5                      # supersample factor
S = 240 * SS                # working canvas size
CX, CY = S / 2, S / 2
R = S / 2 - 4 * SS          # outer radius, small margin from panel edge

BG      = (17, 16, 15)
GOLD    = (196, 158, 74)
CREAM   = (232, 222, 196)
DARK    = (10, 9, 8)
MUSTARD = (196, 150, 60)
TEAL    = (46, 107, 104)
MAROON  = (128, 46, 40)
OLIVE   = (98, 104, 58)
NAVY    = (34, 52, 78)
RUST    = (156, 82, 42)

img = Image.new("RGB", (S, S), BG)
d = ImageDraw.Draw(img)

def ring(cx, cy, r, color, width):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=color, width=width)

def font(size):
    return ImageFont.truetype(
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", size)

def spaced_text(draw, xy, text, fnt, fill, tracking, cy):
    """Draw caps text with extra letter spacing, centered on (xy[0], cy).
    Uses the whole string's bbox (not each glyph's) for vertical metrics
    so every character sits on the same baseline instead of jittering."""
    widths = [draw.textlength(ch, font=fnt) for ch in text]
    total = sum(widths) + tracking * (len(text) - 1)
    x = xy[0] - total / 2
    bbox = draw.textbbox((0, 0), text, font=fnt)
    top, bottom = bbox[1], bbox[3]
    y = cy - (bottom - top) / 2 - top
    for ch, w in zip(text, widths):
        draw.text((x, y), ch, font=fnt, fill=fill)
        x += w + tracking

# ---- circular clip mask (round panel: keep square corners as bezel-hidden) ----

# ---- sunburst rays band (art-deco rising-sun motif) ----
ray_r_in, ray_r_out = R * 0.30, R * 0.62
n_rays = 32
for i in range(n_rays):
    a0 = 2 * math.pi * i / n_rays
    a1 = a0 + (2 * math.pi / n_rays)
    col = CREAM if i % 2 == 0 else DARK
    pts = [
        (CX + ray_r_in * math.cos(a0), CY + ray_r_in * math.sin(a0)),
        (CX + ray_r_out * math.cos(a0), CY + ray_r_out * math.sin(a0)),
        (CX + ray_r_out * math.cos(a1), CY + ray_r_out * math.sin(a1)),
        (CX + ray_r_in * math.cos(a1), CY + ray_r_in * math.sin(a1)),
    ]
    d.polygon(pts, fill=col)

# ---- center bullseye / convergence target ----
for i, (rad, col) in enumerate([
    (R * 0.30, GOLD), (R * 0.24, DARK), (R * 0.18, GOLD),
    (R * 0.12, DARK), (R * 0.06, GOLD),
]):
    d.ellipse([CX - rad, CY - rad, CX + rad, CY + rad], fill=col)
d.ellipse([CX - R * 0.02, CY - R * 0.02, CX + R * 0.02, CY + R * 0.02], fill=CREAM)
# crosshair
ch_len = R * 0.34
d.line([CX - ch_len, CY, CX + ch_len, CY], fill=CREAM, width=int(1.4 * SS))
d.line([CX, CY - ch_len, CX, CY + ch_len], fill=CREAM, width=int(1.4 * SS))

# ---- outer deco border rings + tick marks ----
ring(CX, CY, R, GOLD, int(2.2 * SS))
ring(CX, CY, R - 10 * SS, GOLD, int(1 * SS))
n_ticks = 60
for i in range(n_ticks):
    a = 2 * math.pi * i / n_ticks
    major = (i % 5 == 0)
    r0 = R - (16 if major else 11) * SS
    r1 = R - 3 * SS
    w = int((2.2 if major else 1.1) * SS)
    d.line([
        (CX + r0 * math.cos(a), CY + r0 * math.sin(a)),
        (CX + r1 * math.cos(a), CY + r1 * math.sin(a)),
    ], fill=GOLD, width=w)

# ---- lower band: deco color bars (test-card function) inside a black plate ----
bar_colors = [CREAM, MUSTARD, RUST, MAROON, TEAL, OLIVE, NAVY]
band_half_w = R * 0.86
band_y0, band_y1 = CY + R * 0.34, CY + R * 0.62
plate_pad = 6 * SS
d.rectangle([CX - band_half_w - plate_pad, band_y0 - plate_pad,
             CX + band_half_w + plate_pad, band_y1 + plate_pad], fill=DARK)
n_bars = len(bar_colors)
bar_w = (2 * band_half_w) / n_bars
for i, col in enumerate(bar_colors):
    x0 = CX - band_half_w + i * bar_w
    d.rectangle([x0, band_y0, x0 + bar_w, band_y1], fill=col)
d.rectangle([CX - band_half_w - plate_pad, band_y0 - plate_pad,
             CX + band_half_w + plate_pad, band_y1 + plate_pad],
            outline=GOLD, width=int(1.4 * SS))

# ---- grayscale step wedge, just above color bars ----
gband_y0, gband_y1 = band_y0 - plate_pad - 22 * SS, band_y0 - plate_pad - 4 * SS
n_steps = 8
for i in range(n_steps):
    v = int(255 * i / (n_steps - 1))
    x0 = CX - band_half_w + i * (2 * band_half_w / n_steps)
    x1 = CX - band_half_w + (i + 1) * (2 * band_half_w / n_steps)
    d.rectangle([x0, gband_y0, x1, gband_y1], fill=(v, v, v))
d.rectangle([CX - band_half_w, gband_y0, CX + band_half_w, gband_y1],
            outline=GOLD, width=int(1 * SS))

# ---- deco caption, single line in the gap between the color-bar plate
#      and the bottom ring (the only spot with both width and headroom) ----
f_title = font(int(13 * SS))
caption_cy = (band_y1 + plate_pad + (CY + R - 6 * SS)) / 2
spaced_text(d, (CX, caption_cy), "TEST PATTERN", f_title, CREAM, 4 * SS, caption_cy)

# small corner stars at the four cardinal ring gaps for deco flourish
for a in [0, math.pi / 2, math.pi, 3 * math.pi / 2]:
    sx, sy = CX + (R - 6 * SS) * math.cos(a), CY + (R - 6 * SS) * math.sin(a)
    sr = 3.2 * SS
    pts = []
    for k in range(8):
        ang = math.pi / 4 * k
        rr = sr if k % 2 == 0 else sr * 0.4
        pts.append((sx + rr * math.cos(ang), sy + rr * math.sin(ang)))
    d.polygon(pts, fill=GOLD)

out = img.resize((240, 240), Image.LANCZOS)
out_path = os.path.join(REPO_ROOT, "photos", "tv_test_card_art_deco.png")
out.save(out_path)
print("wrote", out_path)
