#include "anim.h"

#include "../app.h"

AnimRuntime::~AnimRuntime() { reset(); }

void AnimRuntime::reset() {
    for (auto& f : fonts_) DeleteObject(f.second);
    fonts_.clear();
    phase_.clear();
    level_.clear();
    scroll_.clear();
}

HFONT AnimRuntime::font(const AnimElem& e, int scale) {
    std::wstring key = e.font + L"|" + std::to_wstring(e.size * scale) + (e.bold ? L"b" : L"");
    auto it = fonts_.find(key);
    if (it != fonts_.end()) return it->second;
    HFONT f = CreateFontW(-e.size * scale, 0, 0, 0, e.bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, e.font.c_str());
    fonts_[key] = f;
    return f;
}

void AnimRuntime::update() {
    const Skin& sk = g_app->skin;
    const size_t n = sk.anims.size();
    if (!n) return;
    if (phase_.size() != n) {
        phase_.assign(n, 0);
        level_.assign(n, 0);
        scroll_.assign(n, 0);
    }
    ULONGLONG now = GetTickCount64();
    float dt = last_ ? Clamp((now - last_) / 1000.0f, 0.0f, 0.2f) : 0.03f;
    last_ = now;
    PlayState st = g_app->state();
    bool playing = st == PlayState::Playing;

    bool needAudio = false;
    for (auto& e : sk.anims)
        if (e.mode == AM_SCOPE || e.mode == AM_SPECTRUM || (e.mode == AM_LEVEL && e.channel >= 3)) needAudio = true;
    if (needAudio) {
        bool pl = g_app->player.visSamples(samples_.data(), (int)samples_.size());
        an_.update(samples_.data(), (int)samples_.size(), g_app->player.sampleRate(), pl, audio_);
    }
    float l = 0, r = 0;
    g_app->player.levels(l, r);
    if (!playing) l = r = 0;
    levelL_ = l;
    levelR_ = r;

    for (size_t i = 0; i < n; i++) {
        const AnimElem& e = sk.anims[i];
        switch (e.mode) {
            case AM_SPIN:
                if (playing) phase_[i] += e.fps * dt;
                break;
            case AM_LOOP:
                phase_[i] += e.fps * dt;
                break;
            case AM_LEVEL: {
                float target = 0;
                switch (e.channel) {
                    case 1: target = levelL_; break;
                    case 2: target = levelR_; break;
                    case 3: target = audio_.bass; break;
                    case 4: target = audio_.mid; break;
                    case 5: target = audio_.treb; break;
                    default: target = (levelL_ + levelR_) * 0.5f; break;
                }
                if (e.channel <= 2) {
                    // RMS -> meter scale (dB-ish so quiet music still moves the needle)
                    float db = 20 * std::log10(std::max(1e-5f, target * e.gain));
                    target = Clamp((db + 36.0f) / 39.0f, 0.0f, 1.0f);
                } else {
                    target = Clamp(target * e.gain, 0.0f, 1.0f);
                }
                if (!playing) target = 0;
                float k = target > level_[i] ? e.attack : e.release;
                level_[i] += (target - level_[i]) * std::min(1.0f, k * dt * 33.0f);
                break;
            }
            case AM_TEXT:
                scroll_[i] += dt * 22.0f;  // skin pixels per second
                break;
            default:
                break;
        }
    }
}

static bool WhenOk(int when, PlayState st) {
    switch (when) {
        case AW_PLAYING: return st == PlayState::Playing;
        case AW_PAUSED: return st == PlayState::Paused;
        case AW_STOPPED: return st == PlayState::Stopped;
        case AW_ACTIVE: return st != PlayState::Stopped;
        default: return true;
    }
}

void AnimRuntime::draw(Canvas& c, int window) {
    const Skin& sk = g_app->skin;
    if (sk.anims.empty()) return;
    if (phase_.size() != sk.anims.size()) update();
    PlayState st = g_app->state();
    const int s = c.s;
    for (size_t i = 0; i < sk.anims.size(); i++) {
        const AnimElem& e = sk.anims[i];
        if (e.window != window || !WhenOk(e.when, st)) continue;
        auto blitFrame = [&](int frame, int dx, int dy) {
            const Image& img = sk.animImage(e.image);
            if (!img.valid()) return;
            frame = Clamp(frame, 0, e.frames - 1);
            int col = frame % e.cols, row = frame / e.cols;
            c.blit(img, e.sx + col * e.fw, e.sy + row * e.fh, e.fw, e.fh, dx, dy);
        };
        switch (e.mode) {
            case AM_SPIN:
            case AM_LOOP:
                blitFrame(((int)phase_[i]) % e.frames, e.x, e.y);
                break;
            case AM_LEVEL:
                blitFrame((int)std::lround(level_[i] * (e.frames - 1)), e.x, e.y);
                break;
            case AM_STATE:
                blitFrame(st == PlayState::Stopped ? 0 : st == PlayState::Playing ? 1 : 2, e.x, e.y);
                break;
            case AM_PROGRESS: {
                double dur = g_app->duration();
                double p = st != PlayState::Stopped && dur > 0 ? Clamp(g_app->position() / dur, 0.0, 1.0) : 0.0;
                if (e.frames > 1)
                    blitFrame((int)std::lround(p * (e.frames - 1)), e.x, e.y);
                else
                    blitFrame(0, e.x + (int)std::lround(p * std::max(0, e.w - e.fw)), e.y);
                break;
            }
            case AM_SCOPE: {
                c.setClip(e.x, e.y, e.w, e.h);
                int prev = -1;
                int pw = e.w * s;
                for (int px = 0; px < pw; px++) {
                    float t = (float)px / pw * (VisAudio::kWave - 1);
                    int k = std::min(VisAudio::kWave - 2, (int)t);
                    float v = audio_.wave[k] + (audio_.wave[k + 1] - audio_.wave[k]) * (t - k);
                    int py = (int)((e.y + e.h / 2.0f - v * e.h * 0.45f * e.gain) * s);
                    c.spanPhys(e.x * s + px, prev < 0 ? py : prev, py, std::max(1, s / 2), e.color);
                    prev = py;
                }
                c.resetClip();
                break;
            }
            case AM_SPECTRUM: {
                int bars = e.bars > 0 ? e.bars : std::max(4, e.w / 3);
                float bw = (float)e.w * s / bars;
                c.setClip(e.x, e.y, e.w, e.h);
                for (int b = 0; b < bars; b++) {
                    int b0 = b * VisAudio::kSpec / bars, b1 = std::max(b0 + 1, (b + 1) * VisAudio::kSpec / bars);
                    float v = 0;
                    for (int q = b0; q < b1; q++) v = std::max(v, audio_.spec[q]);
                    v = Clamp(v * e.gain, 0.0f, 1.0f);
                    int hgt = (int)(v * e.h * s);
                    int x0 = (int)(e.x * s + b * bw + bw * 0.15f), x1 = (int)(e.x * s + (b + 1) * bw - bw * 0.15f);
                    int base = (e.y + e.h) * s;
                    for (int y = 0; y < hgt; y++) {
                        float f = (float)y / std::max(1, e.h * s);
                        c.fillPhys(x0, base - 1 - y, std::max(1, x1 - x0), 1, BlendColor(e.color2, e.color, f));
                    }
                }
                c.resetClip();
                break;
            }
            case AM_TEXT: {
                std::wstring txt;
                double pos = g_app->position(), dur = g_app->duration();
                const Track* t = g_app->currentTrack();
                switch (e.textKind) {
                    case AT_TITLE: txt = t ? t->display() : std::wstring(APP_NAME); break;
                    case AT_TIME: txt = st == PlayState::Stopped ? L"--:--" : FormatTime(pos); break;
                    case AT_REMAIN: txt = st == PlayState::Stopped || dur <= 0 ? L"--:--" : L"-" + FormatTime(dur - pos); break;
                    case AT_BITRATE: txt = Fmt(L"%d kbps", g_app->player.bitrate()); break;
                    case AT_SAMPLERATE: txt = Fmt(L"%.1f kHz", g_app->player.sampleRate() / 1000.0); break;
                    case AT_TRACK: txt = Fmt(L"%02d", g_app->pl.current + 1); break;
                    case AT_CLOCK: {
                        SYSTEMTIME tm;
                        GetLocalTime(&tm);
                        txt = Fmt(L"%02d:%02d", tm.wHour, tm.wMinute);
                        break;
                    }
                    case AT_COUNTER: txt = Fmt(L"%03d", (int)(pos * 1.6) % 1000); break;
                    default: txt = e.text; break;
                }
                HFONT f = font(e, s);
                int tw = c.textWidth(txt, f);  // skin pixels
                if (e.align == 0 && tw > e.w) {
                    int gap = std::max(20, e.w / 3);
                    int period = tw + gap;
                    int off = (int)scroll_[i] % period;
                    c.text(txt, e.x - off, e.y, tw + 2, e.h, f, e.color, DT_LEFT | DT_SINGLELINE | DT_VCENTER, e.x, e.w);
                    c.text(txt, e.x - off + period, e.y, tw + 2, e.h, f, e.color, DT_LEFT | DT_SINGLELINE | DT_VCENTER, e.x,
                           e.w);
                } else {
                    UINT al = e.align == 1 ? DT_CENTER : e.align == 2 ? DT_RIGHT : DT_LEFT;
                    c.text(txt, e.x, e.y, e.w, e.h, f, e.color, al | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                }
                break;
            }
        }
    }
}
