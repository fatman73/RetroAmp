#pragma once
#include "skinwnd.h"

class EqWnd : public SkinWnd {
public:
    EqWnd() : SkinWnd(W_EQ) {}

protected:
    void paint(Canvas& c) override;
    void onMouseDown(int x, int y, WPARAM mk) override;
    void onMouseMove(int x, int y, WPARAM mk) override;
    void onMouseUp(int x, int y) override;
    void onRightClick(int x, int y) override;
    bool onDblClick(int x, int y) override;
    void onWheel(int delta, int x, int y) override;
    void onCaptureLost() override;
    const Skin::Region* region() const override { return &skin().regEq; }

private:
    int hit(int x, int y) const;
    int sliderAt(int x, int y) const;  // -1 none, 0 preamp, 1..10 bands
    void setSlider(int s, float db);
    float sliderValue(int s) const;
    void drawSlider(Canvas& c, int x, float db, bool pressed);
    PushState push_;
    int slider_ = -1;
};
