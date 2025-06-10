#pragma once
#include "daisysp.h"
#include "GrainEnvelope.hpp"

template<typename T, size_t BufferSize>
struct Grain {
    // Buffer reference
    daisysp::DelayLine<T, BufferSize>* buffer;
    
    // Grain parameters
    float readPosSamples;        // Current read position in samples
    float speed;          // Playback speed (1.0 = normal, 2.0 = double speed, etc.)
    float volume;         // Base volume of the grain
    float sampleRate;     // Sample rate of the buffer
    bool loop;           // Whether the grain should loop
    float startPosSamples;      // Original start position in samples
    float durationSamples;       // Duration of the grain in samples

    // Envelope
    float envelopeValue;  // Current envelope value
    float envelopeDurationSamples;  // Current position in envelope in samples
    GrainEnvelope* envelope; // Envelope type
    
    // State
    bool active;          // Whether the grain is currently playing
    int durationCounter; // Current position in duration in samples
    
    Grain() {
        buffer = nullptr;
        readPosSamples = 12000.0f;
        speed = 1.0f;
        volume = 1.0f;
        sampleRate = 48000.0f;
        envelopeValue = 0.0f;
        envelopeDurationSamples = 0.0f;
        durationCounter = 0.0f;
        active = false;
        durationSamples = 4800.0f;  // Default 100ms duration at 48kHz
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
    
    void trigger(float startPosSeconds, float spd, float vol, float durSeconds, float envDurSeconds, bool loop) {
        // Convert all time-based parameters from seconds to samples
        this->startPosSamples = startPosSeconds * sampleRate;
        readPosSamples = startPosSeconds * sampleRate;
        speed = spd;
        volume = vol;
        durationSamples = durSeconds * sampleRate;
        this->loop = loop;
        printf("triggering with startPos: %f, speed: %f, volume: %f, duration: %f, envDur: %f, loop: %d\n", 
               startPosSeconds, speed, volume, durSeconds, envDurSeconds, (int)loop);
        
        // Initialize envelope
        envelopeDurationSamples = envDurSeconds * sampleRate; // Convert seconds to samples
        durationCounter = 0;
        envelopeValue = 1.0f;
        active = true;
    }
    
    float process() {
        if (!active || !buffer) return 0.0f;
        
        // Process envelope using the selected envelope type
        envelopeValue = envelope->process(durationCounter, envelopeDurationSamples);
        durationCounter += 1;
        
        // Read from buffer at current position
        float output = buffer->ReadHermite(readPosSamples) * envelopeValue * volume;
        
        // Update read position
        readPosSamples -= (speed - 1.0f);

        // Check if grain needs to loop
        if (loop && durationCounter % (int)durationSamples == 0) {
            printf("looping\n");
            int loopCount = durationCounter / (int)durationSamples;
            readPosSamples = startPosSamples + (loopCount * durationSamples);
        }
        
        // Check if grain is finished
        if (durationCounter >= envelopeDurationSamples) {
            active = false;
        }
        
        return output;
    }
    
    bool isActive() const {
        return active;
    }
}; 