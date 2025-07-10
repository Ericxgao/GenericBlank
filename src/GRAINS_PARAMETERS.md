# Grains Module Parameters

## Audio Parameters

**Density** (0.0 - 1.0)
- Controls the probability that a grain will trigger on each clock pulse
- 0.0 = No grains trigger
- 1.0 = Grain triggers on every clock pulse
- Values in between create sparse, probabilistic grain triggering

**Duration** (0.01s - 5.0s)
- Sets how long each grain plays from the delay buffer
- Shorter durations = more percussive grains
- Longer durations = more sustained, drone-like grains

**Env Duration** (0.01s - 10.0s)  
- Controls the amplitude envelope length applied to each grain
- Independent of grain duration - can be shorter or longer
- Shorter envelopes = more clicky/percussive grains
- Longer envelopes = smoother, more ambient grains

**Speed** (-8 to +8 V/oct)
- Playback speed/pitch of grains in V/oct format
- 0V = Normal speed/pitch
- Positive values = Higher pitch/faster playback
- Negative values = Lower pitch/slower playback

**Delay** (0.0s - 10.0s)
- Time offset into the delay buffer where grains are read from
- 0s = Read from current position (most recent audio)
- Higher values = Read from further back in time
- Creates echo/delay effects when combined with live input

**Pan** (-1.0 to +1.0)
- Stereo positioning of grains
- -1.0 = Full left
- 0.0 = Center
- +1.0 = Full right

## Timing Parameters

**Time Division** (0 - 23, discrete steps)
- Sets the rhythmic division for grain triggering when clock is connected
- Includes regular, dotted, and triplet divisions
- Examples: 1/32, 1/16, 1/8, 1/4, 1/2, whole notes and their variants
- Only active when clock input is connected

**Max Grains** (1 - 64)
- Maximum number of grains that can play simultaneously
- Lower values = cleaner, less dense textures
- Higher values = thicker, more complex textures
- Uses voice stealing when limit is reached

**Jitter** (0.0 - 1.0)
- Adds random timing displacement to grain triggers
- 0.0 = Perfect timing, no randomness
- 1.0 = Up to ±100ms random timing offset
- Creates more organic, humanized timing

**Threshold** (0.0 - 1.0)
- *Currently not implemented*
- Reserved for future transient detection features

## Inputs/Outputs

**Clock Input**
- Trigger input for rhythmic grain generation
- Works with Time Division parameter for musical timing
- Can use any trigger/gate source

**Audio Input L/R**
- Live audio input that feeds the delay buffer
- Mono or stereo input supported
- Audio is continuously recorded into circular delay buffer

**Audio Output L/R**
- Processed granular audio output
- Stereo output with pan control per grain
- Volume automatically scaled based on active grain count