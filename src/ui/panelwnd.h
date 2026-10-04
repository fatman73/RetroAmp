#pragma once
#include "skinwnd.h"

// Optional decorative panel supplied by a skin (ANIM.TXT [Panel]), e.g. a cassette deck in a
// hi-fi tower. Shows the panel image plus its animated elements; drag it anywhere.
class PanelWnd : public SkinWnd {
public:
    PanelWnd() : SkinWnd(W_PANEL) {}

protected:
    void paint(Canvas& c) override;
    void onMouseDown(int x, int y, WPARAM mk) override;
    void onMouseMove(int x, int y, WPARAM mk) override;
    void onMouseUp(int x, int y) override;
    void onCaptureLost() override;

private:
    int buttonAt(int x, int y) const;
    int pressed_ = -1;
    bool inside_ = false;
};
