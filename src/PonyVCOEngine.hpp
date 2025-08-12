#pragma once
#include "ChowDSP.hpp"
#include "DSPHelpers.hpp"
#include <rack.hpp>

using namespace ::rack;
using simd::float_4;

template<typename T>
inline T simd_sign(T x) {
    // returns +1 for x>=0, -1 for x<0
    return simd::ifelse(x >= T(0.f), T(1.f), T(-1.f));
}

template<typename T>
inline T rcp_nr(T x) {
    // one-step Newton on hardware reciprocal (keeps accuracy good + faster than div)
    T r0 = simd::rcp(x);         // if your simd doesn't have rcp(), keep 1/x
    return r0 * (T(2.f) - x * r0);
}

/*** Stage 1 (two-segment fold) ********************************************/

template<typename T>
class FoldStage1T {
public:
    T process(T x, T xt) {
        const T dx   = x - xPrev;
        const T adx  = simd::abs(dx);
        const T mid  = T(0.5f) * (x + xPrev);
        const T useMid = (adx < T(1e-5f)); // mask

        // F and f in branch-light form
        const T y_div = (F(x, xt) - F(xPrev, xt)) * rcp_nr(dx);
        const T y_mid = f(mid, xt);

        const T y = simd::ifelse(useMid, y_mid, y_div);
        xPrev = x;
        return y;
    }

    static T f(T x, T xt) {
        // f(x) = x inside |x|<=xt; outside: x - 5*sign(x)*(|x|-xt)
        const T ax = simd::abs(x);
        const T s  = simd_sign(x);
        const T d  = simd::fmax(ax - xt, T(0.f));
        return x - T(5.f) * s * d;
    }

    static T F(T x, T xt) {
        // two-region antiderivative using one mask
        const T ax   = simd::abs(x);
        const T s    = simd_sign(x);
        const T xt2  = xt * xt;
        // inside: 0.5*x^2
        const T Fin  = T(0.5f) * x * x;
        // outside: s*5*xt*x - 2*x^2 - 2.5*xt^2
        const T Fout = s * T(5.f) * xt * x - T(2.f) * x * x - T(2.5f) * xt2;
        const T outside = (ax > xt);
        return simd::ifelse(outside, Fout, Fin);
    }

    void reset() { xPrev = T(0.f); }

private:
    T xPrev = T(0.f);
};

/*** Stage 2 (triangle-with-plateaus) **************************************/

template<typename T>
class FoldStage2T {
public:
    T process(T x) {
        const T dx   = x - xPrev;
        const T adx  = simd::abs(dx);
        const T mid  = T(0.5f) * (x + xPrev);
        const T useMid = (adx < T(1e-5f));

        const T y_div = (F(x) - F(xPrev)) * rcp_nr(dx);
        const T y_mid = f(mid);

        const T y = simd::ifelse(useMid, y_mid, y_div);
        xPrev = x;
        return y;
    }

    // Branch-light f: use |x| bands and a single sign
    static T f(T x) {
        const T cT  = T(c);
        const T ax  = simd::abs(x);
        const T s   = simd_sign(x);

        // center: y =  s*ax          for ax < 1
        // slope:  y =  s*(2-ax)      for 1 <= ax < 2+c
        // far:    y = -s*c           for ax >= 2+c
        T y = s * ax;
        const T m1 = (ax >= T(1.f));
        const T m2 = (ax >= (T(2.f) + cT));
        y = simd::ifelse(m1, s * (T(2.f) - ax), y);
        y = simd::ifelse(m2, -s * cT, y);
        return y;
    }

    static T F(T x) {
        // exploit symmetry: F(x) depends on |x| only
        const T cT = T(c);
        const T ax = simd::abs(x);

        // Region 1: ax < 1         -> 0.5*ax^2
        // Region 2: 1 <= ax < 2+c  -> 2*ax*(1 - ax/4) - 1
        // Region 3: ax >= 2+c      -> const - c*(ax - (2+c))
        const T ax2 = ax * ax;

        const T F1 = T(0.5f) * ax2;

        const T F2 = T(2.f) * ax * (T(1.f) - ax * T(0.25f)) - T(1.f);

        const T K  = T(2.f) + cT;
        const T F2_at_K = T(2.f) * K * (T(1.f) - K * T(0.25f)) - T(1.f);
        const T F3 = F2_at_K - cT * (ax - K);

        T Fs = F1;
        const T m12 = (ax >= T(1.f));
        const T m3  = (ax >= K);
        Fs = simd::ifelse(m12, F2, Fs);
        Fs = simd::ifelse(m3,  F3, Fs);
        return Fs;
    }

    void reset() { xPrev = T(0.f); }
private:
    T xPrev = T(0.f);
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
                    float_4 waveformSel) {

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

        // Sync: per-lane reset (sine uses 0.25 offset)
        const float_4 syncMask = syncTrigger.process(syncVoltage);
        // Make waveform selection robust to floating point jitter by rounding to nearest integer
        const float_4 wfSelRounded = simd::round(waveformSel);
        const float_4 mSin = (wfSelRounded == float_4(0.f));
        const float_4 resetPhase = simd::ifelse(mSin, 0.25f, 0.0f);
        phase = simd::ifelse(syncMask, resetPhase, phase);

        float_4* osBuffer = oversampler.getOSBuffer();
        for (int i = 0; i < oversamplingRatio; ++i) {
            phase += deltaBasePhase + deltaFMPhase;
            phase -= simd::floor(phase);

            // Compute per-lane shapes only as needed
            const float_4 mTri = (wfSelRounded < float_4(2.f));
            const float_4 mSaw = (wfSelRounded < float_4(3.f));
            const float_4 mPulse = (wfSelRounded < float_4(4.f));

            const bool needSin = simd::movemask(mSin) != 0;
            const bool needTri = simd::movemask(mTri) != 0;
            const bool needSaw = simd::movemask(mSaw) != 0;
            const bool needPulse = simd::movemask(mPulse) != 0;

            float_4 out = 0.f;

            printf("needSin: %d, needTri: %d, needSaw: %d, needPulse: %d\n", needSin, needTri, needSaw, needPulse);

            // Compute and apply sine per lane
            if (needSin) {
                const float_4 sinOut = sin2pi_pade_05_5_4(phase);
                out = simd::ifelse(mSin, sinOut, out);
            }

            // Prepare DPW phases once for tri/saw/pulse branches
            float_4 phases[3];
            if (needTri || needSaw || needPulse) {
                phases[0] = phase - 2 * deltaBasePhase + simd::ifelse(phase < 2 * deltaBasePhase, 1.f, 0.f);
                phases[1] = phase - deltaBasePhase + simd::ifelse(phase < deltaBasePhase, 1.f, 0.f);
                phases[2] = phase;
            }

            // Triangle per lane
            if (needTri) {
                const float_4 dpwOrder1 = 1.0 - 2.0 * simd::abs(2 * phase - 1.0);
                const float_4 dpwOrder3 = aliasSuppressedTri(phases) * denominatorInv;
                const float_4 triOut = simd::ifelse(lowFreqRegime, dpwOrder1, dpwOrder3);
                out = simd::ifelse(mTri, triOut, out);
            }

            // Saw per lane
            if (needSaw) {
                const float_4 dpwOrder1 = 2 * phase - 1.0;
                const float_4 dpwOrder3 = aliasSuppressedSaw(phases) * denominatorInv;
                const float_4 sawOut = simd::ifelse(lowFreqRegime, dpwOrder1, dpwOrder3);
                out = simd::ifelse(mSaw, sawOut, out);
            }

            // Pulse per lane
            if (needPulse) {
                float_4 dpwOrder1 = simd::ifelse(phase < 1. - pw, +1.0, -1.0);
                dpwOrder1 -= removePulseDC ? 2.f * (0.5f - pw) : 0.f;
                float_4 saw = aliasSuppressedSaw(phases);
                float_4 sawOffset = aliasSuppressedOffsetSaw(phases, pw);
                float_4 dpwOrder3 = (sawOffset - saw) * denominatorInv + pulseDCOffset;
                const float_4 pulseOut = simd::ifelse(lowFreqRegime, dpwOrder1, dpwOrder3);
                out = simd::ifelse(mPulse, pulseOut, out);
            }

            const float_4 nonPulseMask = simd::ifelse(mPulse, float_4(0.f), float_4(1.f));
            osBuffer[i] = out;
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


