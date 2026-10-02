#include "decoder.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <propvarutil.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "propsys.lib")

// ---------------------------------------------------------------------------
// Supported extensions
static const wchar_t* kMfExts[] = {L"mp3", L"mp2", L"mp1", L"mpa", L"wav", L"wave", L"wma", L"asf", L"m4a", L"m4b",
                                   L"mp4", L"aac", L"adts", L"flac", L"alac", L"3gp", L"3g2", L"ac3", L"ec3", L"eac3",
                                   L"amr", L"caf"};
static const wchar_t* kAllExts[] = {
    L"mp3",  L"mp2",  L"mp1", L"mpa", L"wav",  L"wave", L"wma",  L"asf", L"m4a", L"m4b", L"mp4", L"aac",  L"adts",
    L"flac", L"fla",  L"alac", L"3gp", L"3g2", L"ac3",  L"ec3",  L"eac3", L"amr", L"awb", L"caf", L"ogg", L"oga",
    L"opus", L"spx",  L"aif", L"aiff", L"aifc", L"ape",  L"wv",   L"mpc", L"mp+", L"mpp", L"tta", L"tak", L"ofr",
    L"ofs",  L"dts",  L"dtshd", L"thd", L"truehd", L"mlp", L"mka",  L"webm", L"weba", L"au",  L"snd", L"dsf", L"dff",
    L"w64",  L"rf64", L"voc", L"gsm", L"ra",   L"rm",   L"wvp",  L"mod", L"xm",  L"s3m", L"it",  L"mptm", L"mo3",
    L"669",  L"mtm",  L"stm", L"umx", L"med",  L"okt",  L"far",  L"ult", L"nsf", L"nsfe", L"spc", L"gbs", L"vgm",
    L"vgz",  L"ay",   L"hes", L"kss", L"sap",  L"gym",  L"psf",  L"qoa", L"shn", L"aa3", L"oma", L"at3", L"xwma",
    L"avi",  L"mkv",  L"mov", L"flv"};

bool IsAudioExtension(const std::wstring& ext) {
    for (auto e : kAllExts)
        if (ext == e) return true;
    return false;
}

const wchar_t* AudioFileFilter() {
    static std::wstring filter;
    if (filter.empty()) {
        std::wstring pat;
        for (auto e : kAllExts) {
            if (!pat.empty()) pat += L";";
            pat += L"*.";
            pat += e;
        }
        pat += L";*.m3u;*.m3u8;*.pls";
        filter = L"All supported (audio + playlists)";
        filter.push_back(0);
        filter += pat;
        filter.push_back(0);
        filter += L"All files (*.*)";
        filter.push_back(0);
        filter += L"*.*";
        filter.push_back(0);
        filter.push_back(0);
    }
    return filter.c_str();
}

static bool IsMfExt(const std::wstring& ext) {
    for (auto e : kMfExts)
        if (ext == e) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Media Foundation decoder (built into Windows)
class MFDecoder : public Decoder {
public:
    ~MFDecoder() override {
        if (reader_) reader_->Release();
    }
    const wchar_t* backend() const override { return L"Media Foundation"; }

    bool open(const std::wstring& path) override {
        HRESULT hr = MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader_);
        if (FAILED(hr)) return false;
        reader_->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
        if (FAILED(reader_->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE))) return false;
        IMFMediaType* t = nullptr;
        MFCreateMediaType(&t);
        t->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        t->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
        hr = reader_->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, t);
        t->Release();
        if (FAILED(hr)) return false;
        if (!readFormat()) return false;
        PROPVARIANT v;
        PropVariantInit(&v);
        if (SUCCEEDED(reader_->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &v)))
            duration = v.uhVal.QuadPart / 1e7;
        PropVariantClear(&v);
        if (SUCCEEDED(reader_->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE,
                                                        MF_PD_AUDIO_ENCODING_BITRATE, &v)))
            bitrate = (int)(v.ulVal / 1000);
        PropVariantClear(&v);
        // Make sure the first sample actually decodes (catches unsupported codecs early).
        if (!fill()) return false;
        return true;
    }

    int read(float* out, int frames) override {
        int done = 0;
        while (done < frames) {
            size_t avail = (pending_.size() - pos_) / channels;
            if (avail == 0) {
                if (eof_ || !fill()) break;
                continue;
            }
            size_t n = std::min(avail, (size_t)(frames - done));
            memcpy(out + (size_t)done * channels, pending_.data() + pos_, n * channels * sizeof(float));
            pos_ += n * channels;
            done += (int)n;
        }
        return done;
    }

    bool seek(double sec) override {
        PROPVARIANT v;
        PropVariantInit(&v);
        v.vt = VT_I8;
        v.hVal.QuadPart = (LONGLONG)(sec * 1e7);
        HRESULT hr = reader_->SetCurrentPosition(GUID_NULL, v);
        pending_.clear();
        pos_ = 0;
        eof_ = false;
        skipTo_ = sec;
        return SUCCEEDED(hr);
    }

private:
    bool readFormat() {
        IMFMediaType* cur = nullptr;
        if (FAILED(reader_->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &cur))) return false;
        UINT32 sr = 0, ch = 0;
        cur->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sr);
        cur->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &ch);
        cur->Release();
        if (!sr || !ch) return false;
        sampleRate = (int)sr;
        channels = (int)ch;
        return true;
    }

    bool fill() {
        pending_.clear();
        pos_ = 0;
        for (int guard = 0; guard < 10000; guard++) {
            DWORD flags = 0;
            LONGLONG ts = 0;
            IMFSample* s = nullptr;
            HRESULT hr = reader_->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, &ts, &s);
            if (FAILED(hr) || (flags & MF_SOURCE_READERF_ERROR)) {
                if (s) s->Release();
                eof_ = true;
                return false;
            }
            if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) readFormat();
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
                if (s) s->Release();
                eof_ = true;
                return false;
            }
            if (!s) continue;
            IMFMediaBuffer* b = nullptr;
            if (SUCCEEDED(s->ConvertToContiguousBuffer(&b))) {
                BYTE* data = nullptr;
                DWORD len = 0;
                if (SUCCEEDED(b->Lock(&data, nullptr, &len))) {
                    size_t n = len / sizeof(float);
                    size_t skip = 0;
                    if (skipTo_ > 0) {  // trim samples before an exact seek target
                        double t0 = ts / 1e7;
                        double frames = (double)n / channels;
                        double need = (skipTo_ - t0) * sampleRate;
                        if (need >= frames) skip = n;
                        else if (need > 0) skip = (size_t)need * channels;
                        if (need < frames) skipTo_ = -1;
                    }
                    if (skip < n) pending_.insert(pending_.end(), (float*)data + skip, (float*)data + n);
                    b->Unlock();
                }
                b->Release();
            }
            s->Release();
            if (!pending_.empty()) return true;
        }
        return false;
    }

    IMFSourceReader* reader_ = nullptr;
    std::vector<float> pending_;
    size_t pos_ = 0;
    bool eof_ = false;
    double skipTo_ = -1;
};

// ---------------------------------------------------------------------------
// ffmpeg helper process
static HANDLE KillOnCloseJob() {
    static HANDLE job = [] {
        HANDLE j = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(j, JobObjectExtendedLimitInformation, &info, sizeof(info));
        return j;
    }();
    return job;
}

const std::wstring& FfmpegPath() {
    static std::wstring path = [] {
        std::wstring exeDir = GetExeDir();
        const std::wstring cands[] = {PathJoin(exeDir, L"ffmpeg.exe"), PathJoin(exeDir, L"ffmpeg\\ffmpeg.exe"),
                                      PathJoin(exeDir, L"ffmpeg\\bin\\ffmpeg.exe")};
        for (auto& c : cands)
            if (FileExists(c)) return c;
        wchar_t buf[MAX_PATH];
        if (SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, buf, nullptr)) return std::wstring(buf);
        if (FileExists(L"C:\\ffmpeg\\bin\\ffmpeg.exe")) return std::wstring(L"C:\\ffmpeg\\bin\\ffmpeg.exe");
        return std::wstring();
    }();
    return path;
}

struct Proc {
    HANDLE process = nullptr;
    HANDLE out = nullptr;  // read end of stdout or stderr pipe
};

// Starts ffmpeg; `captureStderr` selects which stream is piped back.
static bool StartFfmpeg(const std::wstring& args, bool captureStderr, Proc& p) {
    const std::wstring& exe = FfmpegPath();
    if (exe.empty()) return false;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 1 << 20)) return false;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                             OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul;
    si.hStdOutput = captureStderr ? nul : wr;
    si.hStdError = captureStderr ? wr : nul;
    std::wstring cmd = L"\"" + exe + L"\" " + args;
    PROCESS_INFORMATION pi = {};
    BOOL ok = CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr,
                             nullptr, &si, &pi);
    CloseHandle(wr);
    CloseHandle(nul);
    if (!ok) {
        CloseHandle(rd);
        return false;
    }
    AssignProcessToJobObject(KillOnCloseJob(), pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    p.process = pi.hProcess;
    p.out = rd;
    return true;
}

static void StopProc(Proc& p) {
    if (p.process) {
        TerminateProcess(p.process, 0);
        WaitForSingleObject(p.process, 2000);
        CloseHandle(p.process);
    }
    if (p.out) CloseHandle(p.out);
    p = Proc();
}

static std::wstring QuoteArg(const std::wstring& s) { return L"\"" + s + L"\""; }

ProbeInfo FfmpegProbe(const std::wstring& path) {
    ProbeInfo info;
    Proc p;
    if (!StartFfmpeg(L"-hide_banner -nostdin -i " + QuoteArg(path), true, p)) return info;
    std::string out;
    char buf[4096];
    DWORD rd = 0;
    ULONGLONG start = GetTickCount64();
    while (ReadFile(p.out, buf, sizeof(buf), &rd, nullptr) && rd > 0) {
        out.append(buf, rd);
        if (out.size() > (1 << 20) || GetTickCount64() - start > 15000) break;
    }
    StopProc(p);
    std::wstring t = Utf8ToWide(out);
    size_t pos = 0;
    while (pos < t.size()) {
        size_t e = t.find(L'\n', pos);
        if (e == std::wstring::npos) e = t.size();
        std::wstring line = Trim(t.substr(pos, e - pos));
        pos = e + 1;
        if (line.rfind(L"Duration:", 0) == 0) {
            int h = 0, m = 0;
            double s = 0;
            if (swscanf_s(line.c_str() + 9, L" %d:%d:%lf", &h, &m, &s) == 3) info.duration = h * 3600 + m * 60 + s;
            size_t br = line.find(L"bitrate:");
            if (br != std::wstring::npos) info.bitrate = _wtoi(line.c_str() + br + 8);
            continue;
        }
        size_t au = line.find(L"Audio:");
        if (line.rfind(L"Stream #", 0) == 0 && au != std::wstring::npos && !info.sampleRate) {
            size_t hz = line.find(L" Hz", au);
            if (hz != std::wstring::npos) {
                size_t b = line.rfind(L',', hz);
                if (b != std::wstring::npos) info.sampleRate = _wtoi(line.c_str() + b + 1);
                std::wstring rest = line.substr(hz + 3);
                if (rest.find(L"mono") != std::wstring::npos) info.channels = 1;
                else if (rest.find(L"stereo") != std::wstring::npos) info.channels = 2;
                else if (rest.find(L"5.1") != std::wstring::npos) info.channels = 6;
                else if (rest.find(L"7.1") != std::wstring::npos) info.channels = 8;
                else if (rest.find(L"quad") != std::wstring::npos) info.channels = 4;
                else {
                    size_t c = rest.find(L" channels");
                    if (c != std::wstring::npos) {
                        size_t b2 = rest.rfind(L',', c);
                        info.channels = _wtoi(rest.c_str() + (b2 == std::wstring::npos ? 0 : b2 + 1));
                    }
                }
            }
            size_t kb = line.rfind(L" kb/s");
            if (kb != std::wstring::npos && !info.bitrate) {
                size_t b = line.rfind(L',', kb);
                if (b != std::wstring::npos) info.bitrate = _wtoi(line.c_str() + b + 1);
            }
            info.ok = true;
            continue;
        }
        size_t colon = line.find(L':');
        if (colon != std::wstring::npos) {
            std::wstring k = ToLower(Trim(line.substr(0, colon)));
            std::wstring v = Trim(line.substr(colon + 1));
            if (k == L"title" && info.title.empty()) info.title = v;
            else if ((k == L"artist" || k == L"author") && info.artist.empty()) info.artist = v;
            else if (k == L"album" && info.album.empty()) info.album = v;
        }
    }
    return info;
}

class FfmpegDecoder : public Decoder {
public:
    ~FfmpegDecoder() override { StopProc(proc_); }
    const wchar_t* backend() const override { return L"ffmpeg"; }

    bool open(const std::wstring& path) override {
        if (FfmpegPath().empty()) {
            error = L"Format not supported natively and ffmpeg.exe was not found.";
            return false;
        }
        path_ = path;
        ProbeInfo pi = FfmpegProbe(path);
        if (!pi.ok) {
            error = L"ffmpeg could not find an audio stream in this file.";
            return false;
        }
        sampleRate = pi.sampleRate > 0 ? std::min(pi.sampleRate, 192000) : 44100;
        if (sampleRate < 8000) sampleRate = 44100;
        channels = 2;
        duration = pi.duration;
        bitrate = pi.bitrate;
        return start(0);
    }

    int read(float* out, int frames) override {
        const size_t frameBytes = sizeof(float) * 2;
        size_t want = (size_t)frames * frameBytes;
        uint8_t* dst = (uint8_t*)out;
        size_t have = 0;
        if (!carry_.empty()) {
            size_t n = std::min(want, carry_.size());
            memcpy(dst, carry_.data(), n);
            carry_.erase(carry_.begin(), carry_.begin() + n);
            have = n;
        }
        while (have < want && proc_.out) {
            DWORD rd = 0;
            if (!ReadFile(proc_.out, dst + have, (DWORD)(want - have), &rd, nullptr) || rd == 0) {
                StopProc(proc_);
                break;
            }
            have += rd;
        }
        size_t whole = have / frameBytes * frameBytes;
        if (whole < have) carry_.assign(dst + whole, dst + have);
        return (int)(whole / frameBytes);
    }

    bool seek(double sec) override { return start(sec); }

private:
    bool start(double sec) {
        StopProc(proc_);
        carry_.clear();
        std::wstring args = L"-hide_banner -nostdin -loglevel quiet ";
        if (sec > 0) args += Fmt(L"-ss %.3f ", sec);
        args += L"-i " + QuoteArg(path_) + Fmt(L" -map 0:a:0 -vn -sn -dn -ac 2 -ar %d -f f32le pipe:1", sampleRate);
        if (!StartFfmpeg(args, false, proc_)) {
            error = L"Could not start ffmpeg.";
            return false;
        }
        return true;
    }

    std::wstring path_;
    Proc proc_;
    std::vector<uint8_t> carry_;
};

// ---------------------------------------------------------------------------
std::unique_ptr<Decoder> CreateDecoder(const std::wstring& path, std::wstring& error) {
    static std::once_flag mfInit;
    std::call_once(mfInit, [] { MFStartup(MF_VERSION, MFSTARTUP_LITE); });

    std::wstring ext = PathExt(path);
    bool mfFirst = IsMfExt(ext) || IsUrl(path);
    for (int pass = 0; pass < 2; pass++) {
        bool useMf = (pass == 0) == mfFirst;
        std::unique_ptr<Decoder> d;
        if (useMf)
            d = std::make_unique<MFDecoder>();
        else
            d = std::make_unique<FfmpegDecoder>();
        if (d->open(path)) return d;
        if (!d->error.empty()) error = d->error;
    }
    if (error.empty()) error = L"Unsupported or damaged file.";
    if (FfmpegPath().empty())
        error += L"\nTip: put ffmpeg.exe next to RetroAmp.exe to play every audio format (OGG, Opus, APE, tracker modules...).";
    return nullptr;
}
