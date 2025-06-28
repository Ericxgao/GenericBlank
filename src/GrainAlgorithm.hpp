#pragma once
#include "GrainManager.hpp"
#include "daisysp.h"

// Abstract base class for grain generation algorithms
template<typename T, size_t BufferSize>
class GrainAlgorithm {
public:
    virtual ~GrainAlgorithm() = default;
    virtual void generateGrains(GrainManager<T, BufferSize>& manager, float bufferSize, float jitter = 0.0f) = 0;
};

// Base class for common grain parameters
template<typename T, size_t BufferSize>
class BaseAlgorithm : public GrainAlgorithm<T, BufferSize> {
protected:
    float density;
    float duration;
    float envelopeDuration;
    float speed;
    float delay;
    float pan;

public:
    BaseAlgorithm(
        float dens = 1.0f,
        float dur = 0.1f,
        float envDur = 0.1f,
        float spd = 1.0f,
        float dly = 0.0f,
        float p = 0.0f
    ) : density(dens), duration(dur), envelopeDuration(envDur), speed(spd), delay(dly), pan(p) {}

    void setParameters(float dens, float dur, float envDur, float spd, float dly, float p) {
        density = dens;
        duration = dur;
        envelopeDuration = envDur;
        speed = spd;
        delay = dly;
        pan = p;
    }

    virtual void generateGrains(GrainManager<T, BufferSize>& manager, float bufferSize, float jitter = 0.0f) override {
        if (random::uniform() < density) {
            // Apply jitter to delay timing
            float jitteredDelay = delay;
            if (jitter > 0.0f) {
                // Add random offset to delay timing: ±jitter * 0.1 seconds max
                float jitterOffset = (random::uniform() * 2.0f - 1.0f) * jitter * 0.1f;
                jitteredDelay = delay + jitterOffset;
            }
            
            // Convert delay from seconds to samples for addGrain
            float delaySamples = jitteredDelay * 48000.0f;
            
            // Generate single grain with jittered timing
            manager.addGrain(delaySamples, speed, 1.0f, duration, envelopeDuration, true, pan);
        }
    }
};
