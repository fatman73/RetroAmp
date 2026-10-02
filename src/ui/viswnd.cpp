#include "viswnd.h"

#include "../app.h"

namespace {
enum { B_NONE = 0, B_CLOSE, B_PREV, B_NEXT, B_AUTO, B_FULL, B_RESIZE };
const UINT_PTR kVisTimer = 7;
const int kCmdPreset = 100, kCmdAuto = 200, kCmdFull = 201, kCmdClose = 202;

void DrawBox(Canvas& c, const Skin& sk, int x, int y, int w, int h, const std::string& text, bool on, bool pressed) {
    c.fill(x, y, w, h, on ? sk.plSelectedBG : sk.plNormalBG);
    if (pressed) c.fill(x, y, w, h, BlendColor(sk.plSelectedBG, sk.plNormal, 0.3f));
    c.frame(x, y, w, h, sk.plNormal);
    int tw = (int)text.size() * 5;
    sk.drawText(c, text, x + (w - tw) / 2 + (pressed ? 1 : 0), y + (h - 6) / 2 + (pressed ? 1 : 0), true);
}
}  // namespace

VisWnd::~VisWnd() { exitFullscreen(); }

void VisWnd::loadState() {
    eng_.setPreset(g_app->cfg.num(L"Vis", L"Preset", 2));
    auto_ = g_app->cfg.flag(L"Vis", L"Auto", false);
    tilesW = Clamp(g_app->cfg.num(L"Vis", L"TilesW", 0), 0, 60);
    tilesH = Clamp(g_app->cfg.num(L"Vis", L"TilesH", 2), 0, 60);
    lw = 275 + 25 * tilesW;
    lh = 116 + 29 * tilesH;
}

void VisWnd::saveState() {
    g_app->cfg.set(L"Vis", L"Preset", eng_.preset());
    g_app->cfg.setFlag(L"Vis", L"Auto", auto_);
    g_app->cfg.set(L"Vis", L"TilesW", tilesW);
    g_app->cfg.set(L"Vis", L"TilesH", tilesH);
}

void VisWnd::setPreset(int p) {
    eng_.setPreset(p);
    lastSwitch_ = GetTickCount64();
    nameUntil_ = lastSwitch_ + 3000;
    g_app->setMarqueeOverride(L"VIS: " + std::wstring(VisEngine::presetName(eng_.preset())));
    saveState();
    redraw();
}

void VisWnd::nextPreset(int d) { setPreset(eng_.preset() + d); }

void VisWnd::setAuto(bool on) {
    auto_ = on;
    lastSwitch_ = GetTickCount64();
    saveState();
    redraw();
}

int VisWnd::buttonAt(int x, int y) const {
    const int W = lw, H = lh;
    if (Rc{W - 11, 3, 9, 9}.hit(x, y)) return B_CLOSE;
    if (Rc{14, H - 14, 12, 11}.hit(x, y)) return B_PREV;
    if (Rc{28, H - 14, 12, 11}.hit(x, y)) return B_NEXT;
    if (Rc{W - 84, H - 14, 30, 11}.hit(x, y)) return B_AUTO;
    if (Rc{W - 52, H - 14, 30, 11}.hit(x, y)) return B_FULL;
    if (Rc{W - 20, H - 16, 20, 16}.hit(x, y)) return B_RESIZE;
    return B_NONE;
}

// ---------------------------------------------------------------------------
void VisWnd::paint(Canvas& c) {
    const Skin& sk = skin();
    const Image& pe = sk.img(SB_PLEDIT);
    const int W = lw, H = lh, ty = active ? 0 : 21;
    c.tile(pe, 127, ty, 25, 20, 25, 0, W - 50, 20);
    c.blit(pe, 0, ty, 25, 20, 0, 0);
    c.blit(pe, 153, ty, 25, 20, W - 25, 0);
    if (push_.is(B_CLOSE)) c.blit(pe, 52, 42, 9, 9, W - 11, 3);
    for (int y = 20; y < H - 16; y += 29) {
        c.setClip(0, 20, W, H - 36);
        c.blit(pe, 0, 42, 12, 29, 0, y);
        c.blit(pe, 0, 42, 12, 29, W - 12, y, true);
        c.resetClip();
    }
    c.tile(pe, 179, 22, 25, 16, 12, H - 16, W - 24, 16);
    c.blit(pe, 0, 72 + 22, 12, 16, 0, H - 16);
    c.blit(pe, 126 + 138, 72 + 22, 12, 16, W - 12, H - 16);
    Rc r = content();
    c.fill(r.x, r.y, r.w, r.h, 0);
    std::string title = "VISUALIZATION";
    int tw = (int)title.size() * 5 + 8;
    c.fill((W - tw) / 2, 4, tw, 10, sk.textBackground());
    sk.drawText(c, title, (W - tw) / 2 + 4, 6);

    DrawBox(c, sk, 14, H - 14, 12, 11, "(", false, push_.is(B_PREV));
    DrawBox(c, sk, 28, H - 14, 12, 11, ")", false, push_.is(B_NEXT));
    DrawBox(c, sk, W - 84, H - 14, 30, 11, "AUTO", auto_, push_.is(B_AUTO));
    DrawBox(c, sk, W - 52, H - 14, 30, 11, "FULL", false, push_.is(B_FULL));
    std::string name = ToSkinText(Fmt(L"%d/%d %s", eng_.preset() + 1, VisEngine::presetCount(),
                                      VisEngine::presetName(eng_.preset())));
    int nx = 44, nw = W - 84 - 4 - nx;
    c.fill(nx, H - 14, nw, 11, sk.plNormalBG);
    c.setClip(nx + 2, H - 12, nw - 4, 7);
    sk.drawText(c, name, nx + 3, H - 12, true);
    c.resetClip();
}

void VisWnd::drawFrame(HDC dc, int x, int y, int w, int h, bool fullscreen) {
    if (w <= 0 || h <= 0 || !eng_.width()) return;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = eng_.width();
    bi.bmiHeader.biHeight = -eng_.height();
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(dc, HALFTONE);
    SetBrushOrgEx(dc, 0, 0, nullptr);
    StretchDIBits(dc, x, y, w, h, 0, 0, eng_.width(), eng_.height(), eng_.pixels(), &bi, DIB_RGB_COLORS, SRCCOPY);
    if (fullscreen && GetTickCount64() < nameUntil_) {
        HFONT f = CreateFontW(-std::max(18, h / 22), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                              ANTIALIASED_QUALITY, 0, L"Segoe UI");
        HGDIOBJ of = SelectObject(dc, f);
        SetBkMode(dc, TRANSPARENT);
        std::wstring t = VisEngine::presetName(eng_.preset());
        RECT rc = {x + h / 30 + 2, y + h / 30 + 2, x + w, y + h};
        SetTextColor(dc, RGB(0, 0, 0));
        DrawTextW(dc, t.c_str(), -1, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE);
        OffsetRect(&rc, -2, -2);
        SetTextColor(dc, RGB(255, 255, 255));
        DrawTextW(dc, t.c_str(), -1, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE);
        SelectObject(dc, of);
        DeleteObject(f);
    }
}

bool VisWnd::liveRect(RECT& rc) const {
    if (fs_) return false;
    Rc r = content();
    rc = {g_app->phys(r.x), g_app->phys(r.y), g_app->phys(r.x + r.w), g_app->phys(r.y + r.h)};
    return true;
}

void VisWnd::postPaint(HDC dc) {
    if (fs_) return;
    Rc r = content();
    int x0 = g_app->phys(r.x), y0 = g_app->phys(r.y);
    drawFrame(dc, x0, y0, g_app->phys(r.x + r.w) - x0, g_app->phys(r.y + r.h) - y0, false);
}

void VisWnd::tick() {
    bool shown = IsWindowVisible(hwnd) && !IsIconic(GetWindow(hwnd, GW_OWNER) ? GetWindow(hwnd, GW_OWNER) : hwnd);
    if (!fs_ && !shown) return;
    if (auto_ && GetTickCount64() - lastSwitch_ > 25000) {
        setPreset(eng_.preset() + 1 + (int)(GetTickCount64() % (VisEngine::presetCount() - 1)));
    }
    bool playing = g_app->player.visSamples(samples_.data(), (int)samples_.size());
    an_.update(samples_.data(), (int)samples_.size(), g_app->player.sampleRate(), playing, audio_);
    int W, H;
    if (fs_) {
        RECT rc;
        GetClientRect(fs_, &rc);
        W = rc.right;
        H = rc.bottom;
    } else {
        Rc r = content();
        W = g_app->phys(r.w);
        H = g_app->phys(r.h);
    }
    // render at most ~640 px wide and let the GPU-free HALFTONE stretch do the rest (classic look)
    int f = std::max(1, (W + 639) / 640);
    eng_.resize(std::max(16, W / f), std::max(16, H / f));
    eng_.render(audio_);
    if (fs_) {
        InvalidateRect(fs_, nullptr, FALSE);
    } else {
        Rc r = content();
        RECT pr = {g_app->phys(r.x), g_app->phys(r.y), g_app->phys(r.x + r.w), g_app->phys(r.y + r.h)};
        InvalidateRect(hwnd, &pr, FALSE);
    }
}

// ---------------------------------------------------------------------------
LRESULT VisWnd::onMessage(UINT msg, WPARAM wp, LPARAM lp, bool& handled) {
    handled = false;
    if (msg == WM_CREATE) SetTimer(hwnd, kVisTimer, 16, nullptr);
    if (msg == WM_TIMER && wp == kVisTimer) {
        tick();
        handled = true;
    }
    return 0;
}

void VisWnd::onMouseDown(int x, int y, WPARAM) {
    int b = buttonAt(x, y);
    if (b == B_RESIZE) {
        resizing_ = true;
        GetCursorPos(&resizeStart_);
        resizeW_ = tilesW;
        resizeH_ = tilesH;
        return;
    }
    if (b == B_NONE) {
        if (!content().hit(x, y)) startWindowDrag();
        return;
    }
    push_.pressed = b;
    push_.inside = true;
    redraw();
}

void VisWnd::onMouseMove(int x, int y, WPARAM mk) {
    if (!(mk & MK_LBUTTON)) return;
    if (resizing_) {
        POINT p;
        GetCursorPos(&p);
        double z = g_app->zoom / 100.0;
        int tw = std::max(0, resizeW_ + (int)std::lround((p.x - resizeStart_.x) / (25.0 * z)));
        int th = std::max(0, resizeH_ + (int)std::lround((p.y - resizeStart_.y) / (29.0 * z)));
        if (tw != tilesW || th != tilesH) {
            tilesW = tw;
            tilesH = th;
            setLogicalSize(275 + 25 * tw, 116 + 29 * th);
        }
        return;
    }
    if (push_.pressed) {
        bool in = buttonAt(x, y) == push_.pressed;
        if (in != push_.inside) {
            push_.inside = in;
            redraw();
        }
    }
}

void VisWnd::onMouseUp(int x, int y) {
    if (resizing_) {
        resizing_ = false;
        saveState();
        return;
    }
    int id = push_.pressed;
    bool inside = push_.inside && buttonAt(x, y) == id;
    push_ = PushState();
    redraw();
    if (!id || !inside) return;
    switch (id) {
        case B_CLOSE: g_app->setWindowVisible(W_VIS, false); break;
        case B_PREV: nextPreset(-1); break;
        case B_NEXT: nextPreset(+1); break;
        case B_AUTO: setAuto(!auto_); break;
        case B_FULL: toggleFullscreen(); break;
    }
}

void VisWnd::onCaptureLost() {
    resizing_ = false;
    if (push_.pressed) {
        push_ = PushState();
        redraw();
    }
}

bool VisWnd::onDblClick(int x, int y) {
    if (content().hit(x, y)) {
        toggleFullscreen();
        return true;
    }
    return false;
}

void VisWnd::onWheel(int delta, int, int) { nextPreset(delta > 0 ? -1 : 1); }

bool VisWnd::onKey(UINT vk, bool ctrl, bool shift) {
    switch (vk) {
        case VK_LEFT: nextPreset(-1); return true;
        case VK_RIGHT: nextPreset(+1); return true;
        case 'F':
        case VK_RETURN: toggleFullscreen(); return true;
        case VK_ESCAPE: exitFullscreen(); return true;
    }
    return false;
}

void VisWnd::onRightClick(int x, int y) {
    POINT pt;
    GetCursorPos(&pt);
    showMenu(pt);
}

void VisWnd::showMenu(POINT pt) {
    HMENU m = CreatePopupMenu();
    for (int i = 0; i < VisEngine::presetCount(); i++)
        AppendMenuW(m, MF_STRING | (i == eng_.preset() ? MF_CHECKED : 0), kCmdPreset + i,
                    Fmt(L"%d. %s", i + 1, VisEngine::presetName(i)).c_str());
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (auto_ ? MF_CHECKED : 0), kCmdAuto, L"Auto change preset (every 25 s)");
    AppendMenuW(m, MF_STRING | (fs_ ? MF_CHECKED : 0), kCmdFull, L"Fullscreen\tF / double-click");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, kCmdClose, L"Close visualization\tAlt+V");
    HWND owner = fs_ ? fs_ : hwnd;
    SetForegroundWindow(owner);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, owner, nullptr);
    DestroyMenu(m);
    if (cmd >= kCmdPreset && cmd < kCmdPreset + VisEngine::presetCount()) setPreset(cmd - kCmdPreset);
    else if (cmd == kCmdAuto) setAuto(!auto_);
    else if (cmd == kCmdFull) toggleFullscreen();
    else if (cmd == kCmdClose) {
        exitFullscreen();
        g_app->setWindowVisible(W_VIS, false);
    }
}

// ---------------------------------------------------------------------------
// Fullscreen output window
void VisWnd::toggleFullscreen() {
    if (fs_) {
        exitFullscreen();
        return;
    }
    static bool registered = false;
    HINSTANCE inst = GetModuleHandleW(nullptr);
    if (!registered) {
        WNDCLASSEXW wc = {sizeof(wc)};
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = FsProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = L"RetroAmpVisFullscreen";
        RegisterClassExW(&wc);
        registered = true;
    }
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
    RECT r = mi.rcMonitor;
    fs_ = CreateWindowExW(WS_EX_TOPMOST, L"RetroAmpVisFullscreen", L"RetroAmp Visualization", WS_POPUP, r.left, r.top,
                          r.right - r.left, r.bottom - r.top, nullptr, nullptr, inst, this);
    ShowWindow(fs_, SW_SHOW);
    SetForegroundWindow(fs_);
    fsMouseMove_ = GetTickCount64();
    nameUntil_ = GetTickCount64() + 3000;
}

void VisWnd::exitFullscreen() {
    if (!fs_) return;
    HWND h = fs_;
    fs_ = nullptr;
    DestroyWindow(h);
    if (hwnd) redraw();
}

LRESULT CALLBACK VisWnd::FsProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    VisWnd* self = (VisWnd*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (msg == WM_NCCREATE) {
        self = (VisWnd*)((CREATESTRUCTW*)lp)->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self);
    }
    if (!self) return DefWindowProcW(h, msg, wp, lp);
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            self->drawFrame(dc, 0, 0, rc.right, rc.bottom, true);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT && GetTickCount64() - self->fsMouseMove_ > 2000) {
                SetCursor(nullptr);
                return TRUE;
            }
            break;
        case WM_MOUSEMOVE:
            self->fsMouseMove_ = GetTickCount64();
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            return 0;
        case WM_LBUTTONDBLCLK:
            self->exitFullscreen();
            return 0;
        case WM_RBUTTONUP: {
            POINT pt;
            GetCursorPos(&pt);
            self->showMenu(pt);
            return 0;
        }
        case WM_MOUSEWHEEL:
            self->nextPreset(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -1 : 1);
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
            bool alt = msg == WM_SYSKEYDOWN;
            if (!alt && self->onKey((UINT)wp, ctrl, shift)) return 0;
            if (g_app->handleKey((UINT)wp, alt, ctrl, shift)) return 0;
            break;
        }
        case WM_ACTIVATE:
            if (LOWORD(wp) == WA_INACTIVE && self->fs_ == h) {
                // keep running on another monitor, but don't stay on top of everything
                SetWindowPos(h, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            } else if (LOWORD(wp) != WA_INACTIVE) {
                SetWindowPos(h, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            break;
        case WM_CLOSE:
            self->exitFullscreen();
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}
