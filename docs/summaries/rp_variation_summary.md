# Summary — Configuring Inductive-to-Digital-Converters for Parallel Resistance (RP) Variation in L-C Tank Sensors (SNAA221B)

Full conversion: `docs/snaa221b.md`.

## Purpose

Explains what sensor RP physically is, how it's derived, and — specifically for the LDC1312/1314 family (§5–7 of the source; §3–4 covers LDC10xx, a different architecture not applicable here) — how the *constant* current-drive model relates to the sensor's RP range. Largely theoretical background supporting `drive_configuration_summary.md`, plus an alternate/corroborating IDRIVE table and the formal auto-calibration procedure.

## Important registers

- `DRIVE_CURRENT_CHx.CHx_IDRIVE` (bits 15:11) — the fixed drive current, same register as covered in `drive_configuration_summary.md`.
- `DRIVE_CURRENT_CHx.CHx_INIT_DRIVE` (bits 10:6, read-only) — auto-calibration readback value.
- `CONFIG.SLEEP_MODE_EN`, `CONFIG.RP_OVERRIDE_EN` — used in the auto-calibration sequence.

## Important formulas

- `RP = L / (C × RS)` (parallel resistance from series model parameters)
- `Fsensor(d) = 1 / (2π√(L(d)·C))`
- `Q = RP·√(C/L) = (1/RS)·√(L/C)`

## Design recommendations

- LDC1312/1314/1612/1614 use a **single constant** drive current per channel (unlike LDC10xx's dynamic RP_MIN/RP_MAX bracketing) — the drive must be set so oscillation amplitude stays acceptable across the *entire* RP range the target's movement produces, not just one operating point.
- Auto-calibration procedure (for finding IDRIVE when RP is unknown) is formalized here as a 9-step register sequence: Sleep → program SETTLECOUNT/RCOUNT → enable auto-cal (`RP_OVERRIDE_EN=0`) → exit Sleep → let one measurement complete at max target distance → read back `CHx_INIT_DRIVE` → write that value into `CHx_IDRIVE` for normal operation → set `RP_OVERRIDE_EN=1` to lock it in.

## Error conditions

Same as `drive_configuration_summary.md`: drive too high → ESD clamp activates, frequency shifts invalid; drive too low → SNR degrades, oscillation may stop near zero target distance (reads all-zero).

## Relevant for ESPHome driver implementation

- Confirms the auto-calibration sequence should be a documented **manual bring-up procedure** (e.g. a one-off script or a clearly-labeled "calibration mode" YAML action), not something the driver runs automatically during normal operation — consistent with `drive_configuration_summary.md`'s recommendation and CLAUDE.md's raw-driver boundary.
- The IDRIVE-vs-RP table here (32 entries) is near-identical to the one in the datasheet/§drive_configuration_summary — use the datasheet's version as authoritative if any value ever needs to be hard-coded (e.g. a default-lookup helper), and treat this one as corroborating.
