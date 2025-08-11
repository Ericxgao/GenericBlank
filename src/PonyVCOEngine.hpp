#pragma once
#include "ChowDSP.hpp"
#include "DSPHelpers.hpp"
#include <rack.hpp>

using namespace ::rack;
using simd::float_4;

template<typename T>
class FoldStage1T {
public:
    T process(T x, T xt) {
        T y = simd::ifelse(simd::abs(x - xPrev) < 1e-5,
                           f(0.5 * (xPrev + x), xt),
                           (F(x, xt) - F(xPrev, xt)) / (x - xPrev));
        xPrev = x;
        return y;
    }
    static T f(T x, T xt) {
        return simd::ifelse(x > xt, +5 * xt - 4 * x, simd::ifelse(x < -xt, -5 * xt - 4 * x, x));
    }
    static T F(T x, T xt) {
        return simd::ifelse(x > xt,  5 * xt * x - 2 * x * x - 2.5 * xt * xt,
                            simd::ifelse(x < -xt, -5 * xt * x - 2 * x * x - 2.5 * xt * xt, x * x / 2.f));
    }
    void reset() { xPrev = 0.f; }
private:
    T xPrev = 0.f;
};

template<typename T>
class FoldStage2T {
public:
    T process(T x) {
        const T y = simd::ifelse(simd::abs(x - xPrev) < 1e-5, f(0.5 * (xPrev + x)), (F(x) - F(xPrev)) / (x - xPrev));
        xPrev = x;
        return y;
    }
    static T f(T x) {
        return simd::ifelse(-(x + 2) > c, c, simd::ifelse(x < -1, -(x + 2), simd::ifelse(x < 1, x, simd::ifelse(-x + 2 > -c, -x + 2, -c))));
    }
    static T F(T x) {
        return simd::ifelse(x > 0, F_signed(x), F_signed(-x));
    }
    static T F_signed(T x) {
        return simd::ifelse(x < 1, x * x * 0.5, simd::ifelse(x < 2.f + c, 2.f * x * (1.f - x * 0.25f) - 1.f,
                            2.f * (2.f + c) * (1.f - (2.f + c) * 0.25f) - 1.f - c * (x - 2.f - c)));
    }
    void reset() { xPrev = 0.f; }
private:
    T xPrev = 0.f;
    static constexpr float c = 0.1f;
};

class PonyVCOEngine {
public:
    bool blockTZFMDC = true;
    bool limitPW = true;
    bool removePulseDC = true;
    int oversamplingIndex = 1; // 2^1 = x2 by default

    void prepare(float sampleRate) {
        blockTZFMDCFilter.setCutoffFreq(5.0 / sampleRate);
        oversampler.setOversamplingIndex(oversamplingIndex);
        oversampler.reset(sampleRate);
        stage1.reset();
        stage2.reset();
    }

    int getOversamplingRatio() const { return oversampler.getOversamplingRatio(); }

    float_4 process(float sampleTime,
                    bool lfoMode,
                    float_4 freq,
                    float_4 timbre,
                    float_4 tzfmVoltage,
                    float_4 syncVoltage,
                    float_4 morph) {

        const int oversamplingRatio = lfoMode ? 1 : oversampler.getOversamplingRatio();

        if (blockTZFMDC) {
            blockTZFMDCFilter.process(tzfmVoltage);
            tzfmVoltage = blockTZFMDCFilter.highpass();
        }

        const float_4 deltaBasePhase = simd::clamp(freq * sampleTime / oversamplingRatio, -0.5f, 0.5f);
        const float_4 lowFreqRegime = simd::abs(deltaBasePhase) < 1e-3;
        const float_4 denominatorInv = 0.25 / (deltaBasePhase * deltaBasePhase);
        const float_4 deltaFMPhase = freq * tzfmVoltage * sampleTime / oversamplingRatio;

        float_4 pw = timbre;
        if (limitPW) pw = clamp(pw, 0.05, 0.95);
        const float_4 pulseDCOffset = (!removePulseDC) * 2.f * (0.5f - pw);

        // Sync
        morph = simd::clamp(morph, 0.f, 3.f);
        const float_4 resetPhase = simd::ifelse(morph < 1.0f, 0.25f, 0.f);
        const float_4 syncMask = syncTrigger.process(syncVoltage);
        phase = simd::ifelse(syncMask, resetPhase, phase);

        float_4* osBuffer = oversampler.getOSBuffer();
        for (int i = 0; i < oversamplingRatio; ++i) {
            phase += deltaBasePhase + deltaFMPhase;
            phase -= simd::floor(phase);

            float_4 phases[3];
            phases[0] = phase - 2 * deltaBasePhase + simd::ifelse(phase < 2 * deltaBasePhase, 1.f, 0.f);
            phases[1] = phase - deltaBasePhase + simd::ifelse(phase < deltaBasePhase, 1.f, 0.f);
            phases[2] = phase;

            // Compute shapes
            float_4 v_sin = sin2pi_pade_05_5_4(phase);
            v_sin = wavefolder(v_sin, (1 - 0.85 * timbre));

            float_4 v_tri;
            {
                const float_4 dpwOrder1 = 1.0 - 2.0 * simd::abs(2 * phase - 1.0);
                const float_4 dpwOrder3 = aliasSuppressedTri(phases) * denominatorInv;
                v_tri = simd::ifelse(lowFreqRegime, dpwOrder1, dpwOrder3);
                v_tri = wavefolder(v_tri, (1 - 0.85 * timbre));
            }

            float_4 v_saw;
            {
                const float_4 dpwOrder1 = 2 * phase - 1.0;
                const float_4 dpwOrder3 = aliasSuppressedSaw(phases) * denominatorInv;
                v_saw = simd::ifelse(lowFreqRegime, dpwOrder1, dpwOrder3);
                v_saw = wavefolder(v_saw, (1 - 0.85 * timbre));
            }

            float_4 v_pulse;
            {
                float_4 dpwOrder1 = simd::ifelse(phase < 1. - pw, +1.0, -1.0);
                dpwOrder1 -= removePulseDC ? 2.f * (0.5f - pw) : 0.f;
                float_4 saw = aliasSuppressedSaw(phases);
                float_4 sawOffset = aliasSuppressedOffsetSaw(phases, pw);
                float_4 dpwOrder3 = (sawOffset - saw) * denominatorInv + pulseDCOffset;
                v_pulse = simd::ifelse(lowFreqRegime, dpwOrder1, dpwOrder3);
                v_pulse *= 0.3f; // loudness trim
            }

            // Equal-power morph
            const float_4 mClamped = simd::clamp(morph, 0.0f, 2.999f);
            const float_4 base = simd::floor(mClamped);
            const float_4 frac = mClamped - base;
            const float_4 theta = frac * float(M_PI_2);
            const float_4 w0 = simd::cos(theta);
            const float_4 w1 = simd::sin(theta);
            const float_4 out0 = w0 * v_sin  + w1 * v_tri;
            const float_4 out1 = w0 * v_tri  + w1 * v_saw;
            const float_4 out2 = w0 * v_saw  + w1 * v_pulse;
            const float_4 sel0 = (base < 1.0f);
            const float_4 sel1 = (base >= 1.0f) & (base < 2.0f);
            osBuffer[i] = simd::ifelse(sel0, out0, simd::ifelse(sel1, out1, out2));
        }

        return (oversamplingRatio > 1) ? oversampler.downsample() : oversampler.getOSBuffer()[0];
    }

private:
    float_4 aliasSuppressedTri(float_4* phases) {
        float_4 triBuffer[3];
        for (int i = 0; i < 3; ++i) {
            float_4 p = 2 * phases[i] - 1.0;
            float_4 s = 0.5 - simd::abs(p);
            triBuffer[i] = (s * s * s - 0.75 * s) / 3.0;
        }
        return (triBuffer[0] - 2.0 * triBuffer[1] + triBuffer[2]);
    }
    float_4 aliasSuppressedSaw(float_4* phases) {
        float_4 sawBuffer[3];
        for (int i = 0; i < 3; ++i) {
            float_4 p = 2 * phases[i] - 1.0;
            sawBuffer[i] = (p * p * p - p) / 6.0;
        }
        return (sawBuffer[0] - 2.0 * sawBuffer[1] + sawBuffer[2]);
    }
    float_4 aliasSuppressedOffsetSaw(float_4* phases, float_4 pw) {
        float_4 sawOffsetBuff[3];
        for (int i = 0; i < 3; ++i) {
            float_4 p = 2 * phases[i] - 1.0;
            float_4 pwp = p + 2 * pw;
            pwp += simd::ifelse(pwp > 1, -2, 0);
            sawOffsetBuff[i] = (pwp * pwp * pwp - pwp) / 6.0;
        }
        return (sawOffsetBuff[0] - 2.0 * sawOffsetBuff[1] + sawOffsetBuff[2]);
    }
    float_4 wavefolder(float_4 x, float_4 xt) {
        return stage2.process(stage1.process(x, xt));
    }

    chowdsp::VariableOversampling<6, float_4> oversampler;
    dsp::TRCFilter<float_4> blockTZFMDCFilter;
    dsp::TSchmittTrigger<float_4> syncTrigger;
    FoldStage1T<float_4> stage1;
    FoldStage2T<float_4> stage2;
    float_4 phase = 0.f;
};


