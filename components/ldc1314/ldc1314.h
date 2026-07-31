#pragma once

#include <string>

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include "ldc1314_registers.h"
#include "ldc1314_settings.h"

namespace esphome {
namespace ldc1314 {

enum ReferenceClockSource {
  LDC1314_REFERENCE_CLOCK_INTERNAL = 0,
  LDC1314_REFERENCE_CLOCK_EXTERNAL = 1,
};

enum Deglitch {
  LDC1314_DEGLITCH_1MHZ = 0b001,
  LDC1314_DEGLITCH_3_3MHZ = 0b100,
  LDC1314_DEGLITCH_10MHZ = 0b101,
  LDC1314_DEGLITCH_33MHZ = 0b111,
};

enum OutputGain {
  LDC1314_OUTPUT_GAIN_1 = 0b00,
  LDC1314_OUTPUT_GAIN_4 = 0b01,
  LDC1314_OUTPUT_GAIN_8 = 0b10,
  LDC1314_OUTPUT_GAIN_16 = 0b11,
};

enum SensorActivationMode {
  LDC1314_SENSOR_ACTIVATE_FULL_CURRENT = 0,
  LDC1314_SENSOR_ACTIVATE_LOW_POWER = 1,
};

// Characterization state machine stages -- see .plan "The staged procedure". IDLE is both the
// resting state and the successful-completion state; every other stage is transient.
enum CharacterizationStage : uint8_t {
  CHAR_STAGE_IDLE = 0,
  CHAR_STAGE_PREPARE,
  CHAR_STAGE_AUTO_IDRIVE,
  CHAR_STAGE_ENVELOPE,
  CHAR_STAGE_DERIVE,
  CHAR_STAGE_VERIFY,
  CHAR_STAGE_COMMIT,
};

/// Driver for the Texas Instruments LDC1314 4-channel inductance-to-digital converter.
///
/// This component only talks to the device and publishes raw per-channel conversion codes; it
/// deliberately does not compute distance, flow, or any other physical/application-specific
/// quantity -- see design_decisions.md ("Publish raw values, not liters/flow").
///
/// It also owns an optional, user-triggered sensor characterization procedure that derives
/// IDRIVE/OFFSET/OUTPUT_GAIN for a coil it has never seen, and a three-layer value resolution
/// model (YAML seed -> stored calibration -> armed manual override) -- see .plan "Value
/// resolution model". Characterization never runs automatically and never leaves the device in
/// auto-amplitude mode outside of a bounded, attended run.
class LDC1314Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  // Global (hub-level) configuration -- set from __init__.py before setup() runs.
  void set_reference_clock_source(ReferenceClockSource source) { this->reference_clock_source_ = source; }
  void set_deglitch(Deglitch deglitch) { this->deglitch_ = deglitch; }
  void set_output_gain(OutputGain gain) { this->yaml_output_gain_ = gain; }
  void set_high_current_drive(bool enabled) { this->high_current_drive_ = enabled; }
  void set_sensor_activation_mode(SensorActivationMode mode) { this->sensor_activation_mode_ = mode; }
  void set_report_errors_on_intb(bool enabled) { this->report_errors_on_intb_ = enabled; }

  // Per-channel configuration and entity registration -- set from sensor.py/binary_sensor.py
  // before setup() runs. `channel` is 0-based and must be < MAX_CHANNELS. `idrive`/`offset` here
  // are the YAML seed layer only -- see the effective_*_() resolvers.
  void set_channel_sensor(uint8_t channel, sensor::Sensor *sensor) { this->channels_[channel].sensor = sensor; }
  void set_channel_error_binary_sensor(uint8_t channel, binary_sensor::BinarySensor *binary_sensor) {
    this->channels_[channel].error_binary_sensor = binary_sensor;
  }
  void set_channel_calibrated_idrive_sensor(uint8_t channel, sensor::Sensor *sensor) {
    this->channels_[channel].calibrated_idrive_sensor = sensor;
  }
  void set_channel_calibrated_offset_sensor(uint8_t channel, sensor::Sensor *sensor) {
    this->channels_[channel].calibrated_offset_sensor = sensor;
  }
  void set_calibrated_output_gain_sensor(sensor::Sensor *sensor) { this->calibrated_output_gain_sensor_ = sensor; }
  void set_channel_rcount(uint8_t channel, uint16_t rcount) { this->channels_[channel].rcount = rcount; }
  void set_channel_settlecount(uint8_t channel, uint16_t settlecount) {
    this->channels_[channel].settlecount = settlecount;
  }
  void set_channel_offset(uint8_t channel, uint16_t offset) { this->channels_[channel].yaml_offset = offset; }
  void set_channel_fin_divider(uint8_t channel, uint8_t fin_divider) {
    this->channels_[channel].fin_divider = fin_divider;
  }
  void set_channel_fref_divider(uint8_t channel, uint16_t fref_divider) {
    this->channels_[channel].fref_divider = fref_divider;
  }
  void set_channel_idrive(uint8_t channel, uint8_t idrive) { this->channels_[channel].yaml_idrive = idrive; }

  // Manual layer (record B) -- set live by the `number`/`switch` platforms. Each of these
  // updates the stored override record and, if the override is armed, re-applies the effective
  // configuration immediately (no reboot required).
  void set_manual_idrive(uint8_t channel, uint8_t idrive);
  void set_manual_offset(uint8_t channel, uint16_t offset);
  void set_manual_output_gain(uint8_t gain_value);  // raw 1/4/8/16, not the enum
  void set_override_armed(bool armed);
  bool override_armed() const { return this->override_.armed != 0; }
  uint8_t manual_idrive(uint8_t channel) const { return this->override_.idrive[channel]; }
  uint16_t manual_offset(uint8_t channel) const { return this->override_.offset[channel]; }
  uint8_t manual_output_gain_value() const { return output_gain_to_value_(static_cast<OutputGain>(this->override_.output_gain)); }

  // Characterization configuration -- set from __init__.py before setup() runs.
  void set_char_restore(bool restore) { this->char_restore_ = restore; }
  void set_char_idle_duration(uint32_t ms) { this->char_idle_duration_ms_ = ms; }
  void set_char_stage_duration(uint32_t ms) { this->char_stage_duration_ms_ = ms; }
  void set_char_verify_duration(uint32_t ms) { this->char_verify_duration_ms_ = ms; }
  void set_char_sample_interval(uint32_t ms) { this->char_sample_interval_ms_ = ms; }
  void set_char_progress_interval(uint32_t ms) { this->char_progress_interval_ms_ = ms; }
  void set_char_headroom(float fraction) { this->char_headroom_ = fraction; }
  void set_char_unify_channels(bool unify) { this->char_unify_channels_ = unify; }
  void set_char_prompt_start(const std::string &text) { this->char_prompt_start_ = text; }
  void set_char_prompt_stop(const std::string &text) { this->char_prompt_stop_ = text; }

  // Characterization control -- the engine's trigger-agnostic entry points. Every trigger path
  // (button entity, physical GPIO button via an automation, a script) calls one of these; none of
  // them know or care which triggered it. See .plan "The engine is independent of its trigger".
  void start_characterization();
  void clear_characterization();
  void clear_overrides();
  bool is_characterizing() const { return this->char_stage_ != CHAR_STAGE_IDLE; }

 protected:
  struct Channel {
    sensor::Sensor *sensor{nullptr};
    binary_sensor::BinarySensor *error_binary_sensor{nullptr};
    sensor::Sensor *calibrated_idrive_sensor{nullptr};
    sensor::Sensor *calibrated_offset_sensor{nullptr};

    uint16_t rcount{0x0080};
    uint16_t settlecount{0x0000};
    uint8_t fin_divider{1};
    uint16_t fref_divider{1};

    // YAML seed layer -- lowest precedence. Renamed from the old `offset`/`idrive` fields to make
    // clear these are never applied directly; effective_idrive_()/effective_offset_() resolve
    // the actual value written to hardware.
    uint16_t yaml_offset{0x0000};
    uint8_t yaml_idrive{0};

    bool active() const { return this->sensor != nullptr || this->error_binary_sensor != nullptr; }
  };

  // --- identity / reset / runtime reconfiguration ---
  bool verify_identity_();
  bool reset_();
  bool enter_sleep_();
  bool exit_sleep_(uint16_t config);
  // Resolves the effective value of every register from the three-layer model, writes the full
  // register set (sleep -> globals -> per-channel -> wake), and is the single code path used by
  // setup(), every manual-layer edit, and every characterization stage transition.
  //
  // `auto_amplitude` and `rp_override` let the characterization engine temporarily select TI's
  // auto-calibration mode (RP_OVERRIDE_EN=0, AUTO_AMP_DIS=0) instead of the fixed-drive production
  // mode; every other caller passes (true, true) for production values.
  bool apply_config_(bool rp_override = true, bool auto_amp_dis = true);
  uint16_t compose_config_(bool sleep, bool rp_override, bool auto_amp_dis) const;
  bool write_channel_config_(uint8_t channel, uint8_t idrive, uint16_t offset);
  // Low-level sleep/write/wake helper used only by the characterization engine: writes an
  // explicit (gain, per-channel idrive, per-channel offset) tuple without touching the persisted
  // layers, leaving RCOUNT/SETTLECOUNT/CLOCK_DIVIDERS/MUX_CONFIG/ERROR_CONFIG untouched (they are
  // not characterized -- see .plan "The staged procedure").
  bool char_write_registers_(OutputGain gain, const uint8_t *idrive, const uint16_t *offset, bool rp_override,
                              bool auto_amp_dis);
  void read_channel_(uint8_t channel);
  void read_status_();
  uint8_t active_channel_count_() const;
  uint8_t highest_active_channel_() const;

  // --- effective value resolution (the three-layer model, .plan "Value resolution model") ---
  uint8_t effective_idrive_(uint8_t channel) const;
  uint16_t effective_offset_(uint8_t channel) const;
  OutputGain effective_output_gain_() const;
  bool channel_calibrated_(uint8_t channel) const;
  static uint8_t output_gain_to_value_(OutputGain gain);
  static OutputGain output_gain_from_value_(uint8_t value);

  // --- raw I/O helpers, used by both normal operation and the characterization engine ---
  bool read_channel_raw_(uint8_t channel, uint16_t *raw);
  bool read_status_raw_(uint16_t *status);
  bool read_init_idrive_(uint8_t channel, uint8_t *init_idrive);

  // --- settings persistence (ldc1314_settings.h records) ---
  void restore_settings_();
  bool save_calibration_(const CalibrationRecord &rec);
  bool save_override_();
  void seed_override_from_calibration_();

  // --- characterization state machine, implemented in ldc1314_characterization.cpp ---
  //
  // The PREPARE/AUTO_IDRIVE/ENVELOPE/VERIFY observation stages sample once per char_tick_() call
  // and are dispatched inline in its switch (they *are* the per-loop sampling loop); only the
  // stage-duration-elapsed decision points that aren't just "take one more sample" get their own
  // function.
  void char_tick_();
  void char_enter_stage_(CharacterizationStage stage);
  void char_abort_(const std::string &reason);
  void char_stage_derive_();
  void char_stage_verify_();
  void char_stage_commit_();
  void char_progress_();
  void char_report_(bool success, const std::string &failure_reason);

  Channel channels_[MAX_CHANNELS];

  ReferenceClockSource reference_clock_source_{LDC1314_REFERENCE_CLOCK_INTERNAL};
  Deglitch deglitch_{LDC1314_DEGLITCH_33MHZ};
  OutputGain yaml_output_gain_{LDC1314_OUTPUT_GAIN_1};
  bool high_current_drive_{false};
  SensorActivationMode sensor_activation_mode_{LDC1314_SENSOR_ACTIVATE_FULL_CURRENT};
  bool report_errors_on_intb_{false};

  // Companion to the per-channel CH[n]| trace, cached so dump_config() (which runs after the API
  // client connects) can print them -- setup()'s own trace only ever reaches the serial console.
  uint16_t last_mux_config_{0};
  uint16_t last_error_config_{0};
  uint16_t last_reset_dev_{0};
  uint16_t last_config_{0};
  uint16_t last_clock_dividers_[MAX_CHANNELS]{};
  uint16_t last_drive_current_[MAX_CHANNELS]{};

  // Settings store -- record A (calibration) and record B (manual override).
  ESPPreferenceObject calibration_pref_;
  ESPPreferenceObject override_pref_;
  CalibrationRecord calibration_{};
  OverrideRecord override_{};
  bool calibration_present_{false};

  sensor::Sensor *calibrated_output_gain_sensor_{nullptr};

  // Characterization configuration.
  bool char_restore_{true};
  uint32_t char_idle_duration_ms_{10000};
  uint32_t char_stage_duration_ms_{60000};
  uint32_t char_verify_duration_ms_{30000};
  uint32_t char_sample_interval_ms_{20};
  uint32_t char_progress_interval_ms_{5000};
  float char_headroom_{0.25f};
  bool char_unify_channels_{false};
  std::string char_prompt_start_{"Start moving the target through its full range now."};
  std::string char_prompt_stop_{"You can stop moving the target now."};

  // Characterization runtime state.
  CharacterizationStage char_stage_{CHAR_STAGE_IDLE};
  uint32_t char_stage_start_ms_{0};
  uint32_t char_last_sample_ms_{0};
  uint32_t char_last_progress_ms_{0};
  uint16_t char_status_or_{0};
  uint32_t char_idle_samples_{0};
  uint32_t char_drive_samples_{0};
  uint32_t char_envelope_samples_{0};
  uint32_t char_verify_samples_{0};
  uint32_t char_i2c_failures_{0};
  uint8_t char_verify_retries_{0};

  struct ChannelAccum {
    uint8_t idrive_min{31};
    uint8_t idrive_max{0};
    uint16_t code_min{0x0FFF};
    uint16_t code_max{0};
    uint16_t idle_code{0};
    uint16_t verify_min{0x0FFF};
    uint16_t verify_max{0};
  };
  ChannelAccum char_accum_[MAX_CHANNELS];

  uint8_t char_result_idrive_{0};
  OutputGain char_result_output_gain_{LDC1314_OUTPUT_GAIN_1};
  uint16_t char_result_offset_[MAX_CHANNELS]{};
  uint8_t char_binding_channel_{0};
  uint32_t char_verify_ur_{0};
  uint32_t char_verify_or_{0};

  // Snapshot of the effective configuration immediately before a run started -- restored on
  // abort at any stage, and never left partially applied. See .plan "Abort path".
  uint8_t char_snapshot_idrive_[MAX_CHANNELS]{};
  uint16_t char_snapshot_offset_[MAX_CHANNELS]{};
  OutputGain char_snapshot_output_gain_{LDC1314_OUTPUT_GAIN_1};
};

}  // namespace ldc1314
}  // namespace esphome
