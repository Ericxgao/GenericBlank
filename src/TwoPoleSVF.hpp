#pragma once

#include <rack.hpp>
#include <cmath>

// Lightweight 2-pole resonant filter using TPT State Variable structure
// - Good sounding, stable at all cutoffs
// - Cheap: a handful of mul/adds per sample, one tanf per cutoff change

namespace mm_dsp {

using namespace ::rack;
using simd::float_4;

// Scalar TPT SVF (float)
struct TwoPoleSVF {
    float sampleRate = 44100.f;
    // Coefficients
    float g = 0.f;      // tan(pi * fc / fs)
    float k = 1.f;      // damping = 1/Q
    float a1 = 1.f;     // 1 / (1 + g*(g + k))
    // State (Simper eq. integrator states)
    float ic1eq = 0.f;
    float ic2eq = 0.f;
    // Extra emphasis mix (adds bandpass into lowpass for stronger resonance peak)
    float emphasis = 0.f;

    void reset() {
        ic1eq = 0.f;
        ic2eq = 0.f;
    }

    void setSampleRate(float sr) {
        sampleRate = std::max(1.f, sr);
    }

    // cutoff Hz in [0, fs/2)
    void setCutoff(float cutoffHz) {
        cutoffHz = math::clamp(cutoffHz, 1.f, 0.49f * sampleRate);
        const float x = float(M_PI) * cutoffHz / sampleRate;
        g = std::tan(x);
        a1 = 1.f / (1.f + g * (g + k));
    }

    // Q in [0.25, 1000], where high values will self-oscillate
    void setQ(float Q) {
        Q = math::clamp(Q, 0.25f, 1000.f);
        k = 1.f / Q;
        // a1 depends on k; recompute with current g
        a1 = 1.f / (1.f + g * (g + k));
    }

    // Resonance in [0,1] mapped to a much higher Q range for strong resonance
    void setResonance01(float r01) {
        r01 = math::clamp(r01, 0.f, 1.f);
        // Steep curve near the top; push to extreme resonance at max
        const float r2 = r01 * r01;
        const float r6 = r2 * r2 * r2; // r01^6
        const float Q = 0.5f + r6 * 1000.f; // Q ≈ 0.5 .. 1000.5
        // Emphasis grows faster than Q so the peak is clearly audible
        emphasis = r2 * 20.0f; // 0..20x BP added to LP
        setQ(Q);
    }

    // Process one sample, return low-pass output
    inline float process(float x) {
        // Simper SVF (zero-delay):
        // v0 = x - k*ic1eq - ic2eq
        // v1 = a1 * v0
        // v2 = g * v1
        // v3 = g * v2
        // lp = ic2eq + v3
        // bp = ic1eq + v2
        // hp = v0 - k*bp - lp
        // ic1eq = bp + v2; ic2eq = lp + v3

        const float v0 = x - k * ic1eq - ic2eq;
        const float v1 = a1 * v0;
        const float v2 = g * v1;
        const float v3 = g * v2;
        const float lp = ic2eq + v3;
        const float bp = ic1eq + v2;

        // Update states
        ic1eq = bp + v2;
        ic2eq = lp + v3;

        // Lowpass with extra bandpass emphasis
        return lp + emphasis * bp;
    }
};

// SIMD wrapper: runs four scalar filters in parallel for a float_4 block
struct TwoPoleSVFSIMD4 {
    TwoPoleSVF lanes[4];

    void reset() {
        for (int i = 0; i < 4; ++i) lanes[i].reset();
    }

    void setSampleRate(float sr) {
        for (int i = 0; i < 4; ++i) lanes[i].setSampleRate(sr);
    }

    void setCutoff(float cutoffHz) {
        for (int i = 0; i < 4; ++i) lanes[i].setCutoff(cutoffHz);
    }

    // Optional per-lane cutoff
    void setCutoff(float_4 cutoffHz) {
        alignas(16) float c[4];
        cutoffHz.store(c);
        for (int i = 0; i < 4; ++i) lanes[i].setCutoff(c[i]);
    }

    // Resonance 0..1 mapped to Q internally
    void setResonance01(float r01) {
        for (int i = 0; i < 4; ++i) lanes[i].setResonance01(r01);
    }

    void setResonance01(float_4 r01) {
        alignas(16) float r[4];
        r01.store(r);
        for (int i = 0; i < 4; ++i) lanes[i].setResonance01(r[i]);
    }

    // Process a SIMD vector; returns low-pass
    inline float_4 process(float_4 x) {
        alignas(16) float in[4];
        alignas(16) float out[4];
        x.store(in);
        for (int i = 0; i < 4; ++i) out[i] = lanes[i].process(in[i]);
        return float_4::load(out);
    }
};

// Cascade two 2-pole SVFs to approximate a 4-pole lowpass with stronger resonance peak
struct TwoPoleSVF2x {
    TwoPoleSVF f1;
    TwoPoleSVF f2;

    void reset() {
        f1.reset();
        f2.reset();
    }
    void setSampleRate(float sr) {
        f1.setSampleRate(sr);
        f2.setSampleRate(sr);
    }
    void setCutoff(float cutoffHz) {
        f1.setCutoff(cutoffHz);
        f2.setCutoff(cutoffHz);
    }
    void setResonance01(float r01) {
        // Apply the same resonance to both stages
        f1.setResonance01(r01);
        f2.setResonance01(r01);
    }
    inline float process(float x) {
        return f2.process(f1.process(x));
    }
};

struct TwoPoleSVF2xSIMD4 {
    TwoPoleSVF2x lanes[4];

    void reset() {
        for (int i = 0; i < 4; ++i) lanes[i].reset();
    }
    void setSampleRate(float sr) {
        for (int i = 0; i < 4; ++i) lanes[i].setSampleRate(sr);
    }
    void setCutoff(float cutoffHz) {
        for (int i = 0; i < 4; ++i) lanes[i].setCutoff(cutoffHz);
    }
    void setCutoff(float_4 cutoffHz) {
        alignas(16) float c[4];
        cutoffHz.store(c);
        for (int i = 0; i < 4; ++i) lanes[i].setCutoff(c[i]);
    }
    void setResonance01(float r01) {
        for (int i = 0; i < 4; ++i) lanes[i].setResonance01(r01);
    }
    void setResonance01(float_4 r01) {
        alignas(16) float r[4];
        r01.store(r);
        for (int i = 0; i < 4; ++i) lanes[i].setResonance01(r[i]);
    }
    inline float_4 process(float_4 x) {
        alignas(16) float in[4];
        alignas(16) float out[4];
        x.store(in);
        for (int i = 0; i < 4; ++i) out[i] = lanes[i].process(in[i]);
        return float_4::load(out);
    }
};

} // namespace mm_dsp


