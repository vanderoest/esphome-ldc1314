# Summary — LDC1312/1314/1612/1614 Sensor Status Monitoring (SNOA959)

Full conversion: `docs/snoa959.md`.

## Purpose

Explains the three parallel error/status reporting paths (DATAx bits, STATUS register, INTB pin) and the precise semantics of each error type — essential for the driver's diagnostics/error-handling design.

## Important registers

- `STATUS` (0x18): all condition bits are **sticky except `UNREADCONVx`** — cleared only by reading `STATUS` (which also clears `ERR_CHAN` and de-asserts INTB). `ERR_CHAN` records only the **first** erroring channel since the last read; a second channel's error while the first is still latched is silently dropped from `ERR_CHAN` attribution (though its own STATUS bit still sets).
- `ERROR_CONFIG` (0x19): per-condition routing bits, independently gating (a) whether the condition appears in the relevant `DATAx` MSBs, and (b) whether it updates `STATUS`/asserts `INTB`.
- `DATAx` (0x00/0x02/0x04/0x06): error bits here are **not sticky** — auto-clear on the next good conversion of that channel, and clear immediately on read.

## Configuration parameters

- To use INTB for a condition: unmask it in `ERROR_CONFIG` **and** set `CONFIG.INTB_DIS=0`.
- `DRDY` semantics depend on mode: single-channel continuous → fires every conversion; multi-channel sequential → fires only after the *last* channel in the configured sequence.

## Error taxonomy (all gated per-type via ERROR_CONFIG)

| Error | Cause | Recommended mitigation |
|---|---|---|
| Under-range | Output would go negative after offset subtraction | Reduce `OFFSETx`, or increase `RCOUNTx` |
| Over-range | Sensor frequency exceeds reference frequency | Raise fREF / lower fSENSOR / adjust FIN_DIVIDERx / FREF_DIVIDERx |
| Watchdog timeout | Sensor stopped oscillating or <250 Hz (continuous mode only) | Check sensor connection; not valid in sequential mode — use Zero Count / amplitude instead |
| Amplitude high | VOSC > ~1.8 V | Lower IDRIVE (see `drive_configuration_summary.md`) |
| Amplitude low | VOSC < ~1.2 V | Raise IDRIVE, or hardware fault (e.g. disconnected sensor cap) |
| Zero count | No oscillations counted on sensor or reference | Increase RCOUNT/reference divider, reduce FIN_DIVIDER, or raise sensor resonant frequency |

## Design recommendations

- Combine STATUS + INTB reporting (not STATUS alone) to avoid losing a second channel's error while the first is still latched — read STATUS promptly on each INTB assertion.
- Watchdog timeout data is invalid and must be discarded — do not publish it as a sensor reading.

## Relevant for ESPHome driver implementation

- Natural mapping to ESPHome diagnostics: expose `STATUS`-derived binary_sensors / a text_sensor summarizing the last error per channel, gated behind a config flag (Iteration 6 "diagnostics" in the roadmap).
- Read order matters: reading `DATAx` clears that channel's DATAx-embedded error bits and the corresponding `UNREADCONVx`; reading `STATUS` clears the sticky STATUS bits and `ERR_CHAN`. The driver's per-update sequence should read `STATUS` (if diagnostics enabled) before/alongside `DATAx` reads, consistently, to avoid dropping channel attribution for a second simultaneous error.
