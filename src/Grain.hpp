#pragma once
#include "daisysp.h"
#include "GrainEnvelope.hpp"

// Maximum duration of a grain in samples.
// e.g. 48000 samples is 1 second at 48kHz.
// This determines the memory usage of each grain object.
#ifndef MAX_GRAIN_SAMPLES
#define MAX_GRAIN_SAMPLES 48000
#endif

template<typename T>
struct Grain {
    T grainBuffer[MAX_GRAIN_SAMPLES]; // Local buffer for this grain
    
    // Grain parameters
    float readPos;        // Current read position in samples
    float speed;          // Playback speed (1.0 = normal)
    float volume;         // Base volume of the grain
    float sampleRate;     // Sample rate of the buffer
    bool loop;            // Whether the grain should loop
    float durationSamples; // Active duration of the grain in samples

    // Envelope
    GrainEnvelope* envelope; // Envelope generator
    float envelopeDurationSamples;
    
    // State
    bool active;          // Whether the grain is currently playing
    float currentSample;  // How many samples have been processed
    
    Grain() {
        active = false;
        speed = 1.0f;
        volume = 1.0f;
        sampleRate = 48000.0f;
        currentSample = 0.0f;
        durationSamples = 0.0f;
        readPos = 0.0f;
        loop = false;
        envelopeDurationSamples = 0.0f;
        envelope = new ADEnvelope(); // Default to AD envelope
    }
    
    ~Grain() {
        delete envelope;
    }
    
    void init(float sr) {
        sampleRate = sr;
    }
    
    void setEnvelope(GrainEnvelope* newEnvelope) {
        if (envelope) {
            delete envelope;
        }
        envelope = newEnvelope;
    }
    
    template <size_t BufferSize>
    void trigger(daisysp::DelayLine<T, BufferSize>* mainBuffer, float startPosSeconds, float spd, float vol, float durSeconds, float envDurSeconds, bool l) {
        durationSamples = durSeconds * sampleRate;
        if (durationSamples > MAX_GRAIN_SAMPLES) {
            durationSamples = MAX_GRAIN_SAMPLES;
            // Optional: Log a warning about truncation
        }
        envelopeDurationSamples = envDurSeconds * sampleRate;

        float startPosSamples = startPosSeconds * sampleRate;
        for (size_t i = 0; i < durationSamples; ++i) {
            grainBuffer[i] = mainBuffer->Read(startPosSamples + i);
        }
        
        speed = spd;
        volume = vol;
        loop = l;
        
        currentSample = 0.0f;
        readPos = 0.0f;
        active = true;
    }
    
    float process() {
        if (!active) return 0.0f;
        
        float envelopeValue = envelope->process(currentSample, envelopeDurationSamples);
        
        // Linear interpolation for playback speed
        int idx1 = (int)readPos;
        float frac = readPos - idx1;
        T s1 = grainBuffer[idx1];
        T s2 = grainBuffer[(idx1 + 1) % (int)durationSamples];
        float output = (s1 + (s2 - s1) * frac);

        output *= envelopeValue * volume;
        
        readPos += speed;

        if (loop) {
            if (readPos >= durationSamples) {
                readPos -= durationSamples;
            } else if(readPos < 0.0f) {
                readPos += durationSamples;
            }
        } else {
            if (readPos >= durationSamples) {
                active = false;
            }
        }

        currentSample += 1.0f;
        if(currentSample >= envelopeDurationSamples) {
            active = false;
        }
        
        return output;
    }
    
    bool isActive() const {
        return active;
    }
}; 