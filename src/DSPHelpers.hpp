#pragma once
#include <rack.hpp>
using namespace ::rack;

// Helper math/DSP functions moved out of plugin.hpp

template <typename T>
T sin2pi_pade_05_5_4(T x) {
    x -= 0.5f;
    return (T(-6.283185307) * x + T(33.19863968) * simd::pow(x, 3) - T(32.44191367) * simd::pow(x, 5))
           / (1 + T(1.296008659) * simd::pow(x, 2) + T(0.7028072946) * simd::pow(x, 4));
}

template <typename T>
T tanh_pade(T x) {
    T x2 = x * x;
    T q = 12.f + x2;
    return 12.f * x * q / (36.f * x2 + q * q);
}

template <typename T>
T exponentialBipolar80Pade_5_4(T x) {
    return (T(0.109568) * x + T(0.281588) * simd::pow(x, 3) + T(0.133841) * simd::pow(x, 5))
           / (T(1.) - T(0.630374) * simd::pow(x, 2) + T(0.166271) * simd::pow(x, 4));
}

template <typename T>
static T clip(T x) {
    const T limit = 1.16691853009184f;
    x = clamp(x * 0.1f, -limit, limit);
    return 10.0f * (x + 1.45833f * simd::pow(x, 13) + 0.559028f * simd::pow(x, 25) + 0.0427035f * simd::pow(x, 37))
           / (1.0f + 1.54167f * simd::pow(x, 12) + 0.642361f * simd::pow(x, 24) + 0.0579909f * simd::pow(x, 36));
}

// Butterworth 2*Nth order highpass DC blocker

template<int N, typename T>
struct DCBlockerT {

    DCBlockerT() {
        setFrequency(0.1f);
    }

    void setFrequency(float fc) {
        fc_ = fc;
        recalc();
    }

    T process(T x) {
        for (int idx = 0; idx < N; idx++) {
            x = blockDCFilter[idx].process(x);
        }
        return x;
    }

private:
    void recalc() {
        float poleInc = M_PI / order;
        float firstAngle = poleInc / 2;
        for (int idx = 0; idx < N; idx++) {
            float Q = 1.0f / (2.0f * std::cos(firstAngle + idx * poleInc));
            blockDCFilter[idx].setParameters(dsp::TBiquadFilter<T>::HIGHPASS, fc_, Q, 1.0f);
        }
    }

    float fc_ {};
    static const int order = 2 * N;
    dsp::TBiquadFilter<T> blockDCFilter[N];
};

typedef DCBlockerT<2, float> DCBlocker;

// Simple SIMD pulse generator with hold
struct PulseGenerator_4 {
    simd::float_4 remaining = 0.f;
    void reset() { remaining = 0.f; }
    simd::float_4 process(float dt) {
        simd::float_4 mask = (remaining > 0.f);
        remaining -= ifelse(mask, dt, 0.f);
        return ifelse(mask, simd::float_4::mask(), 0.f);
    }
    void trigger(simd::float_4 mask, float duration = 1e-3f) {
        remaining = ifelse(mask & (duration > remaining), duration, remaining);
    }
};

// Soft saturator

template <class T>
struct Saturator {
    static T process(T sample) {
        return simd::ifelse(sample < 0.f, -saturation(-sample), saturation(sample));
    }
private:
    static T saturation(T sample) {
        const float limit = 1.05f;
        const float y1 = 0.98765f;
        const float offset = 0.0062522;
        T x = sample / limit;
        T x1 = (x + 1.0f) * 0.5f;
        return limit * (offset + x1 - simd::sqrt(x1 * x1 - y1 * x) * (1.0f / y1));
    }
};


