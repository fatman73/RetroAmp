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
    Image bmp_[SB_COUNT];
    Image scaled_[SB_COUNT];
    int prepared_ = 0;
};
