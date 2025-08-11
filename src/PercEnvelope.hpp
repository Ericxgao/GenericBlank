#pragma once
#include <rack.hpp>
#include "DSPHelpers.hpp"
#include "ChowDSP.hpp"

using namespace ::rack;

/**
 * Lightweight percussive envelope modeled after Befaco Percall.
 *
 * Usage:
 *   PercEnvelope env;
 *   env.setDecayParam(0.3f);      // knob 0..1
 *   env.setDecayCVVolts(cvV);     // volts (optional)
 *   env.setStrengthVolts(strV);   // volts 0..10V -> sqrt mapping (optional)
 *   env.trigger();
 *   float envOut = env.process(args.sampleTime); // returns [0..1] * strength
 */
// Self-contained AD envelope to avoid external type coupling
struct MiniADEnvelope {
    enum Stage { STAGE_OFF, STAGE_ATTACK, STAGE_DECAY };
    Stage stage = STAGE_OFF;
    float env = 0.f;
    float attackTime = 0.1f, decayTime = 0.1f;
    float attackShape = 1.0f, decayShape = 1.0f;
    float envLinear = 0.f;

    void process(float sampleTime) {
        if (stage == STAGE_OFF) {
            env = envLinear = 0.0f;
        } else if (stage == STAGE_ATTACK) {
            envLinear += sampleTime / attackTime;
            env = std::pow(envLinear, attackShape);
        } else if (stage == STAGE_DECAY) {
            envLinear -= sampleTime / decayTime;
            env = std::pow(envLinear, decayShape);
        }
        if (envLinear >= 1.0f) {
            stage = STAGE_DECAY;
            env = envLinear = 1.0f;
        } else if (envLinear <= 0.0f) {
            stage = STAGE_OFF;
            env = envLinear = 0.0f;
        }
    }
    void trigger() {
        stage = STAGE_ATTACK;
        envLinear = std::pow(env, 1.0f / attackShape);
    }
};

class PercEnvelope {
public:
    PercEnvelope() {
        envelope.attackTime = attackTime;
        envelope.attackShape = attackShape;
        envelope.decayShape = decayShape;
    }

    // Parameters
    void setDecayParam(float normalized01) { decayParam = clamp(normalized01, 0.f, 1.f); }
    void setDecayCVVolts(float volts) { decayCVVolts = volts; }
    void setStrengthVolts(float volts) {
        // Percall scales strength as sqrt(volts/10)
        strength = std::sqrt(clamp(volts / 10.0f, 0.0f, 1.0f));
    }
    void setStrengthNormalized(float normalized01) { strength = clamp(normalized01, 0.f, 1.f); }

    void trigger() { envelope.trigger(); }

    float process(float sampleTime) {
        // Update decay time each call (can be decimated externally if desired)
        float fallCv = decayCVVolts * 0.05f + decayParam; // 5%/V as in Percall
        float fall01 = clamp(fallCv, 0.f, 1.f);
        float shaped = std::pow(fall01, 2.0f);
        envelope.decayTime = rescale(shaped, 0.f, 1.f, minDecayTime, maxDecayTime);

        envelope.process(sampleTime);
        return strength * envelope.env; // [0..1]
    }

    // Direct access if needed
    MiniADEnvelope envelope;

    // Tunings matching Percall
    float attackTime = 1.5e-3f;
    float minDecayTime = 4.5e-3f;
    float maxDecayTime = 4.0f;
    float attackShape = 0.5f;
    float decayShape = 2.0f;

private:
    float decayParam = 0.0f;   // 0..1 knob
    float decayCVVolts = 0.0f; // volts
    float strength = 1.0f;     // 0..1
};

/** Small helper to manage N envelopes with optional choke between pairs. */
template <int Num>
class PercEnvelopeBank {
public:
    PercEnvelopeBank() = default;

    PercEnvelope env[Num];

    void trigger(int idx) {
        if (idx < 0 || idx >= Num) return;
        env[idx].trigger();
    }

    // Simple choke: if odd index is triggered while left neighbor is in attack, stop the odd one
    void applyChokePairs(bool enablePair01, bool enablePair23) {
        if (Num >= 2 && enablePair01 && env[0].envelope.stage == MiniADEnvelope::STAGE_ATTACK)
            env[1].envelope.stage = MiniADEnvelope::STAGE_OFF;
        if (Num >= 4 && enablePair23 && env[2].envelope.stage == MiniADEnvelope::STAGE_ATTACK)
            env[3].envelope.stage = MiniADEnvelope::STAGE_OFF;
    }

    void process(float sampleTime, float* outStrengthScaled) {
        for (int i = 0; i < Num; ++i)
            outStrengthScaled[i] = env[i].process(sampleTime);
    }
};


