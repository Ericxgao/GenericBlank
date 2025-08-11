#include "plugin.hpp"
#include "PonyVCOEngine.hpp"
#include "PercEnvelope.hpp"
#include "RingModulator.hpp"
#include "DriveStage.hpp"
#include "LadderFilter.hpp"

using simd::float_4;

struct DrumVoice : Module {
    enum ParamId {
        FREQ_A_PARAM,
        RANGE_A_PARAM,
        TIMBRE_A_PARAM,
        OCT_A_PARAM,
        WAVE_A_PARAM,
        TZFM_A_AMT_PARAM,
        PENV_DECAY_PARAM,
        PENV_AMT_PARAM,
        EXPFM_A_PARAM,
        FREQ_B_PARAM,
        RANGE_B_PARAM,
        TIMBRE_B_PARAM,
        OCT_B_PARAM,
        WAVE_B_PARAM,
        TZFM_B_AMT_PARAM,
        EXPFM_B_PARAM,
        LDR_CUTOFF_PARAM,
        LDR_RES_PARAM,
            // Global ladder cutoff envelope controls
            LDR_ENV_DECAY_PARAM,
            LDR_ENV_AMT_PARAM,
        MIX_A_PARAM,
        MIX_B_PARAM,
        MIX_RING_PARAM,
        DRIVE_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        EXPFM_A_INPUT,
        TIMBRE_A_INPUT,
        VOCT_A_INPUT,
        SYNC_A_INPUT,
        MORPH_A_INPUT,
        TZFM_A_AMT_INPUT,

        EXPFM_B_INPUT,
        TIMBRE_B_INPUT,
        VOCT_B_INPUT,
        SYNC_B_INPUT,
        MORPH_B_INPUT,
        TZFM_B_AMT_INPUT,
        LDR_CUTOFF_INPUT,
        LDR_RES_INPUT,
            // Global ladder cutoff envelope CVs
            LDR_ENV_DECAY_INPUT,
            LDR_ENV_AMT_INPUT,
        PITCH_TRIG_INPUT,
        PENV_DECAY_INPUT,
        PENV_AMT_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        OSC_A_OUTPUT,
        OSC_B_OUTPUT,
        RING_OUTPUT,
        MIX_OUTPUT,
        PENV_OUTPUT,
        LDR_ENV_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId { LIGHTS_LEN };

    float range[4] = {8.f, 1.f, 1.f / 12.f, 10.f};
    PonyVCOEngine enginesA[4];
    PonyVCOEngine enginesB[4];
    int oversamplingIndex = 1;
    float_4 lastOutA[4] = {};
    float_4 lastOutB[4] = {};
    // Single pitch envelope (shared)
    PercEnvelope pitchEnv;
        // Global VCA envelope
        PercEnvelope ldrEnv;
    dsp::SchmittTrigger pitchTrig;
    const float maxPitchEnvVolts = 7.0f;
    RingModulatorSIMD4 ring;
    DriveStageSIMD4 drive;
    LadderFilterSIMD4 ladder[4];

    // Exposed envelope values (0..1) captured during processing for output jacks
    float lastPitchEnv01 = 0.f;
    float lastLdrEnv01 = 0.f;

    DrumVoice() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // A
        configParam(FREQ_A_PARAM, -0.5f, 1.0f, 0.0f, "A Frequency");
        // Range is fixed to Full; no UI
        configParam(TIMBRE_A_PARAM, 0.f, 1.f, 0.f, "A Timbre");
        // Octave selector removed
        configParam(WAVE_A_PARAM, 0.f, 3.f, 0.f, "A Wave morph");
        configParam(TZFM_A_AMT_PARAM, 0.f, 1.f, 0.0f, "A TZFM amount");
        configParam(EXPFM_A_PARAM, 0.f, 1.f, 0.0f, "A Exp FM index");
        configParam(PENV_DECAY_PARAM, 0.f, 1.f, 0.2f, "Pitch env decay");
        configParam(PENV_AMT_PARAM, 0.f, 1.f, 0.0f, "Pitch env amount");
        // Global ladder filter params
        configParam(LDR_CUTOFF_PARAM, 0.f, 1.f, 0.5f, "Ladder cutoff");
        configParam(LDR_RES_PARAM, 0.f, 1.f, 0.0f, "Ladder resonance");
        // Global ladder cutoff envelope params
        configParam(LDR_ENV_DECAY_PARAM, 0.f, 1.f, 0.2f, "Ladder cutoff env decay");
        configParam(LDR_ENV_AMT_PARAM, 0.f, 1.f, 0.0f, "Ladder cutoff env amount");
        configParam(MIX_A_PARAM, 0.f, 1.f, 0.7f, "A");
        configParam(MIX_B_PARAM, 0.f, 1.f, 0.7f, "B");
        configParam(MIX_RING_PARAM, 0.f, 1.f, 0.0f, "Ring");
        configParam(DRIVE_PARAM, 0.f, 1.f, 0.0f, "Drive");

        configInput(EXPFM_A_INPUT, "A Exp FM");
        configInput(TIMBRE_A_INPUT, "A Timber (wavefolder/PWM)");
        configInput(VOCT_A_INPUT, "A Volt per octave");
        configInput(SYNC_A_INPUT, "A Hard sync");
        // Removed VCA A input
        configInput(MORPH_A_INPUT, "A Wave morph CV");
        configInput(TZFM_A_AMT_INPUT, "A TZFM amount CV");
        configInput(LDR_CUTOFF_INPUT, "Ladder cutoff CV");
        configInput(LDR_RES_INPUT, "Ladder resonance CV");
        // Global ladder cutoff envelope CVs
        configInput(LDR_ENV_DECAY_INPUT, "Ladder cutoff env decay CV");
        configInput(LDR_ENV_AMT_INPUT, "Ladder cutoff env amount CV");
        configInput(PENV_DECAY_INPUT, "Pitch env decay CV");
        configInput(PENV_AMT_INPUT, "Pitch env amount CV");
        configOutput(OSC_A_OUTPUT, "Osc A");
        configOutput(OSC_B_OUTPUT, "Osc B");
        configOutput(RING_OUTPUT, "Ring");
        configOutput(MIX_OUTPUT, "Mix");
        configOutput(PENV_OUTPUT, "Pitch envelope (0-10V)");
        configOutput(LDR_ENV_OUTPUT, "Filter env (0-10V)");

        // B
        configParam(FREQ_B_PARAM, -0.5f, 1.0f, 0.0f, "B Frequency");
        // Range is fixed to Full; no UI
        configParam(TIMBRE_B_PARAM, 0.f, 1.f, 0.f, "B Timbre");
        // Octave selector removed
        configParam(WAVE_B_PARAM, 0.f, 3.f, 0.f, "B Wave morph");
        configParam(TZFM_B_AMT_PARAM, 0.f, 1.f, 0.0f, "B TZFM amount");
        configParam(EXPFM_B_PARAM, 0.f, 1.f, 0.0f, "B Exp FM index");
        // global ladder params configured above

        configInput(EXPFM_B_INPUT, "B Exp FM");
        configInput(TIMBRE_B_INPUT, "B Timber (wavefolder/PWM)");
        configInput(VOCT_B_INPUT, "B Volt per octave");
        configInput(SYNC_B_INPUT, "B Hard sync");
        // Removed VCA B input
        configInput(MORPH_B_INPUT, "B Wave morph CV");
        configInput(TZFM_B_AMT_INPUT, "B TZFM amount CV");
        // global ladder CVs configured above
        // pitch env CVs configured above
        // Single mixed output only

        onSampleRateChange();
        drive.reset();
    }

    void onSampleRateChange() override {
        float sampleRate = APP->engine->getSampleRate();
        for (int i = 0; i < 4; ++i) {
            enginesA[i].oversamplingIndex = oversamplingIndex;
            enginesB[i].oversamplingIndex = oversamplingIndex;
            enginesA[i].prepare(sampleRate);
            enginesB[i].prepare(sampleRate);
        }
        drive.reset();
    }

    void processOneVoice(int startCh, bool lfoMode, float baseFreq, int rangeIndex,
                         int freqParam, int timbreParam, int timbreIn, int voctIn, int tzfmIn, int syncIn, int morphParam, int morphIn,
                          int tzfmAmtParam, int tzfmAmtIn,
                          int expfmParam,
                         PercEnvelope& penv, int penvDecayParam, int penvAmtParam, int penvDecayIn, int penvAmtIn,
                         PonyVCOEngine* engines, float_4* lastOutReadOther, float_4* lastOutWriteSelf,
                         bool pitchTrigFired,
                         float_4* voiceNormOut, const ProcessArgs& args) {

        const int channels = std::max({inputs[tzfmIn].getChannels(), inputs[voctIn].getChannels(), inputs[timbreIn].getChannels(), inputs[morphIn].getChannels(), 1});
        float_4* dummy = nullptr; (void)dummy; // suppress warnings if unused

        if (pitchTrigFired) penv.trigger();
        for (int c = startCh; c < channels; c += 4) {
            const float_4 timbre = simd::clamp(params[timbreParam].getValue() + inputs[timbreIn].getPolyVoltageSimd<float_4>(c) / 10.f, 0.f, 1.f);
            // Pitch: base V/Oct + panel offset + pitch envelope in volts
            // Update pitch envelope once per SIMD group
            // triggering handled per voice in process()
            penv.setDecayParam(params[penvDecayParam].getValue());
            penv.setDecayCVVolts(inputs[penvDecayIn].getVoltage());
            const float amtSigned = clamp((params[penvAmtParam].getValue() - 0.5f) * 2.f
                                           + inputs[penvAmtIn].getVoltage() / 10.f,
                                           -1.f, 1.f);
            float penvOut = penv.process(args.sampleTime); // 0..1
            lastPitchEnv01 = penvOut;
            // Center (0) = no pitch modulation; below center sweeps down, above center sweeps up
            const float penvVolts = penvOut * amtSigned * maxPitchEnvVolts;
            const float_4 expfmVolts = float_4(params[expfmParam].getValue()) * lastOutReadOther[c / 4];
            const float_4 pitch = inputs[voctIn].getPolyVoltageSimd<float_4>(c) + expfmVolts + params[freqParam].getValue() * range[rangeIndex] + penvVolts;
            const float_4 freq = baseFreq * simd::pow(2.f, pitch);
            // Calculate normalized TZFM: external TZFM if connected, otherwise from the other voice output scaled by amount
            float_4 tzfmVoltage = inputs[tzfmIn].getPolyVoltageSimd<float_4>(c);
            const bool extConnected = inputs[tzfmIn].isConnected();
            const float_4 amt = simd::clamp(float_4(params[tzfmAmtParam].getValue()) + inputs[tzfmAmtIn].getPolyVoltageSimd<float_4>(c) / 10.f, 0.f, 1.f);
            const float_4 normed = amt * lastOutReadOther[c / 4];
            tzfmVoltage = extConnected ? tzfmVoltage : normed;
            const float_4 morph = simd::clamp(float_4(params[morphParam].getValue()) + 3.f * inputs[morphIn].getPolyVoltageSimd<float_4>(c) / 10.f, 0.f, 3.f);

            float_4 out = engines[c / 4].process(
                args.sampleTime,
                lfoMode,
                freq,
                timbre,
                tzfmVoltage,
                inputs[syncIn].getPolyVoltageSimd<float_4>(c),
                morph
            );

            const float_4 gain = float_4(1.f);
            const float_4 preFilterScaled = 5.f * out * gain; // for cross-normalization
            lastOutWriteSelf[c / 4] = preFilterScaled;
            voiceNormOut[c / 4] = out * gain; // normalized ±1 per voice
        }
        // no output written here
    }

    void process(const ProcessArgs& args) override {
        // Voice A controls
        const int rangeIdxA = 0; // Full range
        const bool lfoA = false;
        const float multA = lfoA ? 1.0 : dsp::FREQ_C4;
        const float baseFreqA = std::pow(2, (int)(params[OCT_A_PARAM].getValue() - 3)) * multA;
        // Compute single trigger
        float trigSrc = inputs[PITCH_TRIG_INPUT].getNormalVoltage(0.f);
        bool trigFlag = pitchTrig.process(rescale(trigSrc, 0.1f, 2.f, 0.f, 1.f));
        float_4 voiceANorm[4] = {};
        float_4 voiceBNorm[4] = {};
        processOneVoice(0, lfoA, baseFreqA, rangeIdxA,
                        FREQ_A_PARAM, TIMBRE_A_PARAM, TIMBRE_A_INPUT, VOCT_A_INPUT, EXPFM_A_INPUT, SYNC_A_INPUT, WAVE_A_PARAM, MORPH_A_INPUT,
                        TZFM_A_AMT_PARAM, TZFM_A_AMT_INPUT,
                        EXPFM_A_PARAM,
                        pitchEnv, PENV_DECAY_PARAM, PENV_AMT_PARAM, PENV_DECAY_INPUT, PENV_AMT_INPUT,
                        enginesA, lastOutB, lastOutA,
                        trigFlag,
                        voiceANorm, args);

        // Voice B controls
        const int rangeIdxB = 0; // Full range
        const bool lfoB = false;
        const float multB = lfoB ? 1.0 : dsp::FREQ_C4;
        const float baseFreqB = std::pow(2, (int)(params[OCT_B_PARAM].getValue() - 3)) * multB;
        // Use same trigger for B
        processOneVoice(0, lfoB, baseFreqB, rangeIdxB,
                        FREQ_B_PARAM, TIMBRE_B_PARAM, TIMBRE_B_INPUT, VOCT_B_INPUT, EXPFM_B_INPUT, SYNC_B_INPUT, WAVE_B_PARAM, MORPH_B_INPUT,
                        TZFM_B_AMT_PARAM, TZFM_B_AMT_INPUT,
                        EXPFM_B_PARAM,
                        pitchEnv, PENV_DECAY_PARAM, PENV_AMT_PARAM, PENV_DECAY_INPUT, PENV_AMT_INPUT,
                        enginesB, lastOutA, lastOutB,
                        trigFlag,
                        voiceBNorm, args);

        // Global ladder filter: mix A and B normalized audio, process once, then output to single output
        const int channels = std::max({inputs[VOCT_A_INPUT].getChannels(), inputs[VOCT_B_INPUT].getChannels(), 1});
        // Trigger global ladder env on the same trigger
        if (trigFlag) {
            ldrEnv.trigger();
        }

        // Update global ladder env params/CVs and process
        ldrEnv.setDecayParam(params[LDR_ENV_DECAY_PARAM].getValue());
        ldrEnv.setDecayCVVolts(inputs[LDR_ENV_DECAY_INPUT].getNormalVoltage(0.f));
        const float ldrAmtNorm = clamp(params[LDR_ENV_AMT_PARAM].getValue() + inputs[LDR_ENV_AMT_INPUT].getNormalVoltage(0.f) / 10.f, 0.f, 1.f);
        ldrEnv.setStrengthNormalized(ldrAmtNorm);
        const float ldrEnvOut01 = ldrEnv.process(args.sampleTime); // 0..1
        lastLdrEnv01 = ldrEnvOut01;
        float cutoff01 = params[LDR_CUTOFF_PARAM].getValue() + inputs[LDR_CUTOFF_INPUT].getNormalVoltage(0.f) / 10.f + ldrEnvOut01;
        cutoff01 = clamp(cutoff01, 0.f, 1.f);
        float cutoffHz = 20.f * std::pow(2.f, cutoff01 * 10.f);
        cutoffHz = clamp(cutoffHz, 1.f, args.sampleRate * 0.18f);
        float res01 = params[LDR_RES_PARAM].getValue() + inputs[LDR_RES_INPUT].getNormalVoltage(0.f) / 10.f;
        res01 = clamp(res01, 0.f, 1.f);
        float_4 resonance = simd::pow(simd::clamp(float_4(res01), 0.f, 1.f), 2) * 10.f;

        for (int c = 0; c < channels; c += 4) {
            const simd::float_4 a = voiceANorm[c / 4];
            const simd::float_4 b = voiceBNorm[c / 4];
            const simd::float_4 ringed = ring.process(a, b, 1.0f);
            const float mixA = clamp(params[MIX_A_PARAM].getValue(), 0.f, 1.f);
            const float mixB = clamp(params[MIX_B_PARAM].getValue(), 0.f, 1.f);
            const float mixRing = clamp(params[MIX_RING_PARAM].getValue(), 0.f, 1.f);
            const simd::float_4 mixNormPreDrive = a * simd::float_4(mixA) + b * simd::float_4(mixB) + ringed * simd::float_4(mixRing);
            const simd::float_4 driven = drive.process(mixNormPreDrive, args.sampleTime, params[DRIVE_PARAM].getValue());
            const simd::float_4 mixNorm = driven;
            ladder[c / 4].setCutoff(float_4(cutoffHz));
            ladder[c / 4].setResonance(resonance);
            ladder[c / 4].process(mixNorm, args.sampleTime);
            const simd::float_4 filtered = ladder[c / 4].lowpass();
            const simd::float_4 mixScaled = 5.f * filtered;
            const simd::float_4 aScaled = 5.f * a;
            const simd::float_4 bScaled = 5.f * b;
            const simd::float_4 ringScaled = 5.f * ringed;
            outputs[MIX_OUTPUT].setVoltageSimd(mixScaled, c);
            outputs[OSC_A_OUTPUT].setVoltageSimd(aScaled, c);
            outputs[OSC_B_OUTPUT].setVoltageSimd(bScaled, c);
            outputs[RING_OUTPUT].setVoltageSimd(ringScaled, c);
        }
        outputs[MIX_OUTPUT].setChannels(channels);
        outputs[OSC_A_OUTPUT].setChannels(channels);
        outputs[OSC_B_OUTPUT].setChannels(channels);
        outputs[RING_OUTPUT].setChannels(channels);
        // Envelope monitor outputs (mono 0..10V)
        outputs[PENV_OUTPUT].setVoltage(10.f * clamp(lastPitchEnv01, 0.f, 1.f));
        outputs[PENV_OUTPUT].setChannels(1);
        outputs[LDR_ENV_OUTPUT].setVoltage(10.f * clamp(lastLdrEnv01, 0.f, 1.f));
        outputs[LDR_ENV_OUTPUT].setChannels(1);
    }
};

struct DrumVoiceWidget : ModuleWidget {
    DrumVoiceWidget(DrumVoice* module) {
        setModule(module);

        // Set panel to 20hp with a simple white panel
        float hp = 20.0f;
        box.size.x = hp * RACK_GRID_WIDTH;
        setPanel(createPanel(asset::plugin(pluginInstance, "res/White20hp.svg")));

        // Screws
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Position controls via adjustable layout parameters (in mm)
        // For 20hp, the panel is 20*5.08 = 101.6mm wide.
        // We'll keep the two columns, but space them further apart.
        // Let's center the columns at 1/3 and 2/3 of the width.
        const float panelWidthMM = 20.0f * 5.08f; // 101.6mm
        const float colLeft = panelWidthMM * 1.0f / 3.0f;   // ~33.87mm
        const float colRight = panelWidthMM * 2.0f / 3.0f;  // ~67.73mm
        const float midCol = (colLeft + colRight) * 0.5f;   // centered between columns
        const float knobY1 = 16.0f;     // large knob Y
        const float knobY2 = 34.0f;     // large knob Y 2
        const float smallKnobY = 52.0f; // small knobs row
        const float jackRow1Y = 76.0f;  // jacks row 1
        const float jackRow2Y = 90.0f;  // jacks row 2
        const float jackRow3Y = 104.0f; // jacks row 3 (extra CVs)
        const float jackRow4Y = 118.0f; // jacks row 4 (extra CVs)
        const float outY = 120.0f;      // output jack Y
        const float smallKnobDX = 10.0f; // horizontal spacing for small knobs around column center (wider for 20hp)
        const float jackDX = 10.0f;      // horizontal spacing for jack triplets around column center (wider for 20hp)
        const float penvKnobY = 60.0f;   // centered row for shared pitch env knobs
        const float ladderKnobY = 70.0f;  // dedicated row for global ladder controls
        const float driveKnobY = 82.0f;   // drive knob centered below ladder
        const float envKnobY = 88.0f;     // small env knobs flanking drive
        const float mixRowY = 96.0f;      // three mix knobs below drive

        // Two voices side-by-side columns
        // Column centers in mm: left voice at colLeft, right voice at colRight
        // Label tuning constants
        static constexpr int LABEL_MAX_CHARS = 6;     // adjustable max label length
        static constexpr float LABEL_FONT_SIZE = 9.0f; // adjustable label font size
        static constexpr float LABEL_DY_KNOB_MM = -6.0f;  // default vertical offset for knob labels (in mm)
        static constexpr float LABEL_DY_JACK_MM = -6.0f;  // default vertical offset for jack labels (in mm)
        static constexpr float LABEL_BOX_WIDTH = 30.0f;   // label box width in px
        static constexpr float LABEL_BOX_HEIGHT = 10.0f;  // label box height in px

        // A minimal tiny label for controls
        struct TinyLabel : TransparentWidget {
            std::string text;
            NVGcolor color = nvgRGB(0x00, 0x00, 0x00);
            float fontSize = 9.f;
            void draw(const DrawArgs& args) override {
                nvgFontSize(args.vg, fontSize);
                nvgFontFaceId(args.vg, APP->window->uiFont->handle);
                nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                nvgFillColor(args.vg, color);
                nvgText(args.vg, box.size.x * 0.5f, box.size.y * 0.5f, text.c_str(), nullptr);
            }
        };

        auto addTinyLabelAtMM = [&](Vec mmCenter, const char* txt, float dyMm) {
            Vec centerPx = mm2px(Vec(mmCenter.x, mmCenter.y + dyMm));
            auto* lab = createWidget<TinyLabel>(centerPx);
            // Set size and reposition so the label is centered at the desired location
            lab->box.size = Vec(LABEL_BOX_WIDTH, LABEL_BOX_HEIGHT);
            Vec half = Vec(lab->box.size.x * 0.5f, lab->box.size.y * 0.5f);
            lab->box.pos = centerPx.minus(half);
            std::string s(txt ? txt : "");
            if ((int)s.size() > LABEL_MAX_CHARS) s = s.substr(0, LABEL_MAX_CHARS);
            lab->text = s;
            lab->fontSize = LABEL_FONT_SIZE;
            addChild(lab);
        };

        // Voice A (left column) — compact 2x3 grid of small knobs in the upper area
        // Set a vertical grid spacing so it can be adjusted in one place
        const float gridYStart = 20.0f;
        const float gridDy = 14.0f; // change this to move all grid rows together
        const float gridY1 = gridYStart;
        const float gridY2 = gridYStart + 1.0f * gridDy;
        const float gridY3 = gridYStart + 2.0f * gridDy;
        const float gridY4 = gridYStart + 3.0f * gridDy;
        const float gridY5 = gridYStart + 4.0f * gridDy;
        const float gridDX = smallKnobDX * 0.8f; // tighter than default to avoid overlap
        // Place related jacks directly below their knobs by a fixed offset tied to the grid spacing
        const float jackYOffset = 4.0f * gridDy + 4.0f; // default 56mm when gridDy=12
        // Precompute knob positions we want to align jacks under (mm)
        const Vec freqAKnob(colLeft - gridDX, gridY1);
        const Vec timbreAKnob(colLeft + gridDX, gridY1);
        const Vec expfmAKnob(colLeft - gridDX, gridY2);
        const Vec waveAKnob(colLeft + gridDX, gridY2);
        const Vec tzfmAmtAKnob(colLeft - 3.f * gridDX, gridY2);
        const Vec penvAmtKnob(colLeft - 3.f * gridDX, gridY3);
        const Vec penvDecayKnob(colLeft - gridDX, gridY3);
        const Vec ldrCutKnob(colRight + 3.f * gridDX, gridY3);
        const Vec ldrResKnob(colRight + gridDX, gridY3);
        const Vec ldrEnvAmtKnob(colLeft + gridDX, gridY3);
        const Vec ldrEnvDecayKnob(colRight - gridDX, gridY3);

        // Row 1: Freq, Timbre
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(freqAKnob), module, DrumVoice::FREQ_A_PARAM));
        addTinyLabelAtMM(freqAKnob, "AFRQ", LABEL_DY_KNOB_MM);
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(timbreAKnob), module, DrumVoice::TIMBRE_A_PARAM));
        addTinyLabelAtMM(timbreAKnob, "ATMB", LABEL_DY_KNOB_MM);
        // Row 2: Exp FM index, Wave morph
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(expfmAKnob), module, DrumVoice::EXPFM_A_PARAM));
        addTinyLabelAtMM(expfmAKnob, "AEXPF", LABEL_DY_KNOB_MM);
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(waveAKnob), module, DrumVoice::WAVE_A_PARAM));
        addTinyLabelAtMM(waveAKnob, "AWAVE", LABEL_DY_KNOB_MM);
        // Row 3: TZFM amount (left)
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(tzfmAmtAKnob), module, DrumVoice::TZFM_A_AMT_PARAM));
        addTinyLabelAtMM(tzfmAmtAKnob, "ATZFM", LABEL_DY_KNOB_MM);
        // Shared pitch env knobs centered between columns
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(penvAmtKnob), module, DrumVoice::PENV_AMT_PARAM));
        addTinyLabelAtMM(penvAmtKnob, "PAMT", LABEL_DY_KNOB_MM);
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(penvDecayKnob), module, DrumVoice::PENV_DECAY_PARAM));
        addTinyLabelAtMM(penvDecayKnob, "PDEC", LABEL_DY_KNOB_MM);
        // Global ladder filter knobs (cutoff left, resonance right) centered between columns
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(ldrCutKnob), module, DrumVoice::LDR_CUTOFF_PARAM));
        addTinyLabelAtMM(ldrCutKnob, "CUT", LABEL_DY_KNOB_MM);
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(ldrResKnob), module, DrumVoice::LDR_RES_PARAM));
        addTinyLabelAtMM(ldrResKnob, "RES", LABEL_DY_KNOB_MM);
        // Drive knob centered below ladder, with env amount/decay flanking
        Vec driveKnob = Vec(colLeft + gridDX, gridY4);
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(driveKnob), module, DrumVoice::DRIVE_PARAM));
        addTinyLabelAtMM(driveKnob, "DRV", LABEL_DY_KNOB_MM);
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(ldrEnvAmtKnob), module, DrumVoice::LDR_ENV_AMT_PARAM));
        addTinyLabelAtMM(ldrEnvAmtKnob, "EAMT", LABEL_DY_KNOB_MM);
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(ldrEnvDecayKnob), module, DrumVoice::LDR_ENV_DECAY_PARAM));
        addTinyLabelAtMM(ldrEnvDecayKnob, "EDEC", LABEL_DY_KNOB_MM);
        // Mix row: A, Ring, B centered
        Vec mixAKnob = Vec(colLeft - 3.f * gridDX, gridY4);
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(mixAKnob), module, DrumVoice::MIX_A_PARAM));
        addTinyLabelAtMM(mixAKnob, "MIXA", LABEL_DY_KNOB_MM);
        Vec mixRingKnob = Vec(colRight - gridDX, gridY4);
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(mixRingKnob), module, DrumVoice::MIX_RING_PARAM));
        addTinyLabelAtMM(mixRingKnob, "MIXR", LABEL_DY_KNOB_MM);
        Vec mixBKnob = Vec(colRight + 3.f * gridDX, gridY4);
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(mixBKnob), module, DrumVoice::MIX_B_PARAM));
        addTinyLabelAtMM(mixBKnob, "MIXB", LABEL_DY_KNOB_MM);

        // Place related jacks under their corresponding knobs using the fixed Y offset
        Vec aExpIn = Vec(expfmAKnob.x, expfmAKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(aExpIn), module, DrumVoice::EXPFM_A_INPUT));
        addTinyLabelAtMM(aExpIn, "AEXPF", LABEL_DY_JACK_MM);
        Vec aTimbIn = Vec(timbreAKnob.x, timbreAKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(aTimbIn), module, DrumVoice::TIMBRE_A_INPUT));
        addTinyLabelAtMM(aTimbIn, "ATMB", LABEL_DY_JACK_MM);
        Vec aVoctIn = Vec(freqAKnob.x, freqAKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(aVoctIn), module, DrumVoice::VOCT_A_INPUT));
        addTinyLabelAtMM(aVoctIn, "AV/O", LABEL_DY_JACK_MM);
        Vec aMorphIn = Vec(waveAKnob.x, waveAKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(aMorphIn), module, DrumVoice::MORPH_A_INPUT));
        addTinyLabelAtMM(aMorphIn, "AMOR", LABEL_DY_JACK_MM);
        // Keep SYNC where it was originally to avoid ambiguity (no direct knob)
        Vec aSyncIn = Vec(freqAKnob.x - 2 * gridDX, freqAKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(aSyncIn), module, DrumVoice::SYNC_A_INPUT));
        addTinyLabelAtMM(aSyncIn, "ASYNC", LABEL_DY_JACK_MM);
        Vec aTzIn = Vec(tzfmAmtAKnob.x, tzfmAmtAKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(aTzIn), module, DrumVoice::TZFM_A_AMT_INPUT));
        addTinyLabelAtMM(aTzIn, "ATZFM", LABEL_DY_JACK_MM);
        // Shared pitch env CVs under their knobs
        Vec pAmtIn = Vec(penvAmtKnob.x, penvAmtKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(pAmtIn), module, DrumVoice::PENV_AMT_INPUT));
        addTinyLabelAtMM(pAmtIn, "PAMT", LABEL_DY_JACK_MM);
        Vec pDecIn = Vec(penvDecayKnob.x, penvDecayKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(pDecIn), module, DrumVoice::PENV_DECAY_INPUT));
        addTinyLabelAtMM(pDecIn, "PDEC", LABEL_DY_JACK_MM);
        // Global ladder CVs under their knobs
        Vec lCutIn = Vec(ldrCutKnob.x, ldrCutKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(lCutIn), module, DrumVoice::LDR_CUTOFF_INPUT));
        addTinyLabelAtMM(lCutIn, "CUT", LABEL_DY_JACK_MM);
        Vec lResIn = Vec(ldrResKnob.x, ldrResKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(lResIn), module, DrumVoice::LDR_RES_INPUT));
        addTinyLabelAtMM(lResIn, "RES", LABEL_DY_JACK_MM);
        Vec lEnvAmtIn = Vec(ldrEnvAmtKnob.x, ldrEnvAmtKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(lEnvAmtIn), module, DrumVoice::LDR_ENV_AMT_INPUT));
        addTinyLabelAtMM(lEnvAmtIn, "EAMT", LABEL_DY_JACK_MM);
        Vec lEnvDecIn = Vec(ldrEnvDecayKnob.x, ldrEnvDecayKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(lEnvDecIn), module, DrumVoice::LDR_ENV_DECAY_INPUT));
        addTinyLabelAtMM(lEnvDecIn, "EDEC", LABEL_DY_JACK_MM);
        // Outputs: evenly spaced across bottom row (Osc A, Ring, Mix, Osc B) plus two envs
        const float outMid = (colLeft + colRight) * 0.5f;
        const float outDx = jackDX * 0.8f; // slightly tighter to keep edge clearance
        const Vec outAmm(outMid - 3 * outDx, outY);
        const Vec outRingmm(outMid - 1 * outDx, outY);
        const Vec outMixmm(outMid + 1 * outDx, outY);
        const Vec outBmm(outMid + 3 * outDx, outY);
        const Vec outPEnv(outMid - 5 * outDx, outY);
        const Vec outFEnv(outMid + 5 * outDx, outY);
        addOutput(createOutputCentered<PJ301MPort>(mm2px(outAmm), module, DrumVoice::OSC_A_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(outRingmm), module, DrumVoice::RING_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(outMixmm), module, DrumVoice::MIX_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(outBmm), module, DrumVoice::OSC_B_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(outPEnv), module, DrumVoice::PENV_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(outFEnv), module, DrumVoice::LDR_ENV_OUTPUT));
        // Output labels
        addTinyLabelAtMM(outAmm, "OSCA", LABEL_DY_JACK_MM);
        addTinyLabelAtMM(outRingmm, "RING", LABEL_DY_JACK_MM);
        addTinyLabelAtMM(outMixmm, "MIX", LABEL_DY_JACK_MM);
        addTinyLabelAtMM(outBmm, "OSCB", LABEL_DY_JACK_MM);
        addTinyLabelAtMM(outPEnv, "PENV", LABEL_DY_JACK_MM);
        addTinyLabelAtMM(outFEnv, "FENV", LABEL_DY_JACK_MM);

        // Voice B (right column) — compact 2x3 grid of small knobs in the upper area
        // Precompute right-column knob positions (mm)
        const Vec freqBKnob(colRight - gridDX, gridY1);
        const Vec timbreBKnob(colRight + gridDX, gridY1);
        const Vec expfmBKnob(colRight - gridDX, gridY2);
        const Vec waveBKnob(colRight + gridDX, gridY2);
        const Vec tzfmAmtBKnob(colRight + 3.f * gridDX, gridY2);

        // Row 1: Freq, Timbre
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(freqBKnob), module, DrumVoice::FREQ_B_PARAM));
        addTinyLabelAtMM(freqBKnob, "BFRQ", LABEL_DY_KNOB_MM);
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(timbreBKnob), module, DrumVoice::TIMBRE_B_PARAM));
        addTinyLabelAtMM(timbreBKnob, "BTMB", LABEL_DY_KNOB_MM);
        // Row 2: Exp FM index, Wave morph
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(expfmBKnob), module, DrumVoice::EXPFM_B_PARAM));
        addTinyLabelAtMM(expfmBKnob, "BEXPF", LABEL_DY_KNOB_MM);
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(waveBKnob), module, DrumVoice::WAVE_B_PARAM));
        addTinyLabelAtMM(waveBKnob, "BWAVE", LABEL_DY_KNOB_MM);
        // Row 3: TZFM amount (left)
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(tzfmAmtBKnob), module, DrumVoice::TZFM_B_AMT_PARAM));
        addTinyLabelAtMM(tzfmAmtBKnob, "BTZFM", LABEL_DY_KNOB_MM);

        // Place related jacks under their corresponding knobs using the fixed Y offset
        Vec bExpIn = Vec(expfmBKnob.x, expfmBKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(bExpIn), module, DrumVoice::EXPFM_B_INPUT));
        addTinyLabelAtMM(bExpIn, "BEXPF", LABEL_DY_JACK_MM);
        Vec bTimbIn = Vec(timbreBKnob.x, timbreBKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(bTimbIn), module, DrumVoice::TIMBRE_B_INPUT));
        addTinyLabelAtMM(bTimbIn, "BTMB", LABEL_DY_JACK_MM);
        Vec bVoctIn = Vec(freqBKnob.x, freqBKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(bVoctIn), module, DrumVoice::VOCT_B_INPUT));
        addTinyLabelAtMM(bVoctIn, "BV/O", LABEL_DY_JACK_MM);
        Vec bMorphIn = Vec(waveBKnob.x, waveBKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(bMorphIn), module, DrumVoice::MORPH_B_INPUT));
        addTinyLabelAtMM(bMorphIn, "BMOR", LABEL_DY_JACK_MM);
        // Keep SYNC where it was originally to avoid ambiguity (no direct knob)
        Vec bSyncIn = Vec(timbreBKnob.x + 2 * gridDX, timbreBKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(bSyncIn), module, DrumVoice::SYNC_B_INPUT));
        addTinyLabelAtMM(bSyncIn, "BSYNC", LABEL_DY_JACK_MM);
        Vec bTzIn = Vec(tzfmAmtBKnob.x, tzfmAmtBKnob.y + jackYOffset);
        addInput(createInputCentered<PJ301MPort>(mm2px(bTzIn), module, DrumVoice::TZFM_B_AMT_INPUT));
        addTinyLabelAtMM(bTzIn, "BTZFM", LABEL_DY_JACK_MM);
        // (global ladder CVs already added above)
        // removed second output

        // Single pitch trigger centered near top
        Vec trigIn = Vec((colLeft + colRight) * 0.5f, 10.0f);
        addInput(createInputCentered<PJ301MPort>(mm2px(trigIn), module, DrumVoice::PITCH_TRIG_INPUT));
        addTinyLabelAtMM(trigIn, "TRIG", LABEL_DY_JACK_MM);
    }
};

Model* modelDrumVoice = createModel<DrumVoice, DrumVoiceWidget>("drumvoice");
