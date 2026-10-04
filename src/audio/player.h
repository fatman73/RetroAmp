#pragma once
#include "decoder.h"
#include "dsp.h"

#include <condition_variable>

enum class PlayState { Stopped, Playing, Paused };

struct AudioDevice {
    std::wstring id, name;
};
std::vector<AudioDevice> ListOutputDevices();

enum PlayerEvent : WPARAM {
    PE_OPENED = 1,  // decoder opened, format/duration known
    PE_ENDED = 2,   // end of track reached (all audio played)
    PE_ERROR = 3,   // could not open / play (see lastError())
};

// Audio engine: decoder -> DSP -> WASAPI shared-mode output, on its own thread.
class Player {
public:
    ~Player();
    void init(HWND notify);
    // Output endpoint id; empty = follow the Windows default device (also when it changes,
    // e.g. Bluetooth headphones connecting).
    void setDevice(const std::wstring& id);
    std::wstring device() const;

    // Starts playing a file. Returns a session id used in WM_PLAYER_EVENT lParam.
    uint64_t play(const std::wstring& path, double startPos = 0, bool startPaused = false);
    void stop();
    void setPaused(bool paused);
    void seek(double seconds);

    PlayState state() const { return (PlayState)state_.load(); }
    double position() const { return position_.load(); }
    double duration() const { return duration_.load(); }
    int sampleRate() const { return sampleRate_.load(); }
    int channels() const { return channels_.load(); }
    int bitrate() const { return bitrate_.load(); }
    uint64_t session() const { return session_; }
    std::wstring lastError() const;
    std::wstring backend() const;

    void setParams(const DspParams& p);
    DspParams params() const;

    // Current RMS level of the left / right channel (pre-volume, 0..1).
    void levels(float& l, float& r) const {
        l = levelL_.load();
        r = levelR_.load();
    }

    // Copies the most recently *heard* `n` mono samples (pre-volume). Returns false if not playing.
    bool visSamples(float* out, int n) const;

private:
    void threadMain(uint64_t session, std::wstring path, double startPos, bool startPaused);
    void joinThread();

    HWND notify_ = nullptr;
    std::thread thread_;
    std::atomic<bool> quit_{false};
    std::atomic<int> state_{(int)PlayState::Stopped};
    std::atomic<bool> pauseReq_{false};
    std::atomic<double> seekReq_{-1};
    std::atomic<double> position_{0}, duration_{0};
    std::atomic<int> sampleRate_{0}, channels_{0}, bitrate_{0};
    uint64_t session_ = 0;

    mutable std::mutex mtx_;  // guards params_, error_, backend_
    std::condition_variable cv_;
    DspParams params_;
    std::atomic<uint32_t> paramsVersion_{1};
    std::wstring error_, backend_, deviceId_;

    static constexpr int kVisSize = 1 << 15;
    mutable std::mutex visMtx_;
    std::vector<float> vis_ = std::vector<float>(kVisSize);
    uint64_t visWritten_ = 0;
    std::atomic<uint64_t> visPlayed_{0};
    std::atomic<float> levelL_{0}, levelR_{0};
};
