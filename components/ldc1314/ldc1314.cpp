#include "ldc1314.h"

#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <cmath>

namespace esphome {
namespace ldc1314 {

static const char *const TAG = "ldc1314";

static const char *deglitch_to_string(Deglitch deglitch) {
  switch (deglitch) {
    case LDC1314_DEGLITCH_1MHZ:
      return "1 MHz";
    case LDC1314_DEGLITCH_3_3MHZ:
      return "3.3 MHz";
    case LDC1314_DEGLITCH_10MHZ:
      return "10 MHz";
    case LDC1314_DEGLITCH_33MHZ:
    default:
      return "33 MHz";
  }
}

uint8_t LDC1314Component::output_gain_to_value_(OutputGain gain) {
  switch (gain) {
    case LDC1314_OUTPUT_GAIN_4:
      return 4;
    case LDC1314_OUTPUT_GAIN_8:
      return 8;
    case LDC1314_OUTPUT_GAIN_16:
      return 16;
    case LDC1314_OUTPUT_GAIN_1:
    default:
      return 1;
  }
}

OutputGain LDC1314Component::output_gain_from_value_(uint8_t value) {
  switch (value) {
    case 4:
      return LDC1314_OUTPUT_GAIN_4;
    case 8:
      return LDC1314_OUTPUT_GAIN_8;
    case 16:
      return LDC1314_OUTPUT_GAIN_16;
    case 1:
    default:
      return LDC1314_OUTPUT_GAIN_1;
  }
}

void LDC1314Component::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");

  // Communication failures here are treated as a hard failure (mark_failed()); an identity
  // mismatch is not, since it could be transient I2C noise -- see verify_identity_().
  if (!this->verify_identity_()) {
    this->mark_failed();
    return;
  }

  if (!this->reset_()) {
    ESP_LOGE(TAG, "Failed to reset device");
    this->mark_failed();
    return;
  }

  this->restore_settings_();

  if (!this->apply_config_()) {
    ESP_LOGE(TAG, "Failed to configure device");
    this->mark_failed();
    return;
  }
}

void LDC1314Component::dump_config() {
  ESP_LOGCONFIG(TAG, "LDC1314:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Reference clock: %s",
                this->reference_clock_source_ == LDC1314_REFERENCE_CLOCK_EXTERNAL ? "external (CLKIN)" : "internal");
  ESP_LOGCONFIG(TAG, "  Deglitch filter: %s", deglitch_to_string(this->deglitch_));
  ESP_LOGCONFIG(TAG, "  Report errors on INTB: %s", YESNO(this->report_errors_on_intb_));

  OutputGain effective_gain = this->effective_output_gain_();
  if (this->override_.armed) {
    ESP_LOGCONFIG(TAG, "  Output gain: %ux (MANUAL, ARMED; yaml %ux)", output_gain_to_value_(effective_gain),
                  output_gain_to_value_(this->yaml_output_gain_));
  } else if (this->calibration_present_) {
    ESP_LOGCONFIG(TAG, "  Output gain: %ux (calibrated; yaml %ux)", output_gain_to_value_(effective_gain),
                  output_gain_to_value_(this->yaml_output_gain_));
  } else {
    ESP_LOGCONFIG(TAG, "  Output gain: %ux (yaml)", output_gain_to_value_(effective_gain));
  }
  ESP_LOGCONFIG(TAG, "  Override: %s", this->override_.armed ? "ARMED" : "disarmed");
  if (this->calibration_present_) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_CHANNELS; i++) {
      if (this->calibration_.channel_mask & (1 << i))
        count++;
    }
    ESP_LOGCONFIG(TAG, "  Calibration: present, %u channel(s), v%u", count, this->calibration_.version);
  } else {
    ESP_LOGCONFIG(TAG, "  Calibration: none");
  }

  for (uint8_t channel = 0; channel < MAX_CHANNELS; channel++) {
    const Channel &ch = this->channels_[channel];
    if (!ch.active())
      continue;
    ESP_LOGCONFIG(TAG, "  Channel %u:", channel);
    LOG_SENSOR("    ", "Raw value", ch.sensor);
    LOG_BINARY_SENSOR("    ", "Error", ch.error_binary_sensor);
    LOG_SENSOR("    ", "Calibrated IDRIVE", ch.calibrated_idrive_sensor);
    LOG_SENSOR("    ", "Calibrated OFFSET", ch.calibrated_offset_sensor);
    ESP_LOGCONFIG(TAG, "    RCOUNT: 0x%04X, SETTLECOUNT: 0x%04X (yaml)", ch.rcount, ch.settlecount);

    uint16_t offset = this->effective_offset_(channel);
    uint8_t idrive = this->effective_idrive_(channel);
    const char *source = this->override_.armed               ? "MANUAL, ARMED"
                         : this->channel_calibrated_(channel) ? "calibrated"
                                                               : "yaml";
    ESP_LOGCONFIG(TAG, "    OFFSET: 0x%04X (%s; yaml 0x%04X)", offset, source, ch.yaml_offset);
    ESP_LOGCONFIG(TAG, "    IDRIVE: %u (%s; yaml %u)", idrive, source, ch.yaml_idrive);
    ESP_LOGCONFIG(TAG, "    FIN_DIVIDER: %u, FREF_DIVIDER: %u", ch.fin_divider, ch.fref_divider);
  }

  // Composed register words actually written. Phase 2 fix: this now runs from dump_config(),
  // which fires after the API client connects (as the existing [C] boot lines prove) -- the old
  // trace fired only inside setup()/configure_(), which runs before the API client connects, so
  // it only ever reached the serial console. Format deliberately mirrors the stock HomeWizard
  // firmware's own boot log (docs/original-homewizard-boot.md §6) so the two can be diffed by eye.
  if (!this->is_failed()) {
    for (uint8_t channel = 0; channel < MAX_CHANNELS; channel++) {
      if (!this->channels_[channel].active())
        continue;
      ESP_LOGD(TAG, "CH[%u]| RCOUNT:0x%04X| OFFSET:0x%04X| SETTLE:0x%04X| CLOCK:0x%04X| DRIVE:0x%04X", channel,
               this->channels_[channel].rcount, this->effective_offset_(channel),
               this->channels_[channel].settlecount, this->last_clock_dividers_[channel],
               this->last_drive_current_[channel]);
    }
    ESP_LOGD(TAG, "GLOBAL| MUX_CONFIG:0x%04X| ERROR_CONFIG:0x%04X| RESET_DEV:0x%04X| CONFIG:0x%04X",
             this->last_mux_config_, this->last_error_config_, this->last_reset_dev_, this->last_config_);
  }
}

void LDC1314Component::update() {
  if (this->is_failed())
    return;
  if (this->is_characterizing())
    return;  // registers are in a transient, non-production state -- publishing would be garbage

  // Multi-channel readback race (docs/knowledge_base.md "Measurement flow"): DATAx is
  // overwritten by that channel's next conversion if not read before it completes. This driver
  // sidesteps that by always reading every active channel's DATAx unconditionally every cycle,
  // rather than gating reads on STATUS.UNREADCONVx -- worst case with a fast device / slow
  // update_interval is a redundant read of an unchanged value, not corrupted/skipped data.

  // Read STATUS first: it's read-to-clear and, per docs/knowledge_base.md "Error handling", the
  // driver's read order should be consistent so a second channel's error isn't dropped from
  // attribution by an out-of-order DATAx read.
  this->read_status_();

  for (uint8_t channel = 0; channel < MAX_CHANNELS; channel++) {
    if (this->channels_[channel].active()) {
      this->read_channel_(channel);
    }
  }
}

void LDC1314Component::loop() {
  if (this->char_stage_ != CHAR_STAGE_IDLE) {
    this->char_tick_();
  }
}

bool LDC1314Component::verify_identity_() {
  uint16_t manufacturer_id = 0;
  uint16_t device_id = 0;
  if (!this->read_byte_16(REG_MANUFACTURER_ID, &manufacturer_id) ||
      !this->read_byte_16(REG_DEVICE_ID, &device_id)) {
    ESP_LOGE(TAG, "Failed to read MANUFACTURER_ID/DEVICE_ID -- check wiring and I2C address");
    return false;
  }
  if (manufacturer_id != MANUFACTURER_ID_VALUE || device_id != DEVICE_ID_VALUE) {
    // Not a hard failure: could be I2C noise on an otherwise-working bus. Register writes further
    // along in setup() will fail (and mark_failed()) on their own if communication is truly bad.
    ESP_LOGW(TAG, "Unexpected MANUFACTURER_ID/DEVICE_ID: got 0x%04X/0x%04X, expected 0x%04X/0x%04X", manufacturer_id,
             device_id, MANUFACTURER_ID_VALUE, DEVICE_ID_VALUE);
  }
  return true;
}

bool LDC1314Component::reset_() {
  if (!this->write_byte_16(REG_RESET_DEV, RESET_DEV_RESET)) {
    return false;
  }
  // RESET_DEV.RESET_DEV always reads back 0 (register_map.md) -- it cannot be polled for
  // completion, and the datasheet gives no explicit reset-complete timing. Wait a conservative
  // fixed delay, then re-verify identity to confirm the device is responsive again.
  delay(10);
  return this->verify_identity_();
}

bool LDC1314Component::enter_sleep_() {
  // The other CONFIG bits don't matter while asleep -- conversions are stopped and no channel
  // data is produced until the device wakes again with the fully-composed word below.
  return this->write_byte_16(REG_CONFIG, this->compose_config_(true, true, true));
}

bool LDC1314Component::exit_sleep_(uint16_t config) {
  // `config` must already have SLEEP_MODE_EN clear (compose_config_(false, ...)) -- writing it is
  // what exits Sleep Mode and starts conversions.
  return this->write_byte_16(REG_CONFIG, config);
}

uint16_t LDC1314Component::compose_config_(bool sleep, bool rp_override, bool auto_amp_dis) const {
  uint8_t highest = this->highest_active_channel_();
  bool autoscan = this->active_channel_count_() > 1;

  uint16_t config = CONFIG_RESERVED_BITS;
  if (sleep) {
    config |= CONFIG_SLEEP_MODE_EN;
  }
  if (rp_override) {
    // Fixed drive current from IDRIVEx, no auto-amplitude drift -- production default. The
    // characterization engine's AUTO_IDRIVE stage is the only caller that passes false.
    config |= CONFIG_RP_OVERRIDE_EN;
  }
  if (auto_amp_dis) {
    config |= CONFIG_AUTO_AMP_DIS;
  }
  if (!autoscan) {
    config |= static_cast<uint16_t>(highest) << CONFIG_ACTIVE_CHAN_SHIFT;
  }
  if (this->sensor_activation_mode_ == LDC1314_SENSOR_ACTIVATE_LOW_POWER) {
    config |= CONFIG_SENSOR_ACTIVATE_SEL;
  }
  if (this->reference_clock_source_ == LDC1314_REFERENCE_CLOCK_EXTERNAL) {
    config |= CONFIG_REF_CLK_SRC;
  }
  if (!this->report_errors_on_intb_) {
    config |= CONFIG_INTB_DIS;
  }
  if (this->high_current_drive_) {
    config |= CONFIG_HIGH_CURRENT_DRV;
  }
  return config;
}

bool LDC1314Component::apply_config_(bool rp_override, bool auto_amp_dis) {
  uint8_t active_count = this->active_channel_count_();
  uint8_t highest = this->highest_active_channel_();
  bool autoscan = active_count > 1;

  if (active_count == 0) {
    ESP_LOGW(TAG, "No channels configured -- nothing will be measured");
  } else if (autoscan) {
    // The device only supports scanning a contiguous run of channels starting at 0
    // (MUX_CONFIG.RR_SEQUENCE, register_map.md). Warn if the configured channels have a gap.
    for (uint8_t i = 0; i < highest; i++) {
      if (!this->channels_[i].active()) {
        ESP_LOGW(TAG,
                 "Channel %u has no sensor/binary_sensor configured but channel %u does -- the "
                 "device always scans contiguously from channel 0, so channel %u will still be "
                 "measured without being published",
                 i, highest, i);
      }
    }
  }

  if (!this->enter_sleep_()) {
    ESP_LOGE(TAG, "Failed to enter Sleep Mode before reconfiguring");
    return false;
  }

  // MUX_CONFIG: deglitch filter + auto-scan sequence (or single-channel continuous mode).
  uint16_t mux_config = MUX_CONFIG_RESERVED_BITS | (static_cast<uint16_t>(this->deglitch_) & MUX_CONFIG_DEGLITCH_MASK);
  if (autoscan) {
    mux_config |= MUX_CONFIG_AUTOSCAN_EN;
    // RR_SEQUENCE: 00=Ch0,Ch1  01=Ch0,Ch1,Ch2  10=Ch0,Ch1,Ch2,Ch3 (register_map.md §7.6.25).
    uint16_t rr_sequence = 0b00;
    if (highest >= 3) {
      rr_sequence = 0b10;
    } else if (highest >= 2) {
      rr_sequence = 0b01;
    }
    mux_config |= rr_sequence << MUX_CONFIG_RR_SEQUENCE_SHIFT;
  }
  if (!this->write_byte_16(REG_MUX_CONFIG, mux_config)) {
    ESP_LOGE(TAG, "Failed to write MUX_CONFIG");
    return false;
  }

  // ERROR_CONFIG: both *_ERR2OUT (per-channel DATAx error bits) and *_ERR2INT (which despite the
  // name also gates whether STATUS.ERR_* updates at all, see register_map.md) are always enabled;
  // the physical INTB pin stays separately gated by CONFIG.INTB_DIS below.
  uint16_t error_config = ERROR_CONFIG_UR_ERR2OUT | ERROR_CONFIG_OR_ERR2OUT | ERROR_CONFIG_WD_ERR2OUT |
                           ERROR_CONFIG_AH_ERR2OUT | ERROR_CONFIG_AL_ERR2OUT | ERROR_CONFIG_UR_ERR2INT |
                           ERROR_CONFIG_OR_ERR2INT | ERROR_CONFIG_WD_ERR2INT | ERROR_CONFIG_AH_ERR2INT |
                           ERROR_CONFIG_AL_ERR2INT | ERROR_CONFIG_ZC_ERR2INT;
  if (this->report_errors_on_intb_) {
    error_config |= ERROR_CONFIG_DRDY_2INT;
  }
  if (!this->write_byte_16(REG_ERROR_CONFIG, error_config)) {
    ESP_LOGE(TAG, "Failed to write ERROR_CONFIG");
    return false;
  }

  // RESET_DEV: only the OUTPUT_GAIN field is meaningful to write here (the RESET bit is a
  // one-shot action, already handled in reset_()). OUTPUT_GAIN is resolved through the
  // three-layer model since it is a single device-global value.
  OutputGain output_gain = this->effective_output_gain_();
  uint16_t reset_dev =
      (static_cast<uint16_t>(output_gain) & RESET_DEV_OUTPUT_GAIN_MASK) << RESET_DEV_OUTPUT_GAIN_SHIFT;
  if (!this->write_byte_16(REG_RESET_DEV, reset_dev)) {
    ESP_LOGE(TAG, "Failed to write RESET_DEV (output gain)");
    return false;
  }

  for (uint8_t channel = 0; channel < MAX_CHANNELS; channel++) {
    if (!this->channels_[channel].active())
      continue;
    uint8_t idrive = this->effective_idrive_(channel);
    uint16_t offset = this->effective_offset_(channel);
    if (!this->write_channel_config_(channel, idrive, offset)) {
      ESP_LOGE(TAG, "Failed to write channel %u configuration", channel);
      return false;
    }
  }

  // CONFIG is written last: it both applies the remaining global bits and, by clearing
  // SLEEP_MODE_EN, exits Sleep Mode and starts conversions. Registers cannot be changed once
  // conversions have started -- see docs/knowledge_base.md "Initialization sequence".
  uint16_t config = this->compose_config_(false, rp_override, auto_amp_dis);
  if (!this->exit_sleep_(config)) {
    ESP_LOGE(TAG, "Failed to write CONFIG");
    return false;
  }

  this->last_mux_config_ = mux_config;
  this->last_error_config_ = error_config;
  this->last_reset_dev_ = reset_dev;
  this->last_config_ = config;

  return true;
}

bool LDC1314Component::write_channel_config_(uint8_t channel, uint8_t idrive, uint16_t offset) {
  const Channel &ch = this->channels_[channel];

  if (!this->write_byte_16(rcount_register(channel), ch.rcount))
    return false;
  if (!this->write_byte_16(settlecount_register(channel), ch.settlecount))
    return false;
  if (!this->write_byte_16(offset_register(channel), offset))
    return false;

  uint16_t clock_dividers = (static_cast<uint16_t>(ch.fin_divider) << CLOCK_DIVIDERS_FIN_DIVIDER_SHIFT) |
                            (ch.fref_divider & CLOCK_DIVIDERS_FREF_DIVIDER_MASK);
  if (!this->write_byte_16(clock_dividers_register(channel), clock_dividers))
    return false;

  uint16_t drive_current = static_cast<uint16_t>(idrive) << DRIVE_CURRENT_IDRIVE_SHIFT;
  if (!this->write_byte_16(drive_current_register(channel), drive_current))
    return false;

  this->last_clock_dividers_[channel] = clock_dividers;
  this->last_drive_current_[channel] = drive_current;

  return true;
}

void LDC1314Component::read_channel_(uint8_t channel) {
  Channel &ch = this->channels_[channel];
  uint16_t raw = 0;
  if (!this->read_channel_raw_(channel, &raw)) {
    ESP_LOGW(TAG, "Channel %u: failed to read DATA%u", channel, channel);
    this->status_set_warning();
    if (ch.sensor != nullptr)
      ch.sensor->publish_state(NAN);
    if (ch.error_binary_sensor != nullptr)
      ch.error_binary_sensor->publish_state(true);
    return;
  }

  bool error = (raw & (DATA_ERR_UR | DATA_ERR_OR | DATA_ERR_WD | DATA_ERR_AE)) != 0;
  if (error) {
    ESP_LOGD(TAG, "Channel %u: conversion error (DATA=0x%04X: %s%s%s%s)", channel, raw,
             (raw & DATA_ERR_UR) ? "under-range " : "", (raw & DATA_ERR_OR) ? "over-range " : "",
             (raw & DATA_ERR_WD) ? "watchdog " : "", (raw & DATA_ERR_AE) ? "amplitude " : "");
  }
  if (ch.error_binary_sensor != nullptr)
    ch.error_binary_sensor->publish_state(error);

  if (ch.sensor != nullptr) {
    // Watchdog-timeout data is explicitly invalid per the datasheet
    // (docs/summaries/status_monitoring_summary.md) and must not be published as a real reading.
    // Under-range/over-range/amplitude-warning data is still a valid (if boundary) conversion
    // result and is published as-is.
    bool discard = (raw & DATA_ERR_WD) != 0;
    ch.sensor->publish_state(discard ? NAN : static_cast<float>(raw & DATA_RESULT_MASK));
  }
}

void LDC1314Component::read_status_() {
  uint16_t status = 0;
  if (!this->read_status_raw_(&status)) {
    ESP_LOGW(TAG, "Failed to read STATUS");
    this->status_set_warning();
    return;
  }

  static const uint16_t ERROR_BITS =
      STATUS_ERR_UR | STATUS_ERR_OR | STATUS_ERR_WD | STATUS_ERR_AHE | STATUS_ERR_ALE | STATUS_ERR_ZC;
  if (status & ERROR_BITS) {
    uint8_t err_chan = (status >> STATUS_ERR_CHAN_SHIFT) & STATUS_ERR_CHAN_MASK;
    // Naming the individual bits matters: unlike DATAx.ERR_AE (which OR-es the two amplitude
    // conditions together) STATUS separates them, so this is the only place that says whether
    // the sensor drive current is too high or too low.
    ESP_LOGD(TAG,
             "STATUS=0x%04X, first attributed to channel %u:%s%s%s%s%s%s -- see per-channel error "
             "flags for full multi-channel attribution",
             status, err_chan, (status & STATUS_ERR_UR) ? " under-range" : "",
             (status & STATUS_ERR_OR) ? " over-range" : "", (status & STATUS_ERR_WD) ? " watchdog-timeout" : "",
             (status & STATUS_ERR_AHE) ? " amplitude-high(reduce idrive)" : "",
             (status & STATUS_ERR_ALE) ? " amplitude-low(raise idrive)" : "",
             (status & STATUS_ERR_ZC) ? " zero-count" : "");
  }
  this->status_clear_warning();
}

uint8_t LDC1314Component::active_channel_count_() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MAX_CHANNELS; i++) {
    if (this->channels_[i].active())
      count++;
  }
  return count;
}

uint8_t LDC1314Component::highest_active_channel_() const {
  uint8_t highest = 0;
  for (uint8_t i = 0; i < MAX_CHANNELS; i++) {
    if (this->channels_[i].active())
      highest = i;
  }
  return highest;
}

// --- Three-layer value resolution (.plan "Value resolution model") -------------------------
//
//   effective(f) = override_armed        ? manual[f]
//                : channel_calibrated_() ? calibration[f]
//                :                         yaml[f]

uint8_t LDC1314Component::effective_idrive_(uint8_t channel) const {
  if (this->override_.armed)
    return this->override_.idrive[channel];
  if (this->channel_calibrated_(channel))
    return this->calibration_.idrive[channel];
  return this->channels_[channel].yaml_idrive;
}

uint16_t LDC1314Component::effective_offset_(uint8_t channel) const {
  if (this->override_.armed)
    return this->override_.offset[channel];
  if (this->channel_calibrated_(channel))
    return this->calibration_.offset[channel];
  return this->channels_[channel].yaml_offset;
}

OutputGain LDC1314Component::effective_output_gain_() const {
  if (this->override_.armed)
    return static_cast<OutputGain>(this->override_.output_gain);
  if (this->calibration_present_)
    return static_cast<OutputGain>(this->calibration_.output_gain);
  return this->yaml_output_gain_;
}

bool LDC1314Component::channel_calibrated_(uint8_t channel) const {
  return this->calibration_present_ && (this->calibration_.channel_mask & (1 << channel)) != 0;
}

// --- Raw I/O helpers, shared by normal operation and the characterization engine -----------

bool LDC1314Component::read_channel_raw_(uint8_t channel, uint16_t *raw) { return this->read_byte_16(data_register(channel), raw); }

bool LDC1314Component::read_status_raw_(uint16_t *status) { return this->read_byte_16(REG_STATUS, status); }

bool LDC1314Component::read_init_idrive_(uint8_t channel, uint8_t *init_idrive) {
  uint16_t raw = 0;
  if (!this->read_byte_16(drive_current_register(channel), &raw))
    return false;
  *init_idrive = static_cast<uint8_t>((raw >> DRIVE_CURRENT_INIT_IDRIVE_SHIFT) & DRIVE_CURRENT_INIT_IDRIVE_MASK);
  return true;
}

// --- Settings persistence (ldc1314_settings.h records) -------------------------------------

void LDC1314Component::restore_settings_() {
  // Hash includes the I2C address so multiple MULTI_CONF hubs on the same device don't collide.
  std::string addr_key = "ldc1314_cal_" + std::to_string(this->get_i2c_address());
  std::string ovr_key = "ldc1314_ovr_" + std::to_string(this->get_i2c_address());
  this->calibration_pref_ = global_preferences->make_preference<CalibrationRecord>(fnv1_hash(addr_key));
  this->override_pref_ = global_preferences->make_preference<OverrideRecord>(fnv1_hash(ovr_key));

  CalibrationRecord cal{};
  if (this->calibration_pref_.load(&cal) && cal.version == LDC1314_SETTINGS_VERSION) {
    this->calibration_ = cal;
    this->calibration_present_ = true;
  }

  OverrideRecord ovr{};
  if (this->override_pref_.load(&ovr) && ovr.version == LDC1314_SETTINGS_VERSION) {
    this->override_ = ovr;
  } else {
    this->seed_override_from_calibration_();
  }
}

bool LDC1314Component::save_calibration_(const CalibrationRecord &rec) {
  this->calibration_ = rec;
  this->calibration_present_ = true;
  bool ok = this->calibration_pref_.save(&this->calibration_);
  global_preferences->sync();
  return ok;
}

bool LDC1314Component::save_override_() {
  bool ok = this->override_pref_.save(&this->override_);
  global_preferences->sync();
  return ok;
}

void LDC1314Component::seed_override_from_calibration_() {
  // Record B always holds a complete set of values so the `number` entities always show a
  // sensible starting point -- see .plan "Value resolution model". `armed` is deliberately left
  // untouched: seeding is not the same action as arming.
  this->override_.version = LDC1314_SETTINGS_VERSION;
  this->override_.output_gain =
      this->calibration_present_ ? this->calibration_.output_gain : static_cast<uint8_t>(this->yaml_output_gain_);
  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    if (this->channel_calibrated_(ch)) {
      this->override_.idrive[ch] = this->calibration_.idrive[ch];
      this->override_.offset[ch] = this->calibration_.offset[ch];
    } else {
      this->override_.idrive[ch] = this->channels_[ch].yaml_idrive;
      this->override_.offset[ch] = this->channels_[ch].yaml_offset;
    }
  }
  this->save_override_();
}

// --- Manual layer entry points, called live by the number/switch platforms -----------------

void LDC1314Component::set_manual_idrive(uint8_t channel, uint8_t idrive) {
  this->override_.idrive[channel] = idrive;
  this->save_override_();
  if (this->override_.armed && !this->is_characterizing()) {
    this->apply_config_();
  }
}

void LDC1314Component::set_manual_offset(uint8_t channel, uint16_t offset) {
  this->override_.offset[channel] = offset;
  this->save_override_();
  if (this->override_.armed && !this->is_characterizing()) {
    this->apply_config_();
  }
}

void LDC1314Component::set_manual_output_gain(uint8_t gain_value) {
  this->override_.output_gain = static_cast<uint8_t>(output_gain_from_value_(gain_value));
  this->save_override_();
  if (this->override_.armed && !this->is_characterizing()) {
    this->apply_config_();
  }
}

void LDC1314Component::set_override_armed(bool armed) {
  this->override_.armed = armed ? 1 : 0;
  this->save_override_();
  ESP_LOGI(TAG, "Manual override %s", armed ? "ARMED" : "disarmed");
  if (!this->is_characterizing()) {
    this->apply_config_();
  }
}

// --- Characterization control -- trigger-agnostic entry points (.plan "The engine is
// independent of its trigger") --------------------------------------------------------------

void LDC1314Component::start_characterization() {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot characterize: component failed setup");
    return;
  }
  if (this->is_characterizing()) {
    ESP_LOGW(TAG, "Characterization already in progress -- ignoring request");
    return;
  }
  if (this->active_channel_count_() == 0) {
    ESP_LOGE(TAG, "Cannot characterize: no channels configured");
    return;
  }
  this->char_enter_stage_(CHAR_STAGE_PREPARE);
}

void LDC1314Component::clear_characterization() {
  if (this->is_characterizing()) {
    ESP_LOGW(TAG, "Cannot clear calibration while characterization is in progress");
    return;
  }
  this->calibration_ = CalibrationRecord{};
  this->calibration_.version = LDC1314_SETTINGS_VERSION;
  this->calibration_present_ = false;
  this->calibration_pref_.save(&this->calibration_);
  global_preferences->sync();
  ESP_LOGI(TAG, "Calibration cleared -- YAML values restored (unless a manual override is armed)");
  if (!this->override_.armed) {
    this->seed_override_from_calibration_();
  }
  this->apply_config_();
}

void LDC1314Component::clear_overrides() {
  if (this->is_characterizing()) {
    ESP_LOGW(TAG, "Cannot reset overrides while characterization is in progress");
    return;
  }
  ESP_LOGI(TAG, "Resetting manual overrides to the calibrated (or YAML) values");
  this->seed_override_from_calibration_();
  if (this->override_.armed) {
    this->apply_config_();
  }
}

}  // namespace ldc1314
}  // namespace esphome
