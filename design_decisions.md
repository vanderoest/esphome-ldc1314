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

**Why:** INTB-driven reads are more efficient and lower-latency, but add a GPIO configuration requirement and interrupt-safety constraints (I2C transactions are not safe to issue directly from ISR context on ESP-IDF/Arduino — they'd need to defer to the main loop via a flag anyway, which is most of the complexity of polling with none of the simplicity). Polling is the standard ESPHome sensor baseline, needs no extra pin, and keeps iterations 0–6 focused on correctness of the register-level protocol rather than concurrency. This is a deliberate scope cut, not a permanent architectural ceiling — see the iteration roadmap.

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
