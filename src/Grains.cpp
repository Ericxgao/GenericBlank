#include "plugin.hpp"
#include "daisysp.h"
#include "Grain.hpp"
#include "GrainManager.hpp"

struct GrainsModule : Module
{
    enum Params {
        NUM_PARAMS
    };
    enum Inputs {
        CLOCK_INPUT,
        AUDIO_INPUT,
        NUM_INPUTS
    };
    enum Outputs {
        AUDIO_OUTPUT,
        NUM_OUTPUTS
    };
    enum Lights {
        NUM_LIGHTS
    };

    static constexpr size_t delayTimeSamples = 96000;
    size_t sampleRate = 48000.f; 
    daisysp::DelayLine<float, delayTimeSamples> delayBuffer;
    
    // Grain manager with 8 grains max
    static constexpr size_t MAX_GRAINS = 8;
    GrainManager grainManager;

    // Add Schmitt trigger for clock input
    dsp::SchmittTrigger clockTrigger;

    GrainsModule()
        : grainManager(&delayBuffer, sampleRate, MAX_GRAINS)
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        
        configInput(CLOCK_INPUT, "Clock");
        configInput(AUDIO_INPUT, "Audio Input");
        configOutput(AUDIO_OUTPUT, "Audio Output");
        
        // Initialize the delay buffer
        delayBuffer.Init();
    }

    void process(const ProcessArgs& args) override
    {
        // Update sample rate if it changed
        if (args.sampleRate != sampleRate) {
            sampleRate = args.sampleRate;
        }
        
        // Get audio input and write to buffer
        float audioInput = inputs[AUDIO_INPUT].getVoltage();
        delayBuffer.Write(audioInput);
        
        // Check for clock trigger to start new grain
        if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
            // Randomize grain parameters
            float startPos = random::uniform() * delayTimeSamples;  // Random start position
            float speed = 6.0f;         // Speed between 0.5x and 2.5x
            float volume = random::uniform() * 0.5f + 0.5f;        // Volume between 0.5 and 1.0
            float duration = random::uniform() * 0.1f + 0.2f;     // Duration between 50ms and 150ms
            
            // Try to add new grain
            grainManager.addGrain(startPos, speed, volume, duration);
        }
        
        // Process all grains and get output
        float output = grainManager.process();
        
        // Output the processed signal
        outputs[AUDIO_OUTPUT].setVoltage(output);
    }
};

struct GrainsModuleWidget : ModuleWidget
{
    struct CustomPanel : Widget {
        void draw(const DrawArgs& args) override {
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.0, 0.0, box.size.x, box.size.y);
            nvgFillColor(args.vg, nvgRGB(255, 255, 255));
            nvgFill(args.vg);
            Widget::draw(args);
        }
    };

    GrainsModuleWidget(GrainsModule *module)
    {
        setModule(module);
        box.size = Vec(6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        
        // Create a custom white panel
        CustomPanel* panel = new CustomPanel();
        panel->box.size = box.size;
        addChild(panel);

        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Inputs
        addInput(createInputCentered<PJ301MPort>(Vec(30, 160), module, GrainsModule::CLOCK_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(60, 160), module, GrainsModule::AUDIO_INPUT));

        // Output
        addOutput(createOutputCentered<PJ301MPort>(Vec(45, 200), module, GrainsModule::AUDIO_OUTPUT));
    }
};

// Model declaration
Model* modelGrains = createModel<GrainsModule, GrainsModuleWidget>("Grains"); 