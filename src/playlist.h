#pragma once
#include "common.h"

#include <condition_variable>
#include <deque>

struct Track {
    uint64_t id = 0;
    std::wstring path;
    std::wstring title;    // "Artist - Title" when known
    double length = -1;    // seconds, -1 unknown
    int bitrate = 0, sampleRate = 0, channels = 0;
    bool metaDone = false;
    bool selected = false;

    std::wstring display() const;
};

class Playlist {
public:
    std::vector<Track> tracks;
    int current = -1;  // index of the current track

    int size() const { return (int)tracks.size(); }
    int add(const std::wstring& path, const std::wstring& title = L"", double length = -1, int insertAt = -1);
    void clear();
    void removeSelected();
    void crop();
    void removeMissing();
    void selectAll(bool sel);
    void invertSelection();
    int selectedCount() const;
    int firstSelected() const;
    int indexOf(uint64_t id) const;

    enum SortKey { ByTitle, ByFileName, ByPath };
    void sort(SortKey key);
    void reverse();
    void randomize();
    // Moves the selected block by delta rows (drag & drop reordering). Returns actual delta.
    int moveSelected(int delta);

    double totalLength(bool selectedOnly, bool* anyUnknown) const;

    int nextIndex(bool shuffle, bool repeat);
    int prevIndex(bool shuffle, bool repeat);
    void resetShuffle();

    bool loadFile(const std::wstring& path, int insertAt = -1);  // m3u / m3u8 / pls
    bool saveM3U(const std::wstring& path) const;
    bool savePLS(const std::wstring& path) const;

    static bool IsPlaylistFile(const std::wstring& path);

private:
    uint64_t nextId_ = 1;
    std::vector<uint64_t> shuffleOrder_;
    int shufflePos_ = -1;
    template <class F>
    void keepCurrent(F fn);
};

// Background metadata reader (Windows property system, ffmpeg fallback).
struct MetaResult {
    uint64_t id;
    std::wstring title;
    double length;
    int bitrate, sampleRate, channels;
};

class MetaLoader {
public:
    ~MetaLoader();
    void start(HWND notify);
    void request(uint64_t id, const std::wstring& path);
    void cancelAll();
    static MetaResult ReadNow(uint64_t id, const std::wstring& path);

private:
    void worker();
    HWND notify_ = nullptr;
    std::thread thread_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<std::pair<uint64_t, std::wstring>> queue_;
    bool quit_ = false;
};

// Recursively collects audio files from a folder (sorted).
void CollectAudioFiles(const std::wstring& dir, std::vector<std::wstring>& out);
