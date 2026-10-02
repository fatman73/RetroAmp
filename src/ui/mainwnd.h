#pragma once
#include "skinwnd.h"

class MainWnd : public SkinWnd {
public:
    MainWnd() : SkinWnd(W_MAIN) {}
    void tick();  // ~33 Hz: marquee + visualisation
    void resetMarquee() { marqueePx_ = 0; }

protected:
    void paint(Canvas& c) override;
    void onMouseDown(int x, int y, WPARAM mk) override;
    void onMouseMove(int x, int y, WPARAM mk) override;
    void onMouseUp(int x, int y) override;
    bool onDblClick(int x, int y) override;
    void onWheel(int delta, int x, int y) override;
    void onCaptureLost() override;
    LRESULT onMessage(UINT msg, WPARAM wp, LPARAM lp, bool& handled) override;
    const Skin::Region* region() const override;

private:
    int hit(int x, int y) const;
    void click(int id);
    void sliderUpdate(int x);
    void paintNormal(Canvas& c);
    void paintShade(Canvas& c);
    void paintVis(Canvas& c, int x, int y, int w, int h);
    void paintMarquee(Canvas& c);
    void updateVis();

    PushState push_;
    int slider_ = 0;
    float sliderVal_ = 0;
    bool marqueeDrag_ = false;
    int marqueeDragX_ = 0, marqueeDragStart_ = 0;
    int marqueePx_ = 0;
    int marqueeTick_ = 0;
    float bars_[76] = {};
    float peaks_[76] = {};
    float peakVel_[76] = {};
    float osc_[76] = {};
};
