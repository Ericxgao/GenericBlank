#include "plugin.hpp"
#include "daisysp.h"

struct DaisyOscillatorModule : Module
{
    enum Params {
        FREQ_PARAM,
        WAVEFORM_PARAM,
        AMPLITUDE_PARAM,
        NUM_PARAMS
    };
    enum Inputs {
        FREQ_INPUT,
        WAVEFORM_INPUT,
        AMPLITUDE_INPUT,
        NUM_INPUTS
    };
    enum Outputs {
        AUDIO_OUTPUT,
        NUM_OUTPUTS
    };
    enum Lights {
        NUM_LIGHTS
    };

    // DaisySP objects
    daisysp::Oscillator osc;
    daisysp::OnePole filter;
    float sampleRate = 44100.f;

    DaisyOscillatorModule()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(FREQ_PARAM, -54.f, 54.f, 0.f, "Frequency", " Hz", dsp::FREQ_SEMITONE, dsp::FREQ_C4);
        configParam(WAVEFORM_PARAM, 0.f, 3.f, 0.f, "Waveform");
        configParam(AMPLITUDE_PARAM, 0.f, 1.f, 0.8f, "Amplitude");
        
        configInput(FREQ_INPUT, "Frequency CV");
        configInput(WAVEFORM_INPUT, "Waveform CV");
        configInput(AMPLITUDE_INPUT, "Amplitude CV");
        
        configOutput(AUDIO_OUTPUT, "Audio");
        
        // Initialize DaisySP objects
        osc.Init(sampleRate);
        filter.Init();
    }

    void process(const ProcessArgs& args) override
    {
        // Update sample rate if it changed
        if (args.sampleRate != sampleRate) {
            sampleRate = args.sampleRate;
            osc.Init(sampleRate);
            // OnePole doesn't need re-initialization on sample rate change
        }
        
        // Get frequency parameter (in semitones) and CV
        float freqParam = params[FREQ_PARAM].getValue();
        if (inputs[FREQ_INPUT].isConnected()) {
            // Standard 1V/octave CV
            freqParam += inputs[FREQ_INPUT].getVoltage() * 12.f;
        }
        // Convert semitones to Hz
        float freq = dsp::FREQ_C4 * std::pow(2.f, freqParam / 12.f);
        osc.SetFreq(freq);
        
        // Get waveform parameter and CV
        float waveformParam = params[WAVEFORM_PARAM].getValue();
        if (inputs[WAVEFORM_INPUT].isConnected()) {
            waveformParam += inputs[WAVEFORM_INPUT].getVoltage() * 0.3f; // Scale CV
        }
        waveformParam = clamp(waveformParam, 0.f, 3.f);
        
        // Set waveform based on parameter value
        if (waveformParam < 1.f) {
            osc.SetWaveform(daisysp::Oscillator::WAVE_SIN);
        } else if (waveformParam < 2.f) {
            osc.SetWaveform(daisysp::Oscillator::WAVE_TRI);
        } else if (waveformParam < 3.f) {
            osc.SetWaveform(daisysp::Oscillator::WAVE_SAW);
        } else {
            osc.SetWaveform(daisysp::Oscillator::WAVE_SQUARE);
        }
        
        // Get amplitude parameter and CV
        float amplitude = params[AMPLITUDE_PARAM].getValue();
        if (inputs[AMPLITUDE_INPUT].isConnected()) {
            amplitude *= inputs[AMPLITUDE_INPUT].getVoltage() / 10.f; // Normalize 10V CV to 0-1
        }
        amplitude = clamp(amplitude, 0.f, 1.f);
        
        // Process DaisySP oscillator
        float sample = osc.Process();
        
        // Apply amplitude and scale to ±5V (common VCV Rack audio range)
        sample *= amplitude * 5.f;
        
        // Optional: apply simple low-pass filtering
        // Convert 2kHz cutoff to normalized frequency (0-0.497 range)
        float normalizedFreq = clamp(2000.f / sampleRate, 0.f, 0.497f);
        filter.SetFrequency(normalizedFreq);
        sample = filter.Process(sample);
        
        // Output the audio
        outputs[AUDIO_OUTPUT].setVoltage(sample);
    }
};

struct DaisyOscillatorModuleWidget : ModuleWidget
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

    DaisyOscillatorModuleWidget(DaisyOscillatorModule *module)
    {
        setModule(module);
        box.size = Vec(6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        
        // Create a custom white panel (same as Blank module)
        CustomPanel* panel = new CustomPanel();
        panel->box.size = box.size;
        addChild(panel);

        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Parameters
        addParam(createParamCentered<RoundBlackKnob>(Vec(box.size.x / 2, 80), module, DaisyOscillatorModule::FREQ_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(Vec(box.size.x / 2, 140), module, DaisyOscillatorModule::WAVEFORM_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(Vec(box.size.x / 2, 200), module, DaisyOscillatorModule::AMPLITUDE_PARAM));

        // Inputs
        addInput(createInputCentered<PJ301MPort>(Vec(20, 80), module, DaisyOscillatorModule::FREQ_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(20, 140), module, DaisyOscillatorModule::WAVEFORM_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(20, 200), module, DaisyOscillatorModule::AMPLITUDE_INPUT));

        // Output
        addOutput(createOutputCentered<PJ301MPort>(Vec(box.size.x / 2, 280), module, DaisyOscillatorModule::AUDIO_OUTPUT));
    }
};

// Model declaration (this should go in plugin.hpp and plugin.cpp)
Model* modelDaisyOscillator = createModel<DaisyOscillatorModule, DaisyOscillatorModuleWidget>("DaisyOscillator"); 