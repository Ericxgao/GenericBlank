#pragma once
#include "GrainManager.hpp"
#include "daisysp.h"

// Abstract base class for grain generation algorithms
class GrainAlgorithm {
public:
    virtual ~GrainAlgorithm() = default;
    virtual void generateGrains(GrainManager& manager, float bufferSize) = 0;
};

// Random grain generation algorithm
class RandomGrainAlgorithm : public GrainAlgorithm {
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

    void generateGrains(GrainManager& manager, float bufferSize) override {
        float startPos = 48000.0f;
        float speed = random::uniform() * (maxSpeed - minSpeed) + minSpeed;
        float volume = random::uniform() * (maxVolume - minVolume) + minVolume;
        float duration = random::uniform() * (maxDuration - minDuration) + minDuration;

        printf("generating grains with startPos: %f, speed: %f, volume: %f, duration: %f\n", startPos, speed, volume, duration);
        
        manager.addGrain(startPos, speed, volume, duration);
    }
};

// Sequential grain generation algorithm
class SequentialGrainAlgorithm : public GrainAlgorithm {
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

    void generateGrains(GrainManager& manager, float bufferSize) override {
        manager.addGrain(currentPos, speed, volume, duration);
        currentPos += stepSize;
        if (currentPos >= bufferSize) {
            currentPos = 0.0f;
        }
    }
};

// Granular cloud algorithm
class CloudGrainAlgorithm : public GrainAlgorithm {
private:
    float centerPos;
    float spread;
    float density;
    float speed;
    float volume;
    float duration;

public:
    CloudGrainAlgorithm(
        float center = 6000.0f,
        float posSpread = 1000.0f,
        float dens = 0.5f,
        float spd = 1.0f,
        float vol = 0.7f,
        float dur = 0.08f
    ) : centerPos(center), spread(posSpread),
        density(dens), speed(spd),
        volume(vol), duration(dur) {}

    void generateGrains(GrainManager& manager, float bufferSize) override {
        if (random::uniform() < density) {
            float startPos = centerPos + (random::uniform() * 2.0f - 1.0f) * spread;
            startPos = std::max(0.0f, std::min(startPos, bufferSize));
            manager.addGrain(startPos, speed, volume, duration);
        }
    }
}; 