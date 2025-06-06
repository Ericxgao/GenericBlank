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

// Random grain generation algorithm
template<typename T, size_t BufferSize>
class RandomGrainAlgorithm : public GrainAlgorithm<T, BufferSize> {
private:
    float minStartPos;
    float maxStartPos;
    float minSpeed;
    float maxSpeed;
    float minVolume;
    float maxVolume;
    float minDuration;
    float maxDuration;

public:
    RandomGrainAlgorithm(
        float minStart = 0.0f, float maxStart = 12000.0f,
        float minSpd = 0.5f, float maxSpd = 2.5f,
        float minVol = 0.5f, float maxVol = 1.0f,
        float minDur = 0.05f, float maxDur = 0.15f
    ) : minStartPos(minStart), maxStartPos(maxStart),
        minSpeed(minSpd), maxSpeed(maxSpd),
        minVolume(minVol), maxVolume(maxVol),
        minDuration(minDur), maxDuration(maxDur) {}

    void generateGrains(GrainManager<T, BufferSize>& manager, float bufferSize) override {
        float startPos = 48000.0f;
        float speed = random::uniform() * (maxSpeed - minSpeed) + minSpeed;
        float volume = random::uniform() * (maxVolume - minVolume) + minVolume;
        float duration = random::uniform() * (maxDuration - minDuration) + minDuration;

        printf("generating grains with startPos: %f, speed: %f, volume: %f, duration: %f\n", startPos, speed, volume, duration);
        
        manager.addGrain(startPos, speed, volume, duration, 0.1f, true);
    }
};

// Sequential grain generation algorithm
template<typename T, size_t BufferSize>
class SequentialGrainAlgorithm : public GrainAlgorithm<T, BufferSize> {
private:
    float stepSize;
    float currentPos;
    float speed;
    float volume;
    float duration;

public:
    SequentialGrainAlgorithm(
        float step = 100.0f,
        float spd = 1.0f,
        float vol = 0.8f,
        float dur = 0.1f
    ) : stepSize(step), currentPos(0.0f),
        speed(spd), volume(vol), duration(dur) {}

    void generateGrains(GrainManager<T, BufferSize>& manager, float bufferSize) override {
        manager.addGrain(currentPos, speed, volume, duration, 0.1f, true);
        currentPos += stepSize;
        if (currentPos >= bufferSize) {
            currentPos = 0.0f;
        }
    }
};

// Granular cloud algorithm
template<typename T, size_t BufferSize>
class CloudGrainAlgorithm : public GrainAlgorithm<T, BufferSize> {
private:
    float centerPos;
    float spread;
    float density;
    float speed;
    float volume;
    float duration;

public:
    CloudGrainAlgorithm(
        float center = 48000.0f * 1.0f,
        float posSpread = 12000.0f,
        float dens = 1.0f,
        float spd = 1.0f,
        float vol = 0.7f,
        float dur = 4.0f
    ) : centerPos(center), spread(posSpread),
        density(dens), speed(spd),
        volume(vol), duration(dur) {}

    void generateGrains(GrainManager<T, BufferSize>& manager, float bufferSize) override {
        if (random::uniform() < density) {
            float startPos = centerPos + (random::uniform() * 2.0f - 1.0f) * spread;
            startPos = std::max(0.0f, std::min(startPos, bufferSize));
            float randomDuration = duration * (1.0f + (random::uniform() * 2.0f - 1.0f));
            // randomly pick between 0.5, 2.0, and 1.0 for speed of those three options only
            float speedOptions[] = {0.5f, 2.0f, 1.0f};
            int speedIndex = static_cast<int>(random::uniform() * 3);
            speed = speedOptions[speedIndex];
            manager.addGrain(startPos, speed, volume, 0.01f, 2.0f, true);
        }
    }
};