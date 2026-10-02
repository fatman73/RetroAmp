#include "common.h"

#include <cstdarg>
#include <cwctype>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "normaliz.lib")

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring AnsiToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_ACP, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

std::wstring ToLower(std::wstring s) {
    if (!s.empty()) CharLowerBuffW(&s[0], (DWORD)s.size());
    return s;
}

std::wstring Trim(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a])) a++;
    while (b > a && iswspace(s[b - 1])) b--;
    return s.substr(a, b - a);
}

std::wstring GetExeDir() {
    wchar_t buf[MAX_PATH * 2];
    GetModuleFileNameW(nullptr, buf, (DWORD)std::size(buf));
    return PathDir(buf);
}

std::wstring GetAppDataDir() {
    PWSTR p = nullptr;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &p))) {
        dir = PathJoin(p, APP_NAME);
        CoTaskMemFree(p);
    } else {
        dir = PathJoin(GetExeDir(), L"data");
    }
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::wstring PathExt(const std::wstring& p) {
    size_t slash = p.find_last_of(L"\\/");
    size_t dot = p.find_last_of(L'.');
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) return L"";
    return ToLower(p.substr(dot + 1));
}

std::wstring PathFileName(const std::wstring& p) {
    size_t slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? p : p.substr(slash + 1);
}

std::wstring PathStem(const std::wstring& p) {
    std::wstring f = PathFileName(p);
    size_t dot = f.find_last_of(L'.');
    return dot == std::wstring::npos || dot == 0 ? f : f.substr(0, dot);
}

std::wstring PathDir(const std::wstring& p) {
    size_t slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"" : p.substr(0, slash);
}

std::wstring PathJoin(const std::wstring& a, const std::wstring& b) {
    if (a.empty()) return b;
    if (a.back() == L'\\' || a.back() == L'/') return a + b;
    return a + L"\\" + b;
}

bool FileExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool DirExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

bool IsUrl(const std::wstring& p) {
    return p.find(L"://") != std::wstring::npos && p.find(L"://") < 10;
}

bool ReadFileBytes(const std::wstring& path, std::vector<uint8_t>& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz) || sz.QuadPart > (1LL << 30)) {
        CloseHandle(h);
        return false;
    }
    out.resize((size_t)sz.QuadPart);
    DWORD rd = 0;
    bool ok = out.empty() || (ReadFile(h, out.data(), (DWORD)out.size(), &rd, nullptr) && rd == out.size());
    CloseHandle(h);
    return ok;
}

bool WriteFileBytes(const std::wstring& path, const void* data, size_t size) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0;
    bool ok = WriteFile(h, data, (DWORD)size, &wr, nullptr) && wr == size;
    CloseHandle(h);
    return ok;
}

static bool IsValidUtf8(const uint8_t* p, size_t n) {
    size_t i = 0;
    while (i < n) {
        uint8_t c = p[i];
        int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        if (!len || i + len > n) return false;
        for (int k = 1; k < len; k++)
            if ((p[i + k] & 0xC0) != 0x80) return false;
        i += len;
    }
    return true;
}

std::wstring DecodeText(const std::vector<uint8_t>& b) {
    if (b.size() >= 2 && b[0] == 0xFF && b[1] == 0xFE)
        return std::wstring((const wchar_t*)(b.data() + 2), (b.size() - 2) / 2);
    if (b.size() >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF)
        return Utf8ToWide(std::string((const char*)b.data() + 3, b.size() - 3));
    std::string s((const char*)b.data(), b.size());
    if (IsValidUtf8(b.data(), b.size())) return Utf8ToWide(s);
    return AnsiToWide(s);
}

std::wstring FormatTime(double sec, bool forceHours) {
    if (sec < 0 || !std::isfinite(sec)) sec = 0;
    long long t = (long long)sec;
    long long h = t / 3600, m = (t / 60) % 60, s = t % 60;
    wchar_t buf[64];
    if (h > 0 || forceHours)
        swprintf_s(buf, L"%lld:%02lld:%02lld", h, m, s);
    else
        swprintf_s(buf, L"%lld:%02lld", m, s);
    return buf;
}

std::wstring Fmt(const wchar_t* fmt, ...) {
    wchar_t buf[2048];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
    va_end(ap);
    return buf;
}

uint64_t HashString(const std::wstring& s) {
    uint64_t h = 1469598103934665603ULL;
    for (wchar_t c : s) {
        h ^= (uint64_t)c;
        h *= 1099511628211ULL;
    }
    return h;
}

std::string ToSkinText(const std::wstring& in) {
    // Decompose accented characters (NFKD) and drop combining marks.
    std::wstring s = in;
    int n = NormalizeString(NormalizationKD, in.c_str(), (int)in.size(), nullptr, 0);
    if (n > 0) {
        std::wstring buf(n, 0);
        n = NormalizeString(NormalizationKD, in.c_str(), (int)in.size(), &buf[0], n);
        if (n > 0) s.assign(buf.c_str(), n);
    }
    std::string out;
    out.reserve(s.size());
    for (wchar_t c : s) {
        if (c >= 0x300 && c <= 0x36F) continue;  // combining marks
        switch (c) {
            case L'ł': case L'Ł': out += 'L'; continue;  // ł Ł
            case L'đ': case L'Đ': out += 'D'; continue;
            case L'ø': case L'Ø': out += 'O'; continue;
            case L'ß': out += "SS"; continue;
            case L'æ': case L'Æ': out += "AE"; continue;
            case L'œ': case L'Œ': out += "OE"; continue;
            case L'…': out += (char)0x85; continue;
            case L'–': case L'—': out += '-'; continue;
            case L'‘': case L'’': out += '\''; continue;
            case L'“': case L'”': out += '"'; continue;
            case L'\t': out += ' '; continue;
            default: break;
        }
        if (c < 128)
            out += (char)toupper((int)c);
        else
            out += '?';
    }
    return out;
}
