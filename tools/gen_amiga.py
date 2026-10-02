"""
"Amiga Workbench" skin for RetroAmp: Workbench 3.0 bevels with the MagicWB palette,
the blue/orange Workbench 1.3 display and the red/white Boing Ball.

Generated in HD (K=4, PNG sheets) using the standard Winamp 2.x sprite layout.
Usage:  python tools/gen_amiga.py   -> skins/AmigaWorkbench.wsz
"""
import math
import os
import shutil
import zipfile

from PIL import Image, ImageDraw

import gen_skin as G
from gen_skin import crop, paste, poly, rect, hline, vline, thick_line, ttf

# MagicWB palette + Workbench 1.3 display colours
GREY = (149, 149, 149)
DKGREY = (123, 123, 123)
LTGREY = (175, 175, 175)
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
BLUE = (59, 103, 162)
WBBLUE = (102, 136, 187)
BEIGE = (170, 144, 124)
PINK = (255, 169, 151)
DISP = (0, 85, 170)          # WB 1.3 blue
DISP_DIM = (40, 110, 190)
ORANGE = (255, 136, 0)
DISP_GHOST = (16, 96, 178)


def new(w, h, c=GREY):
    return G.new(w, h, c)


def bevel(img, x0, y0, x1, y1, raised=True, fill=GREY, t=1):
    """Workbench style box: light top/left, dark bottom/right (inverted when recessed)."""
    if fill is not None:
        rect(img, x0, y0, x1, y1, fill)
    hi, lo = (WHITE, BLACK) if raised else (BLACK, WHITE)
    rect(img, x0, y0, x1, y0 + t, hi)
    rect(img, x0, y0, x0 + t, y1, hi)
    rect(img, x0, y1 - t, x1, y1, lo)
    rect(img, x1 - t, y0, x1, y1, lo)


def text(img, x, y, s, color, cap=5, center=False, right=False, font="arialbd.ttf", maxw=None):
    """Capitals occupy rows y..y+cap (skin pixels)."""
    K = G.K
    f = ttf(font, cap * K / 0.716)
    d = ImageDraw.Draw(img)
    w = d.textlength(s, font=f)
    if maxw and w > maxw * K:
        f = ttf(font, cap * K / 0.716 * maxw * K / w)
    anchor = "ms" if center else ("rs" if right else "ls")
    d.text((x * K, (y + cap) * K), s, font=f, fill=color, anchor=anchor)


def boing_ball(img, cx, cy, r, spin=0.35, tilt=0.35, shadow=True, bg=None):
    """Shaded red/white checkered Boing Ball, anti-aliased (supersampled)."""
    K = G.K
    ss = 4
    R = r * K * ss
    size = int(R * 2 + 4)
    ball = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    px = ball.load()
    ct, st = math.cos(tilt), math.sin(tilt)
    c0 = size / 2
    for y in range(size):
        for x in range(size):
            nx, ny = (x + 0.5 - c0) / R, (y + 0.5 - c0) / R
            r2 = nx * nx + ny * ny
            if r2 > 1:
                continue
            nz = math.sqrt(1 - r2)
            tx, ty = nx * ct - ny * st, nx * st + ny * ct
            lat = math.asin(max(-1, min(1, ty)))
            lon = math.atan2(tx, nz) + spin
            ci = math.floor(lon / (math.pi / 8)) + math.floor(lat / (math.pi / 8))
            base = (255, 255, 255) if ci & 1 else (225, 0, 0)
            light = max(0.3, min(1.0, 0.35 + 0.75 * (-0.45 * nx - 0.55 * ny + 0.7 * nz)))
            px[x, y] = (int(base[0] * light), int(base[1] * light), int(base[2] * light), 255)
    ball = ball.resize((size // ss, size // ss), Image.Resampling.LANCZOS)
    if shadow:
        sh = Image.new("L", ball.size, 0)
        ImageDraw.Draw(sh).ellipse([0, 0, ball.size[0] - 1, ball.size[1] - 1], fill=90)
        dark = Image.new("RGB", ball.size, (60, 60, 60))
        img.paste(dark, (int((cx - r) * K + r * K * 0.3), int((cy - r) * K + r * K * 0.12)), sh)
    img.paste(ball, (int((cx - r) * K), int((cy - r) * K)), ball)


# ---------------------------------------------------------------------------- glyphs (black)
def g_prev(img, cx, cy, c):
    rect(img, cx - 4.5, cy - 3.5, cx - 3, cy + 3.5, c)
    poly(img, [(cx + 3.5, cy - 3.5), (cx + 3.5, cy + 3.5), (cx - 2.5, cy)], c)


def g_next(img, cx, cy, c):
    rect(img, cx + 3, cy - 3.5, cx + 4.5, cy + 3.5, c)
    poly(img, [(cx - 3.5, cy - 3.5), (cx - 3.5, cy + 3.5), (cx + 2.5, cy)], c)


def g_play(img, cx, cy, c):
    poly(img, [(cx - 2.5, cy - 4.5), (cx - 2.5, cy + 4.5), (cx + 4, cy)], c)


def g_pause(img, cx, cy, c):
    rect(img, cx - 3.5, cy - 4, cx - 1, cy + 4, c)
    rect(img, cx + 1, cy - 4, cx + 3.5, cy + 4, c)


def g_stop(img, cx, cy, c):
    rect(img, cx - 3.5, cy - 3.5, cx + 3.5, cy + 3.5, c)


def g_eject(img, cx, cy, c):
    poly(img, [(cx - 4, cy + 0.5), (cx + 4, cy + 0.5), (cx, cy - 4)], c)
    rect(img, cx - 4, cy + 2, cx + 4, cy + 3.5, c)


def button(img, x, y, w, h, pressed, glyph=None, label=None, sel=False, cap=4.5):
    fill = WBBLUE if (pressed or sel) else GREY
    bevel(img, x, y, x + w, y + h, raised=not pressed, fill=fill)
    off = 0.5 if pressed else 0
    col = WHITE if sel and not pressed else BLACK
    if glyph:
        glyph(img, x + w / 2 + off, y + h / 2 + off, col)
    if label:
        text(img, x + w / 2 + off, y + (h - cap) / 2 + off, label, col, cap=cap, center=True, maxw=w - 3)


# ---------------------------------------------------------------------------- MAIN
def make_main():
    img = new(275, 116)
    bevel(img, 0, 0, 275, 116, raised=True, fill=None)
    # L-shaped recessed display: left part down to y=68, right part to y=55
    rect(img, 4, 17, 271, 55, DISP)
    rect(img, 4, 17, 102, 68, DISP)
    rect(img, 4, 16, 271, 17, BLACK); rect(img, 3, 16, 4, 69, BLACK)
    rect(img, 4, 68, 103, 69, WHITE); rect(img, 102, 55, 103, 69, WHITE)
    rect(img, 102, 55, 272, 56, WHITE); rect(img, 271, 16, 272, 56, WHITE)
    # labels and time colon
    text(img, 129, 44, "KBPS", DISP_DIM, cap=4)
    text(img, 168, 44, "KHZ", DISP_DIM, cap=4)
    G.ellipse(img, 72, 29.6, 74, 31.6, ORANGE)
    G.ellipse(img, 71.8, 35, 73.8, 37, ORANGE)
    # groove for the position bar
    bevel(img, 15, 71, 265, 83, raised=False, fill=GREY)
    # lower right panel with the Boing ball
    bevel(img, 159, 85, 273, 113, raised=False, fill=LTGREY)
    bevel(img, 160, 86, 272, 112, raised=True, fill=GREY)
    boing_ball(img, 256.5, 98.5, 11.5)
    return img


# ---------------------------------------------------------------------------- TITLEBAR
def title_bar(active, title, shade=False):
    img = new(275, 14)
    bevel(img, 0, 0, 275, 14, raised=True, fill=WBBLUE if active else GREY)
    # Workbench style gadget separators
    for x in (17, 242):
        rect(img, x, 1, x + 1, 13, BLACK)
        rect(img, x + 1, 1, x + 2, 13, WHITE)
    if shade:
        text(img, 21, 4.5, title, BLACK, cap=5)
        bevel(img, 118, 2, 166, 12, raised=False, fill=DISP)
        return img
    text(img, 22, 4, title, BLACK, cap=6)
    return img


def gadget(img, x, y, kind, pressed):
    bevel(img, x, y, x + 9, y + 9, raised=not pressed, fill=WBBLUE if pressed else GREY)
    c = BLACK
    if kind == "close":     # Workbench close gadget: box with a dot
        rect(img, x + 2.5, y + 2.5, x + 6.5, y + 6.5, WHITE)
        rect(img, x + 3.5, y + 3.5, x + 5.5, y + 5.5, c)
    elif kind == "min":     # iconify
        rect(img, x + 2, y + 5, x + 5, y + 7, c)
    elif kind == "zoom":    # zoom gadget: rect in rect
        rect(img, x + 2, y + 2, x + 7, y + 7, c)
        rect(img, x + 3, y + 3, x + 6, y + 6, WHITE)
        rect(img, x + 3, y + 3, x + 4.5, y + 4.5, c)
    elif kind == "unzoom":
        rect(img, x + 2, y + 2, x + 7, y + 7, c)
        rect(img, x + 3, y + 3, x + 6, y + 6, GREY)
    elif kind == "ball":    # options button = tiny Boing ball
        boing_ball(img, x + 4.5, y + 4.5, 3.4, shadow=False)


def make_titlebar():
    img = new(344, 87)
    for (x, y, pressed, kind) in [
        (0, 0, False, "ball"), (0, 9, True, "ball"),
        (9, 0, False, "min"), (9, 9, True, "min"),
        (18, 0, False, "close"), (18, 9, True, "close"),
        (0, 18, False, "zoom"), (9, 18, True, "zoom"),
        (0, 27, False, "unzoom"), (9, 27, True, "unzoom"),
    ]:
        gadget(img, x, y, kind, pressed)
    paste(img, title_bar(True, "RetroAmp"), 27, 0)
    paste(img, title_bar(False, "RetroAmp"), 27, 15)
    paste(img, title_bar(True, "RetroAmp", shade=True), 27, 29)
    paste(img, title_bar(False, "RetroAmp", shade=True), 27, 42)
    paste(img, title_bar(True, "Only Amiga makes it possible!"), 27, 57)
    paste(img, title_bar(False, "Only Amiga makes it possible!"), 27, 72)
    for x, col in ((304, DISP_DIM), (312, DISP_GHOST)):
        rect(img, x, 0, x + 8, 43, DISP)
        for oy, oh in ((3, 8), (11, 7), (18, 7), (25, 8), (33, 7)):
            rect(img, x + 2, oy + oh // 2, x + 6, oy + oh // 2 + 1, col)
    for x, y, h in ((304, 47, 8), (312, 55, 7), (320, 62, 7), (328, 69, 8), (336, 77, 7)):
        rect(img, x, y, x + 8, y + h, DISP)
        rect(img, x + 1, y + h // 2, x + 7, y + h // 2 + 1, ORANGE)
    rect(img, 0, 36, 17, 43, DISP); hline(img, 0, 17, 39, DISP_DIM)
    for x in (17, 20, 23):
        rect(img, x, 36, x + 3, 43, ORANGE)
    return img


# ---------------------------------------------------------------------------- CBUTTONS
def make_cbuttons(main):
    img = new(136, 36)
    spec = [(0, 23, 18, 16, 88, g_prev), (23, 23, 18, 39, 88, g_play), (46, 23, 18, 62, 88, g_pause),
            (69, 23, 18, 85, 88, g_stop), (92, 22, 18, 108, 88, g_next)]
    for sx, w, h, tx, ty, g in spec:
        for row, pressed in ((0, False), (18, True)):
            cell = crop(main, tx, ty, w, h)
            button(cell, 0.5, 0.5, w - 1, h - 1, pressed, glyph=g)
            paste(img, cell, sx, row)
    for row, pressed in ((0, False), (16, True)):
        cell = crop(main, 136, 89, 22, 16)
        button(cell, 0.5, 0.5, 21, 15, pressed, glyph=g_eject)
        paste(img, cell, 114, row)
    return img


# ---------------------------------------------------------------------------- SHUFREP
def make_shufrep(main):
    img = new(92, 85)
    for row, (sel, pressed) in enumerate([(False, False), (False, True), (True, False), (True, True)]):
        y = row * 15
        cell = crop(main, 164, 89, 47, 15)
        button(cell, 0, 0, 46, 15, pressed, label="SHUFFLE", sel=sel, cap=4.5)
        paste(img, cell, 28, y)
        cell = crop(main, 210, 89, 28, 15)
        button(cell, 1, 0, 27, 15, pressed, label="REP", sel=sel, cap=4.5)
        paste(img, cell, 0, y)
    for lab, sx, tx in (("EQ", 0, 219), ("PL", 23, 242)):
        for (ox, oy, sel, pressed) in ((0, 61, False, False), (0, 73, True, False), (46, 61, False, True), (46, 73, True, True)):
            cell = crop(main, tx, 58, 23, 12)
            button(cell, 0, 0, 23, 12, pressed, label=lab, sel=sel, cap=5)
            paste(img, cell, sx + ox, oy)
    return img


# ---------------------------------------------------------------------------- LCD sprites
def make_numbers(ex=False):
    img = new(108 if ex else 99, 13, DISP)
    f = ttf("consolab.ttf", 15.5 * G.K)
    d = ImageDraw.Draw(img)
    K = G.K
    for i in range(10):
        d.text(((i * 9 + 4.5) * K, 6.6 * K), str(i), font=f, fill=ORANGE, anchor="mm")
    if ex:
        rect(img, 99 + 1.5, 5.8, 99 + 7.5, 7.6, ORANGE)
    return img


def make_text():
    K = G.K
    img = new(155, 18, DISP)
    f = ttf("consolab.ttf", 6.6 * K)
    for r, line in enumerate(G.TEXT_ROWS):
        for c, ch in enumerate(line[:31]):
            if ch == " ":
                continue
            cell = Image.new("RGB", (5 * K, 6 * K), DISP)
            ImageDraw.Draw(cell).text((2.5 * K, 5.1 * K), ch, font=f, fill=WHITE, anchor="ms")
            img.paste(cell, (c * 5 * K, r * 6 * K))
    return img


def make_monoster():
    img = new(58, 24, DISP)
    for y, col in ((3, WHITE), (15, DISP_GHOST)):
        text(img, 14.5, y + 0.5, "STEREO", col, cap=5, center=True, maxw=27)
        text(img, 42.5, y + 0.5, "MONO", col, cap=5, center=True, maxw=25)
    return img


def make_playpaus():
    img = new(42, 9, DISP)
    poly(img, [(3, 1), (3, 8), (7.5, 4.5)], WHITE)
    rect(img, 11, 1, 13, 8, WHITE); rect(img, 15, 1, 17, 8, WHITE)
    rect(img, 20, 1, 26, 7, WHITE)
    G.ellipse(img, 39, 3, 41.5, 5.5, ORANGE)
    return img


def prop_knob(img, x, y, w, h, pressed):
    bevel(img, x, y, x + w, y + h, raised=not pressed, fill=WBBLUE if pressed else GREY)


def make_volume(main, balance=False):
    img = new(68, 433)
    tx, w = (177, 38) if balance else (107, 68)
    for i in range(28):
        cell = crop(main, tx, 57, w, 13)
        bevel(cell, 0, 3, w, 10, raised=False, fill=DKGREY)
        if balance:
            half = (w / 2 - 2) * i / 27
            if half > 0.3:
                rect(cell, w / 2 - half, 4, w / 2 + half, 9, BLUE)
            rect(cell, w / 2 - 0.5, 4, w / 2 + 0.5, 9, WHITE)
            paste(img, cell, 9, i * 15)
        else:
            rect(cell, 1, 4, 1 + (w - 2) * i / 27, 9, BLUE)
            paste(img, cell, 0, i * 15)
    for x, pressed in ((15, False), (0, True)):
        cell = crop(main, 140, 58, 14, 11)
        prop_knob(cell, 0, 0, 14, 11, pressed)
        rect(cell, 6, 3, 7, 8, BLACK); rect(cell, 7, 3, 8, 8, WHITE)
        paste(img, cell, x, 422)
    return img


def make_posbar(main):
    img = new(307, 10)
    bg = crop(main, 16, 72, 248, 10)
    rect(bg, 0, 0, 248, 10, DKGREY)
    paste(img, bg, 0, 0)
    for x, pressed in ((248, False), (278, True)):
        cell = crop(main, 100, 72, 29, 10)
        prop_knob(cell, 0, 0, 29, 10, pressed)
        for xx in (12, 14, 16):
            rect(cell, xx, 2.5, xx + 0.8, 7.5, BLACK)
        paste(img, cell, x, 0)
    return img


# ---------------------------------------------------------------------------- EQMAIN
EQ_LABELS = ["60", "170", "310", "600", "1K", "3K", "6K", "12K", "14K", "16K"]


def eq_title(active):
    img = title_bar(active, "Equalizer")
    return img


def make_eqmain():
    img = new(275, 315)
    bg = new(275, 116)
    bevel(bg, 0, 0, 275, 116, raised=True, fill=None)
    bevel(bg, 5, 15, 270, 113, raised=False, fill=None)
    bevel(bg, 6, 16, 269, 112, raised=True, fill=None)
    paste(bg, eq_title(True), 0, 0)
    for txt, y in (("+12 dB", 41), ("0 dB", 67), ("-12 dB", 92)):
        text(bg, 56, y, txt, BLACK, cap=4, center=True)
    for y in (43, 69, 95):
        rect(bg, 40, y, 45, y + 0.6, BLACK); rect(bg, 67, y, 72, y + 0.6, BLACK)
    text(bg, 28, 104, "PREAMP", BLACK, cap=4, center=True)
    for i, lbl in enumerate(EQ_LABELS):
        text(bg, 78 + i * 18 + 7.5, 104, lbl, BLACK, cap=4, center=True)
    paste(img, bg, 0, 0)
    paste(img, eq_title(True), 0, 134)
    paste(img, eq_title(False), 0, 149)
    for y, pressed in ((116, False), (125, True)):
        cell = new(9, 9)
        gadget(cell, 0, 0, "close", pressed)
        paste(img, cell, 0, y)
    cell = new(9, 9)
    gadget(cell, 0, 0, "zoom", False)
    paste(img, cell, 254, 152)
    for name, w, xs, tx in (("ON", 26, (10, 128, 69, 187), 14), ("AUTO", 32, (36, 154, 95, 213), 40)):
        for x, sel, pressed in zip(xs, (False, False, True, True), (False, True, False, True)):
            cell = crop(bg, tx, 18, w, 12)
            button(cell, 0, 0, w, 12, pressed, label=name, sel=sel, cap=5)
            paste(img, cell, x, 119)
    for n in range(28):
        p = n / 27.0
        cell = new(15, 65)
        paste(cell, crop(bg, 78, 38, 14, 63), 0, 0)
        bevel(cell, 3, 0, 11, 63, raised=False, fill=DKGREY)
        center = 31.5
        pos = 5.5 + (1 - p) * 51
        y0, y1 = sorted((center, pos))
        if y1 - y0 > 0.3:
            rect(cell, 4, y0, 10, y1, BLUE)
        rect(cell, 4, center - 0.3, 10, center + 0.3, BLACK)
        paste(img, cell, 13 + (n % 14) * 15, 164 + (n // 14) * 65)
    for y, pressed in ((164, False), (176, True)):
        cell = crop(bg, 79, 60, 11, 11)
        prop_knob(cell, 0, 0, 11, 11, pressed)
        rect(cell, 2.5, 5, 8.5, 5.8, BLACK); rect(cell, 2.5, 5.8, 8.5, 6.4, WHITE)
        paste(img, cell, 0, y)
    for y, pressed in ((164, False), (176, True)):
        cell = crop(bg, 217, 18, 44, 12)
        button(cell, 0, 0, 44, 12, pressed, label="Presets", cap=5)
        paste(img, cell, 224, y)
    g = new(113, 19, DISP)
    for x in range(0, 113, 12):
        rect(g, x, 0, x + 0.4, 19, DISP_GHOST)
    rect(g, 0, 9.3, 113, 9.7, DISP_GHOST)
    paste(img, g, 0, 294)
    for y in range(19):
        rect(img, 115, 294 + y, 116, 295 + y, G.lerp(ORANGE, (255, 220, 120), abs(y - 9) / 9))
    rect(img, 0, 314, 113, 315, DISP)
    for x in range(0, 113, 2):
        rect(img, x, 314, x + 1, 315, DISP_DIM)
    return img


# ---------------------------------------------------------------------------- PLEDIT
def make_pledit():
    img = new(280, 186)

    def top_piece(w, active, kind):
        p = new(w, 20)
        fill = WBBLUE if active else GREY
        rect(p, 0, 0, w, 14, fill)
        rect(p, 0, 0, w, 1, WHITE); rect(p, 0, 13, w, 14, BLACK)
        rect(p, 0, 14, w, 20, GREY)
        rect(p, 0, 19, w, 20, BLACK)  # top edge of the recessed list
        if kind == "left":
            rect(p, 0, 0, 1, 20, WHITE)
            rect(p, 11, 19, 25, 20, BLACK)
        if kind == "right":
            rect(p, w - 1, 0, w, 20, BLACK)
            rect(p, w - 23, 1, w - 22, 13, BLACK); rect(p, w - 22, 1, w - 21, 13, WHITE)
            gadget(p, 5, 3, "zoom", False)
            gadget(p, 14, 3, "close", False)
        if kind == "title":
            text(p, w / 2, 3.5, "Playlist", BLACK, cap=6, center=True)
        return p

    for active, y in ((True, 0), (False, 21)):
        paste(img, top_piece(25, active, "left"), 0, y)
        paste(img, top_piece(100, active, "title"), 26, y)
        paste(img, top_piece(25, active, "tile"), 127, y)
        paste(img, top_piece(25, active, "right"), 153, y)
    lt = new(12, 29)
    rect(lt, 0, 0, 1, 29, WHITE); rect(lt, 11, 0, 12, 29, BLACK)
    paste(img, lt, 0, 42)
    rt = new(20, 29)
    rect(rt, 0, 0, 1, 29, WHITE)  # bottom/right highlight of the recessed list
    rect(rt, 19, 0, 20, 29, BLACK)
    rect(rt, 4, 0, 5, 29, BLACK); rect(rt, 13, 0, 14, 29, WHITE)
    rect(rt, 5, 0, 13, 29, DKGREY)
    paste(img, rt, 31, 42)
    for x, pressed in ((52, False), (61, True)):
        cell = new(8, 18, DKGREY)
        prop_knob(cell, 0, 0, 8, 18, pressed)
        for yy in (7, 9, 11):
            rect(cell, 2, yy, 6, yy + 0.7, BLACK)
        paste(img, cell, x, 53)
    for x, kind in ((52, "close"), (62, "zoom"), (150, "unzoom")):
        cell = new(9, 9)
        gadget(cell, 0, 0, kind, True)
        paste(img, cell, x, 42)
    for (x, y, w) in ((72, 57, 25), (72, 42, 25), (99, 57, 50), (99, 42, 50)):
        paste(img, new(w, 14, WBBLUE), x, y)

    def bottom_base(w):
        b = new(w, 38)
        rect(b, 0, 0, w, 1, WHITE)   # bottom edge of the recessed list
        rect(b, 0, 37, w, 38, BLACK)
        return b

    paste(img, bottom_base(25), 179, 0)
    bl = bottom_base(125)
    rect(bl, 0, 0, 1, 38, WHITE)
    for bx, g in ((14, "+"), (43, "-"), (72, "SEL"), (101, "MISC")):
        if len(g) == 1:
            button(bl, bx, 8, 25, 18, False, label=g, cap=6)
        else:
            button(bl, bx, 8, 25, 18, False, label=g, cap=4.5)
    paste(img, bl, 0, 72)
    br = bottom_base(150)
    rect(br, 149, 0, 150, 38, BLACK)
    bevel(br, 2, 4, 101, 35, raised=False, fill=DISP)
    c = WHITE
    oy = 22
    rect(br, 6, oy + 1, 7, oy + 8, c); poly(br, [(11.5, oy + 1), (11.5, oy + 8), (7.5, oy + 4.5)], c)
    poly(br, [(15, oy + 1), (15, oy + 8), (19, oy + 4.5)], c)
    rect(br, 25, oy + 1, 27, oy + 8, c); rect(br, 29, oy + 1, 31, oy + 8, c)
    rect(br, 34, oy + 2, 40, oy + 8, c)
    poly(br, [(43, oy + 1), (43, oy + 8), (47, oy + 4.5)], c); rect(br, 47, oy + 1, 48, oy + 8, c)
    poly(br, [(51, oy + 5), (59, oy + 5), (55, oy + 1)], c); rect(br, 51, oy + 6, 59, oy + 8, c)
    button(br, 103, 8, 25, 18, False, label="LIST", cap=4.5)
    poly(br, [(132, 8), (139, 8), (135.5, 4)], BLACK)
    poly(br, [(132, 11), (139, 11), (135.5, 15)], BLACK)
    for i in range(3):
        rect(br, 141 + i * 3, 36 - (i + 1) * 3, 142 + i * 3, 36, BLACK)
    paste(img, br, 126, 72)
    vb = bottom_base(75)
    bevel(vb, 2, 6, 73, 32, raised=False, fill=DISP)
    boing_ball(vb, 37.5, 19, 9)
    paste(img, vb, 205, 0)
    menus = [
        (0, ["URL", "DIR", "FILE"]),
        (54, ["ALL", "CROP", "SEL", "MISC"]),
        (104, ["INV", "NONE", "ALL"]),
        (154, ["SORT", "INFO", "OPTS"]),
        (204, ["NEW", "SAVE", "LOAD"]),
    ]
    for x, items in menus:
        for i, lab in enumerate(items):
            y = 111 + i * 19
            for sel in (False, True):
                cell = new(22, 18)
                button(cell, 0, 0, 22, 18, False, label=lab, sel=sel, cap=4.5)
                paste(img, cell, x + (23 if sel else 0), y)
    for x, h in ((48, 54), (100, 72), (150, 54), (200, 54), (250, 54)):
        bevel(img, x, 111, x + 3, 111 + h, raised=True, fill=BEIGE)
    return img


# ---------------------------------------------------------------------------- build
def hexc(c):
    return "#%02X%02X%02X" % c


def build():
    G.K = 4
    main = make_main()
    files = {
        "main": main, "titlebar": make_titlebar(), "cbuttons": make_cbuttons(main), "shufrep": make_shufrep(main),
        "numbers": make_numbers(), "nums_ex": make_numbers(ex=True), "text": make_text(),
        "monoster": make_monoster(), "playpaus": make_playpaus(), "volume": make_volume(main),
        "balance": make_volume(main, balance=True), "posbar": make_posbar(main), "eqmain": make_eqmain(),
        "pledit": make_pledit(),
    }
    vis = [DISP, DISP_GHOST]
    for i in range(16):
        vis.append(G.lerp((255, 255, 255), ORANGE, min(1, i / 9)) if i < 10 else G.lerp(ORANGE, (200, 60, 0), (i - 9) / 6))
    for i in range(5):
        vis.append(G.lerp(WHITE, ORANGE, i / 5))
    vis.append(WHITE)
    texts = {
        "pledit.txt": "[Text]\r\nNormal=%s\r\nCurrent=%s\r\nNormalBG=%s\r\nSelectedBG=%s\r\nFont=Verdana\r\n"
                      % (hexc(BLACK), hexc(WHITE), hexc(GREY), hexc(WBBLUE)),
        "viscolor.txt": "".join("%d,%d,%d,\r\n" % c for c in vis),
        "readme.txt": "Amiga Workbench - RetroAmp HD skin (Workbench 3.0 / MagicWB style, Boing Ball)\r\n",
    }
    root = G.ROOT
    out = os.path.join(root, "skins", "src", "AmigaWorkbench")
    if os.path.isdir(out):
        shutil.rmtree(out)
    os.makedirs(out)
    for n, im in files.items():
        im.save(os.path.join(out, n + ".png"), optimize=True)
    for n, t in texts.items():
        with open(os.path.join(out, n), "w", newline="") as f:
            f.write(t)
    with zipfile.ZipFile(os.path.join(root, "skins", "AmigaWorkbench.wsz"), "w", zipfile.ZIP_DEFLATED) as z:
        for n in os.listdir(out):
            z.write(os.path.join(out, n), n)
    print("ok")


if __name__ == "__main__":
    build()
