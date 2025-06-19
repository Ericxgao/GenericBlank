#pragma once
#include "daisysp.h"
#include "GrainEnvelope.hpp"
#include <cmath>
#include <utility> // For std::pair

// Helper struct for stereo samples
struct StereoPacket {
    float left;
    float right;
};

// Maximum duration of a grain in samples.
// e.g. 48000 samples is 1 second at 48kHz.
// This determines the memory usage of each grain object.
#ifndef MAX_GRAIN_SAMPLES
#define MAX_GRAIN_SAMPLES 48000
#endif

template<typename T>
struct Grain {
    T grainBufferL[MAX_GRAIN_SAMPLES]; // Local buffer for this grain's left channel
    T grainBufferR[MAX_GRAIN_SAMPLES]; // Local buffer for this grain's right channel
    
    // Grain parameters
    float readPos;        // Current read position in samples
    float speed;          // Playback speed (1.0 = normal)
    float volume;         // Base volume of the grain
    float sampleRate;     // Sample rate of the buffer
    bool loop;            // Whether the grain should loop
    float durationSamples; // Active duration of the grain in samples
    bool glide;           // Whether to apply glide effect
    float pan;            // Stereo panning (-1 to 1)

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
        glide = false;
        pan = 0.0f; // Center panned by default
    }
    
    ~Grain() {
        delete envelope;
    }
    
    void init(float sr) {
        sampleRate = sr;
    }
    
    void setGlide(bool enableGlide) {
        glide = enableGlide;
    }

    void setPan(float p) {
        pan = p;
    }
    
    void setEnvelope(GrainEnvelope* newEnvelope) {
        if (envelope) {
            delete envelope;
        }
        envelope = newEnvelope;
    }
    
    template <size_t BufferSize>
    void trigger(daisysp::DelayLine<T, BufferSize>* mainBufferL, daisysp::DelayLine<T, BufferSize>* mainBufferR, float startPosSeconds, float spd, float vol, float durSeconds, float envDurSeconds, bool l, float p) {
        durationSamples = durSeconds * sampleRate;
        if (durationSamples > MAX_GRAIN_SAMPLES) {
            durationSamples = MAX_GRAIN_SAMPLES;
            // Optional: Log a warning about truncation
        }
        envelopeDurationSamples = envDurSeconds * sampleRate;
        pan = p;

        float startPosSamples = startPosSeconds * sampleRate;
        for (size_t i = 0; i < durationSamples; ++i) {
            grainBufferL[i] = mainBufferL->Read(startPosSamples + i);
            grainBufferR[i] = mainBufferR->Read(startPosSamples + i);
        }
        
        speed = spd;
        volume = vol;
        loop = l;
        
        currentSample = 0.0f;
        readPos = 0.0f;
        active = true;
    }
    
    StereoPacket process() {
        if (!active) return {0.0f, 0.0f};
        
        float envelopeValue = envelope->process(currentSample, envelopeDurationSamples);
        
        int dur = static_cast<int>(durationSamples);
        if (dur <= 0) {
            active = false;
            return {0.0f, 0.0f};
        }

        StereoPacket output = interpolate(dur);

        output.left *= envelopeValue * volume;
        output.right *= envelopeValue * volume;

        // Apply panning
        float panL = sqrtf(0.5f * (1.0f - pan));
        float panR = sqrtf(0.5f * (1.0f + pan));
        output.left *= panL;
        output.right *= panR;

        float finalSpeed = speed;
        if (glide && envelopeDurationSamples > 0) {
            float phase = currentSample / envelopeDurationSamples;
            // Triangle LFO from 0 to 1 to 0
            float tri = 2.0f * (0.5f - fabsf(fmodf(phase, 1.0f) - 0.5f));
            // Speed goes from 1.0 to 2.0 and back
            finalSpeed = 1.0f + tri;
        }

        readPos += finalSpeed;

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
    StereoPacket interpolate(int dur) const {
        // 4-point, 3rd-order Hermite interpolation for playback speed
        int idx1 = static_cast<int>(readPos);
        float frac = readPos - idx1;

        T y0L, y1L, y2L, y3L;
        T y0R, y1R, y2R, y3R;

        if (loop) {
            auto get_index = [&](int i) {
                return ((i % dur) + dur) % dur;
            };
            y0L = grainBufferL[get_index(idx1 - 1)];
            y1L = grainBufferL[get_index(idx1)];
            y2L = grainBufferL[get_index(idx1 + 1)];
            y3L = grainBufferL[get_index(idx1 + 2)];

            y0R = grainBufferR[get_index(idx1 - 1)];
            y1R = grainBufferR[get_index(idx1)];
            y2R = grainBufferR[get_index(idx1 + 1)];
            y3R = grainBufferR[get_index(idx1 + 2)];
        } else {
            // Clamp indices to the valid range [0, dur - 1]
            auto clamp = [&](int i) {
                if (i < 0) return 0;
                if (i >= dur) return dur - 1;
                return i;
            };
            y0L = grainBufferL[clamp(idx1 - 1)];
            y1L = grainBufferL[clamp(idx1)];
            y2L = grainBufferL[clamp(idx1 + 1)];
            y3L = grainBufferL[clamp(idx1 + 2)];

            y0R = grainBufferR[clamp(idx1 - 1)];
            y1R = grainBufferR[clamp(idx1)];
            y2R = grainBufferR[clamp(idx1 + 1)];
            y3R = grainBufferR[clamp(idx1 + 2)];
        }

        auto hermite = [&](T y0, T y1, T y2, T y3) {
            float c0 = y1;
            float c1 = 0.5f * (y2 - y0);
            float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            float c3 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
            return ((c3 * frac + c2) * frac + c1) * frac + c0;
        };

        return {hermite(y0L, y1L, y2L, y3L), hermite(y0R, y1R, y2R, y3R)};
    }
}; 