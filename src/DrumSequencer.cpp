#include "plugin.hpp"
#include <cmath>

struct DrumSequencer : Module {
    enum ParamIds {
        ENUMS(STEP_PARAMS, 4 * 16), // 4 rows x 16 steps = 64 buttons
        ENUMS(MARKOV_WEIGHT_PARAMS, 4), // Per-row blend weight for Markov learning
        NUM_PARAMS
    };
    enum InputIds {
        CLOCK_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        ENUMS(GATE_OUTPUTS, 4), // CV1-4 gate outputs
        NUM_OUTPUTS
    };
    enum LightIds {
        // Two channels per step (red, green)
        ENUMS(STEP_LIGHTS, 4 * 16 * 2),
        ENUMS(PLAYHEAD_LIGHTS, 16), // Playhead position lights
        NUM_LIGHTS
    };

    dsp::SchmittTrigger clockTrigger;
    dsp::PulseGenerator gateGenerators[4];
    int currentStep = 0;
    bool stepStates[4][16] = {}; // 4 rows x 16 steps

    // Context-conditioned learning per row (second order) --------------------------------------
    // We condition P(hit) on an 8-bit context mask built from the previous two steps:
    //  - low nibble (bits 0..3): hits at t-1 (bit3=self, bits0..2=other rows)
    //  - high nibble (bits 4..7): hits at t-2 (bit7=self, bits4..6=other rows)
    // This yields 256 contexts per row.
    struct RowContext2Model {
        float hitCounts[256] = {0};
        float missCounts[256] = {0};

        // Exponential decay over all contexts
        void decay(float factor) {
            for (int i = 0; i < 256; i++) {
                hitCounts[i] *= factor;
                missCounts[i] *= factor;
            }
        }

        // Observe a hit/miss under a given 8-bit context
        void observe(uint16_t contextMask, bool hit, float decayFactor) {
            decay(decayFactor);
            uint8_t idx = (uint8_t)(contextMask & 0xFFu);
            if (hit) hitCounts[idx] += 1.f;
            else missCounts[idx] += 1.f;
        }

        // Laplace-smoothed probability of hit given context
        float probability(uint16_t contextMask, float alpha) const {
            uint8_t idx = (uint8_t)(contextMask & 0xFFu);
            float h = hitCounts[idx];
            float m = missCounts[idx];
            float denom = h + m + 2.f * alpha;
            if (denom < 1e-6f) denom = 1e-6f;
            return (h + alpha) / denom;
        }

        // Retrieve raw counts for a specific 2nd-order context (8-bit index)
        void getCounts2(uint16_t contextMask, float& h, float& m) const {
            uint8_t idx = (uint8_t)(contextMask & 0xFFu);
            h = hitCounts[idx];
            m = missCounts[idx];
        }

        // Aggregate counts across the high nibble to form a 1st-order (t-1 only) context
        void getCounts1(uint8_t lowNibble, float& h, float& m) const {
            h = 0.f;
            m = 0.f;
            for (int high = 0; high < 16; high++) {
                int idx = (high << 4) | (int)lowNibble;
                h += hitCounts[idx];
                m += missCounts[idx];
            }
        }

        // Laplace-smoothed probability for 1st-order (low nibble) by aggregating high nibble
        float probability1(uint8_t lowNibble, float alpha) const {
            float h, m;
            getCounts1(lowNibble, h, m);
            float denom = h + m + 2.f * alpha;
            if (denom < 1e-6f) denom = 1e-6f;
            return (h + alpha) / denom;
        }
    };

    RowContext2Model contextModel[4];
    bool lastHit[4] = {false, false, false, false};   // hits at t-1
    bool prevLastHit[4] = {false, false, false, false}; // hits at t-2
    
    DrumSequencer() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        
        // Configure step buttons
        for (int row = 0; row < 4; row++) {
            for (int step = 0; step < 16; step++) {
                int paramId = row * 16 + step;
                configParam(STEP_PARAMS + paramId, 0.f, 1.f, 0.f, 
                    string::f("Row %d Step %d", row + 1, step + 1));
            }
        }
        
        // Configure inputs and outputs
        configInput(CLOCK_INPUT, "Clock");
        for (int i = 0; i < 4; i++) {
            configOutput(GATE_OUTPUTS + i, string::f("CV%d Gate", i + 1));
        }

        // Configure per-row Markov blend weights
        for (int i = 0; i < 4; i++) {
            configParam(MARKOV_WEIGHT_PARAMS + i, 0.f, 1.f, 0.f, string::f("Row %d Markov weight", i + 1));
        }
        
        // Gate generators are initialized automatically
    }

    void process(const ProcessArgs& args) override {
        // Process clock input
        if (inputs[CLOCK_INPUT].isConnected()) {
            if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
                // Advance to next step
                currentStep = (currentStep + 1) % 16;
                
                // Snapshot previous hits for all rows so each row uses the same prior contexts
                bool snapshotT1[4];
                bool snapshotT2[4];
                for (int r = 0; r < 4; r++) { snapshotT1[r] = lastHit[r]; snapshotT2[r] = prevLastHit[r]; }
                bool nextHits[4];

                // Check each row and trigger gate based on blended probability:
                // base knob probability blended with learned P(hit | context)
                for (int row = 0; row < 4; row++) {
                    int paramId = row * 16 + currentStep;
                    const float baseProb = params[STEP_PARAMS + paramId].getValue();

                    // Build 8-bit context mask (t-2 in high nibble, t-1 in low nibble)
                    auto buildNibble = [&](const bool prev[4]) -> uint8_t {
                        uint8_t n = 0;
                        if (prev[row]) n |= 0b1000;
                        int bitIdx = 0;
                        for (int orow = 0; orow < 4; orow++) {
                            if (orow == row) continue;
                            if (prev[orow]) n |= (1u << bitIdx);
                            bitIdx++;
                        }
                        return n;
                    };
                    uint8_t low = buildNibble(snapshotT1);
                    uint8_t high = buildNibble(snapshotT2);
                    uint16_t ctxMask = ((uint16_t)high << 4) | (uint16_t)low;

                    // Anchors: steps with baseProb >= ~1.0 are fixed hits (theme preserved)
                    bool hit = false;
                    const bool isAnchor = baseProb >= 0.999f;
                    if (isAnchor) {
                        hit = true;
                    } else {
                        // Compute context probabilities with confidence-weighted backoff
                        const float alpha = 2.f;        // Laplace smoothing
                        float h2, m2; contextModel[row].getCounts2(ctxMask, h2, m2);
                        const float n2 = h2 + m2;
                        const float p2 = contextModel[row].probability(ctxMask, alpha);

                        uint8_t low = (uint8_t)(ctxMask & 0x0Fu);
                        float h1, m1; contextModel[row].getCounts1(low, h1, m1);
                        const float n1 = h1 + m1;
                        const float p1 = contextModel[row].probability1(low, alpha);

                        // Confidence factors (backoff weights)
                        const float k2 = 16.f; // controls how fast 2nd-order confidence grows
                        const float k1 = 16.f; // controls how fast 1st-order confidence grows
                        const float c2 = n2 / (n2 + k2);
                        const float c1 = n1 / (n1 + k1);

                        const float p12 = c2 * p2 + (1.f - c2) * p1;
                        const float pBackoff = c1 * p12 + (1.f - c1) * baseProb;

                        // Evidence gate: only apply variation when there's signal
                        const float nMin = 10.f;
                        const float eps = 0.1f;
                        float w = clamp(params[MARKOV_WEIGHT_PARAMS + row].getValue(), 0.f, 1.f);
                        if (n2 < nMin || std::fabs(pBackoff - baseProb) < eps) {
                            w = 0.f;
                        }

                        // Logit-space blending to preserve strong features
                        auto clamp01 = [](float x) {
                            if (x < 1e-6f) return 1e-6f;
                            if (x > 1.f - 1e-6f) return 1.f - 1e-6f;
                            return x;
                        };
                        auto logit = [&](float p) { float q = clamp01(p); return std::log(q / (1.f - q)); };
                        auto sigmoid = [&](float z) { return 1.f / (1.f + std::exp(-z)); };

                        const float L = (1.f - w) * logit(baseProb) + w * logit(pBackoff);
                        const float probability = clamp(sigmoid(L), 0.f, 1.f);

                        // Draw
                        hit = random::uniform() < probability;
                    }
                    if (hit) {
                        gateGenerators[row].trigger(0.01f); // 10ms gate duration
                    }

                    // Online learning: update counts under this context (skip anchors)
                    const float decayFactor = 0.995f; // slower decay for more stable learning
                    if (!isAnchor) {
                        contextModel[row].observe(ctxMask, hit, decayFactor);
                    }
                    nextHits[row] = hit;
                }

                // Shift history: t-1 becomes t-2, new hits become t-1
                for (int r = 0; r < 4; r++) { prevLastHit[r] = snapshotT1[r]; lastHit[r] = nextHits[r]; }
            }
        }
        
        // Update gate outputs
        for (int row = 0; row < 4; row++) {
            bool gateHigh = gateGenerators[row].process(args.sampleTime);
            outputs[GATE_OUTPUTS + row].setVoltage(gateHigh ? 10.f : 0.f);
        }
        
        // Update lights
        for (int row = 0; row < 4; row++) {
            for (int step = 0; step < 16; step++) {
                int paramId = row * 16 + step;
                int lightBase = (row * 16 + step) * 2;

                // Red shows probability below full; Green lights when exactly full (>= 0.999)
                float probability = params[STEP_PARAMS + paramId].getValue();
                bool isFull = probability >= 0.999f;
                lights[STEP_LIGHTS + lightBase + 0].setBrightness(isFull ? 0.f : probability); // Red
                lights[STEP_LIGHTS + lightBase + 1].setBrightness(isFull ? 1.f : 0.f);         // Green
            }
        }
        
        // Playhead lights
        for (int step = 0; step < 16; step++) {
            lights[PLAYHEAD_LIGHTS + step].setBrightness(step == currentStep ? 1.f : 0.f);
        }
    }
    
    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        
        // Save step probabilities
        json_t* stepsJ = json_array();
        for (int row = 0; row < 4; row++) {
            json_t* rowJ = json_array();
            for (int step = 0; step < 16; step++) {
                int paramId = row * 16 + step;
                json_array_append_new(rowJ, json_real(params[STEP_PARAMS + paramId].getValue()));
            }
            json_array_append_new(stepsJ, rowJ);
        }
        json_object_set_new(rootJ, "steps", stepsJ);
        
        // Save current step
        json_object_set_new(rootJ, "currentStep", json_integer(currentStep));

        // Save second-order context model per row (counts and history)
        json_t* ctxJ2 = json_array();
        for (int row = 0; row < 4; row++) {
            json_t* rowJ = json_object();
            json_t* hitJ = json_array();
            json_t* missJ = json_array();
            for (int i = 0; i < 256; i++) {
                json_array_append_new(hitJ, json_real(contextModel[row].hitCounts[i]));
                json_array_append_new(missJ, json_real(contextModel[row].missCounts[i]));
            }
            json_object_set_new(rowJ, "hit", hitJ);
            json_object_set_new(rowJ, "miss", missJ);
            json_object_set_new(rowJ, "lastHit", json_boolean(lastHit[row]));
            json_object_set_new(rowJ, "prevLastHit", json_boolean(prevLastHit[row]));
            json_array_append_new(ctxJ2, rowJ);
        }
        json_object_set_new(rootJ, "context2", ctxJ2);
        
        return rootJ;
    }
    
    void dataFromJson(json_t* rootJ) override {
        // Load step probabilities (backward compatible with old boolean format)
        json_t* stepsJ = json_object_get(rootJ, "steps");
        if (stepsJ) {
            for (int row = 0; row < 4; row++) {
                json_t* rowJ = json_array_get(stepsJ, row);
                if (rowJ) {
                    for (int step = 0; step < 16; step++) {
                        json_t* stepJ = json_array_get(rowJ, step);
                        if (stepJ) {
                            int paramId = row * 16 + step;
                            float v = 0.f;
                            if (json_is_number(stepJ)) {
                                v = (float) json_number_value(stepJ);
                            } else {
                                v = json_boolean_value(stepJ) ? 1.f : 0.f;
                            }
                            params[STEP_PARAMS + paramId].setValue(v);
                        }
                    }
                }
            }
        }
        
        // Load current step
        json_t* currentStepJ = json_object_get(rootJ, "currentStep");
        if (currentStepJ) {
            currentStep = json_integer_value(currentStepJ);
        }

        // Load second-order context model only (prototype: no legacy support)
        json_t* ctxJ2 = json_object_get(rootJ, "context2");
        if (ctxJ2) {
            for (int row = 0; row < 4; row++) {
                json_t* rowJ = json_array_get(ctxJ2, row);
                if (rowJ) {
                    json_t* hitJ = json_object_get(rowJ, "hit");
                    json_t* missJ = json_object_get(rowJ, "miss");
                    if (hitJ && missJ) {
                        for (int i = 0; i < 256; i++) {
                            json_t* h = json_array_get(hitJ, i);
                            json_t* m = json_array_get(missJ, i);
                            if (h) contextModel[row].hitCounts[i] = (float) json_number_value(h);
                            if (m) contextModel[row].missCounts[i] = (float) json_number_value(m);
                        }
                    }
                    json_t* lastHitJ = json_object_get(rowJ, "lastHit");
                    if (lastHitJ) lastHit[row] = json_boolean_value(lastHitJ);
                    json_t* prevLastHitJ = json_object_get(rowJ, "prevLastHit");
                    if (prevLastHitJ) prevLastHit[row] = json_boolean_value(prevLastHitJ);
                }
            }
        }
    }
};

struct DrumSequencerWidget : ModuleWidget {
    DrumSequencerWidget(DrumSequencer* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/DrumSequencer.svg")));
        
        // Add screws
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        
        // Clock input (aligned to panel label area)
        addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(19.3f, 25.4f)), module, DrumSequencer::CLOCK_INPUT));
        
        // Step buttons and lights (4 rows x 16 steps)
        float startX = 12.f;   // left margin
        float startY = 35.f;   // first row Y
        float stepX = 12.f;    // horizontal spacing between steps (fits 16 within 40HP)
        float stepY = 16.f;    // vertical spacing between rows (fits within 128.5mm height)
        
        for (int row = 0; row < 4; row++) {
            for (int step = 0; step < 16; step++) {
                int paramId = row * 16 + step;
                int lightId = row * 16 + step;
                
                Vec pos = mm2px(Vec(startX + step * stepX, startY + row * stepY));
                
                // Step control as a small knob (probability)
                addParam(createParamCentered<Trimpot>(pos, module, DrumSequencer::STEP_PARAMS + paramId));
                
                // Step lights overlapped: red channel and green channel
                addChild(createLightCentered<MediumLight<RedLight>>(pos, module, DrumSequencer::STEP_LIGHTS + lightId * 2 + 0));
                addChild(createLightCentered<MediumLight<GreenLight>>(pos, module, DrumSequencer::STEP_LIGHTS + lightId * 2 + 1));
            }
        }
        
        // Playhead lights (above first row, avoid clock input)
        for (int step = 0; step < 16; step++) {
            Vec pos = mm2px(Vec(startX + step * stepX, startY - 6.f));
            addChild(createLightCentered<SmallLight<GreenLight>>(pos, module, DrumSequencer::PLAYHEAD_LIGHTS + step));
        }
        
        // Gate outputs
        float outputY = startY + 4 * stepY + 14.f; // position near lower area, within panel
        // Align outputs roughly with panel labels
        const float outputXs[4] = { 39.f, 77.f, 115.f, 153.f };
        for (int i = 0; i < 4; i++) {
            Vec pos = mm2px(Vec(outputXs[i], outputY));
            addOutput(createOutputCentered<ThemedPJ301MPort>(pos, module, DrumSequencer::GATE_OUTPUTS + i));
        }

        // Per-row Markov blend knobs, placed above the outputs
        float markovY = outputY - 11.f;
        for (int i = 0; i < 4; i++) {
            Vec pos = mm2px(Vec(outputXs[i], markovY));
            addParam(createParamCentered<Trimpot>(pos, module, DrumSequencer::MARKOV_WEIGHT_PARAMS + i));
        }
    }
};

Model* modelDrumSequencer = createModel<DrumSequencer, DrumSequencerWidget>("DrumSequencer"); 