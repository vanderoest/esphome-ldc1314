# LDC1314 ESPHome Component

A generic ESPHome driver for the Texas Instruments LDC1314 4-channel inductance-to-digital
converter (I2C). It publishes **raw** per-channel conversion values and per-channel error
diagnostics — it does not compute distance, flow, or any other application-specific quantity. See
`design_decisions.md` for why that boundary exists.

**v1 scope:** the LDC1314 only. The LDC1312 shares this exact register map (fewer channels) and
would be cheap to add later; the LDC1612/LDC1614 use a different, undocumented-here register
layout and are out of scope entirely for now — see `design_decisions.md` for the reasoning.

Register-level background: `register_map.md` and `docs/knowledge_base.md`. This README only
covers using the component — it does not repeat register documentation.

## Installing

Reference this repository directly as an external component:

```yaml
external_components:
  - source: github://vanderoest/esphome-ldc1314
    components: [ldc1314]
```

Pin to a specific commit/tag/branch with `source: github://vanderoest/esphome-ldc1314@<ref>` once
this has releases worth pinning to. For local development against a checked-out copy of this repo
instead, use:

```yaml
external_components:
  - source:
      type: local
      path: components
```

> If you're iterating against a real board with the GitHub source, add `refresh: 0s` to the
> `external_components:` entry while you're actively testing register changes — ESPHome caches the
> GitHub fetch, and without it a stale build can silently survive several reflashes. `refresh: 0s`
> forces a re-fetch on every compile.

## Wiring

Connect the LDC1314's `SDA`/`SCL` to your board's I2C bus, and up to 4 LC sensors to `IN0A/IN0B`
… `IN3A/IN3B`. The `ADDR` pin selects the I2C address (low = `0x2A`, high = `0x2B`) and must not
be left floating.

## Minimal example

```yaml
i2c:
  sda: GPIO21
  scl: GPIO22

external_components:
  - source: github://vanderoest/esphome-ldc1314
    components: [ldc1314]

ldc1314:
  - id: ldc

sensor:
  - platform: ldc1314
    ldc1314_id: ldc
    channel: 0
    name: "LDC Channel 0 Raw"
```

This publishes the raw 12-bit conversion code (0–4095) for channel 0, polled at the hub's default
60s `update_interval`. On its own this isn't very useful — see "Tuning per-channel settings"
below for a real sensor.

## Multi-channel example

Add one `sensor:` entry per channel you want measured; the hub configures multi-channel
(auto-scan) mode automatically once more than one channel has an entity registered on it.

```yaml
ldc1314:
  - id: ldc
    update_interval: 10s

sensor:
  - platform: ldc1314
    ldc1314_id: ldc
    channel: 0
    name: "LDC Channel 0 Raw"
    rcount: 0x04D6
    settlecount: 0x000A
    idrive: 18
  - platform: ldc1314
    ldc1314_id: ldc
    channel: 1
    name: "LDC Channel 1 Raw"
    rcount: 0x04D6
    settlecount: 0x000A
    idrive: 18
  - platform: ldc1314
    ldc1314_id: ldc
    channel: 2
    name: "LDC Channel 2 Raw"
    rcount: 0x04D6
    settlecount: 0x000A
    idrive: 18
```

> The device only scans a contiguous run of channels starting at channel 0 (hardware limitation,
> not this component's choice). If you configure e.g. channels 0 and 2 but not 1, channel 1 is
> still measured by the hardware (spending its settling/conversion time) even though nothing is
> published for it — the component logs a warning about this at startup.

## Diagnostics

Add an optional `binary_sensor: platform: ldc1314` entry per channel to expose a `problem`
binary sensor that turns on when that channel's most recent conversion had an under-range,
over-range, watchdog-timeout, or amplitude-warning error flag set:

```yaml
binary_sensor:
  - platform: ldc1314
    ldc1314_id: ldc
    channel: 0
    name: "LDC Channel 0 Error"
  - platform: ldc1314
    ldc1314_id: ldc
    channel: 1
    name: "LDC Channel 1 Error"
  - platform: ldc1314
    ldc1314_id: ldc
    channel: 2
    name: "LDC Channel 2 Error"
```

Errors are also logged at `DEBUG` level, rate-limited to one line per error-state transition plus
a periodic per-channel error count every 10s — not one line per errored conversion, which at a
100 Hz `update_interval` with a persistently-erroring channel would otherwise be hundreds of lines
a second. Communication failures and setup problems log at `WARNING`/`ERROR` and are not
rate-limited. `STATUS` is read on its own 1s cadence independent of `update_interval` (it's
diagnostic-only and read-to-clear) and decoded per-bit, which is the only place amplitude-**high**
and amplitude-**low** are distinguishable — `DATAx.ERR_AE` ORs the two together.

### Error flags do not suppress measurements

The component always publishes the measured `DATA` registers, regardless of `STATUS` flags.
`STATUS` is exposed as diagnostic information only. Some sensor designs, including the
investigated HomeWizard watermeter, may report persistent `ERR_ALE` while still producing stable
and useful frequency measurements.

The single exception is `DATAx.ERR_WD` (watchdog timeout), where the datasheet states the
conversion result is invalid; those samples publish `NAN` rather than a wrong number. Under-range,
over-range and amplitude flags all publish normally and only raise the `problem` binary sensor.

### Datasheet conformance report

Add the `button:` platform to get an on-demand check of whether the current configuration is
inside the datasheet's recommendations:

```yaml
button:
  - platform: ldc1314
    ldc1314_id: ldc
    name: "LDC diagnostics"
```

Pressing it measures the active channels and logs:

```
 Observed
   fCLK  43.40 MHz  (internal oscillator, nominal)
              fREF    DATA    ratio     fSENSOR    Q max
   CH0   21.70 MHz     932   0.2275   4.938 MHz     36.4
   CH1   21.70 MHz     936   0.2285   4.959 MHz     36.6
   CH2   21.70 MHz     970   0.2368   5.139 MHz     37.9
   DEGLITCH configured   3.3 MHz
   amplitude error   currently asserted on CH0 CH1 CH2

 Datasheet
   8.1.7   DEGLITCH must be the lowest setting exceeding the highest
           active sensor frequency.
           3.3 MHz vs 5.14 MHz max sensor freq          NOT MET
   8.1.6   Multi-channel requires fIN < fREF/4, i.e. DATA < 1024.
           max DATA 970, 54 codes of margin (94.7%)     MET
   8.1.6   SETTLECOUNT >= Q*fREF/(16*fSENSOR).
           supports coil Q up to 36.4 (CH0 binding)
           the coil's actual Q is not measurable here   NOT EVALUATED

 Suggested next experiment
   Set  deglitch: 10MHZ   -- lowest setting exceeding 5.14 MHz.
```

It reports measurements and datasheet rules, never a diagnosis: it will tell you a constraint is
not met and suggest an experiment, but it will not claim to know why a sensor misbehaves. Where an
input genuinely isn't available it prints `NOT EVALUATED` rather than guessing — coil Q cannot be
read from registers, and `fCLKIN` is unknown when `reference_clock: external`, so the
frequency-dependent checks are skipped in that case. The `DATA < 1024` and `Q max` checks cancel
`fCLK` out entirely and stay exact regardless.

It is a snapshot of whatever the target is doing when you press the button. That is exact for the
configuration checks — they depend on the sensor frequency, not the target position — but the
amplitude-error line reflects one instant.

### Raw trace capture

Add the `switch:` platform to get an on/off toggle for a `TRACE,<ts>,<ch0>,<ch1>,<ch2>` log line
(one per `update()` cycle, over whichever channels are active):

```yaml
switch:
  - platform: ldc1314
    ldc1314_id: ldc
    name: "LDC trace capture"
```

Flip it on before a capture session and off after — no reflash either direction, and unlike
raising the global `logger:` level it doesn't pull in every other component's logging. It logs at
`DEBUG`, so `logger: level: DEBUG` (the default) is enough; a capture is then just

```bash
esphome logs your-config.yaml | grep -oE 'TRACE,.*' | tee trace.csv
```

The switch defaults to off and does not restore across a reboot (`restore_mode: ALWAYS_OFF`) — a
capture session is something you start deliberately each time.

## Tuning per-channel settings

The register-valued options below map directly to the datasheet registers of the same purpose
(`register_map.md` has the full field-level reference); there are no "friendly units" in v1 —
raw register values are simpler to keep correct and match the datasheet 1:1. Friendlier units
(e.g. settle time in µs) may come in a later revision.

| Option | Register | Range | Default |
|---|---|---|---|
| `rcount` | `RCOUNTx` — conversion time | 5–65535 | `0x0080` |
| `settlecount` | `SETTLECOUNTx` — settling time | 0–65535 | `0x0000` |
| `offset` | `OFFSETx` — output offset | 0–65535 | `0x0000` |
| `fin_divider` | `CLOCK_DIVIDERSx.FIN_DIVIDER` | 1–15 | `1` |
| `fref_divider` | `CLOCK_DIVIDERSx.FREF_DIVIDER` | 1–1023 | `1` |
| `idrive` | `DRIVE_CURRENTx.IDRIVE` — sensor drive current | 0–31 | `0` |

Getting these right for a real sensor requires working through the datasheet's worked example
(`docs/LDC1314_datasheet.md` §8.2.4) or the summarized version in
`docs/summaries/optimizing_resolution_summary.md` /
`docs/summaries/drive_configuration_summary.md`. In short:
- `rcount` and the reference clock together set your conversion time/resolution tradeoff.
- `settlecount` must be large enough for your sensor's Q factor at your reference frequency, or
  you'll see spurious amplitude errors.
- `idrive` must be tuned to your sensor's RP so the oscillation amplitude lands in the
  recommended 1.2 V–1.8 V range — see `docs/summaries/drive_configuration_summary.md` for the
  manual bring-up procedure. Note that not every coil can reach that window: see
  `design_decisions.md` "Persistent amplitude errors are tolerated, not chased".

## Hub-level options

| Option | Purpose | Default |
|---|---|---|
| `address` | I2C address | `0x2A` |
| `update_interval` | Poll interval for all channels | `60s` |
| `reference_clock` | `INTERNAL` or `EXTERNAL` (CLKIN) | `INTERNAL` |
| `deglitch` | Input deglitch filter: `1MHZ` / `3_3MHZ` / `10MHZ` / `33MHZ` | `10MHZ` |
| `output_gain` | Digital output gain: `1`, `4`, `8`, or `16` | `1` |
| `high_current_drive` | Channel-0-only >1.5mA drive (single-channel mode only) | `false` |
| `sensor_activation_mode` | `FULL_CURRENT` or `LOW_POWER` sensor startup | `FULL_CURRENT` |
| `report_errors_on_intb` | Enable INTB pin assertion on errors (the component doesn't wire an interrupt itself — see `design_decisions.md` "PollingComponent, not interrupt-driven") | `false` |

### Choosing `deglitch`

Datasheet §8.1.7: use the lowest setting that **exceeds** the highest active sensor frequency.
Set below it, the filter attenuates the sensor signal itself.

For the HomeWizard watermeter we recommend `deglitch: 10MHZ`, because the measured sensor
frequency is approximately 5 MHz and this follows TI's recommendation. That is also the
component's default, since it suits the ~2–8 MHz coils this driver is typically used with.

A coil oscillating above 10 MHz needs `33MHZ` — the default is a recommendation for the common
case, not a value that is safe for every coil. The diagnostics button above tells you which side
of the rule your coil is on.

## What this component will never do

- Convert raw values to distance, liters, or flow rate.
- Combine multiple channels into an application-specific decoded quantity (e.g. a rotational
  position decoder for a spinning target).
- Run sensor-drive auto-calibration. Operation is always fixed-drive (`RP_OVERRIDE_EN=1`,
  `AUTO_AMP_DIS=1`): the device's auto-amplitude mode can adjust drive mid-measurement and inject
  an offset that reads as a step in target position (SNOA950 §4).
- Store any configuration of its own. Every register written comes from YAML — there is no
  calibration layer, no NVS state, and nothing to clear.

See `design_decisions.md` for the reasoning, and wire that logic in your own
`template`/`sensor.filter`/lambda code consuming this component's raw sensor entities instead.
