# 2026-08-21_rest-slow-fast-bucket.csv

Raw `esphome logs` output, filtered to `TRACE,` lines (esphome log prefix kept). One continuous
capture through the trace switch, covering all four Phase A scenarios back to back. Timestamps
below are wall-clock; segment boundaries were auto-detected from per-channel activity (5s bucket
min/max range) and cross-checked against the operator's own notes.

| Wall time | Duration | Test |
|---|---|---|
| 19:04:36–19:04:59 | ~24s | at rest |
| 19:05:00–19:05:59 | ~60s | slow draw |
| 19:06:00–19:06:20 | ~20s | fast draw |
| 19:06:25–19:06:30 | ~5s | tap closed / bucket refill gap |
| 19:06:35–19:08:05 | ~90s | large tap / bucket draw |
| 19:08:10+ | | at rest |

**Bucket draw:** operator-reported as ~10 L, hand-measured with a 1 L bucket emptied ~10 times on
the fly (approximate, not a precision reference), tap running "between 19:06 and 19:08, about
1 minute 33". A first-pass Clarke transform (whole-window min/max normalization, no hysteresis,
no envelope tracking -- not the real decoder) over the 19:06:33–19:08:07 window gives **10.02
revolutions**, i.e. ~10.02 L at `liters_per_revolution: 1.0` -- consistent with the approximate
10 L draw.

Last line in the CSV is truncated (capture was stopped mid-write); harmless, just drop it when
parsing.

Format: `TRACE,<millis>,<ch0>,<ch1>,<ch2>` (raw 12-bit codes), one esphome log line per row.
