# TODO

Tracks implementation progress against `.plan`. Scope: **LDC1314 only** for this whole list (see `.plan` §1 "Device scope" and `design_decisions.md`) — no LDC1312/LDC1612/LDC1614-specific work belongs here.

## Documentation (done)

- [x] Convert all usable source PDFs to Markdown (`docs/*.md`)
- [x] Write per-document technical summaries (`docs/summaries/`)
- [x] Write `docs/knowledge_base.md`
- [x] Write `register_map.md`
- [x] Write `.plan`
- [x] Write `design_decisions.md`

## Iteration 0 — Project skeleton

- [x] Create `components/ldc1314/__init__.py` with `CONFIG_SCHEMA` (`id`, `address`, plus the full hub option set — grew beyond "minimal" once written in one pass, see note below)
- [x] Create `components/ldc1314/ldc1314.h` / `.cpp` with `LDC1314Component : public PollingComponent, public i2c::I2CDevice` (`setup()`, `update()`, `dump_config()`)
- [x] Resolve the `update_interval` placement question → **hub-level**, via `cv.polling_component_schema()` on the `ldc1314:` domain
- [x] Add a test YAML and confirm `esphome compile` succeeds → `tests/test_ldc1314.yaml`, compiled clean against ESP32/Arduino (0 warnings, 0 errors, firmware produced)
- [x] Decide and document the test-fixture location/naming convention → `tests/test_<scenario>.yaml`, validated with `esphome config`/`esphome compile`

## Iteration 1 — DEVICE_ID

- [x] Implement raw I2C register read (8-bit pointer + 16-bit read) → uses ESPHome's built-in `I2CDevice::read_byte_16`/`write_byte_16` (confirmed these exist and handle the big-endian conversion this device needs, rather than hand-rolling)
- [x] Add `ldc1314_registers.h` with `MANUFACTURER_ID`/`DEVICE_ID` address + expected-value constants
- [x] Read `DEVICE_ID` (0x7F) and `MANUFACTURER_ID` (0x7E) in `setup()`, log a warning (not a hard failure) on mismatch
- [ ] Verify expected values (`0x3054` / `0x5449`) on real hardware — **not done, no physical LDC1314 available in this environment**

## Iteration 2 — Reset

- [x] Implement `RESET_DEV.RESET_DEV=1` write
- [x] Add a fixed post-reset delay (10ms, chosen conservatively — flagged in code comments as needing empirical validation, since the bit never reads back and the datasheet gives no explicit reset-complete timing)
- [x] Re-verify identity via iteration 1's DEVICE_ID/MANUFACTURER_ID read after reset
- [ ] Confirm setup log order (reset → re-verify) on real hardware — **not done, no hardware**

## Iteration 3 — Register configuration

- [x] Add bit-field constants for `CONFIG`, `MUX_CONFIG`, `ERROR_CONFIG` to `ldc1314_registers.h`
- [x] Implement Sleep-Mode-gated global register writes
- [x] Enforce write ordering: all other global registers first, `CONFIG` (which exits Sleep Mode) written **last**
- [ ] Verify on real hardware: device transitions Sleep → Normal, STATUS.DRDY toggles — **not done, no hardware**

## Iteration 4 — Channel 0

- [x] Create `components/ldc1314/sensor.py` with per-channel platform schema (`ldc1314_id`, `channel`, `rcount`, `settlecount`, `offset`, `fin_divider`, `fref_divider`, `idrive`)
- [x] Implement per-channel register writes (`RCOUNTx`, `SETTLECOUNTx`, `CLOCK_DIVIDERSx`, `DRIVE_CURRENTx`)
- [x] Implement `DATAx` read + publish in `update()`
- [x] Decide raw-code vs. frequency-ratio as the default published unit → **raw 12-bit conversion code** (0–4095, no unit_of_measurement); frequency-ratio derivation needs `fREFx` which isn't otherwise tracked by the driver, so it was left for a future iteration rather than added speculatively
- [ ] Verify on real hardware: published value changes when a metal target moves near the channel-0 sensor coil — **not done, no hardware**

## Iteration 5 — All channels

- [x] Introduce `MAX_CHANNELS=4` constant (single source of truth, no scattered literals) → `ldc1314_registers.h`
- [x] Generalize channel registration/array to N channels (bounded by `MAX_CHANNELS`)
- [x] Configure `MUX_CONFIG.AUTOSCAN_EN`/`RR_SEQUENCE` from the set of declared channels, including a startup warning if declared channels aren't contiguous from 0 (hardware limitation)
- [x] Validate declared channel indices (0–3) against `MAX_CHANNELS` (`cv.int_range` in `sensor.py`/`binary_sensor.py`)
- [ ] Verify on real hardware with ≥2 channels: independently correct values — **not done, no hardware**
- [x] Design-level mitigation for the multi-channel data-readback race: `update()` unconditionally reads every active channel's `DATAx` every cycle (not gated on `UNREADCONVx`), which sidesteps corrupted/skipped reads by construction — see the comment in `LDC1314Component::update()`. Real-hardware timing verification (tight `update_interval` vs. actual scan+settle time) is **not done, no hardware**

## Iteration 6 — Diagnostics

- [x] Add STATUS register read to `update()` (ordered before per-channel `DATAx` reads, per `docs/knowledge_base.md` "Error handling" read-order note)
- [x] Expose optional per-channel error diagnostics → `binary_sensor: platform: ldc1314` (device_class `problem`), driven by each channel's own `DATAx` error bits rather than `STATUS.ERR_CHAN` (which can't unambiguously attribute simultaneous errors on different channels — see code comments)
- [x] Implement `dump_config()` showing resolved hub + per-channel configuration
- [ ] Verify on real hardware: an induced fault (e.g. disconnected sensor) surfaces in diagnostics — **not done, no hardware**

## Iteration 7 — Configuration options

- [x] Add remaining documented options: Output Gain, per-channel offset, deglitch bandwidth, reference clock source, high-current-drive (channel 0 only, single-channel mode only), sensor-activation mode, INTB error reporting toggle
- [x] Add schema validation reflecting documented dependencies (RCOUNT 5–65535, FIN_DIVIDER 1–15, FREF_DIVIDER 1–1023, IDRIVE 0–31, channel 0–3)
- [ ] Evaluate friendlier units for register-valued options (e.g. settle time in µs) — **deliberately deferred**, not done; v1 keeps raw register values throughout (see `.plan` §3)
- [x] Compile-test the expanded schema against a range of option combinations → `tests/test_ldc1314.yaml` (full option set, 2 channels, both diagnostics) and `tests/test_ldc1314_single_channel.yaml` (all-defaults, single channel) both validated/compiled clean

## Iteration 8 — Documentation

- [x] Write component usage docs → `components/ldc1314/README.md`, referencing `register_map.md`/`docs/knowledge_base.md` rather than duplicating them
- [x] Add example YAMLs: single sensor (README + `tests/test_ldc1314_single_channel.yaml`), multi-channel + diagnostics (README + `tests/test_ldc1314.yaml`)
- [ ] Confirm a new user can wire up channel 0 from the component docs alone — **not independently confirmed** (would need an actual new user, not the implementer)

## Iteration 9 — Upstream readiness (optional / best-effort, not started)

Nothing in this iteration was attempted — it requires opening a PR against the external `esphome/esphome` repository, which is outside what this pass can reasonably do. Left entirely for a future, explicit decision to pursue upstreaming.

- [ ] CODEOWNERS entry
- [ ] `CONF_*` constants (centralize only if shared with another component)
- [ ] Component test YAML fixtures matching ESPHome's test conventions
- [ ] Pass ESPHome's formatting/CI tooling locally
- [ ] Confirm all entities remain optional in configuration
- [ ] Open PR against `esphome/esphome`, request maintainer review

## Explicitly out of scope (do not add here without a scope change)

- Any LDC1312/LDC1612/LDC1614-specific code, config keys, or branching (see `.plan` §1 "Device scope")
- Distance/liters/flow-rate computation, or any cross-channel application-specific decoding (see `design_decisions.md`)
- Automatic runtime auto-calibration (bring-up-only procedure, not a driver feature)

## Definition of Done

- Builds without warnings
- Passes `esphome compile`
- No TODO comments remain in the implementation
- Changes committed to git