#pragma once
#include "../common.h"

// Interleaved float decoder.
class Decoder {
public:
    virtual ~Decoder() = default;
    virtual bool open(const std::wstring& path) = 0;
    // Reads up to `frames` frames (frame = `channels` floats). Returns 0 at end of stream.
    virtual int read(float* out, int frames) = 0;
    virtual bool seek(double seconds) = 0;
    virtual const wchar_t* backend() const = 0;

    int sampleRate = 44100;
    int channels = 2;
    double duration = 0;  // seconds, 0 = unknown (streams)
    int bitrate = 0;      // kbps
    std::wstring error;
};

std::unique_ptr<Decoder> CreateDecoder(const std::wstring& path, std::wstring& error);

// Path of ffmpeg.exe (next to the player, in PATH or C:\ffmpeg\bin), empty when missing.
const std::wstring& FfmpegPath();

struct ProbeInfo {
    std::wstring title, artist, album;
    double duration = 0;
    int sampleRate = 0, channels = 0, bitrate = 0;
    bool ok = false;
};
// Reads stream information using `ffmpeg -i`.
ProbeInfo FfmpegProbe(const std::wstring& path);

// All extensions the player accepts when adding files / scanning folders.
bool IsAudioExtension(const std::wstring& ext);
const wchar_t* AudioFileFilter();  // for the open dialog
