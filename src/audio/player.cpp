#include "player.h"

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <ksmedia.h>
#include <functiondiscoverykeys_devpkey.h>
#include <cstdio>

#pragma comment(lib, "ole32.lib")

namespace {

// Set whenever Windows changes the default output (Bluetooth headset connected, etc.)
std::atomic<bool> g_deviceChanged{false};

class DeviceNotifier : public IMMNotificationClient {
public:
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
            *ppv = (IMMNotificationClient*)this;
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override {
        if (flow == eRender && role == eConsole) g_deviceChanged = true;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }
};

template <class T>
void SafeRelease(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

class WasapiOut {
public:
    ~WasapiOut() { close(); }

    bool init(int rate, const std::wstring& deviceId) {
        close();
        IMMDeviceEnumerator* en = nullptr;
        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&en)))) return false;
        HRESULT hr = E_FAIL;
        if (!deviceId.empty()) {
            hr = en->GetDevice(deviceId.c_str(), &dev_);
            DWORD state = 0;
            if (SUCCEEDED(hr) && (FAILED(dev_->GetState(&state)) || state != DEVICE_STATE_ACTIVE)) {
                SafeRelease(dev_);
                hr = E_FAIL;  // chosen device unplugged: fall back to the default one
            }
        }
        if (FAILED(hr)) hr = en->GetDefaultAudioEndpoint(eRender, eConsole, &dev_);
        if (FAILED(hr)) hr = en->GetDefaultAudioEndpoint(eRender, eMultimedia, &dev_);
        en->Release();
        if (FAILED(hr)) return false;
        if (FAILED(dev_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client_))) return false;
        WAVEFORMATEXTENSIBLE wf = {};
        wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wf.Format.nChannels = 2;
        wf.Format.nSamplesPerSec = rate;
        wf.Format.wBitsPerSample = 32;
        wf.Format.nBlockAlign = 8;
        wf.Format.nAvgBytesPerSec = rate * 8;
        wf.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        wf.Samples.wValidBitsPerSample = 32;
        wf.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
        wf.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        // Bluetooth endpoints have long periods; 100 ms keeps them glitch-free.
        REFERENCE_TIME dur = 1000000;
        hr = client_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                 AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                     AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                 dur, 0, (WAVEFORMATEX*)&wf, nullptr);
        if (FAILED(hr)) {
            // some drivers refuse the converted format: retry at the device mix rate
            WAVEFORMATEX* mix = nullptr;
            if (SUCCEEDED(client_->GetMixFormat(&mix)) && (int)mix->nSamplesPerSec != rate) {
                SafeRelease(client_);
                CoTaskMemFree(mix);
                return false;
            }
            if (mix) CoTaskMemFree(mix);
            return false;
        }
        event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        client_->SetEventHandle(event_);
        client_->GetBufferSize(&bufferFrames);
        if (FAILED(client_->GetService(IID_PPV_ARGS(&render_)))) return false;
        return true;
    }

    void close() {
        if (client_) client_->Stop();
        SafeRelease(render_);
        SafeRelease(client_);
        SafeRelease(dev_);
        if (event_) CloseHandle(event_);
        event_ = nullptr;
        bufferFrames = 0;
    }

    bool padding(UINT32& pad) { return client_ && SUCCEEDED(client_->GetCurrentPadding(&pad)); }

    bool write(const float* data, UINT32 frames) {
        BYTE* p = nullptr;
        if (FAILED(render_->GetBuffer(frames, &p))) return false;
        memcpy(p, data, frames * 8);
        render_->ReleaseBuffer(frames, 0);
        return true;
    }

    void start() { client_->Start(); }
    void stop() { client_->Stop(); }
    void reset() { client_->Reset(); }
    void wait(DWORD ms) {
        if (event_) WaitForSingleObject(event_, ms);
        else Sleep(ms);
    }

    UINT32 bufferFrames = 0;

private:
    IMMDevice* dev_ = nullptr;
    IAudioClient* client_ = nullptr;
    IAudioRenderClient* render_ = nullptr;
    HANDLE event_ = nullptr;
};

void ToStereo(const float* src, int ch, int frames, float* dst) {
    if (ch == 2) {
        memcpy(dst, src, (size_t)frames * 2 * sizeof(float));
    } else if (ch == 1) {
        for (int i = 0; i < frames; i++) dst[i * 2] = dst[i * 2 + 1] = src[i];
    } else {
        // generic downmix: FL FR FC LFE BL BR SL SR ...
        for (int i = 0; i < frames; i++) {
            const float* s = src + (size_t)i * ch;
            float l = s[0], r = s[1];
            if (ch >= 3) { l += 0.707f * s[2]; r += 0.707f * s[2]; }
            if (ch >= 5) { l += 0.707f * s[4]; }
            if (ch >= 6) { r += 0.707f * s[5]; }
            if (ch >= 7) { l += 0.707f * s[6]; }
            if (ch >= 8) { r += 0.707f * s[7]; }
            float norm = ch >= 5 ? 0.5f : 0.7f;
            dst[i * 2] = l * norm;
            dst[i * 2 + 1] = r * norm;
        }
    }
}

}  // namespace

Player::~Player() { stop(); }

void Player::init(HWND notify) {
    notify_ = notify;
    static DeviceNotifier notifier;
    IMMDeviceEnumerator* en = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&en)))) {
        en->RegisterEndpointNotificationCallback(&notifier);  // kept for the process lifetime
        en->AddRef();
        en->Release();
    }
}

void Player::setDevice(const std::wstring& id) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (deviceId_ == id) return;
        deviceId_ = id;
    }
    g_deviceChanged = true;
    cv_.notify_all();
}

std::wstring Player::device() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return deviceId_;
}

std::vector<AudioDevice> ListOutputDevices() {
    std::vector<AudioDevice> out;
    IMMDeviceEnumerator* en = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&en)))) return out;
    IMMDeviceCollection* col = nullptr;
    if (SUCCEEDED(en->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &col))) {
        UINT n = 0;
        col->GetCount(&n);
        for (UINT i = 0; i < n; i++) {
            IMMDevice* d = nullptr;
            if (FAILED(col->Item(i, &d))) continue;
            AudioDevice ad;
            LPWSTR id = nullptr;
            if (SUCCEEDED(d->GetId(&id))) {
                ad.id = id;
                CoTaskMemFree(id);
            }
            IPropertyStore* ps = nullptr;
            if (SUCCEEDED(d->OpenPropertyStore(STGM_READ, &ps))) {
                PROPVARIANT v;
                PropVariantInit(&v);
                if (SUCCEEDED(ps->GetValue(PKEY_Device_FriendlyName, &v)) && v.vt == VT_LPWSTR) ad.name = v.pwszVal;
                PropVariantClear(&v);
                ps->Release();
            }
            if (ad.name.empty()) ad.name = ad.id;
            out.push_back(ad);
            d->Release();
        }
        col->Release();
    }
    en->Release();
    return out;
}

void Player::joinThread() {
    if (!thread_.joinable()) return;
    HANDLE h = (HANDLE)thread_.native_handle();
    while (WaitForSingleObject(h, 50) == WAIT_TIMEOUT) {
        CancelSynchronousIo(h);  // unblock a stalled ffmpeg pipe read
        cv_.notify_all();
    }
    thread_.join();
}

uint64_t Player::play(const std::wstring& path, double startPos, bool startPaused) {
    stop();
    quit_ = false;
    pauseReq_ = startPaused;
    seekReq_ = -1;
    position_ = startPos;
    duration_ = 0;
    sampleRate_ = 0;
    channels_ = 0;
    bitrate_ = 0;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        error_.clear();
        backend_.clear();
    }
    state_ = (int)(startPaused ? PlayState::Paused : PlayState::Playing);
    uint64_t s = ++session_;
    thread_ = std::thread(&Player::threadMain, this, s, path, startPos, startPaused);
    return s;
}

void Player::stop() {
    quit_ = true;
    cv_.notify_all();
    joinThread();
    state_ = (int)PlayState::Stopped;
    position_ = 0;
}

void Player::setPaused(bool paused) {
    if (state() == PlayState::Stopped) return;
    pauseReq_ = paused;
    state_ = (int)(paused ? PlayState::Paused : PlayState::Playing);
    cv_.notify_all();
}

void Player::seek(double seconds) {
    if (state() == PlayState::Stopped) return;
    double d = duration_.load();
    if (d > 0) seconds = Clamp(seconds, 0.0, std::max(0.0, d - 0.05));
    seekReq_ = std::max(0.0, seconds);
    position_ = seekReq_.load();
    cv_.notify_all();
}

std::wstring Player::lastError() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return error_;
}

std::wstring Player::backend() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return backend_;
}

void Player::setParams(const DspParams& p) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        params_ = p;
    }
    paramsVersion_++;
}

DspParams Player::params() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return params_;
}

bool Player::visSamples(float* out, int n) const {
    std::lock_guard<std::mutex> lk(visMtx_);
    uint64_t end = visPlayed_.load();
    for (int i = 0; i < n; i++) {
        int64_t idx = (int64_t)end - n + i;
        out[i] = idx < 0 || (uint64_t)idx >= visWritten_ ? 0.0f : vis_[(size_t)idx & (kVisSize - 1)];
    }
    return state() == PlayState::Playing;
}

void Player::threadMain(uint64_t session, std::wstring path, double startPos, bool startPaused) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    auto fail = [&](const std::wstring& msg) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            error_ = msg;
        }
        state_ = (int)PlayState::Stopped;
        PostMessageW(notify_, WM_PLAYER_EVENT, PE_ERROR, (LPARAM)session);
    };

    std::wstring err;
    std::unique_ptr<Decoder> dec = CreateDecoder(path, err);
    if (!dec) {
        fail(err);
        CoUninitialize();
        return;
    }
    {
        std::lock_guard<std::mutex> lk(mtx_);
        backend_ = dec->backend();
    }
    const int rate = dec->sampleRate;
    const int srcCh = dec->channels;
    sampleRate_ = rate;
    channels_ = srcCh;
    bitrate_ = dec->bitrate;
    duration_ = dec->duration;
    if (startPos > 0) dec->seek(startPos);
    PostMessageW(notify_, WM_PLAYER_EVENT, PE_OPENED, (LPARAM)session);

    WasapiOut out;
    g_deviceChanged = false;
    if (!out.init(rate, device())) {
        fail(L"Could not open the audio output device.");
        CoUninitialize();
        return;
    }

    DspChain dsp;
    uint32_t version = 0;
    std::vector<float> src, stereo, visTmp;
    double base = startPos;
    uint64_t written = 0;
    bool ended = false, running = false, endedPosted = false;
    (void)startPaused;

    while (!quit_) {
        double sk = seekReq_.exchange(-1);
        if (sk >= 0) {
            if (running) out.stop();
            running = false;
            out.reset();
            dec->seek(sk);
            base = sk;
            written = 0;
            ended = false;
            dsp.reset();
            position_ = sk;
        }
        if (pauseReq_) {
            if (running) out.stop();
            running = false;
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait_for(lk, std::chrono::milliseconds(200),
                         [&] { return !pauseReq_ || quit_ || seekReq_.load() >= 0; });
            continue;
        }
        if (paramsVersion_.load() != version) {
            version = paramsVersion_.load();
            DspParams cur = params();
            dsp.configure(cur, rate);
            static const bool logOn = GetEnvironmentVariableW(L"RETROAMP_LOG", nullptr, 0) > 0;
            if (logOn) {
                wchar_t lp[MAX_PATH];
                GetEnvironmentVariableW(L"RETROAMP_LOG", lp, MAX_PATH);
                FILE* f = nullptr;
                if (_wfopen_s(&f, lp, L"a") == 0 && f) {
                    fprintf(f, "configure v=%u sr=%d eqOn=%d pre=%.1f b=", version, rate, cur.eqOn, cur.preamp);
                    for (float b : cur.bands) fprintf(f, "%.1f ", b);
                    fprintf(f, "boost=%.1f vol=%.2f\n", cur.bassBoost, cur.volume);
                    fclose(f);
                }
            }
        }
        if (g_deviceChanged.exchange(false)) {
            // default device changed (e.g. Bluetooth headphones connected) or user picked another one:
            // continue on the new endpoint from the current position
            double resumeAt = position_.load();
            out.close();
            running = false;
            Sleep(150);
            if (out.init(rate, device())) {
                if (!ended) {
                    dec->seek(resumeAt);
                    dsp.reset();
                }
                base = resumeAt;
                written = 0;
            }
            continue;
        }
        UINT32 pad = 0;
        if (!out.padding(pad)) {
            // device lost (headphones unplugged, default device changed...): reopen
            running = false;
            Sleep(200);
            if (out.init(rate, device())) {
                base = base + (double)written / rate;
                written = 0;
            } else {
                Sleep(500);
            }
            continue;
        }
        uint64_t played = written > pad ? written - pad : 0;
        position_ = base + (double)played / rate;
        visPlayed_ = visWritten_ > pad ? visWritten_ - pad : 0;

        UINT32 avail = out.bufferFrames - pad;
        if (!ended && avail > 0 && (avail >= out.bufferFrames / 4 || !running)) {
            src.resize((size_t)avail * srcCh);
            stereo.resize((size_t)avail * 2);
            visTmp.resize(avail);
            int got = dec->read(src.data(), (int)avail);
            if (got < (int)avail) ended = true;
            if (got > 0) {
                ToStereo(src.data(), srcCh, got, stereo.data());
                dsp.process(stereo.data(), got, visTmp.data());
                {
                    std::lock_guard<std::mutex> lk(visMtx_);
                    for (int i = 0; i < got; i++) vis_[(size_t)(visWritten_ + i) & (kVisSize - 1)] = visTmp[i];
                    visWritten_ += got;
                }
                out.write(stereo.data(), (UINT32)got);
                written += got;
            }
            if (!running && written > 0) {
                out.start();
                running = true;
            }
        }
        if (ended) {
            UINT32 p2 = 0;
            if (!out.padding(p2) || p2 == 0 || !running) {
                if (!endedPosted) {
                    endedPosted = true;
                    state_ = (int)PlayState::Stopped;
                    PostMessageW(notify_, WM_PLAYER_EVENT, PE_ENDED, (LPARAM)session);
                }
                break;
            }
        }
        out.wait(100);
    }
    out.close();
    dec.reset();
    CoUninitialize();
}
