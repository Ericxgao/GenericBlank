#pragma once

#include "plugin.hpp"

// Very basic ring modulator for SIMD4: multiplies two signals, with a strength
// parameter that scales the second signal before multiplication.
// Output = signalA * (signalB * strength01)
struct RingModulatorSIMD4 {
    inline simd::float_4 process(const simd::float_4 signalA,
                                 const simd::float_4 signalB,
                                 float strength01) const {
        const float s = clamp(strength01, 0.f, 1.f);
        return signalA * (signalB * simd::float_4(s));
    }
};


