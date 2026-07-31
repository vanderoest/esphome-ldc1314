# Design Decisions

Running log of significant architectural *why* decisions made while producing `.plan`, kept separate from `.plan` (the *what*/*how*) so the reasoning survives even if the plan itself is later revised. Newest entries at the bottom.

---

## Hub + per-channel-sensor architecture

**Decision:** One `ldc1314:` hub component owns the I2C connection and device-global registers (`CONFIG`, `MUX_CONFIG`, reference clock choice). Each measured channel is a separate `sensor: platform: ldc1314` entry that references the hub by ID and owns its own per-channel registers (`RCOUNTx`, `SETTLECOUNTx`, `CLOCK_DIVIDERSx`, `DRIVE_CURRENTx`, `OFFSETx`).

**Alternatives considered:**
- A single monolithic sensor component exposing 4 hard-coded channel slots as one big config block.
- One `sensor:` platform instance per channel with *no* shared hub, each independently owning I2C address/config (would duplicate global-register writes and risk races between "channel components" all trying to configure the same physical device).

**Why:** This matches the device itself — one physical I2C device with one set of global registers, but up to 4 independently-configurable channels, each optionally present or absent (LDC1312 has 2, LDC1314 has 4). It also mirrors the established ESPHome idiom for exactly this device shape: the ADS1115 component (a multi-channel I2C ADC) uses the identical hub + per-channel `sensor:` pattern, confirmed via `esphome.io/components/sensor/ads1115.html` during planning. Reusing that pattern satisfies CLAUDE.md's "reuse existing ESPHome component patterns" requirement directly, rather than inventing a new shape for a problem ESPHome has already solved.

---

## `PollingComponent`, not interrupt-driven

**Decision:** `LDC1314Component` extends `PollingComponent` and reads channel data on a configurable `update_interval`, for the initial iterations (0–6). INTB-driven (interrupt/GPIO) reads are deferred, not ruled out — revisit once the polling baseline is correct and iteration 7+ (configuration options) is reached.

**Alternatives considered:**
- Wire the INTB pin to a GPIO interrupt from iteration 0, reading data only when INTB asserts.
- Hybrid: poll `STATUS.DRDY` every loop() without a fixed interval.

**Why:** INTB-driven reads are more efficient and lower-latency, but add a GPIO configuration requirement and interrupt-safety constraints (I2C transactions are not safe to issue directly from ISR context on ESP-IDF — they'd need to defer to the main loop via a flag anyway, which is most of the complexity of polling with none of the simplicity). Polling is the standard ESPHome sensor baseline, needs no extra pin, and keeps iterations 0–6 focused on correctness of the register-level protocol rather than concurrency. This is a deliberate scope cut, not a permanent architectural ceiling — see the iteration roadmap.

---

## Publish raw values, not liters/flow

**Decision:** The driver publishes the raw 12-bit `DATAx` conversion code and/or the computed frequency ratio (`fSENSORx/fREFx`, per the datasheet formula in `docs/knowledge_base.md`) as plain ESPHome sensors. It never computes distance, liters, or flow rate.

**Alternatives considered:**
- Add an optional "distance" or "flow" output mode with a user-supplied calibration curve.

**Why:** This is a direct instruction in CLAUDE.md ("Do NOT implement watermeter logic"), but the engineering reason behind that instruction is worth recording: the mapping from a raw conversion code to a physical quantity (distance, flow) depends on sensor geometry, target material, and mounting — all application- and installation-specific, not device-specific. Baking any of that into the driver would make it non-reusable outside one specific watermeter design, and would require the driver to carry calibration state that has nothing to do with talking to the LDC1314 chip. Keeping the boundary at "raw measurement out" keeps the driver generically useful for *any* inductive-sensing application (knobs, encoders, buttons, flow — CLAUDE.md's own feature list), matching the datasheet's own framing of the device as a frequency-ratio converter, not a distance sensor.

---

## Keep application-specific decoding (e.g. a Clarke/quadrature-style decoder) outside the driver

**Decision:** Any algorithm that combines two or more raw per-channel readings into a derived quantity — for example a Clarke/quadrature-style decoder turning two out-of-phase channel signals into a rotational position for a water meter's spinning target — lives entirely in the consuming YAML/application layer (`template` sensors, `sensor.filter`, or external code), never in `ldc1314.h`/`.cpp` or the component's Python config.

**Alternatives considered:**
- Provide a built-in "differential channel pair" or "quadrature decoder" helper in the component, since it's a common pattern across LDC use cases.

**Why:** Same boundary as the raw-values decision above, but worth calling out separately because it's tempting to treat "combine two channels" as a device capability rather than application logic — it isn't. The LDC1314 has no awareness of what its channels are being used for; a decoder that assumes "channel 0 and channel 1 are 90° out of phase on a rotating target" is encoding assumptions about a *specific* sensor/target mechanical design, which is exactly the kind of watermeter-specific logic CLAUDE.md excludes. It also has no natural home in a generic driver: it isn't a register, isn't a device mode, and doesn't generalize to the encoder-knob, button, or keypad use cases the datasheet lists as equally valid applications of this same chip.

---

## ADS1115-like structure specifically (vs. inventing a new pattern)

**Decision:** Use the ADS1115 component's YAML/Python/C++ shape as the concrete structural template (directory layout, hub+per-channel schema keys, `i2c::I2CDevice` + `PollingComponent` base classes) rather than designing the LDC1314 component's structure from scratch.

**Alternatives considered:**
- Design a bespoke structure optimized specifically for the LDC1314's register layout.

**Why:** CLAUDE.md explicitly requires reusing existing ESPHome component patterns instead of inventing new ones, and states the driver should "always follow current ESPHome architecture." ADS1115 is the closest existing multi-channel I2C measurement component in ESPHome core to what the LDC1314 needs (shared I2C hub, independently configured channels, each publishing one raw sensor value) — using it as the template turns most of the Phase 5 architecture decisions into "match the precedent" rather than "invent and justify," which is both faster to implement correctly and easier for ESPHome maintainers to review later (upstream readiness).

---

## Scope v1 to the LDC1314 only, without closing the door on LDC1312/1612/1614

**Decision:** The first implementation writes code for the LDC1314 exclusively. No `device_family`/`model` config option, no branching logic, and no LDC1312/1612/1614-specific code paths exist yet. The only precautions taken are structural: channel count sits behind one named constant instead of scattered literals, and register addresses/bit-fields live entirely in one header matching `register_map.md`.

**Alternatives considered:**
- Build a `model:`/`variant:` selector into the hub schema from the start, with LDC1312/1612/1614 as stub/unimplemented branches.
- Ignore family concerns entirely and hardcode "4 channels, 12-bit" assumptions freely throughout the codebase.

**Why:** The first alternative is itself "writing code for those devices" ahead of need — dead branches and unused schema surface add review burden and can encode wrong guesses about a family (LDC1612/1614) whose register layout hasn't actually been researched yet (different datasheet, SNOSCY9, 28-bit MSB/LSB-split data registers — a materially different shape than LDC1314's single 16-bit `DATAx`). The second alternative would make the *cheap* future case (LDC1312, which is genuinely just "fewer channels, same everything else") needlessly expensive later, for no benefit now — a named channel-count constant and a single register-constants header cost nothing today and remove the only two places LDC1314-specific numbers would otherwise leak into general-purpose logic. This isn't an attempt to architect for a family that hasn't been documented (LDC1612/1614); it's simply not actively sabotaging that future work while staying entirely focused on LDC1314 today.

---

## Scope amendment: a bounded characterization run is now a driver feature

**Decision:** `TODO.md` previously excluded "automatic runtime auto-calibration" outright, as a
bring-up-only procedure that stayed outside the component. That line is amended, not removed.
What is still excluded is the device *free-running* in TI's auto-amplitude mode
(`AUTO_AMP_DIS=0`) during normal operation — `RP_OVERRIDE_EN=1`/`AUTO_AMP_DIS=1` remain the only
production settings, always. What is now in scope is a bounded, user-triggered, one-shot
characterization run that *temporarily* enters auto-amplitude mode to observe the sensor, derives
fixed `IDRIVE`/`OFFSET`/`OUTPUT_GAIN` values, and always exits back to fixed drive — on success,
on failure, and on every abort path, with no exception.

**Alternatives considered:**
- Leave the exclusion as originally written and keep characterization as an external script or a
  human-followed protocol (the original hardware bring-up `.plan`'s Phase 1 sweep).
- Let the device free-run in auto-amplitude mode whenever no calibration is stored yet, deriving
  it implicitly over time instead of via an explicit run.

**Why:** The HomeWizard reference evidence (`docs/homewizard_reference_config.md`) reframed what
this procedure actually is. The stock firmware's only per-sensor changes from TI's Table 45
defaults were `DRIVE_CURRENT` and `OFFSET` — exactly the two values TI's own auto-amplitude
procedure (datasheet §8.1.5.2) derives. That is a sensor characterization step, not a volumetric
calibration, and it generalizes to any LDC1314 application (a knob, a slide, a meter disc) rather
than being watermeter-specific — which is why it belongs in the driver rather than staying an
external protocol. The second alternative — implicit free-running auto-amplitude — is exactly the
behavior SNOA950 §4 warns against: the LDC may adjust current mid-measurement and inject a step
that reads as target movement, so it can never be the default state, only a deliberately-entered
one with a clear beginning and end.

---

## Three-layer value resolution: YAML seed, stored calibration, armed manual override

**Decision:** `idrive`, `offset` and `output_gain` resolve through three layers rather than one:
a YAML seed (used only until a calibration exists), a stored characterization record (wins over
YAML), and a manual override record edited via `number` entities that only takes effect once
armed by a `switch`. Disarming the switch is the "revert to calibrated" action — instant, exact,
and non-destructive, since the manual values are preserved in their own record rather than
overwriting the calibration.

**Alternatives considered:**
- A YAML value written by the user counts as a hard override the driver must never touch.
- One NVS record instead of two, with manual edits and characterization results sharing the same
  storage.

**Why:** The first alternative was the initial design and was explicitly rejected once actual
runtime tuning entered the picture: if writing `idrive: 18` in YAML meant "never touch this,"
there would be no way to benefit from characterization without editing and reflashing, which
defeats most of the feature's point. It also would have made the `switch`'s arm/disarm flag
redundant, since "value present in YAML" would already mean "overridden." The second
alternative — one record for both manual edits and calibration — would make "revert to
calibrated" mean "the calibrated value has been overwritten by whatever was last dialed in,"
turning revert into "re-run characterization," a several-minute physical procedure to undo a
slider drag. Two records cost a small amount of flash and a few extra lines of resolution logic;
in exchange, arming and disarming the override is a clean, reversible A/B between "what I derived"
and "what I'm trying," which is the actual behavior wanted.

---

## `min(INIT_IDRIVE)` as the characterization's IDRIVE recommendation

**Decision:** During the auto-amplitude observation stage, the characterization engine derives
the recommended `IDRIVE` as the *minimum* `INIT_IDRIVE` value observed across the full target
motion — never the mean, and never the maximum.

**Alternatives considered:**
- Average the observed `INIT_IDRIVE` values.
- Use the maximum observed value (closest target approach).

**Why:** `V_OSC = 4·R_P·IDRIVE/π` is linear in drive current, and weaker magnetic coupling
(target farthest away) presents the highest effective `R_P`, which is exactly where the device's
own auto-calibration settles on its lowest current code. Taking the minimum over the full sweep
therefore reproduces TI's own stated calibration condition — "position the target at the maximum
planned operating distance" (datasheet §8.1.5.2, SNAA221B §6) — without requiring a physically
impossible ask of this driver's target class: holding a continuously moving target motionless at
one specific point. It is also the safer failure direction. Over-driving trips the internal ESD
clamp and silently shifts the sensor into an invalid frequency state with no guaranteed error
flag; under-driving only raises the recoverable `ERR_ALE` warning against valid data. SNOA950 §8
gives the identical rule independently for multi-sensor boards: when per-channel tuning disagrees,
use the lowest value across channels.

---

## Guided-run prompt text lives in configuration, not in the component

**Decision:** The characterization procedure needs to tell the user when to start and stop
moving the target to be usable as a guided, black-box run — but the two prompt strings
(`characterization.prompts.start`/`.stop`) are plain configurable text with target-neutral
defaults, never hardcoded application wording.

**Alternatives considered:**
- Hardcode watermeter-appropriate wording ("open the tap") directly into the component, since
  that's the reference application driving this feature's design.
- Leave the user to infer when to act from stage names in the log, with no explicit prompt at all.

**Why:** The first alternative is precisely the boundary this project draws everywhere else —
CLAUDE.md forbids watermeter-specific logic in the driver, and a driver that prints "open the
tap" is a watermeter driver in a way a config-supplied string is not. It costs nothing to make the
two moments configurable, and it's the only place in the whole characterization feature where
application wording is allowed in at all — everywhere else the boundary holds exactly as it does
for the rest of the component. The second alternative would fail the feature's own goal: a
black-box procedure that tells the user what to do, not one that expects the user to have already
read the source or the `.plan` to know when "Stage 1: auto drive" means "start moving the target."

---

## Persistence and timing stay behind ESPHome's own abstractions

**Decision:** The settings store uses `global_preferences`/`ESPPreferenceObject`
(`esphome/core/preferences.h`) exclusively, never a platform NVS API directly; all timing in the
characterization engine uses `millis()` from `esphome/core/hal.h`, never a platform-specific timer
call. No `#ifdef USE_ARDUINO`/`USE_ESP_IDF` branching exists anywhere in the new code.

**Why:** The existing driver was already framework-agnostic by construction — every dependency
came from `esphome/core/*` or `esphome/components/i2c`, and the project's test fixtures moved from
Arduino to ESP-IDF without touching a single line of the component's C++. The characterization
feature adds the two categories of code (persistence, timing) where framework assumptions most
commonly creep in, so keeping both routed through ESPHome's own abstractions was the one rule most
worth being deliberate about — it costs nothing today and keeps the component honestly
framework-agnostic rather than "framework-agnostic until you look at the new feature."
