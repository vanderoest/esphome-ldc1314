#pragma once

#include <cstdint>

// A distinct namespace from esphome::watermeter (the ESPHome wrapper in watermeter.h/.cpp) is
// deliberate, not incidental: ESPHome's generated main.cpp does `using namespace esphome;`, which
// makes a plain top-level `namespace watermeter` collide with `esphome::watermeter` and produces
// "reference to 'watermeter' is ambiguous" the moment both are visible in the same translation
// unit -- caught building tests/watermeter_component_test.yaml.
namespace watermeter_core {

// Physical coil order and rotation sense are wiring/geometry facts, not givens
// (design_decisions.md) -- channel_order maps this decoder's fixed internal phase slots 0/1/2 to
// the raw sample indices passed into update(). invert_direction flips the sign of the
// accumulated angle, i.e. swaps which physical rotation direction counts as "forward".
struct DecoderConfig {
  uint8_t channel_order[3] = {0, 1, 2};
  bool invert_direction = false;

  // Envelope follower (per-phase mid/amp tracking): fast-attack on a new extreme, slow decay
  // otherwise, so it tracks slow drift without a single instant reading resetting it. Units are
  // envelope-fraction per second, e.g. 0.01 = 1% of the current amplitude decays per second.
  float envelope_decay_per_s = 0.02f;

  // The rotor is considered "rotating" -- and the envelope follower allowed to keep adapting --
  // for this many seconds after the last real, hysteresis-confirmed angle step. Two raw-noise
  // heuristics were tried here first (a per-sample delta, then a windowed range with a sustained-
  // crossing requirement) and both were defeated by real hardware noise floors that turned out to
  // vary session-to-session in ways neither threshold anticipated -- one produced steady phantom
  // rotation, the other still let an occasional false "rotating" through even after requiring a
  // full extra second of sustained crossing (TODO.md Phase D). A confirmed accumulation event is
  // categorically different: by the time it fires it has already passed the calibration gate, the
  // r_min signal-quality gate, AND the hysteresis threshold below, so it inherits all of that
  // protection for free instead of needing its own separately-tuned noise ceiling. At this
  // meter's real observed rate, confirmed steps land roughly every 0.25-0.3s during continuous
  // flow (10 deg hysteresis / ~9-10s per revolution), comfortably inside a 2s window with no gaps.
  float rotating_recent_window_s = 2.0f;

  // Signal-quality gate: accumulation (and envelope adaptation, once past the initial fill) is
  // suppressed while r = hypot(alpha, beta) is below this. Diagnoses a detached/failing coil as a
  // collapse in r; also naturally low during the initial envelope-fill period before amp is
  // known. Needs empirical tuning -- observed locus r has ~20% ripple even when healthy
  // (captures/2026-08-21_*, TODO.md Phase A harmonic-content finding), so this must sit well
  // below 1.0, not close to it.
  float r_min = 0.3f;

  // Hysteresis threshold in radians: the tracked angle theta_c only follows the raw
  // atan2()-derived theta once they differ by more than this. This is what makes the accumulator
  // noise-proof -- see design_decisions.md "hysteresis vs deadband". 10 degrees by default
  // (~0.1745 rad), chosen in .plan against the measured ~4-5 degrees of dither this hardware
  // produces at rest.
  float hysteresis_rad = 0.17453293f;  // 10 degrees
};

// Pure algorithm: envelope normalisation -> Clarke transform -> atan2 -> hysteresis-tracked angle
// -> accumulated angle. No ESPHome includes anywhere in this translation unit, deliberately --
// see CLAUDE.md and .plan "Validation": this must compile and run on a desktop so it can be
// tested against captured traces before it ever touches hardware.
class RotationDecoder {
 public:
  explicit RotationDecoder(const DecoderConfig &config = DecoderConfig{});

  // Replaces the whole config in one shot. Exists because ESPHome codegen constructs the owning
  // component with no arguments and applies `set_*()` calls afterward, before setup() runs --
  // there's no way to pass a fully-populated DecoderConfig to the constructor in that flow.
  void set_config(const DecoderConfig &config) { this->config_ = config; }

  // Feed one sample, raw[3] indexed by the *driver's* channel numbering (this applies
  // config_.channel_order itself). A NaN in any channel is a dropped/invalid sample: state holds
  // unchanged, nothing accumulates, matching the driver's own NAN-on-watchdog-error contract
  // (components/ldc1314/ldc1314.cpp read_channel_()).
  void update(const float raw[3], double timestamp_s);

  // Net signed accumulated rotation since construction (or since the last reset()), in radians.
  // This is the one quantity .plan's decoder contract promises -- everything else here is
  // diagnostic.
  double accumulated_angle_rad() const { return this->accumulated_rad_; }
  double revolutions() const;

  // Diagnostics, mirroring the sensor set .plan's YAML contract publishes in Phase D.
  float signal_quality() const { return this->last_r_; }  // last r = hypot(alpha, beta)
  float angle_rad() const { return this->last_theta_; }   // last raw (pre-hysteresis) theta
  bool rotating() const { return this->rotating_; }

  void reset_accumulator() { this->accumulated_rad_ = 0.0; }

  // Learn-pass support (Phase D wires a `button:` to this): overrides the free-running envelope
  // follower with per-phase mid/amp captured over one known revolution. Bypasses the initial-fill
  // bootstrap entirely.
  void set_learned_envelope(const float mid[3], const float amp[3]);

 private:
  void update_envelope_(const float raw[3], double dt);

  DecoderConfig config_;

  float mid_[3] = {0, 0, 0};
  float amp_[3] = {1, 1, 1};  // 1, not 0 -- avoids a divide-by-zero before the envelope fills
  bool envelope_filled_ = false;
  bool envelope_learned_ = false;

  // "rotating" is derived from recent confirmed motion (see rotating_recent_window_s), not a raw
  // noise heuristic -- last_motion_timestamp_s_ is stamped only when the hysteresis-confirmed
  // accumulation step below actually fires.
  double last_motion_timestamp_s_ = 0;
  bool have_motion_timestamp_ = false;
  bool rotating_ = false;

  double theta_c_ = 0;   // hysteresis-tracked angle, unwrapped
  bool have_theta_c_ = false;
  double accumulated_rad_ = 0;

  float last_r_ = 0;
  float last_theta_ = 0;
  bool first_sample_ = true;
  double last_timestamp_s_ = 0;
};

}  // namespace watermeter_core
