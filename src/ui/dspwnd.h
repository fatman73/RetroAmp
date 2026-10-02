#pragma once
#include "skinwnd.h"

// "Bass & DSP" window: bass boost, sub-bass, psycho-acoustic harmonics, stereo width,
// loudness and limiter. Drawn from the current skin's playlist frame + EQ sliders.
class DspWnd : public SkinWnd {
public:
    DspWnd() : SkinWnd(W_DSP) {}

protected:
    void paint(Canvas& c) override;
    void onMouseDown(int x, int y, WPARAM mk) override;
    void onMouseMove(int x, int y, WPARAM mk) override;
    void onMouseUp(int x, int y) override;
    bool onDblClick(int x, int y) override;
    void onWheel(int delta, int x, int y) override;
    void onCaptureLost() override;

private:
    int sliderAt(int x, int y) const;
    int buttonAt(int x, int y) const;
    float getP(int s) const;
    void setP(int s, float p);
    std::string valueText(int s) const;
    void drawBox(Canvas& c, int x, int y, int w, int h, const std::string& text, bool on, bool pressed);
    int slider_ = -1;
    int hoverSlider_ = -1;
    PushState push_;
};
