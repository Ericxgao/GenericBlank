#include "plugin.hpp"
#include "daisysp.h"
#include "Grain.hpp"
#include "GrainManager.hpp"
#include "GrainAlgorithm.hpp"
#include "Dattorro.hpp"
#include <memory>  // Add this for std::make_unique

// Constants
static constexpr size_t DELAY_TIME_SAMPLES = 48000.0f * 30;
static constexpr size_t MAX_GRAINS = 64;

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

    size_t sampleRate = 48000.f; 
    daisysp::DelayLine<float, DELAY_TIME_SAMPLES> delayBuffer;
    
    // Grain manager with MAX_GRAINS grains max
    GrainManager<float, DELAY_TIME_SAMPLES> grainManager;

    // Add Schmitt trigger for clock input
    dsp::SchmittTrigger clockTrigger;

    // Add these as member variables to your Grains class
    private:
        std::unique_ptr<GrainAlgorithm<float, DELAY_TIME_SAMPLES>> currentAlgorithm;
        float bufferSize;
        Dattorro reverb;  // Add Dattorro reverb instance

    public:
        GrainsModule()
            : grainManager(&delayBuffer, sampleRate, MAX_GRAINS)
            , reverb(48000.0, 0.002, 1.0)  // Initialize with 48kHz, 2ms max LFO depth, 1.0 max time scale
        {
            config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
            
            configInput(CLOCK_INPUT, "Clock");
            configInput(AUDIO_INPUT, "Audio Input");
            configOutput(AUDIO_OUTPUT, "Audio Output");
            
            // Initialize the delay buffer
            delayBuffer.Init();

            // Initialize with default random algorithm
            currentAlgorithm = std::make_unique<CloudGrainAlgorithm<float, DELAY_TIME_SAMPLES>>();
            bufferSize = DELAY_TIME_SAMPLES;

            // Set default reverb parameters
            reverb.setTimeScale(1.0);
            reverb.setPreDelay(0.0);
            reverb.setDecay(0.7);
            reverb.setTankDiffusion(0.7);
            reverb.setTankFilterHighCutFrequency(10);
            reverb.setTankFilterLowCutFrequency(0);
            reverb.setInputFilterHighCutoffPitch(10);
            reverb.setInputFilterLowCutoffPitch(0);
            reverb.setTankModSpeed(0.5);
            reverb.setTankModDepth(0.5);
            reverb.setTankModShape(0.5);
        }

        void process(const ProcessArgs& args) override
        {
            // Update sample rate if it changed
            if (args.sampleRate != sampleRate) {
                sampleRate = args.sampleRate;
                reverb.setSampleRate(sampleRate);
            }
            
            // Get audio input and write to buffer
            float audioInput = inputs[AUDIO_INPUT].getVoltage();
            delayBuffer.Write(audioInput);
            
            // Check for clock trigger to start new grain
            if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
                generateGrain();
            }
            
            // Process all grains and get output
            float output = grainManager.process();
            
            // Process through reverb
            reverb.process(output, output);
            
            // Output the processed signal
            outputs[AUDIO_OUTPUT].setVoltage(reverb.getLeftOutput());
        }

        void setAlgorithm(std::unique_ptr<GrainAlgorithm<float, DELAY_TIME_SAMPLES>> newAlgorithm) {
            currentAlgorithm = std::move(newAlgorithm);
        }

        // Replace your existing grain generation code with:
        void generateGrain() {
            if (currentAlgorithm) {
                currentAlgorithm->generateGrains(grainManager, bufferSize);
            }
        }

        // Example methods to switch algorithms
        void setRandomAlgorithm() {
            currentAlgorithm = std::make_unique<RandomGrainAlgorithm<float, DELAY_TIME_SAMPLES>>();
        }

        void setSequentialAlgorithm() {
            currentAlgorithm = std::make_unique<SequentialGrainAlgorithm<float, DELAY_TIME_SAMPLES>>();
        }

        void setCloudAlgorithm() {
            currentAlgorithm = std::make_unique<CloudGrainAlgorithm<float, DELAY_TIME_SAMPLES>>();
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