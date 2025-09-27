#include "plugin.hpp"
#include "PonyVCOEngine.hpp"
#include "PercEnvelope.hpp"
#include "RingModulator.hpp"
#include "DriveStage.hpp"
#include "LadderFilter.hpp"
#include "SvgHelper.hpp"

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
        DRIVE_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        OSC_A_OUTPUT,
        OSC_B_OUTPUT,
        RING_OUTPUT,
        MIX_OUTPUT,
        MIX_R_OUTPUT,
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
        configInput(DRIVE_INPUT, "Drive amount");
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

        const int channels = 1;
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
            // Discrete waveform selection: quantize param+CV to 0..3 and use single selection for this SIMD group
            float morphScalar = clamp(params[morphParam].getValue() + 3.f * inputs[morphIn].getPolyVoltage(c) / 10.f, 0.f, 3.f);
            int waveformSel = (int) std::round(morphScalar);

            float_4 out = engines[c / 4].process(
                args.sampleTime,
                lfoMode,
                freq,
                timbre,
                tzfmVoltage,
                inputs[syncIn].getPolyVoltageSimd<float_4>(c),
                waveformSel
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
        const int channels = 1;
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
        float_4 resonance = simd::pow(simd::clamp(float_4(res01), 0.f, 1.f), 2) * 2.5f;

        // Derive VCA gain from filter envelope (0..1), with exponential response and hard close
        float vcaGain01 = ldrEnvOut01;
        // Simple exponential mapping (square law)
        vcaGain01 = vcaGain01 * vcaGain01;
        // Hard close near zero to avoid denorms
        if (vcaGain01 < 1e-6f)
            vcaGain01 = 0.f;
        const float_4 vcaGain = float_4(vcaGain01);

        for (int c = 0; c < channels; c += 4) {
            const simd::float_4 a = voiceANorm[c / 4];
            const simd::float_4 b = voiceBNorm[c / 4];
            const simd::float_4 ringed = ring.process(a, b, 1.0f);
            const float mixA = clamp(params[MIX_A_PARAM].getValue(), 0.f, 1.f);
            const float mixB = clamp(params[MIX_B_PARAM].getValue(), 0.f, 1.f);
            const float mixRing = clamp(params[MIX_RING_PARAM].getValue(), 0.f, 1.f);
            const simd::float_4 mixNormPreDrive = a * simd::float_4(mixA) + b * simd::float_4(mixB) + ringed * simd::float_4(mixRing);
            const float driveAmt = clamp(params[DRIVE_PARAM].getValue() + inputs[DRIVE_INPUT].getNormalVoltage(0.f) / 10.f, 0.f, 1.f);
            const simd::float_4 driven = drive.process(mixNormPreDrive, args.sampleTime, driveAmt);
            const simd::float_4 mixNorm = driven;
            ladder[c / 4].setCutoff(float_4(cutoffHz));
            ladder[c / 4].setResonance(resonance);
            ladder[c / 4].process(mixNorm, args.sampleTime);
            const simd::float_4 filtered = ladder[c / 4].lowpass();
            const simd::float_4 mixScaled = 5.f * (filtered * vcaGain);
            const simd::float_4 aScaled = 5.f * a;
            const simd::float_4 bScaled = 5.f * b;
            const simd::float_4 ringScaled = 5.f * ringed;
            outputs[MIX_OUTPUT].setVoltageSimd(mixScaled, c);
            outputs[MIX_R_OUTPUT].setVoltageSimd(mixScaled, c);
            outputs[OSC_A_OUTPUT].setVoltageSimd(aScaled, c);
            outputs[OSC_B_OUTPUT].setVoltageSimd(bScaled, c);
            outputs[RING_OUTPUT].setVoltageSimd(ringScaled, c);
        }
        outputs[MIX_OUTPUT].setChannels(channels);
        outputs[MIX_R_OUTPUT].setChannels(channels);
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

struct DrumVoiceWidgetSvg : ModuleWidget, SvgHelper<DrumVoiceWidgetSvg> {
    DrumVoiceWidgetSvg(DrumVoice* module) {
        setModule(module);
        loadPanel(
            asset::plugin(pluginInstance, "res/EC1Day.svg")
        );
        // Bind screws to SVG screw hole centers
        bindChild<ThemedScrew>("circle10"); // top-left
        bindChild<ThemedScrew>("circle11"); // top-right
        bindChild<ThemedScrew>("circle12"); // bottom-left
        bindChild<ThemedScrew>("circle13"); // bottom-right
        bindParam<RoundBigBlackKnob>("k_freq_1", DrumVoice::FREQ_A_PARAM);
        bindParam<RoundBigBlackKnob>("k_freq_2", DrumVoice::FREQ_B_PARAM);
        bindParam<RoundBlackKnob>("k_timbre_1", DrumVoice::TIMBRE_A_PARAM);
        bindParam<RoundBlackKnob>("k_timbre_2", DrumVoice::TIMBRE_B_PARAM);
        bindParam<RoundBlackKnob>("k_wave_1", DrumVoice::WAVE_A_PARAM);
        bindParam<RoundBlackKnob>("k_wave_2", DrumVoice::WAVE_B_PARAM);
        bindParam<RoundBlackKnob>("k_0fm_1", DrumVoice::TZFM_A_AMT_PARAM);
        bindParam<RoundBlackKnob>("k_0fm_2", DrumVoice::TZFM_B_AMT_PARAM);
        bindParam<RoundBlackKnob>("k_expfm_1", DrumVoice::EXPFM_A_PARAM);
        bindParam<RoundBlackKnob>("k_expfm_2", DrumVoice::EXPFM_B_PARAM);
        bindParam<RoundBlackKnob>("k_pitchenv", DrumVoice::PENV_AMT_PARAM);
        bindParam<RoundBlackKnob>("k_pitchdecay", DrumVoice::PENV_DECAY_PARAM);
        bindParam<RoundBigBlackKnob>("k_cutoff", DrumVoice::LDR_CUTOFF_PARAM);
        bindParam<RoundBlackKnob>("k_resonance", DrumVoice::LDR_RES_PARAM);
        bindParam<RoundBlackKnob>("k_envamount", DrumVoice::LDR_ENV_AMT_PARAM);
        bindParam<RoundBlackKnob>("k_envdecay", DrumVoice::LDR_ENV_DECAY_PARAM);
        bindParam<RoundBlackKnob>("k_a_mix", DrumVoice::MIX_A_PARAM);
        bindParam<RoundBlackKnob>("k_b_mix", DrumVoice::MIX_B_PARAM);
        bindParam<RoundBlackKnob>("k_ring", DrumVoice::MIX_RING_PARAM);
        bindParam<RoundBlackKnob>("k_drive", DrumVoice::DRIVE_PARAM);
        bindInput<PJ301MPort>("j_trig", DrumVoice::PITCH_TRIG_INPUT);
        bindInput<PJ301MPort>("j_freq_1", DrumVoice::VOCT_A_INPUT);
        bindInput<PJ301MPort>("j_timbre_1", DrumVoice::TIMBRE_A_INPUT);
        bindInput<PJ301MPort>("j_wave_1", DrumVoice::MORPH_A_INPUT);
        bindInput<PJ301MPort>("j_0fm_1", DrumVoice::TZFM_A_AMT_INPUT);
        bindInput<PJ301MPort>("j_pitchenv", DrumVoice::PENV_AMT_INPUT);
        bindInput<PJ301MPort>("j_pitchdecay", DrumVoice::PENV_DECAY_INPUT);
        bindInput<PJ301MPort>("j_fltrenv", DrumVoice::LDR_ENV_AMT_INPUT);
        bindInput<PJ301MPort>("j_fltrdecay", DrumVoice::LDR_ENV_DECAY_INPUT);
        bindInput<PJ301MPort>("j_expfm_1", DrumVoice::EXPFM_A_INPUT);
        bindInput<PJ301MPort>("j_expfm_2", DrumVoice::EXPFM_B_INPUT);
        bindInput<PJ301MPort>("j_cutoff", DrumVoice::LDR_CUTOFF_INPUT);
        bindInput<PJ301MPort>("j_0fm_2", DrumVoice::TZFM_B_AMT_INPUT);
        bindInput<PJ301MPort>("j_wave_2", DrumVoice::MORPH_B_INPUT);
        bindInput<PJ301MPort>("j_timbre_2", DrumVoice::TIMBRE_B_INPUT);
        bindInput<PJ301MPort>("j_freq_2", DrumVoice::VOCT_B_INPUT);
        bindInput<PJ301MPort>("j_drive", DrumVoice::DRIVE_INPUT);
        bindOutput<PJ301MPort>("J_pitchenv_out", DrumVoice::PENV_OUTPUT);
        bindOutput<PJ301MPort>("j_fltrenv_out", DrumVoice::LDR_ENV_OUTPUT);
        bindOutput<PJ301MPort>("j_left_out", DrumVoice::MIX_OUTPUT);
        bindOutput<PJ301MPort>("j_right_out", DrumVoice::MIX_R_OUTPUT);
    }
    void step() override {
        SvgHelper<DrumVoiceWidgetSvg>::step();
        ModuleWidget::step();
    }
    void appendContextMenu(Menu* menu) override {
        SvgHelper<DrumVoiceWidgetSvg>::appendContextMenu(menu);
    }
};

Model* modelDrumVoice = createModel<DrumVoice, DrumVoiceWidgetSvg>("drumvoice");
