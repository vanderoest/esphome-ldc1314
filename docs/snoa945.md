# Optimizing L Measurement Resolution for the LDC1312 and LDC1314

Texas Instruments Application Report, literature number SNOA945, February 2016. Author: Chris Oberhauser.

Source: `docs/snoa945.pdf`.

## Abstract

TI's LDC1312 and LDC1314 provide 12 bits of inductive measurement resolution. This note covers configuration options and methods to improve *effective* resolution.

## 1. Understanding LDC Resolution

For an ADC, resolution is the output word width; *effective* resolution drops if the input signal doesn't use the full input range (e.g. a 16-bit ADC with 1 VPP range fed only 500 mVPP effectively delivers ~15 bits).

Most inductive sensors don't use the LDC's full input range, reducing effective resolution. A system with 20% inductance shift has twice the effective resolution of one with 10% shift.

LDCs measure resonant frequency, so they're effectively frequency-domain ADCs: 1 LSB is measured in Hz, not volts. Example: 50 Hz/LSB with a 20 kHz sensor signal swing → effective resolution = log2(20000/50) ≈ 8.6 bits.

Two approaches to raise effective resolution: (1) improve frequency resolution, (2) increase sensor frequency variation (target/sensor geometry).

## 2. LDC Configuration Parameters

### 2.1 Reference Count and RCOUNT

Primary resolution-determining setting. Higher `RCOUNT` → higher-resolution L measurement.

| Device | Registers |
|---|---|
| LDC1312 | RCOUNT_CH0 (0x08), RCOUNT_CH1 (0x09) |
| LDC1314 | RCOUNT_CH0 (0x08), RCOUNT_CH1 (0x09), RCOUNT_CH2 (0x0A), RCOUNT_CH3 (0x0B) |

Constraints: `RCOUNT` must be `≥3` and `≤65535`. Values above `0x2000` provide **no additional resolution benefit** for the LDC131x (internal 16-bit quantization limit) — only reduces sample rate.

### 2.2 Reference Frequency

Conversion time:
```
tCONVERSION(s) = (RCOUNT × 16) / fREF
```

To raise sample rate: increase fREF and/or decrease RCOUNT (which lowers resolution). Rule of thumb: use the highest supported reference frequency, then set RCOUNT to hit the required sample rate.

| Parameter | LDC131x |
|---|---|
| Number of channels | 2 (LDC1312) / 4 (LDC1314) |
| Max reference frequency | 40 MHz (dual/multi-channel), 35 MHz (single-channel) |
| Max output resolution | 12/16 bits (via OUTPUT_GAIN) |
| Conversion time at max RCOUNT, max fREF | 26.2 ms @ 40 MHz, 30.0 ms @ 35 MHz |

### 2.3 Using the fREF Divider or Reducing fREF

Reducing the sensor/reference frequency ratio can be used to trade sample rate for resolution — the LDC131x has fewer significant bits when `fSENSOR/fREF` is small, since the leading output bits are always 0 (missing codes / lost resolution, compensable via Output Gain, §3.3).

**Example resolution vs. fSENSOR/fREF ratio**

| fSENSOR | fREF | Output Code (dec) | Output Code (bin) | Bits Available |
|---|---|---|---|---|
| 9.8 MHz | 40 MHz | 1004 | 0011 1110 1100 | 10 |
| 3.2 MHz | 40 MHz | 328 | 0001 0100 1000 | 9 |
| 980 kHz | 40 MHz | 100 | 0000 0110 0100 | 7 |
| 320 kHz | 40 MHz | 33 | 0000 0010 0001 | 6 |

Deliberately lowering fREF (via the reference divider or external clock) increases the *number of visible bits* for a fixed sensor frequency — analogous to lowering an ADC's reference voltage:

| fSENSOR | fREF | Output Code (dec) | Output Code (bin) | Bits | Sample rate (RCOUNT=0x0400) |
|---|---|---|---|---|---|
| 980 kHz | 40 MHz | 100 | 0000 0110 0100 | 7 | 2.441k SPS |
| 980 kHz | 20 MHz | 201 | 0000 1100 1001 | 8 | 1.221k SPS |
| 980 kHz | 10 MHz | 401 | 0001 1001 0001 | 9 | 610 SPS |
| 980 kHz | 4 MHz | 1004 | 0011 1110 1100 | 10 | 244 SPS |

Reduce fREF either by changing the external CLKIN frequency, or via the reference divider (e.g. fCLKIN=40 MHz, FREF divider=4 → fREF=10 MHz).

**Clocking diagram** (Figure 1, textual description): each sensor channel has an input divider (`CHx_FIN_DIVIDER`) feeding a MUX that selects `fIN` for the Frequency Counter/System Controller core; CLKIN or the internal oscillator feed per-channel reference dividers (`CHx_FREF_DIVIDER`) into a second MUX selecting `fREF`; a separate `SYS_CLOCK_DIV` divider derives `fSYS` for the watchdog timer. The core outputs conversion Data and, via `ERROR_CONFIG`, Error Flags.

### 2.4 Resolution as a Function of Sensor Frequency

At higher sensor frequencies, the LDC exhibits "missing codes" — output-code gaps grow as sensor frequency increases, since the code set is a function of RCOUNT. At low sensor frequency, output increments by 1 code per frequency step as expected; at higher frequency it starts skipping codes, with growing gaps.

### 2.5 Sensor Amplitude

Higher sensor oscillation amplitude improves SNR. Amplitude is controlled via `IDRIVE`; the recommended range of 1.2 V–1.8 V gives optimum resolution.

#### 2.5.1 Deglitch Filter Setting

4 settings (1/3.3/10/33 MHz); use the lowest of the four that exceeds the maximum sensor frequency (typically at maximum target-sensor interaction).

#### 2.5.2 Injection Resonance Locking

Analogous to Huygens' pendulum-clock synchronization: at low sensor amplitude, energy can couple from the external clock source into the sensor and lock the sensor frequency to an integer divider of the clock (e.g. a true 8.0003 MHz sensor signal locking to 8 MHz with a 40 MHz reference). More pronounced with low odd dividers (÷3, ÷5, ÷7) of `fCLKIN` than even/higher dividers; it's the *external* fCLKIN that couples in, not the internal fREF.

Mitigations: shift sensor frequency away from a strong locking frequency (e.g. adjust sensor capacitance); increase sensor current drive; choose an fCLKIN without an odd-integer divider landing in the sensor's frequency variation range; well-controlled CLKIN trace impedance (Z0) with minimal overshoot/undershoot; good ground plane; keep sensor traces separated from the CLKIN line.

## 3. LDC Clocking

### 3.1 Internal Oscillator vs. External Oscillator

Internal oscillator has a small temperature shift, less stable than a good external oscillator. LDC131x resolution won't resolve the internal oscillator's added noise floor unless `RCOUNT < 128` (→17.1 kSPS at 35 MHz fREF, exceeding the device's real throughput — not a practical restriction). Internal oscillator is acceptable when applications can tolerate some conversion drift; absolute matching / best sample-to-sample stability needs an external oscillator.

### 3.2 Single-Channel fREF Limitation Work-Around

Single-channel mode caps fREF at 35 MHz (vs. 40 MHz for multi-channel) — a 14% resolution reduction at fixed sample rate. With fCLKIN=40 MHz, the CLKIN divider must be set to 2 to respect the 35 MHz cap, yielding fREF=20 MHz — a 50% resolution drop vs. 40 MHz.

Work-around: if Channel 0's activation time (~Q/fSENSOR) is short enough, attach a second (possibly non-matching) "dummy" sensor to Channel 1 and ignore its data, running in multi-channel mode at fREF=40 MHz — at the cost of Channel 1's conversion time. Minimize that cost by setting Channel 1 to device minimums: `RCOUNT_CH1=3`, `SETTLECOUNT_CH1=1`. Effective for 1 MHz/Q=20 sensors up to ~1500 SPS, and for 5 MHz/Q=20 sensors up to >6k SPS (per the app note's comparison figure). Reducing the dummy channel's Q and/or raising its fSENSOR shortens its activation time, improving effectiveness.

### 3.3 Using Offset Gain

LDC131x converts internally at 16 bits but outputs only 12; the extra 4 bits are accessed via Output Gain (`RESET_DEV.OUTPUT_GAIN`), with no timing penalty. Default (gain disabled): DATA registers show the 12 MSBs of the 16-bit word. Gain settings 4x/8x/16x apply a 2/3/4-bit shift, equivalent to +2/+3/+4 bits of effective resolution.

#### 3.3.1 Example Use of Offset-Gain (LDC1314 EVM, 25 mm flat disk target, 0.2 mm–infinite axial travel)

**Step 1 — determine system boundaries** (Gain=1, OFFSET=0x0000): min target distance (0.2 mm) → DATA=291 (0x123); max (infinite) → DATA=201 (0x0C9). Code delta = 90 codes → ENOB = log2(90) ≈ 6.5 bits.

Output range limits per gain setting (must not clip): ≤100% full scale at gain=1, ≤25% at gain=4, ≤12.5% at gain=8, ≤6.25% at gain=16.

**Step 2 — apply gain**: full-scale word = 2¹²−1 = 4095. Code delta of 90 is only 2.2% of full scale → gain=16 is applicable. At gain=16 (no offset): min distance → 4095 (0xFFF, **clipped**), max distance → 3212 (0xC8C). Code delta 875 (invalid due to clipping); ENOB not meaningful.

**Step 3 — subtract offset**: since the un-offset gain=16 signal crosses the full-scale boundary, subtract a constant via the OFFSET register. Max-distance code (3212) minus a chosen offset of 2000 (0x7D0) keeps values in range. Result: min distance → 2670 (0xA6E), max distance → 1212 (0x4BC). Code delta = 1458 → ENOB = log2(1458) ≈ 10.5 bits — a 4-bit improvement over the unmodified gain=1 case.

### 3.4 Setting LDC131x RCOUNT

`RCOUNT > 0x2000` gives no additional resolution (16-bit internal quantization limit), only increases conversion time — not recommended. For higher resolution at acceptable lower sample rates, average multiple conversions instead.

## 4. Summary

Key resolution levers: managing sensor frequency, RCOUNT, using a clean reference clock, and properly setting sensor current drive (see `docs/snoa950.md`).

---

*Omitted: copyright/footer lines, TI Important Notice/legal disclaimer.*
