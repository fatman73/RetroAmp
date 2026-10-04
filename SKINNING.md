# Making RetroAmp skins

RetroAmp reads **classic Winamp 2.x skins**: a folder, `.wsz` or `.zip` with `main.bmp`, `titlebar.bmp`,
`cbuttons.bmp`, `shufrep.bmp`, `text.bmp`, `numbers.bmp` / `nums_ex.bmp`, `volume.bmp`, `balance.bmp`,
`monoster.bmp`, `playpaus.bmp`, `posbar.bmp`, `eqmain.bmp`, `pledit.bmp`, plus `pledit.txt`, `viscolor.txt`
and (optionally) `region.txt`. Sprite positions are the standard Winamp 2 ones. Any classic skin from
https://skins.webamp.org works unchanged.

On top of that, RetroAmp adds two optional extensions.

## 1. HD skins

Use the same layout, just N times bigger (RetroAmp's own skins use 4×). A skin counts as HD when `main` is
exactly `275·N × 116·N`. Every sheet is then N× larger and may be stored as PNG (`main.png` is the same as
`main.bmp`). RetroAmp scales HD skins to the current zoom with a box filter or bicubic resampling, and smooths
classic 1× skins with Scale2x/Scale3x.

## 2. Animations, panel and buttons: `anim.txt`

An INI file with one section per element. Coordinates are **skin pixels** (the 1× grid), so the same file works
for 1× and HD skins. Images are extra files in the skin, frames laid out left-to-right (wrapping after `Columns`).

```ini
[Panel]                 ; optional cassette deck = the BACK SIDE of the playlist panel (TURN button)
Width=275
Height=116
Image=panel.png

[Anim1]                 ; spinning cassette reel
Window=panel            ; main | eq | playlist | panel
Mode=spin               ; see the table below
Image=reel1.png
FrameW=18
FrameH=18
Frames=12
Columns=12              ; frames per row (default = Frames)
X=54
Y=63
FPS=14

[Anim2]                 ; analog VU needle
Window=panel
Mode=level
Channel=left            ; left | right | mono | bass | mid | treble
Image=vuleft.png
FrameW=44
FrameH=34
Frames=32
X=176
Y=18
Attack=0.55
Release=0.10
Gain=1.0

[Anim3]                 ; the song title written on the cassette label
Window=panel
Mode=text
Text=title              ; title | time | remain | bitrate | samplerate | track | clock | counter | any static text
X=33
Y=30
W=112
H=10
Font=Segoe Print
Size=7
Bold=1
Color=#1E2A78
Align=left              ; left (scrolls when too long) | center | right

[Button1]               ; clickable hot spot (panel)
X=176
Y=95
W=90
H=13
Action=playlist         ; prev | play | pause | stop | next | eject | playlist (switch deck <-> playlist)
```

| Mode | Behaviour |
|---|---|
| `spin` | advances `FPS` frames per second **while playing** (reels, CDs) |
| `loop` | always animates (lamps, scrolling grids); combine with `When=playing` |
| `level` | frame = audio level (VU needles, magic eye); `Attack`/`Release` set the ballistics |
| `state` | frame 0 = stopped, 1 = playing, 2 = paused (LEDs) |
| `progress` | frame = song position; with `Frames=1` the image slides across `W` (dial pointer) |
| `scope` | draws an oscilloscope in `X,Y,W,H` (`Color`, `Gain`) |
| `spectrum` | draws `Bars` spectrum bars in `X,Y,W,H` (gradient from `Color2` at the bottom to `Color` at the top) |
| `text` | draws live text (see `Text=`) with any installed font |

`When=always|playing|paused|stopped|active` limits when an element is drawn.

```ini
[Dsp]                   ; look of the Bass Boost & Effects window
Body=dspbody.png        ; 251x86 inner background
Button=dspbtn.png       ; 104x48: off, on, off-pressed, on-pressed (12 px each)
Text=#282832            ; button text, button text when on, labels / values
TextOn=#FFFFFF
Label=#3C3C3C
Font=Segoe UI

[Playlist]
TurnSprite=1            ; pledit.bmp contains the TURN sprite (see below)
```

**Playlist ↔ deck (TURN):** the playlist window is one panel with two sides. When a skin has a `[Panel]`,
it is drawn on the back of the playlist, fitted into the list area, and the playlist can be turned over:
- with the **TURN** button left of the playlist window buttons (window x `W-53..W-23`, y 2..13). Draw it as a
  30×11 sprite in `pledit.bmp` at (100,43), pressed at (100,58), and enable it with `[Playlist] TurnSprite=1`;
  without the sprite RetroAmp draws a plain button in the skin colours;
- with the `playlist` button action on the deck;
- with the gadget next to close, or with Alt+K.
`Window=panel` elements and buttons use the deck's own coordinates.

The generators in `tools/` (`gen_skin.py`, `gen_amiga.py`, `gen_kit.py`) build every bundled skin and are good
examples. `gen_kit.py` contains the HiFi Tower cassette deck, the Radiola magic eye, the Synthwave grid, the
Aqua CD and the Windows 98 Sound Recorder.
