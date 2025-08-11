#pragma once

#include "plugin.hpp"

// Minimal drive stage inspired by ChowDer: pre-emphasis (bass/treble), soft clip, DC block.
// Single control drive01 in [0,1] simultaneously increases bass, treble, and drive gain.
// SIMD4 version to match the rest of the signal path.
struct DriveStageSIMD4 {
    DriveStageSIMD4() {}

    inline void reset() {
        lowpassState = simd::float_4(0.f);
        dcPrevX = simd::float_4(0.f);
        dcPrevY = simd::float_4(0.f);
    }

    inline simd::float_4 process(const simd::float_4 x,
                                  float sampleTime,
                                  float drive01) {
        // Compute pre-emphasis gains and drive from a single control
        drive01 = clamp(drive01, 0.f, 1.f);
        // Bass/Treble gains: map 0..1 to roughly -6..+9 dB to avoid extreme boosts
        const float bassTrebleDb = -6.f + 15.f * drive01;
        const float bassTrebleLin = dsp::dbToAmplitude(bassTrebleDb);
        // Drive gain: map to 0..24 dB
        const float driveDb = 24.f * drive01;
        const simd::float_4 driveGain = simd::float_4(dsp::dbToAmplitude(driveDb));

        // Split into low and high bands with a simple 1-pole lowpass at ~600 Hz
        const float lpHz = 600.f;
        const float alphaLP = 1.f - std::exp(-2.f * float(M_PI) * lpHz * sampleTime);
        lowpassState += simd::float_4(alphaLP) * (x - lowpassState);
        const simd::float_4 lowBand = lowpassState;
        const simd::float_4 highBand = x - lowBand;

        // Apply equal bass/treble boost/cut
        const simd::float_4 pre = lowBand * simd::float_4(bassTrebleLin)
                                + highBand * simd::float_4(bassTrebleLin);

        // Apply drive and soft clip with a smooth polynomial saturator
        const simd::float_4 d = pre * driveGain;
        const simd::float_4 d2 = d * d;
        // y = x * (27 + x^2) / (27 + 9 x^2) ∈ [-1,1], smooth soft clip
        const simd::float_4 y = d * (simd::float_4(27.f) + d2)
                              / (simd::float_4(27.f) + simd::float_4(9.f) * d2);

        // Simple 1st-order DC blocker at ~30 Hz
        const float dcHz = 30.f;
        const float a = std::exp(-2.f * float(M_PI) * dcHz * sampleTime);
        const simd::float_4 ydc = simd::float_4(a) * (dcPrevY + y - dcPrevX);
        dcPrevX = y;
        dcPrevY = ydc;
        return ydc;
    }

private:
    simd::float_4 lowpassState = simd::float_4(0.f);
    simd::float_4 dcPrevX = simd::float_4(0.f);
    simd::float_4 dcPrevY = simd::float_4(0.f);
};


