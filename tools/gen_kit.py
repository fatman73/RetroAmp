"""
Skin "construction kit" for RetroAmp: the Winamp 2.x sprite layout is written once here,
each style (subclass of Kit) only decides how things look. All skins are generated in HD (K=4)
and may add RetroAmp animation extensions (ANIM.TXT + extra images, optional panel window).

Usage:  python tools/gen_kit.py            -> skins/<Key>.wsz for every style
        python tools/gen_kit.py HiFiTower  -> just one
"""
import math
import os
import random
import shutil
import sys
import zipfile

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

import gen_skin as G
from gen_skin import crop, paste, poly, rect, hline, vline, thick_line, ttf, lerp

K = 4
WHITE, BLACK = (255, 255, 255), (0, 0, 0)


def S(v):
    return int(round(v * K))


def mix(a, b, t):
    return lerp(a, b, max(0.0, min(1.0, t)))


def np_to_img(arr):
    return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGB")


def glow(base, layer, radius, strength=1.0):
    """Adds a blurred copy of `layer` (black background) onto base, then the layer itself."""
    blur = layer.filter(ImageFilter.GaussianBlur(radius * K))
    if strength != 1.0:
        blur = blur.point(lambda v: min(255, int(v * strength)))
    out = ImageChops.add(base, blur)
    return ImageChops.lighter(out, layer)


def text_at(img, x, y, s, color, size, font="segoeuib.ttf", anchor="ls", variation=None):
    """size = font size in skin pixels; (x, y) in skin pixels."""
    f = ttf(font, size * K, variation)
    ImageDraw.Draw(img).text((x * K, y * K), s, font=f, fill=color, anchor=anchor)


def text_w(s, size, font="segoeuib.ttf", variation=None):
    f = ttf(font, size * K, variation)
    return ImageDraw.Draw(Image.new("RGB", (1, 1))).textlength(s, font=f) / K


# ---------------------------------------------------------------------------- glyphs
def g_prev(img, cx, cy, c, s=1.0):
    rect(img, cx - 4.5 * s, cy - 3.5 * s, cx - 3 * s, cy + 3.5 * s, c)
    poly(img, [(cx + 3.5 * s, cy - 3.5 * s), (cx + 3.5 * s, cy + 3.5 * s), (cx - 2.5 * s, cy)], c)


def g_next(img, cx, cy, c, s=1.0):
    rect(img, cx + 3 * s, cy - 3.5 * s, cx + 4.5 * s, cy + 3.5 * s, c)
    poly(img, [(cx - 3.5 * s, cy - 3.5 * s), (cx - 3.5 * s, cy + 3.5 * s), (cx + 2.5 * s, cy)], c)


def g_play(img, cx, cy, c, s=1.0):
    poly(img, [(cx - 2.5 * s, cy - 4.5 * s), (cx - 2.5 * s, cy + 4.5 * s), (cx + 4.5 * s, cy)], c)


def g_pause(img, cx, cy, c, s=1.0):
    rect(img, cx - 3.5 * s, cy - 4 * s, cx - 1 * s, cy + 4 * s, c)
    rect(img, cx + 1 * s, cy - 4 * s, cx + 3.5 * s, cy + 4 * s, c)


def g_stop(img, cx, cy, c, s=1.0):
    rect(img, cx - 3.5 * s, cy - 3.5 * s, cx + 3.5 * s, cy + 3.5 * s, c)


def g_eject(img, cx, cy, c, s=1.0):
    poly(img, [(cx - 4 * s, cy + 0.5 * s), (cx + 4 * s, cy + 0.5 * s), (cx, cy - 4 * s)], c)
    rect(img, cx - 4 * s, cy + 2 * s, cx + 4 * s, cy + 3.5 * s, c)


def g_plus(img, cx, cy, c, s=1.0):
    rect(img, cx - 4, cy - 0.9, cx + 4, cy + 0.9, c)
    rect(img, cx - 0.9, cy - 4, cx + 0.9, cy + 4, c)


def g_minus(img, cx, cy, c, s=1.0):
    rect(img, cx - 4, cy - 0.9, cx + 4, cy + 0.9, c)


def g_lines(img, cx, cy, c, s=1.0):
    for dy in (-3, 0, 3):
        rect(img, cx - 3.5, cy + dy - 0.6, cx + 3.5, cy + dy + 0.6, c)


def g_star(img, cx, cy, c, s=1.0):
    for ang in range(0, 180, 45):
        a = math.radians(ang)
        thick_line(img, cx - math.cos(a) * 4, cy - math.sin(a) * 4, cx + math.cos(a) * 4, cy + math.sin(a) * 4, 1.2, c)


def g_folder(img, cx, cy, c, s=1.0):
    poly(img, [(cx - 5, cy - 3), (cx - 1.5, cy - 3), (cx - 0.5, cy - 2), (cx + 5, cy - 2), (cx + 5, cy + 4), (cx - 5, cy + 4)], c)


def g_shuffle(img, cx, cy, c, s=1.0):
    thick_line(img, cx - 6, cy - 3, cx + 4, cy + 3, 1.2, c)
    thick_line(img, cx - 6, cy + 3, cx + 4, cy - 3, 1.2, c)
    poly(img, [(cx + 3, cy + 1), (cx + 6.5, cy + 3), (cx + 3, cy + 5)], c)
    poly(img, [(cx + 3, cy - 5), (cx + 6.5, cy - 3), (cx + 3, cy - 1)], c)


def g_repeat(img, cx, cy, c, s=1.0):
    pts_o, pts_i = [], []
    for a in range(40, 330, 10):
        r = math.radians(a)
        pts_o.append((cx + math.cos(r) * 4.6, cy - math.sin(r) * 4.6))
        pts_i.append((cx + math.cos(r) * 3.0, cy - math.sin(r) * 3.0))
    poly(img, pts_o + pts_i[::-1], c)
    poly(img, [(cx + 1.5, cy - 6), (cx + 5.5, cy - 3.3), (cx + 1.2, cy - 1)], c)


EQ_LABELS = ["60", "170", "310", "600", "1K", "3K", "6K", "12K", "14K", "16K"]


# ============================================================================ base kit
class Kit:
    key = "Kit"
    name = "Kit"
    title = "RetroAmp"
    # colours
    lcd = (10, 10, 10)
    lcd_text = (240, 240, 240)
    lcd_dim = (120, 120, 120)
    lcd_ghost = (30, 30, 30)
    label_col = (220, 220, 220)
    glyph = (20, 20, 20)
    accent = (80, 140, 240)
    accent2 = (160, 200, 255)
    pl_text, pl_cur, pl_bg, pl_sel, pl_font = (0, 230, 0), (255, 255, 255), (0, 0, 0), (0, 0, 160), "Arial"
    vis_top, vis_bot = (255, 255, 255), (60, 120, 220)
    text_font = ("bahnschrift.ttf", "SemiBold")  # LCD font (text.bmp)
    label_font = ("bahnschrift.ttf", "SemiBold")
    display_glow = 0.0  # >0: LCD text / digits glow (VFD, neon)
    has_panel = True    # skin ships a panel window (deck...) + playlist toggle
    dsp_font = "Bahnschrift"

    def dsp_colors(self):
        """(button text, button text when on, labels on the body)"""
        return self.glyph, self.glyph, self.label_col

    def __init__(self):
        self.anims = []        # list of dict -> ANIM.TXT sections
        self.extra = {}        # extra images name -> Image
        self.panel = None      # panel image (Image) or None
        self.buttons = []      # clickable hot spots: dict(X, Y, W, H, Action)
        self.panel_size = (275, 116)

    # ---------------------------------------------------------------- materials (override)
    def body(self, w, h):
        return G.vgrad(w, h, [(0.0, (90, 90, 96)), (1.0, (50, 50, 56))])

    def frame(self, img, x0, y0, x1, y1):
        rect(img, x0, y0, x1, y0 + 1, (130, 130, 136)); rect(img, x0, y0, x0 + 1, y1, (130, 130, 136))
        rect(img, x0, y1 - 1, x1, y1, BLACK); rect(img, x1 - 1, y0, x1, y1, BLACK)

    def inset(self, img, x0, y0, x1, y1, fill, r=3):
        G.rrect(img, x0, y0, x1, y1, r, fill, outline=BLACK)

    def subpanel(self, img, x0, y0, x1, y1):
        G.rrect(img, x0, y0, x1, y1, 4, [(0.0, (70, 70, 76)), (1.0, (40, 40, 46))], outline=(20, 20, 22))

    def button(self, img, x, y, w, h, pressed, glyph=None, label=None, sel=False, kind="transport"):
        top, bot = ((190, 190, 196), (120, 120, 126)) if not pressed else ((110, 110, 116), (170, 170, 176))
        if sel:
            top, bot = mix(top, self.accent, 0.6), mix(bot, self.accent, 0.6)
        G.rrect(img, x + 0.5, y + 0.5, x + w - 0.5, y + h - 0.5, 2, [(0.0, top), (1.0, bot)], outline=(15, 15, 18))
        self.button_face(img, x, y, w, h, pressed, glyph, label, sel)

    def button_face(self, img, x, y, w, h, pressed, glyph, label, sel, col=None):
        off = 0.5 if pressed else 0
        col = col or self.glyph
        if glyph:
            glyph(img, x + w / 2 + off, y + h / 2 + off, col)
        if label:
            cap = min(5.0, h * 0.42)
            f = ttf(self.label_font[0], cap * K / 0.70, self.label_font[1])
            d = ImageDraw.Draw(img)
            wpx = d.textlength(label, font=f)
            if wpx > (w - 3) * K:
                f = ttf(self.label_font[0], cap * K / 0.70 * (w - 3) * K / wpx, self.label_font[1])
            d.text(((x + w / 2 + off) * K, (y + h / 2 + cap / 2 + off) * K), label, font=f, fill=col, anchor="ms")

    def gadget(self, img, x, y, kind, pressed):
        G.rrect(img, x, y, x + 9, y + 9, 2, [(0.0, (200, 200, 205)), (1.0, (120, 120, 126))] if not pressed
                else [(0.0, (110, 110, 116)), (1.0, (170, 170, 176))], outline=BLACK)
        self.gadget_glyph(img, x, y, kind, self.glyph)

    def gadget_glyph(self, img, x, y, kind, c):
        if kind == "close":
            thick_line(img, x + 2.6, y + 2.6, x + 6.4, y + 6.4, 1.1, c)
            thick_line(img, x + 6.4, y + 2.6, x + 2.6, y + 6.4, 1.1, c)
        elif kind == "min":
            rect(img, x + 2.5, y + 5.5, x + 6.5, y + 6.7, c)
        elif kind in ("shade", "unshade"):
            rect(img, x + 2.5, y + 2.5, x + 6.5, y + 6.5, c)
            rect(img, x + 3.3, y + 3.6, x + 5.7, y + 5.7, (200, 200, 200))
        elif kind == "menu":
            for dy in (3, 4.6, 6.2):
                rect(img, x + 2.5, y + dy - 0.4, x + 6.5, y + dy + 0.4, c)
        elif kind == "deck":  # tiny cassette: switches playlist <-> skin panel
            G.rrect(img, x + 1.6, y + 2.4, x + 7.4, y + 6.8, 0.6, c)
            G.ellipse(img, x + 2.6, y + 3.6, x + 3.9, y + 4.9, (230, 230, 230))
            G.ellipse(img, x + 5.1, y + 3.6, x + 6.4, y + 4.9, (230, 230, 230))
            rect(img, x + 3.4, y + 5.6, x + 5.6, y + 6.3, (230, 230, 230))

    def title_bar(self, active, title, shade=False):
        img = self.body(275, 14)
        self.frame(img, 0, 0, 275, 14)
        col = self.label_col if active else mix(self.label_col, (80, 80, 80), 0.5)
        if shade:
            self.inset(img, 118, 2, 166, 12, self.lcd, r=2)
            text_at(img, 20, 10, title, col, 7, self.label_font[0], "ls", self.label_font[1])
        else:
            text_at(img, 137.5, 10.3, title, col, 8, self.label_font[0], "ms", self.label_font[1])
        return img

    def groove_h(self, img, x0, y0, x1, y1):
        G.rrect(img, x0, y0, x1, y1, (y1 - y0) / 2, (15, 15, 18), outline=(70, 70, 76))

    def fill_h(self, img, x0, y0, x1, y1, frac):
        if frac > 0.01:
            G.rrect(img, x0, y0, x0 + (x1 - x0) * frac, y1, (y1 - y0) / 2, [(0.0, self.accent2), (1.0, self.accent)])

    def thumb(self, img, w, h, pressed, vertical=False):
        top, bot = ((235, 235, 238), (150, 150, 156)) if not pressed else ((150, 150, 156), (220, 220, 224))
        G.rrect(img, 0, 0, w, h, min(w, h) / 2.5, [(0.0, top), (1.0, bot)], outline=(20, 20, 22))
        if vertical:
            rect(img, 2, h / 2 - 0.4, w - 2, h / 2 + 0.4, (40, 40, 44))
        else:
            rect(img, w / 2 - 0.4, 2, w / 2 + 0.4, h - 2, (40, 40, 44))

    def eq_slot(self, cell, p):
        """cell 14x63 (+1 margin) for an EQ slider frame showing value p (0..1)."""
        G.rrect(cell, 5, 1, 9, 62, 2, (12, 12, 14), outline=(70, 70, 76))
        center, pos = 31.5, 5.5 + (1 - p) * 51
        y0, y1 = sorted((center, pos))
        if y1 - y0 > 0.3:
            rect(cell, 6, y0, 8, y1, mix(self.accent, self.accent2, abs(p - 0.5) * 2))
        rect(cell, 3, center - 0.3, 5, center + 0.3, self.label_col)
        rect(cell, 9, center - 0.3, 11, center + 0.3, self.label_col)

    def digit(self, img, x, y, ch):
        G.seg7(img, x, y, ch, self.lcd_text, self.lcd_ghost)

    def logo(self, img, x0, y0, x1, y1):
        text_at(img, (x0 + x1) / 2, (y0 + y1) / 2 + 3, "RA", self.label_col, 8, "ariblk.ttf", "ms")

    def label(self, img, x, y, s, col=None, size=5.5, anchor="ms"):
        text_at(img, x, y, s, col or self.label_col, size, self.label_font[0], anchor, self.label_font[1])

    def make_panel(self):
        """Optional: set self.panel (+ anims / extra images)."""

    def main_extras(self, main):
        """Optional: decorate main.bmp / add main window animations."""

    # ------------------------------------------------------------ cassette deck (back side of the playlist)
    def deck_text(self):
        """(main label colour, secondary label colour) on the deck face"""
        return self.label_col, mix(self.label_col, self.body(1, 1).getpixel((0, 0)), 0.35)

    def cassette_deck(self, title="RETROAMP  STEREO CASSETTE DECK", screws=False, label_extra=None):
        W, H = 275, 116
        tc, tc2 = self.deck_text()
        p = self.body(W, H)
        self.frame(p, 0, 0, W, H)
        if screws:
            for cx, cy in ((4, 4), (271, 4), (4, 112), (271, 112)):
                screw(p, cx, cy, 1.8)
        text_at(p, 10, 12, title, tc, 5.6, self.label_font[0], "ls", self.label_font[1])
        text_at(p, 265, 12, "DOLBY B·C NR", tc2, 5, self.label_font[0], "rs", self.label_font[1])
        # ---- realistic cassette behind the smoked-glass door of a 90s deck
        G.rrect(p, 8, 15, 168, 109, 3, [(0.0, (30, 30, 33)), (1.0, (10, 10, 11))], outline=(2, 2, 3), ow=0.6)
        rect(p, 9, 15.6, 167, 16.1, (90, 90, 96))
        G.rrect(p, 11.5, 18.5, 164.5, 82.5, 2, (6, 6, 7))  # glass frame
        reels = self._cassette(p, 14, 18, 148, 93, label_extra)
        raw = p.copy()  # the cassette before the glass - used for the reel animation frames
        self._glass(p, 0, 0)
        # lower part of the door: opaque strip with lettering and two round buttons
        G.rrect(p, 12, 84, 164, 106, 2, [(0.0, (34, 34, 37)), (0.5, (20, 20, 22)), (1.0, (12, 12, 13))],
                outline=(60, 60, 66), ow=0.35)
        G.rrect(p, 34, 89, 142, 100, 1.5, [(0.0, (16, 16, 18)), (1.0, (6, 6, 7))], outline=(70, 70, 76), ow=0.3)
        text_at(p, 88, 96.6, self.deck_strip, (226, 226, 230), 4.6, "arialbd.ttf", "ms")
        for bx in (23, 153):
            G.ellipse(p, bx - 4.6, 90.4, bx + 4.6, 99.6, [(0.0, (60, 60, 66)), (1.0, (8, 8, 9))], outline=(0, 0, 0), ow=0.4)
            G.ellipse(p, bx - 3, 92, bx + 3, 98, [(0.0, (10, 10, 11)), (1.0, (40, 40, 44))])
        # printing on the glass
        text_at(p, 16, 24.5, "DECK A", (196, 196, 200), 4.2, "arialbd.ttf", "ls")
        text_at(p, 160, 24.5, "PLAYBACK / RECORDING", (170, 170, 176), 3.6, "arial.ttf", "rs")
        # ---- VU meters
        for i, (mx, ch, lab) in enumerate(((176, "left", "L"), (222, "right", "R"))):
            self._vu_face(p, mx, 18, 44, 34, lab)
        # counter + buttons
        self.inset(p, 176, 57, 214, 70, self.lcd, r=1.5)
        text_at(p, 176, 76, "COUNTER", tc2, 4.0, self.label_font[0], "ls", self.label_font[1])
        self.inset(p, 222, 57, 266, 70, self.lcd, r=1.5)
        text_at(p, 222, 76, "TIME", tc2, 4.0, self.label_font[0], "ls", self.label_font[1])
        for i, (lab, act) in enumerate((("REW", "prev"), ("PLAY", "play"), ("FF", "next"), ("STOP", "stop"))):
            bx = 176 + i * 23
            self.button(p, bx, 79, 21, 12, False, label=lab)
            self.buttons.append({"X": bx, "Y": 79, "W": 21, "H": 12, "Action": act})
        self.button(p, 176, 95, 90, 13, False, label="TURN  ›  PLAYLIST")
        self.buttons.append({"X": 176, "Y": 95, "W": 90, "H": 13, "Action": "playlist"})
        self.panel = p
        # reels: spinning hubs (frames = bare cassette + hub, then the same smoked glass)
        frames = 12
        for idx, (cx, cy, pack) in enumerate(reels):
            sheet = Image.new("RGB", (S(18) * frames, S(18)))
            bgc = crop(raw, cx - 9, cy - 9, 18, 18)
            for f in range(frames):
                cell = bgc.copy()
                self._hub(cell, 9, 9, f * (60.0 / frames))
                self._glass(cell, cx - 9, cy - 9)
                sheet.paste(cell, (S(18) * f, 0))
            name = "reel%d" % (idx + 1)
            self.extra[name] = sheet
            self.anims.append({"Window": "panel", "Mode": "spin", "Image": name + ".png", "FrameW": 18, "FrameH": 18,
                               "Frames": frames, "X": cx - 9, "Y": cy - 9, "FPS": 14 if idx == 0 else 22})
        # needles
        nfr = 32
        for idx, (mx, ch) in enumerate(((176, "left"), (222, "right"))):
            sheet = Image.new("RGB", (S(44) * nfr, S(34)))
            face = crop(p, mx, 18, 44, 34)
            for f in range(nfr):
                cell = face.copy()
                self._needle(cell, 44, 34, f / (nfr - 1))
                sheet.paste(cell, (S(44) * f, 0))
            name = "vu%s" % ch
            self.extra[name] = sheet
            self.anims.append({"Window": "panel", "Mode": "level", "Channel": ch, "Image": name + ".png",
                               "FrameW": 44, "FrameH": 34, "Frames": nfr, "X": mx, "Y": 18, "Attack": 0.55,
                               "Release": 0.10})
        # label text (handwritten title), counter, time, play LED
        self.anims.append({"Window": "panel", "Mode": "text", "Text": "title", "X": 25, "Y": 28, "W": 125, "H": 9,
                           "Font": "Segoe Print", "Size": 6.5, "Bold": 1, "Color": "#18205A"})
        self.anims.append({"Window": "panel", "Mode": "text", "Text": "counter", "X": 177, "Y": 58, "W": 36, "H": 11,
                           "Font": "Bahnschrift", "Size": 9, "Bold": 1, "Color": "#%02X%02X%02X" % self.lcd_text, "Align": "center"})
        self.anims.append({"Window": "panel", "Mode": "text", "Text": "time", "X": 223, "Y": 58, "W": 42, "H": 11,
                           "Font": "Bahnschrift", "Size": 9, "Bold": 1, "Color": "#%02X%02X%02X" % self.lcd_text, "Align": "center"})
        led = Image.new("RGB", (S(5) * 3, S(5)))
        for f, col in enumerate(((60, 30, 12), (60, 255, 80), (255, 170, 30))):
            cell = crop(p, 214, 80, 5, 5)
            G.ellipse(cell, 0.6, 0.6, 4.4, 4.4, col, outline=(10, 10, 10), ow=0.4)
            led.paste(cell, (S(5) * f, 0))
        self.extra["led"] = led
        self.anims.append({"Window": "panel", "Mode": "state", "Image": "led.png", "FrameW": 5, "FrameH": 5,
                           "Frames": 3, "X": 214, "Y": 80})

    cassette_style = "classic"  # classic (grey label, blue stripe) | ar (black & gold, high position)
    cassette_stripe = (30, 112, 212)
    glass_mul, glass_add = 0.60, (2, 3, 6)
    deck_strip = "FULL LOGIC COMPUTER CONTROL"

    def _hub(self, img, cx, cy, rot):
        # white plastic hub: outer ring, hole with six teeth pointing inwards, dark spindle opening
        G.ellipse(img, cx - 6.6, cy - 6.6, cx + 6.6, cy + 6.6, [(0.0, (238, 238, 234)), (1.0, (186, 186, 182))],
                  outline=(110, 110, 108), ow=0.35)
        G.ellipse(img, cx - 4.7, cy - 4.7, cx + 4.7, cy + 4.7, (28, 28, 30))
        for k in range(6):
            a = math.radians(rot + k * 60)
            ca, sa = math.cos(a), math.sin(a)
            poly(img, [(cx + ca * 4.8 - sa * 0.75, cy + sa * 4.8 + ca * 0.75), (cx + ca * 4.8 + sa * 0.75, cy + sa * 4.8 - ca * 0.75),
                       (cx + ca * 3.0 + sa * 0.55, cy + sa * 3.0 - ca * 0.55), (cx + ca * 3.0 - sa * 0.55, cy + sa * 3.0 + ca * 0.55)],
                 (220, 220, 216))
        for k in range(3):  # moulding marks on the ring make the rotation visible
            a = math.radians(rot + 30 + k * 120)
            G.ellipse(img, cx + math.cos(a) * 5.7 - 0.45, cy + math.sin(a) * 5.7 - 0.45, cx + math.cos(a) * 5.7 + 0.45,
                      cy + math.sin(a) * 5.7 + 0.45, (140, 140, 136))

    def _cassette(self, p, x, y, w, h, label_extra=None):
        """Compact cassette seen from the front (proportions of a real C-90). Returns the reel geometry."""
        ar = self.cassette_style == "ar"
        X = lambda f: x + f * w
        Y = lambda f: y + f * h
        # translucent smoky shell with the typical fine grid texture
        G.rrect(p, x, y, x + w, y + h, 3.5, [(0.0, (78, 76, 84)), (1.0, (44, 42, 48))], outline=(18, 18, 20), ow=0.5)
        Wp, Hp = S(w), S(h)
        yy, xx = np.mgrid[0:Hp, 0:Wp]
        step = max(2, int(1.3 * K))
        grid = (((xx % step) == 0) | ((yy % step) == 0)).astype(np.float32)
        region = np.asarray(crop(p, x, y, w, h)).astype(np.float32)
        region *= (1 - 0.10 * grid[:, :, None])
        mask = G._rr_mask_px(Wp, Hp, 3.5 * K)
        p.paste(np_to_img(region), (S(x), S(y)), mask)
        rect(p, X(0.02), Y(0.02), X(0.98), Y(0.025), (120, 118, 126))
        for sx, sy in ((0.03, 0.05), (0.97, 0.05), (0.03, 0.95), (0.97, 0.95), (0.5, 0.86)):
            G.ellipse(p, X(sx) - 1.4, Y(sy) - 1.4, X(sx) + 1.4, Y(sy) + 1.4, (36, 34, 40), outline=(100, 98, 106), ow=0.3)
        # label
        lab = (20, 20, 22) if ar else (160, 170, 168)
        G.rrect(p, X(0.045), Y(0.075), X(0.955), Y(0.70), 1.5, lab)
        G.rrect(p, X(0.065), Y(0.115), X(0.935), Y(0.305), 1, (238, 236, 228) if not ar else (226, 214, 176))
        for fy in (0.175, 0.235, 0.295):
            rect(p, X(0.08), Y(fy), X(0.92), Y(fy) + 0.3, (150, 160, 175) if not ar else (170, 140, 70))
        # tape window with the tape packs (clipped to the window)
        wx0, wy0, wx1, wy1 = X(0.20), Y(0.36), X(0.80), Y(0.635)
        G.rrect(p, wx0 - 0.6, wy0 - 0.6, wx1 + 0.6, wy1 + 0.6, 3.5, (90, 90, 96))
        G.rrect(p, wx0, wy0, wx1, wy1, 3, (18, 18, 20))
        reels = [(X(0.31), Y(0.495), 16.5), (X(0.69), Y(0.495), 10.0)]
        win = Image.new("RGB", (S(wx1 - wx0), S(wy1 - wy0)), (18, 18, 20))
        for cx, cy, r in reels:
            lx, ly = cx - wx0, cy - wy0
            G.ellipse(win, lx - r, ly - r, lx + r, ly + r, [(0.0, (78, 52, 34)), (1.0, (44, 28, 18))])
            for k in range(3):  # tape winding rings
                rr = r - 1.5 - k * 2.6
                if rr > 7:
                    ring = Image.new("L", win.size, 0)
                    ImageDraw.Draw(ring).ellipse([S(lx - rr), S(ly - rr), S(lx + rr), S(ly + rr)], outline=90, width=max(1, K // 3))
                    win.paste((96, 66, 44), (0, 0), ring)
        p.paste(win, (S(wx0), S(wy0)), G._rr_mask_px(win.width, win.height, 3 * K))
        # centre window with the length scale
        G.rrect(p, X(0.43), Y(0.40), X(0.57), Y(0.595), 1, (62, 60, 68), outline=(110, 108, 116), ow=0.3)
        for i in range(9):
            tx = X(0.445) + i * (w * 0.11 / 8)
            rect(p, tx, Y(0.49) - (1.4 if i % 4 == 0 else 0.8), tx + 0.3, Y(0.49), (220, 220, 224))
        rect(p, X(0.445), Y(0.49), X(0.555), Y(0.49) + 0.3, (220, 220, 224))
        # side letter / brand / stripe
        if ar:
            text_at(p, X(0.08), Y(0.56), "A", (214, 180, 96), 7, "ariblk.ttf", "ls")
            text_at(p, X(0.93), Y(0.57), "AR90", (214, 180, 96), 6.5, "georgiab.ttf", "rs")
            text_at(p, X(0.5), Y(0.69), "HIGH PRECISION  ·  NORMAL POSITION", (196, 164, 90), 3.4, "arial.ttf", "ms")
            rect(p, X(0.045), Y(0.645), X(0.955), Y(0.65), (196, 164, 90))
        else:
            text_at(p, X(0.08), Y(0.57), "A", (248, 248, 248), 7.5, "arialbd.ttf", "ls")
            text_at(p, X(0.93), Y(0.57), "C-90", (60, 64, 66), 5, "arialbd.ttf", "rs")
            rect(p, X(0.045), Y(0.645), X(0.955), Y(0.70), self.cassette_stripe)
        if label_extra:
            label_extra(p)
        return [(int(round(cx)), int(round(cy)), r) for cx, cy, r in reels]

    def _glass(self, img, ox, oy, rect_=(12, 19, 164, 82)):
        """Smoked glass of the cassette door over `img` whose top-left is skin point (ox, oy)."""
        a = np.asarray(img).astype(np.float32)
        Hp, Wp = a.shape[:2]
        yy, xx = np.mgrid[0:Hp, 0:Wp].astype(np.float32)
        sx, sy = ox + xx / K, oy + yy / K
        x0, y0, x1, y1 = rect_
        inside = (sx >= x0) & (sx < x1) & (sy >= y0) & (sy < y1)
        t = a * np.array(self.glass_mul, np.float32) + np.array(self.glass_add, np.float32)  # tint
        d = (sx - x0) * 0.55 + (sy - y0)
        band = np.clip(1 - np.abs(d - 48) / 10, 0, 1) * 0.16 + np.clip(1 - np.abs(d - 66) / 3, 0, 1) * 0.10
        glare = np.clip(1 - (sy - y0) / 7, 0, 1) * 0.08
        t += (255 - t) * (band + glare)[:, :, None]
        out = np.where(inside[:, :, None], t, a)
        img.paste(np_to_img(out))

    def _vu_face(self, p, x, y, w, h, lab):
        G.rrect(p, x, y, x + w, y + h, 2, (10, 10, 10))
        G.rrect(p, x + 1, y + 1, x + w - 1, y + h - 1, 1.5, [(0.0, (255, 214, 130)), (1.0, (232, 160, 60))])
        cx, cy, r = x + w / 2, y + h + 8, h * 0.95
        for i in range(11):
            a = math.radians(-50 + i * 10)
            red = i >= 8
            x0, y0 = cx + math.sin(a) * (r - 4), cy - math.cos(a) * (r - 4)
            x1, y1 = cx + math.sin(a) * (r - (7 if i % 2 == 0 else 5.5)), cy - math.cos(a) * (r - (7 if i % 2 == 0 else 5.5))
            thick_line(p, x0, y0, x1, y1, 0.45, (200, 20, 20) if red else (40, 30, 20))
        pts = []
        for i in range(31):
            a = math.radians(-50 + i * (100 / 30))
            pts.append((cx + math.sin(a) * (r - 4), cy - math.cos(a) * (r - 4)))
        for i in range(30):
            thick_line(p, *pts[i], *pts[i + 1], 0.35, (200, 20, 20) if i >= 24 else (40, 30, 20))
        text_at(p, x + w / 2, y + h - 6, "VU", (60, 40, 20), 5, "georgiab.ttf", "ms")
        text_at(p, x + 3, y + h - 2.5, lab, (60, 40, 20), 4, "bahnschrift.ttf", "ls", "Bold")
        text_at(p, x + w - 3, y + h - 2.5, "+3", (200, 20, 20), 3.6, "bahnschrift.ttf", "rs", "Bold")

    def _needle(self, cell, w, h, level):
        cx, cy, r = w / 2, h + 8, h * 0.95
        a = math.radians(-50 + level * 100)
        thick_line(cell, cx, cy, cx + math.sin(a) * (r - 2), cy - math.cos(a) * (r - 2), 0.55, (15, 10, 8))
        G.ellipse(cell, cx - 3, h - 3.2, cx + 3, h + 2.8, (30, 30, 32))


    # ---------------------------------------------------------------- RetroAmp extras: TURN + DSP style
    def turn_sprites(self, pl):
        """TURN button (normal at 100,43 / pressed at 100,58) shown left of the playlist window buttons."""
        for y, pressed in ((43, False), (58, True)):
            cell = Image.new("RGB", (S(30), S(11)))
            cell.paste(crop(pl, 127, 2, 25, 11), (0, 0))
            cell.paste(crop(pl, 127, 2, 5, 11), (S(25), 0))
            self.button(cell, 0, 0, 30, 11, pressed, label="TURN", kind="toggle")
            paste(pl, cell, 100, y)

    def dsp_assets(self):
        body = self.body(251, 86)
        btn = Image.new("RGB", (S(104), S(48)))
        for f, (on, pr) in enumerate(((False, False), (True, False), (False, True), (True, True))):
            cell = crop(body, 140, 4, 104, 12)
            self.button(cell, 0, 0, 104, 12, pr, sel=on, kind="toggle")
            paste(btn, cell, 0, f * 12)
        self.extra["dspbody"] = body
        self.extra["dspbtn"] = btn
        t, ton, lab = self.dsp_colors()
        hx = lambda c: "#%02X%02X%02X" % tuple(c)
        return ["[Dsp]", "Body=dspbody.png", "Button=dspbtn.png", "Text=" + hx(t), "TextOn=" + hx(ton),
                "Label=" + hx(lab), "Font=" + self.dsp_font, "", "[Playlist]", "TurnSprite=1", ""]

    # ---------------------------------------------------------------- LCD glow helper
    def lcd_glowify(self, img):
        if self.display_glow <= 0:
            return img
        # brighten pixels that are text-coloured by adding a blurred copy of the bright parts
        arr = np.asarray(img).astype(np.int16)
        bg = np.array(self.lcd, dtype=np.int16)
        mask = (np.abs(arr - bg).sum(axis=2) > 60)
        layer = np.zeros_like(arr)
        layer[mask] = arr[mask]
        lay = np_to_img(layer)
        return glow(img, lay, 0.6, self.display_glow)

    # ================================================================ sprite sheets
    def make_main(self):
        img = self.body(275, 116)
        self.frame(img, 0, 0, 275, 116)
        # display: right part to y=55, left part (clutter, time, vis) to y=68
        self.inset(img, 4, 16, 103, 69, self.lcd, r=3)
        self.inset(img, 4, 16, 272, 56, self.lcd, r=3)
        rect(img, 5, 17, 271, 55, self.lcd)
        rect(img, 5, 17, 102, 68, self.lcd)
        for dx in (48, 60, 78, 90):
            self.digit(img, dx, 26, " ")
        G.ellipse(img, 72, 29.6, 74, 31.6, self.lcd_text)
        G.ellipse(img, 71.8, 35, 73.8, 37, self.lcd_text)
        text_at(img, 128.5, 49, "KBPS", self.lcd_dim, 5, self.text_font[0], "ls", self.text_font[1])
        text_at(img, 167.5, 49, "KHZ", self.lcd_dim, 5, self.text_font[0], "ls", self.text_font[1])
        self.groove_h(img, 15, 74, 265, 80)
        self.subpanel(img, 159, 85, 273, 113)
        self.logo(img, 240, 86, 272, 112)
        self.main_extras(img)
        return img

    def make_titlebar(self):
        img = new_img(344, 87, self.body(344, 87))
        for (x, y, pressed, kind) in [
            (0, 0, False, "menu"), (0, 9, True, "menu"), (9, 0, False, "min"), (9, 9, True, "min"),
            (18, 0, False, "close"), (18, 9, True, "close"), (0, 18, False, "shade"), (9, 18, True, "shade"),
            (0, 27, False, "unshade"), (9, 27, True, "unshade"),
        ]:
            self.gadget(img, x, y, kind, pressed)
        paste(img, self.title_bar(True, self.title), 27, 0)
        paste(img, self.title_bar(False, self.title), 27, 15)
        paste(img, self.title_bar(True, self.title, shade=True), 27, 29)
        paste(img, self.title_bar(False, self.title, shade=True), 27, 42)
        paste(img, self.title_bar(True, self.title), 27, 57)
        paste(img, self.title_bar(False, self.title), 27, 72)
        for x, col in ((304, self.lcd_dim), (312, self.lcd_ghost)):
            rect(img, x, 0, x + 8, 43, self.lcd)
            for oy, oh in ((3, 8), (11, 7), (18, 7), (25, 8), (33, 7)):
                rect(img, x + 2, oy + oh // 2, x + 6, oy + oh // 2 + 1, col)
        for x, y, h in ((304, 47, 8), (312, 55, 7), (320, 62, 7), (328, 69, 8), (336, 77, 7)):
            rect(img, x, y, x + 8, y + h, self.lcd)
            rect(img, x + 1, y + h // 2, x + 7, y + h // 2 + 1, self.lcd_text)
        rect(img, 0, 36, 17, 43, self.lcd); hline(img, 0, 17, 39, self.lcd_dim)
        for x in (17, 20, 23):
            rect(img, x, 36, x + 3, 43, self.lcd_text)
        return img

    def make_cbuttons(self, main):
        img = new_img(136, 36)
        spec = [(0, 23, 18, 16, 88, g_prev), (23, 23, 18, 39, 88, g_play), (46, 23, 18, 62, 88, g_pause),
                (69, 23, 18, 85, 88, g_stop), (92, 22, 18, 108, 88, g_next)]
        for sx, w, h, tx, ty, g in spec:
            for row, pressed in ((0, False), (18, True)):
                cell = crop(main, tx, ty, w, h)
                self.button(cell, 0, 0, w, h, pressed, glyph=g)
                paste(img, cell, sx, row)
        for row, pressed in ((0, False), (16, True)):
            cell = crop(main, 136, 89, 22, 16)
            self.button(cell, 0, 0, 22, 16, pressed, glyph=g_eject)
            paste(img, cell, 114, row)
        return img

    def make_shufrep(self, main):
        img = new_img(92, 85)
        for row, (sel, pressed) in enumerate([(False, False), (False, True), (True, False), (True, True)]):
            cell = crop(main, 164, 89, 47, 15)
            self.button(cell, 0, 0, 46, 15, pressed, label="SHUFFLE", sel=sel, kind="toggle")
            paste(img, cell, 28, row * 15)
            cell = crop(main, 210, 89, 28, 15)
            self.button(cell, 1, 0, 27, 15, pressed, label="REP", sel=sel, kind="toggle")
            paste(img, cell, 0, row * 15)
        for lab, sx, tx in (("EQ", 0, 219), ("PL", 23, 242)):
            for (ox, oy, sel, pressed) in ((0, 61, False, False), (0, 73, True, False), (46, 61, False, True), (46, 73, True, True)):
                cell = crop(main, tx, 58, 23, 12)
                self.button(cell, 0, 0, 23, 12, pressed, label=lab, sel=sel, kind="toggle")
                paste(img, cell, sx + ox, oy)
        return img

    def make_numbers(self, ex=False):
        img = new_img(108 if ex else 99, 13, self.lcd)
        for d in range(10):
            self.digit(img, d * 9, 0, str(d))
        self.digit(img, 90, 0, " ")
        if ex:
            self.digit(img, 99, 0, "-")
        return self.lcd_glowify(img)

    def make_text(self):
        img = new_img(155, 18, self.lcd)
        f = ttf(self.text_font[0], 5.0 * K / 0.70, self.text_font[1])
        for r, line in enumerate(G.TEXT_ROWS):
            for c, ch in enumerate(line[:31]):
                if ch == " ":
                    continue
                cw, chh = 5 * K, 6 * K
                d = ImageDraw.Draw(Image.new("RGB", (1, 1)))
                gw = d.textlength(ch, font=f)
                tmp = Image.new("RGB", (max(cw, int(gw) + 4), chh), self.lcd)
                ImageDraw.Draw(tmp).text((tmp.width / 2, 5.15 * K), ch, font=f, fill=self.lcd_text, anchor="ms")
                if tmp.width > cw * 0.94:
                    tmp = tmp.resize((int(cw * 0.94), chh), Image.Resampling.LANCZOS)
                cell = Image.new("RGB", (cw, chh), self.lcd)
                cell.paste(tmp, ((cw - tmp.width) // 2, 0))
                img.paste(cell, (c * cw, r * chh))
        return self.lcd_glowify(img)

    def make_monoster(self):
        img = new_img(58, 24, self.lcd)
        for y, col in ((3, self.lcd_text), (15, self.lcd_ghost)):
            for cx, s, mw in ((14.5, "STEREO", 27), (42.5, "MONO", 25)):
                f = ttf(self.text_font[0], 5.5 * K / 0.70, self.text_font[1])
                d = ImageDraw.Draw(img)
                if d.textlength(s, font=f) > mw * K:
                    f = ttf(self.text_font[0], 5.5 * K / 0.70 * mw * K / d.textlength(s, font=f), self.text_font[1])
                d.text((cx * K, (y + 8) * K), s, font=f, fill=col, anchor="ms")
        return self.lcd_glowify(img)

    def make_playpaus(self):
        img = new_img(42, 9, self.lcd)
        c = self.lcd_text
        poly(img, [(3, 1), (3, 8), (7.5, 4.5)], c)
        rect(img, 11, 1, 13, 8, c); rect(img, 15, 1, 17, 8, c)
        rect(img, 20, 1, 26, 7, c)
        G.ellipse(img, 39, 3, 41.5, 5.5, self.accent2)
        return self.lcd_glowify(img)

    def make_volume(self, main, balance=False):
        img = new_img(68, 433)
        tx, w = (177, 38) if balance else (107, 68)
        for i in range(28):
            cell = crop(main, tx, 57, w, 13)
            self.groove_h(cell, 1, 4.5, w - 1, 8.5)
            if balance:
                half = (w / 2 - 3) * i / 27
                if half > 0.3:
                    G.rrect(cell, w / 2 - half, 5.2, w / 2 + half, 7.8, 1.2, self.accent)
            else:
                self.fill_h(cell, 1.7, 5.2, w - 1.7, 7.8, i / 27)
            paste(img, cell, 9 if balance else 0, i * 15)
        for x, pressed in ((15, False), (0, True)):
            cell = crop(main, 140, 58, 14, 11)
            self.thumb(cell, 14, 11, pressed)
            paste(img, cell, x, 422)
        return img

    def make_posbar(self, main):
        img = new_img(307, 10)
        paste(img, crop(main, 16, 72, 248, 10), 0, 0)
        for x, pressed in ((248, False), (278, True)):
            cell = crop(main, 100, 72, 29, 10)
            self.thumb(cell, 29, 10, pressed)
            paste(img, cell, x, 0)
        return img

    def make_eqmain(self):
        img = new_img(315 and 275, 315)
        bg = self.body(275, 116)
        self.frame(bg, 0, 0, 275, 116)
        self.eq_decor(bg)
        paste(bg, self.title_bar(True, "EQUALIZER"), 0, 0)
        for txt, y in (("+12 dB", 45), ("0 dB", 71), ("-12 dB", 96.5)):
            self.label(bg, 56, y, txt, size=5)
        self.label(bg, 25, 110, "PREAMP", size=5)
        for i, lbl in enumerate(EQ_LABELS):
            self.label(bg, 78 + i * 18 + 7.5, 110, lbl, size=5)
        paste(img, bg, 0, 0)
        tb_on, tb_off = self.title_bar(True, "EQUALIZER"), self.title_bar(False, "EQUALIZER")
        paste(img, tb_on, 0, 134)
        paste(img, tb_off, 0, 149)
        for y, pressed in ((116, False), (125, True)):
            cell = crop(tb_on, 264, 3, 9, 9)
            self.gadget(cell, 0, 0, "close", pressed)
            paste(img, cell, 0, y)
        cell = crop(tb_off, 254, 3, 9, 9)
        self.gadget(cell, 0, 0, "shade", False)
        paste(img, cell, 254, 152)
        for name, w, xs, tx in (("ON", 26, (10, 128, 69, 187), 14), ("AUTO", 32, (36, 154, 95, 213), 40)):
            for x, sel, pressed in zip(xs, (False, False, True, True), (False, True, False, True)):
                cell = crop(bg, tx, 18, w, 12)
                self.button(cell, 0, 0, w, 12, pressed, label=name, sel=sel, kind="toggle")
                paste(img, cell, x, 119)
        for n in range(28):
            cell = new_img(15, 65)
            paste(cell, crop(bg, 78, 38, 14, 63), 0, 0)
            self.eq_slot(cell, n / 27.0)
            paste(img, cell, 13 + (n % 14) * 15, 164 + (n // 14) * 65)
        for y, pressed in ((164, False), (176, True)):
            cell = crop(bg, 79, 60, 11, 11)
            self.thumb(cell, 11, 11, pressed, vertical=True)
            paste(img, cell, 0, y)
        for y, pressed in ((164, False), (176, True)):
            cell = crop(bg, 217, 18, 44, 12)
            self.button(cell, 0, 0, 44, 12, pressed, label="PRESETS", kind="toggle")
            paste(img, cell, 224, y)
        g = new_img(113, 19, self.lcd)
        for x in range(0, 113, 12):
            rect(g, x, 0, x + 0.35, 19, self.lcd_ghost)
        rect(g, 0, 9.3, 113, 9.7, self.lcd_ghost)
        paste(img, g, 0, 294)
        for y in range(19):
            rect(img, 115, 294 + y, 116, 295 + y, mix(self.accent2, self.accent, abs(y - 9) / 9))
        rect(img, 0, 314, 113, 315, self.lcd)
        for x in range(0, 113, 2):
            rect(img, x, 314, x + 1, 315, self.lcd_dim)
        return img

    def eq_decor(self, bg):
        """Background of the EQ window below the title (override for style)."""
        self.subpanel(bg, 6, 15, 269, 113)
        self.inset(bg, 85, 16, 200, 37, self.lcd, r=2)

    def make_pledit(self):
        img = new_img(280, 186)

        def top_piece(w, active, kind):
            p = self.body(w, 20)
            rect(p, 0, 0, w, 1, mix(self.label_col, BLACK, 0.4))
            rect(p, 0, 19, w, 20, BLACK)
            col = self.label_col if active else mix(self.label_col, (80, 80, 80), 0.5)
            if kind == "title":
                text_at(p, w / 2, 12.5, "PLAYLIST", col, 8, self.label_font[0], "ms", self.label_font[1])
            if kind in ("tile", "left", "right"):
                x0 = 13 if kind == "left" else 0
                x1 = w - 22 if kind == "right" else w
                for yy in (7, 10):
                    rect(p, x0, yy, x1, yy + 0.5, mix(col, BLACK, 0.55))
            if kind == "left":
                rect(p, 0, 0, 1, 20, mix(self.label_col, BLACK, 0.4))
            if kind == "right":
                rect(p, w - 1, 0, w, 20, BLACK)
                self.gadget(p, 5, 3, "deck" if self.has_panel else "shade", False)
                self.gadget(p, 14, 3, "close", False)
            return p

        for active, y in ((True, 0), (False, 21)):
            paste(img, top_piece(25, active, "left"), 0, y)
            paste(img, top_piece(100, active, "title"), 26, y)
            paste(img, top_piece(25, active, "tile"), 127, y)
            paste(img, top_piece(25, active, "right"), 153, y)
        lt = self.body(12, 29)
        rect(lt, 0, 0, 1, 29, mix(self.label_col, BLACK, 0.4)); rect(lt, 11, 0, 12, 29, BLACK)
        paste(img, lt, 0, 42)
        rt = self.body(20, 29)
        rect(rt, 19, 0, 20, 29, BLACK); rect(rt, 0, 0, 1, 29, BLACK)
        rect(rt, 4, 0, 14, 29, BLACK); rect(rt, 5, 0, 13, 29, mix(self.pl_bg, (60, 60, 60), 0.3))
        paste(img, rt, 31, 42)
        for x, pressed in ((52, False), (61, True)):
            cell = new_img(8, 18, self.pl_bg)
            self.thumb(cell, 8, 18, pressed, vertical=True)
            paste(img, cell, x, 53)
        corner = top_piece(25, True, "right")
        for x, kind, gx in ((52, "close", 14), (62, "deck" if self.has_panel else "shade", 5), (150, "unshade", 5)):
            cell = crop(corner, gx, 3, 9, 9)
            self.gadget(cell, 0, 0, kind, True)
            paste(img, cell, x, 42)
        for (x, y, w) in ((72, 57, 25), (72, 42, 25), (99, 57, 50), (99, 42, 50)):
            paste(img, self.body(w, 14), x, y)

        def bottom_base(w):
            b = self.body(w, 38)
            rect(b, 0, 0, w, 1, BLACK)
            rect(b, 0, 37, w, 38, BLACK)
            return b

        paste(img, bottom_base(25), 179, 0)
        bl = bottom_base(125)
        rect(bl, 0, 0, 1, 38, mix(self.label_col, BLACK, 0.4))
        for bx, g in ((14, g_plus), (43, g_minus), (72, g_lines), (101, g_star)):
            self.button(bl, bx, 8, 25, 18, False, glyph=g, kind="small")
        paste(img, bl, 0, 72)
        br = bottom_base(150)
        rect(br, 149, 0, 150, 38, BLACK)
        self.inset(br, 2, 4, 101, 35, self.lcd, r=3)
        c = self.lcd_text
        oy = 22
        rect(br, 6, oy + 1, 7, oy + 8, c); poly(br, [(11.5, oy + 1), (11.5, oy + 8), (7.5, oy + 4.5)], c)
        poly(br, [(15, oy + 1), (15, oy + 8), (19, oy + 4.5)], c)
        rect(br, 25, oy + 1, 27, oy + 8, c); rect(br, 29, oy + 1, 31, oy + 8, c)
        rect(br, 34, oy + 2, 40, oy + 8, c)
        poly(br, [(43, oy + 1), (43, oy + 8), (47, oy + 4.5)], c); rect(br, 47, oy + 1, 48, oy + 8, c)
        poly(br, [(51, oy + 5), (59, oy + 5), (55, oy + 1)], c); rect(br, 51, oy + 6, 59, oy + 8, c)
        self.button(br, 103, 8, 25, 18, False, glyph=g_folder, kind="small")
        poly(br, [(132, 8), (139, 8), (135.5, 4)], self.label_col)
        poly(br, [(132, 11), (139, 11), (135.5, 15)], self.label_col)
        for i in range(4):
            thick_line(br, 148 - i * 4, 36.5, 148.5, 36 - i * 4, 0.8, mix(self.label_col, BLACK, 0.4))
        paste(img, br, 126, 72)
        vb = bottom_base(75)
        self.inset(vb, 2, 6, 73, 32, self.lcd, r=3)
        paste(img, vb, 205, 0)
        menus = [(0, ["URL", "DIR", "FILE"]), (54, ["ALL", "CROP", "SEL", "MISC"]), (104, ["INV", "NONE", "ALL"]),
                 (154, ["SORT", "INFO", "OPTS"]), (204, ["NEW", "SAVE", "LOAD"])]
        for x, items in menus:
            for i, lab in enumerate(items):
                for sel in (False, True):
                    cell = new_img(22, 18, BLACK)
                    self.button(cell, 0, 0, 22, 18, sel, label=lab, sel=sel, kind="small")
                    paste(img, cell, x + (23 if sel else 0), 111 + i * 19)
        for x, h in ((48, 54), (100, 72), (150, 54), (200, 54), (250, 54)):
            rect(img, x, 111, x + 3, 111 + h, self.accent)
        return img

    def vis_colors(self):
        cols = [self.lcd, self.lcd_ghost]
        for i in range(16):
            cols.append(mix(self.vis_top, self.vis_bot, i / 15))
        for i in range(5):
            cols.append(mix(self.vis_top, self.vis_bot, i / 6))
        cols.append(self.vis_top)
        return cols

    # ================================================================ build
    def build(self):
        G.K = K
        random.seed(hash(self.key) & 0xFFFF)
        np.random.seed(hash(self.key) & 0xFFFF)
        main = self.make_main()
        files = {
            "main": main, "titlebar": self.make_titlebar(), "cbuttons": self.make_cbuttons(main),
            "shufrep": self.make_shufrep(main), "numbers": self.make_numbers(), "nums_ex": self.make_numbers(ex=True),
            "text": self.make_text(), "monoster": self.make_monoster(), "playpaus": self.make_playpaus(),
            "volume": self.make_volume(main), "balance": self.make_volume(main, balance=True),
            "posbar": self.make_posbar(main), "eqmain": self.make_eqmain(), "pledit": self.make_pledit(),
        }
        self.turn_sprites(files["pledit"])
        self.make_panel()
        extra_sections = self.dsp_assets()
        hexc = lambda c: "#%02X%02X%02X" % c
        texts = {
            "pledit.txt": "[Text]\r\nNormal=%s\r\nCurrent=%s\r\nNormalBG=%s\r\nSelectedBG=%s\r\nFont=%s\r\n"
                          % (hexc(self.pl_text), hexc(self.pl_cur), hexc(self.pl_bg), hexc(self.pl_sel), self.pl_font),
            "viscolor.txt": "".join("%d,%d,%d,\r\n" % c for c in self.vis_colors()),
            "readme.txt": "%s - RetroAmp HD skin\r\n" % self.name,
        }
        if self.panel is not None or self.anims:
            lines = ["; RetroAmp animation extension (see SKINNING.md)"]
            if self.panel is not None:
                lines += ["[Panel]", "Width=%d" % self.panel_size[0], "Height=%d" % self.panel_size[1], "Image=panel.png", ""]
                self.extra["panel"] = self.panel
            for i, a in enumerate(self.anims):
                lines.append("[Anim%d]" % (i + 1))
                for k, v in a.items():
                    lines.append("%s=%s" % (k, v))
                lines.append("")
            for i, b in enumerate(self.buttons):
                lines.append("[Button%d]" % (i + 1))
                for k, v in b.items():
                    lines.append("%s=%s" % (k, v))
                lines.append("")
            lines += extra_sections
            texts["anim.txt"] = "\r\n".join(lines)
        out = os.path.join(G.ROOT, "skins", "src", self.key)
        if os.path.isdir(out):
            shutil.rmtree(out)
        os.makedirs(out)
        for n, im in list(files.items()) + list(self.extra.items()):
            im.save(os.path.join(out, n + ".png"), optimize=True)
        for n, t in texts.items():
            with open(os.path.join(out, n), "w", newline="", encoding="utf-8") as f:
                f.write(t)
        with zipfile.ZipFile(os.path.join(G.ROOT, "skins", self.key + ".wsz"), "w", zipfile.ZIP_DEFLATED) as z:
            for n in sorted(os.listdir(out)):
                z.write(os.path.join(out, n), n)
        print("built", self.key)


def new_img(w, h, fill=(0, 0, 0)):
    if isinstance(fill, Image.Image):
        return fill
    return Image.new("RGB", (S(w), S(h)), fill)


# ============================================================================ textures
def brushed(w, h, base, var=10, seed=0, vert=(10, -8)):
    W, H = S(w), S(h)
    rng = np.random.default_rng(seed)
    streak = rng.normal(0, 1, (H, max(4, W // 30)))
    streak = np.asarray(Image.fromarray(((streak + 4) * 30).clip(0, 255).astype(np.uint8)).resize((W, H), Image.Resampling.BILINEAR)).astype(np.float32) / 30 - 4
    fine = rng.normal(0, 0.6, (H, W))
    grad = np.linspace(vert[0], vert[1], H)[:, None]
    v = streak * var * 0.5 + fine * var * 0.35 + grad
    arr = np.array(base, dtype=np.float32)[None, None, :] + v[:, :, None]
    return np_to_img(arr)


def wood(w, h, seed=0, dark=(70, 38, 18), light=(128, 76, 40)):
    W, H = S(w), S(h)
    rng = np.random.default_rng(seed)
    y, x = np.mgrid[0:H, 0:W].astype(np.float32)
    n1 = np.asarray(Image.fromarray(rng.integers(0, 255, (max(2, H // 24), max(2, W // 60)), dtype=np.uint8)).resize((W, H), Image.Resampling.BICUBIC)).astype(np.float32) / 255
    rings = np.sin((y / K) * 0.9 + np.sin(x / K / 37.0) * 2.2 + n1 * 6.0)
    fine = rng.normal(0, 0.08, (H, W))
    t = np.clip(0.5 + 0.35 * rings + fine, 0, 1)
    d, l = np.array(dark, np.float32), np.array(light, np.float32)
    arr = d[None, None] + (l - d)[None, None] * t[:, :, None]
    return np_to_img(arr)


def pinstripe(w, h, a=(236, 236, 236), b=(226, 226, 226)):
    W, H = S(w), S(h)
    arr = np.zeros((H, W, 3), np.float32)
    rows = (np.arange(H) // max(1, K // 2)) % 2
    arr[rows == 0] = a
    arr[rows == 1] = b
    return np_to_img(arr)


def screw(img, cx, cy, r=2.2, angle=30):
    G.ellipse(img, cx - r, cy - r, cx + r, cy + r, [(0.0, (210, 210, 214)), (1.0, (90, 90, 96))], outline=(20, 20, 22), ow=0.4)
    a = math.radians(angle)
    for da in (0, 90):
        b = a + math.radians(da)
        thick_line(img, cx - math.cos(b) * r * 0.7, cy - math.sin(b) * r * 0.7, cx + math.cos(b) * r * 0.7,
                   cy + math.sin(b) * r * 0.7, 0.45, (40, 40, 44))


# ============================================================================ 1. HiFi Tower
class HiFiTower(Kit):
    key = "HiFiTower"
    cassette_style = "ar"

    def dsp_colors(self):
        return (24, 24, 26), (24, 24, 26), (196, 198, 202)

    name = "HiFi Tower 90"
    title = "RETROAMP  HI-FI COMPONENT SYSTEM"
    lcd = (5, 13, 14)
    lcd_text = (110, 255, 228)
    lcd_dim = (40, 120, 108)
    lcd_ghost = (12, 34, 32)
    label_col = (196, 198, 202)
    glyph = (24, 24, 26)
    accent = (255, 150, 30)
    accent2 = (255, 210, 120)
    pl_text, pl_cur, pl_bg, pl_sel, pl_font = (110, 235, 215), (255, 255, 255), (6, 9, 10), (24, 62, 58), "Bahnschrift"
    vis_top, vis_bot = (200, 255, 245), (40, 200, 175)
    display_glow = 0.9

    def body(self, w, h):
        return brushed(w, h, (52, 53, 57), var=9, seed=int(w * 7 + h))

    def frame(self, img, x0, y0, x1, y1):
        rect(img, x0, y0, x1, y0 + 1, (110, 112, 118)); rect(img, x0, y0, x0 + 1, y1, (90, 92, 98))
        rect(img, x0, y1 - 1, x1, y1, (8, 8, 9)); rect(img, x1 - 1, y0, x1, y1, (8, 8, 9))

    def inset(self, img, x0, y0, x1, y1, fill, r=3):
        G.rrect(img, x0 - 0.5, y0 - 0.5, x1 + 0.5, y1 + 0.5, r + 0.5, (110, 112, 118))
        G.rrect(img, x0, y0, x1, y1, r, fill, outline=(0, 0, 0))

    def subpanel(self, img, x0, y0, x1, y1):
        G.rrect(img, x0, y0, x1, y1, 2, [(0.0, (38, 39, 42)), (1.0, (28, 29, 31))], outline=(10, 10, 11))
        rect(img, x0 + 2, y1 - 1, x1 - 2, y1 - 0.6, (90, 92, 98))

    def button(self, img, x, y, w, h, pressed, glyph=None, label=None, sel=False, kind="transport"):
        # brushed aluminium key with a dark gap around it
        G.rrect(img, x + 0.3, y + 0.3, x + w - 0.3, y + h - 0.3, 2.2, (6, 6, 7))
        key = brushed(w - 2.2, h - 2.2, (196, 198, 204) if not pressed else (150, 152, 158), var=7, seed=int(w * 13 + h))
        m = G._rr_mask_px(key.width, key.height, 1.6 * K)
        img.paste(key, (S(x + 1.1), S(y + 1.1)), m)
        rect(img, x + 1.6, y + 1.2, x + w - 1.6, y + 1.6, (240, 240, 244) if not pressed else (120, 120, 124))
        if sel:  # small amber LED on toggle keys
            G.ellipse(img, x + w - 5, y + 2.4, x + w - 2.6, y + 4.8, (255, 170, 40), outline=(90, 40, 0), ow=0.3)
        elif kind == "toggle":
            G.ellipse(img, x + w - 5, y + 2.4, x + w - 2.6, y + 4.8, (70, 40, 20), outline=(20, 10, 0), ow=0.3)
        self.button_face(img, x, y, w, h, pressed, glyph, label, sel)

    def gadget(self, img, x, y, kind, pressed):
        G.ellipse(img, x + 0.5, y + 0.5, x + 8.5, y + 8.5, [(0.0, (210, 212, 216)), (1.0, (100, 102, 108))] if not pressed
                  else [(0.0, (90, 92, 98)), (1.0, (170, 172, 176))], outline=(5, 5, 6), ow=0.5)
        if kind == "menu":  # standby / power button
            G.ellipse(img, x + 2.5, y + 2.5, x + 6.5, y + 6.5, (220, 30, 20), outline=(60, 0, 0), ow=0.4)
        else:
            self.gadget_glyph(img, x, y, kind, (30, 30, 32))

    def title_bar(self, active, title, shade=False):
        img = self.body(275, 14)
        self.frame(img, 0, 0, 275, 14)
        col = self.label_col if active else (120, 122, 126)
        if shade:
            self.inset(img, 118, 2.5, 166, 11.5, self.lcd, r=1.5)
            text_at(img, 20, 10, "RETROAMP", col, 6.5, "bahnschrift.ttf", "ls", "Bold")
            return img
        text_at(img, 20, 10, title, col, 6.5, "bahnschrift.ttf", "ls", "SemiBold")
        # power LED
        G.ellipse(img, 230, 4.5, 235, 9.5, (255, 40, 30) if active else (90, 20, 15), outline=(30, 0, 0), ow=0.4)
        return img

    def groove_h(self, img, x0, y0, x1, y1):
        G.rrect(img, x0, y0, x1, y1, 1, (4, 4, 5), outline=(100, 102, 108), ow=0.4)

    def fill_h(self, img, x0, y0, x1, y1, frac):
        # amber LED bar
        n = int((x1 - x0) / 3)
        for i in range(n):
            on = (i + 0.5) / n <= frac
            rect(img, x0 + i * 3 + 0.4, y0, x0 + i * 3 + 2.4, y1, self.accent if on else (50, 30, 10))

    def thumb(self, img, w, h, pressed, vertical=False):
        G.rrect(img, 0, 0, w, h, 1.5, (5, 5, 6))
        cap = brushed(w - 1.4, h - 1.4, (205, 207, 212) if not pressed else (160, 162, 168), var=6, seed=w + h)
        img.paste(cap, (S(0.7), S(0.7)), G._rr_mask_px(cap.width, cap.height, 1.0 * K))
        if vertical:
            rect(img, 1.5, h / 2 - 0.4, w - 1.5, h / 2 + 0.4, (255, 150, 30))
        else:
            rect(img, w / 2 - 0.4, 1.5, w / 2 + 0.4, h - 1.5, (255, 150, 30))

    def eq_slot(self, cell, p):
        rect(cell, 6, 1, 8, 62, (4, 4, 5))
        center, pos = 31.5, 5.5 + (1 - p) * 51
        # LED ladder on both sides of the slot: lit between 0 dB and the knob
        for i in range(17):
            yy = 3 + i * 3.4
            lit = min(center, pos) - 1 <= yy <= max(center, pos) + 1
            col = (255, 150, 30) if lit else (45, 28, 10)
            if lit and yy < 14:
                col = (255, 60, 30)
            rect(cell, 2.5, yy, 4.6, yy + 2, col)
            rect(cell, 9.4, yy, 11.5, yy + 2, col)

    def digit(self, img, x, y, ch):
        G.seg7(img, x, y, ch, self.lcd_text, self.lcd_ghost)

    def logo(self, img, x0, y0, x1, y1):
        text_at(img, (x0 + x1) / 2, y0 + 11, "RS-90", (220, 222, 226), 7.5, "bahnschrift.ttf", "ms", "Bold")
        text_at(img, (x0 + x1) / 2, y0 + 19, "DIGITAL", (150, 152, 158), 4.2, "bahnschrift.ttf", "ms", "SemiBold")

    def main_extras(self, img):
        for cx, cy in ((3.6, 110.5), (271.4, 110.5)):
            screw(img, cx, cy, 1.8)

    def eq_decor(self, bg):
        self.subpanel(bg, 6, 15, 269, 113)
        self.inset(bg, 85, 16, 200, 37, self.lcd, r=2)
        for cx, cy in ((9.5, 18.5), (265.5, 18.5), (9.5, 110), (265.5, 110)):
            screw(bg, cx, cy, 1.6)

    def make_panel(self):
        self.cassette_deck("RETROAMP  RS-90  STEREO CASSETTE DECK", screws=True)

    def deck_text(self):
        return (200, 202, 206), (150, 152, 158)


# ============================================================================ 2. Radiola
class Radiola(Kit):
    key = "Radiola"
    dsp_font = "Georgia"

    def dsp_colors(self):
        return (70, 40, 18), (70, 40, 18), (240, 222, 180)

    name = "Radiola (valve radio)"
    title = "RetroAmp Radiola"
    lcd = (22, 12, 6)
    lcd_text = (255, 176, 80)
    lcd_dim = (130, 80, 40)
    lcd_ghost = (48, 26, 12)
    label_col = (240, 222, 180)
    glyph = (70, 40, 18)
    accent = (210, 150, 60)
    accent2 = (250, 210, 120)
    pl_text, pl_cur, pl_bg, pl_sel, pl_font = (70, 40, 18), (170, 40, 20), (240, 228, 198), (222, 196, 140), "Georgia"
    vis_top, vis_bot = (255, 230, 160), (220, 110, 30)
    text_font = ("bahnschrift.ttf", "SemiBold")
    label_font = ("georgiab.ttf", None)
    display_glow = 1.0
    BRASS = [(0.0, (240, 205, 120)), (0.5, (200, 155, 70)), (1.0, (130, 92, 36))]

    def body(self, w, h):
        return wood(w, h, seed=int(w * 3 + h * 5))

    def frame(self, img, x0, y0, x1, y1):
        G.rrect(img, x0, y0, x1, y1, 3, None or (0, 0, 0)) if False else None
        rect(img, x0, y0, x1, y0 + 1, (150, 100, 55)); rect(img, x0, y0, x0 + 1, y1, (130, 85, 45))
        rect(img, x0, y1 - 1, x1, y1, (30, 15, 6)); rect(img, x1 - 1, y0, x1, y1, (30, 15, 6))

    def inset(self, img, x0, y0, x1, y1, fill, r=3):
        G.rrect(img, x0 - 1, y0 - 1, x1 + 1, y1 + 1, r + 1, self.BRASS, outline=(70, 45, 15), ow=0.4)
        G.rrect(img, x0, y0, x1, y1, r, fill, outline=(10, 5, 2), ow=0.4)

    def subpanel(self, img, x0, y0, x1, y1):
        G.rrect(img, x0, y0, x1, y1, 3, self.BRASS, outline=(70, 45, 15), ow=0.5)
        G.rrect(img, x0 + 1.5, y0 + 1.5, x1 - 1.5, y1 - 1.5, 2, [(0.0, (60, 34, 16)), (1.0, (40, 22, 10))])

    def button(self, img, x, y, w, h, pressed, glyph=None, label=None, sel=False, kind="transport"):
        # ivory bakelite piano key
        G.rrect(img, x + 0.4, y + 0.4, x + w - 0.4, y + h - 0.4, 2.5, (40, 22, 10))
        top, bot = ((250, 244, 226), (214, 200, 166)) if not pressed else ((205, 190, 155), (235, 225, 200))
        if sel:
            top, bot = (255, 215, 140), (225, 160, 70)
        G.rrect(img, x + 1, y + 1, x + w - 1, y + h - 1.6 if not pressed else h + y - 1, 2, [(0.0, top), (1.0, bot)])
        if not pressed:
            rect(img, x + 2, y + h - 1.6, x + w - 2, y + h - 1, (150, 130, 95))
        self.button_face(img, x, y - (0.3 if not pressed else 0), w, h, pressed, glyph, label, sel)

    def gadget(self, img, x, y, kind, pressed):
        G.ellipse(img, x + 0.5, y + 0.5, x + 8.5, y + 8.5, self.BRASS if not pressed else self.BRASS[::-1] and
                  [(0.0, (130, 92, 36)), (1.0, (240, 205, 120))], outline=(60, 38, 12), ow=0.5)
        self.gadget_glyph(img, x, y, kind, (60, 35, 10))

    def title_bar(self, active, title, shade=False):
        img = self.body(275, 14)
        self.frame(img, 0, 0, 275, 14)
        G.rrect(img, 70, 1.5, 205, 12.5, 3, self.BRASS if active else [(0.0, (180, 160, 120)), (1.0, (120, 100, 70))],
                outline=(70, 45, 15), ow=0.5)
        for sx in (73.5, 201.5):
            G.ellipse(img, sx - 1, 6, sx + 1, 8, (110, 80, 30))
        if shade:
            self.inset(img, 212, 3, 240, 11, self.lcd, r=1.5)
        text_at(img, 137.5, 10.2, title, (50, 28, 8), 7.2, "georgiab.ttf", "ms")
        return img

    def groove_h(self, img, x0, y0, x1, y1):
        G.rrect(img, x0, y0, x1, y1, (y1 - y0) / 2, (25, 13, 5), outline=(150, 105, 50), ow=0.4)

    def fill_h(self, img, x0, y0, x1, y1, frac):
        if frac > 0.01:
            G.rrect(img, x0, y0, x0 + (x1 - x0) * frac, y1, (y1 - y0) / 2, [(0.0, (255, 200, 110)), (1.0, (220, 130, 40))])

    def thumb(self, img, w, h, pressed, vertical=False):
        d = min(w, h)
        x0, y0 = (w - d) / 2, (h - d) / 2
        G.ellipse(img, x0, y0, x0 + d, y0 + d, self.BRASS if not pressed else [(0.0, (130, 92, 36)), (1.0, (240, 205, 120))],
                  outline=(60, 38, 12), ow=0.5)
        G.ellipse(img, x0 + d * 0.3, y0 + d * 0.3, x0 + d * 0.7, y0 + d * 0.7, (120, 85, 30))
        if w > h * 1.6:  # long position knob: brass bar
            G.rrect(img, 0, 1, w, h - 1, (h - 2) / 2, self.BRASS, outline=(60, 38, 12), ow=0.5)
            rect(img, w / 2 - 0.4, 2.5, w / 2 + 0.4, h - 2.5, (90, 60, 20))

    def eq_slot(self, cell, p):
        G.rrect(cell, 5.2, 1, 8.8, 62, 1.8, (25, 13, 5), outline=(150, 105, 50), ow=0.4)
        center, pos = 31.5, 5.5 + (1 - p) * 51
        y0, y1 = sorted((center, pos))
        if y1 - y0 > 0.3:
            rect(cell, 6, y0, 8, y1, (255, 170, 70))
        for yy in (5.5, 18.5, 31.5, 44.5, 57.5):
            rect(cell, 2.5, yy - 0.3, 4.5, yy + 0.3, self.label_col)
            rect(cell, 9.5, yy - 0.3, 11.5, yy + 0.3, self.label_col)

    def digit(self, img, x, y, ch):
        # Nixie tube: dark glass, honeycomb mesh, glowing orange numeral
        G.rrect(img, x + 0.3, y + 0.1, x + 8.7, y + 12.9, 2.5, (26, 12, 6), outline=(70, 40, 20), ow=0.3)
        for yy in range(1, 13, 2):
            for xx in range(1, 9, 2):
                rect(img, x + xx + (yy // 2 % 2) * 0.5, y + yy, x + xx + 0.25 + (yy // 2 % 2) * 0.5, y + yy + 0.25, (44, 24, 12))
        if ch.strip():
            text_at(img, x + 4.5, y + 11.3, ch if ch != "-" else "–", (255, 150, 60), 13.5, "bahnschrift.ttf", "ms",
                    "Light Condensed")

    def logo(self, img, x0, y0, x1, y1):
        G.ellipse(img, x0 + 4, y0 + 1, x0 + 30, y0 + 25, self.BRASS, outline=(60, 38, 12), ow=0.5)
        G.ellipse(img, x0 + 7, y0 + 4, x0 + 27, y0 + 22, [(0.0, (60, 34, 16)), (1.0, (30, 16, 6))])
        text_at(img, x0 + 17, y0 + 16, "R", (240, 205, 120), 11, "georgiab.ttf", "ms")

    def label(self, img, x, y, s, col=None, size=5.5, anchor="ms"):
        text_at(img, x, y, s, col or self.label_col, size, "georgiab.ttf", anchor)

    def make_panel(self):
        W, H = 275, 116
        p = self.body(W, H)
        self.frame(p, 0, 0, W, H)
        # tuning dial
        G.rrect(p, 8, 8, 206, 54, 4, self.BRASS, outline=(60, 38, 12), ow=0.6)
        G.rrect(p, 10, 10, 204, 52, 3, [(0.0, (255, 240, 196)), (1.0, (236, 210, 150))])
        text_at(p, 14, 17, "UKF", (120, 70, 30), 4.5, "georgiab.ttf", "ls")
        stations = ["Warszawa", "Londyn", "Paryż", "Praga", "Berlin", "Wiedeń", "Rzym", "Kraków", "Lwów", "Wilno"]
        for i in range(41):
            x = 18 + i * 4.5
            rect(p, x, 38 if i % 5 else 34, x + 0.35, 42, (90, 50, 20))
        for i, st in enumerate(stations):
            x = 20 + i * 18.5
            text_at(p, x, 26 + (i % 2) * 5, st, (100, 50, 20), 3.6, "georgia.ttf", "ms")
        for i, f in enumerate(("88", "92", "96", "100", "104", "108")):
            text_at(p, 18 + i * 36, 49, f, (70, 40, 18), 4.2, "georgiab.ttf", "ms")
        # speaker cloth with wooden slats
        G.rrect(p, 8, 58, 206, 108, 3, (40, 24, 12))
        cloth = np.zeros((S(48), S(196), 3), np.float32)
        yy, xx = np.mgrid[0:S(48), 0:S(196)]
        weave = ((xx // 2 + yy // 2) % 2) * 18 + ((xx // 2) % 2) * 10
        cloth[:] = np.array((150, 120, 80), np.float32)
        cloth += weave[:, :, None] - 14
        p.paste(np_to_img(cloth), (S(9), S(59)))
        for sx in range(30, 200, 34):
            G.rrect(p, sx, 59, sx + 6, 107, 2, wood(6, 48, seed=sx).resize((S(6), S(48))))
        # magic eye + knobs
        G.ellipse(p, 214, 10, 262, 58, self.BRASS, outline=(60, 38, 12), ow=0.6)
        G.ellipse(p, 218, 14, 258, 54, (10, 14, 10))
        text_at(p, 238, 65, "MAGIC EYE", self.label_col, 4.6, "georgiab.ttf", "ms")
        for cx in (222, 252):
            G.ellipse(p, cx - 9.5, 69, cx + 9.5, 88, [(0.0, (70, 40, 20)), (1.0, (30, 15, 6))], outline=(10, 5, 2), ow=0.5)
            G.ellipse(p, cx - 6, 72.5, cx + 6, 84.5, [(0.0, (110, 70, 35)), (1.0, (50, 26, 10))])
            rect(p, cx - 0.5, 72, cx + 0.5, 76.5, (240, 205, 120))
        text_at(p, 222, 94, "VOLUME", self.label_col, 4, "georgiab.ttf", "ms")
        text_at(p, 252, 94, "TONE", self.label_col, 4, "georgiab.ttf", "ms")
        self.button(p, 210, 98, 56, 13, False, label="PLAYLIST")
        self.buttons.append({"X": 210, "Y": 98, "W": 56, "H": 13, "Action": "playlist"})
        self.panel = p
        # magic eye frames: green fan, shadow wedge closes with the signal
        nfr = 24
        sheet = Image.new("RGB", (S(40) * nfr, S(40)))
        for f in range(nfr):
            cell = crop(p, 218, 14, 40, 40)
            lvl = f / (nfr - 1)
            wedge = 80 - lvl * 74  # degrees of the dark wedge
            layer = Image.new("RGB", cell.size, BLACK)
            pts = [(20, 20)]
            for a in np.linspace(-90 + wedge / 2, 270 - wedge / 2, 90):
                r = math.radians(a)
                pts.append((20 + math.cos(r) * 18, 20 + math.sin(r) * 18))
            poly(layer, pts, (80, 255, 110))
            G.ellipse(layer, 13, 13, 27, 27, BLACK)
            G.ellipse(layer, 16, 16, 24, 24, (20, 90, 30))
            cell = glow(cell, layer, 0.9, 0.9)
            sheet.paste(cell, (S(40) * f, 0))
        self.extra["magiceye"] = sheet
        self.anims.append({"Window": "panel", "Mode": "level", "Channel": "bass", "Image": "magiceye.png", "FrameW": 40,
                           "FrameH": 40, "Frames": nfr, "X": 218, "Y": 14, "Gain": 1.25, "Attack": 0.5, "Release": 0.08})
        # dial pointer follows the song position
        ptr = crop(p, 18, 11, 6, 40)
        rect(ptr, 2.6, 0, 3.4, 40, (200, 20, 20))
        poly(ptr, [(1, 0), (5, 0), (3, 3)], (200, 20, 20))
        self.extra["pointer"] = ptr
        self.anims.append({"Window": "panel", "Mode": "progress", "Image": "pointer.png", "FrameW": 6, "FrameH": 40,
                           "Frames": 1, "X": 15, "Y": 11, "W": 186})
        # pilot lamp flicker
        lamp = Image.new("RGB", (S(8) * 8, S(8)))
        for f in range(8):
            cell = crop(p, 180, 13, 8, 8)
            v = 0.82 + 0.18 * math.sin(f / 8 * 2 * math.pi) + (0.06 if f in (2, 5) else 0)
            layer = Image.new("RGB", cell.size, BLACK)
            G.ellipse(layer, 2, 2, 6, 6, (int(255 * min(1, v)), int(170 * v), int(60 * v)))
            cell = glow(cell, layer, 0.7, 1.0)
            lamp.paste(cell, (S(8) * f, 0))
        self.extra["lamp"] = lamp
        self.anims.append({"Window": "panel", "Mode": "loop", "Image": "lamp.png", "FrameW": 8, "FrameH": 8, "Frames": 8,
                           "X": 180, "Y": 13, "FPS": 6})
        self.anims.append({"Window": "panel", "Mode": "text", "Text": "title", "X": 30, "Y": 11, "W": 146, "H": 9,
                           "Font": "Georgia", "Size": 6, "Bold": 1, "Color": "#6E2A10"})


# ============================================================================ 3. Synthwave
class Synthwave(Kit):
    key = "Synthwave"

    def dsp_colors(self):
        return (255, 110, 220), (255, 255, 255), (80, 240, 255)

    name = "Synthwave '84"
    title = "RETROAMP"
    lcd = (6, 0, 14)
    lcd_text = (80, 240, 255)
    lcd_dim = (40, 100, 140)
    lcd_ghost = (22, 8, 38)
    label_col = (255, 110, 220)
    glyph = (80, 240, 255)
    accent = (255, 40, 180)
    accent2 = (80, 240, 255)
    pl_text, pl_cur, pl_bg, pl_sel, pl_font = (80, 230, 255), (255, 90, 210), (10, 2, 22), (60, 12, 92), "Bahnschrift"
    vis_top, vis_bot = (255, 90, 220), (60, 200, 255)
    display_glow = 1.1
    PINK, CYAN = (255, 40, 180), (80, 240, 255)

    def body(self, w, h):
        img = G.vgrad(w, h, [(0.0, (34, 8, 56)), (1.0, (12, 2, 24))])
        for y in range(0, int(h), 2):
            rect(img, 0, y + 1, w, y + 1.25, (24, 6, 40))
        return img

    def frame(self, img, x0, y0, x1, y1):
        layer = Image.new("RGB", img.size, BLACK)
        G.rrect(layer, x0 + 0.5, y0 + 0.5, x1 - 0.5, y1 - 0.5, 3, BLACK, outline=self.PINK, ow=0.6)
        img.paste(glow(img, layer, 1.0, 0.9))

    def neon_outline(self, img, x0, y0, x1, y1, col, r=2.5, ow=0.6, strength=0.9):
        layer = Image.new("RGB", img.size, BLACK)
        G.rrect(layer, x0, y0, x1, y1, r, BLACK, outline=col, ow=ow)
        img.paste(glow(img, layer, 0.9, strength))

    def inset(self, img, x0, y0, x1, y1, fill, r=3):
        G.rrect(img, x0, y0, x1, y1, r, fill)
        self.neon_outline(img, x0, y0, x1, y1, self.CYAN, r, 0.45, 0.7)

    def subpanel(self, img, x0, y0, x1, y1):
        G.rrect(img, x0, y0, x1, y1, 4, [(0.0, (28, 6, 48)), (1.0, (14, 2, 28))])
        self.neon_outline(img, x0, y0, x1, y1, self.PINK, 4, 0.4, 0.6)

    def button(self, img, x, y, w, h, pressed, glyph=None, label=None, sel=False, kind="transport"):
        col = self.PINK if kind == "toggle" else self.CYAN
        fill = [(0.0, (40, 10, 66)), (1.0, (16, 4, 32))]
        if pressed or sel:
            fill = [(0.0, mix(col, BLACK, 0.35)), (1.0, mix(col, BLACK, 0.65))]
        G.rrect(img, x + 1, y + 1, x + w - 1, y + h - 1, 3, fill)
        self.neon_outline(img, x + 1, y + 1, x + w - 1, y + h - 1, col, 3, 0.5, 0.9)
        layer = Image.new("RGB", img.size, BLACK)
        self.button_face(layer, x, y, w, h, pressed, glyph, label, sel, col=(255, 255, 255) if (pressed or sel) else col)
        img.paste(glow(img, layer, 0.6, 0.8))

    def gadget(self, img, x, y, kind, pressed):
        G.rrect(img, x, y, x + 9, y + 9, 2, (30, 6, 50) if not pressed else (120, 20, 90))
        self.neon_outline(img, x + 0.3, y + 0.3, x + 8.7, y + 8.7, self.PINK, 2, 0.4, 0.7)
        self.gadget_glyph(img, x, y, kind, self.CYAN)

    def title_bar(self, active, title, shade=False):
        img = self.body(275, 14)
        layer = Image.new("RGB", img.size, BLACK)
        rect(layer, 2, 12.6, 273, 13.2, self.PINK if active else (90, 20, 70))
        img = glow(img, layer, 0.8, 0.9)
        if shade:
            self.inset(img, 118, 2.5, 166, 11.5, self.lcd, r=2)
        # chrome italic logo text
        f = ttf("ariblk.ttf", 10 * K)
        txt = Image.new("L", (S(140), S(14)), 0)
        ImageDraw.Draw(txt).text((S(70), S(7.3)), title, font=f, fill=255, anchor="mm")
        txt = txt.transform(txt.size, Image.Transform.AFFINE, (1, 0.28, -S(2), 0, 1, 0), Image.Resampling.BICUBIC)
        chrome = G.vgrad(140, 14, [(0.0, (255, 255, 255)), (0.45, (150, 200, 255)), (0.52, (60, 20, 90)),
                                   (0.75, (255, 140, 220)), (1.0, (255, 255, 255))])
        if not active:
            chrome = chrome.point(lambda v: v // 2)
        img.paste(chrome, (S(67.5), 0), txt)
        return img

    def groove_h(self, img, x0, y0, x1, y1):
        G.rrect(img, x0, y0, x1, y1, (y1 - y0) / 2, (8, 0, 18), outline=(90, 30, 120), ow=0.35)

    def fill_h(self, img, x0, y0, x1, y1, frac):
        if frac > 0.01:
            layer = Image.new("RGB", img.size, BLACK)
            G.rrect(layer, x0, y0, x0 + (x1 - x0) * frac, y1, (y1 - y0) / 2, self.CYAN)
            img.paste(glow(img, layer, 0.7, 0.8))

    def thumb(self, img, w, h, pressed, vertical=False):
        layer = Image.new("RGB", img.size, BLACK)
        G.rrect(layer, 1, 1, w - 1, h - 1, min(w, h) / 2.4, self.PINK if not pressed else (255, 255, 255))
        G.rrect(layer, 2.2, 2.2, w - 2.2, h - 2.2, min(w, h) / 3, (255, 160, 230) if not pressed else self.PINK)
        img.paste(glow(img, layer, 0.8, 0.9))

    def eq_slot(self, cell, p):
        G.rrect(cell, 5.5, 1, 8.5, 62, 1.5, (8, 0, 18), outline=(90, 30, 120), ow=0.3)
        center, pos = 31.5, 5.5 + (1 - p) * 51
        y0, y1 = sorted((center, pos))
        if y1 - y0 > 0.3:
            layer = Image.new("RGB", cell.size, BLACK)
            col = self.PINK if pos < center else self.CYAN
            rect(layer, 6.3, y0, 7.7, y1, col)
            cell.paste(glow(cell, layer, 0.8, 0.9))

    def digit(self, img, x, y, ch):
        layer = Image.new("RGB", img.size, BLACK)
        G.seg7(layer, x, y, ch, self.PINK, None)
        G.seg7(img, x, y, " ", self.PINK, self.lcd_ghost)
        img.paste(ImageChops.lighter(img, layer))

    def make_numbers(self, ex=False):
        img = super().make_numbers(ex)
        return img

    def logo(self, img, x0, y0, x1, y1):
        cx, cy, r = (x0 + x1) / 2, y0 + 15, 11
        sun = G.vgrad(2 * r, 2 * r, [(0.0, (255, 230, 80)), (0.6, (255, 90, 120)), (1.0, (200, 30, 160))])
        m = Image.new("L", (S(2 * r), S(2 * r)), 0)
        ImageDraw.Draw(m).ellipse([0, 0, S(2 * r) - 1, S(2 * r) - 1], fill=255)
        md = ImageDraw.Draw(m)
        for i, yy in enumerate((11, 14, 16.5, 18.5, 20)):
            md.rectangle([0, S(yy), S(2 * r), S(yy + 0.8 + i * 0.25)], fill=0)
        img.paste(sun, (S(cx - r), S(cy - r)), m)

    def make_panel(self):
        W, H = 275, 116
        hz = 62
        p = Image.new("RGB", (S(W), S(H)))
        sky = G.vgrad(W, hz, [(0.0, (10, 0, 30)), (0.55, (70, 10, 100)), (0.85, (220, 40, 140)), (1.0, (255, 140, 90))])
        p.paste(sky, (0, 0))
        rng = random.Random(7)
        for _ in range(60):
            x, y = rng.uniform(2, W - 2), rng.uniform(2, hz * 0.55)
            rect(p, x, y, x + 0.4, y + 0.4, (255, 255, 255) if rng.random() > 0.5 else (150, 200, 255))
        # striped sun
        cx, r = W / 2, 30
        sun = G.vgrad(2 * r, 2 * r, [(0.0, (255, 240, 90)), (0.55, (255, 110, 110)), (1.0, (230, 30, 150))])
        m = Image.new("L", (S(2 * r), S(2 * r)), 0)
        ImageDraw.Draw(m).ellipse([0, 0, S(2 * r) - 1, S(2 * r) - 1], fill=255)
        md = ImageDraw.Draw(m)
        for i in range(7):
            yy = 30 + i * 4.5
            md.rectangle([0, S(yy), S(2 * r), S(yy + 0.8 + i * 0.45)], fill=0)
        p.paste(sun, (S(cx - r), S(hz - r * 1.55)), m)
        # mountains
        for pts, col in (([(0, hz), (30, 44), (55, 52), (85, 38), (120, hz)], (40, 6, 70)),
                         ([(150, hz), (185, 40), (210, 50), (240, 36), (275, 48), (275, hz)], (40, 6, 70))):
            poly(p, pts, col)
            for i in range(len(pts) - 1):
                thick_line(p, *pts[i], *pts[i + 1], 0.5, (80, 240, 255))
        # palms
        for px, flip in ((22, 1), (252, -1)):
            for t in range(18):
                yy = hz - 2 - t * 1.6
                rect(p, px + flip * t * 0.25, yy, px + 1.4 + flip * t * 0.25, yy + 1.7, (8, 0, 16))
            top = (px + flip * 4.5, hz - 30)
            for a in (-150, -120, -70, -40, -10, 20):
                ang = math.radians(a)
                thick_line(p, top[0], top[1], top[0] + math.cos(ang) * 11, top[1] + math.sin(ang) * 5 + 4, 1.1, (8, 0, 16))
        # floor (static part) + neon horizon
        floor = G.vgrad(W, H - hz, [(0.0, (30, 0, 50)), (1.0, (6, 0, 14))])
        p.paste(floor, (0, S(hz)))
        layer = Image.new("RGB", p.size, BLACK)
        rect(layer, 0, hz, W, hz + 0.6, (255, 60, 200))
        p = glow(p, layer, 1.2, 1.0)
        self.frame(p, 0, 0, W, H)
        self.panel = p
        # moving grid frames (floor region only)
        nfr = 16
        fh = H - hz - 1
        sheet = Image.new("RGB", (S(W) * 2, S(fh) * 8))
        for f in range(nfr):
            cell = crop(p, 0, hz + 1, W, fh)
            layer = Image.new("RGB", cell.size, BLACK)
            for i in range(-14, 15):
                x_far = W / 2 + i * 9
                x_near = W / 2 + i * 60
                thick_line(layer, x_far, 0, x_near, fh * 1.4, 0.45, (255, 50, 200))
            for k in range(10):
                t = ((k + f / nfr) / 10.0)
                yy = fh * (t ** 2.2)
                rect(layer, 0, yy, W, yy + 0.3 + t * 0.5, (255, 50, 200))
            cell = glow(cell, layer, 0.8, 0.9)
            sheet.paste(cell, (S(W) * (f % 2), S(fh) * (f // 2)))
        self.extra["grid"] = sheet
        self.button(p, 222, 4, 48, 13, False, label="PLAYLIST", kind="toggle")
        self.buttons.append({"X": 222, "Y": 4, "W": 48, "H": 13, "Action": "playlist"})
        self.panel = p
        self.anims.append({"Window": "panel", "Mode": "loop", "Image": "grid.png", "FrameW": W, "FrameH": fh,
                           "Frames": nfr, "Columns": 2, "X": 0, "Y": hz + 1, "FPS": 18})
        self.anims.append({"Window": "panel", "Mode": "spectrum", "X": 70, "Y": 4, "W": 135, "H": 18, "Bars": 27,
                           "Color": "#50F0FF", "Color2": "#FF28B4"})
        self.anims.append({"Window": "panel", "Mode": "text", "Text": "title", "X": 8, "Y": 101, "W": 190, "H": 12,
                           "Font": "Bahnschrift", "Size": 8, "Bold": 1, "Color": "#50F0FF"})
        self.anims.append({"Window": "panel", "Mode": "text", "Text": "time", "X": 200, "Y": 101, "W": 66, "H": 12,
                           "Font": "Bahnschrift", "Size": 8, "Bold": 1, "Color": "#FF5AD2", "Align": "right"})


# ============================================================================ 4. Aqua
class Aqua(Kit):
    key = "Aqua"
    dsp_font = "Segoe UI"

    def dsp_colors(self):
        return (40, 40, 50), (255, 255, 255), (60, 60, 60)

    name = "Aqua (Mac OS X 2001)"
    title = "RetroAmp"
    lcd = (226, 230, 212)
    lcd_text = (40, 44, 40)
    lcd_dim = (140, 146, 132)
    lcd_ghost = (210, 215, 196)
    label_col = (60, 60, 60)
    glyph = (40, 40, 40)
    accent = (40, 110, 220)
    accent2 = (130, 190, 255)
    pl_text, pl_cur, pl_bg, pl_sel, pl_font = (20, 20, 20), (20, 80, 200), (255, 255, 255), (190, 214, 250), "Segoe UI"
    vis_top, vis_bot = (40, 110, 220), (130, 190, 255)
    text_font = ("segoeuib.ttf", None)
    label_font = ("segoeuib.ttf", None)

    def body(self, w, h):
        return pinstripe(w, h)

    def frame(self, img, x0, y0, x1, y1):
        rect(img, x0, y0, x1, y0 + 0.5, (255, 255, 255))
        rect(img, x0, y1 - 0.5, x1, y1, (150, 150, 150)); rect(img, x0, y0, x0 + 0.5, y1, (170, 170, 170))
        rect(img, x1 - 0.5, y0, x1, y1, (150, 150, 150))

    def inset(self, img, x0, y0, x1, y1, fill, r=4):
        G.rrect(img, x0, y0, x1, y1, r + 1, [(0.0, (140, 140, 140)), (1.0, (220, 220, 220))])
        G.rrect(img, x0 + 0.6, y0 + 0.6, x1 - 0.6, y1 - 0.6, r, [(0.0, mix(fill, BLACK, 0.06)), (1.0, fill)])

    def subpanel(self, img, x0, y0, x1, y1):
        G.rrect(img, x0, y0, x1, y1, 5, [(0.0, (200, 200, 200)), (1.0, (245, 245, 245))])
        G.rrect(img, x0 + 0.5, y0 + 0.5, x1 - 0.5, y1 - 0.5, 4.5, pinstripe(x1 - x0 - 1, y1 - y0 - 1))

    def gel(self, img, x0, y0, x1, y1, col, pressed=False, round_=True):
        w, h = x1 - x0, y1 - y0
        r = min(w, h) / 2 if round_ else 3.5
        dark = mix(col, BLACK, 0.45 if not pressed else 0.6)
        G.rrect(img, x0, y0, x1, y1, r, [(0.0, dark), (0.5, mix(col, BLACK, 0.1)), (1.0, mix(col, WHITE, 0.45))],
                outline=mix(col, BLACK, 0.55), ow=0.4)
        hl = G.vgrad(w * 0.8, h * 0.45, [(0.0, (255, 255, 255)), (1.0, mix(col, WHITE, 0.55))])
        m = G._rr_mask_px(hl.width, hl.height, r * 0.8 * K).point(lambda v: int(v * (0.85 if not pressed else 0.5)))
        img.paste(hl, (S(x0 + w * 0.1), S(y0 + h * 0.06)), m)

    def button(self, img, x, y, w, h, pressed, glyph=None, label=None, sel=False, kind="transport"):
        col = (215, 222, 232) if not sel else (70, 145, 240)
        if glyph is g_play:
            col = (90, 160, 245)
        if kind == "transport":
            d = min(w, h) - 1
            x0 = x + (w - d) / 2
            self.gel(img, x0, y + 0.5, x0 + d, y + 0.5 + d, col, pressed)
        else:
            self.gel(img, x + 0.5, y + 0.5, x + w - 0.5, y + h - 0.5, col, pressed, round_=True)
        self.button_face(img, x, y, w, h, pressed, glyph, label, sel,
                         col=(255, 255, 255) if col[2] > 240 and col[0] < 150 else (40, 40, 50))

    def gadget(self, img, x, y, kind, pressed):
        col = {"close": (240, 70, 60), "min": (250, 190, 40), "shade": (90, 200, 70), "unshade": (90, 200, 70),
               "menu": (150, 160, 175), "deck": (110, 170, 245)}[kind]
        self.gel(img, x + 0.3, y + 0.3, x + 8.7, y + 8.7, col, pressed)
        if kind == "deck":
            self.gadget_glyph(img, x, y, "deck", (30, 40, 70))

    def title_bar(self, active, title, shade=False):
        img = pinstripe(275, 14, (240, 240, 240), (230, 230, 230)) if active else pinstripe(275, 14, (246, 246, 246), (240, 240, 240))
        rect(img, 0, 13.5, 275, 14, (150, 150, 150))
        if shade:
            self.inset(img, 118, 2, 166, 12, self.lcd, r=2)
            return img
        text_at(img, 137.5, 10.3, title, (40, 40, 40) if active else (150, 150, 150), 8, "segoeui.ttf", "ms")
        return img

    def groove_h(self, img, x0, y0, x1, y1):
        G.rrect(img, x0, y0, x1, y1, (y1 - y0) / 2, [(0.0, (170, 170, 170)), (1.0, (245, 245, 245))], outline=(130, 130, 130), ow=0.35)

    def fill_h(self, img, x0, y0, x1, y1, frac):
        if frac > 0.01:
            self.gel(img, x0, y0, x0 + (x1 - x0) * frac, y1, (70, 140, 240))

    def thumb(self, img, w, h, pressed, vertical=False):
        d = min(w, h)
        x0, y0 = (w - d) / 2, (h - d) / 2
        if w > h * 2:
            self.gel(img, 0.3, 0.6, w - 0.3, h - 0.6, (215, 222, 232), pressed)
        else:
            self.gel(img, x0 + 0.3, y0 + 0.3, x0 + d - 0.3, y0 + d - 0.3, (90, 160, 245), pressed)

    def eq_slot(self, cell, p):
        G.rrect(cell, 5.2, 1, 8.8, 62, 1.8, [(0.0, (170, 170, 170)), (1.0, (245, 245, 245))], outline=(130, 130, 130), ow=0.3)
        center, pos = 31.5, 5.5 + (1 - p) * 51
        y0, y1 = sorted((center, pos))
        if y1 - y0 > 0.5:
            G.rrect(cell, 5.8, y0, 8.2, y1, 1.2, [(0.0, (70, 140, 240)), (1.0, (150, 200, 255))])

    def digit(self, img, x, y, ch):
        if ch.strip():
            text_at(img, x + 4.5, y + 11.6, ch if ch != "-" else "–", self.lcd_text, 14.5, "segoeuisl.ttf", "ms")

    def logo(self, img, x0, y0, x1, y1):
        self.gel(img, x0 + 6, y0 + 2, x0 + 28, y0 + 24, (90, 160, 245))
        text_at(img, x0 + 17, y0 + 17.5, "♪", WHITE, 12, "segoeuib.ttf", "ms")

    def make_panel(self):
        W, H = 275, 116
        p = pinstripe(W, H)
        self.frame(p, 0, 0, W, H)
        text_at(p, 137.5, 10.5, "Now Playing", (60, 60, 60), 7.5, "segoeui.ttf", "ms")
        rect(p, 0, 14, W, 14.4, (170, 170, 170))
        # brushed-metal well for the disc
        G.rrect(p, 8, 19, 96, 109, 6, [(0.0, (150, 150, 155)), (1.0, (225, 225, 230))])
        G.rrect(p, 9, 20, 95, 108, 5.5, [(0.0, (200, 202, 208)), (1.0, (238, 239, 242))])
        # LCD with title / info
        self.inset(p, 104, 21, 267, 61, self.lcd, r=5)
        G.rrect(p, 104, 67, 267, 88, 5, [(0.0, (232, 234, 238)), (1.0, (250, 250, 252))], outline=(170, 170, 175), ow=0.4)
        self.gel(p, 104, 93, 186, 108, (110, 170, 245))
        text_at(p, 145, 103.3, "Playlist", (255, 255, 255), 7, "segoeuib.ttf", "ms")
        self.gel(p, 192, 93, 267, 108, (215, 222, 232))
        text_at(p, 229.5, 103.3, "Open…", (40, 40, 50), 7, "segoeuib.ttf", "ms")
        self.buttons.append({"X": 104, "Y": 93, "W": 82, "H": 15, "Action": "playlist"})
        self.buttons.append({"X": 192, "Y": 93, "W": 75, "H": 15, "Action": "eject"})
        self.panel = p
        # spinning CD
        nfr, cx, cy, r = 24, 52, 64, 38
        sheet = Image.new("RGB", (S(2 * r + 4) * 6, S(2 * r + 4) * 4))
        bgc = crop(p, cx - r - 2, cy - r - 2, 2 * r + 4, 2 * r + 4)
        for f in range(nfr):
            cell = bgc.copy()
            self._cd(cell, r + 2, r + 2, r, f * 360.0 / nfr)
            sheet.paste(cell, (S(2 * r + 4) * (f % 6), S(2 * r + 4) * (f // 6)))
        self.extra["cd"] = sheet
        self.anims.append({"Window": "panel", "Mode": "spin", "Image": "cd.png", "FrameW": 2 * r + 4, "FrameH": 2 * r + 4,
                           "Frames": nfr, "Columns": 6, "X": cx - r - 2, "Y": cy - r - 2, "FPS": 16})
        self.anims.append({"Window": "panel", "Mode": "text", "Text": "title", "X": 108, "Y": 24, "W": 155, "H": 14,
                           "Font": "Segoe UI", "Size": 9, "Bold": 1, "Color": "#282C28"})
        self.anims.append({"Window": "panel", "Mode": "text", "Text": "time", "X": 108, "Y": 38, "W": 70, "H": 11,
                           "Font": "Segoe UI", "Size": 7.5, "Color": "#464A46"})
        self.anims.append({"Window": "panel", "Mode": "text", "Text": "remain", "X": 190, "Y": 38, "W": 73, "H": 11,
                           "Font": "Segoe UI", "Size": 7.5, "Color": "#464A46", "Align": "right"})
        self.anims.append({"Window": "panel", "Mode": "text", "Text": "bitrate", "X": 150, "Y": 49, "W": 55, "H": 9,
                           "Font": "Segoe UI", "Size": 6, "Color": "#464A46"})
        self.anims.append({"Window": "panel", "Mode": "text", "Text": "samplerate", "X": 205, "Y": 49, "W": 58, "H": 9,
                           "Font": "Segoe UI", "Size": 6, "Color": "#464A46", "Align": "right"})
        self.anims.append({"Window": "panel", "Mode": "spectrum", "X": 108, "Y": 69, "W": 155, "H": 17, "Bars": 31,
                           "Color": "#3C8CF0", "Color2": "#A0CCFF"})

    def _cd(self, img, cx, cy, r, rot):
        # iridescent disc: rainbow sheen + printed label that makes the rotation visible
        Wp = S(2 * r)
        yy, xx = np.mgrid[0:Wp, 0:Wp].astype(np.float32)
        dx, dy = (xx - Wp / 2) / (Wp / 2), (yy - Wp / 2) / (Wp / 2)
        rr = np.sqrt(dx * dx + dy * dy)
        ang = np.arctan2(dy, dx)
        sheen = 0.5 + 0.5 * np.cos(ang * 2.0 - math.radians(40))
        base = np.stack([200 + 40 * sheen, 205 + 35 * np.cos(ang * 2 + 1.2), 215 + 35 * np.cos(ang * 2 + 2.4)], axis=2)
        disc = np_to_img(base)
        m = Image.fromarray(((rr <= 1.0) * 255).astype(np.uint8))
        img.paste(disc, (S(cx - r), S(cy - r)), m)
        # printed label (rotates)
        lab = Image.new("RGB", (S(2 * r), S(2 * r)), (0, 0, 0))
        lm = Image.new("L", lab.size, 0)
        d, dm = ImageDraw.Draw(lab), ImageDraw.Draw(lm)
        c0 = S(r)
        d.pieslice([S(r * 0.42), S(r * 0.42), S(r * 1.58), S(r * 1.58)], 0, 360, fill=(60, 120, 230))
        dm.pieslice([S(r * 0.42), S(r * 0.42), S(r * 1.58), S(r * 1.58)], 0, 360, fill=255)
        d.pieslice([S(r * 0.42), S(r * 0.42), S(r * 1.58), S(r * 1.58)], 200, 300, fill=(255, 255, 255))
        f = ttf("segoeuib.ttf", r * 0.22 * K)
        d.text((c0, c0 - S(r * 0.38)), "RetroAmp", font=f, fill=(255, 255, 255), anchor="mm")
        lab = lab.rotate(-rot, resample=Image.Resampling.BICUBIC)
        lm = lm.rotate(-rot, resample=Image.Resampling.BICUBIC)
        img.paste(lab, (S(cx - r), S(cy - r)), lm)
        G.ellipse(img, cx - r * 0.2, cy - r * 0.2, cx + r * 0.2, cy + r * 0.2, (235, 236, 240), outline=(150, 150, 155), ow=0.4)
        G.ellipse(img, cx - r * 0.08, cy - r * 0.08, cx + r * 0.08, cy + r * 0.08, (190, 192, 198))
        ring = Image.new("L", (S(2 * r), S(2 * r)), 0)
        ImageDraw.Draw(ring).ellipse([0, 0, S(2 * r) - 1, S(2 * r) - 1], outline=255, width=max(1, K // 2))
        img.paste((150, 150, 155), (S(cx - r), S(cy - r)), ring)

    def main_extras(self, main):
        # pulsing default (Play) button while playing - the famous Aqua "throb"
        nfr = 16
        sheet = Image.new("RGB", (S(23) * nfr, S(18)))
        for f in range(nfr):
            cell = crop(main, 39, 88, 23, 18)
            t = 0.5 - 0.5 * math.cos(f / nfr * 2 * math.pi)
            col = mix((70, 140, 240), (160, 210, 255), t)
            d = 17
            x0 = (23 - d) / 2
            self.gel(cell, x0, 0.5, x0 + d, 0.5 + d, col)
            g_play(cell, 11.5, 9, WHITE)
            sheet.paste(cell, (S(23) * f, 0))
        self.extra["throb"] = sheet
        self.anims.append({"Window": "main", "Mode": "loop", "When": "playing", "Image": "throb.png", "FrameW": 23,
                           "FrameH": 18, "Frames": nfr, "X": 39, "Y": 88, "FPS": 14})


# ============================================================================ 5. Windows 98
class Win98(Kit):
    key = "Windows98"
    dsp_font = "Tahoma"

    def dsp_colors(self):
        return (0, 0, 0), (0, 0, 0), (0, 0, 0)

    name = "Windows 98"
    title = "RetroAmp"
    lcd = (0, 0, 0)
    lcd_text = (0, 230, 0)
    lcd_dim = (0, 120, 0)
    lcd_ghost = (0, 40, 0)
    label_col = (0, 0, 0)
    glyph = (0, 0, 0)
    accent = (0, 0, 128)
    accent2 = (16, 132, 208)
    pl_text, pl_cur, pl_bg, pl_sel, pl_font = (0, 0, 0), (0, 0, 128), (255, 255, 255), (166, 202, 240), "Tahoma"
    vis_top, vis_bot = (0, 255, 0), (0, 140, 0)
    text_font = ("tahomabd.ttf", None)
    label_font = ("tahoma.ttf", None)
    GREY = (192, 192, 192)

    def body(self, w, h):
        return Image.new("RGB", (S(w), S(h)), self.GREY)

    def bevel(self, img, x0, y0, x1, y1, raised=True, fill=None, t=0.75):
        if fill is not None:
            rect(img, x0, y0, x1, y1, fill)
        a, b, c, d = ((255, 255, 255), (223, 223, 223), (128, 128, 128), (0, 0, 0)) if raised else \
                     ((128, 128, 128), (0, 0, 0), (255, 255, 255), (223, 223, 223))
        rect(img, x0, y0, x1, y0 + t, a if raised else a); rect(img, x0, y0, x0 + t, y1, a)
        rect(img, x0 + t, y0 + t, x1 - t, y0 + 2 * t, b); rect(img, x0 + t, y0 + t, x0 + 2 * t, y1 - t, b)
        rect(img, x0, y1 - t, x1, y1, d); rect(img, x1 - t, y0, x1, y1, d)
        rect(img, x0 + t, y1 - 2 * t, x1 - t, y1 - t, c); rect(img, x1 - 2 * t, y0 + t, x1 - t, y1 - t, c)

    def frame(self, img, x0, y0, x1, y1):
        self.bevel(img, x0, y0, x1, y1, True)

    def inset(self, img, x0, y0, x1, y1, fill, r=0):
        self.bevel(img, x0 - 1.5, y0 - 1.5, x1 + 1.5, y1 + 1.5, False, fill)

    def subpanel(self, img, x0, y0, x1, y1):
        self.bevel(img, x0, y0, x1, y1, False)
        self.bevel(img, x0 + 1.5, y0 + 1.5, x1 - 1.5, y1 - 1.5, True)

    def button(self, img, x, y, w, h, pressed, glyph=None, label=None, sel=False, kind="transport"):
        fill = self.GREY
        if sel:  # "checked" toggle: dithered light grey + sunken
            fill = (224, 224, 224)
        self.bevel(img, x, y, x + w, y + h, not (pressed or sel), fill)
        self.button_face(img, x, y, w, h, pressed or sel, glyph, label, False)

    def gadget(self, img, x, y, kind, pressed):
        if kind == "menu":  # tiny speaker icon instead of a system menu
            poly(img, [(x + 1.5, y + 3.5), (x + 3.5, y + 3.5), (x + 6, y + 1), (x + 6, y + 8), (x + 3.5, y + 5.5), (x + 1.5, y + 5.5)], (0, 0, 0))
            poly(img, [(x + 2, y + 3.9), (x + 3.6, y + 3.9), (x + 5.6, y + 2), (x + 5.6, y + 7), (x + 3.6, y + 5.1), (x + 2, y + 5.1)], (255, 255, 0))
            return
        self.bevel(img, x, y, x + 9, y + 8, not pressed, self.GREY, t=0.6)
        c = (0, 0, 0)
        if kind == "deck":
            self.gadget_glyph(img, x, y - 0.4, "deck", c)
            return
        if kind == "close":
            thick_line(img, x + 2.6, y + 2.3, x + 6.4, y + 5.9, 1.0, c)
            thick_line(img, x + 6.4, y + 2.3, x + 2.6, y + 5.9, 1.0, c)
        elif kind == "min":
            rect(img, x + 2.5, y + 5.2, x + 6, y + 6.2, c)
        else:
            rect(img, x + 2.3, y + 1.8, x + 6.7, y + 6.3, c)
            rect(img, x + 3, y + 2.9, x + 6, y + 5.6, self.GREY)

    def title_bar(self, active, title, shade=False):
        img = self.body(275, 14)
        self.bevel(img, 0, 0, 275, 14, True)
        bar = G.hgrad(272, 11, [(0.0, (0, 0, 128) if active else (128, 128, 128)),
                                (1.0, (16, 132, 208) if active else (192, 192, 192))])
        img.paste(bar, (S(1.5), S(1.5)))
        if shade:
            self.inset(img, 120, 3, 166, 11, self.lcd)
        text_at(img, 18, 10.2, title, (255, 255, 255) if active else (212, 208, 200), 7.5, "tahomabd.ttf", "ls")
        return img

    def groove_h(self, img, x0, y0, x1, y1):
        yc = (y0 + y1) / 2
        self.bevel(img, x0, yc - 1.5, x1, yc + 1.5, False, (255, 255, 255), t=0.6)

    def fill_h(self, img, x0, y0, x1, y1, frac):
        if frac > 0.01:
            n = int((x1 - x0) * frac / 2.5)
            for i in range(n):  # classic blocky progress bar
                rect(img, x0 + i * 2.5 + 0.3, y0 - 0.5, x0 + i * 2.5 + 2.1, y1 + 0.5, (0, 0, 128))

    def thumb(self, img, w, h, pressed, vertical=False):
        if vertical:
            self.bevel(img, 0, 0, w, h, not pressed, self.GREY, t=0.6)
        else:
            self.bevel(img, 0.5, 0, w - 0.5, h, not pressed, self.GREY, t=0.6)

    def eq_slot(self, cell, p):
        self.bevel(cell, 5.5, 1, 8.5, 62, False, (255, 255, 255), t=0.6)
        for yy in (5.5, 18.5, 31.5, 44.5, 57.5):
            rect(cell, 2, yy - 0.3, 4.5, yy + 0.3, (0, 0, 0))
            rect(cell, 9.5, yy - 0.3, 12, yy + 0.3, (0, 0, 0))

    def logo(self, img, x0, y0, x1, y1):
        # four-colour waving flag
        cols = [(255, 0, 0), (0, 160, 0), (0, 0, 255), (255, 210, 0)]
        for i, col in enumerate(cols):
            ox, oy = (i % 2) * 7, (i // 2) * 7
            poly(img, [(x0 + 7 + ox, y0 + 4 + oy), (x0 + 13 + ox, y0 + 3 + oy), (x0 + 13.5 + ox, y0 + 9 + oy),
                       (x0 + 7.5 + ox, y0 + 10 + oy)], col)
        for k in range(4):
            rect(img, x0 + 2 + k * 1.2, y0 + 5 + k * 3, x0 + 4.5 + k * 1.2, y0 + 6 + k * 3, (0, 0, 0))

    def eq_decor(self, bg):
        self.subpanel(bg, 6, 15, 269, 113)
        self.inset(bg, 86, 17, 199, 36, self.lcd)

    def label(self, img, x, y, s, col=None, size=5.5, anchor="ms"):
        text_at(img, x, y, s, col or (0, 0, 0), size, "tahoma.ttf", anchor)

    def make_panel(self):
        W, H = 275, 116
        p = self.body(W, H)
        self.bevel(p, 0, 0, W, H, True)
        # title bar like a child window
        bar = G.hgrad(W - 3, 11, [(0.0, (0, 0, 128)), (1.0, (16, 132, 208))])
        p.paste(bar, (S(1.5), S(1.5)))
        text_at(p, 6, 10, "Sound - Sound Recorder", WHITE, 7, "tahomabd.ttf", "ls")
        for i, k in enumerate(("min", "shade", "close")):
            self.gadget(p, W - 33 + i * 10, 3, k, False)
        text_at(p, 6, 22, "File   Edit   Effects   Help", BLACK, 6.2, "tahoma.ttf", "ls")
        text_at(p, 8, 33, "Position:", BLACK, 6, "tahoma.ttf", "ls")
        text_at(p, 200, 33, "Length:", BLACK, 6, "tahoma.ttf", "ls")
        self.inset(p, 70, 26, 190, 66, (0, 0, 0))
        rect(p, 70, 45.8, 190, 46.2, (0, 90, 0))
        self.groove_h(p, 10, 70, 265, 78)
        for i, (g, act) in enumerate(((g_prev, "prev"), (g_next, "next"), (g_play, "play"), (g_stop, "stop"))):
            self.button(p, 30 + i * 42, 84, 38, 22, False, glyph=g)
            self.buttons.append({"X": 30 + i * 42, "Y": 84, "W": 38, "H": 22, "Action": act})
        self.button(p, 206, 84, 58, 22, False, label="Playlist")
        self.buttons.append({"X": 206, "Y": 84, "W": 58, "H": 22, "Action": "playlist"})
        self.panel = p
        self.anims.append({"Window": "panel", "Mode": "scope", "X": 70, "Y": 26, "W": 120, "H": 40, "Color": "#00E600",
                           "Gain": 1.3})
        th = new_img(9, 14)
        self.bevel(th, 0, 0, 9, 10, True, self.GREY, t=0.6)
        poly(th, [(0, 10), (9, 10), (4.5, 14)], self.GREY)
        thick_line(th, 0.2, 10, 4.5, 14, 0.6, (255, 255, 255))
        thick_line(th, 8.8, 10, 4.5, 14, 0.6, (0, 0, 0))
        bgc = crop(p, 10, 67, 9, 14)
        bgc.paste(th, (0, 0), Image.eval(th.convert("L"), lambda v: 255 if v < 250 or True else 0))
        self.extra["thumb98"] = th
        self.anims.append({"Window": "panel", "Mode": "progress", "Image": "thumb98.png", "FrameW": 9, "FrameH": 14,
                           "Frames": 1, "X": 10, "Y": 67, "W": 255})
        self.anims.append({"Window": "panel", "Mode": "text", "Text": "time", "X": 8, "Y": 37, "W": 58, "H": 10,
                           "Font": "Tahoma", "Size": 7, "Color": "#000000"})
        self.anims.append({"Window": "panel", "Mode": "text", "Text": "remain", "X": 200, "Y": 37, "W": 66, "H": 10,
                           "Font": "Tahoma", "Size": 7, "Color": "#000000"})


# ============================================================================ 6. Pocket Stereo '79
class PocketStereo(Kit):
    """Late-70s portable cassette player: blue brushed metal, silver keys, orange accents."""
    key = "PocketStereo"
    name = "Pocket Stereo '79"
    title = "RETROAMP  ·  POCKET STEREO"
    lcd = (8, 8, 10)
    lcd_text = (255, 140, 40)
    lcd_dim = (130, 70, 20)
    lcd_ghost = (34, 20, 10)
    label_col = (226, 230, 238)
    glyph = (30, 34, 44)
    accent = (255, 120, 20)
    accent2 = (255, 190, 110)
    pl_text, pl_cur, pl_bg, pl_sel, pl_font = (255, 170, 90), (255, 255, 255), (10, 12, 18), (70, 46, 24), "Bahnschrift"
    vis_top, vis_bot = (255, 220, 160), (255, 100, 20)
    display_glow = 0.8
    cassette_stripe = (255, 110, 20)
    glass_mul, glass_add = (0.55, 0.62, 0.78), (0, 4, 14)
    deck_strip = "STEREO CASSETTE PLAYER  ·  2 HEADPHONE JACKS"
    ORANGE = (255, 120, 20)

    def body(self, w, h):
        return brushed(w, h, (58, 86, 148), var=8, seed=int(w * 11 + h * 3), vert=(14, -10))

    def silver(self, w, h, seed=0):
        return brushed(w, h, (196, 200, 208), var=7, seed=seed, vert=(14, -12))

    def frame(self, img, x0, y0, x1, y1):
        rect(img, x0, y0, x1, y0 + 1, (150, 176, 226)); rect(img, x0, y0, x0 + 1, y1, (120, 146, 200))
        rect(img, x0, y1 - 1, x1, y1, (18, 26, 52)); rect(img, x1 - 1, y0, x1, y1, (18, 26, 52))

    def inset(self, img, x0, y0, x1, y1, fill, r=3):
        G.rrect(img, x0 - 1, y0 - 1, x1 + 1, y1 + 1, r + 1, [(0.0, (230, 232, 238)), (1.0, (120, 124, 134))])
        G.rrect(img, x0, y0, x1, y1, r, fill, outline=(0, 0, 0), ow=0.4)

    def subpanel(self, img, x0, y0, x1, y1):
        G.rrect(img, x0, y0, x1, y1, 3, (20, 28, 54))
        panel = self.silver(x1 - x0 - 1.2, y1 - y0 - 1.2, seed=int(x0 + y0))
        img.paste(panel, (S(x0 + 0.6), S(y0 + 0.6)), G._rr_mask_px(panel.width, panel.height, 2.5 * K))

    def button(self, img, x, y, w, h, pressed, glyph=None, label=None, sel=False, kind="transport"):
        # chunky silver mechanical key with a dark slot around it
        G.rrect(img, x + 0.2, y + 0.2, x + w - 0.2, y + h - 0.2, 2.6, (16, 20, 36))
        key = self.silver(w - 1.8, h - (1.8 if not pressed else 1.2), seed=int(w * 7 + h * 5))
        if pressed:
            key = key.point(lambda v: int(v * 0.82))
        img.paste(key, (S(x + 0.9), S(y + (0.9 if not pressed else 1.4))), G._rr_mask_px(key.width, key.height, 2 * K))
        if not pressed:
            rect(img, x + 1.6, y + 1.1, x + w - 1.6, y + 1.5, (250, 250, 252))
            rect(img, x + 1.6, y + h - 1.5, x + w - 1.6, y + h - 1.0, (110, 114, 124))
        if sel or glyph is g_play:
            rect(img, x + 2, y + h - 2.6, x + w - 2, y + h - 1.8, self.ORANGE)
        self.button_face(img, x, y, w, h, pressed, glyph, label, sel)

    def gadget(self, img, x, y, kind, pressed):
        G.ellipse(img, x + 0.4, y + 0.4, x + 8.6, y + 8.6, [(0.0, (240, 242, 246)), (1.0, (140, 144, 154))] if not pressed
                  else [(0.0, (130, 134, 144)), (1.0, (210, 212, 218))], outline=(16, 20, 36), ow=0.45)
        if kind == "close":
            G.ellipse(img, x + 2.4, y + 2.4, x + 6.6, y + 6.6, self.ORANGE)
        else:
            self.gadget_glyph(img, x, y, kind, (30, 34, 44))

    def title_bar(self, active, title, shade=False):
        img = self.body(275, 14)
        self.frame(img, 0, 0, 275, 14)
        col = self.label_col if active else (150, 164, 196)
        rect(img, 1, 11.6, 274, 12.2, self.ORANGE if active else (120, 90, 70))
        if shade:
            self.inset(img, 118, 2.5, 166, 10.5, self.lcd, r=1.5)
            text_at(img, 20, 9.5, "RETROAMP", col, 6.5, "bahnschrift.ttf", "ls", "Bold")
            return img
        text_at(img, 137.5, 9.6, title, col, 6.6, "bahnschrift.ttf", "ms", "Bold")
        return img

    def groove_h(self, img, x0, y0, x1, y1):
        G.rrect(img, x0, y0, x1, y1, (y1 - y0) / 2, (12, 16, 30), outline=(120, 140, 190), ow=0.35)

    def fill_h(self, img, x0, y0, x1, y1, frac):
        if frac > 0.01:
            G.rrect(img, x0, y0, x0 + (x1 - x0) * frac, y1, (y1 - y0) / 2, [(0.0, self.accent2), (1.0, self.ORANGE)])

    def thumb(self, img, w, h, pressed, vertical=False):
        G.rrect(img, 0, 0, w, h, min(w, h) / 3, (16, 20, 36))
        cap = self.silver(w - 1.2, h - 1.2, seed=w * 3 + h)
        if pressed:
            cap = cap.point(lambda v: int(v * 0.85))
        img.paste(cap, (S(0.6), S(0.6)), G._rr_mask_px(cap.width, cap.height, min(w, h) / 3.4 * K))
        if vertical:
            rect(img, 2, h / 2 - 0.45, w - 2, h / 2 + 0.45, self.ORANGE)
        else:
            rect(img, w / 2 - 0.45, 2, w / 2 + 0.45, h - 2, self.ORANGE)

    def eq_slot(self, cell, p):
        G.rrect(cell, 5.2, 1, 8.8, 62, 1.8, (12, 16, 30), outline=(120, 140, 190), ow=0.3)
        center, pos = 31.5, 5.5 + (1 - p) * 51
        y0, y1 = sorted((center, pos))
        if y1 - y0 > 0.3:
            G.rrect(cell, 6, y0, 8, y1, 0.8, [(0.0, self.accent2), (1.0, self.ORANGE)])
        for yy in (5.5, 31.5, 57.5):
            rect(cell, 2.5, yy - 0.3, 4.5, yy + 0.3, self.label_col)
            rect(cell, 9.5, yy - 0.3, 11.5, yy + 0.3, self.label_col)

    def logo(self, img, x0, y0, x1, y1):
        # the famous orange "hotline" button
        cx, cy = (x0 + x1) / 2, y0 + 12
        G.ellipse(img, cx - 10, cy - 10, cx + 10, cy + 10, (16, 20, 36))
        G.ellipse(img, cx - 9, cy - 9, cx + 9, cy + 9, [(0.0, (255, 180, 90)), (0.6, (255, 110, 20)), (1.0, (200, 70, 10))])
        G.ellipse(img, cx - 5.5, cy - 7.5, cx + 3.5, cy - 2.5, (255, 220, 170))
        text_at(img, cx, y1 - 0.5, "HOTLINE", (240, 240, 244), 3.8, "bahnschrift.ttf", "ms", "Bold")

    def main_extras(self, img):
        # silver strip with the model name under the display (like the side of the player)
        text_at(img, 21, 112.3, "STEREO CASSETTE PLAYER", (200, 210, 232), 3.8, "bahnschrift.ttf", "ls", "SemiBold")

    def make_panel(self):
        self.cassette_deck("RETROAMP  POCKET STEREO  TPS-79")

    def deck_text(self):
        return (226, 230, 238), (170, 186, 220)

    def dsp_colors(self):
        return (30, 34, 44), (30, 34, 44), (226, 230, 238)


STYLES = [HiFiTower, Radiola, Synthwave, Aqua, Win98, PocketStereo]


# ============================================================================ decks for the older skins
class ThemeKit(Kit):
    """Deck materials for the gen_skin.py themes (Retro Blue, Steel Green, Crimson Night)."""

    def __init__(self, tkey):
        super().__init__()
        T = G.THEMES[tkey]
        self.T = T
        self.key = tkey
        self.lcd, self.lcd_text, self.lcd_dim, self.lcd_ghost = T["lcd"], T["lcd_text"], T["lcd_dim"], T["lcd_ghost"]
        light = sum(T["body"][0][1]) > 450
        self.label_col = T["glyph"] if light else T["ttext"]
        self.glyph = T["glyph"]
        self.accent, self.accent2 = T["accent"], T["accent_hi"]

    def body(self, w, h):
        return G.vgrad(w, h, self.T["body"])

    def frame(self, img, x0, y0, x1, y1):
        rect(img, x0, y0, x1, y0 + 1, self.T["edge"]); rect(img, x0, y1 - 1, x1, y1, self.T["edge"])
        rect(img, x0, y0, x0 + 1, y1, self.T["edge"]); rect(img, x1 - 1, y0, x1, y1, self.T["edge"])
        rect(img, x0 + 1, y0 + 1, x1 - 1, y0 + 1.6, self.T["hi"])

    def inset(self, img, x0, y0, x1, y1, fill, r=3):
        G.rrect(img, x0, y0, x1, y1, r, fill, outline=self.T["lcd_edge"])

    def button(self, img, x, y, w, h, pressed, glyph=None, label=None, sel=False, kind="transport"):
        fill = self.T["btn_dn"] if pressed else self.T["btn"]
        if sel:
            fill = [(0.0, self.T["accent_hi"]), (1.0, self.T["accent"])]
        G.rrect(img, x + 0.5, y + 0.5, x + w - 0.5, y + h - 0.5, (h - 1) / 2, fill, outline=self.T["btn_ring"])
        self.button_face(img, x, y, w, h, pressed, glyph, label, sel, col=(255, 255, 255) if sel else None)

    def dsp_colors(self):
        return self.T["glyph"], (255, 255, 255), self.label_col


class AmigaKit(Kit):
    """Deck materials for the Amiga Workbench skin (gen_amiga.py)."""
    key = "AmigaWorkbench"
    label_font = ("arialbd.ttf", None)

    def __init__(self):
        super().__init__()
        import gen_amiga as A
        self.A = A
        self.lcd, self.lcd_text, self.lcd_dim, self.lcd_ghost = A.DISP, A.WHITE, A.DISP_DIM, A.DISP_GHOST
        self.label_col, self.glyph = A.BLACK, A.BLACK

    def body(self, w, h):
        return Image.new("RGB", (S(w), S(h)), self.A.GREY)

    def frame(self, img, x0, y0, x1, y1):
        self.A.bevel(img, x0, y0, x1, y1, raised=True, fill=None)

    def inset(self, img, x0, y0, x1, y1, fill, r=3):
        self.A.bevel(img, x0 - 1, y0 - 1, x1 + 1, y1 + 1, raised=False, fill=fill)

    def button(self, img, x, y, w, h, pressed, glyph=None, label=None, sel=False, kind="transport"):
        self.A.button(img, x, y, w, h, pressed, glyph=glyph, label=label, sel=sel, cap=min(5.0, h * 0.42))

    def deck_text(self):
        return (0, 0, 0), (60, 60, 60)

    dsp_font = "Arial"

    def dsp_colors(self):
        return (0, 0, 0), (255, 255, 255), (0, 0, 0)


def add_deck(kit, title, label_extra=None, also_dirs=()):
    """Builds a cassette deck for an existing skin folder (skins/src/<key>) and re-zips its .wsz."""
    G.K = K
    kit.cassette_deck(title, label_extra=label_extra)
    out = os.path.join(G.ROOT, "skins", "src", kit.key)
    lines = ["; RetroAmp animation extension (see SKINNING.md) - cassette deck on the back of the playlist",
             "[Panel]", "Width=275", "Height=116", "Image=panel.png", ""]
    kit.extra["panel"] = kit.panel
    for i, a in enumerate(kit.anims):
        lines.append("[Anim%d]" % (i + 1))
        lines += ["%s=%s" % kv for kv in a.items()]
        lines.append("")
    for i, b in enumerate(kit.buttons):
        lines.append("[Button%d]" % (i + 1))
        lines += ["%s=%s" % kv for kv in b.items()]
        lines.append("")
    lines += kit.dsp_assets()
    pl = Image.open(os.path.join(out, "pledit.png")).convert("RGB")
    kit.turn_sprites(pl)
    kit.extra["pledit"] = pl
    for d in (out,) + tuple(also_dirs):
        for n, im in kit.extra.items():
            im.save(os.path.join(d, n + ".png"), optimize=True)
        with open(os.path.join(d, "anim.txt"), "w", newline="", encoding="utf-8") as f:
            f.write("\r\n".join(lines))
    with zipfile.ZipFile(os.path.join(G.ROOT, "skins", kit.key + ".wsz"), "w", zipfile.ZIP_DEFLATED) as z:
        for n in sorted(os.listdir(out)):
            z.write(os.path.join(out, n), n)
    print("deck added to", kit.key)


def build_old_decks():
    res_skin = os.path.join(G.ROOT, "res", "skin")
    add_deck(ThemeKit("RetroBlue"), "RETROAMP  STEREO CASSETTE DECK", also_dirs=(res_skin,))
    add_deck(ThemeKit("SteelGreen"), "RETROAMP  STEREO CASSETTE DECK")
    add_deck(ThemeKit("CrimsonNight"), "RETROAMP  STEREO CASSETTE DECK")
    amiga = AmigaKit()
    add_deck(amiga, "RetroAmp  Stereo Cassette Deck",
             label_extra=lambda p: amiga.A.boing_ball(p, 140, 44, 5.5, shadow=False))

if __name__ == "__main__":
    want = sys.argv[1:]
    for cls in STYLES:
        if not want or cls.key in want:
            cls().build()
    if not want or "decks" in want:
        build_old_decks()
