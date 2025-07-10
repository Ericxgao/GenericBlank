#pragma once
#include <cstdint>
#include <string>

// Abstract display driver interface for cross-platform graphics
class DisplayDriver {
public:
    virtual ~DisplayDriver() = default;
    
    // Basic drawing primitives
    virtual void clear(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0) = 0;
    virtual void drawText(const std::string& text, float x, float y, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, float fontSize = 12.0f) = 0;
    virtual void drawRect(float x, float y, float w, float h, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, bool filled = false) = 0;
    virtual void drawLine(float x1, float y1, float x2, float y2, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, float thickness = 1.0f) = 0;
    virtual void drawCircle(float x, float y, float radius, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, bool filled = false) = 0;
    
    // Display management
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void getSize(float& width, float& height) = 0;
};

// Color helper struct
struct Color {
    uint8_t r, g, b;
    Color(uint8_t red = 255, uint8_t green = 255, uint8_t blue = 255) : r(red), g(green), b(blue) {}
};

// Common colors
namespace Colors {
    static const Color WHITE(255, 255, 255);
    static const Color BLACK(0, 0, 0);
    static const Color RED(255, 0, 0);
    static const Color GREEN(0, 255, 0);
    static const Color BLUE(0, 0, 255);
    static const Color GRAY(128, 128, 128);
    static const Color LIGHT_GRAY(200, 200, 200);
    static const Color DARK_GRAY(64, 64, 64);
}