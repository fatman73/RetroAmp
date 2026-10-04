#include "panelwnd.h"

#include "../app.h"

void PanelWnd::paint(Canvas& c) {
    const Skin& sk = skin();
    const Image& img = sk.animImage(sk.panelImage);
    if (img.valid())
        c.blit(img, 0, 0, lw, lh, 0, 0);
    else
        c.fill(0, 0, lw, lh, 0);
    g_app->anim.draw(c, W_PANEL);
    if (pressed_ >= 0 && inside_ && pressed_ < (int)sk.buttons.size()) {
        const SkinButton& b = sk.buttons[pressed_];
        c.darken(b.x, b.y, b.w, b.h, 0.6f);
    }
}

int PanelWnd::buttonAt(int x, int y) const {
    const auto& bs = skin().buttons;
    for (int i = 0; i < (int)bs.size(); i++)
        if (bs[i].window == W_PANEL && Rc{bs[i].x, bs[i].y, bs[i].w, bs[i].h}.hit(x, y)) return i;
    return -1;
}

void PanelWnd::onMouseDown(int x, int y, WPARAM) {
    int b = buttonAt(x, y);
    if (b < 0) {
        startWindowDrag();
        return;
    }
    pressed_ = b;
    inside_ = true;
    redraw();
}

void PanelWnd::onMouseMove(int x, int y, WPARAM mk) {
    if (!(mk & MK_LBUTTON) || pressed_ < 0) return;
    bool in = buttonAt(x, y) == pressed_;
    if (in != inside_) {
        inside_ = in;
        redraw();
    }
}

void PanelWnd::onMouseUp(int x, int y) {
    int b = pressed_;
    bool in = inside_ && buttonAt(x, y) == b;
    pressed_ = -1;
    redraw();
    if (b >= 0 && in && b < (int)skin().buttons.size()) g_app->skinAction(skin().buttons[b].action);
}

void PanelWnd::onCaptureLost() {
    if (pressed_ >= 0) {
        pressed_ = -1;
        redraw();
    }
}
