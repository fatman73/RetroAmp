#include "plwnd.h"

#include "../app.h"

namespace {
const int kRowH = 13;
enum {
    B_NONE = 0,
    B_CLOSE,
    B_SHADE,
    B_ADD,
    B_REM,
    B_SEL,
    B_MISC,
    B_LIST,
    B_PREV,
    B_PLAY,
    B_PAUSE,
    B_STOP,
    B_NEXT,
    B_EJECT,
    B_UP,
    B_DOWN,
    B_RESIZE,
    B_SCROLLBAR,
};
}  // namespace

PlWnd::~PlWnd() {
    if (font_) DeleteObject(font_);
}

int PlWnd::visibleRows() const { return std::max(1, (lh - 58) / kRowH); }

int PlWnd::rowAt(int y) const {
    if (y < 20) return -1;
    int r = scroll_ + (y - 20) / kRowH;
    return r < g_app->pl.size() ? r : -1;
}

void PlWnd::clampScroll() {
    int maxScroll = std::max(0, g_app->pl.size() - visibleRows());
    scroll_ = Clamp(scroll_, 0, maxScroll);
}

void PlWnd::ensureVisible(int idx) {
    if (idx < 0) return;
    int rows = visibleRows();
    if (idx < scroll_) scroll_ = idx;
    else if (idx >= scroll_ + rows) scroll_ = idx - rows + 1;
    clampScroll();
    redraw();
}

int PlWnd::scrollHandleY() const {
    int track = lh - 58 - 18;
    int maxScroll = std::max(0, g_app->pl.size() - visibleRows());
    if (maxScroll == 0 || track <= 0) return 20;
    return 20 + (int)std::lround((double)scroll_ / maxScroll * track);
}

PlWnd::Menu PlWnd::menuDef(int m) const {
    switch (m) {
        case 0: return {14, 3, 0, 48, 54};
        case 1: return {43, 4, 54, 100, 72};
        case 2: return {72, 3, 104, 150, 54};
        case 3: return {101, 3, 154, 200, 54};
        default: return {lw - 49, 3, 204, 250, 54};
    }
}

int PlWnd::menuItemAt(int x, int y) const {
    if (menu_ < 0) return -1;
    Menu d = menuDef(menu_);
    int bottom = lh - 12, top = bottom - d.count * 18;
    if (x < d.x || x >= d.x + 25 || y < top || y >= bottom) return -1;
    return (y - top) / 18;
}

int PlWnd::buttonAt(int x, int y) const {
    const int W = lw, H = lh, X0 = W - 150, Y0 = H - 38;
    if (Rc{W - 11, 3, 9, 9}.hit(x, y)) return B_CLOSE;
    if (Rc{W - 20, 3, 9, 9}.hit(x, y)) return B_SHADE;
    if (Rc{14, H - 30, 25, 18}.hit(x, y)) return B_ADD;
    if (Rc{43, H - 30, 25, 18}.hit(x, y)) return B_REM;
    if (Rc{72, H - 30, 25, 18}.hit(x, y)) return B_SEL;
    if (Rc{101, H - 30, 25, 18}.hit(x, y)) return B_MISC;
    if (Rc{W - 46, H - 30, 22, 18}.hit(x, y)) return B_LIST;
    if (Rc{X0 + 4, H - 17, 10, 11}.hit(x, y)) return B_PREV;
    if (Rc{X0 + 14, H - 17, 10, 11}.hit(x, y)) return B_PLAY;
    if (Rc{X0 + 24, H - 17, 9, 11}.hit(x, y)) return B_PAUSE;
    if (Rc{X0 + 33, H - 17, 9, 11}.hit(x, y)) return B_STOP;
    if (Rc{X0 + 42, H - 17, 8, 11}.hit(x, y)) return B_NEXT;
    if (Rc{X0 + 50, H - 17, 10, 11}.hit(x, y)) return B_EJECT;
    if (Rc{X0 + 130, Y0 + 2, 10, 7}.hit(x, y)) return B_UP;
    if (Rc{X0 + 130, Y0 + 9, 10, 7}.hit(x, y)) return B_DOWN;
    if (Rc{W - 20, H - 20, 20, 20}.hit(x, y)) return B_RESIZE;
    if (Rc{W - 16, 20, 10, H - 58}.hit(x, y)) return B_SCROLLBAR;
    return B_NONE;
}

void PlWnd::ensureFont() {
    int s = renderScale();
    if (font_ && fontScale_ == s && fontName_ == skin().plFont) return;
    if (font_) DeleteObject(font_);
    fontScale_ = s;
    fontName_ = skin().plFont;
    font_ = CreateFontW(-10 * s, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, s == 1 ? NONANTIALIASED_QUALITY : ANTIALIASED_QUALITY,
                        DEFAULT_PITCH | FF_SWISS, fontName_.c_str());
}

// ---------------------------------------------------------------------------
void PlWnd::paint(Canvas& c) {
    const Skin& sk = skin();
    const Image& pe = sk.img(SB_PLEDIT);
    const int W = lw, H = lh;
    const int ty = active ? 0 : 21;
    // top
    c.tile(pe, 127, ty, 25, 20, 25, 0, W - 50, 20);
    c.blit(pe, 0, ty, 25, 20, 0, 0);
    c.blit(pe, 26, ty, 100, 20, (W - 100) / 2, 0);
    c.blit(pe, 153, ty, 25, 20, W - 25, 0);
    if (push_.is(B_CLOSE)) c.blit(pe, 52, 42, 9, 9, W - 11, 3);
    if (push_.is(B_SHADE)) c.blit(pe, 62, 42, 9, 9, W - 20, 3);
    // sides
    c.tile(pe, 0, 42, 12, 29, 0, 20, 12, H - 58);
    c.tile(pe, 31, 42, 20, 29, W - 20, 20, 20, H - 58);
    // bottom
    c.tile(pe, 179, 0, 25, 38, 125, H - 38, W - 275, 38);
    if (W >= 350) c.blit(pe, 205, 0, 75, 38, W - 225, H - 38);
    c.blit(pe, 0, 72, 125, 38, 0, H - 38);
    c.blit(pe, 126, 72, 150, 38, W - 150, H - 38);

    // list
    Rc lr = listRect();
    c.fill(lr.x, lr.y, lr.w, lr.h, sk.plNormalBG);
    ensureFont();
    const Playlist& pl = g_app->pl;
    int rows = visibleRows() + 1;
    for (int i = 0; i < rows; i++) {
        int idx = scroll_ + i;
        if (idx >= pl.size()) break;
        const Track& t = pl.tracks[idx];
        int y = 20 + i * kRowH;
        c.setClip(lr.x, lr.y, lr.w, lr.h);
        if (t.selected) c.fill(lr.x, y, lr.w, kRowH, sk.plSelectedBG);
        uint32_t col = idx == pl.current ? sk.plCurrent : sk.plNormal;
        std::wstring dur = t.length >= 0 ? FormatTime(t.length) : L"";
        int durW = dur.empty() ? 0 : (int)dur.size() * 6 + 4;
        c.text(Fmt(L"%d. ", idx + 1) + t.display(), lr.x + 3, y, lr.w - 6 - durW, kRowH, font_, col,
               DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        if (!dur.empty()) c.text(dur, lr.x, y, lr.w - 3, kRowH, font_, col, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        if (idx == focus_ && active) c.frame(lr.x, y, lr.w, kRowH, BlendColor(sk.plNormal, sk.plNormalBG, 0.5f));
        c.resetClip();
    }
    // scroll handle
    c.blit(pe, mode_ == M_SCROLL ? 61 : 52, 53, 8, 18, W - 15, scrollHandleY());

    // running time  "sel/total"
    bool unkSel = false, unkAll = false;
    double selLen = pl.totalLength(true, &unkSel), allLen = pl.totalLength(false, &unkAll);
    std::wstring rt = FormatTime(selLen) + (unkSel && pl.selectedCount() ? L"+" : L"") + L"/" + FormatTime(allLen) +
                      (unkAll && pl.size() ? L"+" : L"");
    c.setClip(W - 143, H - 28, 90, 6);
    sk.drawText(c, ToSkinText(rt), W - 143, H - 28, true);
    c.resetClip();
    // mini time
    if (g_app->state() != PlayState::Stopped) {
        double pos = g_app->position(), dur = g_app->duration();
        bool rem = g_app->timeRemaining && dur > 0;
        int t = (int)(rem ? std::max(0.0, dur - pos) : pos);
        char buf[16];
        sprintf_s(buf, "%s%d:%02d", rem ? "-" : "", t / 60, t % 60);
        std::string s = buf;
        sk.drawText(c, s, W - 59 - (int)s.size() * 5, H - 15, true);
    }
    // flyout menu
    if (menu_ >= 0) {
        Menu d = menuDef(menu_);
        int bottom = H - 12, top = bottom - d.count * 18;
        c.blit(pe, d.barX, 111, 3, d.barH, d.x, top);
        for (int k = 0; k < d.count; k++)
            c.blit(pe, d.spriteX + (k == menuHover_ ? 23 : 0), 111 + k * 19, 22, 18, d.x + 3, top + k * 18);
    }
}

// ---------------------------------------------------------------------------
void PlWnd::selectOnly(int idx) {
    g_app->pl.selectAll(false);
    if (idx >= 0 && idx < g_app->pl.size()) g_app->pl.tracks[idx].selected = true;
    anchor_ = focus_ = idx;
}

void PlWnd::moveFocus(int idx, bool shift, bool ctrl) {
    Playlist& pl = g_app->pl;
    if (pl.size() == 0) return;
    idx = Clamp(idx, 0, pl.size() - 1);
    if (shift) {
        if (anchor_ < 0) anchor_ = focus_ >= 0 ? focus_ : idx;
        if (!ctrl) pl.selectAll(false);
        int a = std::min(anchor_, idx), b = std::max(anchor_, idx);
        for (int i = a; i <= b; i++) pl.tracks[i].selected = true;
        focus_ = idx;
    } else if (ctrl) {
        focus_ = idx;
    } else {
        selectOnly(idx);
    }
    ensureVisible(idx);
    redraw();
}

void PlWnd::openMenu(int m) {
    menu_ = m;
    menuHover_ = menuDef(m).count - 1;
    menuSticky_ = false;
    menuMoved_ = false;
    mode_ = M_MENU;
    redraw();
}

void PlWnd::execMenu(int m, int item) {
    Playlist& pl = g_app->pl;
    switch (m) {
        case 0:  // add
            if (item == 0) g_app->addUrlDialog();
            else if (item == 1) g_app->openFolderDialog(true);
            else g_app->openFilesDialog(true);
            break;
        case 1:  // remove
            if (item == 0) g_app->handleCommand(CMD_PL_CLEAR);
            else if (item == 1) g_app->handleCommand(CMD_PL_CROP);
            else if (item == 2) g_app->handleCommand(CMD_PL_REMOVE_SEL);
            else g_app->showPlaylistMenu(2, hwnd, menuPoint(46, lh - 30));
            break;
        case 2:  // select
            if (item == 0) pl.invertSelection();
            else if (item == 1) pl.selectAll(false);
            else pl.selectAll(true);
            redraw();
            break;
        case 3:  // misc
            if (item == 0) g_app->showPlaylistMenu(1, hwnd, menuPoint(104, lh - 30));
            else if (item == 1) g_app->showFileInfo(pl.firstSelected() >= 0 ? pl.firstSelected() : pl.current);
            else g_app->showPlaylistMenu(3, hwnd, menuPoint(104, lh - 30));
            break;
        case 4:  // list
            if (item == 0) g_app->handleCommand(CMD_PL_NEW);
            else if (item == 1) g_app->savePlaylistDialog();
            else g_app->loadPlaylistDialog();
            break;
    }
}

void PlWnd::onMouseDown(int x, int y, WPARAM mk) {
    // flyout menu open?
    if (menu_ >= 0) {
        int item = menuItemAt(x, y);
        if (item >= 0) {
            menuHover_ = item;
            menuMoved_ = true;
            mode_ = M_MENU;
            redraw();
            return;
        }
        menu_ = -1;
        redraw();
        int b = buttonAt(x, y);
        if (b >= B_ADD && b <= B_LIST) {
            openMenu(b - B_ADD);
            return;
        }
        mode_ = M_NONE;
        if (GetCapture() == hwnd) ReleaseCapture();
        return;
    }
    Rc lr = listRect();
    if (lr.hit(x, y) && x < lw - 20) {
        int row = rowAt(y);
        bool ctrl = (mk & MK_CONTROL) != 0, shift = (mk & MK_SHIFT) != 0;
        Playlist& pl = g_app->pl;
        if (row < 0) {
            pl.selectAll(false);
            redraw();
            return;
        }
        if (shift) {
            moveFocus(row, true, ctrl);
        } else if (ctrl) {
            pl.tracks[row].selected = !pl.tracks[row].selected;
            anchor_ = focus_ = row;
        } else {
            if (!pl.tracks[row].selected) selectOnly(row);
            focus_ = row;
            mode_ = M_MOVE;
            dragRow_ = row;
            pressRow_ = row;
            moved_ = false;
        }
        redraw();
        return;
    }
    int b = buttonAt(x, y);
    switch (b) {
        case B_ADD:
        case B_REM:
        case B_SEL:
        case B_MISC:
        case B_LIST:
            openMenu(b - B_ADD);
            return;
        case B_RESIZE:
            mode_ = M_RESIZE;
            GetCursorPos(&resizeStart_);
            resizeW_ = g_app->plTilesW;
            resizeH_ = g_app->plTilesH;
            return;
        case B_SCROLLBAR: {
            int hy = scrollHandleY();
            if (y >= hy && y < hy + 18) {
                mode_ = M_SCROLL;
                scrollGrab_ = y - hy;
            } else {
                scroll_ += (y < hy ? -1 : 1) * visibleRows();
                clampScroll();
            }
            redraw();
            return;
        }
        case B_UP:
        case B_DOWN:
            scroll_ += b == B_UP ? -1 : 1;
            clampScroll();
            redraw();
            return;
        case B_NONE:
            startWindowDrag();
            return;
        default:
            mode_ = M_BUTTON;
            push_.pressed = b;
            push_.inside = true;
            redraw();
    }
}

void PlWnd::onMouseMove(int x, int y, WPARAM mk) {
    if (!(mk & MK_LBUTTON)) return;
    switch (mode_) {
        case M_MENU: {
            int item = menuItemAt(x, y);
            if (item != menuHover_) {
                menuHover_ = item;
                menuMoved_ = true;
                redraw();
            }
            break;
        }
        case M_MOVE: {
            int row = scroll_ + (int)std::floor((y - 20) / (double)kRowH);
            if (y < 20 && scroll_ > 0) scroll_--;
            if (y >= lh - 38) scroll_++;
            clampScroll();
            row = Clamp(row, 0, std::max(0, g_app->pl.size() - 1));
            if (row != dragRow_) {
                int moved = g_app->pl.moveSelected(row - dragRow_);
                if (moved) {
                    dragRow_ += moved;
                    moved_ = true;
                    focus_ = Clamp(focus_ + moved, 0, g_app->pl.size() - 1);
                    g_app->redraw(W_MAIN);
                }
            }
            redraw();
            break;
        }
        case M_SCROLL: {
            int track = lh - 58 - 18;
            int maxScroll = std::max(0, g_app->pl.size() - visibleRows());
            if (track > 0) scroll_ = (int)std::lround((double)(y - scrollGrab_ - 20) / track * maxScroll);
            clampScroll();
            redraw();
            break;
        }
        case M_RESIZE: {
            POINT p;
            GetCursorPos(&p);
            double z = g_app->zoom / 100.0;
            int tw = std::max(0, resizeW_ + (int)std::lround((p.x - resizeStart_.x) / (25.0 * z)));
            int th = std::max(0, resizeH_ + (int)std::lround((p.y - resizeStart_.y) / (29.0 * z)));
            if (tw != g_app->plTilesW || th != g_app->plTilesH) g_app->setPlaylistSize(tw, th);
            break;
        }
        case M_BUTTON: {
            bool in = buttonAt(x, y) == push_.pressed;
            if (in != push_.inside) {
                push_.inside = in;
                redraw();
            }
            break;
        }
        default:
            break;
    }
}

void PlWnd::onMouseUp(int x, int y) {
    Mode m = mode_;
    mode_ = M_NONE;
    switch (m) {
        case M_MENU: {
            int item = menuItemAt(x, y);
            int bottomItem = menu_ >= 0 ? menuDef(menu_).count - 1 : -1;
            if (!menuSticky_ && !menuMoved_ && item == bottomItem) {
                menuSticky_ = true;  // simple click on the button: keep the menu open
                redraw();
                return;
            }
            int menu = menu_;
            menu_ = -1;
            redraw();
            if (item >= 0 && menu >= 0) execMenu(menu, item);
            return;
        }
        case M_MOVE:
            if (!moved_ && pressRow_ >= 0 && !(GetKeyState(VK_CONTROL) < 0)) selectOnly(pressRow_);
            redraw();
            return;
        case M_RESIZE:
            g_app->saveSettings();
            return;
        case M_SCROLL:
            redraw();
            return;
        case M_BUTTON: {
            int id = push_.pressed;
            bool inside = push_.inside && buttonAt(x, y) == id;
            push_ = PushState();
            redraw();
            if (!inside) return;
            switch (id) {
                case B_CLOSE: g_app->setWindowVisible(W_PL, false); break;
                case B_SHADE: break;
                case B_PREV: g_app->prevTrack(); break;
                case B_PLAY: g_app->playPressed(); break;
                case B_PAUSE: g_app->pausePressed(); break;
                case B_STOP: g_app->stopPressed(); break;
                case B_NEXT: g_app->nextTrack(); break;
                case B_EJECT: g_app->openFilesDialog(true); break;
            }
            return;
        }
        default:
            return;
    }
}

void PlWnd::onCaptureLost() {
    if (mode_ == M_MENU && menuSticky_) return;  // keep a sticky menu open
    if (mode_ == M_BUTTON) push_ = PushState();
    if (mode_ != M_NONE) {
        mode_ = M_NONE;
        redraw();
    }
}

bool PlWnd::onDblClick(int x, int y) {
    Rc lr = listRect();
    if (lr.hit(x, y) && x < lw - 20 && menu_ < 0) {
        int row = rowAt(y);
        if (row >= 0) {
            selectOnly(row);
            g_app->playIndex(row);
            return true;
        }
    }
    return false;
}

void PlWnd::onRightClick(int x, int y) {
    Rc lr = listRect();
    if (lr.hit(x, y) && x < lw - 20) {
        int row = rowAt(y);
        if (row >= 0 && !g_app->pl.tracks[row].selected) selectOnly(row);
        redraw();
        POINT pt;
        GetCursorPos(&pt);
        g_app->showPlaylistMenu(0, hwnd, pt);
        return;
    }
    SkinWnd::onRightClick(x, y);
}

void PlWnd::onWheel(int delta, int, int) {
    scroll_ += delta > 0 ? -3 : 3;
    clampScroll();
    redraw();
}

bool PlWnd::onKey(UINT vk, bool ctrl, bool shift) {
    Playlist& pl = g_app->pl;
    int f = focus_ >= 0 ? focus_ : (pl.firstSelected() >= 0 ? pl.firstSelected() : 0);
    switch (vk) {
        case VK_UP: moveFocus(f - 1, shift, ctrl); return true;
        case VK_DOWN: moveFocus(f + 1, shift, ctrl); return true;
        case VK_PRIOR: moveFocus(f - visibleRows(), shift, ctrl); return true;
        case VK_NEXT: moveFocus(f + visibleRows(), shift, ctrl); return true;
        case VK_HOME: moveFocus(0, shift, ctrl); return true;
        case VK_END: moveFocus(pl.size() - 1, shift, ctrl); return true;
        case VK_SPACE:
            if (ctrl && focus_ >= 0 && focus_ < pl.size()) {
                pl.tracks[focus_].selected = !pl.tracks[focus_].selected;
                redraw();
                return true;
            }
            return false;
        case VK_RETURN: {
            int idx = focus_ >= 0 ? focus_ : pl.firstSelected();
            if (idx >= 0 && idx < pl.size()) g_app->playIndex(idx);
            return true;
        }
        case VK_DELETE:
            if (shift) g_app->handleCommand(CMD_PL_CROP);
            else g_app->handleCommand(CMD_PL_REMOVE_SEL);
            return true;
        case 'A':
            if (ctrl) {
                pl.selectAll(true);
                redraw();
                return true;
            }
            return false;
        case 'I':
            if (ctrl) {
                pl.invertSelection();
                redraw();
                return true;
            }
            return false;
    }
    return false;
}

void PlWnd::onDropFiles(const std::vector<std::wstring>& files, int x, int y) {
    int at = -1;
    if (listRect().hit(x, y)) {
        at = scroll_ + (y - 20) / kRowH;
        if (at > g_app->pl.size()) at = -1;
    }
    g_app->onDropFiles(files, at, true);
}
