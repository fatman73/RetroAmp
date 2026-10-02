#pragma once
#include "../common.h"

constexpr int kEqBands = 10;

struct DspParams {
    // Graphic equalizer
    bool eqOn = true;
    float preamp = 0;              // dB, -12..+12
    float bands[kEqBands] = {};    // dB, -12..+12
    int eqMode = 0;                // 0 = Winamp frequencies, 1 = ISO octaves
    float eqQ = 0.9f;              // band width (Q), broad like classic Winamp
    // Bass section
    bool bassOn = true;
    float bassBoost = 0;           // dB, 0..18   (low shelf)
    float bassFreq = 80;           // Hz, 30..250 (shelf corner)
    float subBoost = 0;            // dB, 0..12   (deep sub-bass resonance)
    float harmonics = 0;           // 0..1        (psycho-acoustic bass)
    float width = 1.0f;            // 0..2        (stereo width)
    bool loudness = false;         // volume dependent loudness contour
    bool limiter = true;           // soft clip protection
    // Output
    float volume = 0.8f;           // 0..1
    float balance = 0;             // -1..1
};

const float* EqFrequencies(int mode);  // 10 centre frequencies

struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] = {0, 0}, z2[2] = {0, 0};
    bool flat = true;

    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    inline double run(int ch, double x) {
        double y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
    void setCoefs(double B0, double B1, double B2, double A0, double A1, double A2);
    void peaking(double fs, double f, double q, double db);
    void lowShelf(double fs, double f, double q, double db);
    void highShelf(double fs, double f, double q, double db);
    void lowPass(double fs, double f, double q);
    void highPass(double fs, double f, double q);
    // Magnitude response in dB at frequency f.
    double responseDb(double fs, double f) const;
};

class DspChain {
public:
    void configure(const DspParams& p, int sampleRate);
    // Processes interleaved stereo in place; writes a mono pre-volume copy into `vis` (may be null).
    void process(float* buf, int frames, float* vis);
    void reset();

private:
    DspParams p_;
    int sr_ = 0;
    Biquad eq_[kEqBands];
    bool eqActive_ = false;
    float preGain_ = 1;
    Biquad subsonic_, shelf_, sub_;
    bool bassActive_ = false;
    float bassHeadroom_ = 1;
    Biquad hLp1_, hLp2_, hHp_, hLp3_;
    float harmAmt_ = 0;
    Biquad loudLo_, loudHi_;
    bool loudActive_ = false;
    float gainL_ = 1, gainR_ = 1;
    double limGain_ = 1, limRelease_ = 0.0001;
};
