# HomeWizard stock firmware vs. this driver — register comparison

Findings from comparing the stock HomeWizard firmware's LDC register configuration
([`original-homewizard-boot.md`](original-homewizard-boot.md) §6) against what this ESPHome
component actually writes, on the same physical board.

> **Status: findings only.** Nothing in the "candidates" section below has been applied. The
> driver's configuration is unchanged; only the STATUS/ERROR_CONFIG bug (see the last section)
> was fixed. Config changes are to be evaluated one variable at a time.
>
> **Update:** the manual sweep this document calls for is now automatable. The characterization
> framework (`.plan`, `TODO.md` Phase 3–4) derives `idrive`/`offset`/`output_gain` the same way —
> auto-amplitude observation across the full target range, then a fixed-drive envelope
> measurement — and its own report format supersedes the "candidates" table below once a run is
> actually performed on this hardware. That hardware run has not happened yet (no physical board
> was available while building the framework), so the candidates below remain **unmeasured
> estimates**, not confirmed values — do not promote them without an actual run's report to point
> to. See `TODO.md` Phase 5 "Ground truth first" for the validation this still needs.

## What each side writes

Ours is traced statically through `configure_()` and `write_channel_config_()` in
`components/ldc1314/ldc1314.cpp`, for the settings in `tests/watermeter-test.yaml`
(`idrive: 18`, `fin_divider: 1`, `fref_divider: 2`, `output_gain: 1`, `reference_clock: internal`,
`report_errors_on_intb: false`), and is now also printed at runtime by the `CH[n]|`/`GLOBAL|`
debug lines added for exactly this comparison.

| Register | Addr | This driver | HomeWizard | Same? |
|---|---|---|---|---|
| RCOUNT | 0x08–0x0B | `0x04D6` | `0x04D6` | ✅ |
| SETTLECOUNT | 0x10–0x13 | `0x000A` | `0x000A` | ✅ |
| CLOCK_DIVIDERS | 0x14–0x17 | `0x1002` | `0x1002` | ✅ |
| **OFFSET** | 0x0C–0x0F | `0x0000` | **`0x3000`** | ❌ |
| **DRIVE_CURRENT** | 0x1E–0x21 | `0x9000` (IDRIVE 18) | **`0xB800`** (IDRIVE 23) | ❌ |
| RESET_DEV (OUTPUT_GAIN) | 0x1C | `0x0000` (gain 1) | *not printed by their log* | ? |
| MUX_CONFIG | 0x1B | `0xC20C` (4 ch) | *not printed* | ? |
| CONFIG | 0x1A | `0x1481` | *not printed* | ? |
| ERROR_CONFIG | 0x19 | `0xF8FC` | *not printed* | ? |
| Channels used | — | 4 (0–3) | **3 (0–2)** | ❌ |

Three of our registers have no counterpart in their log at all — the stock firmware only prints
per-channel values, so `CONFIG`, `MUX_CONFIG`, `ERROR_CONFIG` and `OUTPUT_GAIN` cannot be
compared and must not be guessed at.

## Encoding check: is `idrive: 18` encoded correctly?

```
drive_current = idrive << DRIVE_CURRENT_IDRIVE_SHIFT      // shift = 11
              = 18 << 11 = 36864 = 0x9000

0x9000 = 1001 0000 0000 0000
         └───┘                bits[15:11] IDRIVE      = 10010b = 18   ✓
              └───┘           bits[10:6]  INIT_IDRIVE = 00000b = 0    ✓ (read-only, written as 0)
                    └────┘    bits[5:0]   reserved    = 0             ✓
```

The datasheet's own worked example (§8.2.4 step 6) states: *"IDRIVE0 value should be set to 18
(decimal). The INIT_DRIVE0 current field should be set to 0x00. The combined value for the
DRIVE_CURRENT0 register (addr 0x1E) is 0x9000."*

Our encoder reproduces TI's number exactly. **The encoding is correct** — `18` is simply a
different *value* than the stock firmware uses, not a mis-encoded one. For reference,
`idrive: 23` would encode as `23 << 11 = 0xB800`, which is byte-identical to HomeWizard's.

Two further cross-checks that the composition logic is sound:
- For a **2-channel** setup our `MUX_CONFIG` composes to `0x820C` and `CLOCK_DIVIDERS` to
  `0x1002` — both byte-identical to the datasheet's recommended multi-channel init (Table 45).
- `OUTPUT_GAIN` maps 1/4/8/16 → `0x0000`/`0x0200`/`0x0400`/`0x0600` in bits [10:9], per §7.6.26.

## Observation: HomeWizard started from TI's reference values

`RCOUNT 0x04D6`, `SETTLECOUNT 0x000A` and `CLOCK_DIVIDERS 0x1002` are **exactly** the values in
the datasheet's "Recommended Initial Register Configuration Values" for multi-channel operation
(§8.2.5 Table 45). The stock firmware appears to have taken TI's reference configuration and
changed only two things — `DRIVE_CURRENT` and `OFFSET` — while extending it to a third channel.

That is useful signal: it suggests those two fields are precisely the ones that *must* be tuned
per sensor design, and that the timing-related registers can be left at TI's defaults.

## Candidates for a later pass — NOT applied

### 1. `idrive: 18` → `23`

`V_OSC = 4·R_P·IDRIVE/π` is linear in IDRIVE, so 18 (212 µA) produces ≈43 % of the amplitude that
23 (489 µA) does. Against the datasheet's 1.2–1.8 V target window that plausibly puts the
oscillation below the 1.2 V floor. Per the IDRIVE table, code 23 targets R_P 1.93–2.95 kΩ while
code 18 targets 4.45–6.21 kΩ — implying this coil is ≈2–3 kΩ.

Consistent with the symptom: `tests/watermeter-test.log` shows a **persistent amplitude error on
every populated channel**. But note the direction is *inferred*, not measured — `DATAx.ERR_AE` is
the logical OR of amplitude-high and amplitude-low (SNOA959 §1.1), so it cannot by itself say
whether drive is too low or too high. Resolving that is what the STATUS fix below enables, and it
should be checked before changing IDRIVE.

### 2. `offset: 0x0000` → `0x3000`

`0x3000` = 12288; `12288 / 2^16` = **0.1875** exactly. Per §8.1.3.1 the offset subtracts a fixed
fraction of `fREF` before the result is scaled, which is the standard way to discard a dead
pedestal so the interesting part can be amplified (SNOA945 §3.3).

Our measured codes (IDRIVE 18, offset 0, gain 1) span 928–981, i.e. a ratio of 0.2266–0.2395 —
comfortably above 0.1875, so subtracting it leaves 0.0391–0.0520.

### 3. `output_gain: 1` → 8 or 16

Combining the offset above with gain, using `DATAx = (ratio − offset) × 2^(12+shift)`:

| Gain | Shift | Code at ratio 0.2266 | at 0.2395 | Span | % of full scale used |
|---|---|---|---|---|---|
| 1 (now, offset 0) | 0 | 928 | 981 | **53** | — |
| 4 | 2 | 640 | 852 | 212 | 21 % |
| **8** | 3 | 1280 | 1704 | **424** | 42 % |
| 16 | 4 | 2560 | 3408 | **848** | 83 % |

Gain 8 gives 8× the usable span with generous headroom; gain 16 gives 16× but sits at 83 % of
full scale, so a wider swing under real flow could clip. Gain 8 is the safer first step.

**Caveat:** this arithmetic is derived from codes measured *while the amplitude error was
present*. Correcting IDRIVE may shift the operating frequency, so these figures are indicative
and must be re-measured after any IDRIVE change — another reason to change one variable at a time.

### 4. Channel 3

Channel 3 reads a constant `0x1FFF` (error flag + full-scale `0xFFF`) — consistent with no coil
attached, and with the stock firmware only ever configuring CH0–CH2. Not a driver fault;
configuring it simply spends conversion time on nothing.

## Separate finding: sample rate cannot resolve rotation

With `fREF` = 43.4 MHz ÷ 2 ≈ 21.7 MHz (internal oscillator):

- conversion `tC = (RCOUNT × 16 + 4) / fREF` = 19812 / 21.7 MHz ≈ **913 µs**
- settling `tS = (SETTLECOUNT × 16) / fREF` = 160 / 21.7 MHz ≈ **7.4 µs**
- channel switch = `692 ns + 5/fREF` ≈ **0.9 µs**
- ⇒ ≈921 µs per channel, ≈**2.8 ms** for a 3-channel scan ⇒ ≈**360 scans/s** available

An `update_interval: 1s` therefore consumes roughly 0.3 % of the samples the hardware produces —
margin, not a bottleneck.

**Correction:** an earlier version of this section called the ~9 s period between peaks "an
alias, not the rotation rate." That was wrong. A controlled 3-litre ground-truth test (three
rotations counted between tap-on and tap-off) confirmed the ~9 s period *is* the real rotation
rate (~0.111 Hz), giving roughly 9 samples per rotation at 1 Hz sampling — comfortably above
Nyquist, with no aliasing involved. The ±6 code jitter noted above is amplitude-error noise from
the under-driven coil (see the `idrive` candidate above), not a sampling artifact.

(If the stock firmware instead drives `CLKIN` from an external 40 MHz source, `fREF` = 20 MHz and
per-channel dwell works out to almost exactly 1.000 ms — matching the 1 kSPS target the datasheet
example was designed around. `CONFIG.REF_CLK_SRC` is not in their log, so this stays unconfirmed.)

Reconstructing flow from rotation is application logic and out of scope for this driver per
`CLAUDE.md`. Recorded here as an open question: it may motivate a future *driver* capability
(burst sampling / on-device accumulation between polls), which would be a device-level feature
rather than application math.

## Fixed in this pass: STATUS was never populated

`ERROR_CONFIG` previously wrote `0xF800` — only the five `*_ERR2OUT` bits. Per §7.6.23 the
`*_ERR2INT` bits gate **both** INTB assertion **and** `STATUS.ERR_*` updating:

> **7 UR_ERR2INT** … b0: Do not report Under-range errors by asserting INTB pin **and STATUS
> register**. b1: Report Under-range errors by asserting INTB pin **and updating STATUS.ERR_UR
> register field**.

With them clear, `STATUS` error bits never set — confirmed empirically by
`tests/watermeter-test.log`, which contains **zero** STATUS messages across 47 update cycles
despite every channel flagging `ERR_AE` in `DATAx`.

`ERROR_CONFIG` is now `0xF8FC`: the six error `*_ERR2INT` bits (7–2) are set, both reserved
fields keep their mandated zeros, no `*_ERR2OUT` bit changes, and `DRDY_2INT` stays off. This
makes amplitude-**high** distinguishable from amplitude-**low**, and makes zero-count errors
visible at all — SNOA959 Table 1 lists zero-count's `DATA_CHx` path as "N/A", so STATUS is its
only route.
