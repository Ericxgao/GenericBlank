#pragma once
#include "DisplayDriver.hpp"
#include <vector>
#include <cstring>
#include <algorithm>

// Simple font data structure for pixel drawing
struct PixelFont {
    static const int CHAR_WIDTH = 6;
    static const int CHAR_HEIGHT = 8;
    
    // Simple 6x8 font data for basic characters (simplified for demo)
    static const uint8_t font_data[95][8]; // ASCII 32-126
};

// Daisy display driver using pixel buffer
class DaisyDisplayDriver : public DisplayDriver {
private:
    std::vector<uint8_t> pixelBuffer;
    int width, height;
    int bytesPerPixel;
    
    void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        
        int index = (y * width + x) * bytesPerPixel;
        if (bytesPerPixel == 1) {
            // Grayscale: convert RGB to luminance
            pixelBuffer[index] = (r * 0.299f + g * 0.587f + b * 0.114f);
        } else if (bytesPerPixel == 3) {
            // RGB
            pixelBuffer[index] = r;
            pixelBuffer[index + 1] = g;
            pixelBuffer[index + 2] = b;
        }
    }
    
    void drawHorizontalLine(int x1, int x2, int y, uint8_t r, uint8_t g, uint8_t b) {
        if (x1 > x2) std::swap(x1, x2);
        for (int x = x1; x <= x2; x++) {
            setPixel(x, y, r, g, b);
        }
    }
    
    void drawVerticalLine(int x, int y1, int y2, uint8_t r, uint8_t g, uint8_t b) {
        if (y1 > y2) std::swap(y1, y2);
        for (int y = y1; y <= y2; y++) {
            setPixel(x, y, r, g, b);
        }
    }
    
public:
    DaisyDisplayDriver(int w, int h, int bpp = 1) : width(w), height(h), bytesPerPixel(bpp) {
        pixelBuffer.resize(width * height * bytesPerPixel, 0);
    }
    
    void clear(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0) override {
        if (bytesPerPixel == 1) {
            uint8_t gray = (r * 0.299f + g * 0.587f + b * 0.114f);
            std::fill(pixelBuffer.begin(), pixelBuffer.end(), gray);
        } else {
            for (int i = 0; i < pixelBuffer.size(); i += bytesPerPixel) {
                pixelBuffer[i] = r;
                if (bytesPerPixel > 1) pixelBuffer[i + 1] = g;
                if (bytesPerPixel > 2) pixelBuffer[i + 2] = b;
            }
        }
    }
    
    void drawText(const std::string& text, float x, float y, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, float fontSize = 12.0f) override {
        // Simple bitmap font rendering
        int startX = (int)x;
        int startY = (int)y;
        
        for (size_t i = 0; i < text.length(); i++) {
            char c = text[i];
            if (c < 32 || c > 126) continue; // Only printable ASCII
            
            int charIndex = c - 32;
            int charX = startX + i * PixelFont::CHAR_WIDTH;
            
            // Draw character using simple pattern (for demo - would use real font data)
            for (int py = 0; py < PixelFont::CHAR_HEIGHT; py++) {
                for (int px = 0; px < PixelFont::CHAR_WIDTH; px++) {
                    // Simple pattern for demo - replace with actual font bitmap
                    bool pixel = (px + py + charIndex) % 3 == 0;
                    if (pixel) {
                        setPixel(charX + px, startY + py, r, g, b);
                    }
                }
            }
        }
    }
    
    void drawRect(float x, float y, float w, float h, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, bool filled = false) override {
        int x1 = (int)x;
        int y1 = (int)y;
        int x2 = (int)(x + w);
        int y2 = (int)(y + h);
        
        if (filled) {
            for (int py = y1; py < y2; py++) {
                drawHorizontalLine(x1, x2 - 1, py, r, g, b);
            }
        } else {
            drawHorizontalLine(x1, x2 - 1, y1, r, g, b);
            drawHorizontalLine(x1, x2 - 1, y2 - 1, r, g, b);
            drawVerticalLine(x1, y1, y2 - 1, r, g, b);
            drawVerticalLine(x2 - 1, y1, y2 - 1, r, g, b);
        }
    }
    
    void drawLine(float x1, float y1, float x2, float y2, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, float thickness = 1.0f) override {
        // Bresenham's line algorithm
        int ix1 = (int)x1, iy1 = (int)y1;
        int ix2 = (int)x2, iy2 = (int)y2;
        
        int dx = abs(ix2 - ix1);
        int dy = abs(iy2 - iy1);
        int sx = ix1 < ix2 ? 1 : -1;
        int sy = iy1 < iy2 ? 1 : -1;
        int err = dx - dy;
        
        while (true) {
            setPixel(ix1, iy1, r, g, b);
            
            if (ix1 == ix2 && iy1 == iy2) break;
            
            int e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                ix1 += sx;
            }
            if (e2 < dx) {
                err += dx;
                iy1 += sy;
            }
        }
    }
    
    void drawCircle(float x, float y, float radius, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, bool filled = false) override {
        // Midpoint circle algorithm
        int cx = (int)x;
        int cy = (int)y;
        int rad = (int)radius;
        
        int dx = rad;
        int dy = 0;
        int err = 0;
        
        while (dx >= dy) {
            if (filled) {
                drawHorizontalLine(cx - dx, cx + dx, cy + dy, r, g, b);
                drawHorizontalLine(cx - dx, cx + dx, cy - dy, r, g, b);
                drawHorizontalLine(cx - dy, cx + dy, cy + dx, r, g, b);
                drawHorizontalLine(cx - dy, cx + dy, cy - dx, r, g, b);
            } else {
                setPixel(cx + dx, cy + dy, r, g, b);
                setPixel(cx + dy, cy + dx, r, g, b);
                setPixel(cx - dy, cy + dx, r, g, b);
                setPixel(cx - dx, cy + dy, r, g, b);
                setPixel(cx - dx, cy - dy, r, g, b);
                setPixel(cx - dy, cy - dx, r, g, b);
                setPixel(cx + dy, cy - dx, r, g, b);
                setPixel(cx + dx, cy - dy, r, g, b);
            }
            
            if (err <= 0) {
                dy += 1;
                err += 2 * dy + 1;
            }
            if (err > 0) {
                dx -= 1;
                err -= 2 * dx + 1;
            }
        }
    }
    
    void beginFrame() override {
        // Clear buffer or prepare for new frame
    }
    
    void endFrame() override {
        // Flush to display hardware (would be platform-specific)
    }
    
    void getSize(float& w, float& h) override {
        w = (float)width;
        h = (float)height;
    }
    
    // Daisy-specific methods
    const uint8_t* getPixelBuffer() const { return pixelBuffer.data(); }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getBytesPerPixel() const { return bytesPerPixel; }
};

// Simple font definition (would typically be in a separate file)
const uint8_t PixelFont::font_data[95][8] = {
    // Simplified font data - in real implementation, this would contain
    // actual bitmap font data for each character
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Space
    // ... additional characters would be defined here
};