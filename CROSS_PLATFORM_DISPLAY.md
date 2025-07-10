# Cross-Platform Display Architecture

This document describes the cross-platform display system implemented for sharing graphics code between VCV Rack and Daisy platforms.

## Architecture Overview

The system uses an abstract `DisplayDriver` interface with platform-specific implementations:

- **VCVDisplayDriver**: Uses NanoVG for vector graphics in VCV Rack
- **DaisyDisplayDriver**: Uses pixel buffer rendering for Daisy hardware
- **DisplayWidget**: Unified widget class that works on both platforms

## File Structure

```
src/
├── DisplayDriver.hpp          # Abstract interface
├── VCVDisplayDriver.hpp       # VCV Rack implementation
├── DaisyDisplayDriver.hpp     # Daisy implementation
└── DisplayWidget.hpp          # Unified widget classes
```

## Usage in VCV Rack

The system is already integrated into the Grains module:

```cpp
#include "DisplayWidget.hpp"

// In your ModuleWidget constructor:
UnifiedBPMDisplay* bpmDisplay = new UnifiedBPMDisplay(
    [module]() -> float { return module ? module->getBPM() : 0.0f; },
    [module]() -> std::string { return module ? module->getTimeDivisionString() : "---"; }
);
bpmDisplay->box.pos = Vec(60, 30);
bpmDisplay->box.size = Vec(120, 30);
addChild(bpmDisplay);
```

## Usage on Daisy Platform

For Daisy, you can use the same display widgets with pixel rendering:

```cpp
#include "DisplayWidget.hpp"

// In your Daisy main loop:
uint8_t displayBuffer[128 * 64]; // For 128x64 OLED display

UnifiedBPMDisplay bpmDisplay(
    []() -> float { return currentBPM; },
    []() -> std::string { return currentTimeDiv; }
);

// Render to pixel buffer
bpmDisplay.drawToPixelBuffer(displayBuffer, 128, 64, 1); // 1 byte per pixel (grayscale)

// Send buffer to your display hardware
display.drawBuffer(displayBuffer);
```

## Available Display Components

### UnifiedBPMDisplay
Shows BPM and time division information.

### UnifiedGrainCountDisplay
Shows active grain count with visual progress bar.

### UnifiedTextDisplay
Generic text display with customizable colors and fonts.

### BouncingBarDisplay
Animated bouncing bar with trail effect. Speed controlled by input parameter (0.0 to 1.0).

**Features:**
- Smooth bouncing animation with physics
- Motion blur trail effect
- Speed indicator text
- Boundary markers
- Real-time speed control

## Creating Custom Display Widgets

```cpp
class MyCustomDisplay : public DisplayWidget {
public:
    void renderContent(DisplayDriver* driver) override {
        float width, height;
        driver->getSize(width, height);
        
        // Clear background
        driver->clear(255, 255, 255); // White
        
        // Draw text
        driver->drawText("Hello World", 10, 10, 0, 0, 0, 12.0f);
        
        // Draw shapes
        driver->drawRect(10, 30, 50, 20, 100, 100, 100, true);
        driver->drawCircle(50, 70, 15, 255, 0, 0, false);
        driver->drawLine(0, 0, width, height, 0, 255, 0, 2.0f);
    }
};
```

## DisplayDriver Interface

### Drawing Methods
- `clear(r, g, b)` - Clear display with color
- `drawText(text, x, y, r, g, b, fontSize)` - Draw text
- `drawRect(x, y, w, h, r, g, b, filled)` - Draw rectangle
- `drawLine(x1, y1, x2, y2, r, g, b, thickness)` - Draw line  
- `drawCircle(x, y, radius, r, g, b, filled)` - Draw circle

### Display Management
- `beginFrame()` / `endFrame()` - Frame boundaries
- `getSize(width, height)` - Get display dimensions

## Color Constants

```cpp
Colors::WHITE
Colors::BLACK  
Colors::RED
Colors::GREEN
Colors::BLUE
Colors::GRAY
Colors::LIGHT_GRAY
Colors::DARK_GRAY
```

## Benefits

1. **Write Once, Run Everywhere**: Display code works on both VCV Rack and Daisy
2. **Consistent Visuals**: Same appearance across platforms
3. **Easy Porting**: Minimal changes needed when moving between platforms
4. **Extensible**: Easy to add new display components
5. **Performance**: Optimized for each platform's capabilities

## Platform Differences

### VCV Rack
- Vector graphics using NanoVG
- Anti-aliasing and smooth curves
- Flexible text rendering
- Real-time updates

### Daisy
- Pixel-based rendering
- Fixed resolution displays
- Optimized for embedded systems
- Simple bitmap fonts

The abstraction layer handles these differences automatically, providing the best experience on each platform.

## Example: Adding Graphics to Your Module

1. Include the display headers in your module
2. Create display widgets using lambda functions for data access
3. Add to VCV Rack module widget as normal
4. For Daisy, call `drawToPixelBuffer()` in your display update loop
5. Send pixel buffer to your display hardware

This architecture makes it easy to create rich visual interfaces that work seamlessly across both desktop and embedded platforms.