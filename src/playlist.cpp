#include "playlist.h"

#include "audio/decoder.h"

#include <propkey.h>
#include <propvarutil.h>
#include <random>

static std::mt19937& Rng() {
    static std::mt19937 rng((unsigned)GetTickCount64());
    return rng;
}

std::wstring Track::display() const {
    if (!title.empty()) return title;
    if (IsUrl(path)) return path;
    return PathStem(path);
}

int Playlist::add(const std::wstring& path, const std::wstring& title, double length, int insertAt) {
    Track t;
    t.id = nextId_++;
    t.path = path;
    t.title = title;
    t.length = length;
    if (insertAt < 0 || insertAt > size()) insertAt = size();
    tracks.insert(tracks.begin() + insertAt, t);
    if (current >= insertAt) current++;
    if (!shuffleOrder_.empty()) {
        std::uniform_int_distribution<size_t> d(shufflePos_ + 1 >= 0 ? (size_t)(shufflePos_ + 1) : 0, shuffleOrder_.size());
        shuffleOrder_.insert(shuffleOrder_.begin() + d(Rng()), t.id);
    }
    return insertAt;
}

void Playlist::clear() {
    tracks.clear();
    current = -1;
    resetShuffle();
}

template <class F>
void Playlist::keepCurrent(F fn) {
    uint64_t curId = current >= 0 && current < size() ? tracks[current].id : 0;
    fn();
    current = curId ? indexOf(curId) : -1;
}

void Playlist::removeSelected() {
    keepCurrent([&] {
        tracks.erase(std::remove_if(tracks.begin(), tracks.end(), [](const Track& t) { return t.selected; }), tracks.end());
    });
}

void Playlist::crop() {
    keepCurrent([&] {
        tracks.erase(std::remove_if(tracks.begin(), tracks.end(), [](const Track& t) { return !t.selected; }), tracks.end());
    });
}

void Playlist::removeMissing() {
    keepCurrent([&] {
        tracks.erase(std::remove_if(tracks.begin(), tracks.end(),
                                    [](const Track& t) { return !IsUrl(t.path) && !FileExists(t.path); }),
                     tracks.end());
    });
}

void Playlist::selectAll(bool sel) {
    for (auto& t : tracks) t.selected = sel;
}

void Playlist::invertSelection() {
    for (auto& t : tracks) t.selected = !t.selected;
}

int Playlist::selectedCount() const {
    int n = 0;
    for (auto& t : tracks) n += t.selected ? 1 : 0;
    return n;
}

int Playlist::firstSelected() const {
    for (int i = 0; i < size(); i++)
        if (tracks[i].selected) return i;
    return -1;
}

int Playlist::indexOf(uint64_t id) const {
    for (int i = 0; i < size(); i++)
        if (tracks[i].id == id) return i;
    return -1;
}

void Playlist::sort(SortKey key) {
    keepCurrent([&] {
        auto k = [&](const Track& t) {
            switch (key) {
                case ByTitle: return ToLower(t.display());
                case ByFileName: return ToLower(PathFileName(t.path));
                default: return ToLower(t.path);
            }
        };
        std::stable_sort(tracks.begin(), tracks.end(),
                         [&](const Track& a, const Track& b) { return StrCmpLogicalW(k(a).c_str(), k(b).c_str()) < 0; });
    });
}

void Playlist::reverse() {
    keepCurrent([&] { std::reverse(tracks.begin(), tracks.end()); });
}

void Playlist::randomize() {
    keepCurrent([&] { std::shuffle(tracks.begin(), tracks.end(), Rng()); });
}

int Playlist::moveSelected(int delta) {
    if (delta == 0) return 0;
    int first = -1, last = -1;
    for (int i = 0; i < size(); i++)
        if (tracks[i].selected) {
            if (first < 0) first = i;
            last = i;
        }
    if (first < 0) return 0;
    if (first + delta < 0) delta = -first;
    if (last + delta >= size()) delta = size() - 1 - last;
    if (delta == 0) return 0;
    keepCurrent([&] {
        if (delta < 0) {
            for (int i = 0; i < size(); i++)
                if (tracks[i].selected) {
                    int to = i + delta;
                    // bubble up past unselected items only
                    for (int j = i; j > to && j > 0 && !tracks[j - 1].selected; j--) std::swap(tracks[j], tracks[j - 1]);
                }
        } else {
            for (int i = size() - 1; i >= 0; i--)
                if (tracks[i].selected) {
                    int to = i + delta;
                    for (int j = i; j < to && j + 1 < size() && !tracks[j + 1].selected; j++) std::swap(tracks[j], tracks[j + 1]);
                }
        }
    });
    return delta;
}

double Playlist::totalLength(bool selectedOnly, bool* anyUnknown) const {
    double total = 0;
    bool unknown = false;
    for (auto& t : tracks) {
        if (selectedOnly && !t.selected) continue;
        if (t.length >= 0) total += t.length;
        else unknown = true;
    }
    if (anyUnknown) *anyUnknown = unknown;
    return total;
}

void Playlist::resetShuffle() {
    shuffleOrder_.clear();
    shufflePos_ = -1;
}

int Playlist::nextIndex(bool shuffle, bool repeat) {
    if (tracks.empty()) return -1;
    if (!shuffle) {
        if (current + 1 < size()) return current + 1;
        return repeat ? 0 : -1;
    }
    if (shuffleOrder_.size() != tracks.size()) {
        shuffleOrder_.clear();
        for (auto& t : tracks) shuffleOrder_.push_back(t.id);
        std::shuffle(shuffleOrder_.begin(), shuffleOrder_.end(), Rng());
        // start the order with the current track so it isn't repeated soon
        if (current >= 0) {
            auto it = std::find(shuffleOrder_.begin(), shuffleOrder_.end(), tracks[current].id);
            if (it != shuffleOrder_.end()) std::iter_swap(it, shuffleOrder_.begin());
            shufflePos_ = 0;
        } else {
            shufflePos_ = -1;
        }
    }
    shufflePos_++;
    if (shufflePos_ >= (int)shuffleOrder_.size()) {
        if (!repeat) {
            shufflePos_ = (int)shuffleOrder_.size() - 1;
            return -1;
        }
        std::shuffle(shuffleOrder_.begin(), shuffleOrder_.end(), Rng());
        shufflePos_ = 0;
    }
    int idx = indexOf(shuffleOrder_[shufflePos_]);
    return idx;
}

int Playlist::prevIndex(bool shuffle, bool repeat) {
    if (tracks.empty()) return -1;
    if (shuffle && shufflePos_ > 0 && shuffleOrder_.size() == tracks.size()) {
        shufflePos_--;
        return indexOf(shuffleOrder_[shufflePos_]);
    }
    if (current > 0) return current - 1;
    return repeat ? size() - 1 : 0;
}

bool Playlist::IsPlaylistFile(const std::wstring& path) {
    std::wstring e = PathExt(path);
    return e == L"m3u" || e == L"m3u8" || e == L"pls";
}

static std::wstring ResolveEntry(const std::wstring& base, std::wstring p) {
    p = Trim(p);
    if (p.rfind(L"file:///", 0) == 0) {
        wchar_t buf[MAX_PATH * 2];
        DWORD n = (DWORD)std::size(buf);
        if (SUCCEEDED(PathCreateFromUrlW(p.c_str(), buf, &n, 0))) p = buf;
    }
    if (IsUrl(p)) return p;
    for (auto& c : p)
        if (c == L'/') c = L'\\';
    if (PathIsRelativeW(p.c_str())) {
        wchar_t buf[MAX_PATH * 2];
        if (PathCombineW(buf, base.c_str(), p.c_str())) return buf;
    }
    return p;
}

bool Playlist::loadFile(const std::wstring& path, int insertAt) {
    std::vector<uint8_t> raw;
    if (!ReadFileBytes(path, raw)) return false;
    std::wstring text = DecodeText(raw);
    std::wstring base = PathDir(path);
    std::wstring ext = PathExt(path);
    int pos = insertAt < 0 ? size() : insertAt;
    std::vector<std::wstring> lines;
    {
        size_t a = 0;
        while (a < text.size()) {
            size_t b = text.find_first_of(L"\r\n", a);
            if (b == std::wstring::npos) b = text.size();
            lines.push_back(text.substr(a, b - a));
            a = b + 1;
        }
    }
    if (ext == L"pls") {
        std::map<int, std::wstring> files, titles;
        std::map<int, double> lens;
        for (auto& l : lines) {
            size_t eq = l.find(L'=');
            if (eq == std::wstring::npos) continue;
            std::wstring k = ToLower(Trim(l.substr(0, eq))), v = Trim(l.substr(eq + 1));
            auto num = [&](const wchar_t* prefix) { return _wtoi(k.c_str() + wcslen(prefix)); };
            if (k.rfind(L"file", 0) == 0) files[num(L"file")] = v;
            else if (k.rfind(L"title", 0) == 0) titles[num(L"title")] = v;
            else if (k.rfind(L"length", 0) == 0) lens[num(L"length")] = _wtof(v.c_str());
        }
        for (auto& kv : files) {
            double len = lens.count(kv.first) && lens[kv.first] > 0 ? lens[kv.first] : -1;
            add(ResolveEntry(base, kv.second), titles.count(kv.first) ? titles[kv.first] : L"", len, pos++);
        }
        return true;
    }
    std::wstring pendingTitle;
    double pendingLen = -1;
    for (auto& l0 : lines) {
        std::wstring l = Trim(l0);
        if (l.empty()) continue;
        if (l[0] == L'#') {
            if (l.rfind(L"#EXTINF:", 0) == 0) {
                size_t comma = l.find(L',');
                pendingLen = _wtof(l.c_str() + 8);
                if (pendingLen <= 0) pendingLen = -1;
                pendingTitle = comma == std::wstring::npos ? L"" : Trim(l.substr(comma + 1));
            }
            continue;
        }
        add(ResolveEntry(base, l), pendingTitle, pendingLen, pos++);
        pendingTitle.clear();
        pendingLen = -1;
    }
    return true;
}

bool Playlist::saveM3U(const std::wstring& path) const {
    std::wstring out = L"#EXTM3U\r\n";
    for (auto& t : tracks) {
        out += Fmt(L"#EXTINF:%d,", t.length >= 0 ? (int)std::lround(t.length) : -1) + t.display() + L"\r\n";
        out += t.path + L"\r\n";
    }
    std::string u = WideToUtf8(out);
    if (PathExt(path) == L"m3u8") u = "\xEF\xBB\xBF" + u;
    return WriteFileBytes(path, u.data(), u.size());
}

bool Playlist::savePLS(const std::wstring& path) const {
    std::wstring out = L"[playlist]\r\n";
    for (int i = 0; i < size(); i++) {
        auto& t = tracks[i];
        out += Fmt(L"File%d=", i + 1) + t.path + L"\r\n";
        out += Fmt(L"Title%d=", i + 1) + t.display() + L"\r\n";
        out += Fmt(L"Length%d=%d\r\n", i + 1, t.length >= 0 ? (int)std::lround(t.length) : -1);
    }
    out += Fmt(L"NumberOfEntries=%d\r\nVersion=2\r\n", size());
    std::string u = WideToUtf8(out);
    return WriteFileBytes(path, u.data(), u.size());
}

// ---------------------------------------------------------------------------
void CollectAudioFiles(const std::wstring& dir, std::vector<std::wstring>& out) {
    std::vector<std::wstring> files, dirs;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(PathJoin(dir, L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            dirs.push_back(PathJoin(dir, n));
        else if (IsAudioExtension(PathExt(n)))
            files.push_back(PathJoin(dir, n));
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    auto cmp = [](const std::wstring& a, const std::wstring& b) { return StrCmpLogicalW(a.c_str(), b.c_str()) < 0; };
    std::sort(files.begin(), files.end(), cmp);
    std::sort(dirs.begin(), dirs.end(), cmp);
    out.insert(out.end(), files.begin(), files.end());
    for (auto& d : dirs) CollectAudioFiles(d, out);
}

// ---------------------------------------------------------------------------
MetaLoader::~MetaLoader() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        quit_ = true;
        queue_.clear();
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void MetaLoader::start(HWND notify) {
    notify_ = notify;
    thread_ = std::thread(&MetaLoader::worker, this);
}

void MetaLoader::request(uint64_t id, const std::wstring& path) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        queue_.emplace_back(id, path);
    }
    cv_.notify_one();
}

void MetaLoader::cancelAll() {
    std::lock_guard<std::mutex> lk(mtx_);
    queue_.clear();
}

static std::wstring PropString(IPropertyStore* ps, REFPROPERTYKEY key) {
    PROPVARIANT v;
    PropVariantInit(&v);
    std::wstring out;
    if (SUCCEEDED(ps->GetValue(key, &v)) && v.vt != VT_EMPTY) {
        PWSTR s = nullptr;
        if (SUCCEEDED(PropVariantToStringAlloc(v, &s)) && s) {
            out = s;
            CoTaskMemFree(s);
        }
    }
    PropVariantClear(&v);
    return Trim(out);
}

static uint64_t PropU64(IPropertyStore* ps, REFPROPERTYKEY key) {
    PROPVARIANT v;
    PropVariantInit(&v);
    ULONGLONG out = 0;
    if (SUCCEEDED(ps->GetValue(key, &v)) && v.vt != VT_EMPTY) PropVariantToUInt64(v, &out);
    PropVariantClear(&v);
    return out;
}

MetaResult MetaLoader::ReadNow(uint64_t id, const std::wstring& path) {
    MetaResult r{id, L"", -1, 0, 0, 0};
    if (IsUrl(path)) return r;
    std::wstring title, artist;
    IPropertyStore* ps = nullptr;
    if (SUCCEEDED(SHGetPropertyStoreFromParsingName(path.c_str(), nullptr, GPS_DEFAULT, IID_PPV_ARGS(&ps)))) {
        title = PropString(ps, PKEY_Title);
        artist = PropString(ps, PKEY_Music_Artist);
        if (artist.empty()) artist = PropString(ps, PKEY_Music_AlbumArtist);
        uint64_t dur = PropU64(ps, PKEY_Media_Duration);
        if (dur > 0) r.length = dur / 1e7;
        r.bitrate = (int)(PropU64(ps, PKEY_Audio_EncodingBitrate) / 1000);
        r.sampleRate = (int)PropU64(ps, PKEY_Audio_SampleRate);
        r.channels = (int)PropU64(ps, PKEY_Audio_ChannelCount);
        ps->Release();
    }
    if (r.length < 0 && !FfmpegPath().empty()) {
        ProbeInfo pi = FfmpegProbe(path);
        if (pi.ok) {
            if (title.empty()) title = pi.title;
            if (artist.empty()) artist = pi.artist;
            r.length = pi.duration > 0 ? pi.duration : -1;
            r.bitrate = pi.bitrate;
            r.sampleRate = pi.sampleRate;
            r.channels = pi.channels;
        }
    }
    if (!title.empty() && !artist.empty())
        r.title = artist + L" - " + title;
    else if (!title.empty())
        r.title = title;
    return r;
}

void MetaLoader::worker() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    for (;;) {
        std::pair<uint64_t, std::wstring> job;
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [&] { return quit_ || !queue_.empty(); });
            if (quit_) break;
            job = queue_.front();
            queue_.pop_front();
        }
        MetaResult* r = new MetaResult(ReadNow(job.first, job.second));
        if (!PostMessageW(notify_, WM_META_READY, 0, (LPARAM)r)) delete r;
    }
    CoUninitialize();
}
