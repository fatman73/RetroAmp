#include "eqwnd.h"

#include "../app.h"

namespace {
enum { C_NONE = 0, C_CLOSE, C_ON, C_AUTO, C_PRESETS, C_GRAPH };

int SliderX(int s) { return s == 0 ? 21 : 78 + (s - 1) * 18; }

double CatmullRom(double p0, double p1, double p2, double p3, double t) {
    double t2 = t * t, t3 = t2 * t;
    return 0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3);
}
}  // namespace

int EqWnd::hit(int x, int y) const {
    if (Rc{264, 3, 9, 9}.hit(x, y)) return C_CLOSE;
    if (Rc{14, 18, 26, 12}.hit(x, y)) return C_ON;
    if (Rc{40, 18, 32, 12}.hit(x, y)) return C_AUTO;
    if (Rc{217, 18, 44, 12}.hit(x, y)) return C_PRESETS;
    if (Rc{86, 17, 113, 19}.hit(x, y)) return C_GRAPH;
    return C_NONE;
}

int EqWnd::sliderAt(int x, int y) const {
    if (y < 38 || y >= 38 + 63) return -1;
    for (int s = 0; s <= 10; s++) {
        int sx = SliderX(s);
        if (x >= sx && x < sx + 14) return s;
    }
    return -1;
}

float EqWnd::sliderValue(int s) const { return s == 0 ? g_app->dsp.preamp : g_app->dsp.bands[s - 1]; }

void EqWnd::setSlider(int s, float db) {
    db = Clamp(db, -12.0f, 12.0f);
    if (std::fabs(db) < 0.6f) db = 0;
    if (s == 0)
        g_app->setPreamp(db);
    else
        g_app->setEqBand(s - 1, db);
}

void EqWnd::drawSlider(Canvas& c, int x, float db, bool pressed) {
    const Image& eq = skin().img(SB_EQMAIN);
    float p = Clamp((db + 12) / 24.0f, 0.0f, 1.0f);
    int frame = (int)std::lround(p * 27);
    c.blit(eq, 13 + (frame % 14) * 15, 164 + (frame / 14) * 65, 14, 63, x, 38);
    c.blit(eq, 0, pressed ? 176 : 164, 11, 11, x + 1, 38 + (int)std::lround((1 - p) * 51));
}

void EqWnd::paint(Canvas& c) {
    const Image& eq = skin().img(SB_EQMAIN);
    const DspParams& d = g_app->dsp;
    c.blit(eq, 0, 0, 275, 116, 0, 0);
    c.blit(eq, 0, active ? 134 : 149, 275, 14, 0, 0);
    c.blit(eq, 0, push_.is(C_CLOSE) ? 125 : 116, 9, 9, 264, 3);
    bool onP = push_.is(C_ON), autoP = push_.is(C_AUTO);
    c.blit(eq, d.eqOn ? (onP ? 187 : 69) : (onP ? 128 : 10), 119, 26, 12, 14, 18);
    c.blit(eq, g_app->eqAuto ? (autoP ? 213 : 95) : (autoP ? 154 : 36), 119, 32, 12, 40, 18);
    c.blit(eq, 224, push_.is(C_PRESETS) ? 176 : 164, 44, 12, 217, 18);

    // response graph
    c.blit(eq, 0, 294, 113, 19, 86, 17);
    int py = Clamp((int)std::lround(9 - d.preamp / 12.0 * 9), 0, 18);
    c.blit(eq, 0, 314, 113, 1, 86, 17 + py);
    double v[kEqBands];
    for (int i = 0; i < kEqBands; i++) v[i] = d.bands[i];
    // curve drawn in physical pixels (smooth at any zoom)
    const int s = c.s;
    const double seg = 112.0 / (kEqBands - 1);
    c.setClip(86, 17, 113, 19);
    int prev = -1;
    for (int px = 0; px < 113 * s; px++) {
        double t = (px + 0.5) / s / seg;
        int k = Clamp((int)t, 0, kEqBands - 2);
        double u = t - k;
        double y = CatmullRom(v[std::max(0, k - 1)], v[k], v[k + 1], v[std::min(kEqBands - 1, k + 2)], u);
        double yl = Clamp(9.5 - y / 12.0 * 9, 0.0, 18.99);
        int py = (int)((17 + yl) * s);
        c.spanPhys(86 * s + px, prev < 0 ? py : prev, py, std::max(1, (s + 1) / 2), eq.get(115, 294 + (int)yl));
        prev = py;
    }
    c.resetClip();
    // sliders
    drawSlider(c, SliderX(0), d.preamp, slider_ == 0);
    for (int i = 0; i < kEqBands; i++) drawSlider(c, SliderX(i + 1), d.bands[i], slider_ == i + 1);
    g_app->anim.draw(c, W_EQ);
}

void EqWnd::onMouseDown(int x, int y, WPARAM) {
    int s = sliderAt(x, y);
    if (s >= 0) {
        slider_ = s;
        onMouseMove(x, y, MK_LBUTTON);
        return;
    }
    int id = hit(x, y);
    if (id == C_NONE || id == C_GRAPH) {
        startWindowDrag();
        return;
    }
    push_.pressed = id;
    push_.inside = true;
    redraw();
}

void EqWnd::onMouseMove(int x, int y, WPARAM mk) {
    if (!(mk & MK_LBUTTON)) return;
    if (slider_ >= 0) {
        float p = 1.0f - (y - 38 - 5) / 51.0f;
        setSlider(slider_, p * 24 - 12);
        float db = sliderValue(slider_);
        g_app->setMarqueeOverride(slider_ == 0 ? Fmt(L"EQ: PREAMP: %+.1f DB", db)
                                               : Fmt(L"EQ: %s: %+.1f DB",
                                                     [&] {
                                                         float f = EqFrequencies(g_app->dsp.eqMode)[slider_ - 1];
                                                         return f >= 1000 ? Fmt(L"%gKHZ", f / 1000) : Fmt(L"%gHZ", f);
                                                     }()
                                                         .c_str(),
                                                     db));
        redraw();
        return;
    }
    if (push_.pressed) {
        bool in = hit(x, y) == push_.pressed;
        if (in != push_.inside) {
            push_.inside = in;
            redraw();
        }
    }
}

void EqWnd::onMouseUp(int x, int y) {
    if (slider_ >= 0) {
        slider_ = -1;
        g_app->setMarqueeOverride(L"");
        g_app->applyDsp(true);
        g_app->saveSettings();
        redraw();
        return;
    }
    int id = push_.pressed;
    bool inside = push_.inside && hit(x, y) == id;
    push_ = PushState();
    redraw();
    if (!id || !inside) return;
    switch (id) {
        case C_CLOSE:
            g_app->setWindowVisible(W_EQ, false);
            break;
        case C_ON:
            g_app->dsp.eqOn = !g_app->dsp.eqOn;
            g_app->applyDsp();
            break;
        case C_AUTO:
            g_app->eqAuto = !g_app->eqAuto;
            g_app->applyDsp();
            break;
        case C_PRESETS:
            g_app->showEqPresetsMenu(hwnd, menuPoint(217, 30));
            break;
    }
}

void EqWnd::onCaptureLost() {
    if (slider_ >= 0) g_app->setMarqueeOverride(L"");
    slider_ = -1;
    if (push_.pressed) {
        push_ = PushState();
        redraw();
    }
}

void EqWnd::onRightClick(int x, int y) {
    int s = sliderAt(x, y);
    if (s >= 0) {
        setSlider(s, 0);
        g_app->applyDsp(true);
        return;
    }
    if (hit(x, y) == C_PRESETS || hit(x, y) == C_GRAPH) {
        g_app->showEqPresetsMenu(hwnd, menuPoint(x, y));
        return;
    }
    SkinWnd::onRightClick(x, y);
}

bool EqWnd::onDblClick(int x, int y) {
    int s = sliderAt(x, y);
    if (s >= 0) {
        setSlider(s, 0);
        g_app->applyDsp(true);
        return true;
    }
    return false;
}

void EqWnd::onWheel(int delta, int x, int y) {
    int s = sliderAt(x, y);
    if (s < 0) return;
    setSlider(s, sliderValue(s) + (delta > 0 ? 1.0f : -1.0f));
    g_app->applyDsp(true);
}
