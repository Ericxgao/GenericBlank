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
        
        int dur = static_cast<int>(durationSamples);
        if (dur <= 0) {
            active = false;
            return 0.0f;
        }

        float output = interpolate(dur);

        output *= envelopeValue * volume;
        
        readPos += speed;

        if (loop) {
            while (readPos >= durationSamples) {
                readPos -= durationSamples;
            }
            while (readPos < 0.0f) {
                readPos += durationSamples;
            }
        } else {
            if (readPos >= durationSamples || readPos < 0.0f) {
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

private:
    float interpolate(int dur) const {
        // 4-point, 3rd-order Hermite interpolation for playback speed
        int idx1 = static_cast<int>(readPos);
        float frac = readPos - idx1;

        T y0, y1, y2, y3;

        if (loop) {
            auto get_index = [&](int i) {
                return ((i % dur) + dur) % dur;
            };
            y0 = grainBuffer[get_index(idx1 - 1)];
            y1 = grainBuffer[get_index(idx1)];
            y2 = grainBuffer[get_index(idx1 + 1)];
            y3 = grainBuffer[get_index(idx1 + 2)];
        } else {
            // Clamp indices to the valid range [0, dur - 1]
            auto clamp = [&](int i) {
                if (i < 0) return 0;
                if (i >= dur) return dur - 1;
                return i;
            };
            y0 = grainBuffer[clamp(idx1 - 1)];
            y1 = grainBuffer[clamp(idx1)];
            y2 = grainBuffer[clamp(idx1 + 1)];
            y3 = grainBuffer[clamp(idx1 + 2)];
        }

        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }
}; 