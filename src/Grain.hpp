#pragma once
#include "daisysp.h"
#include "GrainEnvelope.hpp"

struct Grain {
    // Buffer reference
    daisysp::DelayLine<float, 96000>* buffer;
    
    // Grain parameters
    float readPos;        // Current read position in samples
    float speed;          // Playback speed (1.0 = normal, 2.0 = double speed, etc.)
    float volume;         // Base volume of the grain
    float sampleRate;     // Sample rate of the buffer

    // Envelope
    float envelopeValue;  // Current envelope value
    float envelopeDuration; // Duration in samples
    float envelopeCounter;  // Current position in envelope
    GrainEnvelope* envelope; // Envelope type
    
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
        envelope = new ADEnvelope(); // Default to AD envelope
    }
    
    ~Grain() {
        delete envelope;
    }
    
    void init(daisysp::DelayLine<float, 96000>* buf, float sr) {
        buffer = buf;
        sampleRate = sr;
    }
    
    void setEnvelope(GrainEnvelope* newEnvelope) {
        delete envelope;
        envelope = newEnvelope;
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
        
        // Process envelope using the selected envelope type
        envelopeValue = envelope->process(envelopeCounter, envelopeDuration);
        envelopeCounter += 1.0f;
        
        // Read from buffer at current position
        float output = buffer->Read(readPos) * envelopeValue * volume;
        
        // Update read position
        readPos -= (speed - 1.0f);
        
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