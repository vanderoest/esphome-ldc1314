# LDC1314 Register Map

Authoritative source: TI datasheet SNOSCZ0A (`docs/LDC1314_datasheet.md`, §7.6 "Register Maps"). All registers are 16 bits wide and accessed over I2C via an 8-bit register-pointer byte followed by a 16-bit data word (MSB first, repeated-start for reads). Registers marked "LDC1314 only" are absent/reserved on the 2-channel LDC1312.

R/W legend: `R/W` = readable and writable, `R` = read-only, `W` = write-only.

## Device Identification

| Register | Address | Width | R/W | Purpose | Datasheet §§ | ESPHome driver notes |
|---|---|---|---|---|---|---|
| MANUFACTURER_ID | 0x7E | 16 | R | Fixed manufacturer ID, value `0x5449` ("TI" in ASCII). | §7.6.31 | Use together with DEVICE_ID as the setup-time "is this really an LDC1314" check (Iteration 1). Constant, never changes — safe to hard-code the expected value. |
| DEVICE_ID | 0x7F | 16 | R | Fixed device ID, value `0x3054`. | §7.6.32 | Same value on LDC1312 and LDC1314 (both share this ID) — device-family detection must come from configuration, not this register. |

## Measurement Data

| Register | Address | Width | R/W | Purpose | Datasheet §§ | ESPHome driver notes |
|---|---|---|---|---|---|---|
| DATA0 | 0x00 | 16 | R | Channel 0 conversion result (bits 11:0) + 4 error flags (bits 15:12: ERR_UR0, ERR_OR0, ERR_WD0, ERR_AE0). | §7.6.2 | Reading clears the embedded error bits (if `*_ERR2OUT` enabled) and `STATUS.UNREADCONV0`. `0x000`=under-range, `0xFFF`=over-range (with OUTPUT_GAIN=0). |
| DATA1 | 0x02 | 16 | R | Channel 1 conversion result + error flags (ERR_UR1/OR1/WD1/AE1). | §7.6.3 | Same semantics as DATA0, channel 1. |
| DATA2 | 0x04 | 16 | R | Channel 2 conversion result + error flags (ERR_UR2/OR2/WD2/AE2). LDC1314 only. | §7.6.4 | Same semantics, channel 2. Absent on LDC1312 — driver must gate channel-2/3 sensors behind a device-variant or channel-count config option. |
| DATA3 | 0x06 | 16 | R | Channel 3 conversion result + error flags (ERR_UR3/OR3/WD3/AE3). LDC1314 only. | §7.6.5 | Same semantics, channel 3. |

## Per-channel Configuration

Four instances each (channel 0–1 on LDC1312, 0–3 on LDC1314), address offset by channel index.

| Register | Address | Width | R/W | Purpose | Datasheet §§ | ESPHome driver notes |
|---|---|---|---|---|---|---|
| RCOUNT0 | 0x08 | 16 | R/W | Channel 0 reference count — sets conversion interval: `tC0=(RCOUNT0×16)/fREF0`. Reset 0x0080. | §7.6.6 | Valid 0x0005–0xFFFF (0x0000–0x0004 reserved). Values above `0x2000` give no added resolution, only slow conversion (see `docs/summaries/optimizing_resolution_summary.md`) — consider a schema-level note/warning, not a hard clamp. |
| RCOUNT1 | 0x09 | 16 | R/W | Channel 1 reference count. Reset 0x0080. | §7.6.7 | Same as RCOUNT0. |
| RCOUNT2 | 0x0A | 16 | R/W | Channel 2 reference count (LDC1314 only). Reset 0x0080. | §7.6.8 | Same as RCOUNT0. |
| RCOUNT3 | 0x0B | 16 | R/W | Channel 3 reference count (LDC1314 only). Reset 0x0080. | §7.6.9 | Same as RCOUNT0. |
| OFFSET0 | 0x0C | 16 | R/W | Channel 0 conversion frequency offset: `fOFFSET0=(OFFSET0÷2^16)×fREF0`. Reset 0x0000. | §7.6.10 | Used with Output Gain to keep the shifted output in range without clipping. |
| OFFSET1 | 0x0D | 16 | R/W | Channel 1 offset. Reset 0x0000. | §7.6.11 | Same as OFFSET0. |
| OFFSET2 | 0x0E | 16 | R/W | Channel 2 offset (LDC1314 only). Reset 0x0000. | §7.6.12 | Same as OFFSET0. |
| OFFSET3 | 0x0F | 16 | R/W | Channel 3 offset (LDC1314 only). Reset 0x0000. | §7.6.13 | Same as OFFSET0. |
| SETTLECOUNT0 | 0x10 | 16 | R/W | Channel 0 settling time before conversion starts: `tS0=(SETTLECOUNT0×16)/fREF0` (or `32/fREF0` for values 0/1). Reset 0x0000. | §7.6.14 | Must satisfy `SETTLECOUNTx ≥ Q×fREFx/(16×fSENSORx)` (rounded up) — too low causes spurious amplitude errors. |
| SETTLECOUNT1 | 0x11 | 16 | R/W | Channel 1 settling time. Reset 0x0000. | §7.6.15 | Same as SETTLECOUNT0. |
| SETTLECOUNT2 | 0x12 | 16 | R/W | Channel 2 settling time (LDC1314 only). Reset 0x0000. | §7.6.16 | Same as SETTLECOUNT0. |
| SETTLECOUNT3 | 0x13 | 16 | R/W | Channel 3 settling time (LDC1314 only). Reset 0x0000. | §7.6.17 | Same as SETTLECOUNT0. |
| CLOCK_DIVIDERS0 | 0x14 | 16 | R/W | Channel 0 dividers: `FIN_DIVIDER0` (bits 15:12) and `FREF_DIVIDER0` (bits 9:0). Reset 0x0000. | §7.6.18 | `FIN_DIVIDER0` must be ≥2 if fSENSOR0 ≥8.75 MHz; `b0000`/`0x000` are reserved (do not use) for the respective fields. |
| CLOCK_DIVIDERS1 | 0x15 | 16 | R/W | Channel 1 dividers (`FIN_DIVIDER1`, `FREF_DIVIDER1`). Reset 0x0000. | §7.6.19 | Same constraints as CLOCK_DIVIDERS0. |
| CLOCK_DIVIDERS2 | 0x16 | 16 | R/W | Channel 2 dividers (LDC1314 only). Reset 0x0000. | §7.6.20 | Same constraints as CLOCK_DIVIDERS0. |
| CLOCK_DIVIDERS3 | 0x17 | 16 | R/W | Channel 3 dividers (LDC1314 only). Reset 0x0000. | §7.6.21 | Same constraints as CLOCK_DIVIDERS0. |
| DRIVE_CURRENT0 | 0x1E | 16 | R/W (partial) | Channel 0 sensor drive: `IDRIVE0` (bits 15:11, R/W) sets fixed drive current; `INIT_IDRIVE0` (bits 10:6, R) holds the auto-calibration readback. Reset 0x0000. | §7.6.27 | `IDRIVEx` only takes effect with `RP_OVERRIDE_EN=1`. Set bits 5:0 to `b00 0000` when writing. See `docs/summaries/drive_configuration_summary.md` for the IDRIVE-vs-RP table. `INIT_IDRIVEx` is only meaningful while `RP_OVERRIDE_EN=0` (auto-amplitude mode). This driver always runs fixed-drive (`RP_OVERRIDE_EN=1`), so it never reads this field and defines no constants for it — documented here for completeness of the device's register map. |
| DRIVE_CURRENT1 | 0x1F | 16 | R/W (partial) | Channel 1 drive (`IDRIVE1`/`INIT_IDRIVE1`). Reset 0x0000. | §7.6.28 | Same as DRIVE_CURRENT0. |
| DRIVE_CURRENT2 | 0x20 | 16 | R/W (partial) | Channel 2 drive (LDC1314 only) (`IDRIVE2`/`INIT_IDRIVE2`). Reset 0x0000. | §7.6.29 | Same as DRIVE_CURRENT0. |
| DRIVE_CURRENT3 | 0x21 | 16 | R/W (partial) | Channel 3 drive (LDC1314 only) (`IDRIVE3`/`INIT_IDRIVE3`). Reset 0x0000. | §7.6.30 | Same as DRIVE_CURRENT0. |

## Global Configuration

| Register | Address | Width | R/W | Purpose | Datasheet §§ | ESPHome driver notes |
|---|---|---|---|---|---|---|
| CONFIG | 0x1A | 16 | R/W (partial) | Top-level mode/config register: `ACTIVE_CHAN` [15:14], `SLEEP_MODE_EN` [13], `RP_OVERRIDE_EN` [12], `SENSOR_ACTIVATE_SEL` [11], `AUTO_AMP_DIS` [10], `REF_CLK_SRC` [9], `INTB_DIS` [7], `HIGH_CURRENT_DRV` [6]. Reset 0x2801. | §7.6.24 | **Registers can only be changed while `SLEEP_MODE_EN=1`.** This register's write should be the *last* write in the init sequence (it both applies remaining config and, via `SLEEP_MODE_EN=0`, starts conversions) — see initialization sequence in `docs/knowledge_base.md`. |
| MUX_CONFIG | 0x1B | 16 | R/W (partial) | Channel sequencing/deglitch: `AUTOSCAN_EN` [15], `RR_SEQUENCE` [14:13], `DEGLITCH` [2:0]. Reset 0x020F. | §7.6.25 | Bits 12:3 are reserved and must be written as `00 0100 0001` — do not treat as free bits. `DEGLITCH` should default to the lowest of {1/3.3/10/33 MHz} exceeding the configured channels' max sensor frequency. |
| RESET_DEV | 0x1C | 16 | R/W (partial) | `RESET_DEV` bit [15] triggers a full device reset; `OUTPUT_GAIN` [10:9] sets the digital gain (1/4/8/16×). Reset 0x0000. | §7.6.26 | `RESET_DEV` bit **always reads back 0** — cannot be polled for completion; do not use it as a "reset done" flag. Only change `OUTPUT_GAIN` together with a correctly-scaled `OFFSETx` to avoid output clipping. |

## Error Handling

| Register | Address | Width | R/W | Purpose | Datasheet §§ | ESPHome driver notes |
|---|---|---|---|---|---|---|
| ERROR_CONFIG | 0x19 | 16 | R/W | Per-error-type routing: `*_ERR2OUT` bits [15:11] enable embedding an error in the corresponding DATAx MSBs; `*_ERR2INT` bits [7:0] enable asserting INTB **and** updating STATUS for that error type. Covers under-range, over-range, watchdog, amplitude-high, amplitude-low, zero-count, and DRDY. Reset 0x0000. | §7.6.23 | **`STATUS.ERR_*` is only updated when the matching `*_ERR2INT` bit is set** — the datasheet wording is "b0: Do not report … by asserting INTB pin *and STATUS register*". Leaving these clear makes STATUS permanently empty. The INTB *pin* additionally requires `CONFIG.INTB_DIS=0`, so the two are separable: enabling `*_ERR2INT` with `INTB_DIS=1` gives STATUS reporting without driving the pin. Zero-count has no `*_ERR2OUT` counterpart at all (SNOA959 Table 1), so `ZC_ERR2INT` is its only reporting route. |

## Status Registers

| Register | Address | Width | R/W | Purpose | Datasheet §§ | ESPHome driver notes |
|---|---|---|---|---|---|---|
| STATUS | 0x18 | 16 | R | Device/sensor status: `ERR_CHAN` [15:14] (first erroring channel), `ERR_UR/OR/WD/AHE/ALE/ZC` [13:8] (per-error-type flags), `DRDY` [6], `UNREADCONVx` [3:0] (per-channel unread-conversion flags). Reset 0x0000. | §7.6.22 | The `ERR_*` bits only populate if the matching `ERROR_CONFIG.*_ERR2INT` bit is set — see the ERROR_CONFIG row. All bits **except UNREADCONVx are sticky**, cleared only by reading STATUS (which also de-asserts INTB and clears ERR_CHAN). `ERR_CHAN` latches only the *first* erroring channel since the last read — read STATUS promptly (ideally on every INTB assertion) to avoid losing a second channel's error attribution. `UNREADCONVx` additionally clears when the corresponding `DATAx` is read. Unlike `DATAx.ERR_AE` (which OR-es the two amplitude conditions into one bit), `ERR_AHE` and `ERR_ALE` are separate here — this register is the only way to tell amplitude-high from amplitude-low. |

---

No undocumented registers are included; no descriptions were invented. All entries derive directly from `docs/LDC1314_datasheet.md` §7.6 (converted from the TI SNOSCZ0A datasheet), cross-checked against the register tables reproduced in `docs/snoa959.md` (STATUS/ERROR_CONFIG) and `docs/snoa950.md`/`docs/snaa221b.md` (DRIVE_CURRENTx/IDRIVE).
