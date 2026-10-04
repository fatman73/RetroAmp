#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <commdlg.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define APP_NAME L"RetroAmp"
#include "version.h"
#define APP_VERSION RETROAMP_VERSION_W

// Custom window messages
enum : UINT {
    WM_PLAYER_EVENT = WM_APP + 1,  // wParam = PlayerEvent, lParam = session id
    WM_META_READY = WM_APP + 2,    // lParam = MetaResult*
    WM_TRAYICON = WM_APP + 3,
};

template <class T>
inline T Clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

std::wstring Utf8ToWide(const std::string& s);
std::string WideToUtf8(const std::wstring& s);
std::wstring AnsiToWide(const std::string& s);
std::wstring ToLower(std::wstring s);
std::wstring Trim(const std::wstring& s);

std::wstring GetExeDir();
std::wstring GetAppDataDir();  // %APPDATA%\RetroAmp (created on demand)
std::wstring PathExt(const std::wstring& p);      // lower-case, without dot
std::wstring PathFileName(const std::wstring& p);
std::wstring PathStem(const std::wstring& p);
std::wstring PathDir(const std::wstring& p);
std::wstring PathJoin(const std::wstring& a, const std::wstring& b);
bool FileExists(const std::wstring& p);
bool DirExists(const std::wstring& p);
bool IsUrl(const std::wstring& p);

bool ReadFileBytes(const std::wstring& path, std::vector<uint8_t>& out);
bool WriteFileBytes(const std::wstring& path, const void* data, size_t size);
std::wstring DecodeText(const std::vector<uint8_t>& bytes);  // UTF-8 / UTF-16 / ANSI autodetect

std::wstring FormatTime(double seconds, bool forceHours = false);  // m:ss
std::wstring Fmt(const wchar_t* fmt, ...);
uint64_t HashString(const std::wstring& s);

// Converts a string to the subset of characters the Winamp bitmap font can show.
std::string ToSkinText(const std::wstring& s);
