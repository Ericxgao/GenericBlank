#pragma once
#include "Grain.hpp"
#include <vector>

// Helper class to manage a fixed number of grains
class GrainManager {
private:
    std::vector<Grain> grains;
    float sampleRate;
    
public:
    GrainManager(daisysp::DelayLine<float, 96000>* buf, float sr, size_t maxGrains) 
        : sampleRate(sr) {
        // Initialize grains vector with maxGrains
        grains.resize(maxGrains);
        // Initialize all grains
        for (size_t i = 0; i < maxGrains; i++) {
            grains[i].init(buf, sampleRate);
        }
    }
    
    // Find an inactive grain and trigger it
    bool addGrain(float startPos, float speed, float volume, float duration) {
        // Find first inactive grain
        for (size_t i = 0; i < grains.size(); i++) {
            if (!grains[i].isActive()) {
                grains[i].trigger(startPos, speed, volume, duration);
                return true;
            }
        }
        return false; // No free grains available
    }
    
    // Process all grains
    float process() {
        float output = 0.0f;
        for (size_t i = 0; i < grains.size(); i++) {
            output += grains[i].process();
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