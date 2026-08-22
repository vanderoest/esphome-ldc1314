#pragma once

#include <cstdint>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include "rotation_decoder.h"

namespace esphome {
namespace watermeter {

enum ReverseMode {
  WATERMETER_REVERSE_SUBTRACT = 0,
  WATERMETER_REVERSE_IGNORE = 1,
};

// ESPHome wrapper around ::watermeter_core::RotationDecoder. This is the only file in the component
// that knows about ESPHome, sensor:/binary_sensor: entities, or persistence -- the decoding
// algorithm itself lives in rotation_decoder.h/.cpp with no ESPHome dependency at all (Phase C).
//
// Not a PollingComponent: the three phase sensor: sources publish asynchronously (each is its own
// entity, typically driven by ldc1314's own update() cycle), so this hub reacts to their
// on_state callbacks instead of polling on its own schedule. loop() still runs for the periodic
// bookkeeping that has to happen even when no new phase sample has arrived -- flow_rate timing
// out to zero, and rate-limited persistence.
class WatermeterComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  // A clean reboot or OTA update must not lose whatever's accumulated since the last rate-limited
  // save -- up to save_interval_s_ worth of volume otherwise.
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Hub-level configuration -- set from __init__.py before setup() runs.
  void set_phase_sensors(sensor::Sensor *a, sensor::Sensor *b, sensor::Sensor *c) {
    this->phase_[0] = a;
    this->phase_[1] = b;
    this->phase_[2] = c;
  }
  void set_liters_per_revolution(float v) { this->liters_per_revolution_ = v; }
  void set_invert_direction(bool v) { this->decoder_config_.invert_direction = v; }
  void set_reverse_mode(ReverseMode m) { this->reverse_mode_ = m; }
  void set_hysteresis_rad(float rad) { this->decoder_config_.hysteresis_rad = rad; }
  void set_min_signal(float r) { this->decoder_config_.r_min = r; }
  void set_save_threshold_l(float l) { this->save_threshold_l_ = l; }
  void set_save_interval_s(uint32_t s) { this->save_interval_s_ = s; }
  void set_initial_value_l(double l) { this->initial_value_l_ = l; }
  // How long "flowing"/"continuous_flow"/flow_rate keep reporting activity after the last real
  // increment, before deciding flow has actually stopped. Deliberately NOT the decoder's own
  // rotating_recent_window_s (2s, tuned for envelope protection) -- at this meter's own Q1
  // (~0.10 L/min), confirmed hysteresis steps land only every ~16.7s, so a 2s window made
  // `continuous_flow`'s `delayed_on: 30min` filter reset every ~15s during a genuine slow leak
  // and never reach 30 continuous minutes (found in code review). Meter/installation-specific --
  // exposed as config rather than guessed, since Q1 varies by meter.
  void set_no_flow_timeout_s(uint32_t s) { this->no_flow_timeout_ms_ = s * 1000UL; }
  // Distinguishes preferences between multiple watermeter: instances on one device -- see
  // watermeter.cpp setup() for why the plain component-name hash isn't enough on its own.
  void set_name_hash(uint32_t hash) { this->name_hash_ = hash; }

  // Entity registration -- set from sensor.py/binary_sensor.py before setup() runs. All optional:
  // a nullptr sensor is simply never published to.
  void set_volume_sensor(sensor::Sensor *s) { this->volume_sensor_ = s; }
  void set_flow_rate_sensor(sensor::Sensor *s) { this->flow_rate_sensor_ = s; }
  void set_angle_sensor(sensor::Sensor *s) { this->angle_sensor_ = s; }
  void set_signal_quality_sensor(sensor::Sensor *s) { this->signal_quality_sensor_ = s; }
  void set_revolutions_sensor(sensor::Sensor *s) { this->revolutions_sensor_ = s; }
  void set_reverse_volume_sensor(sensor::Sensor *s) { this->reverse_volume_sensor_ = s; }
  void set_flowing_binary_sensor(binary_sensor::BinarySensor *s) { this->flowing_binary_sensor_ = s; }
  // Fed the exact same "is flowing" boolean as flowing_binary_sensor_ (see publish_() -- based on
  // no_flow_timeout_ms_, not the decoder's own short rotating()) -- the two entities differ only
  // in that this one has a `delayed_on` filter attached (binary_sensor.py's `min_duration`),
  // turning "moving right now" into "has been moving continuously for at least min_duration".
  void set_continuous_flow_binary_sensor(binary_sensor::BinarySensor *s) { this->continuous_flow_binary_sensor_ = s; }

  // `watermeter.set_total` action: re-aligns the published reading to the physical meter face by
  // solving install_offset = x - measured_volume, per .plan "Persistence and meter alignment".
  void set_total(double liters);

  // Learn-pass support (Phase D's `button:`): capture per-phase mid/amp over a window covering
  // at least one revolution -- an installed meter can't be hand-turned to exactly one, so the
  // window just needs to be generous, not precise -- and hand them to the decoder via
  // set_learned_envelope(). Trigger-agnostic, like ldc1314's diagnostics button -- an automation
  // could call this equally well.
  void start_learn_pass(float duration_s);

 protected:
  void on_phase_sample_();
  // Split from a single monolithic publish_() (found in code review, round 3): normal readings
  // are event-driven off the accumulator actually changing, while angle/signal_quality are noisy
  // internal diagnostics that get their own slow, threshold-gated cap instead. See each function's
  // definition for its own trigger.
  void publish_volume_();
  void publish_flow_state_(bool force);
  void publish_diagnostics_(bool force);
  void maybe_save_();
  void save_now_();
  void update_flow_rate_(uint32_t now_ms);

  sensor::Sensor *phase_[3]{nullptr, nullptr, nullptr};
  float last_phase_value_[3] = {NAN, NAN, NAN};
  // Set true when a phase's callback fires, cleared once all three have and a decode has run.
  // The three phase sensors publish as three *separate* callbacks per underlying ldc1314 cycle,
  // not one synchronized triple -- decoding on every individual callback (the first version of
  // this component) mixes a freshly-updated channel with 1-2 channels still holding the
  // *previous* cycle's value on 2 of every 3 calls. Because ldc1314 always reads channels in the
  // same fixed order, that isn't random noise that cancels out -- it's a repeating, systematic
  // bias, and it integrated into real phantom rotation on hardware (a steady, drifting flow_rate
  // reading with the meter sitting still). Waiting for a complete fresh set before decoding fixes
  // it at the source, independent of phases: order or which channel a driver reads last.
  bool phase_fresh_[3]{false, false, false};

  // Output entities -- all optional, set from sensor.py/binary_sensor.py. A nullptr here just
  // means publish_()/update_flow_rate_() skip that one.
  sensor::Sensor *volume_sensor_{nullptr};
  sensor::Sensor *flow_rate_sensor_{nullptr};
  sensor::Sensor *angle_sensor_{nullptr};
  sensor::Sensor *signal_quality_sensor_{nullptr};
  sensor::Sensor *revolutions_sensor_{nullptr};
  sensor::Sensor *reverse_volume_sensor_{nullptr};
  binary_sensor::BinarySensor *flowing_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *continuous_flow_binary_sensor_{nullptr};

  ::watermeter_core::RotationDecoder decoder_;
  ::watermeter_core::DecoderConfig decoder_config_;

  float liters_per_revolution_{1.0f};
  ReverseMode reverse_mode_{WATERMETER_REVERSE_SUBTRACT};

  // The exact accumulators -- double, never float. At a 1.2 M litre install reading, float's
  // ~7 significant digits can't represent a 0.028 L hysteresis increment at all; double's ~15-17
  // keeps the error bounded to one ULP per publish instead of compounding across the meter's
  // life. See TODO.md Phase D and design_decisions.md "accumulator must not be a float".
  double measured_l_{0};
  double reverse_l_{0};
  double install_offset_l_{0};
  double last_decoder_revolutions_{0};
  bool have_last_decoder_revolutions_{false};

  // Persistence, rate-limited by both volume delta and elapsed time -- the time floor is what
  // bounds flash wear (.plan "Storage"): at peak flow the volume delta alone would trigger a
  // write every ~0.3 s.
  ESPPreferenceObject pref_;
  float save_threshold_l_{1.0f};
  uint32_t save_interval_s_{60};
  double initial_value_l_{0};
  double last_saved_measured_l_{0};
  double last_saved_reverse_l_{0};
  uint32_t last_save_ms_{0};
  uint32_t name_hash_{0};

  // Flow rate: a sliding window, forced to zero after a no-increment timeout rather than decaying
  // toward zero slowly once flow actually stops (.plan "Outputs"). The window itself stays an
  // internal constant (watermeter.cpp) since .plan doesn't call for tuning it, but the no-flow
  // timeout is meter-specific (see set_no_flow_timeout_s()) and is exposed as config.
  double flow_window_start_l_{0};
  uint32_t flow_window_start_ms_{0};
  uint32_t last_increment_ms_{0};
  bool flow_rate_is_zero_{true};
  uint32_t no_flow_timeout_ms_{60000};  // overridden by set_no_flow_timeout_s(); see its comment
  // Gates "flowing"/"continuous_flow" so they can't read true from a mere elapsed-time check
  // against an artificially-seeded last_increment_ms_ -- setup() has to set last_increment_ms_ to
  // *some* value before any real sample arrives, and that value being "just now" made both
  // sensors report flowing for a full no_flow_timeout_ms_ after every boot, even with the meter
  // sitting still (found in code review, round 2). Only on_phase_sample_()'s real delta_rev != 0
  // branch sets this true; publish_flow_state_() requires it before considering the elapsed-time
  // check.
  bool have_confirmed_increment_{false};
  // publish_flow_state_() re-derives "flowing" every loop() tick but only calls publish_state()
  // when it differs from this -- edge-triggered, not polled (found in code review, round 3).
  bool last_published_flowing_{false};

  // publish_diagnostics_() caps angle/signal_quality to kDiagnosticsPublishIntervalMs and a
  // minimum-change threshold (watermeter.cpp) -- found in code review, round 3: these are noisy
  // internal diagnostics with no HA-facing need for sub-second resolution, unlike volume/
  // revolutions/reverse_volume which are event-driven off the accumulator actually changing
  // (publish_volume_(), called from on_phase_sample_()'s real delta_rev != 0 branch).
  uint32_t last_diagnostics_publish_ms_{0};
  float last_published_angle_deg_{0};
  float last_published_signal_quality_{0};
  bool have_published_diagnostics_{false};

  // Learn pass: while active, every phase sample is folded into a plain running min/max instead
  // of being decoded, until duration_s elapses -- then the result is handed to the decoder as its
  // new learned envelope. Independent of the decoder's own free-running envelope follower.
  bool learn_active_{false};
  uint32_t learn_end_ms_{0};
  float learn_mid_[3]{0, 0, 0};
  float learn_amp_[3]{0, 0, 0};
  bool learn_have_sample_{false};

  // Tracks whether the decoder's envelope was already filled as of the last time we checked, so
  // on_phase_sample_() can detect the false->true transition (auto-bootstrap completing) and
  // force an immediate save -- otherwise a freshly-calibrated envelope with no volume moved yet
  // (bootstrap, or a learn pass run before any real draw) could sit unsaved indefinitely, since
  // maybe_save_()'s dirty check only looks at measured_l_/reverse_l_ (found in code review,
  // round 2). Learn-pass completion (loop()) saves explicitly too, since envelope_learned can flip
  // true without envelope_filled_ changing (it was already filled by bootstrap).
  bool last_known_envelope_filled_{false};
};

// `watermeter.set_total` action -- see __init__.py's registration.
template<typename... Ts> class SetTotalAction : public Action<Ts...> {
 public:
  SetTotalAction(WatermeterComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(double, value)

  void play(const Ts &...x) override { this->parent_->set_total(this->value_.value(x...)); }

 protected:
  WatermeterComponent *parent_;
};

}  // namespace watermeter
}  // namespace esphome
