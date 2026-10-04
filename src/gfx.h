#pragma once
#include "common.h"

// 32-bit pixel image, pixels stored as 0x00RRGGBB (matches a top-down 32bpp DIB).
// `scale` = physical pixels per skin pixel (1 for classic skins, e.g. 4 for HD skins).
struct Image {
    int w = 0, h = 0;
    int scale = 1;
    std::vector<uint32_t> px;
    bool valid() const { return w > 0 && h > 0; }
    int lw() const { return w / scale; }
    int lh() const { return h / scale; }
    // pixel at skin (logical) coordinates
    uint32_t get(int x, int y) const {
        x *= scale;
        y *= scale;
        return (x >= 0 && y >= 0 && x < w && y < h) ? px[(size_t)y * w + x] : 0;
    }
};

bool LoadImageFromMemory(const void* data, size_t size, Image& out);  // BMP/PNG/GIF... via GDI+
// Resizes `src` (any scale) to `target` physical pixels per skin pixel with the best method:
// box filter / bicubic for HD art, Scale2x/Scale3x edge smoothing for classic pixel art.
Image RescaleImage(const Image& src, int target);
void GdiplusStartupOnce();
void GdiplusShutdownOnce();

inline uint32_t RGBx(int r, int g, int b) { return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b; }
inline COLORREF ToColorRef(uint32_t c) { return RGB((c >> 16) & 255, (c >> 8) & 255, c & 255); }
uint32_t BlendColor(uint32_t a, uint32_t b, float t);

struct Rc {
    int x = 0, y = 0, w = 0, h = 0;
    bool hit(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

// Off-screen canvas. All drawing coordinates are logical (skin) pixels; the canvas is
// `scale` times larger physically and sprites are scaled with nearest-neighbour.
class Canvas {
public:
    ~Canvas();
    void create(int lw, int lh, int scale);
    int lw = 0, lh = 0, s = 1, pw = 0, ph = 0;
    HDC dc = nullptr;
    uint32_t* bits = nullptr;

    void setClip(int x, int y, int w, int h);
    void resetClip();

    // Source/destination in skin pixels; the image may have any `scale`.
    void blit(const Image& img, int sx, int sy, int w, int h, int dx, int dy, bool flipX = false);
    // Blit skipping pixels equal to `key`.
    void blitKeyed(const Image& img, int sx, int sy, int w, int h, int dx, int dy, uint32_t key);
    // Fills destination rect by repeating the source rect.
    void tile(const Image& img, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh);
    void fill(int x, int y, int w, int h, uint32_t c);
    void pixel(int x, int y, uint32_t c) { fill(x, y, 1, 1, c); }
    void frame(int x, int y, int w, int h, uint32_t c);
    void darken(int x, int y, int w, int h, float k);  // multiplies the area by k (pressed look)
    // Copy of the whole canvas as an Image (scale = this canvas' scale).
    Image snapshot() const;
    // Draws the whole image into a logical (fractional) rectangle with bilinear filtering.
    void blitScaled(const Image& img, float dx, float dy, float dw, float dh);
    // Physical-pixel drawing (for smooth curves at high zoom); clipped to the logical clip.
    void fillPhys(int px, int py, int w, int h, uint32_t c);
    void spanPhys(int px, int py0, int py1, int thickness, uint32_t c);

    // TrueType text, coordinates logical, font height already scaled by caller.
    // Optional horizontal clip (clipX/clipW) in addition to the canvas clip, for scrolling text.
    void text(const std::wstring& str, int x, int y, int w, int h, HFONT font, uint32_t color, UINT dtFlags,
              int clipX = INT_MIN, int clipW = 0);
    int textWidth(const std::wstring& str, HFONT font);  // in skin pixels
    // down = 2 averages 2x2 blocks (used for fractional zoom such as 150%).
    void present(HDC target, int down = 1);

private:
    void destroy();
    void blitImpl(const Image& img, int sx, int sy, int w, int h, int dx, int dy, bool flipX, bool keyed, uint32_t key);
    HBITMAP bmp_ = nullptr, old_ = nullptr;
    HDC dc2_ = nullptr;
    HBITMAP bmp2_ = nullptr, old2_ = nullptr;
    uint32_t* bits2_ = nullptr;
    int w2_ = 0, h2_ = 0;
    int cx0_ = 0, cy0_ = 0, cx1_ = 0, cy1_ = 0;  // logical clip
};
