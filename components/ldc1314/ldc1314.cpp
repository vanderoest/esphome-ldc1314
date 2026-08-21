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

uint8_t LDC1314Component::output_gain_shift_(OutputGain gain) {
  switch (gain) {
    case LDC1314_OUTPUT_GAIN_4:
      return 2;
    case LDC1314_OUTPUT_GAIN_8:
      return 3;
    case LDC1314_OUTPUT_GAIN_16:
      return 4;
    case LDC1314_OUTPUT_GAIN_1:
    default:
      return 0;
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

  ESP_LOGCONFIG(TAG, "  Output gain: %ux", output_gain_to_value_(this->output_gain_));

  for (uint8_t channel = 0; channel < MAX_CHANNELS; channel++) {
    const Channel &ch = this->channels_[channel];
    if (!ch.active())
      continue;
    ESP_LOGCONFIG(TAG, "  Channel %u:", channel);
    LOG_SENSOR("    ", "Raw value", ch.sensor);
    LOG_BINARY_SENSOR("    ", "Error", ch.error_binary_sensor);
    ESP_LOGCONFIG(TAG, "    RCOUNT: 0x%04X, SETTLECOUNT: 0x%04X", ch.rcount, ch.settlecount);
    ESP_LOGCONFIG(TAG, "    OFFSET: 0x%04X, IDRIVE: %u", ch.offset, ch.idrive);
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
               this->channels_[channel].rcount, this->channels_[channel].offset,
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

  // Multi-channel readback race (docs/knowledge_base.md "Measurement flow"): DATAx is
  // overwritten by that channel's next conversion if not read before it completes. This driver
  // sidesteps that by always reading every active channel's DATAx unconditionally every cycle,
  // rather than gating reads on STATUS.UNREADCONVx -- worst case with a fast device / slow
  // update_interval is a redundant read of an unchanged value, not corrupted/skipped data.

  uint32_t now = millis();

  // STATUS is read-to-clear diagnostic-only state; at fast update_interval it is not worth a
  // full extra I2C transaction every cycle, so it runs on its own slow cadence instead
  // (.plan Part 1.3). When it does fire, it still runs before the DATAx reads below, per
  // docs/knowledge_base.md "Error handling", so a second channel's error isn't dropped from
  // attribution by an out-of-order DATAx read.
  if (now - this->last_status_read_ms_ >= STATUS_READ_INTERVAL_MS_) {
    this->last_status_read_ms_ = now;
    this->read_status_();
  }

  uint16_t raw_values[MAX_CHANNELS];
  for (uint8_t channel = 0; channel < MAX_CHANNELS; channel++) {
    raw_values[channel] = INVALID_RAW_;
    if (this->channels_[channel].active()) {
      raw_values[channel] = this->read_channel_(channel);
    }
  }

  this->log_trace_(now, raw_values);
  this->maybe_log_error_summary_(now);
}

void LDC1314Component::run_diagnostics() {
  PreflightResult result{};
  if (!this->run_preflight_(&result)) {
    ESP_LOGW(TAG, "Diagnostics skipped: no channel could be read");
    return;
  }
  this->log_preflight_(result);
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
  return this->write_byte_16(REG_CONFIG, this->compose_config_(true));
}

bool LDC1314Component::exit_sleep_(uint16_t config) {
  // `config` must already have SLEEP_MODE_EN clear (compose_config_(false)) -- writing it is what
  // exits Sleep Mode and starts conversions.
  return this->write_byte_16(REG_CONFIG, config);
}

uint16_t LDC1314Component::compose_config_(bool sleep) const {
  uint8_t highest = this->highest_active_channel_();
  bool autoscan = this->active_channel_count_() > 1;

  // RP_OVERRIDE_EN=1 / AUTO_AMP_DIS=1 are unconditional: the drive current is always the fixed
  // IDRIVEx from configuration. The device's own auto-amplitude mode is never enabled, because it
  // can adjust drive mid-measurement and inject an offset that reads as a step in target position
  // (SNOA950 §4).
  uint16_t config = CONFIG_RESERVED_BITS | CONFIG_RP_OVERRIDE_EN | CONFIG_AUTO_AMP_DIS;
  if (sleep) {
    config |= CONFIG_SLEEP_MODE_EN;
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

bool LDC1314Component::apply_config_() {
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

  // ERROR_CONFIG (0xF8FC): both *_ERR2OUT (per-channel DATAx error bits) and *_ERR2INT (which
  // despite the name also gates whether STATUS.ERR_* updates at all, see register_map.md) are
  // always enabled; the physical INTB pin stays separately gated by CONFIG.INTB_DIS below.
  // DRDY_2INT stays off: `report_errors_on_intb` promises error assertion only, and routing
  // data-ready to the same pin would assert it on every conversion, drowning the errors out.
  uint16_t error_config = ERROR_CONFIG_UR_ERR2OUT | ERROR_CONFIG_OR_ERR2OUT | ERROR_CONFIG_WD_ERR2OUT |
                           ERROR_CONFIG_AH_ERR2OUT | ERROR_CONFIG_AL_ERR2OUT | ERROR_CONFIG_UR_ERR2INT |
                           ERROR_CONFIG_OR_ERR2INT | ERROR_CONFIG_WD_ERR2INT | ERROR_CONFIG_AH_ERR2INT |
                           ERROR_CONFIG_AL_ERR2INT | ERROR_CONFIG_ZC_ERR2INT;
  if (!this->write_byte_16(REG_ERROR_CONFIG, error_config)) {
    ESP_LOGE(TAG, "Failed to write ERROR_CONFIG");
    return false;
  }

  // RESET_DEV: only the OUTPUT_GAIN field is meaningful to write here (the RESET bit is a
  // one-shot action, already handled in reset_()). OUTPUT_GAIN is device-global, not per-channel.
  uint16_t reset_dev =
      (static_cast<uint16_t>(this->output_gain_) & RESET_DEV_OUTPUT_GAIN_MASK) << RESET_DEV_OUTPUT_GAIN_SHIFT;
  if (!this->write_byte_16(REG_RESET_DEV, reset_dev)) {
    ESP_LOGE(TAG, "Failed to write RESET_DEV (output gain)");
    return false;
  }

  for (uint8_t channel = 0; channel < MAX_CHANNELS; channel++) {
    if (!this->channels_[channel].active())
      continue;
    if (!this->write_channel_config_(channel, this->channels_[channel].idrive, this->channels_[channel].offset)) {
      ESP_LOGE(TAG, "Failed to write channel %u configuration", channel);
      return false;
    }
  }

  // CONFIG is written last: it both applies the remaining global bits and, by clearing
  // SLEEP_MODE_EN, exits Sleep Mode and starts conversions. Registers cannot be changed once
  // conversions have started -- see docs/knowledge_base.md "Initialization sequence".
  uint16_t config = this->compose_config_(false);
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

uint16_t LDC1314Component::read_channel_(uint8_t channel) {
  Channel &ch = this->channels_[channel];
  uint16_t raw = 0;
  if (!this->read_channel_raw_(channel, &raw)) {
    ESP_LOGW(TAG, "Channel %u: failed to read DATA%u", channel, channel);
    this->status_set_warning();
    if (ch.sensor != nullptr)
      ch.sensor->publish_state(NAN);
    if (ch.error_binary_sensor != nullptr)
      ch.error_binary_sensor->publish_state(true);
    return INVALID_RAW_;
  }

  bool error = (raw & (DATA_ERR_UR | DATA_ERR_OR | DATA_ERR_WD | DATA_ERR_AE)) != 0;
  // Log only on error-state transition, not every errored conversion: ERR_ALE is asserted on
  // every conversion on this board (design_decisions.md "Persistent amplitude errors are
  // tolerated, not chased"), so at 100 Hz per-conversion logging is 300 lines/s (.plan Part 1.2).
  // A running count still accumulates below for the periodic summary.
  if (error != this->channel_error_state_[channel]) {
    this->channel_error_state_[channel] = error;
    if (error) {
      ESP_LOGD(TAG, "Channel %u: conversion error started (DATA=0x%04X: %s%s%s%s)", channel, raw,
               (raw & DATA_ERR_UR) ? "under-range " : "", (raw & DATA_ERR_OR) ? "over-range " : "",
               (raw & DATA_ERR_WD) ? "watchdog " : "", (raw & DATA_ERR_AE) ? "amplitude " : "");
    } else {
      ESP_LOGD(TAG, "Channel %u: conversion error cleared", channel);
    }
  }
  if (error) {
    this->channel_error_count_[channel]++;
  }

  if (ch.error_binary_sensor != nullptr)
    ch.error_binary_sensor->publish_state(error);

  // Watchdog-timeout data is explicitly invalid per the datasheet
  // (docs/summaries/status_monitoring_summary.md) and must not be published as a real reading, or
  // fed into the raw trace as if it were a sample. Under-range/over-range/amplitude-warning data
  // is still a valid (if boundary) conversion result and is published/traced as-is.
  bool discard = (raw & DATA_ERR_WD) != 0;
  if (ch.sensor != nullptr)
    ch.sensor->publish_state(discard ? NAN : static_cast<float>(raw & DATA_RESULT_MASK));

  return discard ? INVALID_RAW_ : (raw & DATA_RESULT_MASK);
}

void LDC1314Component::maybe_log_error_summary_(uint32_t now) {
  if (now - this->last_error_summary_ms_ < ERROR_SUMMARY_INTERVAL_MS_)
    return;
  uint32_t elapsed_ms = now - this->last_error_summary_ms_;
  this->last_error_summary_ms_ = now;

  bool any = false;
  for (uint8_t channel = 0; channel < MAX_CHANNELS; channel++) {
    if (this->channel_error_count_[channel] > 0)
      any = true;
  }
  if (!any)
    return;

  ESP_LOGD(TAG, "Error summary (last %u ms):", static_cast<unsigned>(elapsed_ms));
  for (uint8_t channel = 0; channel < MAX_CHANNELS; channel++) {
    if (!this->channels_[channel].active())
      continue;
    if (this->channel_error_count_[channel] > 0) {
      ESP_LOGD(TAG, "  Channel %u: %u error(s)", channel, static_cast<unsigned>(this->channel_error_count_[channel]));
    }
    this->channel_error_count_[channel] = 0;
  }
}

void LDC1314Component::log_trace_(uint32_t now, const uint16_t *raw_values) const {
  // Gated by the `switch:` platform (set_trace_enabled()), not by log verbosity: at 100 Hz this
  // is a 100 lines/s flood with no state-transition to gate on -- every cycle is meant to be
  // logged during a capture -- so it can't reuse the error-summary approach above. Raising
  // `logger:` to VERY_VERBOSE to unlock it was tried and rejected: it drags every other
  // component's logging along with it, and ESPHome won't allow a per-tag `logs:` override more
  // verbose than the global level to claw that back. A plain runtime flag sidesteps log
  // verbosity entirely -- flip the switch on, capture, flip it off, no reflash either way.
  if (!this->trace_enabled_)
    return;

  // One CSV line per update() cycle -- `ts,ch0,ch1,ch2,...` over the active channels -- so a
  // capture session is `esphome logs | grep TRACE, > trace.csv` (.plan Part 1, "Add a raw-trace
  // log line"; used by Phase A's offline analysis in `tools/`).
  std::string line = str_sprintf("TRACE,%u", static_cast<unsigned>(now));
  for (uint8_t channel = 0; channel < MAX_CHANNELS; channel++) {
    if (!this->channels_[channel].active())
      continue;
    if (raw_values[channel] == INVALID_RAW_) {
      line += ",NAN";
    } else {
      line += str_sprintf(",%u", raw_values[channel]);
    }
  }
  ESP_LOGD(TAG, "%s", line.c_str());
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

// --- Raw I/O helpers, shared by normal operation and the diagnostics report ----------------

bool LDC1314Component::read_channel_raw_(uint8_t channel, uint16_t *raw) { return this->read_byte_16(data_register(channel), raw); }

bool LDC1314Component::read_status_raw_(uint16_t *status) { return this->read_byte_16(REG_STATUS, status); }

}  // namespace ldc1314
}  // namespace esphome
