#pragma once
#include "../gfx.h"
#include "../skin.h"

enum WndId { W_MAIN, W_EQ, W_PL, W_DSP, W_VIS, W_COUNT };

// Base class of the borderless, fully skinned windows (main, EQ, playlist, bass/DSP).
class SkinWnd {
public:
    explicit SkinWnd(int id) : id(id) {}
    virtual ~SkinWnd();

    bool create(HWND owner, int x, int y, const wchar_t* title);
    void setLogicalSize(int w, int h);
    void applyScale();  // re-applies the global scale to the window size
    void redraw();
    RECT screenRect() const;
    void moveTo(int x, int y);
    void updateRegion();
    POINT menuPoint(int lx, int ly) const;  // logical client point -> screen

    const int id;
    HWND hwnd = nullptr;
    int lw = 275, lh = 116;
    bool active = false;

protected:
    virtual void paint(Canvas& c) = 0;
    virtual void postPaint(HDC dc) {}  // drawn on top of the canvas (e.g. live video)
    // Physical rect that postPaint fully covers; the canvas is not blitted there (no flicker).
    virtual bool liveRect(RECT& r) const { return false; }
    virtual void onMouseDown(int x, int y, WPARAM mk) {}
    virtual void onMouseMove(int x, int y, WPARAM mk) {}
    virtual void onMouseUp(int x, int y) {}
    virtual void onRightClick(int x, int y);
    virtual bool onDblClick(int x, int y) { return false; }
    virtual void onWheel(int delta, int x, int y) {}
    virtual bool onKey(UINT vk, bool ctrl, bool shift) { return false; }
    virtual void onDropFiles(const std::vector<std::wstring>& files, int x, int y);
    virtual void onCaptureLost() {}
    virtual LRESULT onMessage(UINT msg, WPARAM wp, LPARAM lp, bool& handled);
    virtual const Skin::Region* region() const { return nullptr; }

    int renderScale() const;  // canvas pixels per skin pixel
    const Skin& skin() const;
    void startWindowDrag();
    bool dragging() const { return dragging_; }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    Canvas canvas_;
    bool dirty_ = true;
    bool dragging_ = false;
};

// Helper: classic push button state tracking.
struct PushState {
    int pressed = 0;   // control id currently held down
    bool inside = false;
    bool is(int id) const { return pressed == id && inside; }
};
