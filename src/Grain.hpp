#pragma once
#include "daisysp.h"
#include "GrainEnvelope.hpp"

template<typename T, size_t BufferSize>
struct Grain {
    // Buffer reference
    daisysp::DelayLine<T, BufferSize>* buffer;
    
    // Grain parameters
    float readPos;        // Current read position in samples
    float speed;          // Playback speed (1.0 = normal, 2.0 = double speed, etc.)
    float volume;         // Base volume of the grain
    float sampleRate;     // Sample rate of the buffer
    bool loop;           // Whether the grain should loop
    float startPos;      // Original start position for looping
    float duration;       // Duration of the grain in seconds

    // Envelope
    float envelopeValue;  // Current envelope value
    float envelopeDuration;  // Current position in envelope
    GrainEnvelope* envelope; // Envelope type
    
    // State
    bool active;          // Whether the grain is currently playing
    int durationCounter; // Current position in duration
    
    Grain() {
        buffer = nullptr;
        readPos = 12000.0f;
        speed = 1.0f;
        volume = 1.0f;
        sampleRate = 48000.0f;
        envelopeValue = 0.0f;
        envelopeDuration = 0.0f;
        durationCounter = 0.0f;
        active = false;
        duration = 0.1f;  // Default 100ms duration
        envelope = new ADEnvelope(); // Default to AD envelope
    }
    
    ~Grain() {
        delete envelope;
    }
    
    void init(daisysp::DelayLine<T, BufferSize>* buf, float sr) {
        buffer = buf;
        sampleRate = sr;
    }
    
    void setEnvelope(GrainEnvelope* newEnvelope) {
        delete envelope;
        envelope = newEnvelope;
    }
    
    void trigger(float startPos, float spd, float vol, float dur, float envDur, bool loop) {
        this->startPos = startPos;
        readPos = startPos;
        speed = spd;
        volume = vol;
        duration = dur;
        this->loop = loop;
        printf("triggering with startPos: %f, speed: %f, volume: %f, duration: %f, envDur: %f, loop: %d\n", startPos, speed, volume, duration, envDur, (int)loop);
        
        // Initialize envelope
        envelopeDuration = envDur * sampleRate; // Convert seconds to samples
        durationCounter = 0;
        envelopeValue = 1.0f;
        active = true;
    }
    
    float process() {
        if (!active || !buffer) return 0.0f;
        
        // Process envelope using the selected envelope type
        envelopeValue = envelope->process(durationCounter, envelopeDuration);
        durationCounter += 1;
        
        // Read from buffer at current position
        float output = buffer->ReadHermite(readPos) * envelopeValue * volume;
        
        // Update read position
        readPos -= (speed - 1.0f);

        if (durationCounter % 480 == 0) {
            printf("loop: %d\n", (int)loop);
            printf("durationCounter mod: %d\n", durationCounter % 480);
            printf("readPos: %f, durationCounter: %d, (int)(duration * sampleRate): %d\n", readPos, durationCounter, (int)(duration * sampleRate));
            printf("loop condition: %d && %d\n", (int)loop, durationCounter % (int)(duration * sampleRate) == 0);
        }

        // Check if grain needs to loop
        if (loop && durationCounter % (int)(duration * sampleRate) == 0) {
            printf("looping\n");
            int loopCount = durationCounter / (int)(duration * sampleRate);
            readPos = startPos + (loopCount * duration * sampleRate);
        }
        
        // Check if grain is finished
        if (durationCounter >= envelopeDuration) {
            active = false;
        }
        
        return output;
    }
    
    bool isActive() const {
        return active;
    }
}; 