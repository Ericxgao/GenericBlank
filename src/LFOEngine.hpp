// Reusable SIMD-4 LFO engine with clock/reset handling and sample-and-hold
//
// This header provides a small API to generate common LFO shapes using VCV Rack
// primitives. It is designed to be embedded in modules that need one or more
// LFO voices without duplicating widget/module-specific code.
//
// Features
// - Sine, Triangle, Sawtooth, and Square wave generation
// - Unipolar/Bipolar offset and inversion
// - External Clock input support (frequency follows clock period)
// - Frequency control via exponential pitch parameter with FM input
// - Pulse width parameter with PWM input
// - Poly/Multichannel friendly via SIMD float_4 state
// - Sample-and-Hold output with optional external trigger; falls back to cycle reset
//
// Typical usage per SIMD group (4 lanes):
//   LFOEngineSIMD4 lfo;
//   lfo.reset();
//   ... inside process() each frame ...
//   lfo.setOffsetEnabled(offset);
//   lfo.setInvertEnabled(invert);
//   lfo.updateClockFromMono(clockVoltage, args.sampleTime); // optional
//   auto out = lfo.process(args.sampleTime,
//                          freqParam,
//                          fmDepth,
//                          fmInputSimd,           // can be 0
//                          pulseWidthParam,
//                          pwmDepth,
//                          pwmInputSimd,          // can be 0
//                          resetInputSimd,        // can be 0
//                          shTriggerInputSimd);   // can be 0
//   // out.sine/triangle/saw/square/hold are ±5V or 0..10V depending on offset
//
#pragma once

#include "plugin.hpp"

using simd::float_4;

struct LFOOutputsSIMD4 {
    float_4 sine;
    float_4 triangle;
    float_4 saw;
    float_4 square;
    float_4 sampleAndHold; // bipolar by default; offset/invert applied to match other shapes
};

class LFOEngineSIMD4 {
public:
    LFOEngineSIMD4() {
        reset();
    }

    void reset() {
        phases = 0.f;
        clockFrequencyHz = 2.f; // default when unclocked (matches Fundamental's behavior)
        clockTimer.reset();
        heldValue = 0.f;
    }

    // UI/behavior toggles
    void setOffsetEnabled(bool enabled) { offsetEnabled = enabled; }
    void setInvertEnabled(bool enabled) { invertEnabled = enabled; }

    // If you have a mono clock input, call once per frame before process()
    void updateClockFromMono(float clockVoltage, float sampleTime) {
        clockTimer.process(sampleTime);
        if (clockMonoTrigger.process(clockVoltage, 0.1f, 2.f)) {
            float freq = 1.f / clockTimer.getTime();
            clockTimer.reset();
            if (freq >= 0.001f && freq <= 1000.f) {
                clockFrequencyHz = freq;
            }
        }
    }

    // Main processor. All inputs are per-SIMD-group values except sampleTime/freqParam/etc.
    // - freqParam is an exponential pitch (octaves) around 0.0 → multiplied by clock freq
    // - fmDepth is linear depth scaling applied to fmInputV
    // - pulseWidthParam is 0.01..0.99
    // - pwmDepth scales pwmInputV (expects 0–10 V poly)
    // - resetV triggers phase reset on rising edge (0.1–2V window)
    // - shTrigV triggers a new sample for S&H; if not provided, cycle reset will sample instead
    LFOOutputsSIMD4 process(
        float sampleTime,
        float freqParam,
        float fmDepth,
        const float_4& fmInputV,
        float pulseWidthParam,
        float pwmDepth,
        const float_4& pwmInputV,
        const float_4& resetV,
        const float_4& shTrigV
    ) {
        // Frequency calc: base from clockFrequencyHz scaled by exp2(pitch)
        float_4 pitch = float_4(freqParam);
        // FM is voltage-scaled by fmDepth (expects ±5V or 0..10V depending on caller)
        pitch += fmInputV * float_4(fmDepth);
        float_4 freq = float_4(clockFrequencyHz / 2.f) * dsp::exp2_taylor5(pitch);

        // Pulse width with PWM
        float_4 pw = float_4(pulseWidthParam);
        pw += pwmInputV / 10.f * float_4(pwmDepth);
        pw = simd::clamp(pw, 0.01f, 0.99f);

        // Advance phase. Clamp delta for stability, then wrap
        float_4 deltaPhase = simd::fmin(freq * float_4(sampleTime), 0.5f);
        phases += deltaPhase;
        // detect wrap for S&H fallback
        float_4 wrapped = phases - simd::trunc(phases);
        float_4 didWrap = simd::notEqual(phases, wrapped);
        phases = wrapped;

        // Reset
        float_4 resetTrig = resetTriggers.process(resetV, 0.1f, 2.f);
        phases = simd::ifelse(resetTrig, 0.f, phases);

        // Optional S&H trigger input; otherwise use wrap/reset as the trigger
        float_4 shTrig = shTriggers.process(shTrigV, 0.1f, 2.f);
        float_4 useWrap = simd::max(shTrig, simd::max(didWrap, resetTrig));
        // When triggered, sample new random value per lane in [-1, 1]
        if (simd::movemask(useWrap)) {
            // Generate new random per lane lazily only when any lane triggers
            float r0 = 2.f * random::uniform() - 1.f;
            float r1 = 2.f * random::uniform() - 1.f;
            float r2 = 2.f * random::uniform() - 1.f;
            float r3 = 2.f * random::uniform() - 1.f;
            float_4 newRand(r0, r1, r2, r3);
            heldValue = simd::ifelse(useWrap, newRand, heldValue);
        }

        // Waveforms in ±1 range
        float_4 p = phases;
        float_4 sine = simd::sin(2.f * M_PI * p);
        float_4 triangle = 4.f * simd::fabs(p - simd::round(p)) - 1.f;
        float_4 saw = 2.f * (p - simd::round(p));
        float_4 square = simd::ifelse(p < pw, 1.f, -1.f);

        // Invert if needed
        if (invertEnabled) {
            sine *= -1.f;
            triangle *= -1.f;
            saw *= -1.f;
            square *= -1.f;
        }

        // Offset to unipolar (0..2) before scaling to volts
        if (offsetEnabled) {
            sine += 1.f;
            triangle += 1.f;
            saw += 1.f;
            square += 1.f;
        }

        // Scale to volts. Bipolar: ±5V, Unipolar: 0..10V
        float_4 scale = float_4(5.f);
        LFOOutputsSIMD4 out;
        out.sine = scale * sine;
        out.triangle = scale * triangle;
        out.saw = scale * saw;
        out.square = scale * square;

        // S&H follows the same offset/invert policy for consistency
        float_4 sh = heldValue;
        if (invertEnabled) sh *= -1.f;
        if (offsetEnabled) sh += 1.f;
        out.sampleAndHold = scale * sh;
        return out;
    }

    // Expose current phases for external visualization if needed
    float_4 getPhases() const { return phases; }

    // Allow external override of the clock-follow frequency (Hz)
    void setClockFrequencyHz(float frequencyHz) { clockFrequencyHz = frequencyHz; }

private:
    // State per SIMD group
    float_4 phases = 0.f;
    float_4 heldValue = 0.f;

    // Global/mono clock tracking
    dsp::SchmittTrigger clockMonoTrigger;
    float clockFrequencyHz = 2.f;
    dsp::Timer clockTimer;

    // Per-lane triggers
    dsp::TSchmittTrigger<float_4> resetTriggers;
    dsp::TSchmittTrigger<float_4> shTriggers;

    bool offsetEnabled = false;
    bool invertEnabled = false;
};


