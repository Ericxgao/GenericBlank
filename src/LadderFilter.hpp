#pragma once
#include <rack.hpp>

using namespace ::rack;
using simd::float_4;

// Simple linear crossfade helper
template <typename T>
inline T lf_crossfade(T a, T b, T x) {
    return a + (b - a) * x;
}

// Soft clip used inside the ladder core (Pade tanh approximant)
template <typename T>
inline T lf_clip(T x) {
    x = simd::clamp(x, T(-3.f), T(3.f));
    return x * (T(27) + x * x) / (T(27) + T(9) * x * x);
}

// 4-pole Moog-style ladder filter core, RK4-integrated
template <typename T>
struct LadderFilter {
    T omega0 {};
    T resonance = 1;
    T state[4] {};
    T input {};

    LadderFilter() {
        reset();
        setCutoff(T(0));
    }

    void reset() {
        for (int i = 0; i < 4; ++i) state[i] = T(0);
        input = T(0);
    }

    // cutoff in Hz
    void setCutoff(T cutoffHz) {
        omega0 = T(2 * M_PI) * cutoffHz;
    }

    // resonance control expects something like 0..~10 (host maps UI/CV)
    void setResonance(T r) { resonance = r; }

    // Process one sample (scalar or SIMD). dt is host sampleTime
    void process(T in, T dt) {
        dsp::stepRK4(T(0), dt, state, 4, [&](T t, const T x[], T dxdt[]) {
            T inp_t = lf_crossfade(input, in, t / dt);

            // Default bass-resonance compensation: boost input as resonance rises,
            // tapered by normalized cutoff so it focuses on low frequencies.
            const T k = T(0.3); // tune 0.2..0.4 as desired
            const T sr = T(1) / dt;
            const T fc = omega0 / T(2 * M_PI);
            const T fcn = simd::clamp(fc / (T(0.5) * sr), T(0), T(1));
            const T comp = T(1) + k * resonance * (T(1) - fcn);

            T inputc = lf_clip(comp * inp_t - resonance * x[3]);
            T yc0 = lf_clip(x[0]);
            T yc1 = lf_clip(x[1]);
            T yc2 = lf_clip(x[2]);
            T yc3 = lf_clip(x[3]);

            dxdt[0] = omega0 * (inputc - yc0);
            dxdt[1] = omega0 * (yc0 - yc1);
            dxdt[2] = omega0 * (yc1 - yc2);
            dxdt[3] = omega0 * (yc2 - yc3);
        });

        input = in;
    }

    T lowpass() const { return state[3]; }

    T highpass() const {
        // Input node estimate
        T u = lf_clip((input - resonance * state[3]));
        return lf_clip(u - T(4) * state[0] + T(6) * state[1] - T(4) * state[2] + state[3]);
    }
};

// Convenience wrapper to manage four SIMD lanes as poly groups of 4
struct LadderFilterSIMD4 {
    LadderFilter<float_4> core;

    void reset() { core.reset(); }
    void setCutoff(float_4 hz) { core.setCutoff(hz); }
    void setResonance(float_4 r) { core.setResonance(r); }
    void process(float_4 in, float dt) { core.process(in, float_4(dt)); }
    float_4 lowpass() const { return core.lowpass(); }
    float_4 highpass() const { return core.highpass(); }
};


