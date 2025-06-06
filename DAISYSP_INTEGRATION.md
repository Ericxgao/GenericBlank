# DaisySP Integration with VCV Rack Plugin

## Overview

This VCV Rack plugin now includes the **DaisySP** library, providing access to a comprehensive collection of high-quality DSP components for audio processing.

## What is DaisySP?

[DaisySP](https://github.com/electro-smith/DaisySP) is an open-source DSP library that provides:

- **Control Signal Generators**: AD and ADSR Envelopes, Phasor
- **Drum Synthesis**: Analog/Synth Bass/Snare Drum Models, HiHat
- **Dynamics Processors**: Crossfade, Limiter
- **Effects**: Phaser, Wavefolder, Decimator, Overdrive, Chorus, Flanger, Tremolo
- **Filters**: One Pole, Ladder, SVF, FIR, SOAP
- **Noise Generators**: Clocked Noise, Dust, Fractal Noise, Particle Noise, Whitenoise
- **Physical Modeling**: Karplus Strong, Resonators, Modal Synthesis, String Voice
- **Sampling Engines**: Granular Player
- **Synthesis Methods**: Oscillators, FM, Formant Synthesis
- **Utilities**: Math Functions, Signal Conditioning, DC Blocker, Metro

## Integration Details

### Files Structure

```
dep/DaisySP/                  # DaisySP library source code
├── Source/
│   ├── daisysp.h            # Main include file
│   ├── Control/             # Envelope generators, phasors
│   ├── Drums/               # Drum synthesis models
│   ├── Dynamics/            # Crossfade, limiter
│   ├── Effects/             # Audio effects
│   ├── Filters/             # Various filter types
│   ├── Noise/               # Noise generators
│   ├── PhysicalModeling/    # Physical modeling synthesis
│   ├── Sampling/            # Granular synthesis
│   ├── Synthesis/           # Oscillators, FM
│   └── Utility/             # Utility functions
```

### Makefile Configuration

The Makefile has been configured to:

- Include all DaisySP source files in the build
- Add proper include paths for all DaisySP modules
- Compile all components for macOS/VCV Rack compatibility

## How to Use DaisySP in Your Modules

### 1. Include the DaisySP Header

```cpp
#include "daisysp.h"
```

### 2. Declare DaisySP Objects

```cpp
struct MyModule : Module {
    // DaisySP objects
    daisysp::Oscillator osc;
    daisysp::OnePole filter;
    daisysp::Adsr envelope;
    // ... other members
};
```

### 3. Initialize in Constructor

```cpp
MyModule() {
    // ... other initialization

    // Initialize DaisySP objects
    osc.Init(44100.f);        // Pass sample rate
    filter.Init();            // Some don't need sample rate
    envelope.Init(44100.f);   // Pass sample rate
}
```

### 4. Update Sample Rate When Needed

```cpp
void process(const ProcessArgs& args) override {
    // Update sample rate if it changed
    static float lastSampleRate = 0.f;
    if (args.sampleRate != lastSampleRate) {
        osc.Init(args.sampleRate);
        envelope.Init(args.sampleRate);
        lastSampleRate = args.sampleRate;
    }
    // ... process audio
}
```

### 5. Process Audio

```cpp
// Set parameters
osc.SetFreq(440.f);                    // Set frequency in Hz
osc.SetWaveform(daisysp::Oscillator::WAVE_SAW);

filter.SetFrequency(0.1f);             // Normalized frequency (0-0.497)
envelope.SetTime(ADSR_SEG_ATTACK, 0.1f);

// Process samples
float sample = osc.Process();
sample = filter.Process(sample);
sample *= envelope.Process(gate);
```

## Example Module: DaisyOscillator

The plugin includes `DaisyOscillator` as a demonstration module that shows:

- How to use `daisysp::Oscillator` for waveform generation
- How to use `daisysp::OnePole` for filtering
- Proper sample rate handling
- CV input processing for VCV Rack integration

### Controls:

- **Frequency**: Oscillator frequency with 1V/octave CV input
- **Waveform**: Selects between sine, triangle, saw, and square waves
- **Amplitude**: Output level control

## Available DaisySP Components

### Most Useful for VCV Rack:

#### Oscillators

- `daisysp::Oscillator` - Basic waveform oscillator
- `daisysp::VariableSawOsc` - Variable saw wave oscillator
- `daisysp::VariableShapeOsc` - Morphing oscillator
- `daisysp::Fm2` - 2-operator FM synthesizer

#### Filters

- `daisysp::OnePole` - Simple high/low pass filter
- `daisysp::Svf` - State variable filter (LP/HP/BP/Notch)
- `daisysp::Ladder` - Moog ladder filter

#### Effects

- `daisysp::Phaser` - Phaser effect
- `daisysp::Chorus` - Chorus effect
- `daisysp::Wavefolder` - Wave folding distortion
- `daisysp::Overdrive` - Overdrive distortion

#### Envelopes

- `daisysp::Adsr` - ADSR envelope generator
- `daisysp::AdEnv` - AD envelope generator

#### Physical Modeling

- `daisysp::KarplusString` - Karplus-Strong string synthesis
- `daisysp::ModalVoice` - Modal synthesis
- `daisysp::Resonator` - Resonant filter

## Notes

- All DaisySP modules process single samples (not blocks)
- Memory usage is static (no dynamic allocation)
- All processing uses `float` type
- Sample rates are handled per-module
- Most frequency parameters expect Hz values
- Filter frequencies often expect normalized values (0-0.497)

## Building

The plugin will automatically build with DaisySP when you run:

```bash
make clean && make
```

The build includes all DaisySP source files and creates a single plugin binary compatible with VCV Rack.
