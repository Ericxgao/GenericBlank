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
        TIME_DIVISION_PARAM,
        MAX_GRAINS_PARAM,
        THRESHOLD_PARAM,
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
        TRANSIENT_LIGHT,
        NUM_LIGHTS
    };

    size_t sampleRate = 48000.f; 
    daisysp::DelayLine<float, DELAY_TIME_SAMPLES> delayBufferL;
    daisysp::DelayLine<float, DELAY_TIME_SAMPLES> delayBufferR;
    
    // Grain manager with MAX_GRAINS grains max
    GrainManager<float, DELAY_TIME_SAMPLES> grainManager;

    // Add Schmitt trigger for clock input
    dsp::SchmittTrigger clockTrigger;
    
    // Simple BPM tracking
    float lastTriggerTime = 0.0f;
    float intervals[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int intervalIndex = 0;
    float currentBPM = 0.0f;
    
    // Time division tracking
    float lastGrainTime = 0.0f;
    float currentTime = 0.0f;

    // Transient detection variables
    float lastAudioLevel = 0.0f;
    float transientThreshold = 0.5f;
    bool transientDetected = false;
    int transientHoldTime = 0;
    static constexpr int TRANSIENT_HOLD_SAMPLES = 4800; // 100ms at 48kHz

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
            configParam(TIME_DIVISION_PARAM, 0.0f, 23.0f, 2.0f, "Time Division", "", 0.0f, 1.0f);
            configParam(MAX_GRAINS_PARAM, 1, 64, 32, "Max Grains");
            configParam(THRESHOLD_PARAM, 0.0f, 1.0f, 0.5f, "Threshold");
            
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
            
            // Update current time
            currentTime = args.frame;
            
            // Get audio input and write to buffer
            float audioInputL = inputs[AUDIO_INPUT_L].getVoltage();
            float audioInputR = inputs[AUDIO_INPUT_R].isConnected() ? inputs[AUDIO_INPUT_R].getVoltage() : audioInputL;
            delayBufferL.Write(audioInputL);
            delayBufferR.Write(audioInputR);
            
            // Transient detection
            float currentAudioLevel = std::abs(audioInputL) + std::abs(audioInputR);
            float audioChange = std::abs(currentAudioLevel - lastAudioLevel);
            float threshold = params[THRESHOLD_PARAM].getValue();
            
            // Detect transient based on rate of change
            if (audioChange > threshold && !transientDetected) {
                transientDetected = true;
                transientHoldTime = 0;
            }
            
            // Hold the transient detection for a short time
            if (transientDetected) {
                transientHoldTime++;
                if (transientHoldTime >= TRANSIENT_HOLD_SAMPLES) {
                    transientDetected = false;
                }
            }
            
            lastAudioLevel = currentAudioLevel;
            
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
            
            // Update max grains parameter
            int maxGrains = (int)params[MAX_GRAINS_PARAM].getValue();
            grainManager.setMaxActiveGrains(maxGrains);
            
            // Check for clock trigger to track BPM
            if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
                // Simple BPM tracking
                if (lastTriggerTime > 0.0f) {
                    float interval = (currentTime - lastTriggerTime) / sampleRate;
                    intervals[intervalIndex] = interval;
                    intervalIndex = (intervalIndex + 1) % 4;
                    
                    // Calculate average BPM from last 4 intervals
                    float avgInterval = (intervals[0] + intervals[1] + intervals[2] + intervals[3]) / 4.0f;
                    currentBPM = 60.0f / avgInterval;
                }
                lastTriggerTime = currentTime;
            }
            
            // Check if we should trigger a grain based on time division
            if (currentBPM > 0.0f) {
                float timeDivision = params[TIME_DIVISION_PARAM].getValue();
                // Snap to nearest integer for discrete divisions
                int divisionIndex = (int)(timeDivision + 0.5f);
                divisionIndex = clamp(divisionIndex, 0, 23);
                
                // Time divisions sorted by actual time value (fastest to slowest)
                // Interweaving regular, dotted, and triplet divisions
                float divisions[] = {
                    1.0f/32.0f,    // 0: 1/32 (fastest)
                    2.0f/96.0f,    // 1: 1/32T (triplet)
                    1.5f/32.0f,    // 2: 1/32. (dotted)
                    1.0f/16.0f,    // 3: 1/16
                    2.0f/48.0f,    // 4: 1/16T
                    1.5f/16.0f,    // 5: 1/16.
                    1.0f/8.0f,     // 6: 1/8
                    2.0f/24.0f,    // 7: 1/8T
                    1.5f/8.0f,     // 8: 1/8.
                    1.0f/4.0f,     // 9: 1/4
                    2.0f/12.0f,    // 10: 1/4T
                    1.5f/4.0f,     // 11: 1/4.
                    1.0f/2.0f,     // 12: 1/2
                    2.0f/6.0f,     // 13: 1/2T
                    1.5f/2.0f,     // 14: 1/2.
                    1.0f,          // 15: 1 (whole)
                    2.0f/3.0f,     // 16: 1T
                    1.5f,          // 17: 1.
                    2.0f,          // 18: 2
                    4.0f/3.0f,     // 19: 2T
                    3.0f,          // 20: 2.
                    4.0f,          // 21: 4
                    8.0f/3.0f,     // 22: 4T
                    6.0f           // 23: 4. (slowest)
                };
                float selectedDivision = divisions[divisionIndex];
                
                // Calculate time between grains based on BPM and division
                float beatTime = 60.0f / currentBPM;  // seconds per beat
                float grainInterval = beatTime * selectedDivision;  // seconds between grains
                float grainIntervalSamples = grainInterval * sampleRate;
                
                // Check if enough time has passed since last grain
                if (currentTime - lastGrainTime >= grainIntervalSamples) {
                    generateGrain();
                    lastGrainTime = currentTime;
                }
            }
            
            // Process all grains and get output
            StereoPacket output = grainManager.process();
            
            // Scale up the output to proper voltage levels
            output.left *= 5.0f;  // Scale to ±5V range
            output.right *= 5.0f; // Scale to ±5V range
            
            // Output the processed signal
            outputs[AUDIO_OUTPUT_L].setVoltage(output.left);
            outputs[AUDIO_OUTPUT_R].setVoltage(output.right);
            
            // Update transient light
            lights[TRANSIENT_LIGHT].setBrightness(transientDetected ? 1.0f : 0.0f);
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
        
        // Simple BPM getter
        float getBPM() const {
            return currentBPM;
        }
        
        // Get current time division as string
        std::string getTimeDivisionString() {
            float timeDivision = params[TIME_DIVISION_PARAM].getValue();
            // Snap to nearest integer for discrete divisions
            int divisionIndex = (int)(timeDivision + 0.5f);
            divisionIndex = clamp(divisionIndex, 0, 23);
            const char* divisions[] = {
                "1/32", "1/32T", "1/32.", "1/16", "1/16T", "1/16.", "1/8", "1/8T", "1/8.", 
                "1/4", "1/4T", "1/4.", "1/2", "1/2T", "1/2.", "1", "1T", "1.", 
                "2", "2T", "2.", "4", "4T", "4."
            };
            return divisions[divisionIndex];
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
    
    // Simple BPM Display
    struct BPMDisplayWidget : Widget {
        GrainsModule* module;
        
        BPMDisplayWidget(GrainsModule* module) : module(module) {}
        
        void draw(const DrawArgs& args) override {
            std::string displayText = "--- BPM";
            if (module) {
                float bpm = module->getBPM();
                if (bpm > 0.0f) {
                    char bpmStr[64];
                    std::string timeDiv = module->getTimeDivisionString();
                    snprintf(bpmStr, sizeof(bpmStr), "%.0f BPM %s", bpm, timeDiv.c_str());
                    displayText = std::string(bpmStr);
                }
            }
            
            nvgFontSize(args.vg, 14);
            nvgFontFaceId(args.vg, APP->window->uiFont->handle);
            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(args.vg, nvgRGB(0, 0, 0));
            nvgText(args.vg, box.size.x / 2, box.size.y / 2, displayText.c_str(), NULL);
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

        // Add simple BPM display
        BPMDisplayWidget* bpmDisplay = new BPMDisplayWidget(module);
        bpmDisplay->box.pos = Vec(60, 30);
        bpmDisplay->box.size = Vec(120, 30);
        addChild(bpmDisplay);

        // Define all parameters with their positions and labels - spaced 60px apart
        std::vector<ParamDef> params = {
            {GrainsModule::DENSITY_PARAM, Vec(60, 80), "Density"},
            {GrainsModule::DURATION_PARAM, Vec(120, 80), "Duration"},
            {GrainsModule::ENV_DURATION_PARAM, Vec(180, 80), "Env Dur"},
            {GrainsModule::SPEED_PARAM, Vec(60, 140), "Speed"},
            {GrainsModule::DELAY_PARAM, Vec(120, 140), "Delay"},
            {GrainsModule::PAN_PARAM, Vec(180, 140), "Pan"},
            {GrainsModule::TIME_DIVISION_PARAM, Vec(120, 200), "Time Div"},
            {GrainsModule::MAX_GRAINS_PARAM, Vec(180, 200), "Max Grains"},
            {GrainsModule::THRESHOLD_PARAM, Vec(120, 260), "Threshold"}
        };

        // Define all inputs/outputs with their positions and labels - spaced 60px apart
        std::vector<IODef> ios = {
            {GrainsModule::CLOCK_INPUT, Vec(60, 320), "Clock", true},
            {GrainsModule::AUDIO_INPUT_L, Vec(120, 320), "Audio L", true},
            {GrainsModule::AUDIO_INPUT_R, Vec(180, 320), "Audio R", true},
            {GrainsModule::AUDIO_OUTPUT_L, Vec(90, 380), "Out L", false},
            {GrainsModule::AUDIO_OUTPUT_R, Vec(150, 380), "Out R", false}
        };

        // Create parameters and their labels
        for (const auto& param : params) {
            if (param.paramId == GrainsModule::TIME_DIVISION_PARAM || param.paramId == GrainsModule::MAX_GRAINS_PARAM) {
                addParam(createParamCentered<RoundBlackSnapKnob>(param.position, module, param.paramId));
            } else {
                addParam(createParamCentered<RoundBlackKnob>(param.position, module, param.paramId));
            }
            
            TextWidget* label = new TextWidget(param.label);
            label->box.pos = Vec(param.position.x - 20, param.position.y + 25);
            label->box.size = Vec(40, 20);
            addChild(label);
        }

        // Add transient light
        addChild(createLightCentered<MediumLight<GreenLight>>(Vec(150, 260), module, GrainsModule::TRANSIENT_LIGHT));

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