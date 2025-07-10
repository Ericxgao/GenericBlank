# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a VCV Rack plugin that started as a generic blank panel template but has evolved into a comprehensive DSP module collection. It integrates the DaisySP library and includes three main modules:

1. **Blank** - A dual envelope generator with VCA functionality
2. **DaisyOscillator** - An oscillator module using DaisySP library
3. **Grains** - A granular synthesis module

## Build Commands

```bash
# Build the plugin
make clean && make

# Install to VCV Rack (if RACK_DIR is set correctly)
make install

# Clean build artifacts
make clean
```

The plugin requires VCV Rack SDK to be installed and accessible via `RACK_DIR` environment variable (defaults to `../..`).

## Architecture

### Main Plugin Structure
- `src/plugin.cpp` - Plugin registration and initialization
- `src/plugin.hpp` - Plugin header with model declarations
- `plugin.json` - Plugin metadata and module definitions

### Module Organization
Each module follows VCV Rack's standard pattern:
- Module class inherits from `Module`
- ModuleWidget class inherits from `ModuleWidget`
- Model created with `createModel<ModuleClass, WidgetClass>("slug")`

### DSP Libraries Integration

#### DaisySP Integration
- Located in `dep/DaisySP/`
- Includes comprehensive DSP components: oscillators, filters, effects, envelopes
- All source files automatically included in build via Makefile
- Use `#include "daisysp.h"` to access components
- Initialize with sample rate: `object.Init(sampleRate)`

#### Custom DSP Components
- `dsp/` directory contains custom DSP classes
- Organized by category: generators, filters, effects, modulation, shaping
- `simd/` directory contains SIMD optimization utilities
- `utilities/` contains general utility functions

### Key Implementation Details

#### Grain Module Architecture
- Uses template-based `GrainManager<T, DelaySize>` for grain processing
- Integrates Dattorro reverb algorithm (`src/Dattorro.cpp`)
- Implements real-time granular synthesis with configurable parameters
- Uses DaisySP DelayLine for audio buffering

#### Sample Rate Handling
VCV Rack modules must handle variable sample rates:
```cpp
void process(const ProcessArgs& args) override {
    if (args.sampleRate != lastSampleRate) {
        // Reinitialize DaisySP objects
        daisyspObject.Init(args.sampleRate);
        lastSampleRate = args.sampleRate;
    }
}
```

#### Parameter Configuration
Use VCV Rack's parameter system with proper scaling:
```cpp
configParam(PARAM_ID, minVal, maxVal, defaultVal, "Name", "Unit", base, multiplier);
configInput(INPUT_ID, "Name");
configOutput(OUTPUT_ID, "Name");
```

## Development Guidelines

### Adding New Modules
1. Create new `.cpp` file in `src/`
2. Add model declaration to `src/plugin.hpp`
3. Register model in `src/plugin.cpp`
4. Update `plugin.json` with module metadata
5. Add source file to `SOURCES` in Makefile

### DSP Best Practices
- Use `float` for all audio processing (VCV Rack standard)
- Handle polyphony with `inputs[].getChannels()` and `outputs[].setChannels()`
- Clamp parameter values to valid ranges
- Use `dsp::SchmittTrigger` for trigger detection
- Apply proper gain staging (VCV Rack uses ±10V signals)

### Using DaisySP Components
- Include `daisysp.h` in modules that use DaisySP
- Initialize objects with current sample rate
- Many DaisySP frequency parameters expect Hz values
- Filter frequencies often expect normalized values (0-0.497)
- All DaisySP processing is single-sample based

## File Structure
- `src/` - Main module source code
- `res/` - SVG panel graphics
- `dsp/` - Custom DSP components
- `dep/DaisySP/` - DaisySP library source
- `utilities/` - Utility functions
- `simd/` - SIMD optimizations
- `build/` - Build artifacts (generated)
- `dist/` - Distribution files (generated)

## Testing
No automated test framework is configured. Test modules by:
1. Building the plugin
2. Running VCV Rack
3. Loading modules and testing functionality
4. Checking audio output and parameter responses