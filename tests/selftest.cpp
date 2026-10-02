// Console self-test: decoders, DSP frequency response and skin archives.
// Usage: retroamp_selftest <folder with audio files> [skin.wsz ...]
#include <cstdio>

#include "../src/audio/decoder.h"
#include "../src/audio/dsp.h"
#include "../src/gfx.h"
#include "../src/skin.h"

static int g_fail = 0;
#define CHECK(cond, ...)                \
    do {                                \
        if (!(cond)) {                  \
            printf("  FAIL: " __VA_ARGS__); \
            printf("\n");               \
            g_fail++;                   \
        }                               \
    } while (0)

static double Rms(const float* p, size_t n, int stride) {
    double s = 0;
    for (size_t i = 0; i < n; i++) s += (double)p[i * stride] * p[i * stride];
    return std::sqrt(s / std::max<size_t>(1, n));
}

static void TestDecoders(const std::wstring& dir) {
    printf("== Decoders (%ls)\n", dir.c_str());
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(PathJoin(dir, L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring path = PathJoin(dir, fd.cFileName);
        std::wstring err;
        auto dec = CreateDecoder(path, err);
        if (!dec) {
            printf("%-14ls  FAIL open: %ls\n", fd.cFileName, err.c_str());
            g_fail++;
            continue;
        }
        std::vector<float> buf(4096 * dec->channels);
        size_t frames = 0;
        double sum = 0;
        int n;
        while ((n = dec->read(buf.data(), 4096)) > 0) {
            frames += n;
            sum += Rms(buf.data(), (size_t)n * dec->channels, 1) * n;
        }
        double secs = (double)frames / dec->sampleRate;
        double level = frames ? sum / frames : 0;
        // seek test
        dec->seek(4.0);
        int got = dec->read(buf.data(), 4096);
        double seekLevel = got > 0 ? Rms(buf.data(), (size_t)got * dec->channels, 1) : 0;
        printf("%-14ls  %-16ls %6d Hz %d ch  dur=%5.2fs decoded=%5.2fs rms=%.3f seek->%d frames rms=%.3f\n",
               fd.cFileName, dec->backend(), dec->sampleRate, dec->channels, dec->duration, secs, level, got, seekLevel);
        CHECK(secs > 7.5 && secs < 8.6, "decoded length %.2f", secs);
        CHECK(level > 0.01, "silent output");
        CHECK(got > 0 && seekLevel > 0.001, "seek failed");
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static double MeasureGainDb(DspParams p, double freq, int sr = 48000) {
    DspChain dsp;
    p.volume = 1.0f;
    p.balance = 0;
    dsp.configure(p, sr);
    const int N = sr;  // 1 second
    std::vector<float> buf(N * 2);
    const double amp = 0.05;
    for (int i = 0; i < N; i++) buf[i * 2] = buf[i * 2 + 1] = (float)(amp * std::sin(2 * 3.14159265358979 * freq * i / sr));
    dsp.process(buf.data(), N, nullptr);
    double out = Rms(buf.data() + N, N / 2, 2);  // second half, left channel
    return 20 * std::log10(out / (amp / std::sqrt(2.0)));
}

static void TestDsp() {
    printf("== DSP frequency response (dB)\n");
    DspParams flat;
    flat.limiter = false;
    const double freqs[] = {30, 60, 120, 250, 1000, 4000, 12000};
    auto row = [&](const char* name, const DspParams& p) {
        printf("%-26s", name);
        std::vector<double> r;
        for (double f : freqs) {
            double g = MeasureGainDb(p, f);
            r.push_back(g);
            printf(" %6.1f", g);
        }
        printf("\n");
        return r;
    };
    printf("%-26s", "freq Hz");
    for (double f : freqs) printf(" %6.0f", f);
    printf("\n");
    auto r0 = row("flat", flat);
    for (double g : r0) CHECK(std::fabs(g) < 0.2, "flat chain not flat (%.2f dB)", g);

    DspParams eq = flat;
    eq.bands[0] = 12;  // 60 Hz
    eq.bands[4] = -12; // 1 kHz
    auto r1 = row("EQ 60Hz +12 / 1k -12", eq);
    CHECK(r1[1] > 9, "60 Hz band boost too small (%.1f)", r1[1]);
    CHECK(r1[4] < -9, "1 kHz band cut too small (%.1f)", r1[4]);
    CHECK(std::fabs(r1[6]) < 1.5, "12 kHz should be untouched (%.1f)", r1[6]);

    DspParams pre = flat;
    pre.preamp = -6;
    auto r2 = row("preamp -6", pre);
    CHECK(std::fabs(r2[4] + 6) < 0.3, "preamp wrong (%.2f)", r2[4]);

    DspParams bass = flat;
    bass.bassBoost = 12;
    bass.bassFreq = 100;
    auto r3 = row("bass boost +12 @100Hz", bass);
    CHECK(r3[1] - r3[4] > 7, "bass shelf too weak (%.1f vs %.1f)", r3[1], r3[4]);

    DspParams sub = flat;
    sub.subBoost = 10;
    sub.bassFreq = 80;
    auto r4 = row("sub +10", sub);
    CHECK(r4[0] - r4[4] > 4, "sub boost too weak");

    DspParams harm = flat;
    harm.harmonics = 1.0f;
    row("harmonics 100%", harm);

    DspParams lim = flat;
    lim.limiter = true;
    lim.bassBoost = 18;
    lim.preamp = 12;
    lim.bands[0] = 12;
    // clipping test: loud input must never exceed the ceiling with the limiter on
    {
        DspChain dsp;
        lim.volume = 1;
        dsp.configure(lim, 48000);
        std::vector<float> buf(48000 * 2);
        for (int i = 0; i < 48000; i++) buf[i * 2] = buf[i * 2 + 1] = (float)(0.9 * std::sin(2 * 3.14159265 * 60 * i / 48000.0));
        dsp.process(buf.data(), 48000, nullptr);
        float peak = 0;
        for (float v : buf) peak = std::max(peak, std::fabs(v));
        printf("%-26s peak=%.4f\n", "limiter (+42 dB drive)", peak);
        CHECK(peak <= 0.99f, "limiter let the signal clip (%.3f)", peak);
    }
    DspParams wide = flat;
    wide.width = 0;  // mono
    {
        DspChain dsp;
        wide.volume = 1;
        dsp.configure(wide, 48000);
        float b[4] = {1.0f, 0.0f, 0.5f, -0.5f};
        dsp.process(b, 2, nullptr);
        printf("%-26s L=%.2f R=%.2f / L=%.2f R=%.2f\n", "width 0% (mono)", b[0], b[1], b[2], b[3]);
        CHECK(std::fabs(b[0] - b[1]) < 1e-4 && std::fabs(b[2] - b[3]) < 1e-4, "width=0 should be mono");
    }
}

static void TestSkins(int argc, wchar_t** argv) {
    for (int i = 2; i < argc; i++) {
        Skin s;
        std::wstring err;
        bool ok = s.loadFrom(argv[i], err);
        printf("== Skin %ls: %s %ls  main=%dx%d pledit=%dx%d numsEx=%d\n", argv[i], ok ? "OK" : "FAIL", err.c_str(),
               s.img(SB_MAIN).w, s.img(SB_MAIN).h, s.img(SB_PLEDIT).w, s.img(SB_PLEDIT).h, s.useNumsEx);
        CHECK(ok, "skin load");
    }
}

int wmain(int argc, wchar_t** argv) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    GdiplusStartupOnce();
    if (argc > 1) TestDecoders(argv[1]);
    TestDsp();
    TestSkins(argc, argv);
    printf("\n%s (%d failures)\n", g_fail ? "FAILED" : "ALL OK", g_fail);
    return g_fail ? 1 : 0;
}
