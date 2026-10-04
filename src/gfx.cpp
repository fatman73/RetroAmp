#include "gfx.h"

#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

static ULONG_PTR g_gdiplusToken = 0;

void GdiplusStartupOnce() {
    if (g_gdiplusToken) return;
    Gdiplus::GdiplusStartupInput in;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &in, nullptr);
}

void GdiplusShutdownOnce() {
    if (g_gdiplusToken) Gdiplus::GdiplusShutdown(g_gdiplusToken);
    g_gdiplusToken = 0;
}

bool LoadImageFromMemory(const void* data, size_t size, Image& out) {
    GdiplusStartupOnce();
    IStream* stream = SHCreateMemStream((const BYTE*)data, (UINT)size);
    if (!stream) return false;
    bool ok = false;
    {
        Gdiplus::Bitmap bmp(stream, FALSE);
        if (bmp.GetLastStatus() == Gdiplus::Ok && bmp.GetWidth() > 0 && bmp.GetHeight() > 0) {
            int w = (int)bmp.GetWidth(), h = (int)bmp.GetHeight();
            Gdiplus::Rect r(0, 0, w, h);
            Gdiplus::BitmapData bd;
            if (bmp.LockBits(&r, Gdiplus::ImageLockModeRead, PixelFormat32bppRGB, &bd) == Gdiplus::Ok) {
                out.w = w;
                out.h = h;
                out.px.resize((size_t)w * h);
                for (int y = 0; y < h; y++) {
                    const uint32_t* src = (const uint32_t*)((const uint8_t*)bd.Scan0 + (ptrdiff_t)y * bd.Stride);
                    for (int x = 0; x < w; x++) out.px[(size_t)y * w + x] = src[x] & 0x00FFFFFF;
                }
                bmp.UnlockBits(&bd);
                ok = true;
            }
        }
    }
    stream->Release();
    return ok;
}

// ---- skin bitmap rescaling ----------------------------------------------
static Image Scale2x(const Image& in) {
    Image out;
    out.w = in.w * 2;
    out.h = in.h * 2;
    out.px.resize((size_t)out.w * out.h);
    auto P = [&](int x, int y) {
        x = Clamp(x, 0, in.w - 1);
        y = Clamp(y, 0, in.h - 1);
        return in.px[(size_t)y * in.w + x];
    };
    for (int y = 0; y < in.h; y++)
        for (int x = 0; x < in.w; x++) {
            uint32_t E = P(x, y), A = P(x, y - 1), B = P(x + 1, y), C = P(x - 1, y), D = P(x, y + 1);
            uint32_t e0 = (C == A && C != D && A != B) ? A : E;
            uint32_t e1 = (A == B && A != C && B != D) ? B : E;
            uint32_t e2 = (D == C && D != B && C != A) ? C : E;
            uint32_t e3 = (B == D && B != A && D != C) ? D : E;
            size_t o = (size_t)(y * 2) * out.w + x * 2;
            out.px[o] = e0;
            out.px[o + 1] = e1;
            out.px[o + out.w] = e2;
            out.px[o + out.w + 1] = e3;
        }
    return out;
}

static Image Scale3x(const Image& in) {
    Image out;
    out.w = in.w * 3;
    out.h = in.h * 3;
    out.px.resize((size_t)out.w * out.h);
    auto P = [&](int x, int y) {
        x = Clamp(x, 0, in.w - 1);
        y = Clamp(y, 0, in.h - 1);
        return in.px[(size_t)y * in.w + x];
    };
    for (int y = 0; y < in.h; y++)
        for (int x = 0; x < in.w; x++) {
            uint32_t A = P(x - 1, y - 1), B = P(x, y - 1), C = P(x + 1, y - 1);
            uint32_t D = P(x - 1, y), E = P(x, y), F = P(x + 1, y);
            uint32_t G = P(x - 1, y + 1), H = P(x, y + 1), I = P(x + 1, y + 1);
            uint32_t e[9];
            e[0] = (D == B && B != F && D != H) ? D : E;
            e[1] = ((D == B && B != F && D != H && E != C) || (B == F && B != D && F != H && E != A)) ? B : E;
            e[2] = (B == F && B != D && F != H) ? F : E;
            e[3] = ((D == B && B != F && D != H && E != G) || (D == H && D != B && H != F && E != A)) ? D : E;
            e[4] = E;
            e[5] = ((B == F && B != D && F != H && E != I) || (H == F && D != H && B != F && E != C)) ? F : E;
            e[6] = (D == H && D != B && H != F) ? D : E;
            e[7] = ((D == H && D != B && H != F && E != I) || (H == F && D != H && B != F && E != G)) ? H : E;
            e[8] = (H == F && D != H && B != F) ? F : E;
            for (int j = 0; j < 3; j++)
                for (int i = 0; i < 3; i++) out.px[(size_t)(y * 3 + j) * out.w + x * 3 + i] = e[j * 3 + i];
        }
    return out;
}

static Image BoxDown(const Image& in, int f) {
    Image out;
    out.w = in.w / f;
    out.h = in.h / f;
    out.px.resize((size_t)out.w * out.h);
    const int n = f * f;
    for (int y = 0; y < out.h; y++)
        for (int x = 0; x < out.w; x++) {
            uint32_t r = 0, g = 0, b = 0;
            for (int j = 0; j < f; j++) {
                const uint32_t* row = &in.px[(size_t)(y * f + j) * in.w + x * f];
                for (int i = 0; i < f; i++) {
                    r += (row[i] >> 16) & 255;
                    g += (row[i] >> 8) & 255;
                    b += row[i] & 255;
                }
            }
            out.px[(size_t)y * out.w + x] = ((r + n / 2) / n << 16) | ((g + n / 2) / n << 8) | ((b + n / 2) / n);
        }
    return out;
}

static Image ResizeBicubic(const Image& in, int W, int H) {
    Image out;
    out.w = W;
    out.h = H;
    out.px.resize((size_t)W * H);
    GdiplusStartupOnce();
    Gdiplus::Bitmap src(in.w, in.h, in.w * 4, PixelFormat32bppRGB, (BYTE*)in.px.data());
    Gdiplus::Bitmap dst(W, H, PixelFormat32bppRGB);
    {
        Gdiplus::Graphics g(&dst);
        g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        Gdiplus::ImageAttributes ia;
        ia.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
        g.DrawImage(&src, Gdiplus::Rect(0, 0, W, H), 0, 0, in.w, in.h, Gdiplus::UnitPixel, &ia);
    }
    Gdiplus::Rect r(0, 0, W, H);
    Gdiplus::BitmapData bd;
    if (dst.LockBits(&r, Gdiplus::ImageLockModeRead, PixelFormat32bppRGB, &bd) == Gdiplus::Ok) {
        for (int y = 0; y < H; y++) {
            const uint32_t* row = (const uint32_t*)((const uint8_t*)bd.Scan0 + (ptrdiff_t)y * bd.Stride);
            for (int x = 0; x < W; x++) out.px[(size_t)y * W + x] = row[x] & 0x00FFFFFF;
        }
        dst.UnlockBits(&bd);
    }
    return out;
}

Image RescaleImage(const Image& src, int target) {
    if (!src.valid() || target <= 0) return src;
    const int k = src.scale;
    Image out;
    if (k == target) {
        out = src;
    } else if (k > target && k % target == 0) {
        out = BoxDown(src, k / target);  // exact area average, e.g. HD 4x -> 2x / 1x
    } else if (k == 1) {
        // classic pixel art: smooth the jaggies with Scale2x/Scale3x instead of plain blocks
        Image t = src;
        int have = 1;
        while (have < target) {
            if (target % 3 == 0 && have * 3 <= target) {
                t = Scale3x(t);
                have *= 3;
            } else {
                t = Scale2x(t);
                have *= 2;
            }
        }
        t.scale = have;
        out = have == target ? t : ResizeBicubic(t, src.lw() * target, src.lh() * target);
    } else {
        out = ResizeBicubic(src, src.lw() * target, src.lh() * target);
    }
    out.scale = target;
    return out;
}

uint32_t BlendColor(uint32_t a, uint32_t b, float t) {
    auto ch = [&](int sh) {
        float x = ((a >> sh) & 255) * (1 - t) + ((b >> sh) & 255) * t;
        return (uint32_t)Clamp((int)(x + 0.5f), 0, 255) << sh;
    };
    return ch(16) | ch(8) | ch(0);
}

Canvas::~Canvas() { destroy(); }

void Canvas::destroy() {
    if (dc2_) {
        SelectObject(dc2_, old2_);
        DeleteObject(bmp2_);
        DeleteDC(dc2_);
    }
    dc2_ = nullptr;
    bmp2_ = nullptr;
    bits2_ = nullptr;
    w2_ = h2_ = 0;
    if (dc) {
        SelectObject(dc, old_);
        DeleteObject(bmp_);
        DeleteDC(dc);
    }
    dc = nullptr;
    bmp_ = nullptr;
    bits = nullptr;
}

void Canvas::create(int w, int h, int scale) {
    if (dc && w == lw && h == lh && scale == s) return;
    destroy();
    lw = w;
    lh = h;
    s = std::max(1, scale);
    pw = lw * s;
    ph = lh * s;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = pw;
    bi.bmiHeader.biHeight = -ph;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    HDC screen = GetDC(nullptr);
    dc = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    void* p = nullptr;
    bmp_ = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &p, nullptr, 0);
    bits = (uint32_t*)p;
    old_ = (HBITMAP)SelectObject(dc, bmp_);
    resetClip();
}

void Canvas::setClip(int x, int y, int w, int h) {
    cx0_ = std::max(0, x);
    cy0_ = std::max(0, y);
    cx1_ = std::min(lw, x + w);
    cy1_ = std::min(lh, y + h);
}

void Canvas::resetClip() {
    cx0_ = cy0_ = 0;
    cx1_ = lw;
    cy1_ = lh;
}

void Canvas::fill(int x, int y, int w, int h, uint32_t c) {
    if (!bits) return;
    int x0 = std::max(x, cx0_), y0 = std::max(y, cy0_);
    int x1 = std::min(x + w, cx1_), y1 = std::min(y + h, cy1_);
    if (x0 >= x1 || y0 >= y1) return;
    GdiFlush();
    for (int py = y0 * s; py < y1 * s; py++) {
        uint32_t* row = bits + (size_t)py * pw;
        for (int px = x0 * s; px < x1 * s; px++) row[px] = c;
    }
}

void Canvas::darken(int x, int y, int w, int h, float k) {
    if (!bits) return;
    int x0 = std::max(x, cx0_) * s, y0 = std::max(y, cy0_) * s;
    int x1 = std::min(x + w, cx1_) * s, y1 = std::min(y + h, cy1_) * s;
    GdiFlush();
    int m = (int)(Clamp(k, 0.0f, 1.0f) * 256);
    for (int py = y0; py < y1; py++) {
        uint32_t* row = bits + (size_t)py * pw;
        for (int px = x0; px < x1; px++) {
            uint32_t c = row[px];
            row[px] = ((((c >> 16) & 255) * m >> 8) << 16) | ((((c >> 8) & 255) * m >> 8) << 8) | ((c & 255) * m >> 8);
        }
    }
}

Image Canvas::snapshot() const {
    Image im;
    if (!bits) return im;
    GdiFlush();
    im.w = pw;
    im.h = ph;
    im.scale = s;
    im.px.assign(bits, bits + (size_t)pw * ph);
    return im;
}

void Canvas::blitScaled(const Image& img, float dx, float dy, float dw, float dh) {
    if (!bits || !img.valid() || dw <= 0 || dh <= 0) return;
    GdiFlush();
    int x0 = std::max((int)std::floor(dx * s), cx0_ * s), y0 = std::max((int)std::floor(dy * s), cy0_ * s);
    int x1 = std::min((int)std::ceil((dx + dw) * s), cx1_ * s), y1 = std::min((int)std::ceil((dy + dh) * s), cy1_ * s);
    const float fx = img.w / (dw * s), fy = img.h / (dh * s);
    for (int py = y0; py < y1; py++) {
        float sy = (py + 0.5f - dy * s) * fy - 0.5f;
        int iy = (int)std::floor(sy);
        int wy = (int)((sy - iy) * 256);
        int ya = Clamp(iy, 0, img.h - 1), yb = Clamp(iy + 1, 0, img.h - 1);
        const uint32_t* ra = &img.px[(size_t)ya * img.w];
        const uint32_t* rb = &img.px[(size_t)yb * img.w];
        uint32_t* dst = bits + (size_t)py * pw;
        for (int px = x0; px < x1; px++) {
            float sx = (px + 0.5f - dx * s) * fx - 0.5f;
            int ix = (int)std::floor(sx);
            int wx = (int)((sx - ix) * 256);
            int xa = Clamp(ix, 0, img.w - 1), xb = Clamp(ix + 1, 0, img.w - 1);
            uint32_t a = ra[xa], b = ra[xb], c = rb[xa], d = rb[xb];
            int w00 = (256 - wx) * (256 - wy), w10 = wx * (256 - wy), w01 = (256 - wx) * wy, w11 = wx * wy;
            auto ch = [&](int sh) {
                return ((((a >> sh) & 255) * w00 + ((b >> sh) & 255) * w10 + ((c >> sh) & 255) * w01 + ((d >> sh) & 255) * w11) >> 16) << sh;
            };
            dst[px] = ch(16) | ch(8) | ch(0);
        }
    }
}

void Canvas::frame(int x, int y, int w, int h, uint32_t c) {
    fill(x, y, w, 1, c);
    fill(x, y + h - 1, w, 1, c);
    fill(x, y, 1, h, c);
    fill(x + w - 1, y, 1, h, c);
}

void Canvas::blitImpl(const Image& img, int sx, int sy, int w, int h, int dx, int dy, bool flipX, bool keyed,
                      uint32_t key) {
    if (!bits || !img.valid() || w <= 0 || h <= 0) return;
    GdiFlush();
    const int k = img.scale;
    int x0 = std::max(dx, cx0_) * s, x1 = std::min(dx + w, cx1_) * s;
    int y0 = std::max(dy, cy0_) * s, y1 = std::min(dy + h, cy1_) * s;
    if (x0 >= x1 || y0 >= y1) return;
    static thread_local std::vector<int> cols;
    cols.resize(x1 - x0);
    for (int px = x0; px < x1; px++) {
        int so = (px - dx * s) * k / s;
        if (flipX) so = w * k - 1 - so;
        cols[px - x0] = sx * k + so;
    }
    for (int py = y0; py < y1; py++) {
        int srow = sy * k + (py - dy * s) * k / s;
        if (srow < 0 || srow >= img.h) continue;
        const uint32_t* src = &img.px[(size_t)srow * img.w];
        uint32_t* dst = bits + (size_t)py * pw;
        for (int i = 0, n = x1 - x0; i < n; i++) {
            int c = cols[i];
            if (c < 0 || c >= img.w) continue;
            uint32_t v = src[c];
            if (keyed && v == key) continue;
            dst[x0 + i] = v;
        }
    }
}

void Canvas::blit(const Image& img, int sx, int sy, int w, int h, int dx, int dy, bool flipX) {
    blitImpl(img, sx, sy, w, h, dx, dy, flipX, false, 0);
}

void Canvas::blitKeyed(const Image& img, int sx, int sy, int w, int h, int dx, int dy, uint32_t key) {
    blitImpl(img, sx, sy, w, h, dx, dy, false, true, key);
}

void Canvas::fillPhys(int px, int py, int w, int h, uint32_t c) {
    if (!bits) return;
    int x0 = std::max(px, cx0_ * s), y0 = std::max(py, cy0_ * s);
    int x1 = std::min(px + w, cx1_ * s), y1 = std::min(py + h, cy1_ * s);
    if (x0 >= x1 || y0 >= y1) return;
    GdiFlush();
    for (int y = y0; y < y1; y++) {
        uint32_t* row = bits + (size_t)y * pw;
        for (int x = x0; x < x1; x++) row[x] = c;
    }
}

void Canvas::spanPhys(int px, int py0, int py1, int thickness, uint32_t c) {
    if (py0 > py1) std::swap(py0, py1);
    int t = std::max(1, thickness);
    fillPhys(px - (t - 1) / 2, py0 - (t - 1) / 2, t, py1 - py0 + t, c);
}

void Canvas::tile(const Image& img, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
    if (sw <= 0 || sh <= 0) return;
    int ox0 = cx0_, oy0 = cy0_, ox1 = cx1_, oy1 = cy1_;
    setClip(std::max(dx, ox0), std::max(dy, oy0), std::min(dx + dw, ox1) - std::max(dx, ox0),
            std::min(dy + dh, oy1) - std::max(dy, oy0));
    for (int y = dy; y < dy + dh; y += sh)
        for (int x = dx; x < dx + dw; x += sw) blit(img, sx, sy, sw, sh, x, y);
    cx0_ = ox0;
    cy0_ = oy0;
    cx1_ = ox1;
    cy1_ = oy1;
}

int Canvas::textWidth(const std::wstring& str, HFONT font) {
    if (!dc || str.empty()) return 0;
    HGDIOBJ of = SelectObject(dc, font);
    SIZE sz = {};
    GetTextExtentPoint32W(dc, str.c_str(), (int)str.size(), &sz);
    SelectObject(dc, of);
    return (sz.cx + s - 1) / s;
}

void Canvas::text(const std::wstring& str, int x, int y, int w, int h, HFONT font, uint32_t color, UINT flags, int clipX,
                  int clipW) {
    if (!dc) return;
    int x0 = std::max(x, cx0_), y0 = std::max(y, cy0_);
    int x1 = std::min(x + w, cx1_), y1 = std::min(y + h, cy1_);
    if (clipX != INT_MIN) {
        x0 = std::max(x0, clipX);
        x1 = std::min(x1, clipX + clipW);
    }
    if (x0 >= x1 || y0 >= y1) return;
    GdiFlush();
    HGDIOBJ of = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, ToColorRef(color));
    int saved = SaveDC(dc);
    IntersectClipRect(dc, x0 * s, y0 * s, x1 * s, y1 * s);
    RECT r = {x * s, y * s, (x + w) * s, (y + h) * s};
    DrawTextW(dc, str.c_str(), (int)str.size(), &r, flags | DT_NOPREFIX);
    RestoreDC(dc, saved);
    SelectObject(dc, of);
    GdiFlush();
}

void Canvas::present(HDC target, int down) {
    if (!dc) return;
    GdiFlush();
    if (down <= 1) {
        BitBlt(target, 0, 0, pw, ph, dc, 0, 0, SRCCOPY);
        return;
    }
    int w = pw / 2, h = ph / 2;
    if (!dc2_ || w2_ != w || h2_ != h) {
        if (dc2_) {
            SelectObject(dc2_, old2_);
            DeleteObject(bmp2_);
            DeleteDC(dc2_);
        }
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -h;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        dc2_ = CreateCompatibleDC(dc);
        void* p = nullptr;
        bmp2_ = CreateDIBSection(dc2_, &bi, DIB_RGB_COLORS, &p, nullptr, 0);
        bits2_ = (uint32_t*)p;
        old2_ = (HBITMAP)SelectObject(dc2_, bmp2_);
        w2_ = w;
        h2_ = h;
    }
    // 2x2 box filter: every output pixel is the exact average of four rendered pixels
    for (int y = 0; y < h; y++) {
        const uint32_t* r0 = bits + (size_t)(y * 2) * pw;
        const uint32_t* r1 = r0 + pw;
        uint32_t* o = bits2_ + (size_t)y * w;
        for (int x = 0; x < w; x++) {
            uint32_t a = r0[x * 2], b = r0[x * 2 + 1], c = r1[x * 2], d = r1[x * 2 + 1];
            uint32_t rb = ((a & 0xFF00FF) + (b & 0xFF00FF) + (c & 0xFF00FF) + (d & 0xFF00FF) + 0x020002) >> 2;
            uint32_t g = ((a & 0x00FF00) + (b & 0x00FF00) + (c & 0x00FF00) + (d & 0x00FF00) + 0x000200) >> 2;
            o[x] = (rb & 0xFF00FF) | (g & 0x00FF00);
        }
    }
    BitBlt(target, 0, 0, w, h, dc2_, 0, 0, SRCCOPY);
}
