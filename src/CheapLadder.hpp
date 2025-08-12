#include <cmath>

struct Ladder4 {
    float fs = 48000.f;
    float z1=0, z2=0, z3=0, z4=0;  // integrator states
    float lastY4 = 0.f;            // for feedback HP (optional)
    float hpZ = 0.f;               // feedback HP state (per instance)

    static inline float fast_tanh(float x) {
        // Use proper bounded tanh to avoid HF growth and instability near Nyquist
        return tanhf(x);
    }

    // kBassComp: [0..1], how much low-end restoration you want
    // fbHPHz:    0 disables; else a small HP in the feedback to preserve bass
    float process(float x, float cutoff, float resonance, float drive,
                  float kBassComp = 0.35f, float fbHPHz = 0.0f)
    {
        // Prewarp with safety clamps to avoid tan(pi/2) singularity
        const float nyquistSafe = 0.49f * fs;
        const float cutoffHz = fminf(fmaxf(cutoff, 0.0f), nyquistSafe);
        const float gRaw  = tanf(3.1415926535f * (cutoffHz / fs));
        // Clamp both lower and upper bounds to avoid denorms and infinities
        const float gg = fminf(fmaxf(gRaw, 1e-9f), 1e3f);
        // Proper TPT one-pole uses G = g / (1 + g)
        const float G = gg / (1.f + gg);

        // Feedback path: optional HP to avoid “sucking” bass at high R
        float y4fb = (fbHPHz <= 0.f) ? lastY4
                                     : feedbackHP(lastY4, fbHPHz);

        // Input nonlinearity (cheap saturation)
        float in = drive * (x - resonance * y4fb);
        if (!std::isfinite(in)) in = 0.f;
        float u = fast_tanh(in);

        // 4 cascaded ZDF 1-poles
        float v, y;

        v = (u   - z1) * G; y = v + z1; z1 = y + v; // stage 1
        v = (y   - z2) * G; y = v + z2; z2 = y + v; // stage 2
        v = (y   - z3) * G; y = v + z3; z3 = y + v; // stage 3
        v = (y   - z4) * G; y = v + z4; z4 = y + v; // stage 4

        // State sanitation: recover from non-finite values
        if (!std::isfinite(y) || !std::isfinite(z1) || !std::isfinite(z2) ||
            !std::isfinite(z3) || !std::isfinite(z4))
        {
            z1 = z2 = z3 = z4 = 0.f;
            y = 0.f;
        }

        float y4 = y;
        lastY4 = y4;

        // --- Bass compensation options ---
        // A) Simple psycho shelf: add a bit of (x - y4). This restores lows and
        //    leaves the resonant peak mostly intact because (x - y4) ~ HP.
        float y_comp = y4 + kBassComp * resonance * (x - y4);

        // B) Exact DC fix (optional): dry feedforward scaled so DC gain ~ 1.
        //    Use a lowpassed dry (e.g., y1 from stage 1) if you want less mid lift.
        // float dcFix = (resonance / (1.f + resonance)) * x;
        // float y_comp = y4 + dcFix;

        return y_comp;
    }

    // One-pole HP for feedback path
    float feedbackHP(float s, float hpHz) {
        float a = expf(-2.f * 3.1415926535f * hpHz / fs);
        // Clamp coefficient to [0,1] and sanitize state
        a = fminf(fmaxf(a, 0.f), 1.f);
        if (!std::isfinite(hpZ)) hpZ = 0.f;

        float y = s - hpZ + a * hpZ;
        if (!std::isfinite(y)) {
            hpZ = 0.f;
            return 0.f;
        }
        hpZ = s + a * y;
        if (!std::isfinite(hpZ)) hpZ = 0.f;
        return y;
    }
};
