# LDC1314 Driver Knowledge Base

Primary developer-oriented technical reference for the ESPHome LDC1314 external component. Cross-references all converted documentation. Do not implement watermeter/application logic based on this document — see "Driver responsibilities" below.

Sources: `docs/LDC1314_datasheet.md` (SNOSCZ0A), `docs/snaa221b.md` (SNAA221B), `docs/snoa945.md` (SNOA945), `docs/snoa950.md` (SNOA950), `docs/snoa959.md` (SNOA959), and their summaries in `docs/summaries/`.

## Register overview

Full field-level detail lives in `register_map.md`. Summary by group:

| Group | Registers | Addresses |
|---|---|---|
| Device Identification | MANUFACTURER_ID, DEVICE_ID | 0x7E, 0x7F |
| Measurement Data | DATA0–DATA3 | 0x00, 0x02, 0x04, 0x06 |
| Per-channel Configuration | RCOUNTx, OFFSETx, SETTLECOUNTx, CLOCK_DIVIDERSx, DRIVE_CURRENTx | 0x08–0x0B, 0x0C–0x0F, 0x10–0x13, 0x14–0x17, 0x1E–0x21 |
| Global Configuration | CONFIG, MUX_CONFIG, RESET_DEV | 0x1A, 0x1B, 0x1C |
| Error Handling | ERROR_CONFIG | 0x19 |
| Status | STATUS | 0x18 |

All registers are 16 bits wide, accessed via I2C repeated-start (8-bit register pointer, then 16-bit data). I2C address 0x2A (ADDR low) or 0x2B (ADDR high).

## Initialization sequence

1. Device powers up in **Sleep Mode** automatically (`CONFIG.SLEEP_MODE_EN` resets to 1).
2. Confirm identity: read `DEVICE_ID` (0x7F, expect 0x3054) and `MANUFACTURER_ID` (0x7E, expect 0x5449).
3. **All configuration registers must be written while still in Sleep Mode** — the datasheet's recommended sequences (§8.2.5) write every setting register first and write `CONFIG` (which contains `SLEEP_MODE_EN`) **last**, since it both applies remaining CONFIG bits and exits Sleep Mode in the same write.
4. Typical per-channel config to write before the final CONFIG write: `RCOUNTx`, `SETTLECOUNTx`, `CLOCK_DIVIDERSx`, `DRIVE_CURRENTx`, and `MUX_CONFIG` (channel selection / auto-scan sequence / deglitch), plus `ERROR_CONFIG` if diagnostics are wanted.
5. Final `CONFIG` write sets `SLEEP_MODE_EN=0`, exiting Sleep Mode and starting conversions. Sensor activation for the first conversion begins after `16384/fINT` elapses.
6. To change any setting later: return to Sleep Mode (`SLEEP_MODE_EN=1`), rewrite, exit Sleep Mode again. Entering Sleep Mode clears all conversion results, error conditions, and de-asserts INTB.

Reset: write `RESET_DEV.RESET_DEV=1` to force all registers to default and abort any active conversion. This bit **always reads back 0** — it cannot be polled for "reset complete"; treat the write as fire-and-forget and re-verify device state via a subsequent `DEVICE_ID` read.

Shutdown (SD pin high) is the lowest-power state but disables I2C entirely — not useful for a polling driver except as a hardware-level enable/disable, and resets all registers to default on entry.

## Measurement flow

- **Single-channel mode** (`MUX_CONFIG.AUTOSCAN_EN=0`): continuously converts one channel selected by `CONFIG.ACTIVE_CHAN`. `STATUS.DRDY` fires on every completed conversion. Max fREF 35 MHz.
- **Multi-channel / auto-scan mode** (`AUTOSCAN_EN=1`): sequences through up to 4 channels per `MUX_CONFIG.RR_SEQUENCE` (Ch0+Ch1, Ch0+Ch1+Ch2, or all 4). `DRDY` fires only after the **last** channel in the sequence completes. Max fREF 40 MHz (a 14–50% resolution advantage over single-channel — see `docs/summaries/optimizing_resolution_summary.md`).
- Each channel's result is read from its `DATAx` register (12-bit result in the low bits + 4 error flags in the high bits, per §7.6.2 in the datasheet).
- `STATUS.UNREADCONVx` flags a completed-but-unread result per channel; cleared by reading either `DATAx` or `STATUS`.
- **Data readback race condition**: in multi-channel mode, a channel's `DATAx` is silently overwritten by that channel's *next* conversion if not read in time. The driver's polling interval (`PollingComponent::update()`) must be slower than one full sequence's conversion+settling time, or must actively check `UNREADCONVx` to detect loss, to avoid publishing stale/skipped data as if it were current.
- Conversion time per channel: `tCx = (RCOUNTx×16+4)/fREFx`. Settling time: `tSx = (SETTLECOUNTx×16)/fREFx`. Multi-channel adds a `692ns + 5/fREF` channel-switch delay between channels.

## Error handling

Full taxonomy in `docs/summaries/status_monitoring_summary.md`. Summary of the three parallel reporting paths, all independently gated per-condition via `ERROR_CONFIG`:

1. **DATAx-embedded** (`*_ERR2OUT` bits) — not sticky, auto-clears on the next good conversion, cleared immediately on read.
2. **STATUS register** — unconditional per condition type (no separate enable bit; STATUS.ERR_* always updates when the condition occurs), sticky except `UNREADCONVx`, cleared only by reading STATUS. `ERR_CHAN` latches only the *first* erroring channel until STATUS is read — a second channel's error can be lost from channel attribution if STATUS isn't read promptly.
3. **INTB pin** (`*_ERR2INT` bits) — requires both the specific `ERROR_CONFIG` bit and `CONFIG.INTB_DIS=0`.

Recommended pattern (from SNOA959): read STATUS on every INTB assertion (or every poll cycle if not using INTB) to avoid losing a second-channel error to the sticky `ERR_CHAN` latch.

Error types: under-range, over-range, watchdog timeout (continuous mode only — not valid in sequential mode), amplitude high, amplitude low, zero count. See the table in `docs/summaries/status_monitoring_summary.md` for causes and mitigations. **Watchdog-timeout data is invalid and must be discarded**, never published as a sensor reading.

## Driver responsibilities (and non-responsibilities)

Per `CLAUDE.md`: this component is a **generic raw-measurement driver**. It owns:
- I2C register access, device identification, reset, mode sequencing (Sleep→Normal).
- Per-channel and global register configuration exposed as YAML options (RCOUNT, SETTLECOUNT, clock dividers, IDRIVE, offset, output gain, deglitch, error reporting config).
- Publishing raw per-channel values: the 12-bit conversion code (`DATAx`) and/or the computed frequency ratio (`fSENSORx/fREFx`, per the formula below) as ESPHome `sensor::Sensor` outputs.
- Publishing diagnostics: per-channel/global error flags, DEVICE_ID/MANUFACTURER_ID verification result.

It explicitly does **not** own: converting raw values to distance/inductance/liters/flow rate, watermeter-specific signal processing (e.g. quadrature/rotational decoding across channels), sensor/coil physical design, or any target-material/geometry-specific calibration math. Those belong in the consuming YAML (`template` sensors, `sensor.filter`, or external application code) — see `design_decisions.md` for the reasoning.

## Configuration dependencies

Several settings are interdependent; the driver's config validation (Python `CONFIG_SCHEMA`) should reflect these, and the YAML docs should explain them, even though the driver doesn't compute physical units:

- `fREFx` (used in every timing formula) = `fCLK / FREF_DIVIDERx`, where `fCLK` is either the internal oscillator or CLKIN, selected by `CONFIG.REF_CLK_SRC` — a **global**, not per-channel, choice, but `FREF_DIVIDERx` is per-channel.
- `fINx` = `fSENSORx / FIN_DIVIDERx`; `FIN_DIVIDERx` must be ≥2 if `fSENSORx ≥ 8.75 MHz`.
- Max `fREFx`: 40 MHz in multi-channel mode (internal or external clock — see the clock-frequency-requirements table in the datasheet, §8.1.6), 35 MHz in single-channel mode. This constrains valid `FREF_DIVIDERx` values given the actual `fCLK`.
- `RCOUNTx` above `0x2000` gives no additional resolution (16-bit internal quantization ceiling) — pure downside (slower conversion) beyond that point.
- `SETTLECOUNTx` has a hard minimum derived from sensor Q and `fREFx`/`fSENSORx` (formula in `docs/summaries/optimizing_resolution_summary.md`); too low causes spurious amplitude errors.
- Output Gain (`RESET_DEV.OUTPUT_GAIN`) is only valid if the un-gained signal swing stays within the corresponding fraction of full scale (100%/25%/12.5%/6.25% for gain 1/4/8/16) — must be paired with a correctly-chosen `OFFSETx` to avoid clipping.
- `IDRIVEx` must be tuned to the sensor's RP range; `RP_OVERRIDE_EN=1` + `AUTO_AMP_DIS=1` is required for a fixed (non-drifting) drive current in normal operation.

## Implementation notes

- **PDF reading in this environment**: the `Read` tool extracts full text/tables from these TI PDFs without needing `pdftoppm`/poppler (not installed here) — omit the `pages` parameter, which triggers image rendering and fails. This matters if documentation needs to be re-derived or expanded later.
- Formulas in the converted Markdown were reconstructed from the PDF's text layer (the PDFs render equations as small embedded graphics/mixed text runs) — cross-check against `docs/*.pdf` directly if a formula looks ambiguous during implementation, rather than trusting the Markdown blindly for anything safety- or calibration-critical.
- The datasheet's own worked example (§8.2.4, step 2) contains an internal inconsistency — it sets `FREF_DIVIDER0=1` (implying fREF0=40 MHz) but then computes settling time using `fREF0=20 MHz`. Flagged in `docs/LDC1314_datasheet.md`; treat as a documentation quirk, not a device constraint, when using that example as a reference init sequence.

## Open questions / documentation gaps

- **LDC Sensor Design (SNOA930)** — referenced throughout the datasheet and other app notes for physical coil/sensor design guidance, but the only local copy (`docs/SNOA930_Application_note_TI.com.html`) is a TI.com navigation-only stub with no usable content, and no replacement was fetched. This is out of scope for the driver anyway (coil design is an application/hardware concern per CLAUDE.md), but should be flagged if a future task needs it — it would need to be fetched from `ti.com` the same way the main datasheet was.
- The stale `docs/SNOSCZ0_Data_sheet_TI.com.html` and `docs/SNOA930_Application_note_TI.com.html` files remain in `docs/` alongside the now-complete `docs/LDC1314_datasheet.pdf`/`.md`. They were left in place (not deleted) per the plan; they should not be used as sources.

## References

| Doc | Literature # | Markdown | PDF |
|---|---|---|---|
| LDC1312/LDC1314 Datasheet | SNOSCZ0A | `docs/LDC1314_datasheet.md` | `docs/LDC1314_datasheet.pdf` |
| RP Variation Configuration | SNAA221B | `docs/snaa221b.md` | `docs/snaa221b.pdf` |
| Optimizing L Measurement Resolution | SNOA945 | `docs/snoa945.md` | `docs/snoa945.pdf` |
| Sensor Drive Configuration | SNOA950 | `docs/snoa950.md` | `docs/snoa950.pdf` |
| Sensor Status Monitoring | SNOA959 | `docs/snoa959.md` | `docs/snoa959.pdf` |
| LDC Sensor Design (unresolved) | SNOA930 | — | — (stub HTML only, not usable) |
