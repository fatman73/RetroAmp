#pragma once
#include "gfx.h"

enum SkinBmp {
    SB_MAIN,
    SB_CBUTTONS,
    SB_TITLEBAR,
    SB_SHUFREP,
    SB_TEXT,
    SB_NUMBERS,
    SB_NUMS_EX,
    SB_VOLUME,
    SB_BALANCE,
    SB_MONOSTER,
    SB_PLAYPAUS,
    SB_POSBAR,
    SB_EQMAIN,
    SB_PLEDIT,
    SB_COUNT
};

// RetroAmp extension: animated skin elements described in ANIM.TXT (see SKINNING.md).
enum AnimMode { AM_SPIN, AM_LOOP, AM_LEVEL, AM_STATE, AM_PROGRESS, AM_SCOPE, AM_SPECTRUM, AM_TEXT };
enum AnimWhen { AW_ALWAYS, AW_PLAYING, AW_PAUSED, AW_STOPPED, AW_ACTIVE };
enum AnimText { AT_TITLE, AT_TIME, AT_REMAIN, AT_BITRATE, AT_SAMPLERATE, AT_TRACK, AT_CLOCK, AT_COUNTER, AT_STATIC };

struct AnimElem {
    int window = 0;  // WndId (0 main, 1 eq, 2 playlist, 5 panel)
    int mode = AM_LOOP;
    int when = AW_ALWAYS;
    int image = -1;           // index into animImage()
    int sx = 0, sy = 0;       // first frame in the image (skin pixels)
    int fw = 0, fh = 0;       // frame size
    int frames = 1, cols = 0; // frame count, frames per row (0 = all in one row)
    int x = 0, y = 0, w = 0, h = 0;  // destination; w/h = travel area for PROGRESS, box for SCOPE/SPECTRUM/TEXT
    float fps = 12, attack = 0.6f, release = 0.12f, gain = 1.0f;
    int channel = 0;          // LEVEL: 0 mono, 1 left, 2 right, 3 bass, 4 mid, 5 treble
    uint32_t color = 0xFFFFFF, color2 = 0x000000;
    std::wstring font = L"Segoe UI", text;
    int size = 8, align = 0, textKind = AT_TITLE, bars = 0;
    bool bold = false;
};

// Clickable hot spot defined by a skin ([Button] sections in ANIM.TXT).
struct SkinButton {
    int window = 5;  // only the panel for now
    int x = 0, y = 0, w = 0, h = 0;
    std::wstring action;  // prev, play, pause, stop, next, eject, playlist
};

// Classic Winamp 2.x skin: a folder or .wsz/.zip archive of BMP sprite sheets.
class Skin {
public:
    struct Region {
        std::vector<int> counts;
        std::vector<POINT> pts;
        bool empty() const { return counts.empty(); }
    };

    bool loadDefault(HINSTANCE inst);
    bool loadFrom(const std::wstring& path, std::wstring& error);

    const Image& img(SkinBmp b) const { return prepared_ ? scaled_[b] : bmp_[b]; }
    // Builds the bitmaps for `scale` canvas pixels per skin pixel (call when zoom changes).
    void prepare(int scale);
    int hdScale() const { return bmp_[SB_MAIN].scale; }

    // animation extension
    std::vector<AnimElem> anims;
    std::vector<SkinButton> buttons;
    // [Dsp] - look of the Bass Boost & Effects window (otherwise it is built from generic boxes)
    struct DspStyle {
        int body = -1;    // image for the inner area (251x86)
        int button = -1;  // image 104x48: off, on, off-pressed, on-pressed (12 px each)
        uint32_t text = 0, textOn = 0xFFFFFF, label = 0;
        std::wstring font = L"Segoe UI";
        bool styled() const { return button >= 0; }
    } dspStyle;
    bool hasTurnSprite = false;  // pledit.bmp holds a TURN button at (100,43) / pressed (100,58)
    bool hasPanel = false;
    int panelW = 275, panelH = 116, panelImage = -1;
    const Image& animImage(int i) const {
        static Image empty;
        if (i < 0 || i >= (int)animImg_.size()) return empty;
        return prepared_ && i < (int)animScaled_.size() ? animScaled_[i] : animImg_[i];
    }
    bool hasAnims(int window) const {
        for (auto& a : anims)
            if (a.window == window) return true;
        return false;
    }

    std::wstring name = L"Default";
    bool useNumsEx = true;
    uint32_t plNormal = 0x00FF00, plCurrent = 0xFFFFFF, plNormalBG = 0x000000, plSelectedBG = 0x0000C6;
    std::wstring plFont = L"Arial";
    uint32_t vis[24] = {};
    Region regMain, regMainShade, regEq;

    // Draws text with TEXT.BMP (5x6 font). `keyed` makes the font background transparent.
    void drawText(Canvas& c, const std::string& text, int x, int y, bool keyed = false) const;
    uint32_t textBackground() const;

private:
    using FileMap = std::map<std::string, std::vector<uint8_t>>;
    bool apply(const FileMap& files, bool isDefault);
    void parsePledit(const std::wstring& txt);
    void parseViscolor(const std::wstring& txt);
    void parseRegion(const std::wstring& txt);
    void parseAnim(const std::wstring& txt, const FileMap& files, int hd);
    Image bmp_[SB_COUNT];
    Image scaled_[SB_COUNT];
    std::vector<Image> animImg_, animScaled_;
    int prepared_ = 0;
};
