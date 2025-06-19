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
        DENSITY_PARAM,
        DURATION_PARAM,
        ENV_DURATION_PARAM,
        SPEED_PARAM,
        DELAY_PARAM,
        PAN_PARAM,
        NUM_PARAMS
    };
    enum Inputs {
        CLOCK_INPUT,
        AUDIO_INPUT_L,
        AUDIO_INPUT_R,
        NUM_INPUTS
    };
    enum Outputs {
        AUDIO_OUTPUT_L,
        AUDIO_OUTPUT_R,
        NUM_OUTPUTS
    };
    enum Lights {
        NUM_LIGHTS
    };

    size_t sampleRate = 48000.f; 
    daisysp::DelayLine<float, DELAY_TIME_SAMPLES> delayBufferL;
    daisysp::DelayLine<float, DELAY_TIME_SAMPLES> delayBufferR;
    
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
            : grainManager(&delayBufferL, &delayBufferR, sampleRate, MAX_GRAINS)
            , reverb(48000.0, 0.002, 1.0)  // Initialize with 48kHz, 2ms max LFO depth, 1.0 max time scale
        {
            config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
            
            // Configure parameters
            configParam(DENSITY_PARAM, 0.0f, 1.0f, 0.5f, "Density");
            configParam(DURATION_PARAM, 0.01f, 5.0f, 0.1f, "Duration", "s");
            configParam(ENV_DURATION_PARAM, 0.01f, 10.0f, 0.1f, "Envelope Duration", "s");
            configParam(SPEED_PARAM, -8.0f, 8.0f, 0.0f, "Speed", "V/oct");
            configParam(DELAY_PARAM, 0.0f, 10.0f, 0.0f, "Delay", "s");
            configParam(PAN_PARAM, -1.0f, 1.0f, 0.0f, "Pan");
            
            configInput(CLOCK_INPUT, "Clock");
            configInput(AUDIO_INPUT_L, "Audio Input L");
            configInput(AUDIO_INPUT_R, "Audio Input R");
            configOutput(AUDIO_OUTPUT_L, "Audio Output L");
            configOutput(AUDIO_OUTPUT_R, "Audio Output R");
            
            // Initialize the delay buffer
            delayBufferL.Init();
            delayBufferR.Init();

            // Initialize with default random algorithm
            currentAlgorithm = std::make_unique<BaseAlgorithm<float, DELAY_TIME_SAMPLES>>();
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
            float audioInputL = inputs[AUDIO_INPUT_L].getVoltage();
            float audioInputR = inputs[AUDIO_INPUT_R].isConnected() ? inputs[AUDIO_INPUT_R].getVoltage() : audioInputL;
            delayBufferL.Write(audioInputL);
            delayBufferR.Write(audioInputR);
            
            // Update algorithm parameters
            if (auto* baseAlgo = dynamic_cast<BaseAlgorithm<float, DELAY_TIME_SAMPLES>*>(currentAlgorithm.get())) {
                float density = params[DENSITY_PARAM].getValue();
                float duration = params[DURATION_PARAM].getValue();
                float envDuration = params[ENV_DURATION_PARAM].getValue();
                float speed = 2.0f * params[SPEED_PARAM].getValue(); // Convert V/oct to speed multiplier
                float delay = params[DELAY_PARAM].getValue();
                float pan = params[PAN_PARAM].getValue();
                
                baseAlgo->setParameters(density, duration, envDuration, speed, delay, pan);
            }
            
            // Check for clock trigger to start new grain
            if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
                generateGrain();
            }
            
            // Process all grains and get output
            StereoPacket output = grainManager.process();
            
            // Scale up the output to proper voltage levels
            output.left *= 5.0f;  // Scale to ±5V range
            output.right *= 5.0f; // Scale to ±5V range
            
            // Output the processed signal
            outputs[AUDIO_OUTPUT_L].setVoltage(output.left);
            outputs[AUDIO_OUTPUT_R].setVoltage(output.right);
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

    struct TextWidget : Widget {
        std::string text;
        NVGcolor color = nvgRGB(0, 0, 0);
        
        TextWidget(std::string text) : text(text) {}
        
        void draw(const DrawArgs& args) override {
            // Draw the text
            nvgFontSize(args.vg, 12);
            nvgFontFaceId(args.vg, APP->window->uiFont->handle);
            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(args.vg, color);
            nvgText(args.vg, box.size.x / 2, box.size.y / 2, text.c_str(), NULL);
        }
    };

    // Parameter definition structure
    struct ParamDef {
        int paramId;
        Vec position;
        std::string label;
        
        // Fixed label positioning that we know works
        Vec getLabelOffset() const { return Vec(-20, 25); }  // Fixed offset
        Vec getLabelSize() const { return Vec(40, 20); }     // Fixed size
    };

    // Input/Output definition structure
    struct IODef {
        int ioId;
        Vec position;
        std::string label;
        bool isInput;
        
        // Fixed label positioning that we know works
        Vec getLabelOffset() const { return Vec(-20, 25); }  // Fixed offset
        Vec getLabelSize() const { return Vec(40, 20); }     // Fixed size
    };

    GrainsModuleWidget(GrainsModule *module)
    {
        setModule(module);
        box.size = Vec(16 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);  // Increased to 12 HP
        
        // Create a custom white panel
        CustomPanel* panel = new CustomPanel();
        panel->box.size = box.size;
        addChild(panel);

        // Add screws first
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Define all parameters with their positions and labels - spaced 60px apart
        std::vector<ParamDef> params = {
            {GrainsModule::DENSITY_PARAM, Vec(60, 80), "Density"},
            {GrainsModule::DURATION_PARAM, Vec(120, 80), "Duration"},
            {GrainsModule::ENV_DURATION_PARAM, Vec(180, 80), "Env Dur"},
            {GrainsModule::SPEED_PARAM, Vec(60, 140), "Speed"},
            {GrainsModule::DELAY_PARAM, Vec(120, 140), "Delay"},
            {GrainsModule::PAN_PARAM, Vec(180, 140), "Pan"}
        };

        // Define all inputs/outputs with their positions and labels - spaced 60px apart
        std::vector<IODef> ios = {
            {GrainsModule::CLOCK_INPUT, Vec(60, 200), "Clock", true},
            {GrainsModule::AUDIO_INPUT_L, Vec(120, 200), "Audio L", true},
            {GrainsModule::AUDIO_INPUT_R, Vec(180, 200), "Audio R", true},
            {GrainsModule::AUDIO_OUTPUT_L, Vec(90, 260), "Out L", false},
            {GrainsModule::AUDIO_OUTPUT_R, Vec(150, 260), "Out R", false}
        };

        // Create parameters and their labels
        for (const auto& param : params) {
            addParam(createParamCentered<RoundBlackKnob>(param.position, module, param.paramId));
            
            TextWidget* label = new TextWidget(param.label);
            label->box.pos = Vec(param.position.x - 20, param.position.y + 25);
            label->box.size = Vec(40, 20);
            addChild(label);
        }

        // Create inputs/outputs and their labels
        for (const auto& io : ios) {
            if (io.isInput) {
                addInput(createInputCentered<PJ301MPort>(io.position, module, io.ioId));
            } else {
                addOutput(createOutputCentered<PJ301MPort>(io.position, module, io.ioId));
            }
            
            TextWidget* label = new TextWidget(io.label);
            label->box.pos = Vec(io.position.x - 20, io.position.y + 25);
            label->box.size = Vec(40, 20);
            addChild(label);
        }
    }
};

// Model declaration
Model* modelGrains = createModel<GrainsModule, GrainsModuleWidget>("Grains"); 