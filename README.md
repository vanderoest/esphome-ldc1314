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
| `offset` | `OFFSETx` — output offset (YAML seed only — see "Characterization") | 0–65535 | `0x0000` |
| `fin_divider` | `CLOCK_DIVIDERSx.FIN_DIVIDER` | 1–15 | `1` |
| `fref_divider` | `CLOCK_DIVIDERSx.FREF_DIVIDER` | 1–1023 | `1` |
| `idrive` | `DRIVE_CURRENTx.IDRIVE` — sensor drive current (YAML seed only — see "Characterization") | 0–31 | `0` |

Getting these right for a real sensor requires working through the datasheet's worked example
(`docs/LDC1314_datasheet.md` §8.2.4) or the summarized version in
`docs/summaries/optimizing_resolution_summary.md` /
`docs/summaries/drive_configuration_summary.md`. In short:
- `rcount` and the reference clock together set your conversion time/resolution tradeoff.
- `settlecount` must be large enough for your sensor's Q factor at your reference frequency, or
  you'll see spurious amplitude errors.
- `idrive` must be tuned to your sensor's RP so the oscillation amplitude lands in the
  recommended 1.2 V–1.8 V range — see `docs/summaries/drive_configuration_summary.md` for the
  manual bring-up procedure, or "Characterization" below to have the component derive it for you.

## Characterization

Deriving `idrive`/`offset`/`output_gain` for a coil you've never measured used to mean an
oscilloscope, or the manual sweep above repeated across many reflashes. The characterization
framework automates it: point it at whatever target motion your application has, and it derives
and stores those three values.

**What it derives, and what it doesn't.** `idrive`, `offset` and `output_gain` are automated;
`rcount`/`settlecount`/`fin_divider`/`fref_divider` are not. TI's own recommended values for those
are almost always right, and a bad `settlecount` is something the run detects and diagnoses rather
than silently overrides — see `design_decisions.md`.

**It never touches litres, flow, or rotations.** What it derives are LDC1314 operating parameters
— sensor drive current, output range placement — never a physical quantity for the application's
target. It is exactly as generic as the rest of this component.

### Triggering a run

Any of these calls the same internal routine — there is no special "the button did it" path:

```yaml
button:
  - platform: ldc1314
    ldc1314_id: ldc
    characterize:
      name: "LDC characterize"
```

```yaml
- ldc1314.characterize: ldc
```

A physical pushbutton needs no component support at all — wire it as a plain GPIO
`binary_sensor:` and call the same action:

```yaml
binary_sensor:
  - platform: gpio
    pin: GPIO0
    on_multi_click:
      - timing:
          - ON for at least 3s
        then:
          - ldc1314.characterize: ldc
```

### What happens during a run

A run takes a few minutes and needs exactly two things from you, at exactly two moments — it
tells you when:

1. Keep the target still for the idle baseline.
2. **Move the target through its complete range of motion** when prompted, and keep it moving
   until told to stop.

Internally: TI's auto-amplitude mode observes the sensor across the full motion to find the
minimum stable drive current (the datasheet's own "maximum operating distance" calibration
condition, reached without needing to physically hold a moving target still); the drive is then
fixed and the resulting code range measured to pick the best `output_gain` and per-channel
`offset`; the final configuration is verified for clipping before being stored. Progress logs
live at every step, and a full report is printed on completion (or on abort) — see `.plan` for the
exact stages and the reasoning behind each derivation, most importantly why the chosen IDRIVE is
the *minimum* observed rather than an average.

### Pre-flight: is the configuration even within the datasheet?

Once the idle baseline is captured, and before any drive current is touched, the run prints a
**pre-flight conformance report**. It measures what the coils are actually doing under the
configuration currently in force, states the datasheet rule bearing on each measurement, and says
whether that rule is met:

```
 Observed
   fCLK  43.40 MHz  (internal oscillator, nominal)
              fREF    DATA    ratio     fSENSOR    Q max
   CH0   21.70 MHz     937   0.2288   4.964 MHz     36.6
   CH1   21.70 MHz     928   0.2266   4.916 MHz     36.2
   CH2   21.70 MHz     977   0.2385   5.176 MHz     38.2
   DEGLITCH configured   3.3 MHz
   amplitude error   currently asserted on CH0 CH1 CH2

 Datasheet
   8.1.7   DEGLITCH must be the lowest setting exceeding the highest
           active sensor frequency.
           3.3 MHz vs 5.18 MHz max sensor freq          NOT MET
   8.1.6   Multi-channel requires fIN < fREF/4, i.e. DATA < 1024.
           max DATA 977, 47 codes of margin (95.4%)     MET
   8.1.6   SETTLECOUNT >= Q*fREF/(16*fSENSOR).
           supports coil Q up to 36.2 (CH1 binding)
           the coil's actual Q is not measurable here   NOT EVALUATED

 Suggested next experiment
   Set  deglitch: 10MHZ   -- lowest setting exceeding 5.18 MHz.
```

The report states measurements and rules, never a diagnosis: it will tell you a constraint is not
met and suggest an experiment, but it will not claim to know why your sensor misbehaves. Where an
input genuinely isn't available it prints `NOT EVALUATED` rather than guessing — coil Q cannot be
read from registers, and `fCLKIN` is unknown when `reference_clock: external`, so the
frequency-dependent checks are skipped in that case. The `DATA < 1024` and `Q max` checks cancel
`fCLK` out entirely and stay exact regardless.

Pre-flight never aborts the run. A configuration outside the datasheet recommendations is still
worth characterizing, if only to compare against a corrected one.

It runs only inside a characterization run, never at boot: the numbers only mean what the report
says they mean once Stage 0 has established a controlled, target-still baseline. At boot the
target sits wherever it happens to be.

### When a run stops early

If the device's own auto-calibration sits pinned at an endpoint of its drive range — `INIT_IDRIVE`
31 with an amplitude-low error, or 0 with amplitude-high — on **every** active channel
*continuously* for 8 seconds, the run aborts immediately instead of finishing the stage. The
device has already swept its whole range, so no `idrive` value can help and there is nothing to be
learned from waiting.

The abort report names which datasheet checks are not met, lists the other constraints that bear
on amplitude, and suggests the next single variable to change. The 8-second window is an internal
constant, not an option: the condition must hold *without interruption*, so the auto-amplitude
loop always gets the full window to move from wherever it is, and there is nothing to tune.

### Guided prompts are configuration, not code

The two messages above are literal, target-neutral defaults and can be overridden per
application:

```yaml
ldc1314:
  - id: ldc
    characterization:
      prompts:
        start: "Open the faucet fully now and leave it running"
        stop: "You can close the faucet now"
```

This is the one place application-specific wording is allowed into an otherwise fully generic
component — it's a string, not a code path, and the component's own behavior never depends on it.

### Persistence and the three-layer value model

A completed run is stored in flash and restored automatically on every boot, taking precedence
over the `idrive`/`offset`/`output_gain` values written in YAML. Those YAML values are never
lost — they're the fallback used until a characterization exists, or after clearing one:

```yaml
- ldc1314.clear_characterization: ldc
```

On top of that sits a manual override layer, edited through `number` entities and applied only
once armed by a `switch`:

```yaml
number:
  - platform: ldc1314
    ldc1314_id: ldc
    channel: 0
    idrive:
      name: "LDC CH0 IDRIVE override"
    offset:
      name: "LDC CH0 OFFSET override"
  - platform: ldc1314
    ldc1314_id: ldc
    output_gain:
      name: "LDC output gain override"

switch:
  - platform: ldc1314
    ldc1314_id: ldc
    override:
      name: "LDC manual override"
      entity_category: config
```

Editing a number never changes anything by itself — it only takes effect once the switch is
turned on, and turning the switch back off reverts to the calibrated (or YAML) values exactly and
instantly, with the manual values preserved for next time. `ldc1314.clear_overrides` resets the
manual layer back to the calibrated values without touching the switch.

Diagnostic sensors publish what a characterization run actually derived, independent of whatever
the manual layer or switch currently show:

```yaml
sensor:
  - platform: ldc1314
    ldc1314_id: ldc
    channel: 0
    name: "LDC Channel 0 Raw"
    calibrated_idrive:
      name: "LDC CH0 calibrated IDRIVE"
    calibrated_offset:
      name: "LDC CH0 calibrated OFFSET"

ldc1314:
  - id: ldc
    characterization:
      calibrated_output_gain:
        name: "LDC calibrated output gain"
```

`dump_config()` always logs the fully resolved chain for every value with its source (`yaml` /
`calibrated` / `MANUAL, ARMED`) — the authoritative place to check what's actually running.

### Full characterization option reference

| Option | Purpose | Default |
|---|---|---|
| `restore` | Apply and persist the result. `false` = measure and report only, nothing is changed | `true` |
| `idle_duration` | Stage 0 baseline observation time, target still | `10s` |
| `stage_duration` | Fixed observation time for the drive-current and envelope stages | `60s` |
| `verify_duration` | Final verification observation time | `30s` |
| `sample_interval` | Time between samples within a stage | `20ms` |
| `progress_interval` | How often live progress is logged during a stage | `5s` |
| `headroom` | Full-scale margin reserved when choosing `output_gain` | `25%` |
| `unify_channels` | Use one IDRIVE across all channels (recommended for nominally identical coils — TI SNOA950 §8) | `false` |
| `prompts.start` / `prompts.stop` | What to tell the user, and when | target-neutral text |
| `calibrated_output_gain` | Diagnostic sensor for the derived global gain | none |

See `.plan` for the full staged procedure, the derivation arithmetic, and the reasoning behind
every default.

## Hub-level options

| Option | Purpose | Default |
|---|---|---|
| `address` | I2C address | `0x2A` |
| `update_interval` | Poll interval for all channels | `60s` |
| `reference_clock` | `INTERNAL` or `EXTERNAL` (CLKIN) | `INTERNAL` |
| `deglitch` | Input deglitch filter: `1MHZ` / `3_3MHZ` / `10MHZ` / `33MHZ` | `33MHZ` |
| `output_gain` | Digital output gain: `1`, `4`, `8`, or `16` (YAML seed only — see "Characterization") | `1` |
| `high_current_drive` | Channel-0-only >1.5mA drive (single-channel mode only) | `false` |
| `sensor_activation_mode` | `FULL_CURRENT` or `LOW_POWER` sensor startup | `FULL_CURRENT` |
| `report_errors_on_intb` | Enable INTB pin assertion on errors (the component doesn't wire an interrupt itself — see `design_decisions.md` "PollingComponent, not interrupt-driven") | `false` |

## What this component will never do

- Convert raw values to distance, liters, or flow rate.
- Combine multiple channels into an application-specific decoded quantity (e.g. a rotational
  position decoder for a spinning target).
- Run automatic sensor-drive auto-calibration during normal operation. Normal operation is always
  fixed-drive (`RP_OVERRIDE_EN=1`, `AUTO_AMP_DIS=1`) — the characterization feature above is a
  bounded, user-triggered exception to this, never a background behavior. See `design_decisions.md`
  and `.plan` "Scope amendment" for exactly where that line sits.

See `design_decisions.md` for the reasoning, and wire that logic in your own
`template`/`sensor.filter`/lambda code consuming this component's raw sensor entities instead.
