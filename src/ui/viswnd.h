#pragma once
#include "../vis/visengine.h"
#include "skinwnd.h"

// Big visualisation window ("old school" MilkDrop / AVS style presets) with fullscreen mode.
class VisWnd : public SkinWnd {
public:
    VisWnd() : SkinWnd(W_VIS) {}
    ~VisWnd() override;
    void loadState();
    void saveState();
    void nextPreset(int delta);
    void setPreset(int p);
    void toggleFullscreen();
    void exitFullscreen();
    bool isFullscreen() const { return fs_ != nullptr; }
    void setAuto(bool on);
    bool autoCycle() const { return auto_; }
    void showMenu(POINT pt);
    int tilesW = 0, tilesH = 2;

protected:
    void paint(Canvas& c) override;
    void postPaint(HDC dc) override;
    bool liveRect(RECT& r) const override;
    void onMouseDown(int x, int y, WPARAM mk) override;
    void onMouseMove(int x, int y, WPARAM mk) override;
    void onMouseUp(int x, int y) override;
    void onRightClick(int x, int y) override;
    bool onDblClick(int x, int y) override;
    void onWheel(int delta, int x, int y) override;
    bool onKey(UINT vk, bool ctrl, bool shift) override;
    void onCaptureLost() override;
    LRESULT onMessage(UINT msg, WPARAM wp, LPARAM lp, bool& handled) override;

private:
    Rc content() const { return {12, 20, lw - 24, lh - 36}; }
    int buttonAt(int x, int y) const;
    void tick();
    void drawFrame(HDC dc, int x, int y, int w, int h, bool fullscreen);
    static LRESULT CALLBACK FsProc(HWND, UINT, WPARAM, LPARAM);

    VisEngine eng_;
    VisAnalyzer an_;
    VisAudio audio_;
    std::vector<float> samples_ = std::vector<float>(2048);
    bool auto_ = false;
    ULONGLONG lastSwitch_ = 0, nameUntil_ = 0, fsMouseMove_ = 0;
    HWND fs_ = nullptr;
    PushState push_;
    bool resizing_ = false;
    POINT resizeStart_ = {};
    int resizeW_ = 0, resizeH_ = 0;
};
