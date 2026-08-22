#include "watermeter.h"

#include <cmath>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace watermeter {

static const char *const TAG = "watermeter";
static constexpr float kRadToDeg = 57.29577951308232f;

// Everything persisted through a reboot: the exact measured total, the install-face alignment,
// the reverse-flow diagnostic, and the decoder's own calibration. Plain POD, saved/loaded as raw
// bytes (esphome/core/preferences.h) -- doubles/floats/bools are all fine here, no serialization
// needed.
//
// envelope_valid/envelope_mid/envelope_amp/envelope_learned were added after code review flagged
// that the decoder's calibration was RAM-only: every reboot forced a full re-bootstrap, during
// which nothing accumulates (found to cost ~0.165 revolution in the gain-8 capture alone). Saving
// it means a restart can trust its calibration immediately instead of waiting to see real motion
// again. It's snapshotted opportunistically inside the same rate-limited save as everything else
// (save_now_()) -- being up to save_interval_s_ stale is fine, it only needs to be a good starting
// point, not byte-exact.
struct PersistedState {
  double measured_l;
  double install_offset_l;
  double reverse_l;
  bool envelope_valid;
  float envelope_mid[3];
  float envelope_amp[3];
  bool envelope_learned;
};

// Pre-round-1 layout (3 doubles, no envelope fields) -- kept only so setup() can migrate data
// saved by an older build instead of silently resetting it. ESP32's NVS backend keys the stored
// blob by the hash alone and requires an exact length match to load (esphome/components/esp32/
// preferences.cpp: ESP32PreferenceBackend::load()), so growing PersistedState made every existing
// device's pref_.load() fail and fall through to the "first boot" branch, silently wiping the
// saved total on the next OTA (found by external code review, round 2, and reproduced on real
// hardware). Since the key is derived from the hash only, not from sizeof(T), a *second*
// ESPPreferenceObject built from the same hash but this old, smaller type correctly targets the
// same stored bytes when the new, larger load fails.
struct PersistedStateV1 {
  double measured_l;
  double install_offset_l;
  double reverse_l;
};

// Flow-rate sliding window. Not exposed as YAML config -- .plan's YAML contract doesn't call for
// tuning this. Widened from an original 10s: at low flow (this meter's own Q1, ~0.10 L/min, gives
// one hysteresis-confirmed step only every ~16.7s) a 10s window either catches zero steps (reads
// 0) or one step's full increment averaged over a partial window (overestimates) -- neither is an
// accurate rate. 30s gives a window comfortably wider than the expected inter-step gap at low
// flow, at the cost of somewhat slower responsiveness when a fast draw starts/stops.
static constexpr uint32_t kFlowWindowMs = 30000;

// angle/signal_quality cadence and noise floor (round 3: replaces an earlier flat 200ms/5Hz
// throttle on *every* sensor -- found in code review that a single timer was the wrong model:
// volume/revolutions/reverse_volume are exact accumulators that should publish exactly when they
// change, not on a schedule, while angle/signal_quality are continuous, noisy internal decoder
// state with no HA-facing need for sub-second resolution at all. 1 Hz plus a minimum-change
// threshold keeps these two live on a dashboard without pushing a new state for every LSB of
// decode noise between real steps.
static constexpr uint32_t kDiagnosticsPublishIntervalMs = 1000;
static constexpr float kAngleChangeThresholdDeg = 1.0f;
static constexpr float kSignalQualityChangeThreshold = 0.01f;

void WatermeterComponent::setup() {
  // Codegen calls set_*() after construction but before setup() -- decoder_ was built with a
  // default DecoderConfig at construction time, so the real config has to be applied explicitly
  // here rather than relying on the constructor.
  this->decoder_.set_config(this->decoder_config_);

  for (int i = 0; i < 3; i++) {
    if (this->phase_[i] == nullptr)
      continue;
    this->phase_[i]->add_on_state_callback([this, i](float state) {
      this->last_phase_value_[i] = state;
      this->phase_fresh_[i] = true;
      // Decode only once a complete fresh triple has arrived -- not on every individual
      // callback. See phase_fresh_'s comment in watermeter.h for why that matters.
      if (this->phase_fresh_[0] && this->phase_fresh_[1] && this->phase_fresh_[2]) {
        this->phase_fresh_[0] = this->phase_fresh_[1] = this->phase_fresh_[2] = false;
        this->on_phase_sample_();
      }
    });
  }

  uint32_t hash = fnv1_hash("watermeter") ^ this->name_hash_;
  this->pref_ = global_preferences->make_preference<PersistedState>(hash);
  PersistedState state{};
  bool migrated_from_v1 = false;
  if (this->pref_.load(&state)) {
    this->measured_l_ = state.measured_l;
    this->install_offset_l_ = state.install_offset_l;
    this->reverse_l_ = state.reverse_l;
    if (state.envelope_valid)
      this->decoder_.restore_envelope(state.envelope_mid, state.envelope_amp, state.envelope_learned);
  } else {
    // New-format load failed -- could be a genuinely fresh device, or one with data still saved
    // under the old, smaller layout (see PersistedStateV1's comment). Same hash, so this targets
    // the same stored bytes; only the requested length differs.
    PersistedStateV1 old_state{};
    auto old_pref = global_preferences->make_preference<PersistedStateV1>(hash);
    if (old_pref.load(&old_state)) {
      ESP_LOGI(TAG, "Migrating persisted state from pre-envelope layout");
      this->measured_l_ = old_state.measured_l;
      this->install_offset_l_ = old_state.install_offset_l;
      this->reverse_l_ = old_state.reverse_l;
      migrated_from_v1 = true;
    } else {
      this->measured_l_ = 0;
      this->install_offset_l_ = this->initial_value_l_;
      this->reverse_l_ = 0;
    }
  }
  this->last_saved_measured_l_ = this->measured_l_;
  this->last_saved_reverse_l_ = this->reverse_l_;
  this->last_known_envelope_filled_ = this->decoder_.envelope_filled();

  uint32_t now = millis();
  this->last_save_ms_ = now;
  this->flow_window_start_ms_ = now;
  this->flow_window_start_l_ = this->measured_l_;
  this->last_increment_ms_ = now;

  // Announce the restored/default state once at boot -- force=true bypasses publish_flow_state_()
  // and publish_diagnostics_()'s own change/interval gating, which otherwise have nothing to
  // compare the very first reading against.
  this->publish_volume_();
  this->publish_flow_state_(true);
  this->publish_diagnostics_(true);

  // Re-save immediately in the new format -- otherwise this device stays on the old layout until
  // the next rate-limited save (or reboots again before then and re-runs this same migration).
  if (migrated_from_v1)
    this->save_now_();
}

void WatermeterComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Watermeter:");
  ESP_LOGCONFIG(TAG, "  Liters per revolution: %.4f", this->liters_per_revolution_);
  ESP_LOGCONFIG(TAG, "  Reverse mode: %s",
                this->reverse_mode_ == WATERMETER_REVERSE_SUBTRACT ? "subtract" : "ignore");
  ESP_LOGCONFIG(TAG, "  Direction inverted: %s", YESNO(this->decoder_config_.invert_direction));
  ESP_LOGCONFIG(TAG, "  Hysteresis: %.2f deg", this->decoder_config_.hysteresis_rad * kRadToDeg);
  ESP_LOGCONFIG(TAG, "  Min signal (r_min): %.3f", this->decoder_config_.r_min);
  ESP_LOGCONFIG(TAG, "  Save threshold: %.3f L, save interval: %u s", this->save_threshold_l_,
                static_cast<unsigned>(this->save_interval_s_));
  ESP_LOGCONFIG(TAG, "  Measured volume: %.4f L, install offset: %.4f L, published: %.4f L",
                this->measured_l_, this->install_offset_l_, this->install_offset_l_ + this->measured_l_);
  LOG_SENSOR("  ", "Volume", this->volume_sensor_);
  LOG_SENSOR("  ", "Flow rate", this->flow_rate_sensor_);
  LOG_SENSOR("  ", "Angle", this->angle_sensor_);
  LOG_SENSOR("  ", "Signal quality", this->signal_quality_sensor_);
  LOG_SENSOR("  ", "Revolutions", this->revolutions_sensor_);
  LOG_SENSOR("  ", "Reverse volume", this->reverse_volume_sensor_);
  LOG_BINARY_SENSOR("  ", "Flowing", this->flowing_binary_sensor_);
}

void WatermeterComponent::loop() {
  uint32_t now = millis();

  // Wrap-safe comparison (found in code review): `now >= learn_end_ms_` breaks the moment millis()
  // wraps past learn_end_ms_ (~49.7 days of uptime) -- now becomes a small value again, so a plain
  // >= would stay false and the learn pass would never end on its own. Casting the difference to
  // signed is the standard idiom: it's correct across the wrap as long as the actual gap between
  // now and learn_end_ms_ is under ~24.8 days, true for any sane learn-pass duration.
  if (this->learn_active_ && static_cast<int32_t>(now - this->learn_end_ms_) >= 0) {
    this->learn_active_ = false;
    if (this->learn_have_sample_) {
      float mid[3], amp[3];
      for (int i = 0; i < 3; i++) {
        float lo = this->learn_mid_[i];
        float hi = this->learn_amp_[i];
        mid[i] = (lo + hi) / 2.0f;
        amp[i] = (hi - lo) / 2.0f;
        if (amp[i] < 1e-6f)
          amp[i] = 1e-6f;
      }
      this->decoder_.set_learned_envelope(mid, amp);
      ESP_LOGI(TAG, "Learn pass complete: mid=[%.1f, %.1f, %.1f] amp=[%.1f, %.1f, %.1f]", mid[0], mid[1], mid[2],
               amp[0], amp[1], amp[2]);
      // Explicit save: envelope_learned can flip true here without envelope_filled_ changing (it
      // was already filled by bootstrap), so on_phase_sample_()'s filled-transition check alone
      // would miss this case and a freshly-learned envelope could be lost on the next reboot
      // (found in code review, round 2).
      this->last_known_envelope_filled_ = this->decoder_.envelope_filled();
      this->save_now_();
    } else {
      ESP_LOGW(TAG, "Learn pass ended with no valid samples -- envelope unchanged");
    }
  }

  // Both self-gate (change-triggered / interval+threshold-gated -- see their own definitions), so
  // it's cheap and correct to call them unconditionally on every loop() tick rather than
  // rate-limiting the call site itself.
  this->publish_flow_state_(false);
  this->publish_diagnostics_(false);

  this->update_flow_rate_(now);
  this->maybe_save_();
}

void WatermeterComponent::on_phase_sample_() {
  if (this->learn_active_) {
    for (int i = 0; i < 3; i++) {
      if (std::isnan(this->last_phase_value_[i]))
        return;  // wait for a fully valid sample before folding it into the learn window
    }
    for (int i = 0; i < 3; i++) {
      float v = this->last_phase_value_[i];
      if (!this->learn_have_sample_) {
        // learn_mid_/learn_amp_ double as running lo/hi while the pass is active -- resolved
        // into an actual mid/amp pair only once the pass ends (loop()).
        this->learn_mid_[i] = v;
        this->learn_amp_[i] = v;
      } else {
        if (v < this->learn_mid_[i])
          this->learn_mid_[i] = v;
        if (v > this->learn_amp_[i])
          this->learn_amp_[i] = v;
      }
    }
    this->learn_have_sample_ = true;
    return;
  }

  double timestamp_s = millis() / 1000.0;
  this->decoder_.update(this->last_phase_value_, timestamp_s);

  // Auto-bootstrap completing (envelope_filled_ flipping false->true with no learn pass involved)
  // wouldn't otherwise trigger a save until measured_l_/reverse_l_ next drifted past
  // save_threshold_l_, which can take a while at low flow -- a reboot in that window would lose
  // the calibration and force a full re-bootstrap again (found in code review, round 2).
  if (!this->last_known_envelope_filled_ && this->decoder_.envelope_filled()) {
    this->last_known_envelope_filled_ = true;
    this->save_now_();
  }

  double now_rev = this->decoder_.revolutions();
  if (!this->have_last_decoder_revolutions_) {
    this->last_decoder_revolutions_ = now_rev;
    this->have_last_decoder_revolutions_ = true;
  } else {
    double delta_rev = now_rev - this->last_decoder_revolutions_;
    this->last_decoder_revolutions_ = now_rev;
    if (delta_rev != 0) {
      double delta_l = delta_rev * this->liters_per_revolution_;
      if (delta_l >= 0) {
        this->measured_l_ += delta_l;
      } else {
        // Reverse motion is tracked as a diagnostic regardless of reverse_mode_; only whether it
        // also subtracts from the published total depends on the mode (.plan "reverse: ignore").
        this->reverse_l_ += -delta_l;
        if (this->reverse_mode_ == WATERMETER_REVERSE_SUBTRACT)
          this->measured_l_ += delta_l;
      }
      this->last_increment_ms_ = millis();
      this->have_confirmed_increment_ = true;
      // Event-driven: publish exactly when the accumulator actually moved, not on a timer (round
      // 3). publish_flow_state_()'s own edge-trigger will pick up the resulting "flowing" change
      // on the next loop() tick, a few ms away at most -- not worth duplicating here too.
      this->publish_volume_();
    }
  }
}

// Exact accumulators -- called whenever one of them actually changes (on_phase_sample_()'s real
// delta_rev != 0 branch, set_total(), and once at boot), never on a timer.
void WatermeterComponent::publish_volume_() {
  if (this->volume_sensor_ != nullptr)
    this->volume_sensor_->publish_state(static_cast<float>(this->install_offset_l_ + this->measured_l_));
  if (this->revolutions_sensor_ != nullptr)
    this->revolutions_sensor_->publish_state(static_cast<float>(this->decoder_.revolutions()));
  if (this->reverse_volume_sensor_ != nullptr)
    this->reverse_volume_sensor_->publish_state(static_cast<float>(this->reverse_l_));
}

// "Flowing" is "a real hysteresis-confirmed step happened within no_flow_timeout_ms_" --
// deliberately NOT the decoder's own rotating() (a 2s window tuned for envelope protection, see
// its comment in rotation_decoder.h): at this meter's own Q1, confirmed steps land only every
// ~16.7s, so gating on rotating() made `flowing` flicker off between every step and meant
// `continuous_flow`'s `delayed_on: 30min` filter reset constantly during a genuine slow leak,
// never reaching 30 continuous minutes (found in code review). last_increment_ms_ covers real
// backflow too even under reverse: ignore, where measured_l_ itself wouldn't move -- it's stamped
// on any nonzero delta_rev, forward or backward. continuous_flow gets the exact same raw signal;
// it's the `delayed_on` filter binary_sensor.py attaches to that entity (not any hub-side logic)
// that turns it into "moving continuously for at least min_duration".
//
// Called every loop() tick since the false transition is purely time-driven (no sample has to
// arrive for "it's been quiet too long" to become true), but only calls publish_state() when the
// value actually changed -- edge-triggered, not polled (round 3: found in code review that most
// callers only need to hear about this on a state change, not once a tick).
void WatermeterComponent::publish_flow_state_(bool force) {
  // have_confirmed_increment_ gates this: without it, setup() has to seed last_increment_ms_ to
  // *some* value before any real sample arrives, and "just now" made the elapsed-time check alone
  // read true (elapsed ~= 0 < no_flow_timeout_ms_) for a full no_flow_timeout_ms_ after every
  // single boot, even with the meter sitting completely still (found in code review, round 2).
  bool flowing = this->have_confirmed_increment_ && (millis() - this->last_increment_ms_) < this->no_flow_timeout_ms_;
  if (!force && flowing == this->last_published_flowing_)
    return;
  this->last_published_flowing_ = flowing;
  if (this->flowing_binary_sensor_ != nullptr)
    this->flowing_binary_sensor_->publish_state(flowing);
  if (this->continuous_flow_binary_sensor_ != nullptr)
    this->continuous_flow_binary_sensor_->publish_state(flowing);
}

// Noisy internal decoder state, capped to kDiagnosticsPublishIntervalMs and gated by a minimum
// apparent change -- unlike the exact accumulators above, nothing HA-facing needs sub-second
// resolution here, and publishing every LSB of decode noise between real steps would just add API
// traffic for no benefit (round 3, mirrors the reasoning that used to apply to every sensor here).
void WatermeterComponent::publish_diagnostics_(bool force) {
  uint32_t now = millis();
  if (!force && now - this->last_diagnostics_publish_ms_ < kDiagnosticsPublishIntervalMs)
    return;
  this->last_diagnostics_publish_ms_ = now;

  float angle_deg = this->decoder_.angle_rad() * kRadToDeg;
  if (force || !this->have_published_diagnostics_ ||
      std::fabs(angle_deg - this->last_published_angle_deg_) >= kAngleChangeThresholdDeg) {
    this->last_published_angle_deg_ = angle_deg;
    if (this->angle_sensor_ != nullptr)
      this->angle_sensor_->publish_state(angle_deg);
  }

  float signal_quality = this->decoder_.signal_quality();
  if (force || !this->have_published_diagnostics_ ||
      std::fabs(signal_quality - this->last_published_signal_quality_) >= kSignalQualityChangeThreshold) {
    this->last_published_signal_quality_ = signal_quality;
    if (this->signal_quality_sensor_ != nullptr)
      this->signal_quality_sensor_->publish_state(signal_quality);
  }

  this->have_published_diagnostics_ = true;
}

void WatermeterComponent::update_flow_rate_(uint32_t now_ms) {
  // Force-zero path: report "flow stopped" as soon as the timeout elapses rather than waiting for
  // the sliding window to close, which could otherwise show a stale nonzero rate for seconds
  // after the tap was actually shut (.plan "Outputs": "forced to zero after a no-increment
  // timeout").
  if (!this->flow_rate_is_zero_ && (now_ms - this->last_increment_ms_) > this->no_flow_timeout_ms_) {
    if (this->flow_rate_sensor_ != nullptr)
      this->flow_rate_sensor_->publish_state(0.0f);
    this->flow_rate_is_zero_ = true;
    this->flow_window_start_l_ = this->measured_l_;
    this->flow_window_start_ms_ = now_ms;
    return;
  }

  if (now_ms - this->flow_window_start_ms_ < kFlowWindowMs)
    return;

  double elapsed_min = (now_ms - this->flow_window_start_ms_) / 60000.0;
  double delta_l = this->measured_l_ - this->flow_window_start_l_;
  double rate = elapsed_min > 0 ? delta_l / elapsed_min : 0.0;
  if (this->flow_rate_sensor_ != nullptr)
    this->flow_rate_sensor_->publish_state(static_cast<float>(rate));
  this->flow_rate_is_zero_ = rate == 0.0;
  this->flow_window_start_l_ = this->measured_l_;
  this->flow_window_start_ms_ = now_ms;
}

void WatermeterComponent::maybe_save_() {
  uint32_t now = millis();
  if (now - this->last_save_ms_ < this->save_interval_s_ * 1000UL)
    return;
  // Checking only measured_l_ here missed reverse_l_ moving on its own -- under reverse: ignore,
  // backflow accumulates into reverse_l_ without ever touching measured_l_, so a save could never
  // trigger and reverse_l_ would not survive a reboot (found in code review). Same threshold as
  // measured_l_: the same per-step granularity and flash-wear reasoning applies to either one.
  bool measured_dirty = std::fabs(this->measured_l_ - this->last_saved_measured_l_) >= this->save_threshold_l_;
  bool reverse_dirty = std::fabs(this->reverse_l_ - this->last_saved_reverse_l_) >= this->save_threshold_l_;
  if (!measured_dirty && !reverse_dirty)
    return;
  this->save_now_();
}

void WatermeterComponent::save_now_() {
  PersistedState state{};
  state.measured_l = this->measured_l_;
  state.install_offset_l = this->install_offset_l_;
  state.reverse_l = this->reverse_l_;
  state.envelope_valid = this->decoder_.envelope_filled();
  if (state.envelope_valid) {
    this->decoder_.get_envelope(state.envelope_mid, state.envelope_amp);
    state.envelope_learned = this->decoder_.envelope_learned();
  }
  this->pref_.save(&state);
  this->last_saved_measured_l_ = this->measured_l_;
  this->last_saved_reverse_l_ = this->reverse_l_;
  this->last_save_ms_ = millis();
}

void WatermeterComponent::set_total(double liters) {
  this->install_offset_l_ = liters - this->measured_l_;
  this->publish_volume_();
  // A deliberate, infrequent user action -- worth an immediate save rather than waiting on the
  // flash-wear rate limit that protects against continuous small increments.
  this->save_now_();
  ESP_LOGI(TAG, "set_total: install_offset now %.4f L (published %.4f L)", this->install_offset_l_, liters);
}

void WatermeterComponent::on_shutdown() {
  // Same reverse_l_ gap as maybe_save_() -- fixed here too, for the same reason.
  if (this->measured_l_ != this->last_saved_measured_l_ || this->reverse_l_ != this->last_saved_reverse_l_)
    this->save_now_();
}

void WatermeterComponent::start_learn_pass(float duration_s) {
  this->learn_active_ = true;
  this->learn_end_ms_ = millis() + static_cast<uint32_t>(duration_s * 1000.0f);
  this->learn_have_sample_ = false;
  ESP_LOGI(TAG, "Learn pass started: let water run continuously for the next %.1f s (needs to cover at "
                "least one full revolution, extra flow doesn't hurt)",
           duration_s);
}

}  // namespace watermeter
}  // namespace esphome
