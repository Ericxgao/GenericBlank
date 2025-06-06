#pragma once
#include "daisysp.h"

template<typename T, size_t size>
struct Grain {
    // Buffer reference
    daisysp::DelayLine<T, size>* buffer;
    
    // Grain parameters
    float readPos;        // Current read position in samples
    float speed;          // Playback speed (1.0 = normal, 2.0 = double speed, etc.)
    float volume;         // Base volume of the grain
    float sampleRate;     // Sample rate of the buffer

    // Envelope
    float envelopeValue;  // Current envelope value
    float envelopeDuration; // Duration in samples
    float envelopeCounter;  // Current position in envelope
    
    // State
    bool active;          // Whether the grain is currently playing
    float duration;       // Duration of the grain in seconds
    
    Grain() {
        buffer = nullptr;
        readPos = 12000.0f;
        speed = 1.0f;
        volume = 1.0f;
        sampleRate = 48000.0f;
        envelopeValue = 0.0f;
        envelopeDuration = 0.0f;
        envelopeCounter = 0.0f;
        active = false;
        duration = 0.1f;  // Default 100ms duration
    }
    
    void init(daisysp::DelayLine<T, size>* buf, float sr) {
        buffer = buf;
        sampleRate = sr;
    }
    
    void trigger(float startPos, float spd, float vol, float dur) {
        readPos = startPos;
        speed = spd;
        volume = vol;
        duration = dur;
        
        // Initialize envelope
        envelopeDuration = duration * sampleRate; // Convert seconds to samples
        envelopeCounter = 0.0f;
        envelopeValue = 1.0f;
        active = true;
    }
    
    float process() {
        if (!active || !buffer) return 0.0f;
        
        // Process envelope - simple ramp down
        envelopeValue = 1.0f - (envelopeCounter / envelopeDuration);
        envelopeCounter += 1.0f;
        
        // Read from buffer at current position
        float output = buffer->Read(readPos) * envelopeValue * volume;
        
        // Update read position
        readPos += (speed - 1.0f);
        
        // Check if grain is finished
        if (envelopeCounter >= envelopeDuration) {
            active = false;
        }
        
        return output;
    }
    
    bool isActive() const {
        return active;
    }
};

// Helper class to manage a fixed number of grains
template<typename T, size_t size, size_t maxGrains>
class GrainManager {
private:
    Grain<T, size> grains[maxGrains];
    daisysp::DelayLine<T, size>* buffer;
    float sampleRate;
    
public:
    GrainManager(daisysp::DelayLine<T, size>* buf, float sr) 
        : buffer(buf), sampleRate(sr) {
        // Initialize all grains
        for (size_t i = 0; i < maxGrains; i++) {
            grains[i].init(buffer, sampleRate);
        }
    }
    
    // Find an inactive grain and trigger it
    bool addGrain(float startPos, float speed, float volume, float duration) {
        // Find first inactive grain
        for (size_t i = 0; i < maxGrains; i++) {
            if (!grains[i].isActive()) {
                printf("Adding grain %d\n", i);
                grains[i].trigger(startPos, speed, volume, duration);
                return true;
            }
        }
        return false; // No free grains available
    }
    
    // Process all grains
    float process() {
        float output = 0.0f;
        for (size_t i = 0; i < maxGrains; i++) {
            output += grains[i].process();
        }
        return output;
    }
    
    // Get number of active grains
    size_t getActiveGrainCount() const {
        size_t count = 0;
        for (size_t i = 0; i < maxGrains; i++) {
            if (grains[i].isActive()) count++;
        }
        return count;
    }
    
    // Clear all grains
    void clear() {
        for (size_t i = 0; i < maxGrains; i++) {
            grains[i].active = false;
        }
    }
}; 