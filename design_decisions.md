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

**Decision:** The driver publishes the raw 12-bit `DATAx` conversion code as a plain ESPHome sensor. It never computes distance, liters, or flow rate. A frequency-ratio output (`fSENSORx/fREFx`, per the datasheet formula in `docs/knowledge_base.md`) is a possible future addition, not a v1 feature — the ratio is currently computed only inside the diagnostics report, and is not exposed as an entity.

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

## Persistent amplitude errors are tolerated, not chased

**Decision:** The driver publishes every conversion result regardless of `STATUS`, and does not
attempt to configure its way out of a standing amplitude error. `DATAx.ERR_WD` (watchdog timeout,
which the datasheet defines as invalid data) is the one flag that suppresses a reading; under-range,
over-range and amplitude flags publish normally and only raise the diagnostic binary sensor.

**Context:** On the HomeWizard watermeter board, `ERR_ALE` (amplitude-low) is asserted on every
channel on every conversion. An investigation established:

- The device's own auto-amplitude calibration drives `INIT_IDRIVE` to 31 — its maximum — on all
  three channels and leaves it there, with `ERR_ALE` continuously asserted. *(Measured.)* The
  device therefore swept its entire drive range without reaching the 1.2–1.8 V target, so no
  `IDRIVE` value can resolve the flag.
- **The mechanism, established 2026-08-21:** that bound is `R_P < 655 Ω`, against a family
  specified for `R_P` = 1 kΩ–100 kΩ (`snaa221b.md` §5). The coil head is below the device's
  supported floor by ~1.6×, so the device physically cannot develop 1.2 V across it at any drive
  setting. See `docs/sensor_head.md`. This does not change the decision below — it removes the
  last reason to revisit it.
- The coils oscillate at ≈4.9–5.2 MHz, against a configuration inherited from the datasheet's
  §8.2.4 worked example, which targets a 2.4 MHz / 6.6 kΩ coil. *(Measured; frequency derived from
  the DATA ratio and the nominal internal oscillator.)*
- `DEGLITCH` was consequently set below the sensor frequency, violating §8.1.7. *(Datasheet rule,
  measured inputs.)* Correcting it to `10MHZ` is worthwhile on its own terms and is now the
  component default — but it did not clear `ERR_ALE`.
- Measurements are nonetheless stable and usable: the codes track the rotating target
  reproducibly, and a controlled 3-litre draw counted the expected rotations.

**Why:** `ERR_AE` is a warning about oscillation amplitude, not a statement that the frequency
count is wrong — §7.6.14 raises it when amplitude has not settled to the target window, while the
conversion itself still completes. Suppressing readings on it would discard a working measurement
for a condition this hardware appears unable to leave. For `ERR_ALE` specifically the failure mode
is also the recoverable one: the datasheet notes over-amplitude activates the internal ESD clamp
and shifts the sensor frequency to an invalid state, whereas under-amplitude reduces SNR and warns.

**What this closes:** an earlier revision of this component carried a full sensor-characterization
framework — a six-stage state machine that entered TI's auto-amplitude mode, derived
`IDRIVE`/`OFFSET`/`OUTPUT_GAIN`, and stored them in NVS ahead of the YAML values. It was removed
once the above was established: it existed to find drive parameters that would clear the amplitude
error, and on this hardware no such parameters exist. Configuration is YAML-only again, and effort
moves to application-level decoding (Clarke transform, rotation detection, litre counting), which
per `CLAUDE.md` lives outside this component.

**What remains from it:** the on-demand datasheet-conformance report (`button:` platform), which
is what identified the `DEGLITCH` violation, plus the `STATUS` per-bit decoding and register write
tracing that made the investigation possible at all.

---

## The watermeter is a second component, not a feature of the driver

**Decision:** The watermeter lives in `components/watermeter/`, a separate ESPHome external
component. `components/ldc1314/` stays exactly what it is — a generic driver publishing raw
conversion codes. The watermeter takes three `sensor:` source IDs as input, not an `ldc1314_id`,
so it has no compile-time dependency on the driver at all.

**Alternatives considered:**
- Add volume/flow outputs to the `ldc1314` hub, gated behind an optional `watermeter:` config block.
- Keep the decoding in consuming YAML (`template` sensors + lambdas), as the previous plan assumed.

**Why:** The project goal changed (a full watermeter is now wanted) but the engineering reason
behind the old boundary did not: a decoder that assumes "three coils, 120° apart, one litre per
revolution" encodes one specific mechanical design, and the LDC1314 has no idea that is what its
channels are for. Splitting into two components gets the new goal without giving that up — the
driver stays usable for encoder knobs, buttons and proximity work, and the watermeter stays usable
with any three-phase inductive head, including one read by an LDC1312 or an LDC1614.

The YAML/lambda route was the previous plan and is now rejected on its own merits, not on scope:
the decoder needs per-sample state (envelope followers, a tracked angle, an accumulator), flash
persistence, and ~50 evaluations per second. That is a component, however it is spelled. Expressing
it as lambdas would produce something harder to read, impossible to unit-test, and slower.

Taking `sensor:` sources rather than the hub is what makes the decoder testable at all: three
`template` sensors replaying a captured trace drive it identically to real hardware.

---

## Clarke transform and a continuously tracked angle, not revolution counting

**Decision:** Volume comes from a continuously accumulated rotor angle: normalise the three
channels, Clarke-transform them into an α/β vector, take `atan2`, follow that angle through a
hysteresis band, and accumulate the wrapped delta. Volume is `accumulated_angle / 2π ×
liters_per_revolution`. Whole-revolution counting is not used at any stage.

**Alternatives considered:**
- Schmitt-trigger threshold on one channel, counting one litre per crossing pair.
- Quadrature decoding on two of the three channels.
- Ellipse/PCA fit of the raw three-channel locus instead of a fixed Clarke matrix.
- A deadband on the per-sample angle delta instead of a hysteresis band on the angle itself.

**Why:** Threshold counting fails on all three axes that matter for a domestic meter. Its
resolution is one litre, which is too coarse for a flow rate and far too coarse for leak
detection. It cannot distinguish forward from reverse, so backflow and mechanical wobble both
count as consumption. And a rotor that stops with the target parked on the threshold dithers
across it on noise alone, generating counts from a closed tap — the worst possible failure mode
for a billing-adjacent number.

The Clarke transform costs about ten floating-point operations per sample and removes all three
problems at once: continuous angle gives arbitrary sub-revolution resolution, the sign of the
angle delta gives direction, and the vector magnitude gives a free signal-quality gate that also
detects a detached coil. It is the standard treatment for a balanced three-phase signal, which is
what three coils at 120° are.

Quadrature on two channels was rejected for throwing away a third of the available signal and the
common-mode rejection that comes from summing all three. Ellipse fitting is strictly more general
and is held in reserve: if the trace capture shows the coils are *not* ~120° apart, Clarke
degenerates and a fit becomes necessary. Until that is measured, the fixed matrix is the simpler
thing that is probably right.

The hysteresis-vs-deadband choice is the subtle one. A deadband on `Δθ` (ignore movements smaller
than ε) gives zero drift at rest, but it systematically discards slow rotation — at low flow every
individual sample delta is below ε, so the meter reads zero exactly when a leak is what you are
looking for. A hysteresis band on the angle itself has the same zero drift at rest, because the
tracked angle simply does not move, while remaining complete at any flow rate: the tracked angle
lags by at most `h` but always catches up. Correct-but-quantised beats fast-but-lossy for a
counter that is supposed to be a total.

---

## Sampling rate is a YAML setting, not a new driver feature

**Decision:** The driver keeps its existing `PollingComponent` shape and reads one sample per
channel per `update_interval`. The watermeter's ~50 Hz requirement is met by setting
`update_interval: 20ms` and raising the I2C bus to 400 kHz. No burst sampling, no on-device
accumulation, no INTB/DRDY synchronisation.

**Alternatives considered:**
- The burst-sampling / between-poll accumulation feature the previous TODO reserved for exactly
  this case.
- Synchronise reads to `STATUS.DRDY` so all three channels come from the same scan.

**Why:** The arithmetic says none of it is needed. A domestic meter at overload flow (Q4 ≈
3.1 m³/h, 1 litre/revolution on this head) turns at ~0.87 rev/s; 50 Hz gives ~57 samples per
revolution there, against a hardware ceiling of ~360 three-channel scans/s. The requirement uses
14 % of what the device already produces, through an interface that already exists.

DRDY synchronisation buys even less. A full three-channel scan takes 2.8 ms. Against a revolution
of roughly one second, a channel occasionally sourced from the adjacent scan contributes about one
degree of phase error — an order of magnitude below the hysteresis quantum, and it does not
accumulate.

What 50 Hz *does* break is incidental: `read_channel_()` logs one line per errored conversion, and
`ERR_ALE` is permanently asserted on this board, so the driver would emit 150 log lines per second.
That is a logging bug the higher rate exposes, not an argument for a different acquisition
architecture.
