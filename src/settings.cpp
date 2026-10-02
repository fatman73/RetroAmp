#include "settings.h"

Ini::Ini(std::wstring path) : path_(std::move(path)) {}

void Ini::setPath(const std::wstring& path) { path_ = path; }

void Ini::ensureUnicode() const {
    // Win32 profile APIs keep a file Unicode only if it already starts with a UTF-16 BOM.
    if (!FileExists(path_)) {
        const uint8_t bom[2] = {0xFF, 0xFE};
        WriteFileBytes(path_, bom, 2);
    }
}

std::wstring Ini::str(const wchar_t* sec, const std::wstring& key, const std::wstring& def) const {
    std::vector<wchar_t> buf(32768);
    GetPrivateProfileStringW(sec, key.c_str(), def.c_str(), buf.data(), (DWORD)buf.size(), path_.c_str());
    return buf.data();
}

int Ini::num(const wchar_t* sec, const std::wstring& key, int def) const {
    std::wstring s = str(sec, key, L"");
    if (s.empty()) return def;
    return _wtoi(s.c_str());
}

float Ini::flt(const wchar_t* sec, const std::wstring& key, float def) const {
    std::wstring s = str(sec, key, L"");
    if (s.empty()) return def;
    return (float)_wtof(s.c_str());
}

void Ini::set(const wchar_t* sec, const std::wstring& key, const std::wstring& v) {
    ensureUnicode();
    WritePrivateProfileStringW(sec, key.c_str(), v.c_str(), path_.c_str());
}

void Ini::set(const wchar_t* sec, const std::wstring& key, int v) { set(sec, key, std::to_wstring(v)); }

void Ini::set(const wchar_t* sec, const std::wstring& key, float v) { set(sec, key, Fmt(L"%.3f", v)); }

void Ini::remove(const wchar_t* sec, const std::wstring& key) {
    WritePrivateProfileStringW(sec, key.c_str(), nullptr, path_.c_str());
}

std::vector<std::wstring> Ini::keys(const wchar_t* sec) const {
    std::vector<wchar_t> buf(65536);
    DWORD n = GetPrivateProfileStringW(sec, nullptr, L"", buf.data(), (DWORD)buf.size(), path_.c_str());
    std::vector<std::wstring> out;
    for (DWORD i = 0; i < n;) {
        std::wstring k(&buf[i]);
        if (k.empty()) break;
        out.push_back(k);
        i += (DWORD)k.size() + 1;
    }
    return out;
}

// ---------------------------------------------------------------------------
const std::vector<EqPreset>& BuiltinEqPresets() {
    static std::vector<EqPreset> list = [] {
        struct P {
            const wchar_t* n;
            float pre;
            float b[10];
        };
        // Classic Winamp preset curves (dB)
        static const P data[] = {
            {L"Classical", 0, {0, 0, 0, 0, 0, 0, -7.2f, -7.2f, -7.2f, -9.6f}},
            {L"Club", 0, {0, 0, 8, 5.6f, 5.6f, 5.6f, 3.2f, 0, 0, 0}},
            {L"Dance", -2, {9.6f, 7.2f, 2.4f, 0, 0, -5.6f, -7.2f, -7.2f, 0, 0}},
            {L"Full Bass", -3, {-8, 9.6f, 9.6f, 5.6f, 1.6f, -4, -8, -10.4f, -11.2f, -11.2f}},
            {L"Full Bass & Treble", -3, {7.2f, 5.6f, 0, -7.2f, -4.8f, 1.6f, 8, 11.2f, 12, 12}},
            {L"Full Treble", -4, {-9.6f, -9.6f, -9.6f, -4, 2.4f, 11.2f, 12, 12, 12, 12}},
            {L"Laptop Speakers / Headphones", -3, {4.8f, 11.2f, 5.6f, -3.2f, -2.4f, 1.6f, 4.8f, 9.6f, 12, 12}},
            {L"Large Hall", -2, {10.4f, 10.4f, 5.6f, 5.6f, 0, -4.8f, -4.8f, -4.8f, 0, 0}},
            {L"Live", 0, {-4.8f, 0, 4, 5.6f, 5.6f, 5.6f, 4, 2.4f, 2.4f, 2.4f}},
            {L"Party", -2, {7.2f, 7.2f, 0, 0, 0, 0, 0, 0, 7.2f, 7.2f}},
            {L"Pop", -1, {-1.6f, 4.8f, 7.2f, 8, 5.6f, 0, -2.4f, -2.4f, -1.6f, -1.6f}},
            {L"Reggae", 0, {0, 0, 0, -5.6f, 0, 6.4f, 6.4f, 0, 0, 0}},
            {L"Rock", -3, {8, 4.8f, -5.6f, -8, -3.2f, 4, 8.8f, 11.2f, 11.2f, 11.2f}},
            {L"Ska", -2, {-2.4f, -4.8f, -4, 0, 4, 5.6f, 8.8f, 9.6f, 11.2f, 9.6f}},
            {L"Soft", -2, {4.8f, 1.6f, 0, -2.4f, 0, 4, 8, 9.6f, 11.2f, 12}},
            {L"Soft Rock", 0, {4, 4, 2.4f, 0, -4, -5.6f, -3.2f, 0, 2.4f, 8.8f}},
            {L"Techno", -2, {8, 5.6f, 0, -5.6f, -4.8f, 0, 8, 9.6f, 9.6f, 8.8f}},
            {L"Bass Boost (Heavy)", -4, {12, 10, 7, 3, 0, 0, 0, 0, 0, 0}},
            {L"Subwoofer", -4, {12, 8, 2, -2, -2, 0, 0, 0, 0, 0}},
            {L"Car Audio", -3, {9, 7, 3, -2, -3, 0, 3, 5, 6, 6}},
            {L"Vocal / Podcast", 0, {-6, -4, -1, 2, 5, 5, 3, 0, -2, -3}},
        };
        std::vector<EqPreset> v;
        for (auto& p : data) {
            EqPreset e;
            e.name = p.n;
            e.preamp = p.pre;
            memcpy(e.bands, p.b, sizeof(e.bands));
            v.push_back(e);
        }
        return v;
    }();
    return list;
}

static const char kEqfHeader[] = "Winamp EQ library file v1.1\x1a!--";

bool LoadEqf(const std::wstring& path, std::vector<EqPreset>& out) {
    std::vector<uint8_t> b;
    if (!ReadFileBytes(path, b)) return false;
    size_t hl = sizeof(kEqfHeader) - 1;
    if (b.size() < hl || memcmp(b.data(), "Winamp EQ library file", 22) != 0) return false;
    size_t pos = hl;
    while (pos + 257 + 11 <= b.size()) {
        EqPreset p;
        std::string name((const char*)&b[pos], strnlen((const char*)&b[pos], 257));
        p.name = AnsiToWide(name);
        pos += 257;
        auto toDb = [](uint8_t v) { return (63.0f - std::min<uint8_t>(v, 63)) / 63.0f * 24.0f - 12.0f; };
        for (int i = 0; i < 10; i++) p.bands[i] = toDb(b[pos + i]);
        p.preamp = toDb(b[pos + 10]);
        pos += 11;
        out.push_back(p);
    }
    return true;
}

bool SaveEqf(const std::wstring& path, const std::vector<EqPreset>& presets) {
    std::vector<uint8_t> b(kEqfHeader, kEqfHeader + sizeof(kEqfHeader) - 1);
    for (auto& p : presets) {
        std::vector<uint8_t> name(257, 0);
        int n = WideCharToMultiByte(CP_ACP, 0, p.name.c_str(), (int)p.name.size(), (char*)name.data(), 256, nullptr, nullptr);
        (void)n;
        b.insert(b.end(), name.begin(), name.end());
        auto toByte = [](float db) { return (uint8_t)Clamp((int)std::lround(63 - (db + 12) / 24 * 63), 0, 63); };
        for (int i = 0; i < 10; i++) b.push_back(toByte(p.bands[i]));
        b.push_back(toByte(p.preamp));
    }
    return WriteFileBytes(path, b.data(), b.size());
}
