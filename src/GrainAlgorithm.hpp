#pragma once
#include "GrainManager.hpp"
#include "daisysp.h"

// Abstract base class for grain generation algorithms
template<typename T, size_t BufferSize>
class GrainAlgorithm {
public:
    virtual ~GrainAlgorithm() = default;
    virtual void generateGrains(GrainManager<T, BufferSize>& manager, float bufferSize) = 0;
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

public:
    BaseAlgorithm(
        float dens = 1.0f,
        float dur = 0.1f,
        float envDur = 0.1f,
        float spd = 1.0f,
        float dly = 0.0f
    ) : density(dens), duration(dur), envelopeDuration(envDur), speed(spd), delay(dly) {}

    void setParameters(float dens, float dur, float envDur, float spd, float dly) {
        density = dens;
        duration = dur;
        envelopeDuration = envDur;
        speed = spd;
        delay = dly;
    }

    virtual void generateGrains(GrainManager<T, BufferSize>& manager, float bufferSize) override {
        if (random::uniform() < density) {
            manager.addGrain(delay, speed, 1.0f, duration, envelopeDuration, true);
        }
    }
};

// Random grain generation algorithm
template<typename T, size_t BufferSize>
class RandomGrainAlgorithm : public BaseAlgorithm<T, BufferSize> {
private:
    float minStartPos;
    float maxStartPos;
    float minVolume;
    float maxVolume;

public:
    RandomGrainAlgorithm(
        float minStart = 0.0f, float maxStart = 12000.0f,
        float minVol = 0.5f, float maxVol = 1.0f,
        float dens = 1.0f, float dur = 0.1f,
        float envDur = 0.1f, float spd = 1.0f,
        float dly = 0.0f
    ) : BaseAlgorithm<T, BufferSize>(dens, dur, envDur, spd, dly),
        minStartPos(minStart), maxStartPos(maxStart),
        minVolume(minVol), maxVolume(maxVol) {}

    void generateGrains(GrainManager<T, BufferSize>& manager, float bufferSize) override {
        if (random::uniform() < this->density) {
            float startPos = this->delay * 48000.0f + random::uniform() * (maxStartPos - minStartPos) + minStartPos;
            float volume = random::uniform() * (maxVolume - minVolume) + minVolume;
            manager.addGrain(startPos, this->speed, volume, this->duration, this->envelopeDuration, true);
        }
    }
};

// Sequential grain generation algorithm
template<typename T, size_t BufferSize>
class SequentialGrainAlgorithm : public BaseAlgorithm<T, BufferSize> {
private:
    float stepSize;
    float currentPos;

public:
    SequentialGrainAlgorithm(
        float step = 100.0f,
        float dens = 1.0f, float dur = 0.1f,
        float envDur = 0.1f, float spd = 1.0f,
        float dly = 0.0f
    ) : BaseAlgorithm<T, BufferSize>(dens, dur, envDur, spd, dly),
        stepSize(step), currentPos(0.0f) {}

    void generateGrains(GrainManager<T, BufferSize>& manager, float bufferSize) override {
        if (random::uniform() < this->density) {
            float startPos = this->delay * 48000.0f + currentPos;
            manager.addGrain(startPos, this->speed, 1.0f, this->duration, this->envelopeDuration, true);
            currentPos += stepSize;
            if (currentPos >= bufferSize) {
                currentPos = 0.0f;
            }
        }
    }
};

// Granular cloud algorithm
template<typename T, size_t BufferSize>
class CloudGrainAlgorithm : public BaseAlgorithm<T, BufferSize> {
private:
    float centerPos;
    float spread;

public:
    CloudGrainAlgorithm(
        float center = 48000.0f * 1.0f,
        float posSpread = 12000.0f,
        float dens = 1.0f, float dur = 0.1f,
        float envDur = 0.1f, float spd = 1.0f,
        float dly = 0.0f
    ) : BaseAlgorithm<T, BufferSize>(dens, dur, envDur, spd, dly),
        centerPos(center), spread(posSpread) {}

    void generateGrains(GrainManager<T, BufferSize>& manager, float bufferSize) override {
        if (random::uniform() < this->density) {
            float startPos = this->delay * 48000.0f + centerPos + (random::uniform() * 2.0f - 1.0f) * spread;
            startPos = std::max(0.0f, std::min(startPos, bufferSize));
            float speedOptions[] = {0.5f, 2.0f, 1.0f};
            int speedIndex = static_cast<int>(random::uniform() * 3);
            float finalSpeed = this->speed * speedOptions[speedIndex];
            manager.addGrain(startPos, finalSpeed, 1.0f, this->duration, this->envelopeDuration, true);
        }
    }
};