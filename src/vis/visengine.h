#pragma once
#include "../common.h"

// Audio features handed to the visual presets every frame.
struct VisAudio {
    static constexpr int kWave = 512;
    static constexpr int kSpec = 128;
    float wave[kWave] = {};  // -1..1, most recent samples
    float spec[kSpec] = {};  // 0..1, log frequency 30 Hz .. 16 kHz
    float bass = 0, mid = 0, treb = 0;  // 0..1
    float bassAvg = 0;                  // slow average of bass
    bool beat = false;
    bool playing = false;
    double time = 0;   // seconds
    float dt = 0.016f; // seconds since last frame
};

// Computes VisAudio from the player's latest samples (FFT, bands, beat detection).
class VisAnalyzer {
public:
    void update(const float* samples, int n, int sampleRate, bool playing, VisAudio& out);

private:
    double time_ = 0;
    ULONGLONG last_ = 0;
    float bassAvg_ = 0, lastBeat_ = -10;
};

// Software renderer of the classic "old school" visualisation presets (MilkDrop / AVS style).
class VisEngine {
public:
    static int presetCount();
    static const wchar_t* presetName(int i);

    void resize(int w, int h);
    void setPreset(int p);
    int preset() const { return preset_; }
    void render(const VisAudio& a);

    int width() const { return w_; }
    int height() const { return h_; }
    const uint32_t* pixels() const { return buf_.data(); }

private:
    void analyzer(const VisAudio& a);
    void scopeTrails(const VisAudio& a);
    void milkTunnel(const VisAudio& a);
    void starfield(const VisAudio& a);
    void plasma(const VisAudio& a);
    void fire(const VisAudio& a);
    void boing(const VisAudio& a);
    void protracker(const VisAudio& a);

    // helpers
    void updateVu(const VisAudio& a, bool follow);  // ProTracker style VU meters (vu_), one per band
    void ptGenerate(int pattern);
    void clear(uint32_t c);
    void fade(int r256, int g256, int b256);
    void warp(float zoom, float rot, float dx, float dy, int fr, int fg, int fb, float waveAmp = 0, float waveFreq = 0,
              float wavePhase = 0);
    void addPixel(int x, int y, uint32_t c);
    void setPixel(int x, int y, uint32_t c);
    void line(float x0, float y0, float x1, float y1, uint32_t c, int thick, bool additive);
    void rectFill(int x, int y, int w, int h, uint32_t c);

    int w_ = 0, h_ = 0;
    int preset_ = 0;
    std::vector<uint32_t> buf_, tmp_;
    // per-preset state
    float bars_[64] = {}, peaks_[64] = {}, peakVel_[64] = {};
    struct Star {
        float x, y, z;
    };
    std::vector<Star> stars_;
    std::vector<uint8_t> heat_;
    std::vector<uint16_t> radial_;
    float rot_ = 0, hue_ = 0, spin_ = 0;
    float pulse_ = 0, kick_ = 0, ring_ = 10;
    float vu_[4] = {}, vuAvg_[4] = {};
    // Protracker preset: fake module (64-row patterns, 4 channels) scrolling past the cursor bar
    struct PtCell {
        uint8_t note = 0, sample = 0;  // note 1..36 = C-1..B-3, 0 = none
        uint16_t fx = 0;               // effect command + parameter (3 hex digits)
    };
    PtCell ptPat_[64][4];
    int ptRow_ = 0, ptPos_ = 0, ptPatNo_ = -1;
    double ptClock_ = 0;
    std::vector<uint32_t> pt_;  // low-res (320 px wide) Amiga screen, scaled into buf_
    uint32_t rng_ = 12345;
    uint32_t rnd() {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return rng_;
    }
    float frnd() { return (rnd() & 0xFFFFFF) / 16777215.0f; }
};

uint32_t Hsv(float h, float s, float v);  // h in [0,1)
