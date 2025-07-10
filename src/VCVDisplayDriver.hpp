#pragma once
#include "DisplayDriver.hpp"
#include "rack.hpp"

// VCV Rack display driver using NanoVG
class VCVDisplayDriver : public DisplayDriver {
private:
    NVGcontext* vg;
    float width, height;
    
public:
    VCVDisplayDriver(NVGcontext* context, float w, float h) : vg(context), width(w), height(h) {}
    
    void clear(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0) override {
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, width, height);
        nvgFillColor(vg, nvgRGB(r, g, b));
        nvgFill(vg);
    }
    
    void drawText(const std::string& text, float x, float y, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, float fontSize = 12.0f) override {
        nvgFontSize(vg, fontSize);
        nvgFontFaceId(vg, APP->window->uiFont->handle);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgFillColor(vg, nvgRGB(r, g, b));
        nvgText(vg, x, y, text.c_str(), NULL);
    }
    
    void drawRect(float x, float y, float w, float h, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, bool filled = false) override {
        nvgBeginPath(vg);
        nvgRect(vg, x, y, w, h);
        if (filled) {
            nvgFillColor(vg, nvgRGB(r, g, b));
            nvgFill(vg);
        } else {
            nvgStrokeColor(vg, nvgRGB(r, g, b));
            nvgStroke(vg);
        }
    }
    
    void drawLine(float x1, float y1, float x2, float y2, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, float thickness = 1.0f) override {
        nvgBeginPath(vg);
        nvgMoveTo(vg, x1, y1);
        nvgLineTo(vg, x2, y2);
        nvgStrokeColor(vg, nvgRGB(r, g, b));
        nvgStrokeWidth(vg, thickness);
        nvgStroke(vg);
    }
    
    void drawCircle(float x, float y, float radius, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, bool filled = false) override {
        nvgBeginPath(vg);
        nvgCircle(vg, x, y, radius);
        if (filled) {
            nvgFillColor(vg, nvgRGB(r, g, b));
            nvgFill(vg);
        } else {
            nvgStrokeColor(vg, nvgRGB(r, g, b));
            nvgStroke(vg);
        }
    }
    
    void beginFrame() override {
        // NanoVG frame management is handled by VCV Rack's draw context
    }
    
    void endFrame() override {
        // NanoVG frame management is handled by VCV Rack's draw context
    }
    
    void getSize(float& w, float& h) override {
        w = width;
        h = height;
    }
    
    void setSize(float w, float h) {
        width = w;
        height = h;
    }
};