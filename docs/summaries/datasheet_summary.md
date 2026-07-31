# Summary — LDC1312/LDC1314 Datasheet (SNOSCZ0A)

Full conversion: `docs/LDC1314_datasheet.md`.

## Purpose

Primary technical reference for the device: electrical specs, register map, functional modes, I2C protocol, and reference application/formulas. This is the authoritative source for the register map and should be preferred over the app notes wherever they overlap.

## Important registers (26 total, all 16-bit)

- **Measurement data**: `DATA0..DATA3` (0x00/0x02/0x04/0x06) — 12-bit conversion result + 4 per-channel error flags in the MSBs.
- **Per-channel config** (×4 each): `RCOUNTx` (0x08–0x0B, conversion time), `OFFSETx` (0x0C–0x0F), `SETTLECOUNTx` (0x10–0x13, settling time), `CLOCK_DIVIDERSx` (0x14–0x17, FIN/FREF dividers), `DRIVE_CURRENTx` (0x1E–0x21, IDRIVE + read-only INIT_IDRIVE).
- **Global config**: `CONFIG` (0x1A), `MUX_CONFIG` (0x1B), `RESET_DEV` (0x1C).
- **Error/status**: `ERROR_CONFIG` (0x19), `STATUS` (0x18).
- **ID**: `MANUFACTURER_ID` (0x7E, =0x5449), `DEVICE_ID` (0x7F, =0x3054).

Full field-level detail in `register_map.md` and `docs/LDC1314_datasheet.md` §7.6.

## Important configuration parameters

- I2C address: 0x2A (ADDR pin low) or 0x2B (ADDR pin high); 400 kHz max, 16-bit registers via repeated-start.
- Functional modes: Startup → Sleep (config) → Normal (conversion) → Shutdown, controlled via `CONFIG.SLEEP_MODE_EN` and the SD pin. **Registers can only be written in Sleep Mode**, not in Normal/active mode — this is a hard sequencing constraint for the driver's init/reconfigure logic.
- Reference clock: internal (~43.4 MHz, `REF_CLK_SRC=0`) or external CLKIN up to 40 MHz (`REF_CLK_SRC=1`).
- Multi-channel (`AUTOSCAN_EN=1`, up to 4 channels sequenced per `RR_SEQUENCE`) vs. single-channel continuous (`AUTOSCAN_EN=0`, selects one channel via `CONFIG.ACTIVE_CHAN`).
- Single-channel mode caps fREF at 35 MHz vs. 40 MHz for multi-channel.

## Important formulas

- Conversion time: `tCx = (RCOUNTx × 16 + 4) / fREFx`
- Settling time: `tSx = (SETTLECOUNTx × 16) / fREFx`, with constraint `SETTLECOUNTx ≥ Q × fREFx / (16 × fSENSORx)`
- Channel switch delay (multi-channel): `692 ns + 5/fREF`
- Sensor frequency from conversion result: `fSENSORx = FIN_DIVIDERx × fREFx × (DATAx / 2^(12+OUTPUT_GAIN) + OFFSETx / 2^16)`
- IDRIVE-to-amplitude: `VOSC = 4·RP·IDRIVE/π` (see `drive_configuration_summary.md`)

## Design recommendations

- Configure while in Sleep Mode; exit Sleep Mode last to start conversions (see recommended register-write sequences in §8.2.5 — write `CONFIG` last).
- Deglitch filter (`MUX_CONFIG.DEGLITCH`): set to the lowest of {1/3.3/10/33 MHz} that exceeds the max active sensor frequency.
- For normal (non-prototyping) operation: `RP_OVERRIDE_EN=1` and `AUTO_AMP_DIS=1` (fixed drive current, no auto-amplitude drift).
- Bypass capacitor: 1 µF X7R close to VDD/GND; add 10 µF if supply is remote.

## Error conditions

Under-range, over-range, watchdog timeout (continuous mode only), amplitude high/low, zero count — see `status_monitoring_summary.md` for the full taxonomy and reporting paths (DATAx bits / STATUS / INTB).

## Relevant for ESPHome driver implementation

- Data readback race condition (§8.1.9): in multi-channel mode, a channel's `DATAx` is silently overwritten by its next conversion if not read in time — poll fast enough relative to the configured conversion interval, or use `UNREADCONVx`/INTB to detect loss.
- `RESET_DEV.RESET_DEV` always reads back 0 — cannot be polled as a "reset complete" flag; the driver must apply a fixed delay or rely on `DEVICE_ID`/`MANUFACTURER_ID` readback succeeding again after reset.
- `DEVICE_ID`(0x7F)=0x3054 and `MANUFACTURER_ID`(0x7E)=0x5449 are the natural "is this really an LDC1314" verification pair for driver setup (Iteration 1 in the roadmap).
