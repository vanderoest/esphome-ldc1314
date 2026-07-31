# Summary — Setting LDC1312/4, LDC1612/4, and LDC1101 Sensor Drive Configuration (SNOA950)

Full conversion: `docs/snoa950.md`.

## Purpose

Explains how to choose the per-channel `IDRIVE` sensor current-drive setting so oscillation amplitude stays in the optimal 1.2 Vp–1.8 Vp range, and why/how to keep it fixed during normal operation.

## Important registers

- `DRIVE_CURRENT_CHx` / `DRIVE_CURRENTx` (0x1E–0x21): `IDRIVEx` field (bits 15:11, R/W) sets the fixed drive current per channel.
- `CONFIG` (0x1A): `RP_OVERRIDE_EN`, `AUTO_AMP_DIS`, `HIGH_CURRENT_DRV` (channel 0 only, single-channel mode only).

## Important configuration parameters

- IDRIVE: 32 steps, 16 µA (code 0) to 1.56 mA (code 31) — see full table in `docs/snoa950.md` §1, duplicated (near-identically) in the datasheet §8.1.5 and in `rp_variation_summary.md`.
- Recommended setting: **highest IDRIVE for which VOSC < 1.8 Vp**, measured at the sensor's maximum operating target distance (worst-case lowest RP → lowest amplitude headroom happens close to the sensor, but tuning is done at max distance to bound the upper amplitude limit).
- `HIGH_CURRENT_DRV`: doubles Channel-0-only max drive to 3 mA, for RP <350 Ω sensors; requires single-channel mode.

## Important formula

`VOSC = 4·RP·IDRIVE / π`

## Design recommendations

- **Do not** use automatic amplitude control (`AUTO_AMP_DIS=0`) during normal operation — it can introduce a mid-measurement current-drive change that shows up as a position-offset step. Use it only for one-time prototyping/calibration.
- Normal operation: `RP_OVERRIDE_EN=1`, `AUTO_AMP_DIS=1` (fixed IDRIVE from config, no drift).
- Determining IDRIVE without knowing RP: sweep IDRIVE from 31 downward while watching amplitude on an oscilloscope at max target distance, until VOSC < 1.8 Vp — the standard bring-up procedure to document for hardware validation.
- Multiple channels sharing identical sensor hardware: use the *same* IDRIVE across them for consistent measurements; if per-channel tuning gives slightly different optimal values, use the lowest of the set.

## Error conditions

Exceeding VOSC=1.8 V activates the internal ESD clamp, shifting sensor frequency into an invalid reading. Setting IDRIVE too low degrades SNR and can stop oscillation entirely as target distance approaches zero (reads as all-zeros / a `STATUS.ERR_ZC` / amplitude-low condition, see `status_monitoring_summary.md`).

## Relevant for ESPHome driver implementation

- `idrive` should be an exposed per-channel YAML config option (numeric 0–31, or a computed default from a user-supplied `rp` estimate using the table). Auto-calibration (§8.1.5.2 in the datasheet / §6 in `docs/snaa221b.md`) is a *bring-up tool*, not something the driver should run automatically in production — matches CLAUDE.md's "no hidden magic, expose raw config" spirit.
