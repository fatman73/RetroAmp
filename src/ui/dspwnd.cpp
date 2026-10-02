#include "dspwnd.h"

#include "../app.h"

namespace {
enum { S_BOOST, S_FREQ, S_SUB, S_HARM, S_WIDTH, S_COUNT };
const char* kLabels[S_COUNT] = {"BOOST", "FREQ", "SUB", "HARM", "WIDTH"};
const int kSliderY = 32;
int SliderX(int s) { return 22 + s * 26; }

enum { B_NONE = 0, B_CLOSE, B_BASS, B_LOUD, B_LIMIT, B_PRESETS, B_EQ };
const Rc kBass = {152, 24, 104, 12};
const Rc kLoud = {152, 39, 104, 12};
const Rc kLimit = {152, 54, 104, 12};
const Rc kPresets = {152, 69, 104, 12};
}  // namespace

int DspWnd::sliderAt(int x, int y) const {
    if (y < kSliderY || y >= kSliderY + 63) return -1;
    for (int s = 0; s < S_COUNT; s++)
        if (x >= SliderX(s) - 2 && x < SliderX(s) + 16) return s;
    return -1;
}

int DspWnd::buttonAt(int x, int y) const {
    if (Rc{lw - 11, 3, 9, 9}.hit(x, y)) return B_CLOSE;
    if (kBass.hit(x, y)) return B_BASS;
    if (kLoud.hit(x, y)) return B_LOUD;
    if (kLimit.hit(x, y)) return B_LIMIT;
    if (kPresets.hit(x, y)) return B_PRESETS;
    return B_NONE;
}

float DspWnd::getP(int s) const {
    const DspParams& d = g_app->dsp;
    switch (s) {
        case S_BOOST: return d.bassBoost / 18.0f;
        case S_FREQ: return (float)(std::log(d.bassFreq / 30.0) / std::log(250.0 / 30.0));
        case S_SUB: return d.subBoost / 12.0f;
        case S_HARM: return d.harmonics;
        case S_WIDTH: return d.width / 2.0f;
    }
    return 0;
}

void DspWnd::setP(int s, float p) {
    p = Clamp(p, 0.0f, 1.0f);
    DspParams& d = g_app->dsp;
    switch (s) {
        case S_BOOST: d.bassBoost = std::round(p * 18 * 2) / 2; break;
        case S_FREQ: d.bassFreq = (float)std::round(30.0 * std::pow(250.0 / 30.0, p)); break;
        case S_SUB: d.subBoost = std::round(p * 12 * 2) / 2; break;
        case S_HARM: d.harmonics = std::round(p * 100) / 100; break;
        case S_WIDTH: {
            float w = std::round(p * 200) / 100;
            if (std::fabs(w - 1) < 0.04f) w = 1;
            d.width = w;
            break;
        }
    }
    g_app->applyDsp(false);
}

std::string DspWnd::valueText(int s) const {
    const DspParams& d = g_app->dsp;
    char b[32];
    switch (s) {
        case S_BOOST: sprintf_s(b, "+%.0f", d.bassBoost); break;
        case S_FREQ: sprintf_s(b, "%.0f", d.bassFreq); break;
        case S_SUB: sprintf_s(b, "+%.0f", d.subBoost); break;
        case S_HARM: sprintf_s(b, "%.0f%%", d.harmonics * 100); break;
        default: sprintf_s(b, "%.0f%%", d.width * 100); break;
    }
    return b;
}

void DspWnd::drawBox(Canvas& c, int x, int y, int w, int h, const std::string& text, bool on, bool pressed) {
    const Skin& sk = skin();
    uint32_t border = sk.plNormal;
    c.fill(x, y, w, h, on ? sk.plSelectedBG : sk.plNormalBG);
    if (pressed) c.fill(x, y, w, h, BlendColor(sk.plSelectedBG, sk.plNormal, 0.3f));
    c.frame(x, y, w, h, border);
    int tw = (int)text.size() * 5;
    sk.drawText(c, text, x + (w - tw) / 2 + (pressed ? 1 : 0), y + (h - 6) / 2 + (pressed ? 1 : 0), true);
}

void DspWnd::paint(Canvas& c) {
    const Skin& sk = skin();
    const Image& pe = sk.img(SB_PLEDIT);
    const Image& eq = sk.img(SB_EQMAIN);
    const DspParams& d = g_app->dsp;
    const int W = lw, H = lh, ty = active ? 0 : 21;
    // frame built from playlist pieces
    c.tile(pe, 127, ty, 25, 20, 25, 0, W - 50, 20);
    c.blit(pe, 0, ty, 25, 20, 0, 0);
    c.blit(pe, 153, ty, 25, 20, W - 25, 0);
    if (push_.is(B_CLOSE)) c.blit(pe, 52, 42, 9, 9, W - 11, 3);
    for (int y = 20; y < H - 10; y += 29) {
        c.setClip(0, 20, W, H - 30);
        c.blit(pe, 0, 42, 12, 29, 0, y);
        c.blit(pe, 0, 42, 12, 29, W - 12, y, true);
        c.resetClip();
    }
    c.tile(pe, 179, 28, 25, 10, 0, H - 10, W, 10);
    c.blit(pe, 0, 72 + 28, 12, 10, 0, H - 10);
    c.blit(pe, 126 + 138, 72 + 28, 12, 10, W - 12, H - 10);
    c.fill(12, 20, W - 24, H - 30, sk.plNormalBG);
    // title
    std::string title = "BASS BOOST & EFFECTS";
    int tw = (int)title.size() * 5 + 8;
    c.fill((W - tw) / 2, 4, tw, 10, sk.textBackground());
    sk.drawText(c, title, (W - tw) / 2 + 4, 6);

    // sliders
    for (int s = 0; s < S_COUNT; s++) {
        int x = SliderX(s);
        float p = Clamp(getP(s), 0.0f, 1.0f);
        int frame = (int)std::lround(p * 27);
        c.blit(eq, 13 + (frame % 14) * 15, 164 + (frame / 14) * 65, 14, 63, x, kSliderY);
        c.blit(eq, 0, slider_ == s ? 176 : 164, 11, 11, x + 1, kSliderY + (int)std::lround((1 - p) * 51));
        std::string v = valueText(s);
        sk.drawText(c, v, x + 7 - (int)v.size() * 5 / 2, 24, true);
        std::string l = kLabels[s];
        sk.drawText(c, l, x + 7 - (int)l.size() * 5 / 2, 98, true);
    }
    if (!d.bassOn) {
        // dim the bass sliders when the section is bypassed
        for (int s = 0; s < S_HARM + 1; s++)
            for (int y = kSliderY; y < kSliderY + 63; y += 2) c.fill(SliderX(s), y, 14, 1, sk.plNormalBG);
    }

    drawBox(c, kBass.x, kBass.y, kBass.w, kBass.h, d.bassOn ? "BASS: ON" : "BASS: OFF", d.bassOn, push_.is(B_BASS));
    drawBox(c, kLoud.x, kLoud.y, kLoud.w, kLoud.h, d.loudness ? "LOUDNESS: ON" : "LOUDNESS: OFF", d.loudness,
            push_.is(B_LOUD));
    drawBox(c, kLimit.x, kLimit.y, kLimit.w, kLimit.h, d.limiter ? "LIMITER: ON" : "LIMITER: OFF", d.limiter,
            push_.is(B_LIMIT));
    drawBox(c, kPresets.x, kPresets.y, kPresets.w, kPresets.h, "PRESETS...", false, push_.is(B_PRESETS));

    // info line
    std::string info;
    int hs = slider_ >= 0 ? slider_ : hoverSlider_;
    switch (hs) {
        case S_BOOST: info = "LOW SHELF " + valueText(S_BOOST) + "DB"; break;
        case S_FREQ: info = "CORNER " + valueText(S_FREQ) + "HZ"; break;
        case S_SUB: info = "SUB BASS " + valueText(S_SUB) + "DB"; break;
        case S_HARM: info = "HARMONICS " + valueText(S_HARM); break;
        case S_WIDTH: info = "STEREO WIDTH " + valueText(S_WIDTH); break;
        default: {
            float total = d.bassOn ? d.bassBoost + d.subBoost : 0;
            char b[48];
            sprintf_s(b, "BASS +%.0fDB @ %.0fHZ", total, d.bassFreq);
            info = b;
        }
    }
    c.setClip(150, 86, 108, 18);
    sk.drawText(c, info.substr(0, 21), 153, 88, true);
    sk.drawText(c, d.limiter ? "CLIP GUARD ACTIVE" : "NO CLIP GUARD", 153, 96, true);
    c.resetClip();
}

void DspWnd::onMouseDown(int x, int y, WPARAM) {
    int s = sliderAt(x, y);
    if (s >= 0) {
        slider_ = s;
        onMouseMove(x, y, MK_LBUTTON);
        return;
    }
    int b = buttonAt(x, y);
    if (b == B_NONE) {
        startWindowDrag();
        return;
    }
    push_.pressed = b;
    push_.inside = true;
    redraw();
}

void DspWnd::onMouseMove(int x, int y, WPARAM mk) {
    if (!(mk & MK_LBUTTON)) {
        int hs = sliderAt(x, y);
        if (hs != hoverSlider_) {
            hoverSlider_ = hs;
            redraw();
        }
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        return;
    }
    if (slider_ >= 0) {
        setP(slider_, 1.0f - (y - kSliderY - 5) / 51.0f);
        redraw();
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

void DspWnd::onMouseUp(int x, int y) {
    if (slider_ >= 0) {
        slider_ = -1;
        g_app->applyDsp(true);
        g_app->saveSettings();
        redraw();
        return;
    }
    int id = push_.pressed;
    bool inside = push_.inside && buttonAt(x, y) == id;
    push_ = PushState();
    redraw();
    if (!id || !inside) return;
    DspParams& d = g_app->dsp;
    switch (id) {
        case B_CLOSE: g_app->setWindowVisible(W_DSP, false); return;
        case B_BASS: d.bassOn = !d.bassOn; break;
        case B_LOUD: d.loudness = !d.loudness; break;
        case B_LIMIT: d.limiter = !d.limiter; break;
        case B_PRESETS: g_app->showDspPresetsMenu(hwnd, menuPoint(kPresets.x, kPresets.y + kPresets.h)); return;
    }
    g_app->applyDsp(true);
    g_app->saveSettings();
}

void DspWnd::onCaptureLost() {
    slider_ = -1;
    if (push_.pressed) {
        push_ = PushState();
        redraw();
    }
}

bool DspWnd::onDblClick(int x, int y) {
    int s = sliderAt(x, y);
    if (s < 0) return false;
    static const float defaults[S_COUNT] = {0.0f, -1.0f, 0.0f, 0.0f, 0.5f};
    if (s == S_FREQ) {
        g_app->dsp.bassFreq = 80;
        g_app->applyDsp(true);
    } else {
        setP(s, defaults[s]);
    }
    redraw();
    return true;
}

void DspWnd::onWheel(int delta, int x, int y) {
    int s = sliderAt(x, y);
    if (s < 0) return;
    setP(s, getP(s) + (delta > 0 ? 0.03f : -0.03f));
    redraw();
}
