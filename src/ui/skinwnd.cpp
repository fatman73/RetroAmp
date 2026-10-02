#include "skinwnd.h"

#include "../app.h"

static const wchar_t* kClassName = L"RetroAmpSkinWnd";
static const wchar_t* kMainClassName = L"RetroAmpMain";

SkinWnd::~SkinWnd() {
    if (hwnd) DestroyWindow(hwnd);
}

int SkinWnd::renderScale() const { return g_app->renderScale(); }
const Skin& SkinWnd::skin() const { return g_app->skin; }

bool SkinWnd::create(HWND owner, int x, int y, const wchar_t* title) {
    static bool registered = false;
    HINSTANCE inst = GetModuleHandleW(nullptr);
    if (!registered) {
        for (int i = 0; i < 2; i++) {
            WNDCLASSEXW wc = {sizeof(wc)};
            wc.style = CS_DBLCLKS;
            wc.lpfnWndProc = WndProc;
            wc.hInstance = inst;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
            wc.hIconSm = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, 0);
            wc.lpszClassName = i == 0 ? kMainClassName : kClassName;
            RegisterClassExW(&wc);
        }
        registered = true;
    }
    bool isMain = id == W_MAIN;
    DWORD style = WS_POPUP | WS_CLIPCHILDREN | (isMain ? (WS_SYSMENU | WS_MINIMIZEBOX) : 0);
    DWORD ex = isMain ? WS_EX_APPWINDOW : 0;
    hwnd = CreateWindowExW(ex, isMain ? kMainClassName : kClassName, title, style, x, y, g_app->phys(lw), g_app->phys(lh), owner, nullptr,
                           inst, this);
    if (!hwnd) return false;
    DragAcceptFiles(hwnd, TRUE);
    updateRegion();
    return true;
}

void SkinWnd::setLogicalSize(int w, int h) {
    lw = w;
    lh = h;
    applyScale();
}

void SkinWnd::applyScale() {
    if (!hwnd) return;
    SetWindowPos(hwnd, nullptr, 0, 0, g_app->phys(lw), g_app->phys(lh), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    updateRegion();
    redraw();
}

void SkinWnd::updateRegion() {
    if (!hwnd) return;
    const Skin::Region* r = region();
    if (!r || r->empty()) {
        SetWindowRgn(hwnd, nullptr, TRUE);
        return;
    }
    std::vector<POINT> pts = r->pts;
    for (auto& p : pts) {
        p.x = g_app->phys(p.x);
        p.y = g_app->phys(p.y);
    }
    std::vector<INT> counts(r->counts.begin(), r->counts.end());
    HRGN rgn = CreatePolyPolygonRgn(pts.data(), counts.data(), (int)counts.size(), WINDING);
    if (rgn) SetWindowRgn(hwnd, rgn, TRUE);  // system owns the region now
}

void SkinWnd::redraw() {
    dirty_ = true;
    if (hwnd) InvalidateRect(hwnd, nullptr, FALSE);
}

RECT SkinWnd::screenRect() const {
    RECT r = {};
    if (hwnd) GetWindowRect(hwnd, &r);
    return r;
}

void SkinWnd::moveTo(int x, int y) {
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

POINT SkinWnd::menuPoint(int lx, int ly) const {
    POINT p = {g_app->phys(lx), g_app->phys(ly)};
    ClientToScreen(hwnd, &p);
    return p;
}

void SkinWnd::startWindowDrag() {
    dragging_ = true;
    POINT pt;
    GetCursorPos(&pt);
    if (GetCapture() != hwnd) SetCapture(hwnd);
    g_app->beginDrag(this, pt);
}

void SkinWnd::onRightClick(int x, int y) {
    POINT pt;
    GetCursorPos(&pt);
    g_app->showMainMenu(hwnd, pt);
}

void SkinWnd::onDropFiles(const std::vector<std::wstring>& files, int x, int y) {
    g_app->onDropFiles(files, -1, false);
}

LRESULT SkinWnd::onMessage(UINT, WPARAM, LPARAM, bool& handled) {
    handled = false;
    return 0;
}

LRESULT CALLBACK SkinWnd::WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    SkinWnd* self = nullptr;
    if (msg == WM_NCCREATE) {
        self = (SkinWnd*)((CREATESTRUCTW*)lp)->lpCreateParams;
        self->hwnd = h;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self);
    } else {
        self = (SkinWnd*)GetWindowLongPtrW(h, GWLP_USERDATA);
    }
    if (!self) return DefWindowProcW(h, msg, wp, lp);
    return self->handle(msg, wp, lp);
}

LRESULT SkinWnd::handle(UINT msg, WPARAM wp, LPARAM lp) {
    bool handled = false;
    LRESULT r = onMessage(msg, wp, lp, handled);
    if (handled) return r;
    const int s = renderScale();
    const double z = g_app->zoom / 100.0;
    auto lx = [&] { return (int)std::floor(GET_X_LPARAM(lp) / z); };
    auto ly = [&] { return (int)std::floor(GET_Y_LPARAM(lp) / z); };
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            if (dirty_ || canvas_.s != s || canvas_.lw != lw || canvas_.lh != lh) {
                canvas_.create(lw, lh, s);
                canvas_.resetClip();
                paint(canvas_);
                dirty_ = false;
            }
            RECT live;
            bool hasLive = liveRect(live);
            if (hasLive) ExcludeClipRect(dc, live.left, live.top, live.right, live.bottom);
            canvas_.present(dc, g_app->downsample());
            if (hasLive) SelectClipRgn(dc, nullptr);
            postPaint(dc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            SetFocus(hwnd);
            onMouseDown(lx(), ly(), wp);
            return 0;
        case WM_LBUTTONDBLCLK:
            SetCapture(hwnd);
            if (!onDblClick(lx(), ly())) onMouseDown(lx(), ly(), wp);
            return 0;
        case WM_MOUSEMOVE:
            if (dragging_) {
                POINT pt;
                GetCursorPos(&pt);
                g_app->dragTo(pt);
            } else {
                onMouseMove(lx(), ly(), wp);
            }
            return 0;
        case WM_LBUTTONUP:
            if (dragging_) {
                dragging_ = false;
                g_app->endDrag();
                if (GetCapture() == hwnd) ReleaseCapture();
                return 0;
            }
            onMouseUp(lx(), ly());
            if (GetCapture() == hwnd) ReleaseCapture();
            return 0;
        case WM_CAPTURECHANGED:
            if ((HWND)lp == hwnd) return 0;  // capture re-taken by this same window
            if (dragging_) {
                dragging_ = false;
                g_app->endDrag();
            }
            onCaptureLost();
            return 0;
        case WM_RBUTTONUP:
            onRightClick(lx(), ly());
            return 0;
        case WM_MOUSEWHEEL: {
            POINT p = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &p);
            onWheel(GET_WHEEL_DELTA_WPARAM(wp), (int)(p.x / z), (int)(p.y / z));
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
            bool alt = msg == WM_SYSKEYDOWN || GetKeyState(VK_MENU) < 0;
            if (!alt && onKey((UINT)wp, ctrl, shift)) return 0;
            if (g_app->handleKey((UINT)wp, alt, ctrl, shift)) return 0;
            break;
        }
        case WM_SYSCHAR:
            return 0;  // no beep for Alt+letter shortcuts
        case WM_ACTIVATE:
            active = LOWORD(wp) != WA_INACTIVE;
            redraw();
            break;
        case WM_DROPFILES: {
            HDROP drop = (HDROP)wp;
            POINT pt;
            DragQueryPoint(drop, &pt);
            UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            std::vector<std::wstring> files;
            for (UINT i = 0; i < n; i++) {
                wchar_t buf[MAX_PATH * 4];
                if (DragQueryFileW(drop, i, buf, (UINT)std::size(buf))) files.push_back(buf);
            }
            DragFinish(drop);
            onDropFiles(files, (int)(pt.x / z), (int)(pt.y / z));
            return 0;
        }
        case WM_CLOSE:
            if (id == W_MAIN) {
                g_app->quit();
            } else {
                g_app->setWindowVisible(id, false);
            }
            return 0;
        case WM_DPICHANGED:
            return 0;  // keep our own integer scaling
        case WM_DESTROY:
            if (id == W_MAIN) PostQuitMessage(0);
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
