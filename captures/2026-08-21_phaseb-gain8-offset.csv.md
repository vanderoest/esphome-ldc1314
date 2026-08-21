# 2026-08-21_phaseb-gain8-offset.csv

Phase B validation capture: `output_gain: 8` + per-channel `OFFSET` (fitted just below each
channel's measured minimum ratio from the Phase A capture) + `SETTLECOUNT: 0x0020`, all applied
to `tests/watermeter-test.yaml`. Raw `esphome logs` output filtered to `TRACE,` lines.

| Wall time | Duration | Segment |
|---|---|---|
| 19:49:05–19:49:30 | 25s | at rest |
| 19:49:30–19:50:05 | 35s | pour (uninterrupted, not volume-measured) |
| 19:50:05–19:50:25 | 20s | at rest |

**Result: gain/offset tuning confirmed working.**

- Per-channel span (whole file): ch0 100-168 (68 codes), ch1 102-171 (69 codes), ch2 104-201
  (97 codes) — up from 11-16 codes at gain 1, roughly 5-7x growth. No channel clipped at 0 or
  4095; comfortable margin on both ends.
- Zero `ERR_OR` (over-range) in the log.
- At rest, each channel dithers by exactly ±1 LSB around a fixed value (e.g. ch0: 146↔147, diff
  histogram 189 down / 190 up -- symmetric, no net drift). This is quantization dither, not real
  motion: expected, and well inside the planned 10° hysteresis margin (~4-5° equivalent).
