#pragma once
#include "../gfx.h"
#include "../skin.h"
#include "../vis/visengine.h"

// Runtime state + drawing of the skin animation extension (ANIM.TXT).
class AnimRuntime {
public:
    ~AnimRuntime();
    void reset();            // call after a skin change
    void update();           // ~33 Hz from the main timer
    void draw(Canvas& c, int window);

private:
    HFONT font(const AnimElem& e, int scale);
    std::vector<float> phase_, level_, scroll_;
    std::vector<float> samples_ = std::vector<float>(2048);
    VisAnalyzer an_;
    VisAudio audio_;
    float levelL_ = 0, levelR_ = 0;
    ULONGLONG last_ = 0;
    std::map<std::wstring, HFONT> fonts_;
};
