#include "visengine.h"

#include <complex>

static const float kPi = 3.14159265f;

uint32_t Hsv(float h, float s, float v) {
    h = h - std::floor(h);
    float r, g, b;
    float i = std::floor(h * 6), f = h * 6 - i;
    float p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
    switch ((int)i % 6) {
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        default: r = v, g = p, b = q; break;
    }
    return ((uint32_t)(Clamp(r, 0.0f, 1.0f) * 255) << 16) | ((uint32_t)(Clamp(g, 0.0f, 1.0f) * 255) << 8) |
           (uint32_t)(Clamp(b, 0.0f, 1.0f) * 255);
}

static inline uint32_t AddSat(uint32_t a, uint32_t b) {
    uint32_t r = std::min(255u, ((a >> 16) & 255) + ((b >> 16) & 255));
    uint32_t g = std::min(255u, ((a >> 8) & 255) + ((b >> 8) & 255));
    uint32_t bl = std::min(255u, (a & 255) + (b & 255));
    return (r << 16) | (g << 8) | bl;
}

static inline uint32_t Scale(uint32_t c, float k) {
    k = Clamp(k, 0.0f, 1.0f);
    return ((uint32_t)(((c >> 16) & 255) * k) << 16) | ((uint32_t)(((c >> 8) & 255) * k) << 8) | (uint32_t)((c & 255) * k);
}

// ---------------------------------------------------------------------------
static void Fft(std::vector<std::complex<float>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * kPi / (float)len;
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

void VisAnalyzer::update(const float* s, int n, int sr, bool playing, VisAudio& o) {
    ULONGLONG now = GetTickCount64();
    float dt = last_ ? Clamp((now - last_) / 1000.0f, 0.001f, 0.1f) : 0.016f;
    last_ = now;
    time_ += dt;
    o.time = time_;
    o.dt = dt;
    o.playing = playing;
    for (int i = 0; i < VisAudio::kWave; i++) o.wave[i] = playing ? s[n - VisAudio::kWave + i] : 0.0f;
    const int N = 2048;
    static std::vector<std::complex<float>> buf(N);
    for (int i = 0; i < N; i++) {
        float w = 0.5f - 0.5f * std::cos(2 * kPi * i / (N - 1));
        buf[i] = std::complex<float>(playing && i < n ? s[n - N + i] * w : 0.0f, 0);
    }
    Fft(buf);
    sr = std::max(8000, sr);
    float bass = 0, mid = 0, treb = 0;
    int nb = 0, nm = 0, nt = 0;
    for (int b = 0; b < VisAudio::kSpec; b++) {
        double f0 = 30.0 * std::pow(16000.0 / 30.0, (double)b / VisAudio::kSpec);
        double f1 = 30.0 * std::pow(16000.0 / 30.0, (double)(b + 1) / VisAudio::kSpec);
        int i0 = std::max(1, (int)(f0 * N / sr)), i1 = std::min(N / 2, std::max(i0 + 1, (int)(f1 * N / sr)));
        float m = 0;
        for (int i = i0; i < i1; i++) m = std::max(m, std::abs(buf[i]) / (N / 4.0f));
        float db = 20 * std::log10(m + 1e-9f);
        float v = Clamp((db + 62.0f) / 58.0f + b * 0.0015f, 0.0f, 1.0f);
        o.spec[b] = std::max(v, o.spec[b] - dt * 2.2f);  // gentle fall
        double fc = (f0 + f1) / 2;
        if (fc < 160) bass += v, nb++;
        else if (fc < 2500) mid += v, nm++;
        else treb += v, nt++;
    }
    o.bass = nb ? bass / nb : 0;
    o.mid = nm ? mid / nm : 0;
    o.treb = nt ? treb / nt : 0;
    bassAvg_ += (o.bass - bassAvg_) * std::min(1.0f, dt * 1.5f);
    o.bassAvg = bassAvg_;
    o.beat = false;
    if (playing && o.bass > bassAvg_ * 1.18f + 0.04f && o.bass > 0.35f && time_ - lastBeat_ > 0.28) {
        o.beat = true;
        lastBeat_ = (float)time_;
    }
}

// ---------------------------------------------------------------------------
int VisEngine::presetCount() { return 7; }

const wchar_t* VisEngine::presetName(int i) {
    static const wchar_t* names[] = {L"Classic Analyzer", L"Scope Trails", L"Milk Tunnel",
                                     L"Starfield Warp",   L"Plasma",       L"Spectrum Fire", L"Alemiga"};
    return names[Clamp(i, 0, presetCount() - 1)];
}

void VisEngine::resize(int w, int h) {
    w = std::max(16, w);
    h = std::max(16, h);
    if (w == w_ && h == h_) return;
    w_ = w;
    h_ = h;
    buf_.assign((size_t)w * h, 0);
    tmp_.assign((size_t)w * h, 0);
    heat_.assign((size_t)w * (h + 2), 0);
    radial_.resize((size_t)w * h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float dx = (x - w / 2.0f) / h, dy = (y - h / 2.0f) / h;
            radial_[(size_t)y * w + x] = (uint16_t)(std::sqrt(dx * dx + dy * dy) * 1024);
        }
}

void VisEngine::setPreset(int p) {
    preset_ = ((p % presetCount()) + presetCount()) % presetCount();
    std::fill(buf_.begin(), buf_.end(), 0);
    std::fill(heat_.begin(), heat_.end(), 0);
    stars_.clear();
}

void VisEngine::render(const VisAudio& a) {
    if (!w_) return;
    hue_ += a.dt * (0.03f + a.treb * 0.08f);
    switch (preset_) {
        case 0: analyzer(a); break;
        case 1: scopeTrails(a); break;
        case 2: milkTunnel(a); break;
        case 3: starfield(a); break;
        case 4: plasma(a); break;
        case 5: fire(a); break;
        default: boing(a); break;
    }
}

// ---------------------------------------------------------------- helpers
void VisEngine::clear(uint32_t c) { std::fill(buf_.begin(), buf_.end(), c); }

void VisEngine::fade(int r, int g, int b) {
    for (auto& p : buf_) {
        uint32_t R = ((p >> 16) & 255) * r >> 8, G = ((p >> 8) & 255) * g >> 8, B = (p & 255) * b >> 8;
        p = (R << 16) | (G << 8) | B;
    }
}

inline void VisEngine::setPixel(int x, int y, uint32_t c) {
    if ((unsigned)x < (unsigned)w_ && (unsigned)y < (unsigned)h_) buf_[(size_t)y * w_ + x] = c;
}

inline void VisEngine::addPixel(int x, int y, uint32_t c) {
    if ((unsigned)x < (unsigned)w_ && (unsigned)y < (unsigned)h_) {
        uint32_t& d = buf_[(size_t)y * w_ + x];
        d = AddSat(d, c);
    }
}

void VisEngine::line(float x0, float y0, float x1, float y1, uint32_t c, int thick, bool additive) {
    float dx = x1 - x0, dy = y1 - y0;
    int steps = (int)std::max(std::fabs(dx), std::fabs(dy)) + 1;
    int r0 = -(thick - 1) / 2, r1 = thick / 2;
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        int x = (int)(x0 + dx * t), y = (int)(y0 + dy * t);
        for (int oy = r0; oy <= r1; oy++)
            for (int ox = r0; ox <= r1; ox++) {
                if (additive) addPixel(x + ox, y + oy, c);
                else setPixel(x + ox, y + oy, c);
            }
    }
}

void VisEngine::rectFill(int x, int y, int w, int h, uint32_t c) {
    int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(w_, x + w), y1 = std::min(h_, y + h);
    for (int yy = y0; yy < y1; yy++) {
        uint32_t* row = &buf_[(size_t)yy * w_];
        for (int xx = x0; xx < x1; xx++) row[xx] = c;
    }
}

// Feedback: new frame = previous frame zoomed / rotated around the centre (bilinear), faded per channel.
void VisEngine::warp(float zoom, float rot, float dx, float dy, int fr, int fg, int fb, float waveAmp, float waveFreq,
                     float wavePhase) {
    const float cx = w_ / 2.0f, cy = h_ / 2.0f;
    const float iz = 1.0f / zoom, cs = std::cos(rot) * iz, sn = std::sin(rot) * iz;
    const uint32_t* src = buf_.data();
    for (int y = 0; y < h_; y++) {
        float ry = y - cy;
        float wx = waveAmp ? waveAmp * std::sin(y * waveFreq + wavePhase) : 0.0f;
        float u = cx + (-cx) * cs - ry * sn + dx + wx;
        float v = cy + (-cx) * sn + ry * cs + dy;
        uint32_t* dst = &tmp_[(size_t)y * w_];
        for (int x = 0; x < w_; x++, u += cs, v += sn) {
            int ui = (int)std::floor(u), vi = (int)std::floor(v);
            if (ui < 0 || vi < 0 || ui >= w_ - 1 || vi >= h_ - 1) {
                dst[x] = 0;
                continue;
            }
            int fu = (int)((u - ui) * 256), fv = (int)((v - vi) * 256);
            const uint32_t* p = src + (size_t)vi * w_ + ui;
            uint32_t a = p[0], b = p[1], c = p[w_], d = p[w_ + 1];
            int w00 = (256 - fu) * (256 - fv), w10 = fu * (256 - fv), w01 = (256 - fu) * fv, w11 = fu * fv;
            uint32_t R = ((((a >> 16) & 255) * w00 + ((b >> 16) & 255) * w10 + ((c >> 16) & 255) * w01 + ((d >> 16) & 255) * w11) >> 16) * fr >> 8;
            uint32_t G = ((((a >> 8) & 255) * w00 + ((b >> 8) & 255) * w10 + ((c >> 8) & 255) * w01 + ((d >> 8) & 255) * w11) >> 16) * fg >> 8;
            uint32_t B = (((a & 255) * w00 + (b & 255) * w10 + (c & 255) * w01 + (d & 255) * w11) >> 16) * fb >> 8;
            dst[x] = (std::min(R, 255u) << 16) | (std::min(G, 255u) << 8) | std::min(B, 255u);
        }
    }
    buf_.swap(tmp_);
}

// ---------------------------------------------------------------- presets
// 0. Classic analyzer: LED bars, falling peaks, mirror reflection
void VisEngine::analyzer(const VisAudio& a) {
    for (int y = 0; y < h_; y++) {
        uint32_t c = Hsv(0.62f, 0.8f, 0.10f * (1.0f - (float)y / h_));
        std::fill(buf_.begin() + (size_t)y * w_, buf_.begin() + (size_t)(y + 1) * w_, c);
    }
    const int nBars = Clamp(w_ / 12, 12, 48);
    const float base = h_ * 0.78f;
    const float barW = (float)w_ / nBars;
    const int seg = std::max(2, h_ / 45);
    for (int i = 0; i < nBars; i++) {
        int b0 = i * VisAudio::kSpec / nBars, b1 = (i + 1) * VisAudio::kSpec / nBars;
        float v = 0;
        for (int b = b0; b < std::max(b1, b0 + 1); b++) v = std::max(v, a.spec[b]);
        bars_[i] = std::max(v, bars_[i] - a.dt * 1.6f);
        if (bars_[i] >= peaks_[i]) {
            peaks_[i] = bars_[i];
            peakVel_[i] = 0;
        } else {
            peakVel_[i] += a.dt * 0.9f;
            peaks_[i] = std::max(0.0f, peaks_[i] - peakVel_[i] * a.dt);
        }
        int x0 = (int)(i * barW + barW * 0.12f), x1 = (int)((i + 1) * barW - barW * 0.12f);
        int top = (int)(base - bars_[i] * base * 0.95f);
        for (int y = (int)base - seg; y >= top; y -= seg) {
            float f = (base - y) / (base * 0.95f);
            uint32_t c = f < 0.5f ? Hsv(0.33f - f * 0.12f, 0.9f, 0.95f) : f < 0.8f ? Hsv(0.15f - (f - 0.5f) * 0.25f, 0.95f, 1.0f)
                                                                                   : Hsv(0.0f, 0.95f, 1.0f);
            rectFill(x0, y, x1 - x0, seg - 1, c);
            // reflection
            int ry = (int)(2 * base - y);
            if (ry < h_) rectFill(x0, ry - seg + 1, x1 - x0, seg - 1, Scale(c, 0.22f * (1 - (ry - base) / (h_ - base))));
        }
        int py = (int)(base - peaks_[i] * base * 0.95f) - seg;
        rectFill(x0, py, x1 - x0, std::max(1, seg - 1), 0xFFFFFF);
    }
    rectFill(0, (int)base, w_, 1, 0x406080);
}

// 1. AVS-style oscilloscope with glowing trails
void VisEngine::scopeTrails(const VisAudio& a) {
    warp(1.012f + a.bass * 0.03f, 0.0f, 0, -0.6f, 228, 226, 236);
    uint32_t c1 = Hsv(hue_, 0.85f, 1.0f), c2 = Hsv(hue_ + 0.5f, 0.85f, 1.0f);
    if (a.beat) c1 = 0xFFFFFF;
    float px = 0, py = 0, qy = 0;
    int thick = std::max(1, h_ / 140);
    for (int i = 0; i < VisAudio::kWave; i += 2) {
        float x = (float)i / (VisAudio::kWave - 1) * w_;
        float y = h_ * 0.5f + a.wave[i] * h_ * 0.38f;
        float y2 = h_ * 0.5f - a.wave[i] * h_ * 0.20f;
        if (i) {
            line(px, py, x, y, c1, thick + 1, true);
            line(px, qy, x, y2, Scale(c2, 0.5f), thick, true);
        }
        px = x;
        py = y;
        qy = y2;
    }
}

// 2. MilkDrop-like tunnel: zoom + rotation feedback, circular waveform, colour drift
void VisEngine::milkTunnel(const VisAudio& a) {
    spin_ += a.dt * (0.4f + a.bass * 1.8f) * (std::sin((float)a.time * 0.13f) > 0 ? 1 : -1);
    float rot = 0.012f * std::sin((float)a.time * 0.3f) + (a.beat ? 0.05f : 0.0f);
    warp(1.035f + a.bass * 0.04f, rot, 0, 0, 246, 240, 250, 1.2f + a.mid * 2.5f, 0.045f, (float)a.time * 2.0f);
    const float cx = w_ / 2.0f, cy = h_ / 2.0f;
    const float R = h_ * (0.16f + a.bass * 0.10f);
    uint32_t col = Hsv(hue_, 0.75f, 0.95f);
    int thick = std::max(1, h_ / 150) + 1;
    float lx = 0, ly = 0;
    const int N = 200;
    for (int i = 0; i <= N; i++) {
        float ang = (float)i / N * 2 * kPi + spin_;
        float s = a.wave[(i * (VisAudio::kWave - 1) / N)];
        float r = R + s * h_ * 0.14f;
        float x = cx + std::cos(ang) * r * 1.15f, y = cy + std::sin(ang) * r;
        if (i) line(lx, ly, x, y, col, thick, true);
        lx = x;
        ly = y;
    }
    if (a.beat) {
        uint32_t bc = Hsv(hue_ + 0.33f, 0.6f, 1.0f);
        float r = h_ * 0.42f;
        for (int i = 0; i <= 120; i++) {
            float ang = (float)i / 120 * 2 * kPi;
            addPixel((int)(cx + std::cos(ang) * r * 1.15f), (int)(cy + std::sin(ang) * r), bc);
        }
    }
}

// 3. Starfield warp driven by the bass
void VisEngine::starfield(const VisAudio& a) {
    if (stars_.empty()) {
        stars_.resize(900);
        for (auto& s : stars_) s = {frnd() * 2 - 1, frnd() * 2 - 1, frnd()};
    }
    fade(150, 150, 170);
    const float cx = w_ / 2.0f, cy = h_ / 2.0f, scale = h_ * 0.5f;
    float speed = 0.18f + a.bass * 1.4f + (a.beat ? 0.6f : 0.0f);
    rot_ += a.dt * 0.05f * (1 + a.mid);
    float cs = std::cos(rot_), sn = std::sin(rot_);
    for (auto& s : stars_) {
        float oz = s.z;
        s.z -= speed * a.dt * 0.6f;
        if (s.z <= 0.02f) {
            s = {frnd() * 2 - 1, frnd() * 2 - 1, 1.0f};
            continue;
        }
        float x = s.x * cs - s.y * sn, y = s.x * sn + s.y * cs;
        float x0 = cx + x / oz * scale, y0 = cy + y / oz * scale;
        float x1 = cx + x / s.z * scale, y1 = cy + y / s.z * scale;
        float br = Clamp(1.2f - s.z, 0.0f, 1.0f);
        uint32_t c = Hsv(hue_ + s.x * 0.15f, 0.35f, br);
        line(x0, y0, x1, y1, c, s.z < 0.25f ? 2 : 1, true);
    }

    // Pulsing sphere in the centre: fast attack / slow release on the bass, extra kick on beats.
    float target = Clamp((a.bass - 0.18f) / 0.55f, 0.0f, 1.0f) * 0.6f + Clamp((a.bass - a.bassAvg * 0.92f) / 0.12f, 0.0f, 1.0f) * 0.4f;
    pulse_ += (target - pulse_) * std::min(1.0f, a.dt * (target > pulse_ ? 22.0f : 3.5f));
    kick_ = a.beat ? 1.0f : kick_ * std::exp(-a.dt * 5.0f);
    if (a.beat) ring_ = 0;
    ring_ += a.dt;
    const float R = h_ * (0.035f + pulse_ * 0.26f + kick_ * 0.08f);
    const uint32_t rim = Hsv(hue_ + 0.55f, 0.75f, 1.0f);
    const uint32_t halo = Hsv(hue_ + 0.55f, 0.6f, 0.55f + kick_ * 0.45f);
    const float haloR = R * (2.3f + kick_ * 0.7f);
    int x0 = std::max(0, (int)(cx - haloR)), x1 = std::min(w_ - 1, (int)(cx + haloR));
    int y0 = std::max(0, (int)(cy - haloR)), y1 = std::min(h_ - 1, (int)(cy + haloR));
    for (int y = y0; y <= y1; y++) {
        uint32_t* row = &buf_[(size_t)y * w_];
        for (int x = x0; x <= x1; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float d = std::sqrt(dx * dx + dy * dy) / R;
            if (d >= haloR / R) continue;
            // halo everywhere (also under the sphere's soft edge, so there is no dark seam)
            float t = d < 1.0f ? 1.0f : 1.0f - (d - 1.0f) / (haloR / R - 1.0f);
            uint32_t under = AddSat(row[x], Scale(halo, t * t * 0.8f));
            if (d < 1.0f) {
                // lit sphere: hot white core fading to the coloured rim, soft edge
                float nz = std::sqrt(1 - d * d);
                float light = 0.55f + 0.45f * Clamp(0.7f * nz - 0.35f * dx / R - 0.35f * dy / R, 0.0f, 1.0f);
                float core = Clamp(1.0f - d * d * 1.2f, 0.0f, 1.0f);
                uint32_t c = Scale(rim, light);
                c = AddSat(c, Scale(0xFFFFFF, core * (0.55f + pulse_ * 0.45f)));
                float edge = Clamp((1.0f - d) * R * 0.8f, 0.0f, 1.0f);
                row[x] = edge >= 1 ? c : AddSat(Scale(under, 1 - edge), Scale(c, edge));
            } else {
                row[x] = under;
            }
        }
    }
    // shock-wave ring after each beat
    if (ring_ < 0.6f) {
        float rr = R * (1.2f + ring_ * 9.0f);
        uint32_t rc = Scale(Hsv(hue_ + 0.2f, 0.4f, 1.0f), 1.0f - ring_ / 0.6f);
        int n = 180;
        float lx = cx + rr, ly = cy;
        for (int i = 1; i <= n; i++) {
            float ang = i * 2 * kPi / n;
            float x = cx + std::cos(ang) * rr, y = cy + std::sin(ang) * rr;
            line(lx, ly, x, y, rc, std::max(1, h_ / 200), true);
            lx = x;
            ly = y;
        }
    }
}

// 4. Demoscene plasma with palette cycling
void VisEngine::plasma(const VisAudio& a) {
    static float sinT[1024];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 1024; i++) sinT[i] = std::sin(i * 2 * kPi / 1024);
        init = true;
    }
    uint32_t pal[256];
    for (int i = 0; i < 256; i++) {
        float t = i / 256.0f;
        float v = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(t * 2 * kPi * 2)) * (0.6f + a.bass * 0.6f);
        pal[i] = Hsv(hue_ + t * 0.6f, 0.8f, Clamp(v, 0.0f, 1.0f));
    }
    float t = (float)a.time;
    int t1 = (int)(t * 60), t2 = (int)(t * 43), t3 = (int)(t * 27 + a.mid * 200);
    float fx = 2048.0f / w_, fy = 1536.0f / h_;
    for (int y = 0; y < h_; y++) {
        uint32_t* row = &buf_[(size_t)y * w_];
        const uint16_t* rad = &radial_[(size_t)y * w_];
        float sy = sinT[((int)(y * fy) + t2) & 1023];
        for (int x = 0; x < w_; x++) {
            float v = sinT[((int)(x * fx) + t1) & 1023] + sy + sinT[((int)((x + y) * fx * 0.5f) + t3) & 1023] +
                      sinT[(rad[x] * 3 - t1 * 2) & 1023];
            row[x] = pal[(int)((v + 4) * 32) & 255];
        }
    }
    // white oscilloscope over the plasma
    float px = 0, py = 0;
    for (int i = 0; i < VisAudio::kWave; i += 2) {
        float x = (float)i / (VisAudio::kWave - 1) * w_, y = h_ * 0.5f + a.wave[i] * h_ * 0.3f;
        if (i) line(px, py, x, y, 0x707070, std::max(1, h_ / 160), true);
        px = x;
        py = y;
    }
}

// 5. Classic fire effect fed by the spectrum
void VisEngine::fire(const VisAudio& a) {
    static uint32_t pal[256];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 256; i++) {
            float t = i / 255.0f;
            int r = (int)Clamp(t * 3.0f * 255, 0.0f, 255.0f);
            int g = (int)Clamp((t * 3.0f - 1.0f) * 255, 0.0f, 255.0f);
            int b = (int)Clamp((t * 3.0f - 2.0f) * 255, 0.0f, 255.0f);
            pal[i] = (r << 16) | (g << 8) | b;
        }
        init = true;
    }
    const int W = w_, H = h_;
    uint8_t* h = heat_.data();
    // seed the two rows below the screen according to the spectrum
    for (int x = 0; x < W; x++) {
        // bass in the middle, treble towards both edges
        float d = std::fabs(x - W / 2.0f) / (W / 2.0f);
        int band = Clamp((int)(d * VisAudio::kSpec * 0.85f), 0, VisAudio::kSpec - 1);
        float v = std::min(1.0f, a.spec[band] * (1.0f + d * 0.6f));
        int heat = (int)(v * v * 320) + (int)(rnd() % 50) - 25 + (a.beat ? 60 : 0);
        h[(size_t)H * W + x] = (uint8_t)Clamp(heat, 0, 255);
        h[(size_t)(H + 1) * W + x] = (uint8_t)Clamp(heat - (int)(rnd() % 40), 0, 255);
    }
    int decay = std::max(1, 520 / H);
    for (int y = 0; y < H; y++) {
        uint8_t* row = h + (size_t)y * W;
        const uint8_t* b1 = row + W;
        const uint8_t* b2 = row + 2 * W;
        for (int x = 0; x < W; x++) {
            int l = x > 0 ? b1[x - 1] : b1[x], r = x < W - 1 ? b1[x + 1] : b1[x];
            int v = (l + b1[x] + r + b2[x]) * 64 / 257 - decay;
            row[x] = (uint8_t)std::max(0, v);
        }
    }
    for (size_t i = 0; i < (size_t)W * H; i++) buf_[i] = pal[h[i]];
}

// 6. Amiga "Boing" demo: checkered ball bouncing in front of a purple grid
void VisEngine::boing(const VisAudio& a) {
    const float W = (float)w_, H = (float)h_;
    const uint32_t bg = 0xA8A8A8, grid = 0xA000A8;
    clear(bg);
    // back wall grid
    const float cell = H / 12.0f;
    const float floorY = H * 0.86f;
    for (float x = std::fmod(W / 2, cell); x < W; x += cell) rectFill((int)x, 0, std::max(1, h_ / 220), (int)floorY, grid);
    for (float y = 0; y < floorY; y += cell) rectFill(0, (int)y, w_, std::max(1, h_ / 220), grid);
    // floor in perspective
    for (int i = -16; i <= 16; i++) {
        float x0 = W / 2 + i * cell, x1 = W / 2 + i * cell * 1.6f;
        line(x0, floorY, x1, H, grid, std::max(1, h_ / 220), false);
    }
    for (int i = 0; i < 4; i++) {
        float t = (float)i / 4;
        float y = floorY + (H - floorY) * t * t;
        rectFill(0, (int)y, w_, std::max(1, h_ / 220), grid);
    }
    // ProTracker style VU meters, one per Paula channel (here: 4 frequency bands).
    // They jump up instantly on a hit and fall at a constant speed, like in PT 2.x.
    {
        static const int bands[5] = {0, 20, 50, 85, VisAudio::kSpec};
        const float meterW = W * 0.085f, maxH = floorY * 0.78f;
        for (int ch = 0; ch < 4; ch++) {
            float e = 0;
            for (int b = bands[ch]; b < bands[ch + 1]; b++) e += a.spec[b];
            e /= (bands[ch + 1] - bands[ch]);
            static const float gain[4] = {1.0f, 1.05f, 1.25f, 1.6f};  // treble is quieter: boost it
            float lvl = Clamp((e * gain[ch] - 0.12f) / 0.6f, 0.0f, 1.0f);
            bool hit = e > vuAvg_[ch] * 1.08f + 0.015f;
            vuAvg_[ch] += (e - vuAvg_[ch]) * std::min(1.0f, a.dt * 3.0f);
            if (hit && lvl > vu_[ch]) vu_[ch] = std::min(1.0f, lvl * 1.15f);
            vu_[ch] = std::max(0.0f, vu_[ch] - a.dt * 1.1f);
            if (!a.playing) vu_[ch] = std::max(0.0f, vu_[ch] - a.dt * 2);
            float cxm = W * (0.2f + ch * 0.2f);
            int x0 = (int)(cxm - meterW / 2), x1 = (int)(cxm + meterW / 2);
            int top = (int)(floorY - vu_[ch] * maxH);
            for (int y = top; y < (int)floorY; y++) {
                // gradient fixed to the screen, so a taller bar reveals more of it (green -> yellow -> red)
                float f = (floorY - y) / maxH;
                uint32_t c = f < 0.5f ? Hsv(0.33f - f * 0.18f, 1.0f, 0.55f + f * 0.8f)
                                      : Hsv(0.24f - (f - 0.5f) * 0.48f, 1.0f, 0.95f);
                rectFill(x0, y, x1 - x0, 1, c);
            }
            if (top < (int)floorY) {
                rectFill(x0, top, x1 - x0, std::max(1, h_ / 200), 0xFFFFFF);
                rectFill(x0, top, std::max(1, h_ / 220), (int)floorY - top, 0x000000);
                rectFill(x1 - std::max(1, h_ / 220), top, std::max(1, h_ / 220), (int)floorY - top, 0x000000);
            }
        }
    }
    // physics: horizontal drift, gravity bounce kicked by the bass
    const float R = H * (0.19f + a.bass * 0.03f);
    if (stars_.empty()) stars_.push_back({R + 1, floorY - R - H * 0.3f, 0});  // x, y, vy (reuse Star storage)
    Star& b = stars_[0];
    static float vx = 1;
    b.x += vx * W * 0.22f * a.dt;
    if (b.x < R) b.x = R, vx = 1;
    if (b.x > W - R) b.x = W - R, vx = -1;
    b.z += H * 2.8f * a.dt;  // gravity (z = vertical speed)
    b.y += b.z * a.dt;
    if (b.y > floorY - R) {
        b.y = floorY - R;
        b.z = -H * (1.35f + a.bass * 0.5f);
    }
    if (a.beat && b.z > -H * 0.5f) b.z -= H * 0.4f;
    spin_ += a.dt * vx * (2.0f + a.treb * 3.0f);
    // shadow
    const float sx = b.x + R * 0.35f, sy = b.y + R * 0.08f;
    for (int y = (int)(sy - R); y <= (int)(sy + R); y++)
        for (int x = (int)(sx - R); x <= (int)(sx + R); x++) {
            float dx = (x - sx) / R, dy = (y - sy) / R;
            if (dx * dx + dy * dy <= 1 && (unsigned)x < (unsigned)w_ && (unsigned)y < (unsigned)h_) {
                uint32_t& p = buf_[(size_t)y * w_ + x];
                p = Scale(p, 0.55f);
            }
        }
    // ball: tilted sphere with a red / white checker, lit from the top left
    const float tilt = 0.30f, ct = std::cos(tilt), st = std::sin(tilt);
    for (int y = (int)(b.y - R); y <= (int)(b.y + R); y++) {
        if ((unsigned)y >= (unsigned)h_) continue;
        for (int x = (int)(b.x - R); x <= (int)(b.x + R); x++) {
            if ((unsigned)x >= (unsigned)w_) continue;
            float nx = (x - b.x) / R, ny = (y - b.y) / R;
            float r2 = nx * nx + ny * ny;
            if (r2 > 1) continue;
            float nz = std::sqrt(1 - r2);
            float tx = nx * ct - ny * st, ty = nx * st + ny * ct;
            float lat = std::asin(Clamp(ty, -1.0f, 1.0f));
            float lon = std::atan2(tx, nz) + spin_;
            int ci = (int)std::floor(lon / (kPi / 8)) + (int)std::floor(lat / (kPi / 8));
            uint32_t base = (ci & 1) ? 0xFFFFFF : 0xE00000;
            float light = Clamp(0.35f + 0.75f * (-0.45f * nx - 0.55f * ny + 0.7f * nz), 0.25f, 1.0f);
            float edge = Clamp((1 - r2) * 6, 0.0f, 1.0f);  // soft anti-aliased rim
            uint32_t c = Scale(base, light);
            uint32_t& p = buf_[(size_t)y * w_ + x];
            if (edge < 1) {
                auto mix = [&](int sh) {
                    return (uint32_t)(((c >> sh) & 255) * edge + ((p >> sh) & 255) * (1 - edge)) << sh;
                };
                p = mix(16) | mix(8) | mix(0);
            } else {
                p = c;
            }
        }
    }
}
