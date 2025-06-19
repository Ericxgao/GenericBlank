#pragma once
#include "Grain.hpp"
#include <vector>

// Helper class to manage a fixed number of grains
template<typename T, size_t BufferSize>
class GrainManager {
private:
    std::vector<Grain<T>> grains;
    std::vector<size_t> grainAges;  // Track age of each grain
    size_t currentAge;              // Global age counter
    size_t maxActiveGrains;         // Maximum number of concurrent grains
    daisysp::DelayLine<T, BufferSize>* mainBufferL;
    daisysp::DelayLine<T, BufferSize>* mainBufferR;
    float sampleRate;
    
public:
    GrainManager(daisysp::DelayLine<T, BufferSize>* bufL, daisysp::DelayLine<T, BufferSize>* bufR, float sr, size_t maxGrains) 
        : mainBufferL(bufL), mainBufferR(bufR), sampleRate(sr), currentAge(0), maxActiveGrains(maxGrains) {
        // Initialize grains vector with maxGrains
        grains.resize(maxGrains);
        grainAges.resize(maxGrains);
        // Initialize all grains
        for (size_t i = 0; i < maxGrains; i++) {
            grains[i].init(sampleRate);
            grainAges[i] = 0;
        }
    }
    
    // Set maximum number of active grains
    void setMaxActiveGrains(size_t maxGrains) {
        maxActiveGrains = std::min(maxGrains, grains.size());
    }
    
    // Get maximum number of active grains
    size_t getMaxActiveGrains() const {
        return maxActiveGrains;
    }
    
    // Find an inactive grain and trigger it, or steal the oldest voice
    bool addGrain(float startPosSamples, float speed, float volume, float duration, float envDur, bool loop, float pan) {
        // First try to find an inactive grain
        for (size_t i = 0; i < maxActiveGrains; i++) {
            if (!grains[i].isActive()) {
                float startPosSeconds = startPosSamples / sampleRate;
                grains[i].trigger(mainBufferL, mainBufferR, startPosSeconds, speed, volume, duration, envDur, loop, pan);
                grainAges[i] = ++currentAge;
                return true;
            }
        }
        
        // If we're at max capacity, steal the oldest grain
        size_t oldestIndex = 0;
        size_t oldestAge = grainAges[0];
        
        for (size_t i = 1; i < grains.size(); i++) {
            if (grains[i].isActive() && grainAges[i] < oldestAge) {
                oldestAge = grainAges[i];
                oldestIndex = i;
            }
        }
        
        // Steal the oldest grain
        float startPosSeconds = startPosSamples / sampleRate;
        grains[oldestIndex].trigger(mainBufferL, mainBufferR, startPosSeconds, speed, volume, duration, envDur, loop, pan);
        grainAges[oldestIndex] = ++currentAge;
        return true;
    }
    
    // Process all grains
    StereoPacket process() {
        StereoPacket output = {0.0f, 0.0f};
        size_t activeCount = 0;
        
        for (size_t i = 0; i < grains.size(); i++) {
            if (grains[i].isActive()) {
                StereoPacket grainOutput = grains[i].process();
                output.left += grainOutput.left;
                output.right += grainOutput.right;
                activeCount++;
            }
        }

        if (activeCount > 0) {
            output.left /= activeCount;
            output.right /= activeCount;
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
            grainAges[i] = 0;
        }
        currentAge = 0;
    }
}; 