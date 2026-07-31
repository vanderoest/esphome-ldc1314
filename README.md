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
```

Errors are also logged (`DEBUG` level for per-conversion errors, `WARNING`/`ERROR` for
communication failures and setup problems).

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
  bring-up procedure (this is a documented manual calibration step, not something the component
  automates — see `design_decisions.md`).

## Hub-level options

| Option | Purpose | Default |
|---|---|---|
| `address` | I2C address | `0x2A` |
| `update_interval` | Poll interval for all channels | `60s` |
| `reference_clock` | `INTERNAL` or `EXTERNAL` (CLKIN) | `INTERNAL` |
| `deglitch` | Input deglitch filter: `1MHZ` / `3_3MHZ` / `10MHZ` / `33MHZ` | `33MHZ` |
| `output_gain` | Digital output gain: `1`, `4`, `8`, or `16` | `1` |
| `high_current_drive` | Channel-0-only >1.5mA drive (single-channel mode only) | `false` |
| `sensor_activation_mode` | `FULL_CURRENT` or `LOW_POWER` sensor startup | `FULL_CURRENT` |
| `report_errors_on_intb` | Enable INTB pin assertion on errors (the component doesn't wire an interrupt itself — see `design_decisions.md` "PollingComponent, not interrupt-driven") | `false` |

## What this component will never do

- Convert raw values to distance, liters, or flow rate.
- Combine multiple channels into an application-specific decoded quantity (e.g. a rotational
  position decoder for a spinning target).
- Run automatic sensor-drive auto-calibration during normal operation.

See `design_decisions.md` for the reasoning, and wire that logic in your own
`template`/`sensor.filter`/lambda code consuming this component's raw sensor entities instead.
