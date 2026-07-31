# Configuring Inductive-to-Digital-Converters for Parallel Resistance (RP) Variation in L-C Tank Sensors

Texas Instruments Application Report, literature number SNAA221B, April 2015 – Revised November 2019. Author: Natallia Holubeva.

Source: `docs/snaa221b.pdf`. Covers LDC100x, LDC1041, LDC1051, LDC1312, LDC1314, LDC1612, LDC1614 — this conversion focuses on the LDC1312/1314-relevant sections (§5–7); LDC10xx-specific RP_MIN/RP_MAX configuration (§3–4) is included for context since it's referenced by the "why" of the LDC131x/161x approach, but is not applicable to the LDC1314 driver.

## Abstract

Reviews sensor RP configuration for TI's LDC devices. Understanding how to set RP-related registers matters for both RP measurement (LDC10xx) and optimum L measurement (LDC131x/LDC161x). RP configuration differs fundamentally between the two device families:
- **LDC131x/LDC161x** (including LDC1312/LDC1314) do **not** measure RP, but still must be configured to accommodate RP variation as the target moves — via a single constant **drive current** setting.
- **LDC10xx** use `RP_MIN`/`RP_MAX` settings to bound injected energy and directly measure the dissipated energy, producing proximity values as output.

## 1. What Is RP and How Is It Derived?

An LC resonator ideally has no resistive component; real inductors have parasitic series resistance (RS) dependent on conductor profile, operating frequency, and geometry — modeled as a series RS-L-C circuit, with RS and L both functions of target distance/proximity.

Norton-equivalent parallel model at resonance: a distance-dependent inductor L(d), a fixed capacitor C, and a distance-dependent parallel resistor RP(d) (the resonant impedance). Note: RS and RP are **AC** resistances (frequency-dependent via skin effect), not DC resistances.

```
RP = L / (C × RS)
```

As the target approaches, eddy currents in it intensify, opposing the sensor's field more strongly — RP and the observed inductance both **decrease** as the target moves closer, which is observed as an **increase** in resonant frequency:

```
Fsensor(d) = 1 / (2π√(L(d)·C))
```

LDC10xx devices detect this by measuring how much energy they must inject to sustain oscillation; the change is a function of target shape/size/composition, and measuring both the RP change and the inductance change can distinguish metal composition.

## 2. Determining System RP

1. Third-party air coil: use Q, LS, RS from its datasheet at the operating frequency, apply `RP = L/(C·RS)`. Gives the highest expected RP (no target present).
2. WEBENCH-designed PCB coil: RP is reported directly by WEBENCH (max expected value).
3. Custom PCB coil (not WEBENCH): measure Q, LS, RS with an impedance analyzer.

## 3. Meeting RP Boundary Conditions of LDC100x, LDC1041, and LDC1051 *(not applicable to LDC1314 — background only)*

LDC10xx devices regulate oscillation amplitude via two programmable current-drive values, `RP_MIN` (register 0x01) and `RP_MAX` (register 0x02): driving RP_MIN injects more current (higher amplitude); once amplitude exceeds the RP_MAX threshold, drive current is reduced. Voltage amplitude can be fixed at 1/2/4 Vpp (4 Vpp generally recommended for best ENOB; lower settings reduce current consumption). Power relationship: `P=VI` or `P=V²/R`, so RP_MIN sets max power/min resistance, RP_MAX sets min power/max resistance (`IMAX = V/RMIN`).

## 4. Setting RP_MAX and RP_MIN for LDC10xx *(not applicable to LDC1314 — background only)*

RP must stay within [RP_MIN, RP_MAX] or the measurement clips; for best resolution, this range should be the *minimum* range covering the sensor's actual RP variation.

### 4.1 Tuning procedure

1. Set `RP_MIN=0x3F`, `RP_MAX=0x00`.
2. Expose the coil to maximum metal coverage (closest/thickest target).
3. Reduce `RP_MIN` one code at a time (or binary search) until the RP measurement reads 20,000–30,000 codes — that's the optimal `RP_MIN`.
4. Move target to minimum exposure (farthest/thinnest).
5. Increase `RP_MAX` one code at a time until RP reads 2,000–3,000 codes, or the `RP_MIN`–`RP_MAX` ratio reaches 25–26× — that's the optimal `RP_MAX` (back off 1–2 codes if noisy).

### 4.2 Limiting cases

- Low RP (LDC10xx minimum acceptable RP = 798 Ω): add a series inductor. Quality factor `Q = RP·√(C/L) = (1/RS)·√(L/C)` — low RP means low Q, and low-Q coils are less noise-immune.
- Saturated RP (sensor RP exceeds programmed `RP_MAX`): amplitude exceeds max range, device enters an invalid state. Fix by raising `RP_MAX`, or redesigning the sensor for lower RP (lower L or higher C).

## 5. Meeting RP Boundary Conditions of LDC1312, LDC1314, LDC1612, and LDC1614

Although the LDC131x/161x family doesn't measure RP, correct configuration still requires accounting for its effect on sensor performance. Supported current drive covers RP from 1 kΩ to 100 kΩ. Unlike LDC10xx, these devices use a **constant** current drive (not dynamically varied) — configured per-channel via `DRIVE_CURRENT_CHx`. Because the drive is constant, it must be set so the sensor oscillation amplitude stays in a usable range across the entire RP range the target movement produces. Max allowable amplitude is 1.8 V; the practical minimum is application-dependent (SNR degrades toward a few hundred mV, and oscillation can stop entirely as target-to-sensor distance approaches zero).

## 6. Setting the Current Drive for LDC1312, LDC1314, LDC1612, and LDC1614

If sensor RP is known, look up the corresponding current-drive code (values below target ~1.65 V oscillation amplitude; approximate to the *lower* RP value if the actual RP falls between table entries). Write the decimal/hex value into `DRIVE_CURRENT_CHx.CHx_IDRIVE`.

**Table 1 — Current drive as a function of parallel resistance**

| RP (kΩ) | Current Drive (decimal) |
|---|---|
| 89.99 | 0 |
| 77.59 | 1 |
| 66.87 | 2 |
| 57.63 | 3 |
| 49.67 | 4 |
| 42.83 | 5 |
| 36.91 | 6 |
| 31.81 | 7 |
| 27.42 | 8 |
| 23.64 | 9 |
| 20.37 | 10 |
| 17.56 | 11 |
| 15.14 | 12 |
| 13.05 | 13 |
| 11.25 | 14 |
| 9.69 | 15 |
| 8.36 | 16 |
| 7.2 | 17 |
| 6.21 | 18 |
| 5.35 | 19 |
| 4.61 | 20 |
| 3.98 | 21 |
| 3.43 | 22 |
| 2.95 | 23 |
| 2.55 | 24 |
| 2.19 | 25 |
| 1.89 | 26 |
| 1.63 | 27 |
| 1.4 | 28 |
| 1.21 | 29 |
| 1.05 | 30 |
| 0.9 | 31 |

*(Note: this table's values are very close to, but not bit-identical to, the IDRIVE table in `docs/snoa950.md`/the datasheet §8.1.5 — both describe the same physical IDRIVEx field; treat the datasheet's table as authoritative for driver defaults and this one as corroborating.)*

### Auto-calibration procedure (unknown RP)

1. Set target at the maximum planned operating distance.
2. Enter Sleep Mode: `CONFIG.SLEEP_MODE_EN=b1`.
3. Program the desired `SETTLECOUNT`/`RCOUNT` for the channel.
4. Enable auto-calibration: `RP_OVERRIDE_EN=b0`.
5. Exit Sleep Mode: `CONFIG.SLEEP_MODE_EN=b0`.
6. Allow at least one measurement with the target held fixed at max distance.
7. Read the resulting drive value from `DRIVE_CURRENT_CHx.CHx_INIT_DRIVE` (bits 10:6) and save it.
8. At normal startup, write the saved value into `CHx_IDRIVE` (bits 15:11).
9. For normal operation, set `RP_OVERRIDE_EN=b1` to force the fixed drive value.

If the resulting oscillation amplitude exceeds 1.8 V, the internal ESD clamp activates and can shift the sensor frequency into an invalid state. If drive is set too low, SNR degrades and oscillation may stop entirely near zero target distance (output reads all zeros).

## 7. Conclusion

LDC10xx devices convert measured energy dissipation directly into proximity values via RP_MIN/RP_MAX. The inductance-only LDC1312/LDC1314/LDC1612/LDC1614 instead use the *maximum expected* RP to select a single constant sensor drive current, which sets the oscillation amplitude and thus the energy the IC injects into the resonator — RP values here bound the drive-current *selection*, not a live measurement.

---

*Omitted: copyright/footer lines, Revision History, TI Important Notice/legal disclaimer.*
