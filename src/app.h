#pragma once
#include "audio/player.h"
#include "common.h"
#include "playlist.h"
#include "settings.h"
#include "skin.h"
#include "ui/skinwnd.h"

class MainWnd;
class EqWnd;
class PlWnd;
class DspWnd;
class VisWnd;

enum VisMode { VIS_SPECTRUM, VIS_OSC, VIS_OFF, VIS_COUNT };

class App {
public:
    App();
    ~App();
    bool init(HINSTANCE inst, const std::vector<std::wstring>& args);
    int run();
    void quit();

    HINSTANCE inst = nullptr;
    Skin skin;
    Player player;
    Playlist pl;
    MetaLoader meta;
    DspParams dsp;
    Ini cfg;

    // UI state
    int zoom = 100;  // percent: 100, 150, 200, 250, 300, 400
    int phys(int logical) const { return logical * zoom / 100; }
    int renderScale() const { return zoom % 100 == 0 ? zoom / 100 : zoom / 50; }
    int downsample() const { return zoom % 100 == 0 ? 1 : 2; }
    bool shuffle = false, repeat = false, timeRemaining = false, alwaysOnTop = false, eqAuto = false;
    int visMode = VIS_SPECTRUM;
    bool visible[W_COUNT] = {true, true, true, false, false};
    bool mainShade = false;
    int plTilesW = 0, plTilesH = 1;
    std::wstring skinPath;  // empty = built-in skin
    SkinWnd* wnd[W_COUNT] = {};
    MainWnd* mainWnd = nullptr;
    EqWnd* eqWnd = nullptr;
    PlWnd* plWnd = nullptr;
    DspWnd* dspWnd = nullptr;
    VisWnd* visWnd = nullptr;

    // ---- playback
    void playIndex(int idx, double startPos = 0, bool paused = false);
    void playPressed();
    void pausePressed();
    void stopPressed();
    void nextTrack();
    void prevTrack();
    void seekTo(double sec);
    void seekBy(double delta);
    void setVolume(float v);
    void setBalance(float b);
    void toggleShuffle();
    void toggleRepeat();
    PlayState state() const { return player.state(); }
    double position() const { return player.position(); }
    double duration() const;
    const Track* currentTrack() const;

    // ---- files
    void openFilesDialog(bool add);
    void openFolderDialog(bool add);
    void addUrlDialog();
    void addPaths(const std::vector<std::wstring>& paths, int insertAt, bool clearFirst, bool playFirst);
    // fromPlaylist: dropped on the playlist (insert, don't auto-play); otherwise append + play.
    void onDropFiles(const std::vector<std::wstring>& files, int insertAt, bool fromPlaylist);
    void loadPlaylistDialog();
    void savePlaylistDialog();
    void showFileInfo(int idx);
    void jumpToFileDialog();
    void requestMeta(int idx);

    // ---- windows
    void toggleWindow(int w);
    void setWindowVisible(int w, bool v);
    void setZoom(int percent);
    void setAlwaysOnTop(bool on);
    void setMainShade(bool on);
    void redraw(int w);
    void redrawAll();
    void setPlaylistSize(int tilesW, int tilesH);

    // ---- skins
    bool loadSkin(const std::wstring& path, bool showErrors);
    void loadSkinDialog();
    std::wstring userSkinDir() const;
    std::vector<std::wstring> listSkins() const;

    // ---- equalizer / DSP
    void applyDsp(bool saveAuto = true);
    void setEqBand(int band, float db);
    void setPreamp(float db);
    void applyEqPreset(const EqPreset& p);
    void showEqPresetsMenu(HWND owner, POINT pt);
    void showDspPresetsMenu(HWND owner, POINT pt);
    void applyBassPreset(int idx);
    std::vector<EqPreset> userPresets() const;

    // ---- menus / keys
    void showMainMenu(HWND owner, POINT pt);
    void showPlaylistMenu(int which, HWND owner, POINT pt);  // sort / misc / list menus
    bool handleKey(UINT vk, bool alt, bool ctrl, bool shift);
    void handleCommand(int cmd);

    // ---- events
    bool handleMainMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result);
    void onPlayerEvent(WPARAM ev, LPARAM session);
    void onMeta(MetaResult* r);
    void onTimer();
    void onTaskbarCreated();
    void onThumbButton(int id);
    void onCopyData(const std::wstring& data);

    // marquee text helpers
    void setMarqueeOverride(const std::wstring& text);
    std::wstring marqueeOverride() const;
    std::wstring currentTitleLine() const;

    // ---- docking / window dragging
    void beginDrag(SkinWnd* w, POINT cursor);
    void dragTo(POINT cursor);
    void endDrag();

    void saveSettings();
    void savePlaylist();

private:
    void loadSettings();
    void createWindows();
    void updateTaskbar();
    void updateTitle();
    void loadAutoEq();
    void saveAutoEq();
    bool snapRect(const RECT& r, const std::vector<int>& exclude, int& dx, int& dy) const;

    std::wstring dataDir_;
    std::wstring marquee_;
    ULONGLONG marqueeUntil_ = 0;
    uint64_t session_ = 0;

    // dragging
    SkinWnd* dragWnd_ = nullptr;
    POINT dragStart_ = {};
    std::vector<int> dragGroup_;
    RECT dragRects_[W_COUNT] = {};

    // taskbar integration
    UINT taskbarMsg_ = 0;
    struct ITaskbarList3* taskbar_ = nullptr;
    bool thumbAdded_ = false;
    HICON thumbIcons_[5] = {};
    PlayState lastTaskbarState_ = PlayState::Stopped;
    int lastProgress_ = -1;
    bool quitting_ = false;
    int errorStreak_ = 0;
    int timerTicks_ = 0;
    std::wstring pendingError_;
};

extern App* g_app;

// Command ids (menus, thumb bar)
enum Cmd {
    CMD_NONE = 0,
    CMD_ABOUT = 1000,
    CMD_PLAY_FILE,
    CMD_PLAY_FOLDER,
    CMD_ADD_FILE,
    CMD_ADD_FOLDER,
    CMD_ADD_URL,
    CMD_EXIT,
    CMD_WND_MAIN,
    CMD_WND_EQ,
    CMD_WND_PL,
    CMD_WND_DSP,
    CMD_WND_VIS,
    CMD_ZOOM100,
    CMD_ZOOM150,
    CMD_ZOOM200,
    CMD_ZOOM250,
    CMD_ZOOM300,
    CMD_ZOOM400,
    CMD_ONTOP,
    CMD_SHADE,
    CMD_TIME_ELAPSED,
    CMD_TIME_REMAINING,
    CMD_VIS_SPECTRUM,
    CMD_VIS_OSC,
    CMD_VIS_OFF,
    CMD_PLAY,
    CMD_PAUSE,
    CMD_STOP,
    CMD_PREV,
    CMD_NEXT,
    CMD_FWD5,
    CMD_BACK5,
    CMD_SHUFFLE,
    CMD_REPEAT,
    CMD_JUMP,
    CMD_FILEINFO,
    CMD_SKIN_LOAD,
    CMD_SKIN_FOLDER,
    CMD_SKIN_WEB,
    CMD_SKIN_DEFAULT,
    CMD_EQ_MODE_WINAMP,
    CMD_EQ_MODE_ISO,
    CMD_PL_SORT_TITLE,
    CMD_PL_SORT_FILE,
    CMD_PL_SORT_PATH,
    CMD_PL_REVERSE,
    CMD_PL_RANDOM,
    CMD_PL_REMOVE_MISSING,
    CMD_PL_NEW,
    CMD_PL_SAVE,
    CMD_PL_LOAD,
    CMD_PL_SELECT_ALL,
    CMD_PL_SELECT_NONE,
    CMD_PL_INVERT,
    CMD_PL_REMOVE_SEL,
    CMD_PL_CROP,
    CMD_PL_CLEAR,
    CMD_PL_PLAY_SEL,
    CMD_EQ_RESET,
    CMD_EQ_SAVE,
    CMD_EQ_IMPORT,
    CMD_EQ_EXPORT,
    CMD_SKIN_BASE = 2000,        // + index into listSkins()
    CMD_EQ_PRESET_BASE = 3000,   // + index into builtin presets
    CMD_EQ_USER_BASE = 3500,     // + index into user presets
    CMD_EQ_DELETE_BASE = 3800,   // + index into user presets
    CMD_BASS_PRESET_BASE = 4000, // + index into bass presets
    CMD_DEVICE_DEFAULT = 4900,
    CMD_DEVICE_BASE = 4901,      // + index into ListOutputDevices()
    CMD_THUMB_PREV = 5000,
    CMD_THUMB_PLAY,
    CMD_THUMB_NEXT,
    CMD_THUMB_STOP,
};
