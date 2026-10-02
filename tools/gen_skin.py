"""
Generates RetroAmp skins in the classic Winamp 2.x sprite layout.

Two flavours are produced for every theme:
  * HD   (K=4): every sprite sheet is 4x larger, drawn as smooth vector shapes with
               TrueType text. RetroAmp detects the size and scales it with a high
               quality filter, so the player stays sharp at any zoom (100%-400%).
               -> skins/<Name>.wsz  (PNG files)  and res/skin/*.png (embedded default)
  * classic (K=1): pixel-exact Winamp 2 skin, loadable by Winamp / Webamp too.
               -> skins/classic/<Name>.wsz

Usage:  python tools/gen_skin.py
"""
import math
import os
import shutil
import zipfile
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SS = 4  # supersampling factor for anti-aliased shapes
RES = Image.Resampling.LANCZOS
FONTDIR = os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts")
K = 1  # pixels per skin pixel for the skin being generated

_font_cache = {}


def ttf(name, size, variation=None):
    key = (name, size, variation)
    if key not in _font_cache:
        f = ImageFont.truetype(os.path.join(FONTDIR, name), max(1, int(size)))
        if variation:
            try:
                f.set_variation_by_name(variation)
            except Exception:
                pass
        _font_cache[key] = f
    return _font_cache[key]


def S(v):
    return int(round(v * K))


# --------------------------------------------------------------------------- helpers
def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def stops_color(stops, t):
    if isinstance(stops[0], int):  # single colour
        return tuple(stops)
    if len(stops) == 2 and not isinstance(stops[0][0], float):
        return lerp(stops[0], stops[1], t)
    for i in range(len(stops) - 1):
        t0, c0 = stops[i]
        t1, c1 = stops[i + 1]
        if t <= t1:
            return lerp(c0, c1, 0 if t1 == t0 else (t - t0) / (t1 - t0))
    return stops[-1][1]


def new(w, h, color=(0, 0, 0)):
    return Image.new("RGB", (S(w), S(h)), color)


def _vgrad_px(W, H, stops):
    col = Image.new("RGB", (1, max(H, 1)))
    for y in range(H):
        col.putpixel((0, y), stops_color(stops, y / (H - 1) if H > 1 else 0))
    return col.resize((max(W, 1), max(H, 1)), Image.Resampling.NEAREST)


def vgrad(w, h, stops):
    return _vgrad_px(S(w), S(h), stops)


def hgrad(w, h, stops):
    W, H = S(w), S(h)
    row = Image.new("RGB", (max(W, 1), 1))
    for x in range(W):
        row.putpixel((x, 0), stops_color(stops, x / (W - 1) if W > 1 else 0))
    return row.resize((max(W, 1), max(H, 1)), Image.Resampling.NEAREST)


def _rr_mask_px(W, H, R):
    m = Image.new("L", (W * SS, H * SS), 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, W * SS - 1, H * SS - 1], radius=max(0, R * SS), fill=255)
    return m.resize((W, H), RES)


def _ell_mask_px(W, H):
    m = Image.new("L", (W * SS, H * SS), 0)
    ImageDraw.Draw(m).ellipse([0, 0, W * SS - 1, H * SS - 1], fill=255)
    return m.resize((W, H), RES)


def ell_mask(w, h):
    return _ell_mask_px(S(w), S(h))


def _fill_px(W, H, fill):
    if isinstance(fill, Image.Image):
        return fill
    if isinstance(fill, tuple) and isinstance(fill[0], int):
        return Image.new("RGB", (W, H), fill)
    return _vgrad_px(W, H, fill)


def rrect(base, x0, y0, x1, y1, r, fill, outline=None, ow=1):
    """Rounded rect in skin pixels, (x1,y1) exclusive."""
    X0, Y0, X1, Y1 = S(x0), S(y0), S(x1), S(y1)
    W, H, R = X1 - X0, Y1 - Y0, r * K
    if W <= 0 or H <= 0:
        return
    if outline is not None:
        OW = max(1, S(ow))
        base.paste(Image.new("RGB", (W, H), outline), (X0, Y0), _rr_mask_px(W, H, R))
        X0 += OW; Y0 += OW; W -= 2 * OW; H -= 2 * OW; R = max(0, R - OW)
        if W <= 0 or H <= 0:
            return
    base.paste(_fill_px(W, H, fill), (X0, Y0), _rr_mask_px(W, H, R))


def ellipse(base, x0, y0, x1, y1, fill, outline=None, ow=1):
    X0, Y0, X1, Y1 = S(x0), S(y0), S(x1), S(y1)
    W, H = X1 - X0, Y1 - Y0
    if outline is not None:
        OW = max(1, S(ow))
        base.paste(Image.new("RGB", (W, H), outline), (X0, Y0), _ell_mask_px(W, H))
        X0 += OW; Y0 += OW; W -= 2 * OW; H -= 2 * OW
    base.paste(_fill_px(W, H, fill), (X0, Y0), _ell_mask_px(W, H))


def poly(base, pts, color, ox=0, oy=0):
    """Anti-aliased polygon; pts in (float) skin pixels."""
    pts = [(x * K, y * K) for x, y in pts]
    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
    bx0, by0 = int(min(xs)) - 1, int(min(ys)) - 1
    bx1, by1 = int(max(xs)) + 2, int(max(ys)) + 2
    w, h = bx1 - bx0, by1 - by0
    m = Image.new("L", (w * SS, h * SS), 0)
    ImageDraw.Draw(m).polygon([((x - bx0) * SS, (y - by0) * SS) for x, y in pts], fill=255)
    m = m.resize((w, h), RES)
    base.paste(Image.new("RGB", (w, h), color), (bx0 + S(ox), by0 + S(oy)), m)


def thick_line(base, x0, y0, x1, y1, t, color):
    dx, dy = x1 - x0, y1 - y0
    L = math.hypot(dx, dy) or 1
    nx, ny = -dy / L * t / 2, dx / L * t / 2
    poly(base, [(x0 + nx, y0 + ny), (x1 + nx, y1 + ny), (x1 - nx, y1 - ny), (x0 - nx, y0 - ny)], color)


def rect(base, x0, y0, x1, y1, color):
    """Solid rect in skin pixels, (x1,y1) exclusive."""
    X0, Y0, X1, Y1 = S(x0), S(y0), S(x1), S(y1)
    if X1 <= X0 and x1 > x0:
        X1 = X0 + 1
    if Y1 <= Y0 and y1 > y0:
        Y1 = Y0 + 1
    if X1 > X0 and Y1 > Y0:
        base.paste(Image.new("RGB", (X1 - X0, Y1 - Y0), color), (X0, Y0))


def putpx(base, x, y, color):
    rect(base, x, y, x + 1, y + 1, color)


def hline(base, x0, x1, y, color):
    rect(base, x0, y, x1, y + 1, color)


def vline(base, x, y0, y1, color):
    rect(base, x, y0, x + 1, y1, color)


def dotted_v(base, x, y0, y1, color, step=2):
    d = 1 if K == 1 else 0.6
    for y in range(y0, y1, step):
        rect(base, x, y, x + d, y + d, color)


def dotted_h(base, x0, x1, y, color, step=2):
    d = 1 if K == 1 else 0.6
    for x in range(x0, x1, step):
        rect(base, x, y, x + d, y + d, color)


def crop(img, x, y, w, h):
    return img.crop((S(x), S(y), S(x) + S(w), S(y) + S(h))).copy()


def paste(dst, src, x, y):
    dst.paste(src, (S(x), S(y)))


def ttf_text(base, x, y, text, font, color, shadow=None, anchor="lt"):
    """x, y in skin pixels; font already sized in physical pixels."""
    d = ImageDraw.Draw(base)
    if shadow is not None:
        d.text((S(x) + max(1, K // 2), S(y) + max(1, K // 2)), text, font=font, fill=shadow, anchor=anchor)
    d.text((S(x), S(y)), text, font=font, fill=color, anchor=anchor)


# TrueType "LCD" label font used for the HD skins
def lcd_font(cap_h, bold=False):
    return ttf("bahnschrift.ttf", cap_h * K / 0.70, "Bold Condensed" if bold else "SemiBold Condensed")


# --------------------------------------------------------------------------- pixel fonts (classic skins)
# 5x6 font (Winamp TEXT.BMP), glyphs up to 5 wide x 5 tall
F5 = {
    "A": [".##.", "#..#", "####", "#..#", "#..#"],
    "B": ["###.", "#..#", "###.", "#..#", "###."],
    "C": [".###", "#...", "#...", "#...", ".###"],
    "D": ["###.", "#..#", "#..#", "#..#", "###."],
    "E": ["####", "#...", "###.", "#...", "####"],
    "F": ["####", "#...", "###.", "#...", "#..."],
    "G": [".###", "#...", "#.##", "#..#", ".###"],
    "H": ["#..#", "#..#", "####", "#..#", "#..#"],
    "I": ["###.", ".#..", ".#..", ".#..", "###."],
    "J": ["..##", "...#", "...#", "#..#", ".##."],
    "K": ["#..#", "#.#.", "##..", "#.#.", "#..#"],
    "L": ["#...", "#...", "#...", "#...", "####"],
    "M": ["#...#", "##.##", "#.#.#", "#...#", "#...#"],
    "N": ["#..#", "##.#", "#.##", "#..#", "#..#"],
    "O": [".##.", "#..#", "#..#", "#..#", ".##."],
    "P": ["###.", "#..#", "###.", "#...", "#..."],
    "Q": [".##.", "#..#", "#..#", "#.#.", ".#.#"],
    "R": ["###.", "#..#", "###.", "#.#.", "#..#"],
    "S": [".###", "#...", ".##.", "...#", "###."],
    "T": ["###.", ".#..", ".#..", ".#..", ".#.."],
    "U": ["#..#", "#..#", "#..#", "#..#", ".##."],
    "V": ["#...#", "#...#", ".#.#.", ".#.#.", "..#.."],
    "W": ["#...#", "#...#", "#.#.#", "##.##", "#...#"],
    "X": ["#..#", "#..#", ".##.", "#..#", "#..#"],
    "Y": ["#.#.", "#.#.", ".#..", ".#..", ".#.."],
    "Z": ["####", "...#", ".##.", "#...", "####"],
    "0": [".##.", "#..#", "#..#", "#..#", ".##."],
    "1": [".#..", "##..", ".#..", ".#..", "###."],
    "2": ["###.", "...#", ".##.", "#...", "####"],
    "3": ["###.", "...#", ".##.", "...#", "###."],
    "4": ["#..#", "#..#", "####", "...#", "...#"],
    "5": ["####", "#...", "###.", "...#", "###."],
    "6": [".##.", "#...", "###.", "#..#", ".##."],
    "7": ["####", "...#", "..#.", ".#..", ".#.."],
    "8": [".##.", "#..#", ".##.", "#..#", ".##."],
    "9": [".##.", "#..#", ".###", "...#", ".##."],
    '"': ["#.#.", "#.#.", "....", "....", "...."],
    "@": [".##.", "#.##", "#.##", "#...", ".##."],
    "…": [".....", ".....", ".....", ".....", "#.#.#"],
    ".": ["....", "....", "....", "....", ".#.."],
    ":": ["....", ".#..", "....", ".#..", "...."],
    "(": ["..#.", ".#..", ".#..", ".#..", "..#."],
    ")": [".#..", "..#.", "..#.", "..#.", ".#.."],
    "-": ["....", "....", "###.", "....", "...."],
    "'": [".#..", ".#..", "....", "....", "...."],
    "!": [".#..", ".#..", ".#..", "....", ".#.."],
    "_": ["....", "....", "....", "....", "####"],
    "+": ["....", ".#..", "###.", ".#..", "...."],
    "\\": ["#...", ".#..", ".#..", "..#.", "...#"],
    "/": ["...#", "..#.", ".#..", ".#..", "#..."],
    "[": [".##.", ".#..", ".#..", ".#..", ".##."],
    "]": [".##.", "..#.", "..#.", "..#.", ".##."],
    "^": [".#..", "#.#.", "....", "....", "...."],
    "&": [".#..", "#.#.", ".#..", "#.#.", ".#.#"],
    "%": ["#..#", "..#.", ".#..", "#...", "#..#"],
    ",": ["....", "....", "....", ".#..", "#..."],
    "=": ["....", "###.", "....", "###.", "...."],
    "$": [".###", "##..", ".##.", "..##", "###."],
    "#": [".#.#", "####", ".#.#", "####", ".#.#"],
    "Å": [".##.", ".##.", "#..#", "####", "#..#"],
    "Ö": ["#..#", ".##.", "#..#", "#..#", ".##."],
    "Ä": ["#..#", ".##.", "#..#", "####", "#..#"],
    "?": ["###.", "...#", ".##.", "....", ".#.."],
    "*": ["....", "#.#.", ".#..", "#.#.", "...."],
    " ": ["....", "....", "....", "....", "...."],
}

# 3x5 font for small labels baked into backgrounds
F3 = {
    "A": [".#.", "#.#", "###", "#.#", "#.#"], "B": ["##.", "#.#", "##.", "#.#", "##."],
    "C": [".##", "#..", "#..", "#..", ".##"], "D": ["##.", "#.#", "#.#", "#.#", "##."],
    "E": ["###", "#..", "##.", "#..", "###"], "F": ["###", "#..", "##.", "#..", "#.."],
    "G": [".##", "#..", "#.#", "#.#", ".##"], "H": ["#.#", "#.#", "###", "#.#", "#.#"],
    "I": ["###", ".#.", ".#.", ".#.", "###"], "J": ["..#", "..#", "..#", "#.#", ".#."],
    "K": ["#.#", "#.#", "##.", "#.#", "#.#"], "L": ["#..", "#..", "#..", "#..", "###"],
    "M": ["#.#", "###", "###", "#.#", "#.#"], "N": ["##.", "#.#", "#.#", "#.#", "#.#"],
    "O": ["###", "#.#", "#.#", "#.#", "###"], "P": ["##.", "#.#", "##.", "#..", "#.."],
    "Q": ["###", "#.#", "#.#", "###", "..#"], "R": ["##.", "#.#", "##.", "#.#", "#.#"],
    "S": [".##", "#..", ".#.", "..#", "##."], "T": ["###", ".#.", ".#.", ".#.", ".#."],
    "U": ["#.#", "#.#", "#.#", "#.#", "###"], "V": ["#.#", "#.#", "#.#", "#.#", ".#."],
    "W": ["#.#", "#.#", "###", "###", "#.#"], "X": ["#.#", "#.#", ".#.", "#.#", "#.#"],
    "Y": ["#.#", "#.#", ".#.", ".#.", ".#."], "Z": ["###", "..#", ".#.", "#..", "###"],
    "0": ["###", "#.#", "#.#", "#.#", "###"], "1": [".#.", "##.", ".#.", ".#.", "###"],
    "2": ["##.", "..#", ".#.", "#..", "###"], "3": ["##.", "..#", ".#.", "..#", "##."],
    "4": ["#.#", "#.#", "###", "..#", "..#"], "5": ["###", "#..", "##.", "..#", "##."],
    "6": [".##", "#..", "###", "#.#", "###"], "7": ["###", "..#", ".#.", ".#.", ".#."],
    "8": ["###", "#.#", "###", "#.#", "###"], "9": ["###", "#.#", "###", "..#", "##."],
    "+": ["...", ".#.", "###", ".#.", "..."], "-": ["...", "...", "###", "...", "..."],
    ".": ["...", "...", "...", "...", ".#."], " ": ["...", "...", "...", "...", "..."],
    "&": [".#.", "#.#", ".#.", "#.#", ".##"], "/": ["..#", "..#", ".#.", "#..", "#.."],
}


def glyph_width(text, font=F3, spacing=1, bold=False):
    w = 0
    for ch in text.upper():
        g = font.get(ch, font[" "])
        w += len(g[0]) + spacing + (1 if bold else 0)
    return w - spacing


# --------------------------------------------------------------------------- themes
THEMES = {
    "RetroBlue": dict(
        name="Retro Blue",
        title="RETROAMP",
        body=[(0.0, (232, 235, 244)), (0.5, (196, 202, 218)), (1.0, (150, 157, 180))],
        edge=(58, 64, 92),
        hi=(250, 251, 255),
        panel=[(0.0, (226, 230, 240)), (1.0, (178, 185, 204))],
        panel_edge=(122, 130, 156),
        lcd=(30, 52, 132),
        lcd_edge=(12, 20, 64),
        lcd_text=(226, 234, 255),
        lcd_dim=(66, 88, 160),
        lcd_ghost=(40, 64, 146),
        tbar=[(0.0, (74, 92, 152)), (0.5, (40, 54, 108)), (1.0, (26, 36, 80))],
        tbar_in=[(0.0, (110, 118, 146)), (1.0, (70, 78, 108))],
        groove_hi=(182, 194, 226),
        groove_lo=(16, 22, 56),
        ttext=(226, 232, 248),
        ttext_in=(170, 176, 196),
        btn=[(0.0, (255, 255, 255)), (0.55, (214, 219, 232)), (1.0, (150, 158, 182))],
        btn_dn=[(0.0, (150, 158, 182)), (1.0, (214, 219, 232))],
        btn_ring=(46, 52, 78),
        glyph=(28, 32, 52),
        accent=(78, 128, 232),
        accent_hi=(150, 190, 255),
        led_on=[(0.0, (190, 220, 255)), (1.0, (60, 120, 240))],
        led_off=[(0.0, (180, 186, 204)), (1.0, (120, 128, 152))],
        groove=(54, 62, 92),
        list_bg=(30, 52, 132),
        list_text=(196, 210, 242),
        list_cur=(255, 255, 255),
        list_sel=(82, 112, 196),
        eq_label_bg=(140, 148, 172),
        eq_label=(250, 252, 255),
        vis_top=(255, 255, 255),
        vis_bot=(84, 140, 240),
        bolt=((255, 220, 60), (230, 90, 20)),
    ),
    "SteelGreen": dict(
        name="Steel Green",
        title="RETROAMP",
        body=[(0.0, (206, 208, 210)), (0.5, (170, 172, 176)), (1.0, (120, 122, 128))],
        edge=(40, 42, 46),
        hi=(240, 240, 242),
        panel=[(0.0, (196, 198, 202)), (1.0, (150, 152, 158))],
        panel_edge=(96, 98, 104),
        lcd=(10, 16, 10),
        lcd_edge=(0, 0, 0),
        lcd_text=(80, 255, 110),
        lcd_dim=(20, 90, 34),
        lcd_ghost=(16, 34, 18),
        tbar=[(0.0, (110, 112, 118)), (0.5, (66, 68, 74)), (1.0, (44, 46, 52))],
        tbar_in=[(0.0, (130, 132, 136)), (1.0, (96, 98, 102))],
        groove_hi=(170, 172, 180),
        groove_lo=(20, 20, 24),
        ttext=(236, 240, 236),
        ttext_in=(180, 182, 186),
        btn=[(0.0, (246, 246, 246)), (0.55, (200, 202, 206)), (1.0, (128, 130, 136))],
        btn_dn=[(0.0, (128, 130, 136)), (1.0, (200, 202, 206))],
        btn_ring=(34, 36, 40),
        glyph=(24, 26, 30),
        accent=(60, 200, 90),
        accent_hi=(150, 255, 170),
        led_on=[(0.0, (190, 255, 200)), (1.0, (40, 190, 70))],
        led_off=[(0.0, (160, 162, 166)), (1.0, (100, 102, 108))],
        groove=(40, 42, 48),
        list_bg=(0, 0, 0),
        list_text=(0, 230, 70),
        list_cur=(255, 255, 255),
        list_sel=(0, 70, 150),
        eq_label_bg=(96, 98, 104),
        eq_label=(230, 255, 230),
        vis_top=(255, 60, 40),
        vis_bot=(40, 210, 60),
        bolt=((255, 220, 60), (230, 90, 20)),
    ),
    "CrimsonNight": dict(
        name="Crimson Night",
        title="RETROAMP",
        body=[(0.0, (74, 70, 78)), (0.5, (44, 40, 48)), (1.0, (24, 22, 28))],
        edge=(8, 6, 10),
        hi=(110, 104, 116),
        panel=[(0.0, (64, 58, 68)), (1.0, (34, 30, 38))],
        panel_edge=(14, 12, 16),
        lcd=(24, 4, 10),
        lcd_edge=(0, 0, 0),
        lcd_text=(255, 96, 110),
        lcd_dim=(110, 30, 44),
        lcd_ghost=(46, 10, 20),
        tbar=[(0.0, (150, 24, 44)), (0.5, (96, 12, 28)), (1.0, (54, 6, 16))],
        tbar_in=[(0.0, (84, 70, 78)), (1.0, (50, 42, 48))],
        groove_hi=(220, 110, 120),
        groove_lo=(20, 2, 6),
        ttext=(255, 228, 232),
        ttext_in=(170, 150, 156),
        btn=[(0.0, (120, 114, 124)), (0.55, (70, 64, 74)), (1.0, (34, 30, 38))],
        btn_dn=[(0.0, (30, 26, 34)), (1.0, (80, 72, 84))],
        btn_ring=(6, 4, 8),
        glyph=(255, 120, 132),
        accent=(220, 30, 60),
        accent_hi=(255, 140, 150),
        led_on=[(0.0, (255, 190, 196)), (1.0, (220, 20, 50))],
        led_off=[(0.0, (90, 80, 88)), (1.0, (44, 38, 44))],
        groove=(10, 8, 12),
        list_bg=(16, 4, 8),
        list_text=(236, 120, 130),
        list_cur=(255, 255, 255),
        list_sel=(120, 20, 44),
        eq_label_bg=(90, 16, 32),
        eq_label=(255, 220, 224),
        vis_top=(255, 240, 200),
        vis_bot=(200, 20, 50),
        bolt=((255, 220, 60), (230, 40, 40)),
    ),
}




# --------------------------------------------------------------------------- text helpers (K aware)
def glyph_text(base, x, y, text, color, font=F5, spacing=1, bold=False):
    cx = x
    for ch in text.upper():
        g = font.get(ch, font[" "])
        for gy, row in enumerate(g):
            for gx, c in enumerate(row):
                if c == "#":
                    putpx(base, cx + gx, y + gy, color)
                    if bold:
                        putpx(base, cx + gx + 1, y + gy, color)
        cx += len(g[0]) + spacing + (1 if bold else 0)
    return cx - x - spacing


def glyph_center(base, cx, y, text, color, font=F3, bold=False):
    w = glyph_width(text, font, bold=bold)
    glyph_text(base, cx - w // 2, y, text, color, font, bold=bold)


def label(base, x, y, text, color, pix=F3, center=False, cap=5, bold=False, maxw=None):
    """Capital letters occupy rows y .. y+cap. x is the left edge, or the centre when center=True."""
    if K == 1:
        if center:
            glyph_center(base, x, y, text, color, pix)
        else:
            glyph_text(base, x, y, text, color, pix)
        return
    f = lcd_font(cap, bold)
    d = ImageDraw.Draw(base)
    w = d.textlength(text, font=f)
    if maxw and w > maxw * K:
        f = ttf("bahnschrift.ttf", f.size * maxw * K / w, "Bold Condensed" if bold else "SemiBold Condensed")
    d.text((x * K, (y + cap) * K), text, font=f, fill=color, anchor="ms" if center else "ls")


# --------------------------------------------------------------------------- components
def round_button(img, x, y, w, h, T, pressed=False, glyph=None):
    """Circular button centred in the (w,h) cell; glyph(img, cx, cy, color) draws the icon."""
    d = min(w, h)
    cx0 = x + (w - d) / 2.0
    ellipse(img, cx0, y, cx0 + d, y + d, T["btn_dn"] if pressed else T["btn"], outline=T["btn_ring"])
    if not pressed:  # top gloss
        gw, gh = d - 6, (d - 6) / 2.0
        gl = Image.new("RGB", (S(gw), S(gh)), T["hi"])
        m = ell_mask(gw, gh).point(lambda v: v * 0.55)
        img.paste(gl, (S(cx0 + 3), S(y + 2)), m)
    if glyph:
        off = 1 if pressed else 0
        glyph(img, cx0 + d / 2 + off, y + d / 2 + off, T["glyph"])


def g_prev(img, cx, cy, c):
    poly(img, [(cx - 0.5, cy - 3.5), (cx - 0.5, cy + 3.5), (cx - 4.5, cy)], c)
    poly(img, [(cx + 3.5, cy - 3.5), (cx + 3.5, cy + 3.5), (cx - 0.5, cy)], c)


def g_next(img, cx, cy, c):
    poly(img, [(cx + 0.5, cy - 3.5), (cx + 0.5, cy + 3.5), (cx + 4.5, cy)], c)
    poly(img, [(cx - 3.5, cy - 3.5), (cx - 3.5, cy + 3.5), (cx + 0.5, cy)], c)


def g_play(img, cx, cy, c):
    poly(img, [(cx - 2.5, cy - 4.5), (cx - 2.5, cy + 4.5), (cx + 4.5, cy)], c)


def g_pause(img, cx, cy, c):
    poly(img, [(cx - 3.5, cy - 4), (cx - 1, cy - 4), (cx - 1, cy + 4), (cx - 3.5, cy + 4)], c)
    poly(img, [(cx + 1, cy - 4), (cx + 3.5, cy - 4), (cx + 3.5, cy + 4), (cx + 1, cy + 4)], c)


def g_stop(img, cx, cy, c):
    poly(img, [(cx - 3.5, cy - 3.5), (cx + 3.5, cy - 3.5), (cx + 3.5, cy + 3.5), (cx - 3.5, cy + 3.5)], c)


def g_eject(img, cx, cy, c):
    poly(img, [(cx - 4, cy + 0.5), (cx + 4, cy + 0.5), (cx, cy - 4)], c)
    poly(img, [(cx - 4, cy + 2), (cx + 4, cy + 2), (cx + 4, cy + 3.5), (cx - 4, cy + 3.5)], c)


def g_plus(img, cx, cy, c):
    poly(img, [(cx - 4, cy - 1), (cx + 4, cy - 1), (cx + 4, cy + 1), (cx - 4, cy + 1)], c)
    poly(img, [(cx - 1, cy - 4), (cx + 1, cy - 4), (cx + 1, cy + 4), (cx - 1, cy + 4)], c)


def g_minus(img, cx, cy, c):
    poly(img, [(cx - 4, cy - 1), (cx + 4, cy - 1), (cx + 4, cy + 1), (cx - 4, cy + 1)], c)


def g_lines(img, cx, cy, c):
    for dy in (-3, 0, 3):
        poly(img, [(cx - 3.5, cy + dy - 0.6), (cx + 3.5, cy + dy - 0.6), (cx + 3.5, cy + dy + 0.6), (cx - 3.5, cy + dy + 0.6)], c)


def g_star(img, cx, cy, c):
    for ang in range(0, 180, 45):
        a = math.radians(ang)
        thick_line(img, cx - math.cos(a) * 4, cy - math.sin(a) * 4, cx + math.cos(a) * 4, cy + math.sin(a) * 4, 1.2, c)


def g_folder(img, cx, cy, c):
    poly(img, [(cx - 5, cy - 3), (cx - 1.5, cy - 3), (cx - 0.5, cy - 2), (cx + 5, cy - 2), (cx + 5, cy + 4), (cx - 5, cy + 4)], c)


def oval_button(img, x, y, w, h, T, pressed, glyph):
    rrect(img, x + 1, y + 1, x + w - 1, y + h - 1, (h - 2) / 2.0, T["btn_dn"] if pressed else T["btn"], outline=T["btn_ring"])
    if glyph:
        glyph(img, x + w / 2 + (1 if pressed else 0), y + h / 2 + (1 if pressed else 0), T["glyph"])


def capsule_thumb(img, x, y, w, h, T, pressed):
    rrect(img, x, y, x + w, y + h, h / 2.0, T["btn_dn"] if pressed else T["btn"], outline=T["btn_ring"])


def seg7(img, x, y, digit, on, ghost):
    lit = {
        "0": "abcdef", "1": "bc", "2": "abged", "3": "abgcd", "4": "fgbc", "5": "afgcd",
        "6": "afgedc", "7": "abc", "8": "abcdefg", "9": "abcdfg", "-": "g", " ": "",
    }[digit]
    if K == 1:
        segs = {
            "a": (2, 0, 7, 2), "b": (6, 1, 8, 7), "c": (6, 6, 8, 12), "d": (2, 11, 7, 13),
            "e": (1, 6, 3, 12), "f": (1, 1, 3, 7), "g": (2, 5, 7, 7),
        }
        if ghost is not None:
            for s in "abcdefg":
                x0, y0, x1, y1 = segs[s]
                rect(img, x + x0, y + y0, x + x1, y + y1, ghost)
        for s in lit:
            x0, y0, x1, y1 = segs[s]
            rect(img, x + x0, y + y0, x + x1, y + y1, on)
        return
    # HD: bevelled segments (slightly italic like a real LCD)
    L, R, T_, M, B, t, gap, sl = 1.6, 7.4, 1.0, 6.5, 12.0, 1.7, 0.35, 0.45

    def sx(px, py):  # slant
        return px + (M - py) * sl / 6.0

    def hseg(a, b, yc):
        h = t / 2
        return [(sx(a, yc), yc), (sx(a + h, yc - h), yc - h), (sx(b - h, yc - h), yc - h), (sx(b, yc), yc),
                (sx(b - h, yc + h), yc + h), (sx(a + h, yc + h), yc + h)]

    def vseg(xc, a, b):
        h = t / 2
        return [(sx(xc, a), a), (sx(xc + h, a + h), a + h), (sx(xc + h, b - h), b - h), (sx(xc, b), b),
                (sx(xc - h, b - h), b - h), (sx(xc - h, a + h), a + h)]

    shapes = {
        "a": hseg(L + gap, R - gap, T_), "g": hseg(L + gap, R - gap, M), "d": hseg(L + gap, R - gap, B),
        "f": vseg(L, T_ + gap, M - gap), "b": vseg(R, T_ + gap, M - gap),
        "e": vseg(L, M + gap, B - gap), "c": vseg(R, M + gap, B - gap),
    }
    for s in "abcdefg":
        col = on if s in lit else ghost
        if col is not None:
            poly(img, [(x + px, y + py) for px, py in shapes[s]], col)


# --------------------------------------------------------------------------- MAIN.BMP
def make_main(T):
    img = vgrad(275, 116, T["body"])
    rect(img, 0, 0, 275, 1, T["edge"]); rect(img, 0, 115, 275, 116, T["edge"])
    rect(img, 0, 0, 1, 116, T["edge"]); rect(img, 274, 0, 275, 116, T["edge"])
    hline(img, 1, 274, 14, T["hi"])
    # LCD
    rrect(img, 4, 17, 271, 71, 7, T["lcd"], outline=T["lcd_edge"])
    hline(img, 10, 265, 71, T["hi"])
    # silver tab with volume / balance / EQ / PL
    rrect(img, 102, 53, 273, 76, 9, T["panel"], outline=T["panel_edge"])
    body_rows = vgrad(275, 116, T["body"])
    paste(img, crop(body_rows, 103, 71, 169, 10), 103, 71)
    hline(img, 4, 271, 84, T["hi"])
    # right lower tab for shuffle / repeat
    rrect(img, 157, 85, 273, 114, 9, T["panel"], outline=T["panel_edge"])
    # LCD details: ghost 88:88, colon, labels
    for dx in (48, 60, 78, 90):
        seg7(img, dx, 26, " ", T["lcd_text"], T["lcd_ghost"])
    if K == 1:
        rect(img, 72, 30, 74, 32, T["lcd_text"]); rect(img, 72, 35, 74, 37, T["lcd_text"])
    else:
        ellipse(img, 72, 29.6, 74, 31.6, T["lcd_text"]); ellipse(img, 71.6, 35, 73.6, 37, T["lcd_text"])
    label(img, 129, 44, "KBPS", T["lcd_text"], F3)
    label(img, 168, 44, "KHZ", T["lcd_text"], F3)
    dotted_h(img, 24, 100, 61, T["lcd_ghost"])
    # lightning bolt logo
    c1, c2 = T["bolt"]
    poly(img, [(262.5, 91.5), (255.5, 99.5), (259.5, 99.5), (254, 106), (264.5, 97), (260, 97), (265.5, 91.5)], T["edge"])
    poly(img, [(262, 92), (256.5, 99), (260, 99), (255.5, 104.5), (263.5, 97.5), (259.5, 97.5), (264.5, 92)], c1)
    poly(img, [(259.8, 98.5), (255.8, 104), (263, 98)], c2)
    for i in range(4):
        thick_line(img, 273 - i * 3 - 0.5, 113, 273, 112.5 - i * 3 - 0.5, 0.8, T["panel_edge"])
    return img


# --------------------------------------------------------------------------- TITLEBAR.BMP
def title_bar(T, active, title, shade=False):
    img = new(275, 14, stops_color(T["body"], 0.0))
    rrect(img, 0, 0, 275, 15, 4, T["tbar"] if active else T["tbar_in"], outline=T["edge"])
    rect(img, 1, 13, 274, 14, T["edge"])
    tc = T["ttext"] if active else T["ttext_in"]
    if shade:
        label(img, 20, 4.5, title, tc, F5, cap=5)
        rrect(img, 120, 2, 162, 12, 3, T["lcd"], outline=T["lcd_edge"])
        return img
    font = ttf("ariblk.ttf", 11 * K)
    tw = ImageDraw.Draw(img).textlength(title, font=font) / K
    tx0 = int(137 - tw / 2) - 6
    tx1 = int(137 + tw / 2) + 6
    for y, c in ((4, T["groove_hi"]), (5, T["groove_lo"]), (8, T["groove_hi"]), (9, T["groove_lo"])):
        hline(img, 19, tx0, y, c if active else lerp(c, T["ttext_in"], 0.5))
        hline(img, tx1, 240, y, c if active else lerp(c, T["ttext_in"], 0.5))
    ttf_text(img, 137, 7, title, font, tc, shadow=T["groove_lo"] if active else None, anchor="mm")
    return img


def small_square_button(img, x, y, T, pressed, glyph_fn):
    rrect(img, x, y, x + 9, y + 9, 2, T["btn_dn"] if pressed else T["btn"], outline=T["btn_ring"])
    glyph_fn(img, x, y, T["glyph"])


def gl_menu(img, x, y, c):
    rect(img, x + 3, y + 3, x + 6, y + 6, c)


def gl_min(img, x, y, c):
    rect(img, x + 2, y + 5, x + 7, y + 7, c)


def gl_close(img, x, y, c):
    if K == 1:
        for i in range(5):
            putpx(img, x + 2 + i, y + 2 + i, c)
            putpx(img, x + 6 - i, y + 2 + i, c)
    else:
        thick_line(img, x + 2.3, y + 2.3, x + 6.7, y + 6.7, 1.1, c)
        thick_line(img, x + 6.7, y + 2.3, x + 2.3, y + 6.7, 1.1, c)


def gl_shade(img, x, y, c):
    rect(img, x + 2, y + 2, x + 7, y + 4, c)
    rect(img, x + 2, y + 4, x + 3, y + 7, c); rect(img, x + 6, y + 4, x + 7, y + 7, c)
    rect(img, x + 2, y + 6, x + 7, y + 7, c)


def gl_unshade(img, x, y, c):
    rect(img, x + 2, y + 5, x + 7, y + 7, c)
    rect(img, x + 2, y + 2, x + 7, y + 3, c)


def make_titlebar(T):
    img = new(344, 87, stops_color(T["body"], 0.3))
    for (x, y, pressed, fn) in [
        (0, 0, False, gl_menu), (0, 9, True, gl_menu),
        (9, 0, False, gl_min), (9, 9, True, gl_min),
        (18, 0, False, gl_close), (18, 9, True, gl_close),
        (0, 18, False, gl_shade), (9, 18, True, gl_shade),
        (0, 27, False, gl_unshade), (9, 27, True, gl_unshade),
    ]:
        small_square_button(img, x, y, T, pressed, fn)
    paste(img, title_bar(T, True, T["title"]), 27, 0)
    paste(img, title_bar(T, False, T["title"]), 27, 15)
    paste(img, title_bar(T, True, T["title"], shade=True), 27, 29)
    paste(img, title_bar(T, False, T["title"], shade=True), 27, 42)
    paste(img, title_bar(T, True, "IT REALLY WHIPS!"), 27, 57)
    paste(img, title_bar(T, False, "IT REALLY WHIPS!"), 27, 72)
    # clutter bar (sits on the LCD at 10,22)
    for x, col in ((304, T["lcd_dim"]), (312, T["lcd_ghost"])):
        rect(img, x, 0, x + 8, 43, T["lcd"])
        for oy, oh in ((3, 8), (11, 7), (18, 7), (25, 8), (33, 7)):
            rect(img, x + 2, oy + oh // 2, x + 6, oy + oh // 2 + 1, col)
    for x, y, h in ((304, 47, 8), (312, 55, 7), (320, 62, 7), (328, 69, 8), (336, 77, 7)):
        rect(img, x, y, x + 8, y + h, T["lcd"])
        rect(img, x + 1, y + h // 2, x + 7, y + h // 2 + 1, T["accent_hi"])
    # shade mode position bar
    rect(img, 0, 36, 17, 43, T["lcd"]); hline(img, 0, 17, 39, T["lcd_dim"])
    for x in (17, 20, 23):
        rect(img, x, 36, x + 3, 43, T["lcd_text"])
    return img


# --------------------------------------------------------------------------- CBUTTONS.BMP
def make_cbuttons(T, main):
    img = new(136, 36)
    spec = [(0, 23, 18, 16, 88, g_prev), (23, 23, 18, 39, 88, g_play), (46, 23, 18, 62, 88, g_pause),
            (69, 23, 18, 85, 88, g_stop), (92, 22, 18, 108, 88, g_next)]
    for sx, w, h, tx, ty, g in spec:
        for row, pressed in ((0, False), (18, True)):
            cell = crop(main, tx, ty, w, h)
            round_button(cell, 0, 0, w, h, T, pressed, g)
            paste(img, cell, sx, row)
    for row, pressed in ((0, False), (16, True)):
        cell = crop(main, 136, 89, 22, 16)
        round_button(cell, 0, 0, 22, 16, T, pressed, g_eject)
        paste(img, cell, 114, row)
    return img


# --------------------------------------------------------------------------- SHUFREP.BMP
def g_123(img, cx, cy, c):
    if K == 1:
        glyph_text(img, int(cx) - 5, int(cy) - 2, "123", c, F3, spacing=1)
    else:
        label(img, cx, cy - 2.5, "123", c, F3, center=True, cap=5, bold=True)


def g_repeat(img, cx, cy, c):
    pts_o, pts_i = [], []
    for a in range(40, 330, 10):
        r = math.radians(a)
        pts_o.append((cx + math.cos(r) * 4.6, cy - math.sin(r) * 4.6))
        pts_i.append((cx + math.cos(r) * 3.0, cy - math.sin(r) * 3.0))
    poly(img, pts_o + pts_i[::-1], c)
    poly(img, [(cx + 1.5, cy - 6), (cx + 5.5, cy - 3.3), (cx + 1.2, cy - 1)], c)


def make_shufrep(T, main):
    img = new(92, 85, stops_color(T["body"], 0.5))
    for row, (sel, pressed) in enumerate([(False, False), (False, True), (True, False), (True, True)]):
        y = row * 15
        cell = crop(main, 164, 89, 47, 15)
        rrect(cell, 3, 0, 33, 15, 7, T["btn_dn"] if pressed else T["btn"], outline=T["btn_ring"])
        g_123(cell, 18 + (1 if pressed else 0), 7 + (1 if pressed else 0), T["glyph"])
        ellipse(cell, 36, 4, 44, 12, T["led_on"] if sel else T["led_off"], outline=T["btn_ring"])
        paste(img, cell, 28, y)
        cell = crop(main, 210, 89, 28, 15)
        ellipse(cell, 1, 0, 16, 15, T["btn_dn"] if pressed else T["btn"], outline=T["btn_ring"])
        g_repeat(cell, 8.5 + (1 if pressed else 0), 7.5 + (1 if pressed else 0), T["glyph"])
        ellipse(cell, 18, 4, 26, 12, T["led_on"] if sel else T["led_off"], outline=T["btn_ring"])
        paste(img, cell, 0, y)
    for lab, sx, tx in (("EQ", 0, 219), ("PL", 23, 242)):
        for (ox, oy, sel, pressed) in ((0, 61, False, False), (0, 73, True, False), (46, 61, False, True), (46, 73, True, True)):
            cell = crop(main, tx, 58, 23, 12)
            if sel:
                fill = [(0.0, T["accent_hi"]), (1.0, T["accent"])] if not pressed else [(0.0, T["accent"]), (1.0, T["accent_hi"])]
                txt = (255, 255, 255)
            else:
                fill = T["btn_dn"] if pressed else T["btn"]
                txt = T["accent"]
            rrect(cell, 0, 0, 23, 12, 6, fill, outline=T["btn_ring"])
            label(cell, 12 + (1 if pressed else 0), 3.5 + (1 if pressed else 0), lab, txt, F5, center=True, bold=True)
            paste(img, cell, sx + ox, oy)
    return img


# --------------------------------------------------------------------------- small LCD sprites
def make_numbers(T, ex=False):
    img = new(108 if ex else 99, 13, T["lcd"])
    for d in range(10):
        seg7(img, d * 9, 0, str(d), T["lcd_text"], T["lcd_ghost"])
    seg7(img, 90, 0, " ", T["lcd_text"], T["lcd_ghost"])
    if ex:
        seg7(img, 99, 0, "-", T["lcd_text"], None)
    return img


TEXT_ROWS = [
    'ABCDEFGHIJKLMNOPQRSTUVWXYZ"@   ',
    "0123456789….:()-'!_+\\/[]^&%,=$#",
    "ÅÖÄ?*                          ",
]


def make_text(T):
    img = new(155, 18, T["lcd"])
    for r, line in enumerate(TEXT_ROWS):
        for c, ch in enumerate(line[:31]):
            if ch == " ":
                continue
            if K == 1:
                if F5.get(ch):
                    glyph_text(img, c * 5, r * 6, ch, T["lcd_text"], F5)
                continue
            # HD: TrueType glyph centred in its 5x6 cell, capitals on rows 0..5
            cw, chh = S(5), S(6)
            cell = Image.new("RGB", (cw * 3, chh * 2), T["lcd"])
            f = ttf("bahnschrift.ttf", 5.0 * K / 0.70, "SemiBold")
            d = ImageDraw.Draw(cell)
            d.text((cw * 1.5, S(5.2) + chh * 0.5), ch, font=f, fill=T["lcd_text"], anchor="ms")
            bbox = cell.getbbox() if False else None
            # crop the centre cell; squeeze wide glyphs (M, W, @...) horizontally
            gw = d.textlength(ch, font=f)
            if gw > cw * 0.92:
                wide = Image.new("RGB", (int(gw) + 4, chh * 2), T["lcd"])
                ImageDraw.Draw(wide).text(((int(gw) + 4) / 2, S(5.2) + chh * 0.5), ch, font=f, fill=T["lcd_text"], anchor="ms")
                wide = wide.resize((int(cw * 0.92), chh * 2), RES)
                cell = Image.new("RGB", (cw * 3, chh * 2), T["lcd"])
                cell.paste(wide, (int(cw * 1.5 - wide.width / 2), 0))
            piece = cell.crop((cw, int(chh * 0.5), cw * 2, int(chh * 0.5) + chh))
            img.paste(piece, (c * cw, r * chh))
    return img


def make_monoster(T):
    img = new(58, 24, T["lcd"])
    if K == 1:
        glyph_text(img, 0, 3, "STEREO", T["lcd_text"], F5)
        glyph_text(img, 0, 15, "STEREO", T["lcd_dim"], F5)
        glyph_text(img, 29 + 4, 3, "MONO", T["lcd_text"], F5)
        glyph_text(img, 29 + 4, 15, "MONO", T["lcd_dim"], F5)
    else:
        for y, col in ((3, T["lcd_text"]), (15, T["lcd_dim"])):
            label(img, 14.5, y, "STEREO", col, F5, center=True, cap=5.5, bold=True, maxw=28)
            label(img, 29 + 13.5, y, "MONO", col, F5, center=True, cap=5.5, bold=True, maxw=26)
    return img


def make_playpaus(T):
    img = new(42, 9, T["lcd"])
    c = T["lcd_text"]
    poly(img, [(3, 1), (3, 8), (7.5, 4.5)], c)
    rect(img, 11, 1, 13, 8, c); rect(img, 15, 1, 17, 8, c)
    rect(img, 20, 1, 26, 7, c)
    if K == 1:
        rect(img, 39, 3, 41, 6, T["accent_hi"])
    else:
        ellipse(img, 39, 3, 41.5, 5.5, T["accent_hi"])
    return img


def make_volume(T, main, balance=False):
    img = new(68, 433, stops_color(T["panel"], 0.5))
    tx, w = (177, 38) if balance else (107, 68)
    for i in range(28):
        cell = crop(main, tx, 57, w, 13)
        rrect(cell, 1, 4, w - 1, 9, 2, T["groove"])
        if balance:
            half = (w / 2 - 3) * i / 27
            if half > 0.3:
                rrect(cell, w / 2 - half, 5, w / 2 + half, 8, 1.5, T["accent"])
            rect(cell, w / 2 - 1, 5, w / 2 + 1, 8, T["accent_hi"])
            paste(img, cell, 9, i * 15)
        else:
            fill = 3 + (w - 6) * i / 27
            rrect(cell, 2, 5, fill, 8, 1.5, lerp(T["accent"], T["accent_hi"], i / 27 * 0.6))
            paste(img, cell, 0, i * 15)
    for x, pressed in ((15, False), (0, True)):
        cell = crop(main, 140, 58, 14, 11)
        capsule_thumb(cell, 0, 0, 14, 11, T, pressed)
        paste(img, cell, x, 422)
    return img


def make_posbar(T, main):
    img = new(307, 10, stops_color(T["body"], 0.65))
    bg = crop(main, 16, 72, 248, 10)
    rrect(bg, 0, 3, 248, 8, 2, T["groove"])
    hline(bg, 2, 246, 8, T["hi"])
    paste(img, bg, 0, 0)
    for x, pressed in ((248, False), (278, True)):
        cell = crop(main, 100, 72, 29, 10)
        capsule_thumb(cell, 0, 0, 29, 10, T, pressed)
        rect(cell, 13, 3, 14, 7, T["btn_ring"]); rect(cell, 15, 3, 16, 7, T["btn_ring"])
        paste(img, cell, x, 0)
    return img


# --------------------------------------------------------------------------- EQMAIN.BMP
EQ_LABELS = ["60", "170", "310", "600", "1K", "3K", "6K", "12K", "14K", "16K"]


def eq_title(T, active):
    img = new(275, 14, stops_color(T["body"], 0.0))
    rect(img, 0, 0, 275, 1, T["edge"]); rect(img, 0, 0, 1, 14, T["edge"]); rect(img, 274, 0, 275, 14, T["edge"])
    hline(img, 1, 274, 1, T["hi"])
    rrect(img, 16, 2, 100, 20, 5, T["tbar"] if active else T["tbar_in"], outline=T["edge"])
    label(img, 58, 4.5, "EQUALIZER", T["ttext"] if active else T["ttext_in"], F5, center=True, cap=5.5, bold=True)
    for y, c in ((5, T["panel_edge"]), (6, T["hi"]), (9, T["panel_edge"]), (10, T["hi"])):
        hline(img, 106, 248, y, c)
    return img


def make_eqmain(T):
    img = new(275, 315, stops_color(T["body"], 0.5))
    bg = vgrad(275, 116, T["body"])
    rect(bg, 0, 0, 275, 1, T["edge"]); rect(bg, 0, 115, 275, 116, T["edge"])
    rect(bg, 0, 0, 1, 116, T["edge"]); rect(bg, 274, 0, 275, 116, T["edge"])
    rrect(bg, 6, 15, 269, 113, 7, T["panel"], outline=T["panel_edge"])
    paste(bg, eq_title(T, True), 0, 0)
    lab = T["glyph"]
    for txt, y in (("+12 DB", 41), ("+0 DB", 67), ("-12 DB", 92)):
        label(bg, 56, y, txt, lab, F3, center=True)
    for x in (42, 70):
        dotted_v(bg, x, 43, 96, T["panel_edge"])
    for y in (43, 69, 95):
        hline(bg, 40, 45, y, T["panel_edge"]); hline(bg, 67, 72, y, T["panel_edge"])
    for i in range(10):
        dotted_v(bg, 78 + i * 18 - 2, 40, 100, T["panel_edge"])
    rrect(bg, 9, 102, 40, 111, 2, T["eq_label_bg"])
    label(bg, 24.5, 104, "PREAMP", T["eq_label"], F3, center=True)
    rrect(bg, 42, 102, 74, 111, 2, T["eq_label_bg"])
    for i, lbl in enumerate(EQ_LABELS):
        bx = 78 + i * 18
        rrect(bg, bx - 1, 102, bx + 16, 111, 2, T["eq_label_bg"])
        label(bg, bx + 7.5, 104, lbl, T["eq_label"], F3, center=True)
    paste(img, bg, 0, 0)
    paste(img, eq_title(T, True), 0, 134)
    paste(img, eq_title(T, False), 0, 149)
    for y, pressed in ((116, False), (125, True)):
        cell = new(9, 9, stops_color(T["body"], 0))
        small_square_button(cell, 0, 0, T, pressed, gl_close)
        paste(img, cell, 0, y)
    cell = new(9, 9, stops_color(T["body"], 0))
    small_square_button(cell, 0, 0, T, False, gl_shade)
    paste(img, cell, 254, 152)
    for name, w, xs, tx in (("ON", 26, (10, 128, 69, 187), 14), ("AUTO", 32, (36, 154, 95, 213), 40)):
        for x, sel, pressed in zip(xs, (False, False, True, True), (False, True, False, True)):
            cell = crop(bg, tx, 18, w, 12)
            if sel:
                fill = [(0.0, T["accent_hi"]), (1.0, T["accent"])] if not pressed else [(0.0, T["accent"]), (1.0, T["accent_hi"])]
                txt = (255, 255, 255)
            else:
                fill = T["btn_dn"] if pressed else T["btn"]
                txt = T["glyph"]
            rrect(cell, 0, 0, w, 12, 6, fill, outline=T["btn_ring"])
            label(cell, w / 2 + (1 if pressed else 0), 3.5 + (1 if pressed else 0), name, txt, F5, center=True, bold=True)
            paste(img, cell, x, 119)
    # slider frames (28) 15x65 each, 14 per row
    for n in range(28):
        p = n / 27.0
        cell = new(15, 65, stops_color(T["panel"], 0.5))
        paste(cell, crop(bg, 78, 38, 14, 63), 0, 0)
        rrect(cell, 5, 1, 9, 62, 2, T["groove"])
        vline(cell, 9, 3, 60, T["hi"])
        center = 31
        pos = 5 + (1 - p) * 51
        y0, y1 = sorted((center, pos))
        if y1 - y0 > 0.3:
            rrect(cell, 5.8, y0, 8.2, y1 + 1, 1, lerp(T["accent"], T["accent_hi"], abs(p - 0.5)))
        hline(cell, 3, 5, center, T["panel_edge"]); hline(cell, 9, 11, center, T["panel_edge"])
        paste(img, cell, 13 + (n % 14) * 15, 164 + (n // 14) * 65)
    for y, pressed in ((164, False), (176, True)):
        cell = crop(bg, 79, 60, 11, 11)
        ellipse(cell, 0, 0, 11, 11, T["btn_dn"] if pressed else T["btn"], outline=T["btn_ring"])
        if K == 1:
            putpx(cell, 4, 3, T["hi"]); putpx(cell, 3, 4, T["hi"])
        else:
            ellipse(cell, 2.5, 2.2, 6.0, 5.2, T["hi"])
        paste(img, cell, 0, y)
    for y, pressed in ((164, False), (176, True)):
        cell = crop(bg, 217, 18, 44, 12)
        rrect(cell, 0, 0, 44, 12, 6, T["btn_dn"] if pressed else T["btn"], outline=T["btn_ring"])
        label(cell, 22 + (1 if pressed else 0), 3.5 + (1 if pressed else 0), "PRESETS", T["glyph"], F3, center=True, bold=True)
        paste(img, cell, 224, y)
    g = new(113, 19, T["lcd"])
    for x in range(0, 113, 12):
        dotted_v(g, x, 0, 19, T["lcd_ghost"])
    dotted_h(g, 0, 113, 9, T["lcd_ghost"])
    paste(img, g, 0, 294)
    for y in range(19):
        putpx(img, 115, 294 + y, lerp(T["accent_hi"], T["accent"], abs(y - 9) / 9))
    rect(img, 0, 314, 113, 315, T["lcd"])
    for x in range(0, 113, 2):
        putpx(img, x, 314, T["lcd_dim"])
    return img


# --------------------------------------------------------------------------- PLEDIT.BMP
def make_pledit(T):
    img = new(280, 186, stops_color(T["body"], 0.5))

    def top_piece(w, active, kind):
        p = new(w, 20, stops_color(T["body"], 0.15))
        paste(p, vgrad(w, 15, T["tbar"] if active else T["tbar_in"]), 0, 0)
        rect(p, 0, 0, w, 1, T["edge"]); rect(p, 0, 15, w, 16, T["edge"])
        hline(p, 0, w, 16, T["hi"])
        rect(p, 0, 19, w, 20, T["edge"])
        gc = lambda c: c if active else lerp(c, T["ttext_in"], 0.5)
        if kind in ("tile", "left", "right"):
            x0 = 13 if kind == "left" else 0
            x1 = w - 22 if kind == "right" else w
            for y, c in ((5, T["groove_hi"]), (6, T["groove_lo"]), (9, T["groove_hi"]), (10, T["groove_lo"])):
                hline(p, x0, x1, y, gc(c))
        if kind == "left":
            rect(p, 0, 0, 1, 20, T["edge"])
        if kind == "right":
            rect(p, w - 1, 0, w, 20, T["edge"])
            small_square_button(p, 5, 3, T, False, gl_shade)
            small_square_button(p, 14, 3, T, False, gl_close)
        if kind == "title":
            font = ttf("ariblk.ttf", 11 * K)
            ttf_text(p, w / 2, 7, "PLAYLIST", font, T["ttext"] if active else T["ttext_in"],
                     shadow=T["groove_lo"] if active else None, anchor="mm")
            for y, c in ((5, T["groove_hi"]), (6, T["groove_lo"]), (9, T["groove_hi"]), (10, T["groove_lo"])):
                hline(p, 0, 12, y, gc(c)); hline(p, w - 12, w, y, gc(c))
        return p

    for active, y in ((True, 0), (False, 21)):
        paste(img, top_piece(25, active, "left"), 0, y)
        paste(img, top_piece(100, active, "title"), 26, y)
        paste(img, top_piece(25, active, "tile"), 127, y)
        paste(img, top_piece(25, active, "right"), 153, y)
    lt = hgrad(12, 29, [(0.0, T["body"][1][1]), (1.0, T["body"][0][1])])
    rect(lt, 0, 0, 1, 29, T["edge"]); rect(lt, 11, 0, 12, 29, T["edge"]); rect(lt, 10, 0, 11, 29, T["panel_edge"])
    paste(img, lt, 0, 42)
    rt = hgrad(20, 29, [(0.0, T["body"][0][1]), (1.0, T["body"][1][1])])
    rect(rt, 0, 0, 1, 29, T["edge"]); rect(rt, 19, 0, 20, 29, T["edge"])
    rect(rt, 4, 0, 14, 29, T["edge"]); rect(rt, 5, 0, 13, 29, T["groove"])
    paste(img, rt, 31, 42)
    for x, pressed in ((52, False), (61, True)):
        cell = new(8, 18, T["groove"])
        rrect(cell, 0, 0, 8, 18, 3, [(0.0, T["btn"][0][1]), (1.0, T["btn"][-1][1])] if not pressed else T["btn_dn"], outline=T["btn_ring"])
        for yy in (6, 8, 10):
            hline(cell, 2, 6, yy, T["btn_ring"])
        paste(img, cell, x, 53)
    for x, fn in ((52, gl_close), (62, gl_shade), (150, gl_unshade)):
        cell = new(9, 9)
        small_square_button(cell, 0, 0, T, True, fn)
        paste(img, cell, x, 42)
    for (x, y, w) in ((72, 57, 25), (72, 42, 25), (99, 57, 50), (99, 42, 50)):
        paste(img, vgrad(w, 14, T["tbar"]), x, y)

    def bottom_base(w):
        b = vgrad(w, 38, [(0.0, T["body"][1][1]), (1.0, T["body"][-1][1])])
        rect(b, 0, 0, w, 1, T["panel_edge"]); rect(b, 0, 1, w, 2, T["hi"])
        rect(b, 0, 37, w, 38, T["edge"])
        return b

    paste(img, bottom_base(25), 179, 0)
    bl = bottom_base(125)
    rect(bl, 0, 0, 1, 38, T["edge"])
    for bx, g in ((14, g_plus), (43, g_minus), (72, g_lines), (101, g_star)):
        oval_button(bl, bx, 8, 25, 18, T, False, g)
    paste(img, bl, 0, 72)
    br = bottom_base(150)
    rect(br, 149, 0, 150, 38, T["edge"])
    rrect(br, 2, 4, 101, 35, 6, T["lcd"], outline=T["lcd_edge"])
    c = T["lcd_text"]
    oy = 22
    # mini transport glyphs
    rect(br, 6, oy + 1, 7, oy + 8, c); poly(br, [(11.5, oy + 1), (11.5, oy + 8), (7.5, oy + 4.5)], c)
    poly(br, [(15, oy + 1), (15, oy + 8), (19, oy + 4.5)], c)
    rect(br, 25, oy + 1, 27, oy + 8, c); rect(br, 29, oy + 1, 31, oy + 8, c)
    rect(br, 34, oy + 2, 40, oy + 8, c)
    poly(br, [(43, oy + 1), (43, oy + 8), (47, oy + 4.5)], c); rect(br, 47, oy + 1, 48, oy + 8, c)
    poly(br, [(51, oy + 5), (59, oy + 5), (55, oy + 1)], c); rect(br, 51, oy + 6, 59, oy + 8, c)
    oval_button(br, 103, 8, 25, 18, T, False, g_folder)
    poly(br, [(132, 8), (139, 8), (135.5, 4)], T["accent"])
    poly(br, [(132, 11), (139, 11), (135.5, 15)], T["accent"])
    for i in range(4):
        thick_line(br, 148 - i * 4, 36.5, 148.5, 36 - i * 4, 0.8, T["panel_edge"])
    paste(img, br, 126, 72)
    vb = bottom_base(75)
    rrect(vb, 2, 6, 73, 32, 4, T["lcd"], outline=T["lcd_edge"])
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
                cell = new(22, 18, T["edge"])
                rrect(cell, 0, 0, 22, 18, 3, [(0.0, T["accent_hi"]), (1.0, T["accent"])] if sel else T["btn"], outline=T["btn_ring"])
                label(cell, 11, 6.5, lab, (255, 255, 255) if sel else T["glyph"], F3, center=True, bold=True, maxw=19)
                paste(img, cell, x + (23 if sel else 0), y)
    for x, h in ((48, 54), (100, 72), (150, 54), (200, 54), (250, 54)):
        rect(img, x, 111, x + 3, 111 + h, T["groove"])
        vline(img, x + 1, 112, 110 + h, T["accent"])
    return img


# --------------------------------------------------------------------------- text files
def hexc(c):
    return "#%02X%02X%02X" % c


def pledit_txt(T):
    return ("[Text]\r\nNormal=%s\r\nCurrent=%s\r\nNormalBG=%s\r\nSelectedBG=%s\r\nFont=%s\r\n"
            % (hexc(T["list_text"]), hexc(T["list_cur"]), hexc(T["list_bg"]), hexc(T["list_sel"]),
               "Arial" if K == 1 else "Segoe UI"))


def viscolor_txt(T):
    cols = [T["lcd"], T["lcd_ghost"]]
    for i in range(16):
        cols.append(lerp(T["vis_top"], T["vis_bot"], i / 15))
    for i in range(5):
        cols.append(lerp(T["vis_top"], T["vis_bot"], i / 6))
    cols.append(T["vis_top"])
    names = ["background", "dots"] + ["analyzer %d" % i for i in range(16)] + ["osc %d" % i for i in range(5)] + ["peaks"]
    return "".join("%d,%d,%d, // %s\r\n" % (c[0], c[1], c[2], n) for c, n in zip(cols, names))


# --------------------------------------------------------------------------- build
def build(key, k):
    global K
    K = k
    T = THEMES[key]
    main = make_main(T)
    files = {
        "main": main,
        "titlebar": make_titlebar(T),
        "cbuttons": make_cbuttons(T, main),
        "shufrep": make_shufrep(T, main),
        "numbers": make_numbers(T),
        "nums_ex": make_numbers(T, ex=True),
        "text": make_text(T),
        "monoster": make_monoster(T),
        "playpaus": make_playpaus(T),
        "volume": make_volume(T, main),
        "balance": make_volume(T, main, balance=True),
        "posbar": make_posbar(T, main),
        "eqmain": make_eqmain(T),
        "pledit": make_pledit(T),
    }
    texts = {"pledit.txt": pledit_txt(T), "viscolor.txt": viscolor_txt(T),
             "readme.txt": "%s - RetroAmp skin%s\r\n" % (T["name"], " (HD, %dx)" % K if K > 1 else " (Winamp 2.x format)")}
    ext = "png" if K > 1 else "bmp"
    out_dir = os.path.join(ROOT, "skins", "src", key + ("" if K > 1 else "_classic"))
    if os.path.isdir(out_dir):
        shutil.rmtree(out_dir)
    os.makedirs(out_dir)
    for n, im in files.items():
        if K > 1:
            im.save(os.path.join(out_dir, n + ".png"), optimize=True)
        else:
            im.save(os.path.join(out_dir, n + ".bmp"))
    for n, t in texts.items():
        with open(os.path.join(out_dir, n), "w", newline="") as f:
            f.write(t)
    wsz = os.path.join(ROOT, "skins", key + ".wsz") if K > 1 else os.path.join(ROOT, "skins", "classic", key + ".wsz")
    os.makedirs(os.path.dirname(wsz), exist_ok=True)
    with zipfile.ZipFile(wsz, "w", zipfile.ZIP_DEFLATED) as z:
        for n in os.listdir(out_dir):
            z.write(os.path.join(out_dir, n), n)
    return out_dir


def make_icon():
    global K
    K = 1
    T = THEMES["RetroBlue"]
    m = Image.new("L", (512, 512), 0)
    ImageDraw.Draw(m).rounded_rectangle([8, 8, 504, 504], radius=96, fill=255)
    m = m.resize((256, 256), RES)
    rgb = _vgrad_px(256, 256, T["tbar"])
    c1, c2 = T["bolt"]
    pts = [(160, 22), (70, 140), (122, 140), (88, 236), (196, 104), (142, 104), (190, 22)]
    poly(rgb, [(x + 4, y + 4) for x, y in pts], (10, 14, 40))
    poly(rgb, pts, c1)
    poly(rgb, [(118, 150), (88, 236), (150, 150)], c2)
    out = rgb.convert("RGBA")
    out.putalpha(m)
    out.save(os.path.join(ROOT, "res", "retroamp.ico"), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])


if __name__ == "__main__":
    os.makedirs(os.path.join(ROOT, "res"), exist_ok=True)
    for key in THEMES:
        build(key, 1)
        build(key, 4)
    dst = os.path.join(ROOT, "res", "skin")
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    shutil.copytree(os.path.join(ROOT, "skins", "src", "RetroBlue"), dst)
    make_icon()
    print("ok")
