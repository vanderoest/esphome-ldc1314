# Setting LDC1312/4, LDC1612/4, and LDC1101 Sensor Drive Configuration

Texas Instruments Application Report, literature number SNOA950, April 2016. Author: Ben Kasemsadeh.

Source: `docs/snoa950.pdf`.

## Abstract

TI's multichannel inductance-to-digital converters (LDCs) LDC1612, LDC1614, LDC1312, and LDC1314 feature an adjustable sensor current-drive to set the sensor amplitude. The LDC1101 (a high-speed device) has an equivalent set of controls. The optimal current-drive is sensor-dependent and based on the sensor parallel resistance at resonance, RP. A sensor with lower RP needs higher current-drive than one with higher RP. This note explains how to select an appropriate IDRIVE setting by analyzing the sensor signal with an oscilloscope — usually the most straightforward method.

## 1. Why Do Different Sensors Need Different IDRIVE Settings?

Sensor electrical models: series (RS, C, L) or equivalent parallel (RP, C, L).

```
RP = L / (C × RS)
```
where RP = equivalent parallel resistance at resonance, L = coil inductance, C = sensor capacitance (incl. parasitic), RS = series resistance of the inductor at resonance.

Small coils / wide traces → lower RS → higher RP, and vice versa. A sensor with lower RP needs higher current-drive to maintain a given oscillation amplitude.

Peak sensor oscillation voltage:
```
VOSC = 4·RP·IDRIVE / π
```

Each channel's IDRIVE is fixed and independently configurable, 16 µA (IDRIVE=0) to 1.56 mA (IDRIVE=31). Higher current increases oscillation amplitude.

**CHx_IDRIVE values for maximum measured RP** (32 steps)

| RP (kΩ) | IDRIVEx Field Value | Nominal Current (µA) |
|---|---|---|
| 90.0 | 0 (b00000) | 16 |
| 77.6 | 1 (b00001) | 18 |
| 66.9 | 2 (b00010) | 20 |
| 57.6 | 3 (b00011) | 23 |
| 49.7 | 4 (b00100) | 28 |
| 42.8 | 5 (b00101) | 32 |
| 36.9 | 6 (b00110) | 40 |
| 31.8 | 7 (b00111) | 46 |
| 27.4 | 8 (b01000) | 52 |
| 23.6 | 9 (b01001) | 59 |
| 20.4 | 10 (b01010) | 72 |
| 17.6 | 11 (b01011) | 82 |
| 15.1 | 12 (b01100) | 95 |
| 13.0 | 13 (b01101) | 110 |
| 11.2 | 14 (b01110) | 127 |
| 9.69 | 15 (b01111) | 146 |
| 8.36 | 16 (b10000) | 169 |
| 7.20 | 17 (b10001) | 195 |
| 6.21 | 18 (b10010) | 212 |
| 5.35 | 19 (b10011) | 244 |
| 4.61 | 20 (b10100) | 297 |
| 3.98 | 21 (b10101) | 342 |
| 3.43 | 22 (b10110) | 424 |
| 2.95 | 23 (b10111) | 489 |
| 2.55 | 24 (b11000) | 551 |
| 2.20 | 25 (b11001) | 635 |
| 1.89 | 26 (b11010) | 763 |
| 1.63 | 27 (b11011) | 880 |
| 1.40 | 28 (b11100) | 1017 |
| 1.21 | 29 (b11101) | 1173 |
| 1.05 | 30 (b11110) | 1355 |
| 0.90 | 31 (b11111) | 1563 |

> **Note:** For LDC1312/1314/1612/1614, the preferred IDRIVE setting is the highest value for which VOSC < 1.8 Vp.

## 2. Why Is the Correct Sensor Amplitude Important?

Best measurement accuracy: VOSC between 1.2 Vp and 1.8 Vp.
- VOSC > 1.8 Vp → reduced accuracy over temperature (internal architecture effect).
- VOSC < 1.2 Vp → SNR degrades.
- VOSC < ~0.5 Vp → sensor oscillation may become unstable, LDC cannot measure inductance.

Staying below 1.8 Vp matters more than staying above 1.2 Vp. Amplitude decreases as target approaches (RP decreases), so measure/tune at the maximum target distance in the system (free air if no target is always present).

## 3. Methods for Determining an Appropriate IDRIVE Setting

1. Measure sensor RP directly with a network/impedance analyzer (see SNOA936).
2. Adjust IDRIVE until no amplitude warning flags (ERR_AHE, ERR_ALE) are indicated.
3. Use automatic amplitude control during prototyping to let the LDC select CHx_IDRIVE (see SNAA221).
4. Use an oscilloscope to measure oscillation amplitude directly.

## 4. Why Not Use the Automatic Amplitude Setting?

Recommended for one-time system configuration only. If enabled during normal operation, the LDC may adjust current mid-measurement, introducing an offset that appears as a step in target position. For normal operation: `RP_OVERRIDE_EN=1` and `AUTO_AMP_DIS=1`, forcing use of the fixed IDRIVE register value — ensures the same current drive on every power-up regardless of target distance.

## 5. How to Set IDRIVE

Write directly to the `DRIVE_CURRENT_CHx` registers, or via the EVM GUI ("Current Drive and Power" panel, per-channel Idrive code/current fields).

## 6. Procedure for Determining an Appropriate Current-Drive (Oscilloscope Method)

1. Move target to expected maximum distance from sensor (normal operating condition).
2. Set IDRIVE to 31 (max); if RP is roughly known, start closer to the estimated table value.
3. Measure oscillation amplitude relative to ground at the INAx pin.
4. Reduce IDRIVE until VOSC < 1.8 Vp.

Example from the app note: IDRIVE=25 gave VOSC=3.3 Vp (too high, not recommended); reducing to IDRIVE=19 brought VOSC below 1.8 V (recommended). Reducing target distance decreases measured amplitude — not a concern if system accuracy specs are still met.

## 7. High Current Sensor Drive Mode

`HIGH_CURRENT_DRV`, Channel 0 only, ignores the normal IDRIVE setting: doubles max drive current from the normal 1.5 mA to 3 mA. Useful for very small/low-RP sensors. Requires `AUTOSCAN_EN=0` (single-channel mode).

## 8. How to Use Multiple Sensors

Different sensor components per channel → evaluate/tune each channel individually. Same sensor characteristics across channels (multi-target, differential, or reference-sensor use cases) → use the same IDRIVE across those channels for consistency. If per-channel tuning yields different optimal IDRIVE values for otherwise-identical sensors, use the **lowest** IDRIVE across those channels (example: channels tuned to 14/13/13/14 → use 13 for all four).

## 9. LDC1101 Sensor Amplitude Control

Not directly applicable to LDC1314, included for completeness: the LDC1101's LHR mode is a single-channel 24-bit equivalent of the LDC1612. Its automatic amplitude control increases measurement noise; disable via `LOPTIMAL`/`DOK_REPORT=1` (see LDC1101 datasheet §9.1.10). Equivalent control to IDRIVE is `RPMIN`.

**LDC1101 RPMIN sensor drive settings**

| RPMIN | Sensor Drive (µA) | Min Sensor RP (kΩ) | Max Sensor RP (kΩ) |
|---|---|---|---|
| b111 | 600 | 0.53 | 1.65 |
| b110 | 300 | 1.1 | 3.3 |
| b101 | 150 | 2.1 | 6.5 |
| b100 | 75 | 4.2 | 13.1 |
| b011 | 37.5 | 8.4 | 26.2 |
| b010 | 18.7 | 16.9 | 52.4 |
| b001 | 9.4 | 33.9 | 105 |
| b000 | 4.7 | 67.9 | 209 |

> Note: For LDC1101, preferred RPMIN setting is the highest value for which VOSC < 1.25 Vp.

## 10. Summary

Knowing RP is not strictly necessary — the optimal IDRIVE can also be found by direct oscilloscope amplitude measurement at maximum target distance. Recommendation:
- LDC1312/1314/1612/1614: set IDRIVE to the highest value giving VOSC ≤ 1.8 Vp.
- LDC1101 (LHR mode): set RPMIN to the highest value giving VOSC ≤ 1.25 Vp.

## 11. Additional Resources

- LDC1612/LDC1614 datasheet (SNOSCY9)
- LDC1312/LDC1314 datasheet (SNOSCZ0) — see `docs/LDC1314_datasheet.md`
- LDC1101 datasheet (SNOSD01)
- TI inductive-sensing portfolio / WEBENCH Inductive Sensing Designer

---

*Omitted: copyright/footer lines, TI Important Notice/legal disclaimer.*
