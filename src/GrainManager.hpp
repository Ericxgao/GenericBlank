#pragma once
#include "Grain.hpp"
#include <vector>

// Helper class to manage a fixed number of grains
template<typename T, size_t BufferSize>
class GrainManager {
private:
    std::vector<Grain<T>> grains;
    daisysp::DelayLine<T, BufferSize>* mainBuffer;
    float sampleRate;
    
public:
    GrainManager(daisysp::DelayLine<T, BufferSize>* buf, float sr, size_t maxGrains) 
        : mainBuffer(buf), sampleRate(sr) {
        // Initialize grains vector with maxGrains
        grains.resize(maxGrains);
        // Initialize all grains
        for (size_t i = 0; i < maxGrains; i++) {
            grains[i].init(sampleRate);
        }
    }
    
    // Find an inactive grain and trigger it
    bool addGrain(float startPosSamples, float speed, float volume, float duration, float envDur, bool loop) {
        // Find first inactive grain
        for (size_t i = 0; i < grains.size(); i++) {
            if (!grains[i].isActive()) {
                float startPosSeconds = startPosSamples / sampleRate;
                grains[i].trigger(mainBuffer, startPosSeconds, speed, volume, duration, envDur, loop);
                return true;
            }
        }
        return false; // No free grains available
    }
    
    // Process all grains
    float process() {
        float output = 0.0f;
        for (size_t i = 0; i < grains.size(); i++) {
            output += grains[i].process() / grains.size();
        }
        return output;
    }
    
    // Get number of active grains
    size_t getActiveGrainCount() const {
        size_t count = 0;
        for (size_t i = 0; i < grains.size(); i++) {
            if (grains[i].isActive()) count++;
        }
        return count;
    }
    
    // Clear all grains
    void clear() {
        for (size_t i = 0; i < grains.size(); i++) {
            grains[i].active = false;
        }
    }
}; 