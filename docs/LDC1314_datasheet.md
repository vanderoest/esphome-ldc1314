# LDC1312, LDC1314 — Multi-Channel 12-Bit Inductance to Digital Converter (LDC) for Inductive Sensing

Texas Instruments, literature number SNOSCZ0A, December 2014 – Revised March 2018.

Source: `docs/LDC1314_datasheet.pdf` (fetched from `ti.com/lit/ds/symlink/ldc1314.pdf`).

## 1. Features

- Easy-to-Use – Minimal Configuration Required
- Up to 4 Channels With Matched Sensor Drive
- Multiple Channels Support Environmental and Aging Compensation
- Remote Sensor Position of >20 cm Supports Operation In Harsh Environments
- Pin-Compatible Medium and High-Resolution Options:
  - LDC1312/4: 2/4-ch 12-Bit LDC
  - LDC1612/4: 2/4-ch 28-Bit LDC
- Supports Wide Sensor Frequency Range of 1 kHz to 10 MHz
- Power Consumption:
  - 35 µA Low Power Sleep Mode
  - 200 nA Shutdown Mode
- 2.7 V to 3.6 V Operation
- Multiple Reference Clocking Options:
  - Included Internal Clock For Lower System Cost
  - Support for 40 MHz External Clock For Higher System Performance
- Immunity to DC Magnetic Fields and Magnets

## 2. Applications

- Knobs in Consumer, Appliances, and Automotive
- Linear and Rotational Encoders
- Buttons in Home Electronics, Wearables, Manufacturing, and Automotive
- Keypads in Manufacturing and Appliances
- Slider Buttons in Consumer Products
- Metal Detection in Industrial and Automotive
- POS and EPOS
- Flow Meters in Consumer and Appliances

## 3. Description

The LDC1312 and LDC1314 are 2- and 4-channel, 12-bit inductance to digital converters (LDCs) for inductive sensing solutions. With multiple channels and support for remote sensing, the LDC1312 and LDC1314 enable the performance and reliability benefits of inductive sensing to be realized at minimal cost and power. The products are easy to use, only requiring that the sensor frequency be within 1 kHz and 10 MHz to begin sensing. The wide 1 kHz to 10 MHz sensor frequency range also enables use of very small PCB coils, further reducing sensing solution cost and size.

The LDC1312 and LDC1314 offer well-matched channels, which allow for differential and ratiometric measurements. This enables designers to use one channel to compensate their sensing for environmental and aging conditions such as temperature, humidity, and mechanical drift.

The LDC1312 and LDC1314 are easily configured via an I2C interface. The two-channel LDC1312 is available in a WSON-12 package and the four-channel LDC1314 is available in a WQFN-16 package.

**Device Information**

| Part Number | Package | Body Size (nom) |
|---|---|---|
| LDC1312 | WSON-12 | 4 mm × 4 mm |
| LDC1314 | WQFN-16 | 4 mm × 4 mm |

## 5. Pin Configuration and Functions

WSON-12 (LDC1312) and WQFN-16 (LDC1314), top view. LDC1312 pins 1–12 map to LDC1314 pins as follows: SCL=1, SDA=2, CLKIN=3, ADDR=4, INTB=5, SD=6, VDD=7, GND=8, IN0A=9, IN0B=10, IN1A=11, IN1B=12 (LDC1312 ends here); LDC1314 continues with IN2A=13, IN2B=14, IN3A=15, IN3B=16. Both packages have an exposed Die Attach Pad (DAP), internally connected to GND.

**Pin Functions**

| Name | No. | Type | Description |
|---|---|---|---|
| SCL | 1 | I | I2C Clock input. Open drain output; requires resistive pullup to logic high level. |
| SDA | 2 | I/O | I2C Data input/output. Open drain output; requires resistive pullup to logic high level. |
| CLKIN | 3 | I | External Reference Clock input. Tie this pin to GND if internal oscillator is used. |
| ADDR | 4 | I | I2C Address selection pin: ADDR=L → I2C address = 0x2A, ADDR=H → I2C address = 0x2B. Must not be allowed to float. |
| INTB | 5 | O | Configurable Interrupt output pin. Push-pull output; does not require pullup. |
| SD | 6 | I | Shutdown input: SD=L for normal operation, SD=H for inactive mode. Must not be allowed to float. |
| VDD | 7 | P | Power Supply |
| GND | 8 | G | Ground |
| IN0A | 9 | A | External LC sensor 0 connection |
| IN0B | 10 | A | External LC sensor 0 connection |
| IN1A | 11 | A | External LC sensor 1 connection |
| IN1B | 12 | A | External LC sensor 1 connection |
| IN2A | 13 | A | External LC sensor 2 connection (LDC1314 only) |
| IN2B | 14 | A | External LC sensor 2 connection (LDC1314 only) |
| IN3A | 15 | A | External LC sensor 3 connection (LDC1314 only) |
| IN3B | 16 | A | External LC sensor 3 connection (LDC1314 only) |
| DAP | – | N/A | Connect to Ground. Internally tied to GND pin; do not use as primary ground. |

## 6. Specifications

### 6.1 Absolute Maximum Ratings

| Symbol | Parameter | Min | Max | Unit |
|---|---|---|---|---|
| VDD | Supply Voltage Range | | 5 | V |
| Vi | Voltage on any pin | -0.3 | VDD+0.3 | V |
| IA | Input current on any INx pin | -8 | 8 | mA |
| ID | Input current on any Digital pin | -5 | 5 | mA |
| Tj | Junction Temperature | -55 | 150 | °C |
| Tstg | Storage temperature range | -65 | 150 | °C |

### 6.2 ESD Ratings

| | | LDC1312 (WSON-12) | LDC1314 (WQFN-16) | Unit |
|---|---|---|---|---|
| V(ESD) | Human-body model (HBM), ANSI/ESDA/JEDEC JS-001 | ±2000 | ±2000 | V |
| V(ESD) | Charged-device model (CDM), JESD22-C101 | ±750 | ±750 | V |

### 6.3 Recommended Operating Conditions

Unless otherwise specified, all limits ensured for TA = 25°C, VDD = 3.3 V.

| Symbol | Parameter | Min | Nom | Max | Unit |
|---|---|---|---|---|---|
| VDD | Supply Voltage | 2.7 | | 3.6 | V |
| TA | Operating Temperature | -40 | | 125 | °C |

### 6.4 Thermal Information

| Metric | LDC1312 WSON (DNT), 12 pins | LDC1314 WQFN (RGH), 16 pins | Unit |
|---|---|---|---|
| RθJA — Junction-to-ambient thermal resistance | 50 | 38 | °C/W |

### 6.5 Electrical Characteristics

Unless otherwise specified, all limits ensured for TA = 25°C, VDD = 3.3 V. Register values in binary (`b` prefix) or hex (`0x` prefix); decimal has no prefix. Test conditions footnote (sensor used for IDD test): 2-layer, 32 turns/layer, 14 mm diameter PCB inductor, L=19.4 µH, RP=5.7 kΩ at 2 MHz; sensor capacitor 330 pF 1% COG/NP0; target Aluminum, 1.5 mm thickness; Channel 0 continuous mode; fCLKIN=40 MHz, FIN_DIVIDER0=b0000, FREF_DIVIDER0=0x0001, RCOUNT0=0xFFFF, SETTLECOUNT0=0x0100, RP_OVERRIDE=b1, AUTO_AMP_DIS=b1, DRIVE_CURRENT0=0x9800.

**POWER**

| Symbol | Parameter | Test Conditions | Min | Typ | Max | Unit |
|---|---|---|---|---|---|---|
| VDD | Supply Voltage | TA = -40°C to +125°C | 2.7 | | 3.6 | V |
| IDD | Supply Current (not incl. sensor current) | fCLKIN = 10 MHz | | 2.1 | | mA |
| IDDSL | Sleep Mode Supply Current | SLEEP_MODE_EN = b1 | | 35 | 60 | µA |
| ISD | Shutdown Mode Supply Current | SD = VDD | | 0.2 | 1 | µA |

**SENSOR**

| Symbol | Parameter | Test Conditions | Min | Typ | Max | Unit |
|---|---|---|---|---|---|---|
| ISENSORMAX | Sensor Maximum Current drive | HIGH_CURRENT_DRV=b0, DRIVE_CURRENTx=0xF800 | | 1.5 | | mA |
| RP | Sensor RP | | 1 | | 100 | kΩ |
| IHDSENSORMAX | High current sensor drive mode: Sensor Max Current | HIGH_CURRENT_DRV=b1, DRIVE_CURRENT0=0xF800, Channel 0 only | | 6 | | mA |
| RP_HD_MIN | Minimum sensor RP (high current drive mode) | | | 250 | | Ω |
| fSENSOR | Sensor Resonance Frequency | TA = -40°C to +125°C | 0.001 | | 10 | MHz |
| VSENSORMAX | Maximum oscillation amplitude (peak) | | | 1.8 | | V |
| NBITS | Number of bits | RESET_DEV.OUTPUT_GAIN=b00, RCOUNTx ≥ 0x0400 | | 12 | | bits |
| fCS | Maximum Channel Sample Rate | single active channel continuous conversion, SCL=400 kHz | | 13.3 | | kSPS |
| CIN | Sensor Pin input capacitance | | | 4 | | pF |

**DIGITAL PIN LEVELS**

| Symbol | Parameter | Test Conditions | Min | Typ | Max | Unit |
|---|---|---|---|---|---|---|
| VIL | Low voltage threshold (ADDR, SD) | | | | 0.3×VDD | V |
| VIH | High voltage threshold (ADDR, SD) | | 0.7×VDD | | | V |
| VOL | INTB low voltage output level | 3 mA sink current | | | 0.4 | V |
| VOH | INTB high voltage output level | | 2.4 | | | V |

**REFERENCE CLOCK**

| Symbol | Parameter | Test Conditions | Min | Typ | Max | Unit |
|---|---|---|---|---|---|---|
| fCLKIN | External Reference Clock Input Frequency | TA = -40°C to +125°C | 2 | | 40 | MHz |
| CLKINDUTY_MIN | External Reference Clock min duty cycle | | | 40% | | |
| CLKINDUTY_MAX | External Reference Clock max duty cycle | | | 60% | | |
| VCLKIN_LO | CLKIN low voltage threshold | | | | 0.3×VDD | V |
| VCLKIN_HI | CLKIN high voltage threshold | | 0.7×VDD | | | V |
| fINTCLK | Internal Reference Clock Frequency range | | 35 | 43.4 | 55 | MHz |
| TCf_int_µ | Internal Reference Clock Temperature Coefficient (mean) | | | -13 | | ppm/°C |

**TIMING CHARACTERISTICS**

| Symbol | Parameter | Min | Typ | Max | Unit |
|---|---|---|---|---|---|
| tWAKEUP | Wake-up Time from SD high→low transition to I2C readback | | | 2 | ms |
| tWD-TIMEOUT | Sensor recovery time (after watchdog timeout) | | 5.2 | | ms |

### 6.6 Switching Characteristics — I2C

Unless otherwise specified, all limits ensured for TA = 25°C, VDD = 3.3 V.

| Symbol | Parameter | Test Conditions | Min | Typ | Max | Unit |
|---|---|---|---|---|---|---|
| VIH | Input High Voltage | | 0.7×VDD | | | V |
| VIL | Input Low Voltage | | | | 0.3×VDD | V |
| VOL | Output Low Voltage | 3 mA sink current | | | 0.4 | V |
| HYS | Hysteresis | | | 0.1×VDD | | V |
| fSCL | Clock Frequency | | 10 | | 400 | kHz |
| tLOW | Clock Low Time | | 1.3 | | | µs |
| tHIGH | Clock High Time | | 0.6 | | | µs |
| tHD;STA | Hold Time (repeated) START condition | | 0.6 | | | µs |
| tSU;STA | Set-up time for repeated START | | 0.6 | | | µs |
| tHD;DAT | Data hold time | | 0 | | | µs |
| tSU;DAT | Data setup time | | 100 | | | ns |
| tSU;STO | Set-up time for STOP condition | | 0.6 | | | µs |
| tBUF | Bus free time between STOP and START | | 1.3 | | | µs |
| tVD;DAT | Data valid time | | | | 0.9 | µs |
| tVD;ACK | Data valid acknowledge time | | | | 0.9 | µs |
| tSP | Pulse width of spikes suppressed by input filter | | | | 50 | ns |

I2C timing diagram (Figure 1, textual description): standard I2C waveform on SCL/SDA showing START, repeated START, and STOP conditions, with the timing parameters above (tLOW, tHIGH, tHD;STA, tSU;STA, tHD;DAT, tSU;DAT, tSU;STO, tBUF, rise/fall times tr/tf, spike width tSP) annotated at their respective transitions.

### 6.7 Typical Characteristics

Common test conditions unless noted otherwise: sensor inductor 2-layer, 32 turns/layer, 14 mm diameter PCB, L=19.4 µH, RP=5.7 kΩ at 2 MHz; sensor capacitor 330 pF 1% COG/NP0; target Aluminum 1.5 mm; Channel 0 continuous mode; fCLKIN=40 MHz, FIN_DIVIDER0=0x1, FREF_DIVIDER0=0x001, RCOUNT0=0xFFFF, SETTLECOUNT0=0x0100, RP_OVERRIDE=1, AUTO_AMP_DIS=1, DRIVE_CURRENT0=0x9800.

Graphs provided (values over -40°C to +125°C and 2.7–3.6 V unless noted, described textually — plots not reproducible in Markdown):
- Active Mode IDD vs. Temperature / vs. VDD — IDD rises gently with both temperature and VDD, roughly 3.05–3.25 mA including 1.57 mA sensor coil current.
- Sleep Mode IDD vs. Temperature / vs. VDD — flat ~30 µA up to ~80°C, rising sharply above 100°C to ~55–60 µA.
- Shutdown Mode IDD vs. Temperature / vs. VDD — flat ~0.1–0.2 µA below 80°C, rising to ~1.2–1.4 µA at 125°C.
- Internal Oscillator Frequency vs. Temperature / vs. VDD — decreases roughly linearly from ~43.39 MHz at -40°C to ~43.32 MHz at 125°C (single-unit data), largely independent of VDD.

## 7. Detailed Description

### 7.1 Overview

The LDC1312/LDC1314 is an inductance-to-digital converter (LDC) that measures the oscillation frequency of multiple LC resonators. The device outputs a digital value proportional to frequency, with 12 bits of measurement resolution. This frequency measurement can be converted to an equivalent inductance, or mapped to the movement of a conductive object. Supports oscillation frequencies from 1 kHz to 10 MHz with equivalent parallel resistances as low as 1.0 kΩ. Includes a stable internal reference, with the option to drive a clean external oscillator for improved measurement noise. Conversion time is configurable per channel; longer conversion times provide higher effective resolution.

Configured through a 400-kbit/s I2C bus with an ADDR pin for address selection. Supply range 2.7 V–3.6 V. Only external components required: supply bypass capacitors and I2C pull-ups.

### 7.2 Functional Block Diagram

Textual description (Figure 10, LDC1312 left / LDC1314 right): each channel has a front-end "Resonant Circuit Driver" connected to a pair of sensor pins (INxA/INxB). All channel drivers feed a multiplexer that sequences the active channel's sensor frequency (fIN) into the "Core", which also receives the reference frequency (fREF) derived from CLKIN or the internal oscillator. The Core performs the frequency-ratio measurement and exposes results/config via the I2C block (SDA, SCL, ADDR). VDD/GND, SD and INTB connect directly to the Core.

### 7.3 Feature Description

#### 7.3.1 Multi-Channel and Single Channel Operation

Flexible channel sampling: continuous conversion on a single selected channel, or automatic sequencing across multiple channels. In multi-channel mode, the LDC sequentially samples the selected channels. In single-channel mode, the LDC continuously samples only the selected channel. At the end of each conversion (single-channel) or after converting all selected channels (multi-channel), INTB can be configured to assert to indicate completion.

#### 7.3.2 Adjustable Conversion Time

Tradeoff between measurement resolution and conversion interval: longer intervals give higher resolution. Conversion interval configurable from 3.2 µs to >26.2 ms with 16 bits of resolution, per-channel, via the `RCOUNTx` register. It is possible to configure the interval shorter than the DATAx readback time.

#### 7.3.3 Digital Signal Gain

Output resolution is 12 bits, but the internal signal path supports 16 bits via the GAIN setting (`RESET_DEV.OUTPUT_GAIN`).

#### 7.3.4 Sensor Startup and Glitch Configuration

Adjustable per-channel sensor startup (settling) timing, from 1.2 µs to >26.2 ms via `SETTLECOUNTx`. Sensors with lower resonant frequency or higher Q may need more settling time. Sensor activation can be configured for faster startup (max current) or lower current (power-optimized) via `CONFIG.SENSOR_ACTIVATE_SEL`. An internal deglitch filter attenuates external noise interference (`MUX_CONFIG.DEGLITCH`).

#### 7.3.5 Reference Clock

Requires a clean reference clock; internal oscillator typical frequency ~43 MHz, temperature coefficient ~-13 ppm/°C. External reference can be applied via CLKIN for higher resolution/stability. Digital dividers scale fCLK and the sensor inputs per-channel (`FIN_DIVIDERx`, `FREF_DIVIDERx`). Higher reference frequency gives higher sample rate for a given resolution.

#### 7.3.6 Sensor Current Drive Control

An AC current matching the sensor resonant frequency must be driven across the LC sensor to sustain oscillation. Optimum performance requires sensor amplitude within 1.2 Vp–1.8 Vp. Each channel's current drive is set independently (16 µA–1.6 mA) via `IDRIVEx`. The device can also automatically determine/dynamically adjust drive current via `RP_OVERRIDE_EN`.

#### 7.3.7 Device Status Monitoring

Reports on device/sensor status via I2C: sensor amplitude outside optimum range, sensor unable to oscillate, new conversion data available, conversion errors. See `STATUS`/`ERROR_CONFIG` registers.

### 7.4 Device Functional Modes

#### 7.4.1 Startup Mode

On power-up, the LDC enters Sleep Mode and waits for configuration. Once configured, exit Sleep Mode (set `CONFIG.SLEEP_MODE_EN=b0`) to begin conversions. Recommended to configure the LDC while in Sleep Mode; to change a setting, return to Sleep Mode, change the register, then exit Sleep Mode again.

#### 7.4.2 Sleep Mode (Configuration Mode)

Entered by `CONFIG.SLEEP_MODE_EN=1`. Device configuration is retained but no conversions occur. Exit to Normal mode with `CONFIG.SLEEP_MODE_EN=0`; sensor activation for the first conversion begins after 16,384/fINT elapses. I2C register access is functional in Sleep Mode. Entering Sleep Mode clears all conversion results, error conditions, and de-asserts INTB.

#### 7.4.3 Normal (Conversion) Mode

The LDC repeatedly samples the frequency of the active sensor(s) and generates output for the active channel(s).

#### 7.4.4 Shutdown Mode

SD pin high → Shutdown Mode, the lowest power state. Exit to Sleep Mode by setting SD low. Entering Shutdown Mode returns all registers to default state, clears any error condition, and de-asserts INTB (driven high). No I2C register access is possible while in Shutdown Mode. The ADDR pin setting may be changed while in Shutdown Mode.

##### 7.4.4.1 Reset

Writing `RESET_DEV.RESET_DEV=1` resets the device: any active conversion stops and all registers return to default values. This bit always reads back 0.

### 7.5 Programming

Configuration procedure: put the device into Sleep Mode, set the appropriate registers, then enter Normal Mode. Conversion results must be read while in Normal Mode. Entering Shutdown Mode resets the device configuration.

#### 7.5.1 I2C Interface Specifications

400 kbit/s max. 16-bit-wide registers accessed via a repeated start (to read/write the 2nd byte). Standard I2C 7-bit slave address followed by an 8-bit pointer register byte. No clock stretching. I2C address = 0x2A when ADDR is low, 0x2B when ADDR is high (changeable while in Shutdown Mode).

Write sequence (textual description of Figure 11): Master START → 7-bit slave address + W bit (Frame 1) → slave ACK → 8-bit register pointer (Frame 2) → slave ACK → 16-bit data MSB (Frame 3) → slave ACK → 16-bit data LSB (Frame 4) → slave ACK → Master STOP.

Read sequence (textual description of Figure 12): Master START → 7-bit slave address + W bit (Frame 1) → slave ACK → 8-bit register pointer (Frame 2) → slave ACK → repeated START → 7-bit slave address + R bit (Frame 3) → slave ACK → 16-bit data MSB from slave (Frame 4) → master ACK → 16-bit data LSB from slave (Frame 5) → master NACK → Master STOP.

#### 7.5.2 Pulses on I2C

The LDC's I2C interface does not support early termination of a transaction (a STOP before the normal ACK-terminated end); this can corrupt the current and/or following transaction. The device is also sensitive to any extraneous pulse on SDA during the SCL-low period of the first bit of the address byte — such pulses must not exceed the tSP specification. The master must avoid generating an SDA pulse between the I2C START and the ADDR bits.

### 7.6 Register Maps

#### 7.6.1 Register List

`Reserved` fields must be written only with the indicated value. R/W column: `R/W` = read+write, `R` = read-only, `W` = write-only. For registers mixing R and R/W fields, write the reset value into the R field(s) when setting the R/W field(s).

| Address | Name | Default | Description |
|---|---|---|---|
| 0x00 | DATA0 | 0x0000 | Channel 0 Conversion Result and Error Status |
| 0x02 | DATA1 | 0x0000 | Channel 1 Conversion Result and Error Status |
| 0x04 | DATA2 | 0x0000 | Channel 2 Conversion Result and Error Status (LDC1314 only) |
| 0x06 | DATA3 | 0x0000 | Channel 3 Conversion Result and Error Status (LDC1314 only) |
| 0x08 | RCOUNT0 | 0x0080 | Reference Count setting for Channel 0 |
| 0x09 | RCOUNT1 | 0x0080 | Reference Count setting for Channel 1 |
| 0x0A | RCOUNT2 | 0x0080 | Reference Count setting for Channel 2 (LDC1314 only) |
| 0x0B | RCOUNT3 | 0x0080 | Reference Count setting for Channel 3 (LDC1314 only) |
| 0x0C | OFFSET0 | 0x0000 | Offset value for Channel 0 |
| 0x0D | OFFSET1 | 0x0000 | Offset value for Channel 1 |
| 0x0E | OFFSET2 | 0x0000 | Offset value for Channel 2 (LDC1314 only) |
| 0x0F | OFFSET3 | 0x0000 | Offset value for Channel 3 (LDC1314 only) |
| 0x10 | SETTLECOUNT0 | 0x0000 | Channel 0 Settling Reference Count |
| 0x11 | SETTLECOUNT1 | 0x0000 | Channel 1 Settling Reference Count |
| 0x12 | SETTLECOUNT2 | 0x0000 | Channel 2 Settling Reference Count (LDC1314 only) |
| 0x13 | SETTLECOUNT3 | 0x0000 | Channel 3 Settling Reference Count (LDC1314 only) |
| 0x14 | CLOCK_DIVIDERS0 | 0x0000 | Reference and Sensor Divider settings for Channel 0 |
| 0x15 | CLOCK_DIVIDERS1 | 0x0000 | Reference and Sensor Divider settings for Channel 1 |
| 0x16 | CLOCK_DIVIDERS2 | 0x0000 | Reference and Sensor Divider settings for Channel 2 (LDC1314 only) |
| 0x17 | CLOCK_DIVIDERS3 | 0x0000 | Reference and Sensor Divider settings for Channel 3 (LDC1314 only) |
| 0x18 | STATUS | 0x0000 | Device Status Report |
| 0x19 | ERROR_CONFIG | 0x0000 | Error Reporting Configuration |
| 0x1A | CONFIG | 0x2801 | Conversion Configuration |
| 0x1B | MUX_CONFIG | 0x020F | Channel Multiplexing Configuration |
| 0x1C | RESET_DEV | 0x0000 | Reset Device |
| 0x1E | DRIVE_CURRENT0 | 0x0000 | Channel 0 sensor current drive configuration |
| 0x1F | DRIVE_CURRENT1 | 0x0000 | Channel 1 sensor current drive configuration |
| 0x20 | DRIVE_CURRENT2 | 0x0000 | Channel 2 sensor current drive configuration (LDC1314 only) |
| 0x21 | DRIVE_CURRENT3 | 0x0000 | Channel 3 sensor current drive configuration (LDC1314 only) |
| 0x7E | MANUFACTURER_ID | 0x5449 | Manufacturer ID |
| 0x7F | DEVICE_ID | 0x3054 | Device ID |

#### 7.6.2 Address 0x00, DATA0

Bit layout: `[15]=ERR_UR0 [14]=ERR_OR0 [13]=ERR_WD0 [12]=ERR_AE0 [11:0]=DATA0`

| Bit | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15 | ERR_UR0 | R | 0 | Channel 0 Conversion Under-range Error Flag. Cleared by reading the bit. |
| 14 | ERR_OR0 | R | 0 | Channel 0 Conversion Over-range Error Flag. Cleared by reading the bit. |
| 13 | ERR_WD0 | R | 0 | Channel 0 Conversion Watchdog Timeout Error Flag. Cleared by reading the bit. |
| 12 | ERR_AE0 | R | 0 | Channel 0 Conversion Amplitude Error Flag. Cleared by reading the bit. |
| 11:0 | DATA0[11:0] | R | 0x000 | Channel 0 Conversion Result |

#### 7.6.3 Address 0x02, DATA1

Same layout as DATA0, fields renamed `ERR_UR1/ERR_OR1/ERR_WD1/ERR_AE1/DATA1[11:0]`.

#### 7.6.4 Address 0x04, DATA2 (LDC1314 only)

Same layout, fields `ERR_UR2/ERR_OR2/ERR_WD2/ERR_AE2/DATA2[11:0]`.

#### 7.6.5 Address 0x06, DATA3 (LDC1314 only)

Same layout, fields `ERR_UR3/ERR_OR3/ERR_WD3/ERR_AE3/DATA3[11:0]`.

#### 7.6.6 Address 0x08, RCOUNT0

`[15:0] RCOUNT0`, R/W, reset 0x0080. Channel 0 Reference Count Conversion Interval Time. `0x0000–0x0004`: Reserved. `0x0005–0xFFFF`: Conversion Time `tC0 = (RCOUNT0 × 16) / fREF0`.

#### 7.6.7 Address 0x09, RCOUNT1

`[15:0] RCOUNT1`, R/W, reset 0x0080. Same semantics, `tC1 = (RCOUNT1 × 16) / fREF1`.

#### 7.6.8 Address 0x0A, RCOUNT2 (LDC1314 only)

`[15:0] RCOUNT2`, R/W, reset 0x0080. `tC2 = (RCOUNT2 × 16) / fREF2`.

#### 7.6.9 Address 0x0B, RCOUNT3 (LDC1314 only)

`[15:0] RCOUNT3`, R/W, reset 0x0080. `tC3 = (RCOUNT3 × 16) / fREF3`.

#### 7.6.10 Address 0x0C, OFFSET0

`[15:0] OFFSET0`, R/W, reset 0x0000. Channel 0 Conversion Offset: `fOFFSET0 = (OFFSET0 ÷ 2^16) × fREF0`.

#### 7.6.11 Address 0x0D, OFFSET1

`[15:0] OFFSET1`, R/W, reset 0x0000. `fOFFSET1 = (OFFSET1 ÷ 2^16) × fREF1`.

#### 7.6.12 Address 0x0E, OFFSET2 (LDC1314 only)

`[15:0] OFFSET2`, R/W, reset 0x0000. `fOFFSET2 = (OFFSET2 ÷ 2^16) × fREF2`.

#### 7.6.13 Address 0x0F, OFFSET3 (LDC1314 only)

`[15:0] OFFSET3`, R/W, reset 0x0000. `fOFFSET3 = (OFFSET3 ÷ 2^16) × fREF3`.

#### 7.6.14 Address 0x10, SETTLECOUNT0

`[15:0] SETTLECOUNT0`, R/W, reset 0x0000. Channel 0 Conversion Settling — settling time allowed for the LC sensor to stabilize before starting a conversion on Channel 0. If amplitude has not settled before conversion start, an Amplitude error is generated (if enabled). `0x0000`/`0x0001`: Settle Time `tS0 = 32 ÷ fREF0`. `0x0002–0xFFFF`: `tS0 = (SETTLECOUNT0 × 16) ÷ fREF0`.

#### 7.6.15 Address 0x11, SETTLECOUNT1

`[15:0] SETTLECOUNT1`, R/W, reset 0x0000. Same semantics for Channel 1.

#### 7.6.16 Address 0x12, SETTLECOUNT2 (LDC1314 only)

`[15:0] SETTLECOUNT2`, R/W, reset 0x0000. Same semantics for Channel 2.

#### 7.6.17 Address 0x13, SETTLECOUNT3 (LDC1314 only)

`[15:0] SETTLECOUNT3`, R/W, reset 0x0000. Same semantics for Channel 3.

#### 7.6.18 Address 0x14, CLOCK_DIVIDERS0

Bit layout: `[15:12]=FIN_DIVIDER0 [11:10]=RESERVED [9:0]=FREF_DIVIDER0`

| Bit | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15:12 | FIN_DIVIDER0 | R/W | 0000 | Channel 0 Input Divider. Must be ≥2 if sensor frequency ≥8.75 MHz. `b0000`: Reserved, do not use. `≥b0001`: `fin0 = fSENSOR0 / FIN_DIVIDER0`. |
| 11:10 | RESERVED | R/W | 00 | Set to b00. |
| 9:0 | FREF_DIVIDER0 | R/W | 0x000 | Channel 0 Reference Divider, scales max conversion frequency. `0x000`: Reserved. `≥0x001`: `fREF0 = fCLK / FREF_DIVIDER0`. |

#### 7.6.19 Address 0x15, CLOCK_DIVIDERS1

Same layout, fields `FIN_DIVIDER1`/`FREF_DIVIDER1`, `fin1 = fSENSOR1 / FIN_DIVIDER1`, `fREF1 = fCLK / FREF_DIVIDER1`.

#### 7.6.20 Address 0x16, CLOCK_DIVIDERS2 (LDC1314 only)

Same layout, fields `FIN_DIVIDER2`/`FREF_DIVIDER2`.

#### 7.6.21 Address 0x17, CLOCK_DIVIDERS3 (LDC1314 only)

Same layout, fields `FIN_DIVIDER3`/`FREF_DIVIDER3`.

#### 7.6.22 Address 0x18, STATUS

Bit layout: `[15:14]=ERR_CHAN [13]=ERR_UR [12]=ERR_OR [11]=ERR_WD [10]=ERR_AHE [9]=ERR_ALE [8]=ERR_ZC [7]=Reserved [6]=DRDY [5:4]=Reserved [3]=UNREADCONV0 [2]=UNREADCONV1 [1]=UNREADCONV2 [0]=UNREADCONV3`

| Bit | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15:14 | ERR_CHAN | R | 00 | Error Channel — indicates which channel generated a flag/error. Once flagged, latched until STATUS or the corresponding DATAx register is read. `b00`=Ch0, `b01`=Ch1, `b10`=Ch2 (LDC1314 only), `b11`=Ch3 (LDC1314 only). |
| 13 | ERR_UR | R | 0 | Conversion Under-range Error. Set when an active channel generates an under-range error since last STATUS read. |
| 12 | ERR_OR | R | 0 | Conversion Over-range Error. |
| 11 | ERR_WD | R | 0 | Watchdog Timeout Error. |
| 10 | ERR_AHE | R | 0 | Sensor Amplitude High Error — sensor amplitude above nominal 1.8 V; reduce corresponding IDRIVEx. |
| 9 | ERR_ALE | R | 0 | Sensor Amplitude Low Error — sensor amplitude below nominal 1.2 V. |
| 8 | ERR_ZC | R | 0 | Zero Count Error. |
| 7 | Reserved | R | 0 | Reads 0. |
| 6 | DRDY | R | 0 | Data Ready Flag. In single-channel mode: one conversion available. In sequential mode: all active channels' results available. |
| 5:4 | Reserved | R | 00 | Reads 00b. |
| 3 | UNREADCONV0 | R | 0 | Channel 0 unread conversion present. Read DATA0 to retrieve/clear. |
| 2 | UNREADCONV1 | R | 0 | Channel 1 unread conversion present. Read DATA1 to retrieve/clear. |
| 1 | UNREADCONV2 | R | 0 | Channel 2 unread conversion present (LDC1314 only). Read DATA2. |
| 0 | UNREADCONV3 | R | 0 | Channel 3 unread conversion present (LDC1314 only). Read DATA3. |

All STATUS bits except the UNREADCONVx flags are sticky and cleared only by reading STATUS (which also de-asserts INTB); UNREADCONVx additionally clears on reading the corresponding DATAx.

#### 7.6.23 Address 0x19, ERROR_CONFIG

Bit layout: `[15]=UR_ERR2OUT [14]=OR_ERR2OUT [13]=WD_ERR2OUT [12]=AH_ERR2OUT [11]=AL_ERR2OUT [10:8]=Reserved [7]=UR_ERR2INT [6]=OR_ERR2INT [5]=WD_ERR2INT [4]=AH_ERR2INT [3]=AL_ERR2INT [2]=ZC_ERR2INT [1]=Reserved [0]=DRDY_2INT`

| Bit | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15 | UR_ERR2OUT | R/W | 0 | Report Under-range errors in DATAx.ERR_URx. |
| 14 | OR_ERR2OUT | R/W | 0 | Report Over-range errors in DATAx.ERR_ORx. |
| 13 | WD_ERR2OUT | R/W | 0 | Report Watchdog Timeout errors in DATAx.ERR_WDx. |
| 12 | AH_ERR2OUT | R/W | 0 | Report Amplitude High errors in DATAx.ERR_AEx (OR-ed with AL_ERR2OUT in that bit). |
| 11 | AL_ERR2OUT | R/W | 0 | Report Amplitude Low warnings in DATAx.ERR_AEx (OR-ed with AH_ERR2OUT in that bit). |
| 10:8 | Reserved | R/W | 00 | Set to b00. |
| 7 | UR_ERR2INT | R/W | 0 | Report Under-range errors via INTB + STATUS.ERR_UR. |
| 6 | OR_ERR2INT | R/W | 0 | Report Over-range errors via INTB + STATUS.ERR_OR. |
| 5 | WD_ERR2INT | R/W | 0 | Report Watchdog Timeout errors via INTB + STATUS.ERR_WD. |
| 4 | AH_ERR2INT | R/W | 0 | Report Amplitude High errors via INTB + STATUS.ERR_AHE. |
| 3 | AL_ERR2INT | R/W | 0 | Report Amplitude Low errors via INTB + STATUS.ERR_ALE. |
| 2 | ZC_ERR2INT | R/W | 0 | Report Zero Count errors via INTB + STATUS.ERR_ZC. |
| 1 | Reserved | R/W | 0 | Set to b0. |
| 0 | DRDY_2INT | R/W | 0 | Report Data Ready via INTB + STATUS.DRDY. |

#### 7.6.24 Address 0x1A, CONFIG

Bit layout: `[15:14]=ACTIVE_CHAN [13]=SLEEP_MODE_EN [12]=RP_OVERRIDE_EN [11]=SENSOR_ACTIVATE_SEL [10]=AUTO_AMP_DIS [9]=REF_CLK_SRC [8]=RESERVED [7]=INTB_DIS [6]=HIGH_CURRENT_DRV [5:0]=RESERVED`

| Bit | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15:14 | ACTIVE_CHAN | R/W | 00 | Channel for continuous conversions when `MUX_CONFIG.AUTOSCAN_EN=0`. `b00`=Ch0, `b01`=Ch1, `b10`=Ch2 (LDC1314 only), `b11`=Ch3 (LDC1314 only). |
| 13 | SLEEP_MODE_EN | R/W | 1 | `b0`=Active, `b1`=Sleep Mode. |
| 12 | RP_OVERRIDE_EN | R/W | 0 | Sensor RP Override Enable — `b0`=auto amplitude override off, `b1`=on (use IDRIVEx fixed value). |
| 11 | SENSOR_ACTIVATE_SEL | R/W | 1 | `b0`=Full Current Activation (max current, shorter activation). `b1`=Low Power Activation (uses DRIVE_CURRENTx value, minimizes power). |
| 10 | AUTO_AMP_DIS | R/W | 0 | `b0`=Automatic amplitude correction enabled. `b1`=disabled (recommended for precision applications). |
| 9 | REF_CLK_SRC | R/W | 0 | `b0`=internal oscillator. `b1`=CLKIN external clock. |
| 8 | RESERVED | R/W | 0 | Set to b0. |
| 7 | INTB_DIS | R/W | 0 | `b0`=INTB asserts on status updates. `b1`=INTB disabled, held high. |
| 6 | HIGH_CURRENT_DRV | R/W | 0 | `b0`=normal drive (≤1.5 mA all channels). `b1`=Channel 0 driven >1.5 mA. Not supported with `AUTOSCAN_EN=1`. |
| 5:0 | RESERVED | R/W | 00'0001 | Set to b00'0001. |

#### 7.6.25 Address 0x1B, MUX_CONFIG

Bit layout: `[15]=AUTOSCAN_EN [14:13]=RR_SEQUENCE [12:3]=RESERVED [2:0]=DEGLITCH`

| Bit | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15 | AUTOSCAN_EN | R/W | 0 | `b0`=continuous conversion on single channel (`CONFIG.ACTIVE_CHAN`). `b1`=auto-scan per `RR_SEQUENCE`. |
| 14:13 | RR_SEQUENCE | R/W | 00 | `b00`=Ch0,Ch1. `b01`=Ch0,Ch1,Ch2 (LDC1314 only). `b10`=Ch0,Ch1,Ch2,Ch3 (LDC1314 only). `b11`=Ch0,Ch1. |
| 12:3 | RESERVED | R/W | 00 0100 0001 | Set to this fixed value. |
| 2:0 | DEGLITCH | R/W | 111 | Input deglitch filter bandwidth — select lowest setting exceeding the max sensor oscillation frequency. `b001`=1.0 MHz, `b100`=3.3 MHz, `b101`=10 MHz, `b111`=33 MHz. |

#### 7.6.26 Address 0x1C, RESET_DEV

Bit layout: `[15]=RESET_DEV [14:11]=RESERVED [10:9]=OUTPUT_GAIN [8:0]=RESERVED`

| Bit | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15 | RESET_DEV | R/W | 0 | Write b1 to reset the device. Always reads back 0. |
| 14:11 | RESERVED | R/W | 0000 | Set to b0000. |
| 10:9 | OUTPUT_GAIN | R/W | 00 | `00`=Gain 1 (no shift). `01`=Gain 4 (2-bit shift). `10`=Gain 8 (3-bit shift). `11`=Gain 16 (4-bit shift). |
| 8:0 | RESERVED | R/W | 0x000 | Set to b0 0000 0000. |

#### 7.6.27 Address 0x1E, DRIVE_CURRENT0

Bit layout: `[15:11]=IDRIVE0 [10:6]=INIT_IDRIVE0 [5:0]=RESERVED`

| Bit | Field | Type | Reset | Description |
|---|---|---|---|---|
| 15:11 | IDRIVE0 | R/W | 0 0000 | Channel 0 sensor drive current used during settling + conversion. Requires `RP_OVERRIDE_EN=1`. |
| 10:6 | INIT_IDRIVE0 | R | 0 0000 | Initial drive current measured during Amplitude Calibration; updated after each correction phase if `AUTO_AMP_DIS=0`. Set to b0 0000 when writing IDRIVE0. |
| 5:0 | RESERVED | R/W | 00 0000 | Set to b00 0000. |

#### 7.6.28 Address 0x1F, DRIVE_CURRENT1

Same layout, `IDRIVE1`/`INIT_IDRIVE1`, for Channel 1.

#### 7.6.29 Address 0x20, DRIVE_CURRENT2 (LDC1314 only)

Same layout, `IDRIVE2`/`INIT_IDRIVE2`, for Channel 2.

#### 7.6.30 Address 0x21, DRIVE_CURRENT3 (LDC1314 only)

Same layout, `IDRIVE3`/`INIT_IDRIVE3`, for Channel 3.

#### 7.6.31 Address 0x7E, MANUFACTURER_ID

`[15:0] MANUFACTURER_ID`, R, reset `0101 0100 0100 1001` = `0x5449`.

#### 7.6.32 Address 0x7F, DEVICE_ID

`[15:0] DEVICE_ID`, R, reset `0011 0000 0101 0100` = `0x3054`.

## 8. Application and Implementation

> Note: content in this section is application guidance, not part of the TI component specification — TI does not warrant its accuracy/completeness; customers must validate.

### 8.1 Application Information

#### 8.1.1 Conductive Objects in a Time-Varying EM Field

An AC current through an inductor generates an AC magnetic field. A nearby conductive target develops an induced eddy current, generating its own opposing magnetic field. This is modeled as coupled inductors (sensor = primary, target eddy current = secondary); the coupling is a function of sensor inductance and target resistivity/distance/size/shape.

#### 8.1.2 L-C Resonators

Parallel R-L-C tank model. Oscillation criteria: loop gain > 1, closed-loop phase shift of 2π. At resonance, reactive impedances cancel, leaving only RP (lossy element); voltage amplitude is maximized there.

Sensor oscillation frequency:

```
fSENSOR = 1 / (2π√(LC)) * sqrt(1 − 1/Q² − 5e−9/(Q√(LC)))  ≈  1 / (2π√(LC))
```
where C = CSENSOR + CPARASITIC, L = sensor inductance.

Quality factor: `Q = RP × √(C/L)`, where RP = AC parallel resistance of the LC resonator at the operating frequency.

Sensor current drive must support oscillation at the minimum expected RP (which typically occurs at maximum target interaction/closest distance); both minimum and maximum RP conditions must yield oscillation amplitudes within the device's operating range.

Measured inductance as a function of target distance d:
```
L(d) = Linf − M(d) = 1 / ((2π·fSENSOR)² · C)
```
where Linf = coil inductance with no target (infinite distance), M(d) = mutual inductance, C = CSENSOR + CPARASITIC.

TI's WEBENCH design tool can compute RP, L, C for a coil design.

#### 8.1.3 Multi-Channel and Single Channel Operation

Multi-channel mode lets one channel act as a reference/differential compensation for another (e.g. temperature drift cancellation). Only one channel is active at a time in multi-channel sequencing; inactive channels' INAx/INBx pins are tied to ground. Single-channel mode continuously samples one selectable channel.

**Single/Multi-channel configuration registers**

| Mode | Register | Field | Value |
|---|---|---|---|
| Single channel | CONFIG (0x1A) | ACTIVE_CHAN [15:14] | 00=ch0, 01=ch1, 10=ch2, 11=ch3 |
| Single channel | MUX_CONFIG (0x1B) | AUTOSCAN_EN [15] | 0 = continuous single-channel (default) |
| Multi-channel | MUX_CONFIG (0x1B) | AUTOSCAN_EN [15] | 1 = continuous multi-channel |
| Multi-channel | MUX_CONFIG (0x1B) | RR_SEQUENCE [14:13] | 00=Ch0,Ch1; 01=Ch0,Ch1,Ch2; 10=Ch0,Ch1,Ch2,Ch3 |

Each channel's DATAx represents the ratio of sensor frequency to reference frequency, output as the 12 MSBs of a 16-bit result. With FIN_DIVIDER=1 and OFFSET=0:

```
fsensorx = DATAx * fREFx / 2^12
```

**Sample data registers**: DATA0 (0x00), DATA1 (0x02), DATA2 (0x04, LDC1314 only), DATA3 (0x06, LDC1314 only) — each `[11:0]` = 12-bit conversion result; `0x000`=under-range, `0xFFF`=over-range. `DATAx = DATAx_MSB×65536 + DATAx_LSB`.

##### 8.1.3.1 Data Offset

Subtract an offset to compensate frequency offset / maximize dynamic range. Offset should be `< fSENSORx_MIN / fREFx`, otherwise it may mask the changing LSBs.

| Channel | Register | Value |
|---|---|---|
| 0 | OFFSET0 (0x0C) | fOFFSET0 = OFFSET0 × (fREF0/2^16) |
| 1 | OFFSET1 (0x0D) | fOFFSET1 = OFFSET1 × (fREF1/2^16) |
| 2 | OFFSET2 (0x0E) | fOFFSET2 = OFFSET2 × (fREF2/2^16) |
| 3 | OFFSET3 (0x0F) | fOFFSET3 = OFFSET3 × (fREF3/2^16) |

Full sensor-frequency formula including gain/offset:
```
fSENSORx = FIN_DIVIDERx * fREFx * ( DATAx / 2^(12+OUTPUT_GAIN) + OFFSETx / 2^16 )
```

##### 8.1.3.2 Digital Signal Gain

Internally 16-bit resolution, output word is 12 bits. For systems with sensor signal variation <25% of full scale, `OUTPUT_GAIN` shifts additional LSBs into the visible output word (at the cost of the corresponding MSBs). Use with Data Offset to avoid MSB toggling / data corruption.

| Channel | Register | Field | Values | Effective Resolution | Output Range |
|---|---|---|---|---|---|
| All | RESET_DEV (0x1C) | OUTPUT_GAIN [10:9] | 00 (default): Gain=1 | 12 bits | 100% full scale |
| | | | 01: Gain=4 (2-bit shift) | 14 bits | 25% full scale |
| | | | 10: Gain=8 (3-bit shift) | 15 bits | 12.5% full scale |
| | | | 11: Gain=16 (4-bit shift) | 16 bits | 6.25% full scale |

Example: conversion result 0x07A3, OUTPUT_GAIN=0x0 → reported 0x07A. OUTPUT_GAIN=0x3 → reported 0x7A3 (original MSBs no longer accessible).

#### 8.1.4 Sensor Conversion Time

Configurable 1.2 µs to >26.2 ms, 16-bit resolution, via `RCOUNTx`. Conversion time:
```
tCx = (RCOUNTx × 16 + 4) / fREFx
```

| Channel | Register | Field | Conversion Time |
|---|---|---|---|
| 0 | RCOUNT0 (0x08) | RCOUNT0 [15:0] | (RCOUNT0×16)/fREF0 |
| 1 | RCOUNT1 (0x09) | RCOUNT1 [15:0] | (RCOUNT1×16)/fREF1 |
| 2 | RCOUNT2 (0x0A) | RCOUNT2 [15:0] | (RCOUNT2×16)/fREF2 |
| 3 | RCOUNT3 (0x0B) | RCOUNT3 [15:0] | (RCOUNT3×16)/fREF3 |

Channel switch delay (multi-channel): `692 ns + 5/fref`. A DRDY flag (via INTB) can signal completion for interrupt-driven designs.

##### 8.1.4.1 Settling Time

Multi-channel dwell time per channel = sensor activation time + conversion time + channel switch delay. Settling time:
```
tSx = (SETTLECOUNTx × 16) / fREFx
```

| Channel | Register | Field | Time |
|---|---|---|---|
| 0 | SETTLECOUNT0 (0x10) | SETTLECOUNT0[15:0] | (SETTLECOUNT0×16)/fREF0 |
| 1 | SETTLECOUNT1 (0x11) | SETTLECOUNT1[15:0] | (SETTLECOUNT1×16)/fREF1 |
| 2 | SETTLECOUNT2 (0x12) | SETTLECOUNT2[15:0] | (SETTLECOUNT2×16)/fREF2 |
| 3 | SETTLECOUNT3 (0x13) | SETTLECOUNT3[15:0] | (SETTLECOUNT3×16)/fREF3 |

Constraint: `SETTLECOUNTx ≥ QSENSORx × fREFx / (16 × fSENSORx)`, rounded up, where `Q = RP × √(C/L)`.

##### 8.1.4.2 Sensor Activation

Two modes (via `CONFIG.SENSOR_ACTIVATE_SEL`): faster activation at max drive current (b0, "Full Current"), or lower-power activation using the configured `IDRIVEx` (b1, "Low Power"). No effect if IDRIVEx is already at maximum (b11111).

#### 8.1.5 Sensor Current Drive Configuration

Sensor amplitude should be kept within 1.2 Vp–1.8 Vp (device still converts below that with increasing noise; below ~0.6 Vp oscillation may become unstable/stop). Exceeding 1.8 V activates internal ESD clamping, which can shift the sensor frequency to an invalid state.

**Current drive control registers**

| Channel | Register | Field | Purpose |
|---|---|---|---|
| All | CONFIG (0x1A) | SENSOR_ACTIVATE_SEL [11] | Recommend b0 (Full Current) |
| All | CONFIG (0x1A) | RP_OVERRIDE_EN [12] | Set b1 for normal operation |
| All | CONFIG (0x1A) | AUTO_AMP_DIS [10] | Set b1 for normal operation (disabled) |
| 0 only | CONFIG (0x1A) | HIGH_CURRENT_DRV [6] | b1 = >1.5 mA on Ch0, single-channel mode only |
| 0–3 | DRIVE_CURRENTx (0x1E–0x21) | IDRIVEx [15:11] | Drive current (requires AUTO_AMP_DIS=1, RP_OVERRIDE_EN=1) |
| 0–3 | DRIVE_CURRENTx (0x1E–0x21) | INIT_IDRIVEx [10:6] | Auto-calibration readback, not used in normal operation |

Formula: `IDRIVE = π·VP ÷ (4·RP)`.

**IDRIVEx setting vs. sensor RP (32 steps, 16 µA–1.56 mA)**

| IDRIVEx | Nominal Current (µA) | Min RP (kΩ) | Max RP (kΩ) |
|---|---|---|---|
| 0 (b00000) | 16 | 60.0 | 90.0 |
| 1 (b00001) | 18 | 51.8 | 77.6 |
| 2 (b00010) | 20 | 44.6 | 66.9 |
| 3 (b00011) | 23 | 38.4 | 57.6 |
| 4 (b00100) | 28 | 33.7 | 49.7 |
| 5 (b00101) | 32 | 29.5 | 42.8 |
| 6 (b00110) | 40 | 23.6 | 36.9 |
| 7 (b00111) | 46 | 20.5 | 31.8 |
| 8 (b01000) | 52 | 18.1 | 27.4 |
| 9 (b01001) | 59 | 16.1 | 23.6 |
| 10 (b01010) | 72 | 13.1 | 20.4 |
| 11 (b01011) | 82 | 11.5 | 17.6 |
| 12 (b01100) | 95 | 9.92 | 15.1 |
| 13 (b01101) | 110 | 8.57 | 13.0 |
| 14 (b01110) | 127 | 7.42 | 11.2 |
| 15 (b01111) | 146 | 6.46 | 9.69 |
| 16 (b10000) | 169 | 5.58 | 8.35 |
| 17 (b10001) | 195 | 4.83 | 7.20 |
| 18 (b10010) | 212 | 4.45 | 6.21 |
| 19 (b10011) | 244 | 3.86 | 5.35 |
| 20 (b10100) | 297 | 3.17 | 4.61 |
| 21 (b10101) | 342 | 2.76 | 3.97 |
| 22 (b10110) | 424 | 2.22 | 3.42 |
| 23 (b10111) | 489 | 1.93 | 2.95 |
| 24 (b11000) | 551 | 1.71 | 2.54 |
| 25 (b11001) | 635 | 1.48 | 2.19 |
| 26 (b11010) | 763 | 1.24 | 1.89 |
| 27 (b11011) | 880 | 1.07 | 1.63 |
| 28 (b11100) | 1017 | 0.93 | 1.40 |
| 29 (b11101) | 1173 | 0.80 | 1.21 |
| 30 (b11110) | 1355 | 0.70 | 1.05 |
| 31 (b11111) | 1563 | 0.60 | 0.90 |

RP > 90 kΩ: add a 100 kΩ resistor in parallel with the sensor inductor to reduce effective RP. Wide-RP-range systems may need per-position dynamic IDRIVE, or a parallel resistor to compress the RP range (at the cost of Q).

##### 8.1.5.1 Inactive Channel Sensor Connections

Inactive channels' INAx/INBx are tied to ground (~10 Ω); all channels tied to ground in Sleep/Shutdown. Only the active channel is driven during conversion; unused channels may be left No-Connect.

##### 8.1.5.2 Automatic IDRIVE Setting with RP_OVERRIDE_EN

Auto-calibration procedure for unknown sensor RP: set target at max operating distance → Sleep Mode → program SETTLECOUNT/RCOUNT → enable auto-cal (`RP_OVERRIDE_EN=b0`) → exit Sleep Mode → allow ≥1 measurement with target fixed at max distance → read `INIT_IDRIVEx` from `DRIVE_CURRENTx` and save it → at normal startup, write the saved value into `IDRIVEx` → set `RP_OVERRIDE_EN=b1` for fixed drive during normal operation. Repeat per channel if sensors differ.

##### 8.1.5.3 Determining Sensor IDRIVE via Oscilloscope

Move target to farthest operating distance, measure amplitude on INAx/INBx to ground after it stabilizes; increase IDRIVE if <1.5 Vp, decrease if >1.75 Vp. Repeat per channel if sensors differ.

##### 8.1.5.4 Sensor Auto-Calibration Mode

`AUTO_AMP_DIS=b0` enables dynamic amplitude correction (target 1.2–1.8 V) between conversions, for all active channels; `INIT_IDRIVEx` tracks the live setting. Can introduce output-code offsets on drive changes — not recommended for high-precision applications.

##### 8.1.5.5 Channel 0 High Current Drive

`HIGH_CURRENT_DRV=b1` drives Channel 0 at >3.5 mA typical, for RP <350 Ω sensors. Channel 0 only, single-channel mode only (`AUTOSCAN_EN=0`).

#### 8.1.6 Clocking Architecture

Key clocks: fINx (from sensor, per-channel divided by FIN_DIVIDERx), fREFx (from fCLK, per-channel divided by FREF_DIVIDERx), fCLK (internal oscillator or CLKIN, selected by `CONFIG.REF_CLK_SRC`). Internal oscillator is stable across temperature and suitable for most applications; an external oscillator may be needed for matched multi-device performance or higher long-term stability. Some internal functions (e.g. watchdog) always use fINT.

**Clock frequency requirements**

| Mode | Reference Source | Valid fREFx | Valid fINx | FIN_DIVIDERx | SETTLECOUNTx | RCOUNTx |
|---|---|---|---|---|---|---|
| Multi-Channel | Internal | ≤55 MHz | <fREFx/4 | ≥b0001 (≥2 if fSENSOR≥8.75 MHz) | >3 | >8 |
| Multi-Channel | External | ≤40 MHz | | | | |
| Single-Channel | Either | ≤35 MHz | | | | |

**Clock configuration registers**

| Channel | Clock | Register | Field | Formula |
|---|---|---|---|---|
| All | fCLK source | CONFIG (0x1A) | REF_CLK_SRC [9] | b0=internal, b1=CLKIN |
| 0 | fREF0 | CLOCK_DIVIDERS0 (0x14) | FREF_DIVIDER0 [9:0] | fREF0 = fCLK / FREF_DIVIDER0 |
| 1 | fREF1 | CLOCK_DIVIDERS1 (0x15) | FREF_DIVIDER1 [9:0] | fREF1 = fCLK / FREF_DIVIDER1 |
| 2 | fREF2 | CLOCK_DIVIDERS2 (0x16) | FREF_DIVIDER2 [9:0] | fREF2 = fCLK / FREF_DIVIDER2 |
| 3 | fREF3 | CLOCK_DIVIDERS3 (0x17) | FREF_DIVIDER3 [9:0] | fREF3 = fCLK / FREF_DIVIDER3 |
| 0 | fIN0 | CLOCK_DIVIDERS0 (0x14) | FIN_DIVIDER0 [15:12] | fIN0 = fSENSOR0 / FIN_DIVIDER0 |
| 1 | fIN1 | CLOCK_DIVIDERS1 (0x15) | FIN_DIVIDER1 [15:12] | fIN1 = fSENSOR1 / FIN_DIVIDER1 |
| 2 | fIN2 | CLOCK_DIVIDERS2 (0x16) | FIN_DIVIDER2 [15:12] | fIN2 = fSENSOR2 / FIN_DIVIDER2 |
| 3 | fIN3 | CLOCK_DIVIDERS3 (0x17) | FIN_DIVIDER3 [15:12] | fIN3 = fSENSOR3 / FIN_DIVIDER3 |

#### 8.1.7 Input Deglitch Filter

Suppresses EMI/ringing above the sensor frequency; does not affect conversion result if set above the max sensor frequency. Applies to all channels via `MUX_CONFIG.DEGLITCH`. Recommend the lowest setting exceeding the highest active sensor frequency.

| MUX_CONFIG.DEGLITCH | Frequency |
|---|---|
| b001 | 1.0 MHz |
| b100 | 3.3 MHz |
| b101 | 10 MHz |
| b011 | 33 MHz |

#### 8.1.8 Device Status Registers

`STATUS` (0x18) and `ERROR_CONFIG` (0x19) — see §7.6.22–7.6.23. To trigger INTB: unmask the relevant bit in ERROR_CONFIG and set `CONFIG.INTB_DIS=0`. STATUS bit fields are held until STATUS or the corresponding DATAx is read (which also de-asserts INTB). Interrupts also clear on: entering Sleep Mode, POR, Shutdown Mode, software reset. After first starting conversions, perform the first STATUS read after INTB asserts. `CONFIG.INTB_DIS=b1` disables INTB (held high).

#### 8.1.9 Multi-Channel Data Readback

In multi-channel mode, each channel's DATAx is overwritten when its next conversion completes. INTB pulls low (if `DRDY_2INT=1`) when the last channel in the sequence completes; results should be read promptly. Sleep/Shutdown clears all DATAx.

Race condition (textual description of the datasheet's readback-timing figure): if I2C readback of a completed channel's data is delayed past the next conversion of that same channel, the earlier conversion is silently overwritten before it's read — this can happen per-channel independently. Monitoring `UNREADCONVx` in STATUS is the way to detect this loss; a delayed STATUS read after INTB can show Channel 0 with no unread data while other channels still do, indicating Channel 0's result was overwritten before being read.

### 8.2 Typical Application

#### 8.2.1 System Sensing Functionality

Applications: Angular measurement, linear position sensing (differential 2-channel recommended for absolute positioning), encoder knobs, inductive buttons/keypads via snap-domes.

#### 8.2.2 Example Application

Multi-channel LDC1312 example: Channel 0 for proximity measurement, Channel 1 for temperature/environmental compensation (reference sensor). Standard hookup: CLKIN from 40 MHz oscillator (or tied low for internal osc.), VDD/GND, SD and INTB to MCU GPIOs, SDA/SCL to MCU I2C with pull-ups, ADDR tied for address selection.

#### 8.2.3 Design Requirements (worked example)

Target distance 1.0 mm, distance resolution 0.2 µm, target diameter 10 mm, target material stainless steel (SS416), 2-layer PCB coil, 1 kSPS (TSAMPLE=1.00 ms).

#### 8.2.4 Detailed Design Procedure (worked example)

WEBENCH-derived coil: 2-layer, 2.5 cm² area, 17.7 mm diameter, 39 turns; RP=6.6 kΩ, L=43.9 µH, C=100 pF → fSENSOR = 1/(2π√(LC)) ≈ 2.4 MHz. With fCLKIN=40 MHz on Channel 0 (IN0A/IN0B):

1. **Dividers**: fSENSOR<8.75 MHz → FIN_DIVIDER0=1. fREF0 constraint `>4×fSENSOR` satisfied by 40 MHz → FREF_DIVIDER0=1. CLOCK_DIVIDERS0 (0x14) = 0x1002.
2. **Settling time**: coil Q≈10. `SETTLECOUNT0 ≥ Q×fREF0/(16×fSENSOR0)` → 5.2, rounded up with margin to 10. `tS0 = (10×16)/20,000,000 = 8 µs`. SETTLECOUNT0 (0x10) = 0x000A. *(Note: worked example uses fREF0=20 MHz in this step, differing from the 40 MHz set above — treat as datasheet-example inconsistency, not a device constraint.)*
3. **Channel switch delay**: ~1 µs for fREF=20 MHz.
4. **Conversion time**: budget = TSAMPLE − settle − switch = 1000 − 8 − 1 = 991 µs. Solve `tC0=(RCOUNT0×16)/fREF0` → RCOUNT0=1238 decimal (rounded down) = 0x04D6.
5. **ERROR_CONFIG** (0x19): default 0x0000 (no interrupts enabled); can be changed to report status/errors.
6. **Drive current**: RP=6.6 kΩ → IDRIVE0=18 decimal from the IDRIVEx table; INIT_IDRIVE0=0x00. DRIVE_CURRENT0 (0x1E) = 0x9000.
7. **MUX_CONFIG** (0x1B): AUTOSCAN_EN=b1 (sequential), RR_SEQUENCE=b00 (Ch0,Ch1), DEGLITCH=b100 (3.3 MHz, lowest exceeding tank frequency) → combined 0x820C.
8. **CONFIG** (0x1A): ACTIVE_CHAN=b00, SLEEP_MODE_EN=b0 (enable conversion), RP_OVERRIDE_EN=b1, SENSOR_ACTIVATE_SEL=b0 (full current activation), AUTO_AMP_DIS=b1, REF_CLK_SRC=b1 (external clock) → combined 0x1601.

Then read DATA0/DATA1 every 1.00 ms from 0x00/0x02.

#### 8.2.5 Recommended Initial Register Configuration Values

**Single-Channel Operation**

| Address | Value | Register | Comment |
|---|---|---|---|
| 0x08 | 0x04D6 | RCOUNT0 | From timing/resolution requirements (1 kSPS) |
| 0x10 | 0x000A | SETTLECOUNT0 | Minimum settling time for chosen sensor |
| 0x14 | 0x1002 | CLOCK_DIVIDERS0 | FIN_DIVIDER0=1, FREF_DIVIDER0=2 |
| 0x19 | 0x0000 | ERROR_CONFIG | Default; can enable status/error reporting |
| 0x1B | 0x020C | MUX_CONFIG | Ch0 continuous, deglitch 3.3 MHz |
| 0x1E | 0x9000 | DRIVE_CURRENT0 | Sensor drive current, Ch0 |
| 0x1A | 0x1601 | CONFIG | Select ch0, disable auto-amp/auto-cal, full current activation, external clock, wake device. **Write last** — device configuration is not permitted while active. |

**Multi-Channel Operation**

| Address | Value | Register | Comment |
|---|---|---|---|
| 0x08 | 0x04D6 | RCOUNT0 | From timing/resolution requirements |
| 0x09 | 0x04D6 | RCOUNT1 | From timing/resolution requirements |
| 0x10 | 0x000A | SETTLECOUNT0 | Minimum settling time |
| 0x11 | 0x000A | SETTLECOUNT1 | Minimum settling time |
| 0x14 | 0x1002 | CLOCK_DIVIDERS0 | FIN_DIVIDER0=1, FREF_DIVIDER0=2 |
| 0x15 | 0x1002 | CLOCK_DIVIDERS1 | FIN_DIVIDER1=1, FREF_DIVIDER1=2 |
| 0x19 | 0x0000 | ERROR_CONFIG | Default; can enable |
| 0x1B | 0x820C | MUX_CONFIG | Ch0+Ch1 sequential, deglitch 3.3 MHz |
| 0x1E | 0x9000 | DRIVE_CURRENT0 | Sensor drive current, Ch0 |
| 0x1F | 0x9000 | DRIVE_CURRENT1 | Sensor drive current, Ch1 |
| 0x1A | 0x1601 | CONFIG | Disable auto-amp/auto-cal, full current activation, external clock, wake device. **Write last.** |

#### 8.2.6 Application Curves

Typical output code vs. target distance and measurement precision vs. distance graphs (test conditions: 14 mm PCB coil, L=19.4 µH, RP=5.7 kΩ@2 MHz, 330 pF cap, aluminum 1.5 mm target). Described qualitatively: output code decreases monotonically as target distance increases (from ~3400 to ~1100 counts over 0–100% of target-distance/coil-diameter ratio); measurement precision degrades (larger µm error) as target distance increases, from <0.25 µm near the sensor to ~2 µm at 70% of the sensing range.

#### 8.2.7 Inductor Self-Resonant Frequency (SRF)

Every inductor has parasitic capacitance; at SRF the inductor's reactance cancels the parasitic capacitance's reactance, and above SRF the inductor behaves capacitively. Recommendation: `fSENSOR < 0.8 × fSR`. Example in the datasheet: SRF=6.38 MHz → do not exceed 5.1 MHz.

## 9. Power Supply Recommendations

- Supply must be within 2.7 V–3.6 V. Use a 1 µF X7R ceramic bypass capacitor between VDD and GND; add a 10 µF ceramic capacitor if the supply is more than a few inches away.
- Place bypass capacitors as close as possible to the VDD/GND pins; minimize the loop area formed by the bypass capacitor, VDD pin, and GND pin.

## 10. Layout

### 10.1 Layout Guidelines

- Avoid long traces between sensor and LDC; higher-frequency sensors need closer placement to minimize noise.
- Route INAx/INBx as differential pairs (parallel, close together); lower trace impedance is acceptable/beneficial (reduces parasitic inductance).
- Place the sensor capacitor close to the inductor to minimize sensor RP.
- No filled ground/power planes underneath or between sensor layers; if a plane is nearby, keep ≥20% of the sensor diameter as a gap from the outermost coil turn.
- Avoid any continuous conductive ring encircling the sensor (break it with a small cut if unavoidable).

### 10.2 Layout Example

Example PCB (textual description): LDC1312 placed with a ground-plane fill on the controller side, two circular multi-turn PCB coil sensors ("SENSOR CH0", "SENSOR CH1") placed away from the ground plane, with bypass capacitors (Cbypass1/2) close to VDD, and per-channel sensor capacitors (Csensor0/1) placed close to each coil.

## 11. Device and Documentation Support (relevant subset)

### 11.2.1 Related Documentation

- LDC1000 Temperature Compensation — SNAA212
- LDC Sensor Design — SNOA930 *(no usable local Markdown source; see knowledge base open questions)*
- LDC1612/LDC1614 Linear Position Sensing Application Note — SNOA931
- Optimizing L Measurement Resolution for the LDC1312 and LDC1314 — SNOA945 (`docs/snoa945.md`)
- Power Reduction Techniques for the LDC131x/161x for Inductive Sensing — SNOA949
- Optimizing L Measurement Resolution for the LDC161x and LDC1101 — SNOA950 *(note: LDC1312/1314 sensor-drive app note SNOA950 in `docs/snoa950.pdf` is actually titled "Setting LDC1312/4, LDC1612/4, and LDC1101 Sensor Drive Configuration" — same literature number, different subject; TI's own cross-reference list appears to have a typo here)*
- Inductive Sensing Touch-On-Metal Buttons Design Guide — SNOA951
- LDC Target Design — SNOA957
- LDC1312, LDC1314, LDC1612, LDC1614 Sensor Status Monitoring — SNOA959 (`docs/snoa959.md`)

### 11.7 Electrostatic Discharge Caution

Limited built-in ESD protection. Short leads together or store in conductive foam to prevent electrostatic damage to the MOS gates.

---

*Omitted from this conversion (no technical/implementation content): Revision History (§4), Mechanical/Packaging/Orderable Information (§12), Package Option Addendum, Package Materials Information (tape/reel/stencil drawings), and the TI Important Notice/legal disclaimer pages.*
