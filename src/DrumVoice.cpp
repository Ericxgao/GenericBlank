#include "plugin.hpp"
#include "PonyVCOEngine.hpp"
#include "PercEnvelope.hpp"
#include "LadderFilter.hpp"
#include "RingModulator.hpp"
#include "DriveStage.hpp"

using simd::float_4;

struct DrumVoice : Module {
    enum ParamId {
        FREQ_A_PARAM,
        RANGE_A_PARAM,
        TIMBRE_A_PARAM,
        OCT_A_PARAM,
        WAVE_A_PARAM,
        TZFM_A_AMT_PARAM,
        PENV_DECAY_A_PARAM,
        PENV_AMT_A_PARAM,
        FREQ_B_PARAM,
        RANGE_B_PARAM,
        TIMBRE_B_PARAM,
        OCT_B_PARAM,
        WAVE_B_PARAM,
        TZFM_B_AMT_PARAM,
        PENV_DECAY_B_PARAM,
        PENV_AMT_B_PARAM,
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
        TZFM_A_INPUT,
        TIMBRE_A_INPUT,
        VOCT_A_INPUT,
        SYNC_A_INPUT,
        MORPH_A_INPUT,
        TZFM_A_AMT_INPUT,

        TZFM_B_INPUT,
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
        PITCH_TRIG_A_INPUT,
        PITCH_TRIG_B_INPUT,
        PENV_DECAY_A_INPUT,
        PENV_AMT_A_INPUT,
        PENV_DECAY_B_INPUT,
        PENV_AMT_B_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        OUT_A_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId { LIGHTS_LEN };

    float range[4] = {8.f, 1.f, 1.f / 12.f, 10.f};
    PonyVCOEngine enginesA[4];
    PonyVCOEngine enginesB[4];
    int oversamplingIndex = 1;
    float_4 lastOutA[4] = {};
    float_4 lastOutB[4] = {};
    // Pitch envelopes per voice
    PercEnvelope envA;
    PercEnvelope envB;
        // Global ladder cutoff envelope
        PercEnvelope ldrEnv;
    dsp::SchmittTrigger pitchTrigA;
    dsp::SchmittTrigger pitchTrigB;
    const float maxPitchEnvVolts = 7.0f;
    LadderFilterSIMD4 ladder[4];
    RingModulatorSIMD4 ring;
    DriveStageSIMD4 drive;

    DrumVoice() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        // A
        configParam(FREQ_A_PARAM, -0.5f, 0.5f, 0.0f, "A Frequency");
        // Range is fixed to Full; no UI
        configParam(TIMBRE_A_PARAM, 0.f, 1.f, 0.f, "A Timbre");
        // Octave selector removed
        configParam(WAVE_A_PARAM, 0.f, 3.f, 0.f, "A Wave morph");
        configParam(TZFM_A_AMT_PARAM, 0.f, 1.f, 0.0f, "A TZFM amount");
        configParam(PENV_DECAY_A_PARAM, 0.f, 1.f, 0.2f, "A Pitch env decay");
        configParam(PENV_AMT_A_PARAM, 0.f, 1.f, 0.0f, "A Pitch env amount");
        configParam(LDR_CUTOFF_PARAM, 0.f, 1.f, 0.5f, "Ladder cutoff");
        configParam(LDR_RES_PARAM, 0.f, 1.f, 0.0f, "Ladder resonance");
        // Global ladder cutoff envelope params
        configParam(LDR_ENV_DECAY_PARAM, 0.f, 1.f, 0.2f, "Ladder cutoff env decay");
        configParam(LDR_ENV_AMT_PARAM, 0.f, 1.f, 0.0f, "Ladder cutoff env amount");
        configParam(MIX_A_PARAM, 0.f, 1.f, 0.7f, "A");
        configParam(MIX_B_PARAM, 0.f, 1.f, 0.7f, "B");
        configParam(MIX_RING_PARAM, 0.f, 1.f, 0.0f, "Ring");
        configParam(DRIVE_PARAM, 0.f, 1.f, 0.0f, "Drive");

        configInput(TZFM_A_INPUT, "A Through-zero FM");
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
        configInput(PENV_DECAY_A_INPUT, "A Pitch env decay CV");
        configInput(PENV_AMT_A_INPUT, "A Pitch env amount CV");
        configOutput(OUT_A_OUTPUT, "Output");

        // B
        configParam(FREQ_B_PARAM, -0.5f, 0.5f, 0.0f, "B Frequency");
        // Range is fixed to Full; no UI
        configParam(TIMBRE_B_PARAM, 0.f, 1.f, 0.f, "B Timbre");
        // Octave selector removed
        configParam(WAVE_B_PARAM, 0.f, 3.f, 0.f, "B Wave morph");
        configParam(TZFM_B_AMT_PARAM, 0.f, 1.f, 0.0f, "B TZFM amount");
        configParam(PENV_DECAY_B_PARAM, 0.f, 1.f, 0.2f, "B Pitch env decay");
        configParam(PENV_AMT_B_PARAM, 0.f, 1.f, 0.0f, "B Pitch env amount");
        // global ladder params configured above

        configInput(TZFM_B_INPUT, "B Through-zero FM");
        configInput(TIMBRE_B_INPUT, "B Timber (wavefolder/PWM)");
        configInput(VOCT_B_INPUT, "B Volt per octave");
        configInput(SYNC_B_INPUT, "B Hard sync");
        // Removed VCA B input
        configInput(MORPH_B_INPUT, "B Wave morph CV");
        configInput(TZFM_B_AMT_INPUT, "B TZFM amount CV");
        // global ladder CVs configured above
        configInput(PENV_DECAY_B_INPUT, "B Pitch env decay CV");
        configInput(PENV_AMT_B_INPUT, "B Pitch env amount CV");
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
            // Center (0) = no pitch modulation; below center sweeps down, above center sweeps up
            const float penvVolts = penvOut * amtSigned * maxPitchEnvVolts;
            const float_4 pitch = inputs[voctIn].getPolyVoltageSimd<float_4>(c) + params[freqParam].getValue() * range[rangeIndex] + penvVolts;
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
        // Compute trigger for A
        float trigSrcA = inputs[PITCH_TRIG_A_INPUT].getNormalVoltage(0.f);
        bool trigAFlag = pitchTrigA.process(rescale(trigSrcA, 0.1f, 2.f, 0.f, 1.f));
        float_4 voiceANorm[4] = {};
        float_4 voiceBNorm[4] = {};
        processOneVoice(0, lfoA, baseFreqA, rangeIdxA,
                        FREQ_A_PARAM, TIMBRE_A_PARAM, TIMBRE_A_INPUT, VOCT_A_INPUT, TZFM_A_INPUT, SYNC_A_INPUT, WAVE_A_PARAM, MORPH_A_INPUT,
                        TZFM_A_AMT_PARAM, TZFM_A_AMT_INPUT,
                        envA, PENV_DECAY_A_PARAM, PENV_AMT_A_PARAM, PENV_DECAY_A_INPUT, PENV_AMT_A_INPUT,
                        enginesA, lastOutB, lastOutA,
                        trigAFlag,
                        voiceANorm, args);

        // Voice B controls
        const int rangeIdxB = 0; // Full range
        const bool lfoB = false;
        const float multB = lfoB ? 1.0 : dsp::FREQ_C4;
        const float baseFreqB = std::pow(2, (int)(params[OCT_B_PARAM].getValue() - 3)) * multB;
        // Compute trigger for B: prefer own, else normalled from A
        float trigSrcB = inputs[PITCH_TRIG_B_INPUT].getNormalVoltage(inputs[PITCH_TRIG_A_INPUT].getVoltage());
        bool trigBFlag = pitchTrigB.process(rescale(trigSrcB, 0.1f, 2.f, 0.f, 1.f));
        processOneVoice(0, lfoB, baseFreqB, rangeIdxB,
                        FREQ_B_PARAM, TIMBRE_B_PARAM, TIMBRE_B_INPUT, VOCT_B_INPUT, TZFM_B_INPUT, SYNC_B_INPUT, WAVE_B_PARAM, MORPH_B_INPUT,
                        TZFM_B_AMT_PARAM, TZFM_B_AMT_INPUT,
                        envB, PENV_DECAY_B_PARAM, PENV_AMT_B_PARAM, PENV_DECAY_B_INPUT, PENV_AMT_B_INPUT,
                        enginesB, lastOutA, lastOutB,
                        trigBFlag,
                        voiceBNorm, args);

        // Global ladder filter: mix A and B normalized audio, process once, then output to single output
        const int channels = std::max({inputs[VOCT_A_INPUT].getChannels(), inputs[VOCT_B_INPUT].getChannels(), 1});
        // Trigger global ladder env on either pitch trigger
        if (trigAFlag || trigBFlag) {
            ldrEnv.trigger();
        }

        // Update global ladder env params/CVs and process
        ldrEnv.setDecayParam(params[LDR_ENV_DECAY_PARAM].getValue());
        ldrEnv.setDecayCVVolts(inputs[LDR_ENV_DECAY_INPUT].getNormalVoltage(0.f));
        const float ldrAmtNorm = clamp(params[LDR_ENV_AMT_PARAM].getValue() + inputs[LDR_ENV_AMT_INPUT].getNormalVoltage(0.f) / 10.f, 0.f, 1.f);
        ldrEnv.setStrengthNormalized(ldrAmtNorm);
        const float ldrEnvOut01 = ldrEnv.process(args.sampleTime); // 0..1

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
            float_4 filtered = ladder[c / 4].lowpass();
            float_4 scaled = 5.f * filtered;
            outputs[OUT_A_OUTPUT].setVoltageSimd(scaled, c);
        }
        outputs[OUT_A_OUTPUT].setChannels(channels);
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
        const float outY = 128.0f;      // output jack Y
        const float smallKnobDX = 12.0f; // horizontal spacing for small knobs around column center (wider for 20hp)
        const float jackDX = 12.0f;      // horizontal spacing for jack triplets around column center (wider for 20hp)
        const float ladderKnobY = 70.0f;  // dedicated row for global ladder controls
        const float driveKnobY = 82.0f;   // drive knob centered below ladder
        const float envKnobY = 88.0f;     // small env knobs flanking drive
        const float mixRowY = 96.0f;      // three mix knobs below drive

        // Two voices side-by-side columns
        // Column centers in mm: left voice at colLeft, right voice at colRight
        // Voice A (left column)
        addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(colLeft, knobY1)), module, DrumVoice::FREQ_A_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(colLeft, knobY2)), module, DrumVoice::TIMBRE_A_PARAM));
        // Range and Octave controls removed
        // Single-row small knobs per voice: TZFM amt, PENV amt, PENV decay, Wave morph
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(colLeft - 1.5f * smallKnobDX, smallKnobY)), module, DrumVoice::TZFM_A_AMT_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(colLeft - 0.5f * smallKnobDX, smallKnobY)), module, DrumVoice::PENV_AMT_A_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(colLeft + 0.5f * smallKnobDX, smallKnobY)), module, DrumVoice::PENV_DECAY_A_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(colLeft + 1.5f * smallKnobDX, smallKnobY)), module, DrumVoice::WAVE_A_PARAM));
        // Global ladder filter knobs (cutoff left, resonance right) centered between columns
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(midCol - smallKnobDX, ladderKnobY)), module, DrumVoice::LDR_CUTOFF_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(midCol + smallKnobDX, ladderKnobY)), module, DrumVoice::LDR_RES_PARAM));
        // Drive knob centered below ladder, with env amount/decay flanking
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(midCol, driveKnobY)), module, DrumVoice::DRIVE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(midCol - smallKnobDX, envKnobY)), module, DrumVoice::LDR_ENV_AMT_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(midCol + smallKnobDX, envKnobY)), module, DrumVoice::LDR_ENV_DECAY_PARAM));
        // Mix row: A, Ring, B centered
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(midCol - smallKnobDX, mixRowY)), module, DrumVoice::MIX_A_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(midCol, mixRowY)), module, DrumVoice::MIX_RING_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(midCol + smallKnobDX, mixRowY)), module, DrumVoice::MIX_B_PARAM));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colLeft - jackDX, jackRow1Y)), module, DrumVoice::TZFM_A_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colLeft, jackRow1Y)), module, DrumVoice::TIMBRE_A_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colLeft + jackDX, jackRow1Y)), module, DrumVoice::VOCT_A_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colLeft - jackDX, jackRow2Y)), module, DrumVoice::SYNC_A_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colLeft + jackDX, jackRow2Y)), module, DrumVoice::MORPH_A_INPUT));
        // Row 3: TZFM amt CV centered, pitch env decay/amt CV on sides
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colLeft - jackDX, jackRow3Y)), module, DrumVoice::PENV_DECAY_A_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colLeft, jackRow3Y)), module, DrumVoice::TZFM_A_AMT_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colLeft + jackDX, jackRow3Y)), module, DrumVoice::PENV_AMT_A_INPUT));
        // Row 4: Global ladder CVs centered between columns (cutoff, res, env amt, env decay)
        const float mid = (colLeft + colRight) * 0.5f;
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(mid - 1.5f * jackDX, jackRow4Y)), module, DrumVoice::LDR_CUTOFF_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(mid - 0.5f * jackDX, jackRow4Y)), module, DrumVoice::LDR_RES_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(mid + 0.5f * jackDX, jackRow4Y)), module, DrumVoice::LDR_ENV_AMT_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(mid + 1.5f * jackDX, jackRow4Y)), module, DrumVoice::LDR_ENV_DECAY_INPUT));
        // Single output centered at bottom
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec((colLeft + colRight) * 0.5f, outY)), module, DrumVoice::OUT_A_OUTPUT));

        // Voice B (right column)
        addParam(createParamCentered<RoundHugeBlackKnob>(mm2px(Vec(colRight, knobY1)), module, DrumVoice::FREQ_B_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(colRight, knobY2)), module, DrumVoice::TIMBRE_B_PARAM));
        // Range and Octave controls removed
        // Single-row small knobs per voice: TZFM amt, PENV amt, PENV decay, Wave morph
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(colRight - 1.5f * smallKnobDX, smallKnobY)), module, DrumVoice::TZFM_B_AMT_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(colRight - 0.5f * smallKnobDX, smallKnobY)), module, DrumVoice::PENV_AMT_B_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(colRight + 0.5f * smallKnobDX, smallKnobY)), module, DrumVoice::PENV_DECAY_B_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(colRight + 1.5f * smallKnobDX, smallKnobY)), module, DrumVoice::WAVE_B_PARAM));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colRight - jackDX, jackRow1Y)), module, DrumVoice::TZFM_B_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colRight, jackRow1Y)), module, DrumVoice::TIMBRE_B_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colRight + jackDX, jackRow1Y)), module, DrumVoice::VOCT_B_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colRight - jackDX, jackRow2Y)), module, DrumVoice::SYNC_B_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colRight + jackDX, jackRow2Y)), module, DrumVoice::MORPH_B_INPUT));
        // Row 3: TZFM amt CV centered, pitch env decay/amt CV on sides
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colRight - jackDX, jackRow3Y)), module, DrumVoice::PENV_DECAY_B_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colRight, jackRow3Y)), module, DrumVoice::TZFM_B_AMT_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colRight + jackDX, jackRow3Y)), module, DrumVoice::PENV_AMT_B_INPUT));
        // (global ladder CVs already added above)
        // removed second output

        // Per-voice pitch triggers near top (B is normalled from A)
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colLeft, 10.0f)), module, DrumVoice::PITCH_TRIG_A_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colRight, 10.0f)), module, DrumVoice::PITCH_TRIG_B_INPUT));
    }
};

Model* modelDrumVoice = createModel<DrumVoice, DrumVoiceWidget>("drumvoice");
