#include "rotation_decoder.h"

#include <cmath>

namespace watermeter_core {

static constexpr double kTwoPi = 6.28318530717958647692;
static constexpr double kPi = 3.14159265358979323846;

RotationDecoder::RotationDecoder(const DecoderConfig &config) : config_(config) {}

void RotationDecoder::update_activity_(const float ordered[3]) {
  for (int i = 0; i < 3; i++)
    this->activity_buf_[i][this->activity_buf_pos_] = ordered[i];
  this->activity_buf_pos_ = (this->activity_buf_pos_ + 1) % kActivityWindowSamples;
  if (this->activity_buf_count_ < kActivityWindowSamples)
    this->activity_buf_count_++;

  float range_sum = 0;
  for (int i = 0; i < 3; i++) {
    float lo = this->activity_buf_[i][0];
    float hi = lo;
    for (int j = 1; j < this->activity_buf_count_; j++) {
      float v = this->activity_buf_[i][j];
      if (v < lo)
        lo = v;
      if (v > hi)
        hi = v;
    }
    range_sum += hi - lo;
  }
  this->activity_ = range_sum;

  // A partial (not-yet-full) window must never be read as "parked" -- that would freeze the
  // envelope before it has ever seen real motion, right at startup.
  bool above_threshold = this->activity_buf_count_ >= kActivityWindowSamples && this->activity_ > this->config_.activity_threshold;
  if (above_threshold) {
    if (this->rotating_streak_ < kRotatingSustainSamples)
      this->rotating_streak_++;
  } else {
    this->rotating_streak_ = 0;  // fast-off: see kRotatingSustainSamples's comment in the header
  }
  this->rotating_ = this->rotating_streak_ >= kRotatingSustainSamples;
}

void RotationDecoder::update_envelope_(const float ordered[3], double dt) {
  // A learn pass (Phase D's button:) is authoritative once set -- the free-running follower never
  // overwrites it. Re-enabling auto-adaptation, if ever wanted, would be a deliberate new API, not
  // a side effect of this function running.
  if (this->envelope_learned_)
    return;

  if (!this->envelope_filled_) {
    // Bootstrap: unconditionally widen min/max from whatever's been seen so far, ignoring the
    // rotating_ gate entirely -- there is no prior envelope to protect yet, and gating on
    // rotation here would mean a decoder that happens to start up on a parked meter never fills
    // at all. Ends once every channel has shown a non-trivial spread, i.e. actually varied.
    for (int i = 0; i < 3; i++) {
      if (this->first_sample_) {
        this->mid_[i] = ordered[i];
        this->amp_[i] = 1.0f;  // placeholder so early r/theta don't divide by zero
        continue;
      }
      float lo = this->mid_[i] - this->amp_[i];
      float hi = this->mid_[i] + this->amp_[i];
      if (ordered[i] < lo)
        lo = ordered[i];
      if (ordered[i] > hi)
        hi = ordered[i];
      this->mid_[i] = (lo + hi) / 2.0f;
      this->amp_[i] = (hi - lo) / 2.0f;
    }
    // 5.0 codes, not the original 0.5: found on real hardware, not in review. At the gain-8
    // tuning (TODO.md Phase B), steady-state LSB dither alone reaches amp ~0.5-1.0 -- comfortably
    // above the old 0.5 threshold -- so a meter that never rotates during the whole bootstrap
    // window would "fill" from pure noise and permanently lock in a calibration ~30-50x smaller
    // than the true signal amplitude (~34-48 codes, captures/2026-08-21_phaseb-gain8-offset.csv).
    // Once locked that small, atan2 becomes hypersensitive to the same LSB noise it was
    // calibrated from: theta swings wildly, r reads misleadingly high, and the freeze-while-not-
    // rotating logic then protects the bad calibration indefinitely, since the resulting noise
    // looks enough like "activity" to avoid re-adapting. 5.0 sits comfortably above the measured
    // noise floor (~1-2 codes) and well below the true signal amplitude, so only genuine partial
    // rotation can satisfy it -- matches activity_threshold's own reasoning and margin.
    static constexpr float kBootstrapMinAmp = 5.0f;
    bool all_spread = true;
    for (int i = 0; i < 3; i++) {
      if (this->amp_[i] < kBootstrapMinAmp)
        all_spread = false;
    }
    if (all_spread && !this->first_sample_)
      this->envelope_filled_ = true;
    return;
  }

  // Frozen: an envelope that keeps decaying toward a parked rotor's resting value destroys the
  // transform the moment flow resumes (.plan "Normalise") -- mid/amp simply hold their last
  // known-good values until activity_ says the rotor is moving again.
  if (!this->rotating_)
    return;

  float decay = static_cast<float>(this->config_.envelope_decay_per_s * dt);
  if (decay > 1.0f)
    decay = 1.0f;  // guard a large dt (e.g. after a dropped-sample gap) from overshooting

  for (int i = 0; i < 3; i++) {
    float lo = this->mid_[i] - this->amp_[i];
    float hi = this->mid_[i] + this->amp_[i];
    // Fast attack on a new extreme, slow decay otherwise -- each bound creeps back toward the
    // current sample only when the sample doesn't itself set a new record.
    if (ordered[i] < lo) {
      lo = ordered[i];
    } else {
      lo += (ordered[i] - lo) * decay;
    }
    if (ordered[i] > hi) {
      hi = ordered[i];
    } else {
      hi -= (hi - ordered[i]) * decay;
    }
    this->mid_[i] = (lo + hi) / 2.0f;
    this->amp_[i] = (hi - lo) / 2.0f;
    if (this->amp_[i] < 1e-6f)
      this->amp_[i] = 1e-6f;
  }
}

void RotationDecoder::update(const float raw[3], double timestamp_s) {
  float ordered[3];
  for (int i = 0; i < 3; i++)
    ordered[i] = raw[this->config_.channel_order[i]];

  // Watchdog-invalid samples are NAN on the driver's own contract (ldc1314.cpp read_channel_());
  // treat them as dropped, not as zero or as a real reading -- state holds, nothing accumulates.
  for (int i = 0; i < 3; i++) {
    if (std::isnan(ordered[i]))
      return;
  }

  double dt = this->first_sample_ ? 0.0 : (timestamp_s - this->last_timestamp_s_);
  if (dt < 0)
    dt = 0;  // a timestamp going backwards (e.g. a wraparound clock source) must not un-decay
  this->last_timestamp_s_ = timestamp_s;

  this->update_activity_(ordered);
  this->update_envelope_(ordered, dt);

  float x[3];
  for (int i = 0; i < 3; i++)
    x[i] = (ordered[i] - this->mid_[i]) / this->amp_[i];

  float alpha = (2.0f / 3.0f) * (x[0] - x[1] / 2.0f - x[2] / 2.0f);
  float beta = (x[1] - x[2]) / std::sqrt(3.0f);
  float theta = std::atan2(beta, alpha);
  if (this->config_.invert_direction)
    theta = -theta;

  // Before the envelope is genuinely calibrated (real signal seen, or a learn pass set it),
  // amp_ is derived purely from whatever's been observed so far -- which, for a meter that
  // hasn't rotated yet, is LSB dither alone. Normalizing dither against its own amplitude makes
  // it read as full-scale noise (r near 1, theta swinging wildly across the full range), not the
  // near-zero it should be. Found on real hardware (captures/log3.txt): a meter sitting still
  // reported signal_quality 0.667 and jumped by whole fractions of a revolution with raw codes
  // barely moving. Forcing r to 0 while uncalibrated keeps that noise out of both the published
  // diagnostic and the accumulation gate below, rather than relying on r_min to reject it (r_min
  // is tuned against a real, calibrated locus's ~20% ripple -- not against noise self-normalized
  // to look like signal, which can read arbitrarily high).
  bool calibrated = this->envelope_filled_ || this->envelope_learned_;
  float r = calibrated ? std::hypot(alpha, beta) : 0.0f;

  this->last_r_ = r;
  // Same reasoning as r above, and originally missed: an uncalibrated theta is exactly as
  // meaningless as an uncalibrated r, but the first version of this fix only gated r, so the
  // `angle` diagnostic kept publishing wild swings from raw noise even after r/accumulation were
  // correctly suppressed. Hold the last trustworthy value instead of publishing noise.
  if (calibrated)
    this->last_theta_ = theta;

  if (!calibrated) {
    // Don't anchor theta_c_ to an uncalibrated (noise-derived) theta -- that would cause a
    // spurious jump of up to half a revolution the moment calibration completes and the tracker
    // suddenly compares against a real angle instead. Wait for calibration, then anchor fresh.
    this->have_theta_c_ = false;
  } else if (!this->have_theta_c_) {
    // Anchor the tracker to wherever the rotor happens to be on the first calibrated sample --
    // there is nothing to compare against yet, so nothing can accumulate this sample regardless.
    this->theta_c_ = theta;
    this->have_theta_c_ = true;
  } else if (r > this->config_.r_min) {
    // theta_c_ is unwrapped (grows without bound across many revolutions); theta is atan2's
    // wrapped (-pi, pi] output. Compare them via the wrapped wrapped-shortest-path delta, then
    // apply that same delta to the unwrapped tracker -- this is what keeps theta_c_ continuous
    // across a wraparound instead of jumping by +-2pi.
    double theta_c_wrapped = std::fmod(this->theta_c_, kTwoPi);
    if (theta_c_wrapped > kPi)
      theta_c_wrapped -= kTwoPi;
    if (theta_c_wrapped < -kPi)
      theta_c_wrapped += kTwoPi;

    double delta = static_cast<double>(theta) - theta_c_wrapped;
    if (delta > kPi)
      delta -= kTwoPi;
    if (delta < -kPi)
      delta += kTwoPi;

    // The hysteresis gate: theta_c_ (and with it the accumulator) only moves once the raw angle
    // has pulled more than hysteresis_rad away from it. At rest this holds theta_c_ dead still
    // through the LSB dither documented in TODO.md Phase B; at any real rotation speed it still
    // tracks to within one hysteresis quantum. See design_decisions.md "hysteresis vs deadband"
    // for why a deadband on the per-sample delta was rejected instead.
    if (std::fabs(delta) > this->config_.hysteresis_rad) {
      this->theta_c_ += delta;
      this->accumulated_rad_ += delta;
    }
  }

  this->first_sample_ = false;
}

double RotationDecoder::revolutions() const { return this->accumulated_rad_ / kTwoPi; }

void RotationDecoder::set_learned_envelope(const float mid[3], const float amp[3]) {
  for (int i = 0; i < 3; i++) {
    this->mid_[i] = mid[i];
    this->amp_[i] = (amp[i] > 1e-6f) ? amp[i] : 1e-6f;
  }
  this->envelope_learned_ = true;
  this->envelope_filled_ = true;
}

}  // namespace watermeter_core
