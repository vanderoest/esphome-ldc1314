# Watermeter component

Three-phase rotation decoding on top of three raw `sensor:` inputs: it turns the coil signals
into a rotor angle, counts revolutions, and accumulates volume and flow rate. It has no
compile-time dependency on `ldc1314` — any three sensors carrying the phase signals will do (see
`../../design_decisions.md` for why that boundary exists, and `../../README.md` for the driver).

## Setting the meter reading

The component counts from zero. To make it show what the physical meter shows, you set the
reading once and it continues from there: `watermeter.set_total` stores the difference between the
value you give and the volume it has counted itself as a persistent *install offset*, so

    published total = install offset + counted volume

The offset lives in the ESP's NVS together with the counted volume, the reverse-flow total and the
decoder's envelope calibration. It survives reboots, OTA updates and a Home Assistant reinstall,
and `set_total` writes it immediately rather than waiting for the rate-limited periodic save.

A `number:` entity is the convenient way to do it by hand — it shows the current total and accepts
a new one, in m³ like the dials on the meter:

```yaml
sensor:
  - platform: watermeter
    watermeter_id: meter
    volume:
      name: "Water"
      id: water_total

number:
  - platform: template
    name: "Set meter reading"
    entity_category: config
    unit_of_measurement: "m³"
    min_value: 0
    max_value: 100000
    step: 0.001
    mode: box
    update_interval: 60s
    lambda: |-
      if (std::isnan(id(water_total).state))
        return {};
      return id(water_total).state / 1000.0;   // component counts litres, this entity is m3
    set_action:
      - watermeter.set_total:
          id: meter
          value: !lambda 'return x * 1000.0;'
```

For scripts and automations, expose it as an API action instead (or as well) — `actions:` is
ESPHome ≥ 2025.5, older versions call the key `services:` with `- service: set_total`:

```yaml
api:
  actions:
    - action: set_total
      variables:
        liters: float
      then:
        - watermeter.set_total:
            id: meter
            value: !lambda 'return liters;'
```

Home Assistant then offers `esphome.<device>_set_total` with a `liters` parameter. A successful
call logs `set_total: install_offset now <x> L (published <y> L)` at `INFO`.

The `initial_value:` option on the `watermeter:` block is **not** a way to change this later: it
only applies when nothing could be loaded from NVS at all (a fresh device, or one whose flash was
erased). On any normal boot the stored offset wins and `initial_value:` is ignored.

### Two consequences worth knowing

**Each `set_total` is a step in Home Assistant's long-term statistics.** The volume sensor is
`state_class: total`, so HA reads the difference between consecutive states as consumption:
jumping from 0 to 1334.169 m³ books 1334.169 m³ of water in that hour. Do the initial match before
the history matters, or correct it afterwards in Developer Tools → Statistics.

**Don't re-run it to paper over drift.** If the reading has wandered away from the meter after
running a while, that is data: check `Revolutions` and `Reverse flow` first, and fix the cause
(`liters_per_revolution`, `direction`, `hysteresis`, `min_signal`, or a learn pass) rather than
nudging the offset. `set_total` hides the symptom and the drift simply resumes.

### Resolution at a large offset

The volume sensor publishes a 32-bit float, so its resolution degrades with magnitude: near
1 334 169 L consecutive representable values are 0.125 L apart, and increments smaller than that
do not move the published number on their own. The counter itself is a `double` and keeps every
increment exactly — nothing is lost, it only becomes invisible until enough has accumulated to
cross into the next float step. If you want litre-level detail, read it from `Water flow` or from
the volume delta over a window, not from the absolute total.
