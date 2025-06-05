#include "plugin.hpp"

struct BlankModule : Module
{
    enum Params {
        ATTACK1_PARAM,
        RELEASE1_PARAM,
        ATTEN1_PARAM,
        ATTACK2_PARAM,
        RELEASE2_PARAM,
        ATTEN2_PARAM,
        NUM_PARAMS
    };
    enum Inputs {
        TRIG_INPUT,
        AUDIO_INPUT,
        MIX1_INPUT,
        MIX2_INPUT,
        ATTACK1_INPUT,
        RELEASE1_INPUT,
        ATTACK2_INPUT,
        RELEASE2_INPUT,
        NUM_INPUTS
    };
    enum Outputs {
        ENV1_OUTPUT,
        ENV2_OUTPUT,
        MIX1_OUTPUT,
        MIX2_OUTPUT,
        AUDIO_OUTPUT,
        NUM_OUTPUTS
    };
    enum Lights {
        NUM_LIGHTS
    };

    // Envelope states
    float env1 = 0.f;
    float env2 = 0.f;
    bool gate1 = false;
    bool gate2 = false;
    
    // For trigger detection
    dsp::SchmittTrigger trigger;

    BlankModule()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(ATTACK1_PARAM, 0.001f, 10.f, 0.5f, "Attack 1", " s");
        configParam(RELEASE1_PARAM, 0.001f, 10.f, 0.5f, "Release 1", " s");
        configParam(ATTEN1_PARAM, -1.f, 1.f, 1.f, "Attenuverter 1");
        configParam(ATTACK2_PARAM, 0.001f, 10.f, 0.5f, "Attack 2", " s");
        configParam(RELEASE2_PARAM, 0.001f, 10.f, 0.5f, "Release 2", " s");
        configParam(ATTEN2_PARAM, -1.f, 1.f, 1.f, "Attenuverter 2");
        
        configInput(TRIG_INPUT, "Trigger");
        configInput(AUDIO_INPUT, "Audio");
        configInput(MIX1_INPUT, "Mix 1");
        configInput(MIX2_INPUT, "Mix 2");
        configInput(ATTACK1_INPUT, "Attack 1 CV");
        configInput(RELEASE1_INPUT, "Release 1 CV");
        configInput(ATTACK2_INPUT, "Attack 2 CV");
        configInput(RELEASE2_INPUT, "Release 2 CV");
        
        configOutput(ENV1_OUTPUT, "Envelope 1");
        configOutput(ENV2_OUTPUT, "Envelope 2");
        configOutput(MIX1_OUTPUT, "Mix 1");
        configOutput(MIX2_OUTPUT, "Mix 2");
        configOutput(AUDIO_OUTPUT, "Audio");
    }

    // Process envelope with exponential decay
    float processEnvelope(float env, float attackTime, float releaseTime, float sampleRate, bool gateActive) {
        // Simple linear attack, exponential release
        if (gateActive) {
            // Attack phase - linear ramp up
            float attackRate = 1.0f / (attackTime * sampleRate);
            env += attackRate;
            
            // Clamp to maximum
            if (env >= 1.0f) {
                env = 1.0f;
                return env;
            }
        } 
        else {
            // Release phase - exponential decay
            // Calculate coefficient to match the expected decay time
            // For exponential decay, we use the formula: value * e^(-t/τ)
            // Where τ (time constant) is calibrated to reach ~0.01 (-40dB) at the specified releaseTime
            // This means: 0.01 = 1 * e^(-releaseTime/τ), so τ = -releaseTime/ln(0.01) ≈ releaseTime/4.6
            float timeConstant = releaseTime / 4.6f;
            float releaseCoef = expf(-1.0f / (timeConstant * sampleRate));
            env *= releaseCoef;
            
            // Clamp to minimum
            if (env <= 0.001f) {
                env = 0.0f;
            }
        }
        
        return env;
    }

    // Helper to get parameter with CV modulation (clamped to range)
    float getParameterWithCV(int paramId, int cvId, float minVal, float maxVal) {
        float paramValue = params[paramId].getValue();
        
        // Apply CV modulation if connected
        if (inputs[cvId].isConnected()) {
            // Normalize CV to 0-1 range (assuming standard 10V CV)
            float cv = clamp(inputs[cvId].getVoltage() / 10.f, 0.f, 1.f);
            // Scale to parameter range and add
            paramValue += cv * (maxVal - minVal);
        }
        
        // Clamp to valid range
        return clamp(paramValue, minVal, maxVal);
    }

    // To process inputs and outputs, override process()
    void process(const ProcessArgs& args) override
    {
        // Process trigger input
        bool triggerState = trigger.process(inputs[TRIG_INPUT].getVoltage());
        
        if (triggerState) {
            // Reset envelopes to 0 when triggered
            env1 = 0.f;
            env2 = 0.f;
            gate1 = true;
            gate2 = true;
        }
        
        // Get parameters with CV modulation
        float attack1 = getParameterWithCV(ATTACK1_PARAM, ATTACK1_INPUT, 0.001f, 10.f);
        float release1 = getParameterWithCV(RELEASE1_PARAM, RELEASE1_INPUT, 0.001f, 10.f);
        float attack2 = getParameterWithCV(ATTACK2_PARAM, ATTACK2_INPUT, 0.001f, 10.f);
        float release2 = getParameterWithCV(RELEASE2_PARAM, RELEASE2_INPUT, 0.001f, 10.f);
        
        // Get attenuverter values
        float atten1 = params[ATTEN1_PARAM].getValue();
        float atten2 = params[ATTEN2_PARAM].getValue();
        
        // Update envelope 1
        env1 = processEnvelope(env1, attack1, release1, args.sampleRate, gate1);
        if (env1 >= 1.0f && gate1) {
            gate1 = false;
        }
        
        // Update envelope 2
        env2 = processEnvelope(env2, attack2, release2, args.sampleRate, gate2);
        if (env2 >= 1.0f && gate2) {
            gate2 = false;
        }
        
        // Apply attenuverters to envelope outputs (scaled to 10V range)
        float scaledEnv1 = env1 * 10.0f * atten1;
        float scaledEnv2 = env2 * 10.0f * atten2;
        
        // Output envelopes
        outputs[ENV1_OUTPUT].setVoltage(scaledEnv1);
        outputs[ENV2_OUTPUT].setVoltage(scaledEnv2);
        
        // Process mix inputs and outputs
        if (inputs[MIX1_INPUT].isConnected()) {
            float mixIn1 = inputs[MIX1_INPUT].getVoltage();
            // Sum the envelope with the mix input
            outputs[MIX1_OUTPUT].setVoltage(mixIn1 + scaledEnv1);
        } else {
            // If no input, just output the envelope
            outputs[MIX1_OUTPUT].setVoltage(scaledEnv1);
        }
        
        if (inputs[MIX2_INPUT].isConnected()) {
            float mixIn2 = inputs[MIX2_INPUT].getVoltage();
            // Sum the envelope with the mix input
            outputs[MIX2_OUTPUT].setVoltage(mixIn2 + scaledEnv2);
        } else {
            // If no input, just output the envelope
            outputs[MIX2_OUTPUT].setVoltage(scaledEnv2);
        }
        
        // Process audio input and output - use only envelope 1 as VCA
        if (inputs[AUDIO_INPUT].isConnected() && outputs[AUDIO_OUTPUT].isConnected()) {
            float audioIn = inputs[AUDIO_INPUT].getVoltage();
            // Apply only envelope 1 modulation to audio (VCA)
            float audioOut = audioIn * env1 * atten1;
            outputs[AUDIO_OUTPUT].setVoltage(audioOut);
        }
    }
};

struct BlankModuleWidget : ModuleWidget
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

    BlankModuleWidget(BlankModule *module)
    {
        setModule(module);
        // Increase to 12 HP for more space
        box.size = Vec(12 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        
        // Create a custom white panel
        CustomPanel* panel = new CustomPanel();
        panel->box.size = box.size;
        addChild(panel);

        // Add standard rack screws
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Column positions
        float col1 = box.size.x / 4;
        float col2 = box.size.x / 2;
        float col3 = 3 * box.size.x / 4;
        
        // *** INPUTS SECTION (TOP) ***
        
        // Add main inputs at the top
        addInput(createInputCentered<PJ301MPort>(Vec(col1, 50), module, BlankModule::TRIG_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(col3, 50), module, BlankModule::AUDIO_INPUT));
        
        // Add CV inputs for envelope 1 (left side)
        addInput(createInputCentered<PJ301MPort>(Vec(col1 - 25, 90), module, BlankModule::ATTACK1_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(col1, 90), module, BlankModule::RELEASE1_INPUT));
        
        // Add Mix inputs 
        addInput(createInputCentered<PJ301MPort>(Vec(col1, 130), module, BlankModule::MIX1_INPUT));
        
        // Add CV inputs for envelope 2 (right side)
        addInput(createInputCentered<PJ301MPort>(Vec(col3 - 25, 90), module, BlankModule::ATTACK2_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(col3, 90), module, BlankModule::RELEASE2_INPUT));
        
        // Add Mix input for env 2
        addInput(createInputCentered<PJ301MPort>(Vec(col3, 130), module, BlankModule::MIX2_INPUT));
        
        // *** CONTROL KNOBS (MIDDLE) ***
        
        // Envelope 1 knobs (left side)
        addParam(createParamCentered<RoundBlackKnob>(Vec(col1 - 25, 180), module, BlankModule::ATTACK1_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(Vec(col1, 180), module, BlankModule::RELEASE1_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(Vec(col1, 230), module, BlankModule::ATTEN1_PARAM));
        
        // Envelope 2 knobs (right side)
        addParam(createParamCentered<RoundBlackKnob>(Vec(col3 - 25, 180), module, BlankModule::ATTACK2_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(Vec(col3, 180), module, BlankModule::RELEASE2_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(Vec(col3, 230), module, BlankModule::ATTEN2_PARAM));
        
        // *** OUTPUTS SECTION (BOTTOM) ***
        
        // Envelope outputs
        addOutput(createOutputCentered<PJ301MPort>(Vec(col1 - 25, 280), module, BlankModule::ENV1_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(col3 - 25, 280), module, BlankModule::ENV2_OUTPUT));
        
        // Mix outputs
        addOutput(createOutputCentered<PJ301MPort>(Vec(col1 + 25, 280), module, BlankModule::MIX1_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(col3 + 25, 280), module, BlankModule::MIX2_OUTPUT));
        
        // Audio output at the bottom
        addOutput(createOutputCentered<PJ301MPort>(Vec(col2, 330), module, BlankModule::AUDIO_OUTPUT));
    }

    // Add options to your module's menu here
    //void appendContextMenu(Menu *menu) override
    //{
    //}
};

Model *modelBlank = createModel<BlankModule, BlankModuleWidget>("blank");
