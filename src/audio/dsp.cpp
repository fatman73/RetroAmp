#include "dsp.h"

static const double kPi = 3.14159265358979323846;

const float* EqFrequencies(int mode) {
    static const float winamp[kEqBands] = {60, 170, 310, 600, 1000, 3000, 6000, 12000, 14000, 16000};
    static const float iso[kEqBands] = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    return mode == 1 ? iso : winamp;
}

void Biquad::setCoefs(double B0, double B1, double B2, double A0, double A1, double A2) {
    b0 = B0 / A0;
    b1 = B1 / A0;
    b2 = B2 / A0;
    a1 = A1 / A0;
    a2 = A2 / A0;
    flat = false;
}

static double SafeFreq(double fs, double f) { return Clamp(f, 10.0, fs * 0.45); }

void Biquad::peaking(double fs, double f, double q, double db) {
    if (std::fabs(db) < 0.01) {
        b0 = 1; b1 = b2 = a1 = a2 = 0;
        flat = true;
        return;
    }
    double A = std::pow(10.0, db / 40.0), w0 = 2 * kPi * SafeFreq(fs, f) / fs;
    double c = std::cos(w0), alpha = std::sin(w0) / (2 * q);
    setCoefs(1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
}

void Biquad::lowShelf(double fs, double f, double q, double db) {
    double A = std::pow(10.0, db / 40.0), w0 = 2 * kPi * SafeFreq(fs, f) / fs;
    double c = std::cos(w0), alpha = std::sin(w0) / (2 * q), sa = 2 * std::sqrt(A) * alpha;
    setCoefs(A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
             (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
}

void Biquad::highShelf(double fs, double f, double q, double db) {
    double A = std::pow(10.0, db / 40.0), w0 = 2 * kPi * SafeFreq(fs, f) / fs;
    double c = std::cos(w0), alpha = std::sin(w0) / (2 * q), sa = 2 * std::sqrt(A) * alpha;
    setCoefs(A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
             (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
}

void Biquad::lowPass(double fs, double f, double q) {
    double w0 = 2 * kPi * SafeFreq(fs, f) / fs, c = std::cos(w0), alpha = std::sin(w0) / (2 * q);
    setCoefs((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + alpha, -2 * c, 1 - alpha);
}

void Biquad::highPass(double fs, double f, double q) {
    double w0 = 2 * kPi * SafeFreq(fs, f) / fs, c = std::cos(w0), alpha = std::sin(w0) / (2 * q);
    setCoefs((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha);
}

double Biquad::responseDb(double fs, double f) const {
    if (flat) return 0;
    double w = 2 * kPi * f / fs;
    // H(e^jw) = (b0 + b1 e^-jw + b2 e^-2jw) / (1 + a1 e^-jw + a2 e^-2jw)
    double cr = std::cos(w), ci = -std::sin(w), c2r = std::cos(2 * w), c2i = -std::sin(2 * w);
    double nr = b0 + b1 * cr + b2 * c2r, ni = b1 * ci + b2 * c2i;
    double dr = 1 + a1 * cr + a2 * c2r, di = a1 * ci + a2 * c2i;
    double mag = std::sqrt((nr * nr + ni * ni) / std::max(1e-20, dr * dr + di * di));
    return 20 * std::log10(std::max(1e-9, mag));
}

void DspChain::reset() {
    for (auto& b : eq_) b.reset();
    for (Biquad* b : {&subsonic_, &shelf_, &sub_, &hLp1_, &hLp2_, &hHp_, &hLp3_, &loudLo_, &loudHi_}) b->reset();
    limGain_ = 1;
}

void DspChain::configure(const DspParams& p, int sampleRate) {
    bool srChanged = sampleRate != sr_;
    p_ = p;
    sr_ = sampleRate;
    double fs = sr_;
    const float* freqs = EqFrequencies(p.eqMode);
    eqActive_ = p.eqOn;
    for (int i = 0; i < kEqBands; i++) {
        double q = p.eqQ;
        if (p.eqMode == 0 && i >= 7) q *= 1.6;  // 12k/14k/16k are close together
        eq_[i].peaking(fs, freqs[i], q, p.eqOn ? p.bands[i] : 0);
    }
    preGain_ = (float)std::pow(10.0, p.preamp / 20.0);

    bassActive_ = p.bassOn && (p.bassBoost > 0.05f || p.subBoost > 0.05f || p.harmonics > 0.005f);
    shelf_.lowShelf(fs, p.bassFreq, 0.707, p.bassOn ? p.bassBoost : 0);
    sub_.peaking(fs, std::max(25.0, p.bassFreq * 0.55), 1.1, p.bassOn ? p.subBoost : 0);
    subsonic_.highPass(fs, 18, 0.707);
    // keep some headroom so the limiter does not have to work too hard
    bassHeadroom_ = (float)std::pow(10.0, -(p.bassBoost * 0.12 + p.subBoost * 0.1) / 20.0);
    harmAmt_ = p.bassOn ? p.harmonics : 0;
    double hf = Clamp((double)p.bassFreq * 1.3, 60.0, 250.0);
    hLp1_.lowPass(fs, hf, 0.707);
    hLp2_.lowPass(fs, hf, 0.707);
    hHp_.highPass(fs, hf * 1.1, 0.707);
    hLp3_.lowPass(fs, hf * 5, 0.707);

    float lc = Clamp(1.0f - p.volume, 0.0f, 1.0f);
    loudActive_ = p.loudness && lc > 0.01f;
    loudLo_.lowShelf(fs, 100, 0.707, lc * 12.0);
    loudHi_.highShelf(fs, 9000, 0.707, lc * 6.0);

    float vol = p.volume * p.volume;
    gainL_ = vol * std::min(1.0f, 1.0f - p.balance);
    gainR_ = vol * std::min(1.0f, 1.0f + p.balance);
    limRelease_ = 1.0 - std::exp(-1.0 / (0.12 * fs));
    if (srChanged) reset();
}

static inline double Shaper(double x) {
    // mix of odd (tanh) and even (rectifier) harmonics
    double a = std::tanh(x * 2.5);
    return a * 0.6 + std::fabs(x) * 0.8;
}

void DspChain::process(float* buf, int frames, float* vis) {
    const float width = p_.width;
    const bool doWidth = std::fabs(width - 1.0f) > 0.01f;
    const double ceiling = 0.985;
    for (int i = 0; i < frames; i++) {
        double l = buf[i * 2], r = buf[i * 2 + 1];
        if (eqActive_) {
            l *= preGain_;
            r *= preGain_;
            for (auto& b : eq_) {
                if (b.flat) continue;
                l = b.run(0, l);
                r = b.run(1, r);
            }
        }
        if (bassActive_) {
            if (harmAmt_ > 0) {
                double hl = hLp2_.run(0, hLp1_.run(0, l));
                double hr = hLp2_.run(1, hLp1_.run(1, r));
                hl = hLp3_.run(0, hHp_.run(0, Shaper(hl * 3.0)));
                hr = hLp3_.run(1, hHp_.run(1, Shaper(hr * 3.0)));
                l += hl * harmAmt_ * 1.6;
                r += hr * harmAmt_ * 1.6;
            }
            if (!shelf_.flat) {
                l = shelf_.run(0, l);
                r = shelf_.run(1, r);
            }
            if (!sub_.flat) {
                l = sub_.run(0, l);
                r = sub_.run(1, r);
            }
            l = subsonic_.run(0, l) * bassHeadroom_;
            r = subsonic_.run(1, r) * bassHeadroom_;
        }
        if (loudActive_) {
            l = loudHi_.run(0, loudLo_.run(0, l));
            r = loudHi_.run(1, loudLo_.run(1, r));
        }
        if (doWidth) {
            double m = (l + r) * 0.5, s = (l - r) * 0.5 * width;
            l = m + s;
            r = m - s;
        }
        if (vis) vis[i] = (float)((l + r) * 0.5);
        l *= gainL_;
        r *= gainR_;
        if (p_.limiter) {
            double peak = std::max(std::fabs(l), std::fabs(r));
            double target = peak > ceiling ? ceiling / peak : 1.0;
            limGain_ = std::min(limGain_ + (1.0 - limGain_) * limRelease_, target);
            l *= limGain_;
            r *= limGain_;
        } else {
            l = Clamp(l, -1.0, 1.0);
            r = Clamp(r, -1.0, 1.0);
        }
        buf[i * 2] = (float)l;
        buf[i * 2 + 1] = (float)r;
    }
}
