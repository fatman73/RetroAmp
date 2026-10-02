#pragma once
#include "common.h"

// Tiny INI wrapper (UTF-16 file so non-ASCII paths survive).
class Ini {
public:
    explicit Ini(std::wstring path = L"");
    void setPath(const std::wstring& path);
    const std::wstring& path() const { return path_; }

    std::wstring str(const wchar_t* sec, const std::wstring& key, const std::wstring& def = L"") const;
    int num(const wchar_t* sec, const std::wstring& key, int def) const;
    float flt(const wchar_t* sec, const std::wstring& key, float def) const;
    bool flag(const wchar_t* sec, const std::wstring& key, bool def) const { return num(sec, key, def ? 1 : 0) != 0; }

    void set(const wchar_t* sec, const std::wstring& key, const std::wstring& v);
    void set(const wchar_t* sec, const std::wstring& key, int v);
    void set(const wchar_t* sec, const std::wstring& key, float v);
    void setFlag(const wchar_t* sec, const std::wstring& key, bool v) { set(sec, key, v ? 1 : 0); }
    void remove(const wchar_t* sec, const std::wstring& key);

    std::vector<std::wstring> keys(const wchar_t* sec) const;

private:
    void ensureUnicode() const;
    std::wstring path_;
};

// Winamp EQ library (.eqf / winamp.q1) support
struct EqPreset {
    std::wstring name;
    float bands[10] = {};
    float preamp = 0;
};
const std::vector<EqPreset>& BuiltinEqPresets();
bool LoadEqf(const std::wstring& path, std::vector<EqPreset>& out);
bool SaveEqf(const std::wstring& path, const std::vector<EqPreset>& presets);
