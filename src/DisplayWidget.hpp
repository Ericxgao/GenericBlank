#pragma once
#include "DisplayDriver.hpp"
#include "VCVDisplayDriver.hpp"
#include "DaisyDisplayDriver.hpp"
#include "rack.hpp"
#include <memory>
#include <functional>

// Base class for cross-platform display widgets
class DisplayWidget : public rack::Widget {
protected:
    std::unique_ptr<DisplayDriver> driver;
    
public:
    DisplayWidget() = default;
    virtual ~DisplayWidget() = default;
    
    // Called by subclasses to render their content
    virtual void renderContent(DisplayDriver* driver) = 0;
    
    // VCV Rack draw implementation
    void draw(const rack::Widget::DrawArgs& args) override {
        // Create VCV driver for this draw call
        auto vcvDriver = std::make_unique<VCVDisplayDriver>(args.vg, box.size.x, box.size.y);
        
        // Let subclass render its content
        renderContent(vcvDriver.get());
    }
    
    // For Daisy platform - would be called by Daisy main loop
    void drawToPixelBuffer(uint8_t* buffer, int width, int height, int bytesPerPixel = 1) {
        // Create Daisy driver for this draw call
        auto daisyDriver = std::make_unique<DaisyDisplayDriver>(width, height, bytesPerPixel);
        
        // Let subclass render its content
        renderContent(daisyDriver.get());
        
        // Copy pixel data to provided buffer
        const uint8_t* pixelData = daisyDriver->getPixelBuffer();
        memcpy(buffer, pixelData, width * height * bytesPerPixel);
    }
};

// Specific implementation for grain count display
class UnifiedGrainCountDisplay : public DisplayWidget {
private:
    std::function<size_t()> getActiveGrains;
    std::function<size_t()> getMaxGrains;
    
public:
    UnifiedGrainCountDisplay(std::function<size_t()> activeGrainsGetter, 
                           std::function<size_t()> maxGrainsGetter)
        : getActiveGrains(activeGrainsGetter), getMaxGrains(maxGrainsGetter) {}
    
    void renderContent(DisplayDriver* driver) override {
        if (!getActiveGrains || !getMaxGrains) return;
        
        // Get display size
        float width, height;
        driver->getSize(width, height);
        
        // Clear background
        driver->clear(255, 255, 255); // White background
        
        // Get grain counts
        size_t activeGrains = getActiveGrains();
        size_t maxGrains = getMaxGrains();
        
        // Format text
        char grainStr[64];
        snprintf(grainStr, sizeof(grainStr), "%zu/%zu grains", activeGrains, maxGrains);
        
        // Draw text centered
        float fontSize = 12.0f;
        float textX = width * 0.1f; // Left-aligned with some margin
        float textY = height * 0.5f - fontSize * 0.5f; // Vertically centered
        
        driver->drawText(std::string(grainStr), textX, textY, 100, 100, 100, fontSize);
        
        // Draw visual indicator bar
        float barWidth = width * 0.8f;
        float barHeight = 4.0f;
        float barX = width * 0.1f;
        float barY = height * 0.7f;
        
        // Background bar
        driver->drawRect(barX, barY, barWidth, barHeight, 200, 200, 200, true);
        
        // Active grains bar
        if (maxGrains > 0) {
            float fillWidth = barWidth * ((float)activeGrains / (float)maxGrains);
            driver->drawRect(barX, barY, fillWidth, barHeight, 100, 150, 255, true);
        }
    }
};

// BPM display implementation
class UnifiedBPMDisplay : public DisplayWidget {
private:
    std::function<float()> getBPM;
    std::function<std::string()> getTimeDivision;
    
public:
    UnifiedBPMDisplay(std::function<float()> bpmGetter, 
                     std::function<std::string()> timeDivGetter)
        : getBPM(bpmGetter), getTimeDivision(timeDivGetter) {}
    
    void renderContent(DisplayDriver* driver) override {
        if (!getBPM || !getTimeDivision) return;
        
        // Get display size
        float width, height;
        driver->getSize(width, height);
        
        // Clear background
        driver->clear(255, 255, 255); // White background
        
        // Get BPM and time division
        float bpm = getBPM();
        std::string timeDiv = getTimeDivision();
        
        // Format display text
        std::string displayText = "--- BPM";
        if (bpm > 0.0f) {
            char bpmStr[64];
            snprintf(bpmStr, sizeof(bpmStr), "%.0f BPM %s", bpm, timeDiv.c_str());
            displayText = std::string(bpmStr);
        }
        
        // Draw text centered
        float fontSize = 14.0f;
        float textX = width * 0.05f; // Small left margin
        float textY = height * 0.5f - fontSize * 0.5f; // Vertically centered
        
        driver->drawText(displayText, textX, textY, 0, 0, 0, fontSize);
    }
};

// Generic text display
class UnifiedTextDisplay : public DisplayWidget {
private:
    std::function<std::string()> getText;
    Color textColor;
    Color backgroundColor;
    float fontSize;
    
public:
    UnifiedTextDisplay(std::function<std::string()> textGetter, 
                      Color textCol = Colors::BLACK,
                      Color bgCol = Colors::WHITE,
                      float size = 12.0f)
        : getText(textGetter), textColor(textCol), backgroundColor(bgCol), fontSize(size) {}
    
    void renderContent(DisplayDriver* driver) override {
        if (!getText) return;
        
        // Get display size
        float width, height;
        driver->getSize(width, height);
        
        // Clear background
        driver->clear(backgroundColor.r, backgroundColor.g, backgroundColor.b);
        
        // Get text
        std::string text = getText();
        
        // Draw text centered
        float textX = width * 0.1f;
        float textY = height * 0.5f - fontSize * 0.5f;
        
        driver->drawText(text, textX, textY, textColor.r, textColor.g, textColor.b, fontSize);
    }
};

// Bouncing bar animation display
class BouncingBarDisplay : public DisplayWidget {
private:
    std::function<float()> getAnimationSpeed; // Speed multiplier (0.0 to 1.0)
    float barPosition;
    float barVelocity;
    float lastTime;
    Color barColor;
    Color backgroundColor;
    bool initialized;
    
public:
    BouncingBarDisplay(std::function<float()> speedGetter, 
                      Color barCol = Colors::BLUE,
                      Color bgCol = Colors::WHITE)
        : getAnimationSpeed(speedGetter), barPosition(0.0f), barVelocity(1.0f), 
          lastTime(0.0f), barColor(barCol), backgroundColor(bgCol), initialized(false) {}
    
    void renderContent(DisplayDriver* driver) override {
        // Get display size
        float width, height;
        driver->getSize(width, height);
        
        // Initialize on first render
        if (!initialized) {
            barPosition = width * 0.1f;
            barVelocity = 50.0f; // pixels per second
            initialized = true;
        }
        
        // Get current time (approximation based on frame count)
        static int frameCount = 0;
        frameCount++;
        float currentTime = frameCount * 0.016667f; // Assume 60 FPS
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        
        // Get animation speed
        float speed = getAnimationSpeed ? getAnimationSpeed() : 0.5f;
        speed = std::max(0.0f, std::min(1.0f, speed)); // Clamp to 0-1
        
        // Update bar position
        float effectiveVelocity = barVelocity * (0.2f + speed * 2.0f); // Scale velocity based on speed
        barPosition += effectiveVelocity * deltaTime;
        
        // Bounce off edges
        float barWidth = 20.0f;
        float leftBound = width * 0.05f;
        float rightBound = width * 0.95f - barWidth;
        
        if (barPosition <= leftBound) {
            barPosition = leftBound;
            barVelocity = std::abs(barVelocity); // Bounce right
        } else if (barPosition >= rightBound) {
            barPosition = rightBound;
            barVelocity = -std::abs(barVelocity); // Bounce left
        }
        
        // Clear background
        driver->clear(backgroundColor.r, backgroundColor.g, backgroundColor.b);
        
        // Draw bouncing bar
        float barHeight = height * 0.3f;
        float barY = height * 0.35f;
        
        // Draw trail effect (multiple bars with decreasing opacity)
        for (int i = 0; i < 3; i++) {
            float trailOffset = barVelocity > 0 ? -i * 8.0f : i * 8.0f;
            float trailPos = barPosition + trailOffset;
            if (trailPos >= leftBound && trailPos <= rightBound) {
                uint8_t alpha = 255 - (i * 80); // Fade trail
                uint8_t r = (barColor.r * alpha) / 255;
                uint8_t g = (barColor.g * alpha) / 255;
                uint8_t b = (barColor.b * alpha) / 255;
                driver->drawRect(trailPos, barY, barWidth, barHeight, r, g, b, true);
            }
        }
        
        // Draw main bar
        driver->drawRect(barPosition, barY, barWidth, barHeight, barColor.r, barColor.g, barColor.b, true);
        
        // Draw speed indicator text
        char speedText[32];
        snprintf(speedText, sizeof(speedText), "Speed: %.1f", speed);
        driver->drawText(std::string(speedText), width * 0.05f, height * 0.8f, 0, 0, 0, 10.0f);
        
        // Draw boundary lines
        driver->drawLine(leftBound, 0, leftBound, height, 100, 100, 100, 1.0f);
        driver->drawLine(rightBound + barWidth, 0, rightBound + barWidth, height, 100, 100, 100, 1.0f);
    }
};