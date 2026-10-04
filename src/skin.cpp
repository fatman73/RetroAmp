#include "skin.h"

#include "zip.h"

static const char* kBmpNames[SB_COUNT] = {"main.bmp",     "cbuttons.bmp", "titlebar.bmp", "shufrep.bmp", "text.bmp",
                                          "numbers.bmp",  "nums_ex.bmp",  "volume.bmp",   "balance.bmp", "monoster.bmp",
                                          "playpaus.bmp", "posbar.bmp",   "eqmain.bmp",   "pledit.bmp"};
static const char* kTextNames[] = {"pledit.txt", "viscolor.txt", "region.txt"};

static std::string LowerA(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

static std::string BaseNameA(const std::string& p) {
    size_t s = p.find_last_of("\\/");
    std::string b = s == std::string::npos ? p : p.substr(s + 1);
    // HD skins may store the sprite sheets as PNG: main.png == main.bmp
    if (b.size() > 4 && b.compare(b.size() - 4, 4, ".png") == 0) b = b.substr(0, b.size() - 4) + ".bmp";
    return b;
}

void Skin::prepare(int scale) {
    if (prepared_ == scale) return;
    for (int i = 0; i < SB_COUNT; i++) scaled_[i] = RescaleImage(bmp_[i], scale);
    animScaled_.resize(animImg_.size());
    for (size_t i = 0; i < animImg_.size(); i++) animScaled_[i] = RescaleImage(animImg_[i], scale);
    prepared_ = scale;
}

bool Skin::loadDefault(HINSTANCE inst) {
    FileMap files;
    auto addRes = [&](const char* fname) {
        std::string resName = fname;
        for (auto& c : resName) c = c == '.' ? '_' : (char)toupper((unsigned char)c);
        HRSRC r = FindResourceA(inst, resName.c_str(), MAKEINTRESOURCEA(10) /*RT_RCDATA*/);
        if (!r) return;
        HGLOBAL g = LoadResource(inst, r);
        DWORD sz = SizeofResource(inst, r);
        const uint8_t* p = (const uint8_t*)LockResource(g);
        if (p && sz) files[fname] = std::vector<uint8_t>(p, p + sz);
    };
    for (auto n : kBmpNames) addRes(n);
    for (auto n : kTextNames) addRes(n);
    for (auto n : {"anim.txt", "panel.bmp", "reel1.bmp", "reel2.bmp", "vuleft.bmp", "vuright.bmp", "led.bmp", "dspbody.bmp", "dspbtn.bmp"}) addRes(n);
    if (files.empty()) {
        // Development fallback: load from <exe>\skins\RetroBlue
        std::wstring dir = PathJoin(GetExeDir(), L"skins\\RetroBlue");
        for (auto n : kBmpNames) {
            std::vector<uint8_t> b;
            std::wstring stem = PathStem(Utf8ToWide(n));
            if (ReadFileBytes(PathJoin(dir, stem + L".png"), b) || ReadFileBytes(PathJoin(dir, stem + L".bmp"), b))
                files[n] = b;
        }
        for (auto n : kTextNames) {
            std::vector<uint8_t> b;
            if (ReadFileBytes(PathJoin(dir, Utf8ToWide(n)), b)) files[n] = b;
        }
    }
    name = L"Retro Blue (default)";
    return apply(files, true);
}

bool Skin::loadFrom(const std::wstring& path, std::wstring& error) {
    FileMap files;
    auto want = [](const std::string& base) {
        for (auto n : kBmpNames)
            if (base == n) return true;
        for (auto n : kTextNames)
            if (base == n) return true;
        // animation extension: anim.txt and any extra image it may reference
        if (base == "anim.txt") return true;
        return base.size() > 4 && base.compare(base.size() - 4, 4, ".bmp") == 0;
    };
    if (DirExists(path)) {
        std::function<void(const std::wstring&, int)> scan = [&](const std::wstring& dir, int depth) {
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW(PathJoin(dir, L"*").c_str(), &fd);
            if (h == INVALID_HANDLE_VALUE) return;
            do {
                std::wstring n = fd.cFileName;
                if (n == L"." || n == L"..") continue;
                std::wstring full = PathJoin(dir, n);
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (depth < 3) scan(full, depth + 1);
                } else {
                    std::string base = BaseNameA(LowerA(WideToUtf8(n)));
                    if (want(base) && !files.count(base)) {
                        std::vector<uint8_t> b;
                        if (ReadFileBytes(full, b)) files[base] = std::move(b);
                    }
                }
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        };
        scan(path, 0);
    } else {
        std::vector<uint8_t> raw;
        if (!ReadFileBytes(path, raw)) {
            error = L"Cannot read file.";
            return false;
        }
        std::vector<ZipEntry> entries;
        if (!ZipReadAll(raw, entries)) {
            error = L"The file is not a valid ZIP/WSZ archive.";
            return false;
        }
        for (auto& e : entries) {
            std::string base = BaseNameA(LowerA(e.name));
            if (want(base) && !files.count(base)) files[base] = std::move(e.data);
        }
    }
    if (!files.count("main.bmp")) {
        error = L"This is not a Winamp 2.x skin (main.bmp not found).";
        return false;
    }
    // Start from the default skin so that missing parts are still drawn.
    Skin def;
    def.loadDefault(GetModuleHandleW(nullptr));
    *this = def;
    if (!apply(files, false)) {
        error = L"The skin bitmaps could not be decoded.";
        return false;
    }
    name = PathStem(path);
    return true;
}

bool Skin::apply(const FileMap& files, bool isDefault) {
    bool provided[SB_COUNT] = {};
    for (int i = 0; i < SB_COUNT; i++) {
        auto it = files.find(kBmpNames[i]);
        if (it == files.end()) continue;
        Image im;
        if (LoadImageFromMemory(it->second.data(), it->second.size(), im)) {
            bmp_[i] = std::move(im);
            provided[i] = true;
        }
    }
    // HD skins: MAIN is an exact multiple of 275x116; every sheet of that size is HD too.
    int k = 1;
    if (bmp_[SB_MAIN].valid() && bmp_[SB_MAIN].w % 275 == 0 && bmp_[SB_MAIN].h == bmp_[SB_MAIN].w / 275 * 116)
        k = Clamp(bmp_[SB_MAIN].w / 275, 1, 8);
    for (int i = 0; i < SB_COUNT; i++) {
        if (!provided[i]) continue;
        Image& im = bmp_[i];
        im.scale = (k > 1 && im.w % k == 0 && im.h % k == 0) ? k : 1;
    }
    prepared_ = 0;
    if (!isDefault) {
        // Winamp falls back to VOLUME.BMP when BALANCE.BMP is missing.
        if (provided[SB_VOLUME] && !provided[SB_BALANCE]) bmp_[SB_BALANCE] = bmp_[SB_VOLUME];
        useNumsEx = provided[SB_NUMS_EX] || !provided[SB_NUMBERS];
        regMain = regMainShade = regEq = Region();
    } else {
        useNumsEx = bmp_[SB_NUMS_EX].valid();
    }
    // Default visualisation colours (Winamp base skin) before parsing viscolor.txt
    static const uint32_t kVis[24] = {0x000000, 0x181818, 0xEF3110, 0xCE2921, 0xD65A00, 0xD66600, 0xD67300, 0xC67B08,
                                      0xDEA518, 0xD6B521, 0xBDDE29, 0x94DE21, 0x29CE10, 0x32BE10, 0x239A0C, 0x29940C,
                                      0x18840C, 0x29CE10, 0xFFFFFF, 0xD6D6DE, 0xB5B5BD, 0xA5A5AD, 0x9C9CA5, 0x969696};
    if (!isDefault || !files.count("viscolor.txt")) memcpy(vis, kVis, sizeof(vis));
    if (!isDefault) {
        plNormal = 0x00FF00;
        plCurrent = 0xFFFFFF;
        plNormalBG = 0x000000;
        plSelectedBG = 0x0000C6;
        plFont = L"Arial";
    }
    auto txt = [&](const char* n) -> std::wstring {
        auto it = files.find(n);
        return it == files.end() ? L"" : DecodeText(it->second);
    };
    if (files.count("pledit.txt")) parsePledit(txt("pledit.txt"));
    if (files.count("viscolor.txt")) parseViscolor(txt("viscolor.txt"));
    if (files.count("region.txt")) parseRegion(txt("region.txt"));
    anims.clear();
    buttons.clear();
    dspStyle = DspStyle();
    hasTurnSprite = false;
    animImg_.clear();
    animScaled_.clear();
    hasPanel = false;
    panelImage = -1;
    if (files.count("anim.txt")) parseAnim(txt("anim.txt"), files, k);
    return bmp_[SB_MAIN].valid();
}

static bool ParseColor(std::wstring v, uint32_t& out) {
    v = Trim(v);
    if (!v.empty() && v[0] == L'#') v = v.substr(1);
    if (v.size() < 6) return false;
    wchar_t* end = nullptr;
    unsigned long c = wcstoul(v.substr(0, 6).c_str(), &end, 16);
    if (end == nullptr || *end != 0) return false;
    out = (uint32_t)c & 0xFFFFFF;
    return true;
}

static std::vector<std::wstring> SplitLines(const std::wstring& t) {
    std::vector<std::wstring> lines;
    size_t a = 0;
    while (a <= t.size()) {
        size_t b = t.find_first_of(L"\r\n", a);
        if (b == std::wstring::npos) b = t.size();
        lines.push_back(t.substr(a, b - a));
        a = b + 1;
    }
    return lines;
}

void Skin::parsePledit(const std::wstring& t) {
    for (auto& line : SplitLines(t)) {
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring k = ToLower(Trim(line.substr(0, eq)));
        std::wstring v = Trim(line.substr(eq + 1));
        if (k == L"normal") ParseColor(v, plNormal);
        else if (k == L"current") ParseColor(v, plCurrent);
        else if (k == L"normalbg") ParseColor(v, plNormalBG);
        else if (k == L"selectedbg") ParseColor(v, plSelectedBG);
        else if (k == L"font" && !v.empty()) plFont = v;
    }
}

void Skin::parseViscolor(const std::wstring& t) {
    int idx = 0;
    for (auto& line : SplitLines(t)) {
        if (idx >= 24) break;
        std::wstring l = line;
        size_t c = l.find(L"//");
        if (c != std::wstring::npos) l = l.substr(0, c);
        int v[3], n = 0;
        const wchar_t* p = l.c_str();
        while (*p && n < 3) {
            if (iswdigit(*p)) {
                v[n++] = (int)wcstol(p, (wchar_t**)&p, 10);
            } else {
                p++;
            }
        }
        if (n == 3) vis[idx++] = RGBx(Clamp(v[0], 0, 255), Clamp(v[1], 0, 255), Clamp(v[2], 0, 255));
    }
}

void Skin::parseRegion(const std::wstring& t) {
    Region* cur = nullptr;
    std::vector<int> counts;
    std::vector<int> nums;
    auto flush = [&]() {
        if (cur && !counts.empty()) {
            Region r;
            size_t total = 0;
            for (int c : counts) total += c;
            if (nums.size() >= total * 2) {
                r.counts = counts;
                for (size_t i = 0; i < total; i++) r.pts.push_back({nums[i * 2], nums[i * 2 + 1]});
                *cur = r;
            }
        }
        counts.clear();
        nums.clear();
    };
    auto parseInts = [](const std::wstring& s) {
        std::vector<int> out;
        const wchar_t* p = s.c_str();
        while (*p) {
            if (iswdigit(*p) || (*p == L'-' && iswdigit(p[1]))) {
                out.push_back((int)wcstol(p, (wchar_t**)&p, 10));
            } else {
                p++;
            }
        }
        return out;
    };
    for (auto& line : SplitLines(t)) {
        std::wstring l = Trim(line);
        if (l.empty() || l[0] == L';') continue;
        if (l[0] == L'[') {
            flush();
            std::wstring sec = ToLower(l);
            cur = sec == L"[normal]" ? &regMain : sec == L"[windowshade]" ? &regMainShade : sec == L"[equalizer]" ? &regEq : nullptr;
            continue;
        }
        size_t eq = l.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring k = ToLower(Trim(l.substr(0, eq)));
        if (k == L"numpoints") counts = parseInts(l.substr(eq + 1));
        else if (k == L"pointlist") {
            auto v = parseInts(l.substr(eq + 1));
            nums.insert(nums.end(), v.begin(), v.end());
        }
    }
    flush();
}

static bool CharCell(unsigned char ch, int& col, int& row) {
    if (ch >= 'A' && ch <= 'Z') { col = ch - 'A'; row = 0; return true; }
    if (ch >= 'a' && ch <= 'z') { col = ch - 'a'; row = 0; return true; }
    if (ch >= '0' && ch <= '9') { col = ch - '0'; row = 1; return true; }
    row = 1;
    switch (ch) {
        case '"': col = 26; row = 0; return true;
        case '@': col = 27; row = 0; return true;
        case ' ': col = 30; row = 0; return true;
        case 0x85: col = 10; return true;
        case '.': col = 11; return true;
        case ':': case ';': col = 12; return true;
        case '(': case '<': case '{': col = 13; return true;
        case ')': case '>': case '}': col = 14; return true;
        case '-': case '~': col = 15; return true;
        case '\'': case '`': col = 16; return true;
        case '!': case '|': col = 17; return true;
        case '_': col = 18; return true;
        case '+': col = 19; return true;
        case '\\': col = 20; return true;
        case '/': col = 21; return true;
        case '[': col = 22; return true;
        case ']': col = 23; return true;
        case '^': col = 24; return true;
        case '&': col = 25; return true;
        case '%': col = 26; return true;
        case ',': col = 27; return true;
        case '=': col = 28; return true;
        case '$': col = 29; return true;
        case '#': col = 30; return true;
        case 0xC5: col = 0; row = 2; return true;
        case 0xD6: col = 1; row = 2; return true;
        case 0xC4: col = 2; row = 2; return true;
        case '?': col = 3; row = 2; return true;
        case '*': col = 4; row = 2; return true;
    }
    col = 30;
    row = 0;
    return false;
}

uint32_t Skin::textBackground() const { return bmp_[SB_TEXT].get(152, 2); }

void Skin::drawText(Canvas& c, const std::string& text, int x, int y, bool keyed) const {
    const Image& t = bmp_[SB_TEXT];
    uint32_t key = textBackground();
    for (unsigned char ch : text) {
        int col, row;
        CharCell(ch, col, row);
        if (keyed)
            c.blitKeyed(t, col * 5, row * 6, 5, 6, x, y, key);
        else
            c.blit(t, col * 5, row * 6, 5, 6, x, y);
        x += 5;
    }
}

// ---------------------------------------------------------------------------
// ANIM.TXT - RetroAmp animation extension (INI style, one section per element)
void Skin::parseAnim(const std::wstring& t, const FileMap& files, int hd) {
    std::map<std::string, int> imageIndex;
    auto image = [&](const std::wstring& name) -> int {
        std::string key = BaseNameA(LowerA(WideToUtf8(Trim(name))));
        auto it = imageIndex.find(key);
        if (it != imageIndex.end()) return it->second;
        auto f = files.find(key);
        if (f == files.end()) return -1;
        Image im;
        if (!LoadImageFromMemory(f->second.data(), f->second.size(), im)) return -1;
        im.scale = (hd > 1 && im.w % hd == 0 && im.h % hd == 0) ? hd : 1;
        animImg_.push_back(std::move(im));
        imageIndex[key] = (int)animImg_.size() - 1;
        return (int)animImg_.size() - 1;
    };
    auto lower = [](std::wstring s) { return ToLower(Trim(s)); };

    std::wstring section;
    AnimElem cur;
    SkinButton btn;
    bool inElem = false, inButton = false;
    auto flush = [&]() {
        if (inButton && btn.w > 0 && btn.h > 0 && !btn.action.empty()) buttons.push_back(btn);
        inButton = false;
        btn = SkinButton();
        if (inElem && (cur.image >= 0 || cur.mode == AM_SCOPE || cur.mode == AM_SPECTRUM || cur.mode == AM_TEXT)) {
            if (cur.fw <= 0 && cur.image >= 0) cur.fw = animImg_[cur.image].lw();
            if (cur.fh <= 0 && cur.image >= 0) cur.fh = animImg_[cur.image].lh();
            if (cur.w <= 0) cur.w = cur.fw;
            if (cur.h <= 0) cur.h = cur.fh;
            if (cur.cols <= 0) cur.cols = std::max(1, cur.frames);
            anims.push_back(cur);
        }
        inElem = false;
        cur = AnimElem();
    };
    size_t a = 0;
    while (a <= t.size()) {
        size_t b = t.find_first_of(L"\r\n", a);
        if (b == std::wstring::npos) b = t.size();
        std::wstring line = Trim(t.substr(a, b - a));
        a = b + 1;
        if (line.empty() || line[0] == L';' || line[0] == L'#') continue;
        if (line[0] == L'[') {
            flush();
            section = lower(line.substr(1, line.find(L']') - 1));
            inButton = section.rfind(L"button", 0) == 0;
            inElem = section != L"panel" && section != L"dsp" && section != L"playlist" && !inButton;
            continue;
        }
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring k = lower(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
        int iv = _wtoi(v.c_str());
        float fv = (float)_wtof(v.c_str());
        if (inButton) {
            if (k == L"x") btn.x = iv;
            else if (k == L"y") btn.y = iv;
            else if (k == L"w") btn.w = iv;
            else if (k == L"h") btn.h = iv;
            else if (k == L"action") btn.action = lower(v);
            continue;
        }
        if (section == L"dsp") {
            if (k == L"body") dspStyle.body = image(v);
            else if (k == L"button") dspStyle.button = image(v);
            else if (k == L"text") ParseColor(v, dspStyle.text);
            else if (k == L"texton") ParseColor(v, dspStyle.textOn);
            else if (k == L"label") ParseColor(v, dspStyle.label);
            else if (k == L"font") dspStyle.font = v;
            continue;
        }
        if (section == L"playlist") {
            if (k == L"turnsprite") hasTurnSprite = iv != 0;
            continue;
        }
        if (section == L"panel") {
            if (k == L"width") panelW = Clamp(iv, 50, 2000);
            else if (k == L"height") panelH = Clamp(iv, 14, 2000);
            else if (k == L"image") {
                panelImage = image(v);
                hasPanel = panelImage >= 0;
            }
            continue;
        }
        if (k == L"window") {
            std::wstring w = lower(v);
            cur.window = w == L"eq" || w == L"equalizer" ? 1 : w == L"playlist" || w == L"pl" ? 2 : w == L"panel" ? 5 : 0;
        } else if (k == L"mode") {
            std::wstring m = lower(v);
            cur.mode = m == L"spin" ? AM_SPIN : m == L"level" ? AM_LEVEL : m == L"state" ? AM_STATE
                     : m == L"progress" ? AM_PROGRESS : m == L"scope" ? AM_SCOPE : m == L"spectrum" ? AM_SPECTRUM
                     : m == L"text" ? AM_TEXT : AM_LOOP;
        } else if (k == L"when") {
            std::wstring m = lower(v);
            cur.when = m == L"playing" ? AW_PLAYING : m == L"paused" ? AW_PAUSED : m == L"stopped" ? AW_STOPPED
                     : m == L"active" ? AW_ACTIVE : AW_ALWAYS;
        } else if (k == L"image") cur.image = image(v);
        else if (k == L"srcx") cur.sx = iv;
        else if (k == L"srcy") cur.sy = iv;
        else if (k == L"framew") cur.fw = iv;
        else if (k == L"frameh") cur.fh = iv;
        else if (k == L"frames") cur.frames = std::max(1, iv);
        else if (k == L"columns") cur.cols = iv;
        else if (k == L"x") cur.x = iv;
        else if (k == L"y") cur.y = iv;
        else if (k == L"w") cur.w = iv;
        else if (k == L"h") cur.h = iv;
        else if (k == L"fps") cur.fps = fv;
        else if (k == L"attack") cur.attack = Clamp(fv, 0.01f, 1.0f);
        else if (k == L"release") cur.release = Clamp(fv, 0.005f, 1.0f);
        else if (k == L"gain") cur.gain = fv;
        else if (k == L"channel") {
            std::wstring c = lower(v);
            cur.channel = c == L"left" || c == L"l" ? 1 : c == L"right" || c == L"r" ? 2 : c == L"bass" ? 3
                        : c == L"mid" ? 4 : c == L"treble" ? 5 : 0;
        } else if (k == L"color") ParseColor(v, cur.color);
        else if (k == L"color2") ParseColor(v, cur.color2);
        else if (k == L"font") cur.font = v;
        else if (k == L"size") cur.size = Clamp(iv, 3, 200);
        else if (k == L"bold") cur.bold = iv != 0;
        else if (k == L"bars") cur.bars = iv;
        else if (k == L"align") {
            std::wstring al = lower(v);
            cur.align = al == L"center" ? 1 : al == L"right" ? 2 : 0;
        } else if (k == L"text") {
            std::wstring tk = lower(v);
            cur.textKind = tk == L"time" ? AT_TIME : tk == L"remain" ? AT_REMAIN : tk == L"bitrate" ? AT_BITRATE
                         : tk == L"samplerate" ? AT_SAMPLERATE : tk == L"track" ? AT_TRACK : tk == L"clock" ? AT_CLOCK
                         : tk == L"counter" ? AT_COUNTER : tk == L"title" ? AT_TITLE : AT_STATIC;
            if (cur.textKind == AT_STATIC) cur.text = v;
        }
    }
    flush();
}
