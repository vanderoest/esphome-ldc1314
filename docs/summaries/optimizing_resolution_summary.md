# Summary — Optimizing L Measurement Resolution for the LDC1312 and LDC1314 (SNOA945)

Full conversion: `docs/snoa945.md`.

## Purpose

Explains the tradeoffs between `RCOUNT`, reference frequency, and Output Gain that determine the *effective* (not just nominal 12-bit) measurement resolution, and clocking pitfalls (injection locking, oscillator choice).

## Important registers

- `RCOUNTx` (0x08–0x0B) — primary resolution lever; conversion time `tC = (RCOUNT×16)/fREF`.
- `CLOCK_DIVIDERSx` (0x14–0x17) — `FREF_DIVIDERx` trades sample rate for resolution by lowering fREF relative to fSENSOR.
- `RESET_DEV.OUTPUT_GAIN` (0x1C, bits 10:9) + `OFFSETx` (0x0C–0x0F) — Output Gain shifts additional internal LSBs into the visible 12-bit word (up to 16-bit effective), paired with an offset to avoid clipping.

## Important configuration parameters

- `RCOUNT` valid range 3–65535; values above `0x2000` give **no further resolution** (16-bit internal quantization ceiling) — only slow the conversion down. Do not let user config exceed this without at least a warning/clamp consideration.
- Max fREF: 40 MHz multi-channel, 35 MHz single-channel (14–50% resolution penalty if forced to 35/20 MHz — see the single-channel work-around below).
- Deglitch filter and IDRIVE also affect *effective* resolution indirectly (SNR), covered in `drive_configuration_summary.md`.

## Important formulas

- `tCONVERSION = (RCOUNT × 16) / fREF`
- Effective resolution ≈ `log2(sensor_frequency_swing_in_Hz / Hz_per_LSB)`
- Output Gain: gain 4/8/16 ↔ +2/+3/+4 effective bits, valid only if the un-gained signal swing is ≤25%/12.5%/6.25% of full scale respectively (must pair with an `OFFSETx` to avoid clipping — worked example achieves 6.5→10.5 ENOB via gain=16 + offset).

## Design recommendations

- General rule: use the **highest** supported reference frequency, then set `RCOUNT` to hit the required sample rate (not the other way around).
- Single-channel-mode fREF cap work-around: attach a throwaway "dummy" sensor to a second channel (minimum `RCOUNT=3`, `SETTLECOUNT=1`) purely to unlock multi-channel mode's 40 MHz fREF ceiling, discarding that channel's data — effective for sensors up to several MHz depending on Q.
- Injection resonance locking: avoid CLKIN frequencies whose low odd integer dividers (÷3/÷5/÷7) land near the sensor's operating frequency range; keep CLKIN trace impedance controlled and away from sensor traces.
- Internal oscillator is fine unless RCOUNT would need to go below 128 for the application's target resolution (rare, exceeds practical throughput) — external oscillator only needed for multi-device matching or best long-term stability.

## Relevant for ESPHome driver implementation

- `output_gain` and per-channel `offset` are natural advanced YAML config options (Iteration 7 "configuration options"), not needed for the v1 raw-readout path.
- The driver should document (not silently clamp) the `RCOUNT > 0x2000` "no benefit" note in YAML schema comments/validation, per CLAUDE.md's "raw measurements only" boundary — surfacing configuration guidance is fine, computing physical units is not.
