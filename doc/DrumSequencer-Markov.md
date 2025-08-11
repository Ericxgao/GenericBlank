## DrumSequencer: Context-Conditioned (Second-Order) Learning

This module learns short-term rhythmic tendencies from its own recent hits and the other rows, and blends that into the programmed step probabilities.

### High-level behavior

- Each step has a base probability `p_base[row, step]` from the knob.
- For each row, we also estimate `P(hit | context)` where the context describes which rows hit in the previous two steps.
- On each clock:
  1. Build the row’s 8-bit context mask from the previous two steps.
  2. Get `p_ctx = P(hit | context)` via Laplace-smoothed counts.
  3. Blend: `p_eff = (1 − w) * p_base + w * p_ctx`, where `w` is the row’s Markov Weight.
  4. Draw a hit from `Bernoulli(p_eff)` and update the learned counts with decay.

### Context mask (second-order)

Per row, we track the last two steps’ hits (t−1 and t−2). For each time slice, we create a 4-bit nibble:

- bit 3: this row’s previous hit (1) or miss (0)
- bits 0..2: previous hits of the other three rows, packed in ascending row order, skipping self

Then we pack two nibbles into an 8-bit mask:

- Low nibble (bits 0..3): `t−1`
- High nibble (bits 4..7): `t−2`

This yields 256 possible contexts per row.

### Learning data structures

In `src/DrumSequencer.cpp`:

- `RowContext2Model` stores per-row counts:
  - `hitCounts[256]`: decaying counts of hits under each context
  - `missCounts[256]`: decaying counts of misses under each context
- `lastHit[4]`: whether each row hit at `t−1`
- `prevLastHit[4]`: whether each row hit at `t−2`

### Online update and smoothing

- On each step for each row, after drawing the hit, we update counts:
  - Apply exponential decay to all counts: `counts *= decayFactor` (default `0.99` per observation)
  - Increment exactly one bin (hit or miss) for the current context
- Probability query uses Laplace smoothing with `alpha` (default `1.0`):
  - Let `h = hitCounts[c]` and `m = missCounts[c]`
  - `p_ctx = (h + alpha) / (h + m + 2*alpha)`

These choices avoid overfitting in sparse contexts and allow the model to adapt over time.

### Blending with user intent

- Per-row knob: `Markov weight` in `[0, 1]`
- Effective probability at the current step:
  - `p_eff = clamp( p_base * (1 − w) + p_ctx * w , 0, 1 )`
- Intuition:
  - `w = 0`: pure programmed pattern
  - `w = 1`: pure learned behavior conditioned on recent cross-row context

### Process order and state update

To ensure consistent conditioning across rows:

1. Snapshot `lastHit` (t−1) and `prevLastHit` (t−2) for all rows before evaluating the step.
2. For each row, compute `p_eff`, draw `hit`, and stage `nextHits[row]`.
3. After all rows are processed for the step, shift history: `prevLastHit = snapshot(t−1)` and `lastHit = nextHits`.

### Persistence

- Saved state (JSON) field: `context2`
  - For each row: arrays `hit[256]`, `miss[256]`, plus `lastHit` and `prevLastHit` booleans
- Only this format is loaded (prototype: no legacy support)

### Code map

- Types and state: see `RowContext2Model`, `lastHit`, `prevLastHit`
- Probability blending: `process()` where `p_ctx` and `p_base` are mixed by `MARKOV_WEIGHT_PARAMS[row]`
- Decay and smoothing: constants near the update (`decayFactor = 0.99f`, `alpha = 1.f`)
- Persistence: `dataToJson()` and `dataFromJson()` under keys described above

### Tuning and ideas

- Expose `decayFactor` and `alpha` as params if you want slower/faster adaptation.
- Restrict or weight certain context bits (e.g., make kick influence snare more than hats) by mapping the mask or scaling contributions.
- Add a switch to select context depth: 1st vs 2nd order (fewer vs more specific contexts).

### Constraints and tradeoffs

- 256 contexts per row is a reasonable size for live learning with decay.
- Sparse contexts will converge slower; smoothing prevents pathological outputs.
- With high `w`, behavior can drift from knobs; balance with `w` and decay.
