#include "mainwnd.h"

#include <complex>

#include "../app.h"

namespace {

enum {
    C_NONE = 0,
    C_OPTIONS,
    C_MIN,
    C_SHADE,
    C_CLOSE,
    C_PREV,
    C_PLAY,
    C_PAUSE,
    C_STOP,
    C_NEXT,
    C_EJECT,
    C_SHUFFLE,
    C_REPEAT,
    C_EQ,
    C_PL,
    C_POS,
    C_VOL,
    C_BAL,
    C_VIS,
    C_TIME,
    C_MARQUEE,
    C_ABOUT,
    C_CL_O,
    C_CL_A,
    C_CL_I,
    C_CL_D,
    C_CL_V,
};

struct Ctl {
    int id;
    Rc r;
};

const Ctl kNormal[] = {
    {C_OPTIONS, {6, 3, 9, 9}},      {C_MIN, {244, 3, 9, 9}},       {C_SHADE, {254, 3, 9, 9}},
    {C_CLOSE, {264, 3, 9, 9}},      {C_PREV, {16, 88, 23, 18}},    {C_PLAY, {39, 88, 23, 18}},
    {C_PAUSE, {62, 88, 23, 18}},    {C_STOP, {85, 88, 23, 18}},    {C_NEXT, {108, 88, 22, 18}},
    {C_EJECT, {136, 89, 22, 16}},   {C_SHUFFLE, {164, 89, 47, 15}}, {C_REPEAT, {210, 89, 28, 15}},
    {C_EQ, {219, 58, 23, 12}},      {C_PL, {242, 58, 23, 12}},     {C_POS, {16, 72, 248, 10}},
    {C_VOL, {107, 57, 68, 13}},     {C_BAL, {177, 57, 38, 13}},    {C_VIS, {24, 43, 76, 16}},
    {C_TIME, {36, 26, 63, 13}},     {C_MARQUEE, {111, 24, 155, 12}}, {C_ABOUT, {253, 91, 13, 15}},
    {C_CL_O, {10, 25, 8, 8}},       {C_CL_A, {10, 33, 8, 7}},      {C_CL_I, {10, 40, 8, 7}},
    {C_CL_D, {10, 47, 8, 8}},       {C_CL_V, {10, 55, 8, 7}},
};

const Ctl kShade[] = {
    {C_OPTIONS, {6, 3, 9, 9}},   {C_MIN, {244, 3, 9, 9}},    {C_SHADE, {254, 3, 9, 9}},  {C_CLOSE, {264, 3, 9, 9}},
    {C_PREV, {169, 2, 8, 11}},   {C_PLAY, {177, 2, 10, 11}}, {C_PAUSE, {187, 2, 10, 11}}, {C_STOP, {197, 2, 9, 11}},
    {C_NEXT, {206, 2, 8, 11}},   {C_EJECT, {216, 2, 9, 11}}, {C_POS, {226, 4, 17, 7}},   {C_VIS, {79, 5, 38, 5}},
    {C_TIME, {125, 4, 30, 6}},
};

bool IsButton(int id) {
    return id != C_NONE && id != C_POS && id != C_VOL && id != C_BAL && id != C_VIS && id != C_TIME &&
           id != C_MARQUEE;
}

void FFT(std::vector<std::complex<float>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * 3.14159265f / (float)len;
        std::complex<float> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1, 0);
            for (size_t k = 0; k < len / 2; k++) {
                auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

}  // namespace

const Skin::Region* MainWnd::region() const {
    return g_app->mainShade ? &skin().regMainShade : &skin().regMain;
}

int MainWnd::hit(int x, int y) const {
    if (g_app->mainShade) {
        for (auto& c : kShade)
            if (c.r.hit(x, y)) return c.id;
        return C_NONE;
    }
    for (auto& c : kNormal)
        if (c.r.hit(x, y)) return c.id;
    return C_NONE;
}

// ---------------------------------------------------------------------------
void MainWnd::tick() {
    if (++marqueeTick_ % 2 == 0 && !marqueeDrag_) marqueePx_++;
    updateVis();
    redraw();
}

void MainWnd::updateVis() {
    const int N = 1024;
    static float samples[N];
    bool playing = g_app->player.visSamples(samples, N);
    if (!playing || g_app->visMode == VIS_OFF) {
        for (int i = 0; i < 76; i++) {
            bars_[i] = std::max(0.0f, bars_[i] - 1.0f);
            peakVel_[i] += 0.06f;
            peaks_[i] = std::max(0.0f, peaks_[i] - peakVel_[i]);
            osc_[i] = 0;
        }
        return;
    }
    if (g_app->visMode == VIS_OSC) {
        int step = 6, start = N - 76 * step;
        for (int i = 0; i < 76; i++) osc_[i] = samples[start + i * step];
        return;
    }
    static std::vector<std::complex<float>> buf(N);
    for (int i = 0; i < N; i++) {
        float w = 0.5f - 0.5f * std::cos(2 * 3.14159265f * i / (N - 1));
        buf[i] = std::complex<float>(samples[i] * w, 0);
    }
    FFT(buf);
    int sr = std::max(8000, g_app->player.sampleRate());
    for (int b = 0; b < 19; b++) {
        double f0 = 40.0 * std::pow(18000.0 / 40.0, b / 19.0);
        double f1 = 40.0 * std::pow(18000.0 / 40.0, (b + 1) / 19.0);
        int i0 = std::max(1, (int)(f0 * N / sr));
        int i1 = std::max(i0 + 1, (int)(f1 * N / sr));
        i1 = std::min(i1, N / 2);
        float m = 0;
        for (int i = i0; i < i1; i++) m = std::max(m, std::abs(buf[i]) / (N / 4.0f));
        float db = 20 * std::log10(m + 1e-9f);
        float h = (db + 54.0f) / 50.0f * 16.0f + b * 0.12f;
        h = Clamp(h, 0.0f, 16.0f);
        bars_[b] = std::max(h, bars_[b] - 1.1f);
        if (bars_[b] >= peaks_[b]) {
            peaks_[b] = bars_[b];
            peakVel_[b] = 0;
        } else {
            peakVel_[b] += 0.035f;
            peaks_[b] = std::max(0.0f, peaks_[b] - peakVel_[b]);
        }
    }
}

// ---------------------------------------------------------------------------
void MainWnd::paint(Canvas& c) {
    if (g_app->mainShade)
        paintShade(c);
    else
        paintNormal(c);
}

void MainWnd::paintVis(Canvas& c, int x0, int y0, int w, int h) {
    const Skin& sk = skin();
    c.fill(x0, y0, w, h, sk.vis[0]);
    const int s = c.s;
    const int dot = std::max(1, s / 2);
    for (int y = 1; y < h; y += 2)
        for (int x = 1; x < w; x += 2) c.fillPhys((x0 + x) * s + (s - dot) / 2, (y0 + y) * s + (s - dot) / 2, dot, dot, sk.vis[1]);
    int mode = g_app->visMode;
    if (mode == VIS_OFF) return;
    if (mode == VIS_SPECTRUM) {
        int barW = w >= 76 ? 3 : 1, stride = barW + 1;
        for (int b = 0; b < 19 && b * stride < w; b++) {
            int bh = (int)std::lround(bars_[b] * h / 16.0f);
            for (int k = 0; k < bh; k++) {
                int yy = h - 1 - k;
                int ci = 2 + Clamp(yy * 16 / h, 0, 15);
                c.fill(x0 + b * stride, y0 + yy, barW, 1, sk.vis[ci]);
            }
            int ph = (int)std::lround(peaks_[b] * h / 16.0f);
            if (ph > 0 && h >= 16) c.fill(x0 + b * stride, y0 + h - ph, barW, 1, sk.vis[23]);
        }
    } else {
        // drawn in physical pixels so the line stays smooth at any zoom
        c.setClip(x0, y0, w, h);
        int prev = -1;
        const int pwid = w * s;
        for (int i = 0; i < pwid; i++) {
            float t = (float)i / pwid * 75.0f;
            int k = std::min(74, (int)t);
            float v = osc_[k] + (osc_[k + 1] - osc_[k]) * (t - k);
            float yl = Clamp(h / 2.0f - v * h * 0.9f, 0.0f, h - 0.01f);
            int py = (int)((y0 + yl) * s);
            int dist = (int)(std::fabs(yl - h / 2.0f) * 8 / std::max(1, h));
            c.spanPhys(x0 * s + i, prev < 0 ? py : prev, py, std::max(1, s / 2), sk.vis[18 + Clamp(dist, 0, 4)]);
            prev = py;
        }
        c.resetClip();
    }
}

void MainWnd::paintMarquee(Canvas& c) {
    const Skin& sk = skin();
    std::wstring ov = g_app->marqueeOverride();
    std::string txt = ToSkinText(ov.empty() ? g_app->currentTitleLine() : ov);
    c.setClip(111, 27, 154, 6);
    int w = (int)txt.size() * 5;
    if (!ov.empty() || w <= 154) {
        sk.drawText(c, txt, 111, 27);
        if (w < 154) sk.drawText(c, std::string((154 - w) / 5 + 1, ' '), 111 + w, 27);
    } else {
        std::string full = txt + "  ***  ";
        int fw = (int)full.size() * 5;
        int off = ((marqueePx_ % fw) + fw) % fw;
        sk.drawText(c, full, 111 - off, 27);
        sk.drawText(c, full, 111 - off + fw, 27);
    }
    c.resetClip();
}

void MainWnd::paintNormal(Canvas& c) {
    const Skin& sk = skin();
    const Image& tb = sk.img(SB_TITLEBAR);
    const PlayState st = g_app->state();
    c.blit(sk.img(SB_MAIN), 0, 0, 275, 116, 0, 0);
    // title bar + buttons
    c.blit(tb, 27, active ? 0 : 15, 275, 14, 0, 0);
    c.blit(tb, 0, push_.is(C_OPTIONS) ? 9 : 0, 9, 9, 6, 3);
    c.blit(tb, 9, push_.is(C_MIN) ? 9 : 0, 9, 9, 244, 3);
    c.blit(tb, push_.is(C_SHADE) ? 9 : 0, 18, 9, 9, 254, 3);
    c.blit(tb, 18, push_.is(C_CLOSE) ? 9 : 0, 9, 9, 264, 3);
    // clutter bar
    c.blit(tb, 304, 0, 8, 43, 10, 22);
    if (push_.is(C_CL_O)) c.blit(tb, 304, 47, 8, 8, 10, 25);
    if (g_app->alwaysOnTop || push_.is(C_CL_A)) c.blit(tb, 312, 55, 8, 7, 10, 33);
    if (push_.is(C_CL_I)) c.blit(tb, 320, 62, 8, 7, 10, 40);
    if (g_app->zoom > 100 || push_.is(C_CL_D)) c.blit(tb, 328, 69, 8, 8, 10, 47);
    if (g_app->visible[W_VIS] || push_.is(C_CL_V)) c.blit(tb, 336, 77, 8, 7, 10, 55);
    // play status
    const Image& pp = sk.img(SB_PLAYPAUS);
    if (st == PlayState::Playing) {
        c.blit(pp, 0, 0, 9, 9, 26, 28);
        c.blit(pp, 39, 0, 3, 9, 24, 28);
    } else if (st == PlayState::Paused) {
        c.blit(pp, 9, 0, 9, 9, 26, 28);
        c.blit(pp, 36, 0, 3, 9, 24, 28);
    } else {
        c.blit(pp, 18, 0, 9, 9, 26, 28);
        c.blit(pp, 36, 0, 3, 9, 24, 28);
    }
    // time
    bool blinkOff = st == PlayState::Paused && (GetTickCount64() / 500) % 2 == 1;
    if (st != PlayState::Stopped && !blinkOff) {
        double pos = g_app->position(), dur = g_app->duration();
        double t = g_app->timeRemaining && dur > 0 ? std::max(0.0, dur - pos) : pos;
        int total = (int)t;
        int m = std::min(99, total / 60), s = total % 60;
        const Image& num = sk.img(sk.useNumsEx ? SB_NUMS_EX : SB_NUMBERS);
        if (g_app->timeRemaining && dur > 0) {
            if (sk.useNumsEx)
                c.blit(num, 99, 0, 9, 13, 36, 26);
            else
                c.blit(num, 20, 6, 5, 1, 39, 32);
        }
        int dg[4] = {m / 10, m % 10, s / 10, s % 10};
        const int xs[4] = {48, 60, 78, 90};
        for (int i = 0; i < 4; i++) c.blit(num, dg[i] * 9, 0, 9, 13, xs[i], 26);
    }
    // visualisation
    paintVis(c, 24, 43, 76, 16);
    // marquee, kbps, khz
    paintMarquee(c);
    if (st != PlayState::Stopped) {
        int br = g_app->player.bitrate();
        const Track* tr = g_app->currentTrack();
        if (br <= 0 && tr) br = tr->bitrate;
        if (br > 0) {
            std::string s = std::to_string(br);
            sk.drawText(c, s, 111 + 15 - (int)s.size() * 5, 43);
        }
        int sr = g_app->player.sampleRate();
        if (sr > 0) {
            std::string s = std::to_string((sr + 500) / 1000);
            sk.drawText(c, s, 156 + 10 - (int)s.size() * 5, 43);
        }
    }
    // mono / stereo
    const Image& ms = sk.img(SB_MONOSTER);
    int ch = st == PlayState::Stopped ? 0 : g_app->player.channels();
    c.blit(ms, 29, ch == 1 ? 0 : 12, 27, 12, 212, 41);
    c.blit(ms, 0, ch >= 2 ? 0 : 12, 29, 12, 239, 41);
    // volume
    float vol = slider_ == C_VOL ? sliderVal_ : g_app->dsp.volume;
    int vf = (int)std::lround(vol * 27);
    const Image& vimg = sk.img(SB_VOLUME);
    c.blit(vimg, 0, vf * 15, 68, 13, 107, 57);
    if (vimg.lh() >= 433) c.blit(vimg, slider_ == C_VOL ? 0 : 15, 422, 14, 11, 107 + (int)std::lround(vol * 54), 58);
    // balance
    float bal = slider_ == C_BAL ? sliderVal_ * 2 - 1 : g_app->dsp.balance;
    int bf = (int)std::lround(std::fabs(bal) * 27);
    const Image& bimg = sk.img(SB_BALANCE);
    c.blit(bimg, 9, bf * 15, 38, 13, 177, 57);
    if (bimg.lh() >= 433) c.blit(bimg, slider_ == C_BAL ? 0 : 15, 422, 14, 11, 177 + (int)std::lround((bal + 1) / 2 * 24), 58);
    // EQ / PL toggle buttons
    const Image& sr = sk.img(SB_SHUFREP);
    bool eqOn = g_app->visible[W_EQ], plOn = g_app->visible[W_PL];
    c.blit(sr, push_.is(C_EQ) ? 46 : 0, eqOn ? 73 : 61, 23, 12, 219, 58);
    c.blit(sr, push_.is(C_PL) ? 69 : 23, plOn ? 73 : 61, 23, 12, 242, 58);
    // position bar
    double dur = g_app->duration();
    if (st != PlayState::Stopped && dur > 0) {
        const Image& pb = sk.img(SB_POSBAR);
        c.blit(pb, 0, 0, 248, 10, 16, 72);
        double p = slider_ == C_POS ? sliderVal_ : Clamp(g_app->position() / dur, 0.0, 1.0);
        c.blit(pb, slider_ == C_POS ? 278 : 248, 0, 29, 10, 16 + (int)std::lround(p * 219), 72);
    }
    // transport buttons
    const Image& cb = sk.img(SB_CBUTTONS);
    c.blit(cb, 0, push_.is(C_PREV) ? 18 : 0, 23, 18, 16, 88);
    c.blit(cb, 23, push_.is(C_PLAY) ? 18 : 0, 23, 18, 39, 88);
    c.blit(cb, 46, push_.is(C_PAUSE) ? 18 : 0, 23, 18, 62, 88);
    c.blit(cb, 69, push_.is(C_STOP) ? 18 : 0, 23, 18, 85, 88);
    c.blit(cb, 92, push_.is(C_NEXT) ? 18 : 0, 22, 18, 108, 88);
    c.blit(cb, 114, push_.is(C_EJECT) ? 16 : 0, 22, 16, 136, 89);
    // shuffle / repeat
    int shy = (g_app->shuffle ? 30 : 0) + (push_.is(C_SHUFFLE) ? 15 : 0);
    int rey = (g_app->repeat ? 30 : 0) + (push_.is(C_REPEAT) ? 15 : 0);
    c.blit(sr, 28, shy, 47, 15, 164, 89);
    c.blit(sr, 0, rey, 28, 15, 210, 89);
    g_app->anim.draw(c, W_MAIN);
}

void MainWnd::paintShade(Canvas& c) {
    const Skin& sk = skin();
    const Image& tb = sk.img(SB_TITLEBAR);
    const PlayState st = g_app->state();
    c.blit(tb, 27, active ? 29 : 42, 275, 14, 0, 0);
    c.blit(tb, 0, push_.is(C_OPTIONS) ? 9 : 0, 9, 9, 6, 3);
    c.blit(tb, 9, push_.is(C_MIN) ? 9 : 0, 9, 9, 244, 3);
    c.blit(tb, push_.is(C_SHADE) ? 9 : 0, 27, 9, 9, 254, 3);
    c.blit(tb, 18, push_.is(C_CLOSE) ? 9 : 0, 9, 9, 264, 3);
    if (st != PlayState::Stopped) {
        double pos = g_app->position(), dur = g_app->duration();
        bool rem = g_app->timeRemaining && dur > 0;
        double t = rem ? std::max(0.0, dur - pos) : pos;
        int total = (int)t;
        char buf[16];
        sprintf_s(buf, "%c%02d:%02d", rem ? '-' : ' ', std::min(99, total / 60), total % 60);
        sk.drawText(c, buf, 125, 4, true);
        if (dur > 0) {
            c.blit(tb, 0, 36, 17, 7, 226, 4);
            double p = slider_ == C_POS ? sliderVal_ : Clamp(pos / dur, 0.0, 1.0);
            int tx = p <= 0.01 ? 17 : (p >= 0.99 ? 23 : 20);
            c.blit(tb, tx, 36, 3, 7, 226 + (int)std::lround(p * 14), 4);
        }
    }
    if (g_app->visMode != VIS_OFF) paintVis(c, 79, 5, 38, 5);
}

// ---------------------------------------------------------------------------
void MainWnd::sliderUpdate(int x) {
    double dur = g_app->duration();
    switch (slider_) {
        case C_POS:
            if (g_app->mainShade)
                sliderVal_ = Clamp((x - 226 - 1) / 14.0f, 0.0f, 1.0f);
            else
                sliderVal_ = Clamp((x - 16 - 14) / 219.0f, 0.0f, 1.0f);
            g_app->setMarqueeOverride(Fmt(L"SEEK TO: %s/%s (%d%%)", FormatTime(sliderVal_ * dur).c_str(),
                                          FormatTime(dur).c_str(), (int)std::lround(sliderVal_ * 100)));
            break;
        case C_VOL:
            sliderVal_ = Clamp((x - 107 - 7) / 54.0f, 0.0f, 1.0f);
            g_app->setVolume(sliderVal_);
            break;
        case C_BAL: {
            sliderVal_ = Clamp((x - 177 - 7) / 24.0f, 0.0f, 1.0f);
            float b = sliderVal_ * 2 - 1;
            if (std::fabs(b) < 0.1f) {
                b = 0;
                sliderVal_ = 0.5f;
            }
            g_app->setBalance(b);
            break;
        }
    }
    redraw();
}

void MainWnd::onMouseDown(int x, int y, WPARAM) {
    int id = hit(x, y);
    switch (id) {
        case C_POS:
            if (g_app->state() == PlayState::Stopped || g_app->duration() <= 0) {
                if (!g_app->mainShade) startWindowDrag();
                return;
            }
            // fallthrough
        case C_VOL:
        case C_BAL:
            slider_ = id;
            sliderUpdate(x);
            return;
        case C_VIS:
            g_app->visMode = (g_app->visMode + 1) % VIS_COUNT;
            redraw();
            return;
        case C_TIME:
            g_app->timeRemaining = !g_app->timeRemaining;
            g_app->redrawAll();
            return;
        case C_MARQUEE:
            marqueeDrag_ = true;
            marqueeDragX_ = x;
            marqueeDragStart_ = marqueePx_;
            return;
        case C_NONE:
            startWindowDrag();
            return;
        default:
            push_.pressed = id;
            push_.inside = true;
            redraw();
    }
}

void MainWnd::onMouseMove(int x, int y, WPARAM mk) {
    if (!(mk & MK_LBUTTON)) return;
    if (slider_) {
        sliderUpdate(x);
    } else if (marqueeDrag_) {
        marqueePx_ = marqueeDragStart_ - (x - marqueeDragX_);
        redraw();
    } else if (push_.pressed) {
        bool in = hit(x, y) == push_.pressed;
        if (in != push_.inside) {
            push_.inside = in;
            redraw();
        }
    }
}

void MainWnd::onMouseUp(int x, int y) {
    if (slider_) {
        int s = slider_;
        slider_ = 0;
        if (s == C_POS) g_app->seekTo(sliderVal_ * g_app->duration());
        if (s == C_VOL || s == C_BAL) g_app->saveSettings();
        g_app->setMarqueeOverride(L"");
        redraw();
        return;
    }
    marqueeDrag_ = false;
    int id = push_.pressed;
    bool inside = push_.inside && hit(x, y) == id;
    push_ = PushState();
    redraw();
    if (id && inside) click(id);
}

void MainWnd::onCaptureLost() {
    if (slider_ == C_POS) g_app->setMarqueeOverride(L"");
    slider_ = 0;
    marqueeDrag_ = false;
    if (push_.pressed) {
        push_ = PushState();
        redraw();
    }
}

bool MainWnd::onDblClick(int x, int y) {
    if (y < 14 && hit(x, y) == C_NONE) {
        g_app->setMainShade(!g_app->mainShade);
        return true;
    }
    return false;
}

void MainWnd::onWheel(int delta, int, int) {
    g_app->setVolume(Clamp(g_app->dsp.volume + (delta > 0 ? 0.04f : -0.04f), 0.0f, 1.0f));
}

void MainWnd::click(int id) {
    switch (id) {
        case C_OPTIONS:
        case C_CL_O:
            g_app->showMainMenu(hwnd, menuPoint(id == C_OPTIONS ? 6 : 18, id == C_OPTIONS ? 12 : 25));
            break;
        case C_MIN:
            ShowWindow(hwnd, SW_MINIMIZE);
            break;
        case C_SHADE:
            g_app->setMainShade(!g_app->mainShade);
            break;
        case C_CLOSE:
            g_app->quit();
            break;
        case C_PREV: g_app->prevTrack(); break;
        case C_PLAY: g_app->playPressed(); break;
        case C_PAUSE: g_app->pausePressed(); break;
        case C_STOP: g_app->stopPressed(); break;
        case C_NEXT: g_app->nextTrack(); break;
        case C_EJECT: g_app->openFilesDialog(GetKeyState(VK_SHIFT) < 0); break;
        case C_SHUFFLE: g_app->toggleShuffle(); break;
        case C_REPEAT: g_app->toggleRepeat(); break;
        case C_EQ: g_app->toggleWindow(W_EQ); break;
        case C_PL:
            g_app->toggleWindow(W_PL);
            break;
        case C_ABOUT: g_app->handleCommand(CMD_ABOUT); break;
        case C_CL_A: g_app->setAlwaysOnTop(!g_app->alwaysOnTop); break;
        case C_CL_I: g_app->showFileInfo(g_app->pl.current); break;
        case C_CL_D: {
            static const int steps[] = {100, 150, 200, 300};
            int next = 100;
            for (int z : steps)
                if (z > g_app->zoom) {
                    next = z;
                    break;
                }
            g_app->setZoom(next);
            break;
        }
        case C_CL_V:
            g_app->toggleWindow(W_VIS);
            break;
    }
}

LRESULT MainWnd::onMessage(UINT msg, WPARAM wp, LPARAM lp, bool& handled) {
    LRESULT res = 0;
    handled = g_app->handleMainMessage(msg, wp, lp, res);
    return res;
}
