#pragma once
#include "skinwnd.h"

class PlWnd : public SkinWnd {
public:
    PlWnd() : SkinWnd(W_PL) {}
    ~PlWnd() override;
    void ensureVisible(int idx);
    void clampScroll();
    // The playlist panel has two sides: the list and (for skins with a deck) the cassette deck.
    bool deckSide = false;
    bool hasDeck() const;

protected:
    void paint(Canvas& c) override;
    void onMouseDown(int x, int y, WPARAM mk) override;
    void onMouseMove(int x, int y, WPARAM mk) override;
    void onMouseUp(int x, int y) override;
    void onRightClick(int x, int y) override;
    bool onDblClick(int x, int y) override;
    void onWheel(int delta, int x, int y) override;
    bool onKey(UINT vk, bool ctrl, bool shift) override;
    void onDropFiles(const std::vector<std::wstring>& files, int x, int y) override;
    void onCaptureLost() override;

private:
    enum Mode { M_NONE, M_MOVE, M_SCROLL, M_RESIZE, M_MENU, M_BUTTON };
    struct Menu {
        int x;       // bar x
        int count;   // items
        int spriteX; // in PLEDIT.BMP
        int barX, barH;
    };
    int visibleRows() const;
    int rowAt(int y) const;  // track index or -1
    Rc listRect() const { return {12, 20, lw - 32, lh - 58}; }
    Menu menuDef(int m) const;
    int menuItemAt(int x, int y) const;
    void openMenu(int m);
    void execMenu(int m, int item);
    int buttonAt(int x, int y) const;
    void ensureFont();
    void selectOnly(int idx);
    void moveFocus(int idx, bool shift, bool ctrl);
    int scrollHandleY() const;
    Rc turnRect() const { return {lw - 53, 2, 30, 11}; }  // left of the shade/close gadgets
    void paintDeck(Canvas& c, const Rc& area);
    int deckButtonAt(int x, int y) const;  // skin button index under a playlist-window point
    Canvas deckCanvas_;
    float deckX_ = 0, deckY_ = 0, deckF_ = 1;
    int deckPressed_ = -1;
    bool deckInside_ = false;

    int scroll_ = 0;
    int anchor_ = -1, focus_ = -1;
    Mode mode_ = M_NONE;
    int dragRow_ = -1;
    bool moved_ = false;
    int pressRow_ = -1;
    int scrollGrab_ = 0;
    POINT resizeStart_ = {};
    int resizeW_ = 0, resizeH_ = 0;
    int menu_ = -1, menuHover_ = -1;
    bool menuSticky_ = false, menuMoved_ = false;
    PushState push_;

    HFONT font_ = nullptr;
    int fontScale_ = 0;
    std::wstring fontName_;
};
