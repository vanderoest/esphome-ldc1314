# LDC1312, LDC1314, LDC1612, LDC1614 Sensor Status Monitoring

Texas Instruments Application Report, literature number SNOA959, October 2016. Author: Ben Kasemsadeh.

Source: `docs/snoa959.pdf`.

## Abstract

TI's multichannel inductance-to-digital converters (LDCs) LDC1612, LDC1614, LDC1312, and LDC1314 feature three methods for reporting conversion status information (errors, warnings, completed conversion results): the data registers, the status registers, and the INTB pin. This note explains usage and interpretation of the reported information in detail.

## 1. Reporting Mechanisms

Errors can be reported via:
- the four MSBs in the conversion output registers `DATA_MSB_CHx` (for LDC1314: MSBs of `DATAx`),
- the `STATUS` register,
- asserting the `INTB` pin.

**Table 1 — Error and status condition reporting options**

| Condition | DATA_CHx Reporting | STATUS Register Reporting | INTB Reporting |
|---|---|---|---|
| Data ready (DRDY) | N/A | Reported | Set DRDY_2INT=1 |
| Unread conversion | N/A | Reported | N/A |
| Under-range error | Set UR_ERR2OUT=1 | Reported | Set UR_ERR2INT=1 |
| Over-range error | Set OR_ERR2OUT=1 | Reported | Set OR_ERR2INT=1 |
| Watchdog timeout error | Set WD_ERR2OUT=1 | Reported | Set WD_ERR2INT=1 |
| Amplitude high error | Set AH_ERR2OUT=1¹ | Reported | Set AH_ERR2INT=1 |
| Amplitude low warning | Set AL_ERR2OUT=1¹ | Reported | Set AL_ERR2INT=1 |
| Zero count error | N/A | Reported | Set ZC_ERR2INT=1 |

¹ If both `AH_ERR2OUT=1` and `AL_ERR2OUT=1`, the amplitude warning bit in `CHx_ERR_AE` reports a logic OR of the two.

### 1.1 Conversion Output Register Behavior and Available Reports

Supports: under-range, over-range, watchdog timeout, amplitude warnings. Any error bit set in `DATA_CHx` is cleared by reading `DATA_CHx`; if that bit also caused the STATUS error bit / INTB, those clear too. These error bits are **not sticky** — cleared automatically if the next conversion on that channel completes without the condition.

**Table 2 — DATA_CHx output per condition** (LDC1312 example; refer to LDC161x/LDC131x for exact MSB/LSB register split)

| Condition | DATA_CHx Reporting | DATA_CHx Output |
|---|---|---|
| Data ready (DRDY) | N/A | 0x0XXX |
| Unread conversion | N/A | 0x0XXX |
| Under-range error | UR_ERR2OUT=1 | 0x8000 |
| Over-range error | OR_ERR2OUT=1 | 0x4FFF |
| Watchdog timeout error | WD_ERR2OUT=1 | 0x2000 |
| Amplitude high error | AH_ERR2OUT=1 | 0x1XXX |
| Amplitude low warning | AL_ERR2OUT=1 | 0x1XXX |
| Zero count error | N/A | 0x0000 or 0x8000 |

### 1.2 Status Register Behavior and Available Reports

Supports all conditions: under-range, over-range, watchdog timeout, amplitude high, amplitude low, zero count, DRDY, unread conversion notification.

`STATUS.ERR_CHAN` records which channel reported the error — only the **first** channel to error is recorded; if a second channel errors before STATUS is read, its error/channel attribution is lost (not reported). To avoid missing errors from multiple channels, combine STATUS reporting with INTB reporting: INTB asserts on each new error, prompting a read that clears STATUS/ERR_CHAN and re-arms detection for the next error.

All STATUS bits except Unread Conversion are **sticky** — cleared only by reading STATUS (which also de-asserts INTB).

### 1.3 Reporting Errors and Status on the INTB Pin

Conditions to enable INTB reporting for a given condition:
1. Unmask it in `ERROR_CONFIG`.
2. Set `CONFIG.INTB_DIS=0`.

Supports: under-range, over-range, watchdog timeout, amplitude high, amplitude low, zero count, DRDY.

Interrupts clear on: entering Sleep Mode, power-on reset, entering Shutdown Mode (SD asserted), software reset, or an I2C read of STATUS (clears the error bits + ERR_CHAN, de-asserts INTB).

`CONFIG.INTB_DIS=1` disables INTB entirely, holding the pin high.

## 2. Reporting of Completed Conversions

### 2.1 Unread Conversion

`STATUS.CHx_UNREADCONV` (LDC1314: `UNREADCONVx`) flags a completed-but-unread conversion on channel x. Cleared by reading either the corresponding `DATAx` or the `STATUS` register. In multi-channel mode, this flag lets software identify a completed channel result without waiting for the full scan sequence to finish.

### 2.2 Data Ready Reporting

Single-channel continuous mode (`AUTOSCAN_EN=0`): DRDY fires on every completed conversion. Multi-channel/sequential mode (`AUTOSCAN_EN=1`): DRDY fires only when the **last** conversion in the configured sequence completes (e.g. both Ch0 and Ch1 for `RR_SEQUENCE=0`). Reported in `STATUS.DRDY` and/or via INTB, gated by `ERROR_CONFIG.DRDY_2INT`.

### 2.3 Reading Data Without Using DRDY or CHx_UNREADCONV

Deterministic conversion time allows fixed-interval polling instead of interrupt-driven reads, provided the microcontroller and LDC share a clock reference (see "Multi-Channel and Single Channel Operation" in the datasheet for the conversion-time formula). If clocks aren't shared, polling faster than the LDC's actual conversion time can cause repeated/duplicate reads of the same data.

## 3. Reporting of Errors and Warnings

### 3.1 Frequency Under-Range Errors

Occurs when the output code (post-offset-subtraction) would go negative. Mitigation: reduce the channel's `OFFSETx`, or increase `RCOUNTx`. Output on error: LDC1312/1314 → `DATA[11:0]=0x000`; LDC1612/1614 → `DATA[27:0]=0x0000000`. Reported via `UR_ERR2OUT` (DATAx), `STATUS.ERR_UR` bit 13, and/or INTB (`UR_ERR2INT`).

### 3.2 Frequency Over-Range Errors

Occurs when sensor frequency exceeds the reference frequency; output clamps full-scale. Mitigation: increase reference frequency, decrease sensor frequency, increase `FIN_DIVIDERx`, or decrease `FREF_DIVIDERx`. Output on error: LDC1312/1314 → `0xFFF`; LDC1612/1614 → `0xFFFFFFF`. Reported via `OR_ERR2OUT`, `STATUS.ERR_OR` bit 12, and/or INTB (`OR_ERR2INT`).

### 3.3 Watchdog Timeout Errors

Occurs in continuous conversion mode when the sensor stops oscillating, or oscillates below 250 Hz. On timeout, the LDC resets the sensor, aborts the current conversion, and attempts to restart it on the active channel; if oscillation resumes, conversion resumes and INTB de-asserts (if enabled). If not, repeated watchdog errors are issued. Data read during a watchdog event is invalid. Sensor recovery time is ~5.2 ms (LDC requires ≥1 oscillation in that window); if the conversion time is shorter than 5.2 ms, one or more Zero Count errors precede the watchdog error. Only occurs in continuous mode — in sequential mode, use Zero Count errors and amplitude warnings instead to detect a stopped oscillation. Reported via `WD_ERR2OUT`, `STATUS.ERR_WD` bit 11, and/or INTB (`WD_ERR2INT`).

### 3.4 Amplitude Warnings

Two types: amplitude-low warning and amplitude-high error, occurring when sensor amplitude is outside range at conversion start. Causes: wrong channel IDRIVE, or sensor impedance outside the LDC's drivable range. May also indicate a hardware fault (e.g. disconnected sensor capacitor). See `docs/snoa950.md` for correct sensor-drive configuration. Amplitude High reported via `AH_ERR2OUT` (DATAx, OR-ed with AL in the same bit), `STATUS.ERR_AHE` bit 10, INTB (`AH_ERR2INT`). Amplitude Low reported via `AL_ERR2OUT` (DATAx, OR-ed with AH), `STATUS.ERR_ALE` bit 9, INTB (`AL_ERR2INT`).

### 3.5 Zero Count Errors

No oscillations recorded on either the sensor channel or the reference input — indicates a stopped sensor or a stopped external clock. Also occurs if: (1) conversion time is shorter than one sensor oscillation period — increase `RCOUNTx` or the reference divider (reduces sample rate); (2) the channel's input divider (`FIN_DIVIDERx`) is too large — reduce it; (3) sensor resonant frequency is too low for the measurement — increase it. Reported via `STATUS.ERR_ZC` bit 8 and/or INTB (`ZC_ERR2INT`).

## 4. Summary of Relevant Register Tables

(Full canonical bit-field definitions live in `docs/LDC1314_datasheet.md` §7.6.2/§7.6.22/§7.6.23 — this app note's tables are the same registers presented in a status/error-focused context, using LDC1312 `DATA_CH0` as its worked example.)

### 4.1 Address 0x00, DATA_CH0 (LDC1312 example)

| Bit | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15 | CH0_ERR_UR | R | 0 | Ch0 Under-range Error Flag. Cleared by reading. |
| 14 | CH0_ERR_OR | R | 0 | Ch0 Over-range Error Flag. Cleared by reading. |
| 13 | CH0_ERR_WD | R | 0 | Ch0 Watchdog Timeout Error Flag. Cleared by reading. |
| 12 | CH0_ERR_AE | R | 0 | Ch0 Amplitude Warning. Cleared by reading. |
| 11:0 | DATA0[11:0] | R | 0x000 | Ch0 Conversion Result. |

### 4.2 Address 0x18, STATUS

| Bit | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15:14 | ERR_CHAN | R | 00 | Source channel of warning/error (first-latched). b00=Ch0, b01=Ch1, b10=Ch2 (1614/1314 only), b11=Ch3 (1614/1314 only). |
| 13 | ERR_UR | R | 0 | Under-range error since last STATUS read. |
| 12 | ERR_OR | R | 0 | Over-range error since last STATUS read. |
| 11 | ERR_WD | R | 0 | Watchdog timeout error since last STATUS read. |
| 10 | ERR_AHE | R | 0 | Amplitude high error since last STATUS read. |
| 9 | ERR_ALE | R | 0 | Amplitude low warning since last STATUS read. |
| 8 | ERR_ZC | R | 0 | Zero count error since last STATUS read. |
| 6 | DRDY | R | 0 | New conversion result ready (semantics differ single- vs multi-channel — see §2.2 above). |
| 3 | CH0_UNREADCONV | R | 0 | Unread conversion present for Ch0; read DATA_CH0 to clear. |
| 2 | CH1_UNREADCONV | R | 0 | Unread conversion present for Ch1. |
| 1 | CH2_UNREADCONV | R | 0 | Unread conversion present for Ch2 (1614/1314 only). |
| 0 | CH3_UNREADCONV | R | 0 | Unread conversion present for Ch3 (1614/1314 only). |

### 4.3 Address 0x19, ERROR_CONFIG

| Bit | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15 | UR_ERR2OUT | R/W | 0 | Report under-range errors in DATA_CHx.CHx_ERR_UR. |
| 14 | OR_ERR2OUT | R/W | 0 | Report over-range errors in DATA_CHx.CHx_ERR_OR. |
| 13 | WD_ERR2OUT | R/W | 0 | Report watchdog errors in DATA_CHx.CHx_ERR_WD. |
| 12 | AH_ERR2OUT | R/W | 0 | Report amplitude-high errors in DATA_CHx.CHx_ERR_AE (OR-ed with AL). |
| 11 | AL_ERR2OUT | R/W | 0 | Report amplitude-low warnings in DATA_CHx.CHx_ERR_AE (OR-ed with AH). |
| 7 | UR_ERR2INT | R/W | 0 | Report under-range via INTB + STATUS.ERR_UR. |
| 6 | OR_ERR2INT | R/W | 0 | Report over-range via INTB + STATUS.ERR_OR. |
| 5 | WD_ERR2INT | R/W | 0 | Report watchdog via INTB + STATUS.ERR_WD. |
| 4 | AH_ERR2INT | R/W | 0 | Report amplitude-high via INTB + STATUS.ERR_AHE. |
| 3 | AL_ERR2INT | R/W | 0 | Report amplitude-low via INTB + STATUS.ERR_ALE. |
| 2 | ZC_ERR2INT | R/W | 0 | Report zero-count via INTB + STATUS.ERR_ZC. |
| 0 | DRDY_2INT | R/W | 0 | Report DRDY via INTB + STATUS.DRDY. |

## 5. Conclusion

The LDC1312/1314/1612/1614's error reporting (DATAx bits, STATUS register, INTB pin) provides an effective way to diagnose sensor issues or device configuration errors, and can greatly simplify LDC system design.

---

*Omitted: copyright/footer lines, TI Important Notice/legal disclaimer.*
