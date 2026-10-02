#include "app.h"

#include <shobjidl.h>

#include "../res/resource.h"
#include "ui/dspwnd.h"
#include "ui/eqwnd.h"
#include "ui/mainwnd.h"
#include "ui/plwnd.h"
#include "ui/viswnd.h"

App* g_app = nullptr;

namespace {

const UINT_PTR kTimerId = 1;

struct BassPreset {
    const wchar_t* name;
    float boost, freq, sub, harm, width;
    bool loud;
};
const BassPreset kBassPresets[] = {
    {L"Off (flat)", 0, 80, 0, 0, 1.0f, false},
    {L"Light warmth", 4, 90, 0, 0.15f, 1.0f, false},
    {L"Deep bass", 6, 60, 5, 0, 1.0f, false},
    {L"Club", 8, 100, 3, 0.25f, 1.2f, false},
    {L"Car subwoofer", 9, 55, 8, 0, 1.0f, false},
    {L"Earthquake", 14, 70, 10, 0.3f, 1.1f, false},
    {L"Small speakers / laptop", 3, 140, 0, 0.8f, 1.0f, false},
    {L"Headphones", 5, 80, 3, 0.2f, 0.9f, false},
    {L"Night mode (quiet listening)", 4, 100, 2, 0.3f, 1.0f, true},
    {L"Wide stereo", 0, 80, 0, 0, 1.6f, false},
};

std::wstring SerializeEq(const float* bands, float preamp) {
    std::wstring s = Fmt(L"%.1f", preamp);
    for (int i = 0; i < kEqBands; i++) s += Fmt(L",%.1f", bands[i]);
    return s;
}

bool ParseEq(const std::wstring& s, float* bands, float& preamp) {
    std::vector<float> v;
    const wchar_t* p = s.c_str();
    while (*p) {
        wchar_t* end = nullptr;
        float f = wcstof(p, &end);
        if (end == p) {
            p++;
            continue;
        }
        v.push_back(f);
        p = end;
    }
    if (v.size() < kEqBands + 1) return false;
    preamp = Clamp(v[0], -12.0f, 12.0f);
    for (int i = 0; i < kEqBands; i++) bands[i] = Clamp(v[i + 1], -12.0f, 12.0f);
    return true;
}

// ---- simple modal dialogs (templates in retroamp.rc)
struct InputDlgData {
    std::wstring title, prompt, value;
};

INT_PTR CALLBACK InputDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    auto* d = (InputDlgData*)GetWindowLongPtrW(dlg, DWLP_USER);
    switch (msg) {
        case WM_INITDIALOG:
            SetWindowLongPtrW(dlg, DWLP_USER, lp);
            d = (InputDlgData*)lp;
            SetWindowTextW(dlg, d->title.c_str());
            SetDlgItemTextW(dlg, IDC_INPUT_PROMPT, d->prompt.c_str());
            SetDlgItemTextW(dlg, IDC_INPUT_EDIT, d->value.c_str());
            SendDlgItemMessageW(dlg, IDC_INPUT_EDIT, EM_SETSEL, 0, -1);
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wp) == IDOK) {
                wchar_t buf[4096];
                GetDlgItemTextW(dlg, IDC_INPUT_EDIT, buf, (int)std::size(buf));
                d->value = buf;
                EndDialog(dlg, IDOK);
                return TRUE;
            }
            if (LOWORD(wp) == IDCANCEL) {
                EndDialog(dlg, IDCANCEL);
                return TRUE;
            }
            break;
    }
    return FALSE;
}

bool InputBox(HWND owner, const std::wstring& title, const std::wstring& prompt, std::wstring& value) {
    InputDlgData d{title, prompt, value};
    if (DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_INPUT), owner, InputDlgProc, (LPARAM)&d) != IDOK)
        return false;
    value = Trim(d.value);
    return !value.empty();
}

struct JumpData {
    std::vector<int> shown;
    int result = -1;
};

void FillJumpList(HWND dlg, JumpData* d) {
    wchar_t buf[512];
    GetDlgItemTextW(dlg, IDC_JUMP_EDIT, buf, (int)std::size(buf));
    std::wstring q = ToLower(Trim(buf));
    HWND list = GetDlgItem(dlg, IDC_JUMP_LIST);
    SendMessageW(list, WM_SETREDRAW, FALSE, 0);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    d->shown.clear();
    // every whitespace separated word must match
    std::vector<std::wstring> words;
    size_t a = 0;
    while (a < q.size()) {
        size_t b = q.find(L' ', a);
        if (b == std::wstring::npos) b = q.size();
        if (b > a) words.push_back(q.substr(a, b - a));
        a = b + 1;
    }
    const Playlist& pl = g_app->pl;
    for (int i = 0; i < pl.size(); i++) {
        std::wstring line = Fmt(L"%d. ", i + 1) + pl.tracks[i].display();
        std::wstring hay = ToLower(line + L" " + pl.tracks[i].path);
        bool ok = true;
        for (auto& w : words)
            if (hay.find(w) == std::wstring::npos) {
                ok = false;
                break;
            }
        if (!ok) continue;
        SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)line.c_str());
        d->shown.push_back(i);
    }
    SendMessageW(list, LB_SETCURSEL, 0, 0);
    SendMessageW(list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list, nullptr, TRUE);
}

LRESULT CALLBACK JumpEditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref) {
    if (msg == WM_KEYDOWN && (wp == VK_DOWN || wp == VK_UP || wp == VK_PRIOR || wp == VK_NEXT)) {
        SendMessageW((HWND)ref, msg, wp, lp);  // forward navigation keys to the list
        return 0;
    }
    return DefSubclassProc(h, msg, wp, lp);
}

INT_PTR CALLBACK JumpDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    auto* d = (JumpData*)GetWindowLongPtrW(dlg, DWLP_USER);
    switch (msg) {
        case WM_INITDIALOG:
            SetWindowLongPtrW(dlg, DWLP_USER, lp);
            d = (JumpData*)lp;
            SetWindowSubclass(GetDlgItem(dlg, IDC_JUMP_EDIT), JumpEditProc, 1, (DWORD_PTR)GetDlgItem(dlg, IDC_JUMP_LIST));
            FillJumpList(dlg, d);
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wp) == IDC_JUMP_EDIT && HIWORD(wp) == EN_CHANGE) {
                FillJumpList(dlg, d);
                return TRUE;
            }
            if ((LOWORD(wp) == IDC_JUMP_LIST && HIWORD(wp) == LBN_DBLCLK) || LOWORD(wp) == IDOK) {
                int sel = (int)SendDlgItemMessageW(dlg, IDC_JUMP_LIST, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)d->shown.size()) d->result = d->shown[sel];
                EndDialog(dlg, IDOK);
                return TRUE;
            }
            if (LOWORD(wp) == IDCANCEL) {
                EndDialog(dlg, IDCANCEL);
                return TRUE;
            }
            break;
    }
    return FALSE;
}

// ---- thumbnail toolbar icons, rendered at runtime (anti-aliased)
HICON MakeGlyphIcon(int kind, int size) {
    BITMAPV5HEADER bi = {};
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = size;
    bi.bV5Height = -size;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;
    HDC dc = GetDC(nullptr);
    void* bits = nullptr;
    HBITMAP color = CreateDIBSection(dc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, dc);
    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
    auto inTri = [](float px, float py, float ax, float ay, float bx, float by, float cx, float cy) {
        auto sgn = [](float x1, float y1, float x2, float y2, float x3, float y3) {
            return (x1 - x3) * (y2 - y3) - (x2 - x3) * (y1 - y3);
        };
        float d1 = sgn(px, py, ax, ay, bx, by), d2 = sgn(px, py, bx, by, cx, cy), d3 = sgn(px, py, cx, cy, ax, ay);
        bool neg = d1 < 0 || d2 < 0 || d3 < 0, pos = d1 > 0 || d2 > 0 || d3 > 0;
        return !(neg && pos);
    };
    auto inRect = [](float px, float py, float x, float y, float w, float h) {
        return px >= x && px < x + w && py >= y && py < y + h;
    };
    auto inside = [&](float x, float y) {  // 16x16 design space
        switch (kind) {
            case 0: return inRect(x, y, 2, 3, 2, 10) || inTri(x, y, 14, 3, 14, 13, 4.5f, 8);  // prev
            case 1: return inTri(x, y, 4, 2, 4, 14, 13.5f, 8);                                 // play
            case 2: return inRect(x, y, 3, 2, 3.5f, 12) || inRect(x, y, 9.5f, 2, 3.5f, 12);    // pause
            case 3: return inRect(x, y, 12, 3, 2, 10) || inTri(x, y, 2, 3, 2, 13, 11.5f, 8);   // next
            default: return inRect(x, y, 3, 3, 10, 10);                                        // stop
        }
    };
    uint32_t* px = (uint32_t*)bits;
    const int ss = 4;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            int cov = 0;
            for (int sy = 0; sy < ss; sy++)
                for (int sx = 0; sx < ss; sx++) {
                    float fx = (x + (sx + 0.5f) / ss) * 16.0f / size, fy = (y + (sy + 0.5f) / ss) * 16.0f / size;
                    cov += inside(fx, fy) ? 1 : 0;
                }
            uint32_t a = cov * 255 / (ss * ss);
            px[y * size + x] = (a << 24) | (a << 16) | (a << 8) | a;  // premultiplied white
        }
    ICONINFO ii = {TRUE, 0, 0, mask, color};
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(color);
    DeleteObject(mask);
    return icon;
}

bool RectsAdjacent(const RECT& a, const RECT& b) {
    const int t = 1;
    bool vOverlap = a.top < b.bottom && b.top < a.bottom;
    bool hOverlap = a.left < b.right && b.left < a.right;
    if (vOverlap && (std::abs(a.right - b.left) <= t || std::abs(a.left - b.right) <= t)) return true;
    if (hOverlap && (std::abs(a.bottom - b.top) <= t || std::abs(a.top - b.bottom) <= t)) return true;
    return false;
}

void OffsetR(RECT& r, int dx, int dy) {
    r.left += dx;
    r.right += dx;
    r.top += dy;
    r.bottom += dy;
}

}  // namespace

// ===========================================================================
App::App() { g_app = this; }

App::~App() {
    for (auto& w : wnd) delete w;
    for (auto& i : thumbIcons_)
        if (i) DestroyIcon(i);
    if (taskbar_) ((IUnknown*)taskbar_)->Release();
    g_app = nullptr;
}

bool App::init(HINSTANCE hinst, const std::vector<std::wstring>& args) {
    inst = hinst;
    dataDir_ = GetAppDataDir();
    cfg.setPath(PathJoin(dataDir_, L"retroamp.ini"));
    skin.loadDefault(inst);
    loadSettings();
    if (!skinPath.empty() && !loadSkin(skinPath, false)) skinPath.clear();
    skin.prepare(renderScale());
    createWindows();
    player.init(mainWnd->hwnd);
    player.setParams(dsp);
    meta.start(mainWnd->hwnd);

    // restore last playlist
    std::wstring plPath = PathJoin(dataDir_, L"playlist.m3u8");
    if (FileExists(plPath)) {
        pl.loadFile(plPath);
        for (auto& t : pl.tracks)
            if (t.length >= 0 && !t.title.empty()) t.metaDone = true;
        int cur = cfg.num(L"Main", L"Current", -1);
        pl.current = cur >= 0 && cur < pl.size() ? cur : -1;
        for (int i = 0; i < pl.size(); i++) requestMeta(i);
    }
    if (!args.empty()) addPaths(args, -1, true, true);

    SetTimer(mainWnd->hwnd, kTimerId, 30, nullptr);
    RegisterHotKey(mainWnd->hwnd, 1, MOD_NOREPEAT, VK_MEDIA_PLAY_PAUSE);
    RegisterHotKey(mainWnd->hwnd, 2, MOD_NOREPEAT, VK_MEDIA_NEXT_TRACK);
    RegisterHotKey(mainWnd->hwnd, 3, MOD_NOREPEAT, VK_MEDIA_PREV_TRACK);
    RegisterHotKey(mainWnd->hwnd, 4, MOD_NOREPEAT, VK_MEDIA_STOP);
    taskbarMsg_ = RegisterWindowMessageW(L"TaskbarButtonCreated");
    ChangeWindowMessageFilterEx(mainWnd->hwnd, taskbarMsg_, MSGFLT_ALLOW, nullptr);
    ChangeWindowMessageFilterEx(mainWnd->hwnd, WM_COMMAND, MSGFLT_ALLOW, nullptr);
    updateTitle();
    return true;
}

int App::run() {
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return 0;
}

void App::quit() {
    if (quitting_) return;
    quitting_ = true;
    KillTimer(mainWnd->hwnd, kTimerId);
    saveSettings();
    pl.saveM3U(PathJoin(dataDir_, L"playlist.m3u8"));
    meta.cancelAll();
    visWnd->exitFullscreen();
    visWnd->saveState();
    player.stop();
    for (int i = W_COUNT - 1; i >= 1; i--)
        if (wnd[i] && wnd[i]->hwnd) ShowWindow(wnd[i]->hwnd, SW_HIDE);
    DestroyWindow(mainWnd->hwnd);
}

// ---------------------------------------------------------------------------
void App::loadSettings() {
    const wchar_t* M = L"Main";
    UINT dpi = GetDpiForSystem();
    int defZoom = dpi >= 288 ? 300 : dpi >= 192 ? 200 : dpi >= 144 ? 150 : 100;
    zoom = cfg.num(M, L"Zoom", 0);
    if (zoom <= 0) zoom = cfg.num(M, L"Scale", 0) * 100;  // older settings
    if (zoom <= 0) zoom = defZoom;
    if (zoom % 50 != 0) zoom = 100;
    zoom = Clamp(zoom, 100, 400);
    shuffle = cfg.flag(M, L"Shuffle", false);
    repeat = cfg.flag(M, L"Repeat", false);
    timeRemaining = cfg.flag(M, L"TimeRemaining", false);
    alwaysOnTop = cfg.flag(M, L"AlwaysOnTop", false);
    eqAuto = cfg.flag(M, L"EqAuto", false);
    visMode = Clamp(cfg.num(M, L"VisMode", VIS_SPECTRUM), 0, VIS_COUNT - 1);
    mainShade = cfg.flag(M, L"Shade", false);
    plTilesW = Clamp(cfg.num(M, L"PlTilesW", 0), 0, 60);
    plTilesH = Clamp(cfg.num(M, L"PlTilesH", 2), 0, 60);
    skinPath = cfg.str(M, L"Skin", L"");
    for (int i = 0; i < W_COUNT; i++)
        visible[i] = i == W_MAIN || cfg.flag(M, Fmt(L"Visible%d", i), i != W_DSP && i != W_VIS);

    const wchar_t* E = L"Equalizer";
    dsp.eqOn = cfg.flag(E, L"On", true);
    dsp.preamp = Clamp(cfg.flt(E, L"Preamp", 0), -12.0f, 12.0f);
    for (int i = 0; i < kEqBands; i++) dsp.bands[i] = Clamp(cfg.flt(E, Fmt(L"Band%d", i), 0), -12.0f, 12.0f);
    dsp.eqMode = Clamp(cfg.num(E, L"Mode", 0), 0, 1);
    dsp.eqQ = Clamp(cfg.flt(E, L"BandQ", 0.9f), 0.5f, 4.0f);

    const wchar_t* D = L"Dsp";
    dsp.bassOn = cfg.flag(D, L"BassOn", true);
    dsp.bassBoost = Clamp(cfg.flt(D, L"Boost", 0), 0.0f, 18.0f);
    dsp.bassFreq = Clamp(cfg.flt(D, L"Freq", 80), 30.0f, 250.0f);
    dsp.subBoost = Clamp(cfg.flt(D, L"Sub", 0), 0.0f, 12.0f);
    dsp.harmonics = Clamp(cfg.flt(D, L"Harmonics", 0), 0.0f, 1.0f);
    dsp.width = Clamp(cfg.flt(D, L"Width", 1), 0.0f, 2.0f);
    dsp.loudness = cfg.flag(D, L"Loudness", false);
    dsp.limiter = cfg.flag(D, L"Limiter", true);
    dsp.volume = Clamp(cfg.flt(D, L"Volume", 0.8f), 0.0f, 1.0f);
    dsp.balance = Clamp(cfg.flt(D, L"Balance", 0), -1.0f, 1.0f);
}

void App::saveSettings() {
    const wchar_t* M = L"Main";
    cfg.set(M, L"Zoom", zoom);
    cfg.set(M, L"PosZoom", zoom);
    cfg.remove(M, L"Scale");
    cfg.setFlag(M, L"Shuffle", shuffle);
    cfg.setFlag(M, L"Repeat", repeat);
    cfg.setFlag(M, L"TimeRemaining", timeRemaining);
    cfg.setFlag(M, L"AlwaysOnTop", alwaysOnTop);
    cfg.setFlag(M, L"EqAuto", eqAuto);
    cfg.set(M, L"VisMode", visMode);
    cfg.setFlag(M, L"Shade", mainShade);
    cfg.set(M, L"PlTilesW", plTilesW);
    cfg.set(M, L"PlTilesH", plTilesH);
    cfg.set(M, L"Skin", skinPath);
    cfg.set(M, L"Current", pl.current);
    for (int i = 0; i < W_COUNT; i++) {
        cfg.setFlag(M, Fmt(L"Visible%d", i), visible[i]);
        if (wnd[i] && wnd[i]->hwnd) {
            RECT r = wnd[i]->screenRect();
            cfg.set(M, Fmt(L"X%d", i), (int)r.left);
            cfg.set(M, Fmt(L"Y%d", i), (int)r.top);
        }
    }
    const wchar_t* E = L"Equalizer";
    cfg.setFlag(E, L"On", dsp.eqOn);
    cfg.set(E, L"Preamp", dsp.preamp);
    for (int i = 0; i < kEqBands; i++) cfg.set(E, Fmt(L"Band%d", i), dsp.bands[i]);
    cfg.set(E, L"Mode", dsp.eqMode);
    cfg.set(E, L"BandQ", dsp.eqQ);
    const wchar_t* D = L"Dsp";
    cfg.setFlag(D, L"BassOn", dsp.bassOn);
    cfg.set(D, L"Boost", dsp.bassBoost);
    cfg.set(D, L"Freq", dsp.bassFreq);
    cfg.set(D, L"Sub", dsp.subBoost);
    cfg.set(D, L"Harmonics", dsp.harmonics);
    cfg.set(D, L"Width", dsp.width);
    cfg.setFlag(D, L"Loudness", dsp.loudness);
    cfg.setFlag(D, L"Limiter", dsp.limiter);
    cfg.set(D, L"Volume", dsp.volume);
    cfg.set(D, L"Balance", dsp.balance);
}

void App::createWindows() {
    mainWnd = new MainWnd();
    eqWnd = new EqWnd();
    plWnd = new PlWnd();
    dspWnd = new DspWnd();
    visWnd = new VisWnd();
    wnd[W_VIS] = visWnd;
    visWnd->loadState();
    wnd[W_MAIN] = mainWnd;
    wnd[W_EQ] = eqWnd;
    wnd[W_PL] = plWnd;
    wnd[W_DSP] = dspWnd;
    mainWnd->lw = 275;
    mainWnd->lh = mainShade ? 14 : 116;
    eqWnd->lw = dspWnd->lw = 275;
    eqWnd->lh = dspWnd->lh = 116;
    plWnd->lw = 275 + 25 * plTilesW;
    plWnd->lh = 116 + 29 * plTilesH;

    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int defX = wa.left + (wa.right - wa.left - phys(275)) / 3, defY = wa.top + 60;
    int defPos[W_COUNT][2] = {{defX, defY},
                              {defX, defY + phys(116)},
                              {defX, defY + phys(232)},
                              {defX + phys(275), defY + phys(116)},
                              {defX + phys(275), defY}};
    int pos[W_COUNT][2];
    bool valid = true;
    for (int i = 0; i < W_COUNT; i++) {
        pos[i][0] = cfg.num(L"Main", Fmt(L"X%d", i), INT_MIN);
        pos[i][1] = cfg.num(L"Main", Fmt(L"Y%d", i), INT_MIN);
        if (pos[i][0] == INT_MIN || pos[i][1] == INT_MIN) {
            pos[i][0] = defPos[i][0];
            pos[i][1] = defPos[i][1];
        }
    }
    // positions saved at another zoom: keep the windows docked by scaling their offsets
    int posZoom = cfg.num(L"Main", L"PosZoom", zoom);
    if (posZoom != zoom && posZoom > 0)
        for (int i = 1; i < W_COUNT; i++) {
            pos[i][0] = pos[0][0] + (pos[i][0] - pos[0][0]) * zoom / posZoom;
            pos[i][1] = pos[0][1] + (pos[i][1] - pos[0][1]) * zoom / posZoom;
        }
    // reset everything if the main window would be off-screen (monitor removed)
    RECT mr = {pos[0][0], pos[0][1], pos[0][0] + phys(275), pos[0][1] + phys(14)};
    if (!MonitorFromRect(&mr, MONITOR_DEFAULTTONULL)) valid = false;
    if (!valid)
        for (int i = 0; i < W_COUNT; i++) {
            pos[i][0] = defPos[i][0];
            pos[i][1] = defPos[i][1];
        }

    mainWnd->create(nullptr, pos[0][0], pos[0][1], APP_NAME);
    eqWnd->create(mainWnd->hwnd, pos[1][0], pos[1][1], L"RetroAmp Equalizer");
    plWnd->create(mainWnd->hwnd, pos[2][0], pos[2][1], L"RetroAmp Playlist");
    dspWnd->create(mainWnd->hwnd, pos[3][0], pos[3][1], L"RetroAmp Bass & DSP");
    visWnd->create(mainWnd->hwnd, pos[4][0], pos[4][1], L"RetroAmp Visualization");
    ShowWindow(mainWnd->hwnd, SW_SHOW);
    for (int i = 1; i < W_COUNT; i++)
        if (visible[i]) ShowWindow(wnd[i]->hwnd, SW_SHOWNOACTIVATE);
    if (alwaysOnTop) setAlwaysOnTop(true);
}

// ---------------------------------------------------------------------------
// Playback
const Track* App::currentTrack() const {
    return pl.current >= 0 && pl.current < pl.size() ? &pl.tracks[pl.current] : nullptr;
}

double App::duration() const {
    double d = player.duration();
    if (d > 0) return d;
    const Track* t = currentTrack();
    return t && t->length > 0 ? t->length : 0;
}

void App::playIndex(int idx, double startPos, bool paused) {
    if (idx < 0 || idx >= pl.size()) return;
    pl.current = idx;
    const Track& t = pl.tracks[idx];
    session_ = player.play(t.path, startPos, paused);
    if (eqAuto) loadAutoEq();
    mainWnd->resetMarquee();
    plWnd->ensureVisible(idx);
    requestMeta(idx);
    updateTitle();
    redrawAll();
}

void App::playPressed() {
    if (state() == PlayState::Paused) {
        player.setPaused(false);
    } else if (pl.size() == 0) {
        openFilesDialog(false);
        return;
    } else {
        int idx = pl.current;
        if (idx < 0 || idx >= pl.size()) idx = pl.firstSelected() >= 0 ? pl.firstSelected() : 0;
        playIndex(idx);
    }
    redrawAll();
}

void App::pausePressed() {
    if (state() == PlayState::Playing) player.setPaused(true);
    else if (state() == PlayState::Paused) player.setPaused(false);
    redrawAll();
}

void App::stopPressed() {
    player.stop();
    updateTitle();
    redrawAll();
}

void App::nextTrack() {
    int idx = pl.nextIndex(shuffle, true);
    if (idx < 0) return;
    if (state() == PlayState::Stopped) {
        pl.current = idx;
        plWnd->ensureVisible(idx);
        mainWnd->resetMarquee();
        updateTitle();
        redrawAll();
    } else {
        playIndex(idx);
    }
}

void App::prevTrack() {
    if (state() != PlayState::Stopped && position() > 5.0 && !shuffle) {
        // like most players: first press restarts the song
        seekTo(0);
        return;
    }
    int idx = pl.prevIndex(shuffle, true);
    if (idx < 0) return;
    if (state() == PlayState::Stopped) {
        pl.current = idx;
        plWnd->ensureVisible(idx);
        mainWnd->resetMarquee();
        updateTitle();
        redrawAll();
    } else {
        playIndex(idx);
    }
}

void App::seekTo(double sec) {
    if (state() == PlayState::Stopped) return;
    player.seek(sec);
    redraw(W_MAIN);
}

void App::seekBy(double delta) {
    if (state() == PlayState::Stopped) return;
    seekTo(std::max(0.0, position() + delta));
}

void App::setVolume(float v) {
    dsp.volume = Clamp(v, 0.0f, 1.0f);
    player.setParams(dsp);
    setMarqueeOverride(Fmt(L"VOLUME: %d%%", (int)std::lround(dsp.volume * 100)));
    redraw(W_MAIN);
}

void App::setBalance(float b) {
    dsp.balance = Clamp(b, -1.0f, 1.0f);
    player.setParams(dsp);
    int pct = (int)std::lround(std::fabs(dsp.balance) * 100);
    setMarqueeOverride(pct == 0 ? std::wstring(L"BALANCE: CENTER")
                                : Fmt(L"BALANCE: %d%% %s", pct, dsp.balance < 0 ? L"LEFT" : L"RIGHT"));
    redraw(W_MAIN);
}

void App::toggleShuffle() {
    shuffle = !shuffle;
    pl.resetShuffle();
    redraw(W_MAIN);
    saveSettings();
}

void App::toggleRepeat() {
    repeat = !repeat;
    redraw(W_MAIN);
    saveSettings();
}

void App::setMarqueeOverride(const std::wstring& text) {
    marquee_ = text;
    marqueeUntil_ = text.empty() ? 0 : GetTickCount64() + 1500;
    redraw(W_MAIN);
}

std::wstring App::marqueeOverride() const {
    if (marquee_.empty() || GetTickCount64() > marqueeUntil_) return L"";
    return marquee_;
}

std::wstring App::currentTitleLine() const {
    const Track* t = currentTrack();
    if (!t) return std::wstring(APP_NAME) + L" " + APP_VERSION + L" - DROP MUSIC HERE";
    double len = t->length >= 0 ? t->length : player.duration();
    std::wstring s = Fmt(L"%d. ", pl.current + 1) + t->display();
    if (len > 0) s += L" (" + FormatTime(len) + L")";
    return s;
}

void App::updateTitle() {
    const Track* t = currentTrack();
    std::wstring title = t ? Fmt(L"%d. ", pl.current + 1) + t->display() + L" - " APP_NAME : std::wstring(APP_NAME);
    if (mainWnd && mainWnd->hwnd) SetWindowTextW(mainWnd->hwnd, title.c_str());
}

// ---------------------------------------------------------------------------
// Files
void App::requestMeta(int idx) {
    if (idx < 0 || idx >= pl.size()) return;
    Track& t = pl.tracks[idx];
    if (t.metaDone || IsUrl(t.path)) return;
    t.metaDone = true;  // requested
    meta.request(t.id, t.path);
}

void App::addPaths(const std::vector<std::wstring>& paths, int insertAt, bool clearFirst, bool playFirst) {
    if (clearFirst) {
        player.stop();
        meta.cancelAll();
        pl.clear();
        insertAt = -1;
    }
    int pos = insertAt < 0 || insertAt > pl.size() ? pl.size() : insertAt;
    int first = pos;
    int before = pl.size();
    for (auto& p : paths) {
        if (DirExists(p)) {
            std::vector<std::wstring> files;
            CollectAudioFiles(p, files);
            for (auto& f : files) pl.add(f, L"", -1, pos++);
        } else if (Playlist::IsPlaylistFile(p)) {
            int n0 = pl.size();
            pl.loadFile(p, pos);
            pos += pl.size() - n0;
        } else {
            pl.add(p, L"", -1, pos++);
        }
    }
    int added = pl.size() - before;
    for (int i = first; i < first + added; i++) {
        if (pl.tracks[i].length >= 0 && !pl.tracks[i].title.empty()) pl.tracks[i].metaDone = true;
        requestMeta(i);
    }
    if (playFirst && added > 0) playIndex(first);
    savePlaylist();
    plWnd->clampScroll();
    redrawAll();
}

void App::onDropFiles(const std::vector<std::wstring>& files, int insertAt, bool fromPlaylist) {
    if (files.empty()) return;
    // a dropped skin is applied instead of being added to the playlist
    if (files.size() == 1) {
        std::wstring ext = PathExt(files[0]);
        if (ext == L"wsz" || (ext == L"zip" && !fromPlaylist)) {
            std::wstring dst = PathJoin(userSkinDir(), PathFileName(files[0]));
            if (ToLower(dst) != ToLower(files[0])) CopyFileW(files[0].c_str(), dst.c_str(), FALSE);
            loadSkin(FileExists(dst) ? dst : files[0], true);
            return;
        }
        if (ext == L"eqf" || ext == L"q1") {
            std::vector<EqPreset> ps;
            if (LoadEqf(files[0], ps) && !ps.empty()) applyEqPreset(ps[0]);
            return;
        }
    }
    bool wasEmpty = pl.size() == 0;
    addPaths(files, fromPlaylist ? insertAt : -1, false, !fromPlaylist || (wasEmpty && state() == PlayState::Stopped));
}

static std::vector<std::wstring> OpenFilesDlg(HWND owner, const wchar_t* filter, bool multi, const wchar_t* title) {
    std::vector<wchar_t> buf(1 << 16, 0);
    OPENFILENAMEW ofn = {sizeof(ofn)};
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf.data();
    ofn.nMaxFile = (DWORD)buf.size();
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | (multi ? OFN_ALLOWMULTISELECT : 0);
    std::vector<std::wstring> out;
    if (!GetOpenFileNameW(&ofn)) return out;
    std::wstring dir = buf.data();
    const wchar_t* p = buf.data() + dir.size() + 1;
    if (!*p) {
        out.push_back(dir);
    } else {
        while (*p) {
            out.push_back(PathJoin(dir, p));
            p += wcslen(p) + 1;
        }
    }
    return out;
}

void App::openFilesDialog(bool add) {
    auto files = OpenFilesDlg(mainWnd->hwnd, AudioFileFilter(), true, add ? L"Add files to playlist" : L"Play files");
    if (files.empty()) return;
    std::sort(files.begin(), files.end(),
              [](const std::wstring& a, const std::wstring& b) { return StrCmpLogicalW(a.c_str(), b.c_str()) < 0; });
    addPaths(files, -1, !add, !add || state() == PlayState::Stopped && pl.size() == 0);
}

void App::openFolderDialog(bool add) {
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg)))) return;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dlg->SetTitle(add ? L"Add folder to playlist" : L"Play folder");
    std::wstring folder;
    if (SUCCEEDED(dlg->Show(mainWnd->hwnd))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p))) {
                folder = p;
                CoTaskMemFree(p);
            }
            item->Release();
        }
    }
    dlg->Release();
    if (!folder.empty()) addPaths({folder}, -1, !add, !add);
}

void App::addUrlDialog() {
    std::wstring url = L"http://";
    if (!InputBox(mainWnd->hwnd, L"Open URL", L"Enter a stream or file URL (http/https, Icecast/Shoutcast...):", url))
        return;
    if (url == L"http://") return;
    addPaths({url}, -1, false, true);
}

void App::loadPlaylistDialog() {
    static const wchar_t filter[] = L"Playlists (*.m3u;*.m3u8;*.pls)\0*.m3u;*.m3u8;*.pls\0All files (*.*)\0*.*\0";
    auto files = OpenFilesDlg(mainWnd->hwnd, filter, false, L"Load playlist");
    if (files.empty()) return;
    addPaths(files, -1, true, false);
}

void App::savePlaylistDialog() {
    wchar_t buf[MAX_PATH * 2] = L"playlist.m3u8";
    OPENFILENAMEW ofn = {sizeof(ofn)};
    ofn.hwndOwner = mainWnd->hwnd;
    ofn.lpstrFilter = L"M3U8 playlist (UTF-8)\0*.m3u8\0M3U playlist\0*.m3u\0PLS playlist\0*.pls\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = (DWORD)std::size(buf);
    ofn.lpstrDefExt = L"m3u8";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER;
    if (!GetSaveFileNameW(&ofn)) return;
    std::wstring path = buf;
    bool ok = PathExt(path) == L"pls" ? pl.savePLS(path) : pl.saveM3U(path);
    if (!ok) MessageBoxW(mainWnd->hwnd, L"Could not save the playlist.", APP_NAME, MB_ICONERROR);
}

void App::showFileInfo(int idx) {
    if (idx < 0 || idx >= pl.size()) return;
    const Track& t = pl.tracks[idx];
    std::wstring s = L"Title:\t" + t.display() + L"\nFile:\t" + t.path;
    s += L"\nLength:\t" + (t.length >= 0 ? FormatTime(t.length) : std::wstring(L"unknown"));
    if (idx == pl.current && state() != PlayState::Stopped) {
        s += Fmt(L"\nFormat:\t%d Hz, %d ch, %d kbps", player.sampleRate(), player.channels(), player.bitrate());
        s += L"\nDecoder:\t" + player.backend();
    } else if (t.sampleRate) {
        s += Fmt(L"\nFormat:\t%d Hz, %d ch, %d kbps", t.sampleRate, t.channels, t.bitrate);
    }
    if (!IsUrl(t.path)) {
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExW(t.path.c_str(), GetFileExInfoStandard, &fa))
            s += Fmt(L"\nSize:\t%.2f MB", ((double)fa.nFileSizeHigh * 4294967296.0 + fa.nFileSizeLow) / 1048576.0);
    }
    MessageBoxW(mainWnd->hwnd, s.c_str(), L"File info", MB_ICONINFORMATION);
}

void App::jumpToFileDialog() {
    JumpData d;
    if (DialogBoxParamW(inst, MAKEINTRESOURCEW(IDD_JUMP), mainWnd->hwnd, JumpDlgProc, (LPARAM)&d) == IDOK &&
        d.result >= 0) {
        pl.selectAll(false);
        pl.tracks[d.result].selected = true;
        playIndex(d.result);
    }
}

// ---------------------------------------------------------------------------
// Windows
void App::redraw(int w) {
    if (w >= 0 && w < W_COUNT && wnd[w]) wnd[w]->redraw();
}

void App::redrawAll() {
    for (auto w : wnd)
        if (w) w->redraw();
}

void App::toggleWindow(int w) { setWindowVisible(w, !visible[w]); }

void App::setWindowVisible(int w, bool v) {
    if (w == W_MAIN) return;
    if (w == W_DSP && v != visible[W_DSP]) {
        // The bass panel docks directly under the equalizer (or the main window when the EQ
        // is hidden); windows stacked below it move down / back up to make room.
        RECT anchor = visible[W_EQ] ? eqWnd->screenRect() : mainWnd->screenRect();
        RECT dr = dspWnd->screenRect();
        int h = phys(dspWnd->lh);
        int edge = v ? anchor.bottom : dr.bottom;
        int left = v ? anchor.left : dr.left, right = v ? anchor.right : dr.right;
        for (int i = 0; i < W_COUNT; i++) {
            if (i == W_DSP || i == W_MAIN || !visible[i]) continue;
            RECT r = wnd[i]->screenRect();
            if (v && i == W_EQ) continue;
            if (r.top >= edge - 1 && r.left < right && r.right > left) wnd[i]->moveTo(r.left, r.top + (v ? h : -h));
        }
        if (v) dspWnd->moveTo(anchor.left, anchor.bottom);
    }
    if (w == W_VIS && v) {
        RECT vr = visWnd->screenRect();
        if (!MonitorFromRect(&vr, MONITOR_DEFAULTTONULL)) {
            RECT mr = mainWnd->screenRect();
            visWnd->moveTo(mr.right, mr.top);
        }
    }
    if (w == W_VIS && !v) visWnd->exitFullscreen();
    visible[w] = v;
    ShowWindow(wnd[w]->hwnd, v ? SW_SHOWNOACTIVATE : SW_HIDE);
    if (v && alwaysOnTop) SetWindowPos(wnd[w]->hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    redraw(W_MAIN);
    saveSettings();
}

void App::setZoom(int s) {
    s = Clamp(s, 100, 400);
    if (s == zoom) return;
    int old = zoom;
    RECT mr = mainWnd->screenRect();
    RECT rects[W_COUNT];
    for (int i = 0; i < W_COUNT; i++) rects[i] = wnd[i]->screenRect();
    zoom = s;
    skin.prepare(renderScale());
    for (int i = 0; i < W_COUNT; i++) {
        if (i != W_MAIN) {
            int x = mr.left + (rects[i].left - mr.left) * s / old;
            int y = mr.top + (rects[i].top - mr.top) * s / old;
            wnd[i]->moveTo(x, y);
        }
        wnd[i]->applyScale();
    }
    saveSettings();
}

void App::setAlwaysOnTop(bool on) {
    alwaysOnTop = on;
    for (auto w : wnd)
        if (w && w->hwnd)
            SetWindowPos(w->hwnd, on ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    redraw(W_MAIN);
    saveSettings();
}

void App::setMainShade(bool on) {
    if (on == mainShade) return;
    RECT before = mainWnd->screenRect();
    // windows docked below the main window follow it up/down
    std::vector<int> group;
    {
        std::vector<int> stack = {W_MAIN};
        bool seen[W_COUNT] = {true};
        while (!stack.empty()) {
            int a = stack.back();
            stack.pop_back();
            RECT ra = wnd[a]->screenRect();
            for (int b = 1; b < W_COUNT; b++) {
                if (seen[b] || !visible[b]) continue;
                if (RectsAdjacent(ra, wnd[b]->screenRect())) {
                    seen[b] = true;
                    stack.push_back(b);
                    group.push_back(b);
                }
            }
        }
    }
    mainShade = on;
    mainWnd->setLogicalSize(275, on ? 14 : 116);
    int dy = phys(on ? 14 : 116) - (before.bottom - before.top);
    for (int b : group) {
        RECT r = wnd[b]->screenRect();
        if (r.top >= before.bottom - 1) wnd[b]->moveTo(r.left, r.top + dy);
    }
    saveSettings();
}

void App::setPlaylistSize(int tw, int th) {
    plTilesW = Clamp(tw, 0, 60);
    plTilesH = Clamp(th, 0, 60);
    plWnd->setLogicalSize(275 + 25 * plTilesW, 116 + 29 * plTilesH);
    plWnd->clampScroll();
}

// ---------------------------------------------------------------------------
// Docking: dragging the main window moves every window attached to it;
// all windows snap to each other and to the screen edges.
void App::beginDrag(SkinWnd* w, POINT cursor) {
    dragWnd_ = w;
    dragStart_ = cursor;
    dragGroup_.clear();
    for (int i = 0; i < W_COUNT; i++) dragRects_[i] = wnd[i]->screenRect();
    if (w->id == W_MAIN) {
        bool seen[W_COUNT] = {};
        std::vector<int> stack = {W_MAIN};
        seen[W_MAIN] = true;
        while (!stack.empty()) {
            int a = stack.back();
            stack.pop_back();
            dragGroup_.push_back(a);
            for (int b = 0; b < W_COUNT; b++) {
                if (seen[b] || !visible[b]) continue;
                if (RectsAdjacent(dragRects_[a], dragRects_[b])) {
                    seen[b] = true;
                    stack.push_back(b);
                }
            }
        }
    } else {
        dragGroup_.push_back(w->id);
    }
}

bool App::snapRect(const RECT& r, const std::vector<int>& exclude, int& sx, int& sy) const {
    const int snap = 10;
    int bestX = snap + 1, bestY = snap + 1;
    auto consider = [&](int d, int& best, int& out) {
        if (std::abs(d) < std::abs(best)) {
            best = d;
            out = d;
        }
    };
    auto test = [&](const RECT& t, bool screen) {
        bool vOverlap = screen || (r.top < t.bottom + snap && r.bottom > t.top - snap);
        bool hOverlap = screen || (r.left < t.right + snap && r.right > t.left - snap);
        if (vOverlap) {
            if (!screen) {
                consider(t.left - r.right, bestX, sx);
                consider(t.right - r.left, bestX, sx);
            }
            consider(t.left - r.left, bestX, sx);
            consider(t.right - r.right, bestX, sx);
        }
        if (hOverlap) {
            if (!screen) {
                consider(t.top - r.bottom, bestY, sy);
                consider(t.bottom - r.top, bestY, sy);
            }
            consider(t.top - r.top, bestY, sy);
            consider(t.bottom - r.bottom, bestY, sy);
        }
    };
    for (int i = 0; i < W_COUNT; i++) {
        if (!visible[i] || std::find(exclude.begin(), exclude.end(), i) != exclude.end()) continue;
        RECT t = wnd[i]->screenRect();
        test(t, false);
    }
    MONITORINFO mi = {sizeof(mi)};
    if (GetMonitorInfoW(MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST), &mi)) test(mi.rcWork, true);
    if (bestX > snap) sx = 0;
    if (bestY > snap) sy = 0;
    return bestX <= snap || bestY <= snap;
}

void App::dragTo(POINT cursor) {
    if (!dragWnd_) return;
    int dx = cursor.x - dragStart_.x, dy = cursor.y - dragStart_.y;
    int bestSx = 0, bestSy = 0, bestAx = INT_MAX, bestAy = INT_MAX;
    for (int id : dragGroup_) {
        RECT r = dragRects_[id];
        OffsetR(r, dx, dy);
        int sx = 0, sy = 0;
        snapRect(r, dragGroup_, sx, sy);
        if (sx && std::abs(sx) < bestAx) {
            bestAx = std::abs(sx);
            bestSx = sx;
        }
        if (sy && std::abs(sy) < bestAy) {
            bestAy = std::abs(sy);
            bestSy = sy;
        }
    }
    HDWP dwp = BeginDeferWindowPos((int)dragGroup_.size());
    for (int id : dragGroup_) {
        RECT r = dragRects_[id];
        dwp = DeferWindowPos(dwp, wnd[id]->hwnd, nullptr, r.left + dx + bestSx, r.top + dy + bestSy, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    EndDeferWindowPos(dwp);
}

void App::endDrag() {
    dragWnd_ = nullptr;
    dragGroup_.clear();
    saveSettings();
}

// ---------------------------------------------------------------------------
// Skins
std::wstring App::userSkinDir() const {
    std::wstring d = PathJoin(dataDir_, L"Skins");
    CreateDirectoryW(d.c_str(), nullptr);
    return d;
}

std::vector<std::wstring> App::listSkins() const {
    std::vector<std::wstring> out;
    std::vector<std::wstring> names;
    for (const std::wstring& dir : {PathJoin(GetExeDir(), L"Skins"), userSkinDir()}) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(PathJoin(dir, L"*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            std::wstring n = fd.cFileName;
            if (n == L"." || n == L"..") continue;
            std::wstring full = PathJoin(dir, n);
            bool ok = false;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                ok = FileExists(PathJoin(full, L"main.bmp"));
            else
                ok = PathExt(n) == L"wsz" || PathExt(n) == L"zip";
            if (!ok) continue;
            std::wstring key = ToLower(PathStem(n));
            if (std::find(names.begin(), names.end(), key) != names.end()) continue;
            names.push_back(key);
            out.push_back(full);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::sort(out.begin(), out.end(), [](const std::wstring& a, const std::wstring& b) {
        return StrCmpLogicalW(PathStem(a).c_str(), PathStem(b).c_str()) < 0;
    });
    return out;
}

bool App::loadSkin(const std::wstring& path, bool showErrors) {
    Skin ns;
    if (path.empty()) {
        ns.loadDefault(inst);
    } else {
        std::wstring err;
        if (!ns.loadFrom(path, err)) {
            if (showErrors)
                MessageBoxW(mainWnd ? mainWnd->hwnd : nullptr, (L"Could not load skin:\n" + path + L"\n\n" + err).c_str(),
                            APP_NAME, MB_ICONERROR);
            return false;
        }
    }
    skin = std::move(ns);
    skin.prepare(renderScale());
    skinPath = path;
    for (auto w : wnd)
        if (w && w->hwnd) {
            w->updateRegion();
            w->redraw();
        }
    if (mainWnd) saveSettings();
    return true;
}

void App::loadSkinDialog() {
    static const wchar_t filter[] = L"Winamp classic skins (*.wsz;*.zip)\0*.wsz;*.zip\0All files (*.*)\0*.*\0";
    auto files = OpenFilesDlg(mainWnd->hwnd, filter, false, L"Load skin");
    if (files.empty()) return;
    std::wstring dst = PathJoin(userSkinDir(), PathFileName(files[0]));
    if (ToLower(dst) != ToLower(files[0])) CopyFileW(files[0].c_str(), dst.c_str(), FALSE);
    loadSkin(FileExists(dst) ? dst : files[0], true);
}

// ---------------------------------------------------------------------------
// Equalizer / DSP
void App::applyDsp(bool saveAuto) {
    player.setParams(dsp);
    if (saveAuto && eqAuto) saveAutoEq();
    redraw(W_EQ);
    redraw(W_DSP);
}

void App::setEqBand(int band, float db) {
    if (band < 0 || band >= kEqBands) return;
    dsp.bands[band] = Clamp(db, -12.0f, 12.0f);
    applyDsp(false);
}

void App::setPreamp(float db) {
    dsp.preamp = Clamp(db, -12.0f, 12.0f);
    applyDsp(false);
}

void App::applyEqPreset(const EqPreset& p) {
    memcpy(dsp.bands, p.bands, sizeof(dsp.bands));
    dsp.preamp = p.preamp;
    dsp.eqOn = true;
    applyDsp(true);
    saveSettings();
    setMarqueeOverride(L"EQ PRESET: " + p.name);
}

std::vector<EqPreset> App::userPresets() const {
    Ini ini(PathJoin(dataDir_, L"eqpresets.ini"));
    std::vector<EqPreset> out;
    for (auto& k : ini.keys(L"Presets")) {
        EqPreset p;
        p.name = k;
        if (ParseEq(ini.str(L"Presets", k), p.bands, p.preamp)) out.push_back(p);
    }
    return out;
}

void App::loadAutoEq() {
    const Track* t = currentTrack();
    if (!t) return;
    Ini ini(PathJoin(dataDir_, L"autoeq.ini"));
    std::wstring v = ini.str(L"Tracks", Fmt(L"%016llx", HashString(ToLower(t->path))));
    if (v.empty()) return;
    if (ParseEq(v, dsp.bands, dsp.preamp)) applyDsp(false);
}

void App::saveAutoEq() {
    const Track* t = currentTrack();
    if (!t) return;
    Ini ini(PathJoin(dataDir_, L"autoeq.ini"));
    ini.set(L"Tracks", Fmt(L"%016llx", HashString(ToLower(t->path))), SerializeEq(dsp.bands, dsp.preamp));
}

void App::showEqPresetsMenu(HWND owner, POINT pt) {
    const auto& builtin = BuiltinEqPresets();
    auto user = userPresets();
    HMENU load = CreatePopupMenu();
    for (size_t i = 0; i < builtin.size(); i++)
        AppendMenuW(load, MF_STRING, CMD_EQ_PRESET_BASE + i, builtin[i].name.c_str());
    if (!user.empty()) {
        AppendMenuW(load, MF_SEPARATOR, 0, nullptr);
        for (size_t i = 0; i < user.size() && i < 250; i++)
            AppendMenuW(load, MF_STRING, CMD_EQ_USER_BASE + i, user[i].name.c_str());
    }
    HMENU del = CreatePopupMenu();
    for (size_t i = 0; i < user.size() && i < 190; i++)
        AppendMenuW(del, MF_STRING, CMD_EQ_DELETE_BASE + i, user[i].name.c_str());
    HMENU mode = CreatePopupMenu();
    AppendMenuW(mode, MF_STRING | (dsp.eqMode == 0 ? MF_CHECKED : 0), CMD_EQ_MODE_WINAMP, L"Winamp style (60 Hz - 16 kHz)");
    AppendMenuW(mode, MF_STRING | (dsp.eqMode == 1 ? MF_CHECKED : 0), CMD_EQ_MODE_ISO, L"ISO octaves (31 Hz - 16 kHz)");
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_POPUP, (UINT_PTR)load, L"Load preset");
    AppendMenuW(m, MF_STRING, CMD_EQ_SAVE, L"Save preset...");
    AppendMenuW(m, MF_POPUP | (user.empty() ? MF_GRAYED : 0), (UINT_PTR)del, L"Delete preset");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, CMD_EQ_IMPORT, L"Import Winamp EQ file (.eqf/.q1)...");
    AppendMenuW(m, MF_STRING, CMD_EQ_EXPORT, L"Export current as .eqf...");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)mode, L"Frequency bands");
    AppendMenuW(m, MF_STRING, CMD_EQ_RESET, L"Reset to flat");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (visible[W_DSP] ? MF_CHECKED : 0), CMD_WND_DSP, L"Bass && DSP panel\tAlt+B");
    SetForegroundWindow(owner);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, owner, nullptr);
    DestroyMenu(m);
    if (cmd) handleCommand(cmd);
}

void App::showDspPresetsMenu(HWND owner, POINT pt) {
    HMENU m = CreatePopupMenu();
    for (size_t i = 0; i < std::size(kBassPresets); i++)
        AppendMenuW(m, MF_STRING, CMD_BASS_PRESET_BASE + i, kBassPresets[i].name);
    SetForegroundWindow(owner);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, owner, nullptr);
    DestroyMenu(m);
    if (cmd) handleCommand(cmd);
}

void App::applyBassPreset(int idx) {
    if (idx < 0 || idx >= (int)std::size(kBassPresets)) return;
    const BassPreset& p = kBassPresets[idx];
    dsp.bassOn = true;
    dsp.bassBoost = p.boost;
    dsp.bassFreq = p.freq;
    dsp.subBoost = p.sub;
    dsp.harmonics = p.harm;
    dsp.width = p.width;
    dsp.loudness = p.loud;
    applyDsp(true);
    saveSettings();
    setMarqueeOverride(std::wstring(L"BASS PRESET: ") + p.name);
}

// ---------------------------------------------------------------------------
// Menus
void App::showMainMenu(HWND owner, POINT pt) {
    HMENU wins = CreatePopupMenu();
    AppendMenuW(wins, MF_STRING | MF_CHECKED | MF_GRAYED, CMD_WND_MAIN, L"Main window");
    AppendMenuW(wins, MF_STRING | (visible[W_EQ] ? MF_CHECKED : 0), CMD_WND_EQ, L"Equalizer\tAlt+G");
    AppendMenuW(wins, MF_STRING | (visible[W_PL] ? MF_CHECKED : 0), CMD_WND_PL, L"Playlist editor\tAlt+E");
    AppendMenuW(wins, MF_STRING | (visible[W_DSP] ? MF_CHECKED : 0), CMD_WND_DSP, L"Bass && DSP\tAlt+B");
    AppendMenuW(wins, MF_STRING | (visible[W_VIS] ? MF_CHECKED : 0), CMD_WND_VIS, L"Visualization\tAlt+V");

    HMENU skins = CreatePopupMenu();
    AppendMenuW(skins, MF_STRING | (skinPath.empty() ? MF_CHECKED : 0), CMD_SKIN_DEFAULT, L"<Base skin: Retro Blue>");
    auto list = listSkins();
    for (size_t i = 0; i < list.size() && i < 900; i++) {
        bool cur = ToLower(list[i]) == ToLower(skinPath);
        AppendMenuW(skins, MF_STRING | (cur ? MF_CHECKED : 0) | ((i + 1) % 30 == 0 ? MF_MENUBARBREAK : 0),
                    CMD_SKIN_BASE + i, PathStem(list[i]).c_str());
    }
    AppendMenuW(skins, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(skins, MF_STRING, CMD_SKIN_LOAD, L"Load skin (.wsz)...\tAlt+S");
    AppendMenuW(skins, MF_STRING, CMD_SKIN_FOLDER, L"Open skins folder");
    AppendMenuW(skins, MF_STRING, CMD_SKIN_WEB, L"Get more skins (Winamp Skin Museum)...");

    HMENU size = CreatePopupMenu();
    static const int zooms[] = {100, 150, 200, 250, 300, 400};
    for (int i = 0; i < 6; i++)
        AppendMenuW(size, MF_STRING | (zoom == zooms[i] ? MF_CHECKED : 0), CMD_ZOOM100 + i,
                    Fmt(L"%d%%  (x%g)%s", zooms[i], zooms[i] / 100.0, zooms[i] == 200 ? L"\tCtrl+D" : L"").c_str());
    HMENU timeM = CreatePopupMenu();
    AppendMenuW(timeM, MF_STRING | (!timeRemaining ? MF_CHECKED : 0), CMD_TIME_ELAPSED, L"Time elapsed");
    AppendMenuW(timeM, MF_STRING | (timeRemaining ? MF_CHECKED : 0), CMD_TIME_REMAINING, L"Time remaining\tCtrl+T");
    HMENU vis = CreatePopupMenu();
    AppendMenuW(vis, MF_STRING | (visMode == VIS_SPECTRUM ? MF_CHECKED : 0), CMD_VIS_SPECTRUM, L"Spectrum analyzer");
    AppendMenuW(vis, MF_STRING | (visMode == VIS_OSC ? MF_CHECKED : 0), CMD_VIS_OSC, L"Oscilloscope");
    AppendMenuW(vis, MF_STRING | (visMode == VIS_OFF ? MF_CHECKED : 0), CMD_VIS_OFF, L"Off");
    HMENU opts = CreatePopupMenu();
    AppendMenuW(opts, MF_STRING | (alwaysOnTop ? MF_CHECKED : 0), CMD_ONTOP, L"Always on top\tCtrl+A");
    AppendMenuW(opts, MF_STRING | (mainShade ? MF_CHECKED : 0), CMD_SHADE, L"Window shade\tCtrl+W");
    AppendMenuW(opts, MF_POPUP, (UINT_PTR)size, L"Size");
    AppendMenuW(opts, MF_POPUP, (UINT_PTR)timeM, L"Time display");
    AppendMenuW(opts, MF_POPUP, (UINT_PTR)vis, L"Visualization");

    HMENU play = CreatePopupMenu();
    AppendMenuW(play, MF_STRING, CMD_PREV, L"Previous\tZ");
    AppendMenuW(play, MF_STRING, CMD_PLAY, L"Play\tX");
    AppendMenuW(play, MF_STRING, CMD_PAUSE, L"Pause\tC");
    AppendMenuW(play, MF_STRING, CMD_STOP, L"Stop\tV");
    AppendMenuW(play, MF_STRING, CMD_NEXT, L"Next\tB");
    AppendMenuW(play, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(play, MF_STRING, CMD_BACK5, L"Back 5 seconds\tLeft");
    AppendMenuW(play, MF_STRING, CMD_FWD5, L"Forward 5 seconds\tRight");
    AppendMenuW(play, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(play, MF_STRING | (shuffle ? MF_CHECKED : 0), CMD_SHUFFLE, L"Shuffle\tS");
    AppendMenuW(play, MF_STRING | (repeat ? MF_CHECKED : 0), CMD_REPEAT, L"Repeat\tR");
    AppendMenuW(play, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(play, MF_STRING, CMD_JUMP, L"Jump to file...\tJ");
    AppendMenuW(play, MF_STRING, CMD_FILEINFO, L"File info...\tAlt+3");

    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, CMD_ABOUT, L"RetroAmp " APP_VERSION L" - About...");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, CMD_PLAY_FILE, L"Play file...\tL");
    AppendMenuW(m, MF_STRING, CMD_PLAY_FOLDER, L"Play folder...\tShift+L");
    AppendMenuW(m, MF_STRING, CMD_ADD_URL, L"Play URL...\tCtrl+L");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)wins, L"Windows");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)skins, L"Skins");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)opts, L"Options");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)play, L"Playback");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, CMD_EXIT, L"Exit");
    SetForegroundWindow(owner);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, owner, nullptr);
    DestroyMenu(m);
    if (cmd) handleCommand(cmd);
}

void App::showPlaylistMenu(int which, HWND owner, POINT pt) {
    HMENU sort = CreatePopupMenu();
    AppendMenuW(sort, MF_STRING, CMD_PL_SORT_TITLE, L"Sort list by title");
    AppendMenuW(sort, MF_STRING, CMD_PL_SORT_FILE, L"Sort list by file name");
    AppendMenuW(sort, MF_STRING, CMD_PL_SORT_PATH, L"Sort list by path and file name");
    AppendMenuW(sort, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(sort, MF_STRING, CMD_PL_REVERSE, L"Reverse list");
    AppendMenuW(sort, MF_STRING, CMD_PL_RANDOM, L"Randomize list");
    HMENU m = sort;
    if (which == 0) {
        m = CreatePopupMenu();
        bool sel = pl.selectedCount() > 0;
        AppendMenuW(m, MF_STRING | (sel ? 0 : MF_GRAYED), CMD_PL_PLAY_SEL, L"Play item\tEnter");
        AppendMenuW(m, MF_STRING | (sel ? 0 : MF_GRAYED), CMD_FILEINFO, L"File info...");
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING | (sel ? 0 : MF_GRAYED), CMD_PL_REMOVE_SEL, L"Remove selected\tDel");
        AppendMenuW(m, MF_STRING | (sel ? 0 : MF_GRAYED), CMD_PL_CROP, L"Crop selection\tShift+Del");
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, CMD_PL_SELECT_ALL, L"Select all\tCtrl+A");
        AppendMenuW(m, MF_STRING, CMD_PL_SELECT_NONE, L"Select none");
        AppendMenuW(m, MF_STRING, CMD_PL_INVERT, L"Invert selection\tCtrl+I");
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_POPUP, (UINT_PTR)sort, L"Sort");
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, CMD_ADD_FILE, L"Add files...");
        AppendMenuW(m, MF_STRING, CMD_ADD_FOLDER, L"Add folder...");
        AppendMenuW(m, MF_STRING, CMD_ADD_URL, L"Add URL...");
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, CMD_PL_LOAD, L"Load playlist...");
        AppendMenuW(m, MF_STRING, CMD_PL_SAVE, L"Save playlist...");
        AppendMenuW(m, MF_STRING, CMD_PL_NEW, L"New (clear) playlist");
    } else if (which == 2) {
        DestroyMenu(sort);
        m = CreatePopupMenu();
        AppendMenuW(m, MF_STRING, CMD_PL_REMOVE_MISSING, L"Remove missing files");
        AppendMenuW(m, MF_STRING, CMD_PL_CLEAR, L"Clear playlist");
    } else if (which == 3) {
        DestroyMenu(sort);
        m = CreatePopupMenu();
        AppendMenuW(m, MF_STRING, CMD_JUMP, L"Jump to file...\tJ");
        AppendMenuW(m, MF_STRING, CMD_FILEINFO, L"File info...");
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING, CMD_PL_RANDOM, L"Randomize list");
        AppendMenuW(m, MF_STRING, CMD_PL_REVERSE, L"Reverse list");
        AppendMenuW(m, MF_STRING, CMD_PL_REMOVE_MISSING, L"Remove missing files");
    }
    SetForegroundWindow(owner);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, owner, nullptr);
    DestroyMenu(m);
    if (cmd) handleCommand(cmd);
}

void App::handleCommand(int cmd) {
    if (cmd >= CMD_SKIN_BASE && cmd < CMD_EQ_PRESET_BASE) {
        auto list = listSkins();
        size_t i = cmd - CMD_SKIN_BASE;
        if (i < list.size()) loadSkin(list[i], true);
        return;
    }
    if (cmd >= CMD_EQ_PRESET_BASE && cmd < CMD_EQ_USER_BASE) {
        size_t i = cmd - CMD_EQ_PRESET_BASE;
        if (i < BuiltinEqPresets().size()) applyEqPreset(BuiltinEqPresets()[i]);
        return;
    }
    if (cmd >= CMD_EQ_USER_BASE && cmd < CMD_EQ_DELETE_BASE) {
        auto u = userPresets();
        size_t i = cmd - CMD_EQ_USER_BASE;
        if (i < u.size()) applyEqPreset(u[i]);
        return;
    }
    if (cmd >= CMD_EQ_DELETE_BASE && cmd < CMD_BASS_PRESET_BASE) {
        auto u = userPresets();
        size_t i = cmd - CMD_EQ_DELETE_BASE;
        if (i < u.size()) Ini(PathJoin(dataDir_, L"eqpresets.ini")).remove(L"Presets", u[i].name);
        return;
    }
    if (cmd == CMD_DEVICE_DEFAULT || (cmd >= CMD_DEVICE_BASE && cmd < CMD_THUMB_PREV)) {
        std::wstring id;
        if (cmd != CMD_DEVICE_DEFAULT) {
            auto list = ListOutputDevices();
            size_t i = cmd - CMD_DEVICE_BASE;
            if (i >= list.size()) return;
            id = list[i].id;
        }
        player.setDevice(id);
        cfg.set(L"Main", L"OutputDevice", id);
        return;
    }
    if (cmd >= CMD_BASS_PRESET_BASE && cmd < CMD_DEVICE_DEFAULT) {
        applyBassPreset(cmd - CMD_BASS_PRESET_BASE);
        return;
    }
    HWND owner = mainWnd->hwnd;
    switch (cmd) {
        case CMD_ABOUT: {
            std::wstring ff = FfmpegPath();
            std::wstring s = L"RetroAmp " APP_VERSION L"\nA classic skinnable audio player for Windows.\n\n"
                             L"Skins: Winamp 2.x classic format (.wsz / .zip / folder)\n"
                             L"Decoders: Windows Media Foundation (MP3, WAV, FLAC, AAC/M4A, WMA, ALAC, AC3...)\n"
                             L"ffmpeg: " + (ff.empty() ? std::wstring(L"not found - OGG/Opus/APE/WV/MOD etc. unavailable") : ff) +
                             L"\n\nKeys: Z prev, X play, C pause, V stop, B next, L open, J jump,\n"
                             L"S shuffle, R repeat, Left/Right seek, Up/Down volume,\n"
                             L"Alt+G equalizer, Alt+E playlist, Alt+B bass & DSP, Alt+V vis, Ctrl+D size.\n\n"
                             L"Settings: " + dataDir_;
            MessageBoxW(owner, s.c_str(), L"About RetroAmp", MB_ICONINFORMATION);
            break;
        }
        case CMD_PLAY_FILE: openFilesDialog(false); break;
        case CMD_PLAY_FOLDER: openFolderDialog(false); break;
        case CMD_ADD_FILE: openFilesDialog(true); break;
        case CMD_ADD_FOLDER: openFolderDialog(true); break;
        case CMD_ADD_URL: addUrlDialog(); break;
        case CMD_EXIT: quit(); break;
        case CMD_WND_EQ: toggleWindow(W_EQ); break;
        case CMD_WND_PL: toggleWindow(W_PL); break;
        case CMD_WND_DSP: toggleWindow(W_DSP); break;
        case CMD_WND_VIS: toggleWindow(W_VIS); break;
        case CMD_ZOOM100: case CMD_ZOOM150: case CMD_ZOOM200: case CMD_ZOOM250: case CMD_ZOOM300: case CMD_ZOOM400: {
            static const int zooms[] = {100, 150, 200, 250, 300, 400};
            setZoom(zooms[cmd - CMD_ZOOM100]);
            break;
        }
        case CMD_ONTOP: setAlwaysOnTop(!alwaysOnTop); break;
        case CMD_SHADE: setMainShade(!mainShade); break;
        case CMD_TIME_ELAPSED: timeRemaining = false; redrawAll(); break;
        case CMD_TIME_REMAINING: timeRemaining = true; redrawAll(); break;
        case CMD_VIS_SPECTRUM: visMode = VIS_SPECTRUM; break;
        case CMD_VIS_OSC: visMode = VIS_OSC; break;
        case CMD_VIS_OFF: visMode = VIS_OFF; break;
        case CMD_PLAY: playPressed(); break;
        case CMD_PAUSE: pausePressed(); break;
        case CMD_STOP: stopPressed(); break;
        case CMD_PREV: prevTrack(); break;
        case CMD_NEXT: nextTrack(); break;
        case CMD_FWD5: seekBy(5); break;
        case CMD_BACK5: seekBy(-5); break;
        case CMD_SHUFFLE: toggleShuffle(); break;
        case CMD_REPEAT: toggleRepeat(); break;
        case CMD_JUMP: jumpToFileDialog(); break;
        case CMD_FILEINFO: showFileInfo(pl.firstSelected() >= 0 ? pl.firstSelected() : pl.current); break;
        case CMD_SKIN_LOAD: loadSkinDialog(); break;
        case CMD_SKIN_DEFAULT: loadSkin(L"", true); break;
        case CMD_SKIN_FOLDER: ShellExecuteW(owner, L"open", userSkinDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
        case CMD_SKIN_WEB: ShellExecuteW(owner, L"open", L"https://skins.webamp.org/", nullptr, nullptr, SW_SHOWNORMAL); break;
        case CMD_EQ_MODE_WINAMP: dsp.eqMode = 0; applyDsp(); saveSettings(); break;
        case CMD_EQ_MODE_ISO: dsp.eqMode = 1; applyDsp(); saveSettings(); break;
        case CMD_EQ_RESET: {
            EqPreset flat;
            flat.name = L"Flat";
            applyEqPreset(flat);
            break;
        }
        case CMD_EQ_SAVE: {
            std::wstring name;
            if (InputBox(owner, L"Save EQ preset", L"Preset name:", name)) {
                for (auto& c : name)
                    if (c == L'=' || c == L'[' || c == L']') c = L'_';
                Ini(PathJoin(dataDir_, L"eqpresets.ini")).set(L"Presets", name, SerializeEq(dsp.bands, dsp.preamp));
            }
            break;
        }
        case CMD_EQ_IMPORT: {
            static const wchar_t filter[] = L"Winamp EQ files (*.eqf;*.q1)\0*.eqf;*.q1\0All files (*.*)\0*.*\0";
            auto files = OpenFilesDlg(owner, filter, false, L"Import EQ presets");
            if (files.empty()) break;
            std::vector<EqPreset> ps;
            if (!LoadEqf(files[0], ps) || ps.empty()) {
                MessageBoxW(owner, L"Not a Winamp EQ library file.", APP_NAME, MB_ICONERROR);
                break;
            }
            Ini ini(PathJoin(dataDir_, L"eqpresets.ini"));
            for (auto& p : ps)
                if (!p.name.empty()) ini.set(L"Presets", p.name, SerializeEq(p.bands, p.preamp));
            applyEqPreset(ps[0]);
            break;
        }
        case CMD_EQ_EXPORT: {
            wchar_t buf[MAX_PATH * 2] = L"preset.eqf";
            OPENFILENAMEW ofn = {sizeof(ofn)};
            ofn.hwndOwner = owner;
            ofn.lpstrFilter = L"Winamp EQ file (*.eqf)\0*.eqf\0";
            ofn.lpstrFile = buf;
            ofn.nMaxFile = (DWORD)std::size(buf);
            ofn.lpstrDefExt = L"eqf";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER;
            if (GetSaveFileNameW(&ofn)) {
                EqPreset p;
                p.name = PathStem(buf);
                memcpy(p.bands, dsp.bands, sizeof(p.bands));
                p.preamp = dsp.preamp;
                SaveEqf(buf, {p});
            }
            break;
        }
        case CMD_PL_SORT_TITLE: pl.sort(Playlist::ByTitle); redrawAll(); break;
        case CMD_PL_SORT_FILE: pl.sort(Playlist::ByFileName); redrawAll(); break;
        case CMD_PL_SORT_PATH: pl.sort(Playlist::ByPath); redrawAll(); break;
        case CMD_PL_REVERSE: pl.reverse(); redrawAll(); break;
        case CMD_PL_RANDOM: pl.randomize(); redrawAll(); break;
        case CMD_PL_REMOVE_MISSING: pl.removeMissing(); plWnd->clampScroll(); redrawAll(); break;
        case CMD_PL_NEW:
        case CMD_PL_CLEAR:
            meta.cancelAll();
            pl.clear();
            plWnd->clampScroll();
            updateTitle();
            redrawAll();
            break;
        case CMD_PL_SAVE: savePlaylistDialog(); break;
        case CMD_PL_LOAD: loadPlaylistDialog(); break;
        case CMD_PL_SELECT_ALL: pl.selectAll(true); redraw(W_PL); break;
        case CMD_PL_SELECT_NONE: pl.selectAll(false); redraw(W_PL); break;
        case CMD_PL_INVERT: pl.invertSelection(); redraw(W_PL); break;
        case CMD_PL_REMOVE_SEL:
            pl.removeSelected();
            plWnd->clampScroll();
            updateTitle();
            redrawAll();
            break;
        case CMD_PL_CROP:
            pl.crop();
            plWnd->clampScroll();
            updateTitle();
            redrawAll();
            break;
        case CMD_PL_PLAY_SEL:
            if (pl.firstSelected() >= 0) playIndex(pl.firstSelected());
            break;
        case CMD_THUMB_PREV: prevTrack(); break;
        case CMD_THUMB_PLAY:
            if (state() == PlayState::Playing) pausePressed();
            else playPressed();
            break;
        case CMD_THUMB_NEXT: nextTrack(); break;
        case CMD_THUMB_STOP: stopPressed(); break;
    }
}

bool App::handleKey(UINT vk, bool alt, bool ctrl, bool shift) {
    if (alt) {
        switch (vk) {
            case 'G': toggleWindow(W_EQ); return true;
            case 'E': toggleWindow(W_PL); return true;
            case 'B': toggleWindow(W_DSP); return true;
            case 'V': toggleWindow(W_VIS); return true;
            case 'S': loadSkinDialog(); return true;
            case '3': showFileInfo(pl.firstSelected() >= 0 ? pl.firstSelected() : pl.current); return true;
        }
        return false;
    }
    if (ctrl) {
        switch (vk) {
            case 'D': setZoom(zoom >= 200 ? 100 : 200); return true;
            case VK_OEM_PLUS:
            case VK_ADD: setZoom(zoom >= 300 ? 400 : zoom + 50); return true;
            case VK_OEM_MINUS:
            case VK_SUBTRACT: setZoom(zoom <= 100 ? 100 : zoom - 50); return true;
            case 'A': setAlwaysOnTop(!alwaysOnTop); return true;
            case 'T': timeRemaining = !timeRemaining; redrawAll(); return true;
            case 'W': setMainShade(!mainShade); return true;
            case 'L': addUrlDialog(); return true;
            case 'P': showMainMenu(mainWnd->hwnd, mainWnd->menuPoint(6, 12)); return true;
        }
        return false;
    }
    switch (vk) {
        case 'Z': prevTrack(); return true;
        case 'X': playPressed(); return true;
        case 'C': pausePressed(); return true;
        case 'V': stopPressed(); return true;
        case 'B': nextTrack(); return true;
        case 'L':
            if (shift) openFolderDialog(false);
            else openFilesDialog(false);
            return true;
        case 'J': jumpToFileDialog(); return true;
        case 'S': toggleShuffle(); return true;
        case 'R': toggleRepeat(); return true;
        case VK_LEFT: seekBy(shift ? -30 : -5); return true;
        case VK_RIGHT: seekBy(shift ? 30 : 5); return true;
        case VK_UP: setVolume(dsp.volume + 0.02f); return true;
        case VK_DOWN: setVolume(dsp.volume - 0.02f); return true;
        case VK_NUMPAD4: prevTrack(); return true;
        case VK_NUMPAD5: playPressed(); return true;
        case VK_NUMPAD6: nextTrack(); return true;
        case VK_INSERT: openFolderDialog(true); return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Events
bool App::handleMainMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result) {
    result = 0;
    if (taskbarMsg_ && msg == taskbarMsg_) {
        onTaskbarCreated();
        return true;
    }
    switch (msg) {
        case WM_PLAYER_EVENT: onPlayerEvent(wp, lp); return true;
        case WM_META_READY: onMeta((MetaResult*)lp); return true;
        case WM_TIMER:
            if (wp == kTimerId) onTimer();
            return true;
        case WM_HOTKEY:
            switch (wp) {
                case 1: handleCommand(CMD_THUMB_PLAY); break;
                case 2: nextTrack(); break;
                case 3: prevTrack(); break;
                case 4: stopPressed(); break;
            }
            return true;
        case WM_APPCOMMAND:
            switch (GET_APPCOMMAND_LPARAM(lp)) {
                case APPCOMMAND_MEDIA_PLAY_PAUSE: handleCommand(CMD_THUMB_PLAY); break;
                case APPCOMMAND_MEDIA_PLAY: playPressed(); break;
                case APPCOMMAND_MEDIA_PAUSE: pausePressed(); break;
                case APPCOMMAND_MEDIA_STOP: stopPressed(); break;
                case APPCOMMAND_MEDIA_NEXTTRACK: nextTrack(); break;
                case APPCOMMAND_MEDIA_PREVIOUSTRACK: prevTrack(); break;
                default: return false;
            }
            result = TRUE;
            return true;
        case WM_COMMAND:
            if (HIWORD(wp) == THBN_CLICKED) {
                onThumbButton(LOWORD(wp));
                return true;
            }
            return false;
        case WM_COPYDATA: {
            auto* cds = (COPYDATASTRUCT*)lp;
            if (cds && cds->dwData == 0x52414D50 && cds->lpData)
                onCopyData(std::wstring((const wchar_t*)cds->lpData, cds->cbData / sizeof(wchar_t)));
            result = TRUE;
            return true;
        }
        case WM_QUERYENDSESSION:
            result = TRUE;
            return true;
        case WM_ENDSESSION:
            if (wp) {
                saveSettings();
                pl.saveM3U(PathJoin(dataDir_, L"playlist.m3u8"));
            }
            return true;
    }
    return false;
}

void App::onPlayerEvent(WPARAM ev, LPARAM session) {
    if ((uint64_t)session != player.session()) return;
    switch (ev) {
        case PE_OPENED: {
            errorStreak_ = 0;
            if (pl.current >= 0 && pl.current < pl.size()) {
                Track& t = pl.tracks[pl.current];
                if (t.length < 0 && player.duration() > 0) t.length = player.duration();
                if (!t.bitrate) t.bitrate = player.bitrate();
                t.sampleRate = player.sampleRate();
                t.channels = player.channels();
            }
            redrawAll();
            break;
        }
        case PE_ENDED: {
            int idx = pl.nextIndex(shuffle, repeat);
            if (idx >= 0)
                playIndex(idx);
            else {
                player.stop();
                redrawAll();
            }
            break;
        }
        case PE_ERROR: {
            std::wstring err = player.lastError();
            const Track* t = currentTrack();
            std::wstring name = t ? PathFileName(t->path) : L"";
            setMarqueeOverride(L"ERROR: " + name);
            marqueeUntil_ = GetTickCount64() + 4000;
            errorStreak_++;
            if (errorStreak_ == 1 && pl.size() <= 1) {
                MessageBoxW(mainWnd->hwnd, (L"Cannot play:\n" + (t ? t->path : L"") + L"\n\n" + err).c_str(), APP_NAME,
                            MB_ICONWARNING);
            }
            if (errorStreak_ < pl.size()) {
                int idx = pl.nextIndex(shuffle, true);
                if (idx >= 0) playIndex(idx);
            } else {
                errorStreak_ = 0;
                redrawAll();
            }
            break;
        }
    }
    updateTaskbar();
}

void App::onMeta(MetaResult* r) {
    std::unique_ptr<MetaResult> owned(r);
    int idx = pl.indexOf(r->id);
    if (idx < 0) return;
    Track& t = pl.tracks[idx];
    if (!r->title.empty()) t.title = r->title;
    if (r->length > 0) t.length = r->length;
    if (r->bitrate) t.bitrate = r->bitrate;
    if (r->sampleRate) t.sampleRate = r->sampleRate;
    if (r->channels) t.channels = r->channels;
    redraw(W_PL);
    if (idx == pl.current) {
        updateTitle();
        redraw(W_MAIN);
    }
}

void App::savePlaylist() { pl.saveM3U(PathJoin(dataDir_, L"playlist.m3u8")); }

void App::onTimer() {
    timerTicks_++;
    if (timerTicks_ % 300 == 0) savePlaylist();  // ~every 10 s
    mainWnd->tick();
    if (timerTicks_ % 8 == 0) {
        if (state() != PlayState::Stopped) redraw(W_PL);
        updateTaskbar();
    }
}

void App::onCopyData(const std::wstring& data) {
    std::vector<std::wstring> files;
    size_t a = 0;
    while (a < data.size()) {
        size_t b = data.find(L'\n', a);
        if (b == std::wstring::npos) b = data.size();
        std::wstring f = Trim(data.substr(a, b - a));
        if (!f.empty()) files.push_back(f);
        a = b + 1;
    }
    if (files.empty()) {
        ShowWindow(mainWnd->hwnd, SW_RESTORE);
        SetForegroundWindow(mainWnd->hwnd);
        return;
    }
    onDropFiles(files, -1, false);
}

// ---------------------------------------------------------------------------
// Taskbar (thumbnail toolbar + progress)
void App::onTaskbarCreated() {
    if (!taskbar_) {
        ITaskbarList3* tb = nullptr;
        if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&tb)))) return;
        if (FAILED(tb->HrInit())) {
            tb->Release();
            return;
        }
        taskbar_ = tb;
    }
    int sz = GetSystemMetrics(SM_CXSMICON);
    for (int i = 0; i < 5; i++)
        if (!thumbIcons_[i]) thumbIcons_[i] = MakeGlyphIcon(i, sz);
    THUMBBUTTON b[4] = {};
    const int ids[4] = {CMD_THUMB_PREV, CMD_THUMB_PLAY, CMD_THUMB_STOP, CMD_THUMB_NEXT};
    const int icons[4] = {0, 1, 4, 3};
    const wchar_t* tips[4] = {L"Previous", L"Play", L"Stop", L"Next"};
    for (int i = 0; i < 4; i++) {
        b[i].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
        b[i].iId = ids[i];
        b[i].hIcon = thumbIcons_[icons[i]];
        wcscpy_s(b[i].szTip, tips[i]);
        b[i].dwFlags = THBF_ENABLED;
    }
    auto* tb = (ITaskbarList3*)taskbar_;
    thumbAdded_ = SUCCEEDED(tb->ThumbBarAddButtons(mainWnd->hwnd, 4, b));
    lastTaskbarState_ = PlayState::Stopped;
    lastProgress_ = -1;
    updateTaskbar();
}

void App::onThumbButton(int id) { handleCommand(id); }

void App::updateTaskbar() {
    if (!taskbar_) return;
    auto* tb = (ITaskbarList3*)taskbar_;
    PlayState st = state();
    if (thumbAdded_ && st != lastTaskbarState_) {
        THUMBBUTTON b = {};
        b.dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
        b.iId = CMD_THUMB_PLAY;
        b.hIcon = thumbIcons_[st == PlayState::Playing ? 2 : 1];
        wcscpy_s(b.szTip, st == PlayState::Playing ? L"Pause" : L"Play");
        b.dwFlags = THBF_ENABLED;
        tb->ThumbBarUpdateButtons(mainWnd->hwnd, 1, &b);
    }
    double dur = duration();
    if (st == PlayState::Stopped || dur <= 0) {
        if (lastProgress_ != -1 || st != lastTaskbarState_) tb->SetProgressState(mainWnd->hwnd, TBPF_NOPROGRESS);
        lastProgress_ = -1;
    } else {
        int p = (int)(position() / dur * 1000);
        if (st != lastTaskbarState_)
            tb->SetProgressState(mainWnd->hwnd, st == PlayState::Paused ? TBPF_PAUSED : TBPF_NORMAL);
        if (p != lastProgress_) tb->SetProgressValue(mainWnd->hwnd, Clamp(p, 0, 1000), 1000);
        lastProgress_ = p;
    }
    lastTaskbarState_ = st;
}
