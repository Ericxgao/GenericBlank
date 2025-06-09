#pragma once
#include <cmath>

// Base envelope class
class GrainEnvelope {
public:
    virtual float process(float counter, float duration) = 0;
    virtual ~GrainEnvelope() = default;
};

// Simple attack/decay envelope
class ADEnvelope : public GrainEnvelope {
public:
    float process(float counter, float duration) override {
        float attackDuration = duration * 0.05f; // 10% attack
        if (counter < attackDuration) {
            return counter / attackDuration;
        } else {
            float decayCounter = counter - attackDuration;
            float decayDuration = duration - attackDuration;
            return 1.0f - (decayCounter / decayDuration);
        }
    }
};

// Hann window envelope
class HannEnvelope : public GrainEnvelope {
public:
    float process(float counter, float duration) override {
        float phase = counter / duration;
        return 0.5f * (1.0f - cosf(2.0f * M_PI * phase));
    }
};

// Square envelope (no smoothing)
class SquareEnvelope : public GrainEnvelope {
public:
    float process(float counter, float duration) override {
        return 1.0f;
    }
};

// Reverse envelope (decay then attack)
class ReverseEnvelope : public GrainEnvelope {
public:
    float process(float counter, float duration) override {
        float attackDuration = duration * 0.1f; // 10% attack
        if (counter < (duration - attackDuration)) {
            float decayCounter = counter;
            float decayDuration = duration - attackDuration;
            return 1.0f - (decayCounter / decayDuration);
        } else {
            float attackCounter = counter - (duration - attackDuration);
            return attackCounter / attackDuration;
        }
    }
}; 