# The sensor head

What the coil assembly actually is, from photographs (`sensorpics/`, 2026-08-21) and from what the
registers imply. Written because the part could not be identified by searching: it is a custom
board, not a catalogue component, so there is no datasheet to find.

## What it is

A small round PCB — the **coil head only**. The LDC1314 is not on it; it lives on the main board
and connects here through a 2×3 pin header (`J1`), which is exactly three differential pairs
(INA/INB × 3) for the three channels.

**Coil side** (`IMG_4520`): three planar spiral coils, air-core, etched into the copper and
covered by soldermask, arranged with 3-fold symmetry on a common bolt circle around the board
centre. Each spiral is roughly a third of the board diameter.

**Component side** (`IMG_4518`, `IMG_4519`): silkscreen `C1`, `C2`, `C3` — one resonating capacitor
per coil, in small chip packages — plus `J1`. Nothing else. No LDC, no shield, no ferrite backing.

This is the classic LDC131x reference topology: three unshielded PCB spirals at 120°, each in
parallel with its own C, sensed differentially. Nothing exotic, and nothing that carries a part
number.

## Confirmed: the 120° assumption

The three spirals sit at 120° on a circle concentric with the rotation axis. That is the geometry
the Clarke transform assumes, and it is now confirmed by inspection rather than assumed.

Three things it does *not* settle, all of which fall out of the trace capture (Phase A):

- **Which channel is at which angle.** CH0/CH1/CH2 map to three physical positions through the
  header and the main board's routing; the photo cannot say which.
- **Rotation sense.** Whether forward flow advances θ or retards it.
- **Amplitude balance.** Whether the three coils couple equally to the target.

## Derived: the tank is below the LDC1314's supported R_P range

The auto-calibration run drove `INIT_IDRIVE` to 31 — maximum — on all three channels and still
reported `ERR_ALE` (amplitude below 1.2 V). That bounds R_P.

Per `snaa221b.md` Table 1, code 31 corresponds to R_P = 0.9 kΩ at the ~1.65 V target amplitude,
i.e. a drive current of ~1.44 mA (datasheet ceiling: 1.6 mA). With `V_OSC = 4·R_P·I/π`:

    R_P < 1.2 V · π / (4 · 1.44 mA) = 655 Ω        (589 Ω if the drive is the full 1.6 mA)

**`snaa221b.md` §5 states the LDC131x/161x family supports R_P from 1 kΩ to 100 kΩ.** This head is
below that floor by a factor of ~1.6. The standing amplitude error is therefore a property of the
hardware, not a misconfiguration — no `IDRIVE` value can fix it because the device cannot deliver
enough current to develop 1.2 V across a resistance this low. This is the mechanism behind the
conclusion already recorded in `design_decisions.md` ("Persistent amplitude errors are tolerated,
not chased"); that entry established *that* no setting works, this establishes *why*.

## Derived: L, C and Q

`f_SENSOR ≈ 5.0 MHz` fixes the product `L·C = 1/(2πf)² = 1.01×10⁻¹⁵`. Combining that with
`R_P = L/(R_S·C) < 620 Ω` and a series resistance plausible for a PCB spiral at 5 MHz (skin effect
included) gives a consistent family:

| R_S (Ω) | L (µH) | C (nF) | Q |
|---|---|---|---|
| 0.5 | 0.56 | 1.81 | 35 |
| 1.0 | 0.79 | 1.28 | 25 |
| 1.5 | 0.97 | 1.04 | 20 |
| 2.0 | 1.12 | 0.90 | 18 |

So: **L ≈ 0.6–1.1 µH, C ≈ 1–2 nF, Q ≈ 18–35.** Sub-µH and ~1 nF is entirely consistent with a
7 mm-ish spiral of a dozen-odd turns and a C0G chip capacitor, which is what the photographs show.

These are constraints, not measurements. Measuring C directly across the header (many DMMs read
nF) would collapse the whole table to one row, since L then follows exactly from f.

### Consequence: `SETTLECOUNT` is tighter than it looks

The datasheet's settling rule (§8.1.6) is `Q ≤ SETTLECOUNT × 16 × f_SENSOR/f_REF`. At the current
`SETTLECOUNT = 0x000A` that ceiling is **36.4** — and the Q estimate above runs to 35 at the
low-R_S end. It is not violated, but the margin may be nearly zero rather than the comfortable gap
the preflight report currently implies.

`SETTLECOUNT = 0x0020` raises the ceiling to ~116 and costs 16 µs per channel per conversion —
1.8 % on top of a 913 µs conversion. Cheap insurance, worth trying in Phase B.

(Both TI's reference configuration and the stock HomeWizard firmware use `0x000A`, so this is a
margin question, not a defect.)

## Caution: the resting inter-channel spread is not tolerance

The at-rest codes 932 / 936 / 970 work out to 4.938 / 4.959 / 5.139 MHz — CH2 sits 4.1 % above
CH0. It is tempting to read that as capacitor tolerance, but the full observed span across a
rotation is only 5.7 %, the same order. A single at-rest reading is one arbitrary rotor position,
so the spread is equally well explained by the rotor simply sitting nearer one coil than another.

Do not infer per-channel component tolerance from static readings. The trace capture separates
them: over a full revolution each channel's *mean* is its baseline and the tolerance question
becomes answerable.

## The target: Sensus 620, half-metallised disc

The meter is a **Sensus 620** (`DE-15-MI001-PTB019`), and the element under the coil head is its
fine-resolution indicator disc: round, one half metallised. One half-disc means **one electrical
cycle per mechanical revolution** — no pole-count ambiguity.

### 1 litre per revolution, now confirmed twice

The 620's fine dial is marked **×0.0001 m³** with ten graduations. One full revolution is therefore
10 × 0.0001 m³ = 0.001 m³ = **1 litre**, independently agreeing with the controlled 3 L draw that
counted three rotations. `liters_per_revolution: 1.0` rests on two unrelated lines of evidence.

### Why this target makes Clarke work better than it has any right to

A half-disc sweeping across an offset circular coil does not produce a sine. Its coverage waveform
is `segment_fraction(r·sin θ / a)`, where `a` is the coil radius and `r` the coil centre's offset
from the disc axis. Two symmetries then do most of the work for free:

- **Half-wave symmetry.** `coverage(θ + π) = 1 − coverage(θ)`, because the metal half that
  uncovers one side covers the other. All **even** harmonics are identically zero.
- **Clarke kills the triplen harmonics.** For three phases at 120°, the transform passes only
  `n ≡ 1 (mod 3)`; harmonics 3, 6, 9 … land in the zero sequence and vanish. Verified numerically.

The 3rd is by far the largest distortion term in this waveform, and it is exactly the one that
disappears. What survives is the 5th:

| r/a | h1 | h3 | h5 | 5th relative | peak angle error |
|---|---|---|---|---|---|
| 0.5 | 0.308 | 0.004 | 0.000 | 0.01 % | 0.01° |
| 1.0 | 0.540 | 0.036 | 0.003 | 0.57 % | 0.33° |
| **1.5** | **0.599** | **0.117** | **0.013** | **2.2 %** | **1.25°** |
| 2.0 | 0.616 | 0.156 | 0.049 | 7.9 % | 4.52° |
| 3.0 | 0.628 | 0.186 | 0.087 | 13.9 % | 8.00° |

The photographed head has coils roughly a third of the board diameter centred at about half its
radius, so `r/a ≈ 1.5`: **~1.25° of peak angle error**, an eighth of the 10° hysteresis quantum.

**And it does not accumulate.** Harmonic distortion warps the α/β locus into a non-circle, so the
angle leads and lags within a revolution — but its mean over a full revolution is exactly zero
(checked to 1e-14). Distortion costs a little jitter in the instantaneous *flow rate* and
**nothing at all** in the *volume total*. This is the strongest argument yet for Clarke over
two-phase quadrature decoding, which cancels no harmonic at all.

### Sample rate depends on the meter's Q3 — check yours

The supplied photograph is a catalogue image, not this meter, and it shows **Q3 10, R315**. That
matters, because Q3 sets the peak rotation rate at 1 L/rev:

| Q3 | Q4 = 1.25·Q3 | peak rev/s | samples/rev @50 Hz | @100 Hz |
|---|---|---|---|---|
| 2.5 m³/h | 3.125 | 0.87 | 58 | 115 |
| 4.0 m³/h | 5.0 | 1.39 | 36 | 72 |
| 10 m³/h | 12.5 | 3.47 | **14** | 29 |

At Q3 = 10 a 50 Hz sample rate leaves only ~14 samples per revolution at overload flow. Unwrapping
still works (the limit is 2 samples/rev), but there is little left for noise rejection. **The plan
therefore targets 100 Hz rather than 50 Hz** — it costs ~5 % of the main loop at 400 kHz I2C and
covers every variant. Read the Q3 off the meter's own face to settle which row applies.

R315 also sets the meter's own minimum registered flow, Q1 = Q3/315. Below that the mechanism
itself stops turning reliably, which is the real floor on leak detection — not our resolution.

## Still unknown

- **Whether `CLKIN` is driven.** Still open from `homewizard_reference_config.md`; a crystal on the
  main board would settle it, and it moves f_REF by 8.5 %.
