#include "ldc1314.h"

#include "esphome/core/log.h"
#include "esphome/core/hal.h"

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

static uint8_t output_gain_to_multiplier(OutputGain gain) {
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

  if (!this->configure_()) {
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
  ESP_LOGCONFIG(TAG, "  Output gain: %ux", output_gain_to_multiplier(this->output_gain_));
  ESP_LOGCONFIG(TAG, "  Report errors on INTB: %s", YESNO(this->report_errors_on_intb_));

  for (uint8_t channel = 0; channel < MAX_CHANNELS; channel++) {
    const Channel &ch = this->channels_[channel];
    if (!ch.active())
      continue;
    ESP_LOGCONFIG(TAG, "  Channel %u:", channel);
    LOG_SENSOR("    ", "Raw value", ch.sensor);
    LOG_BINARY_SENSOR("    ", "Error", ch.error_binary_sensor);
    ESP_LOGCONFIG(TAG, "    RCOUNT: 0x%04X, SETTLECOUNT: 0x%04X, OFFSET: 0x%04X", ch.rcount, ch.settlecount,
                  ch.offset);
    ESP_LOGCONFIG(TAG, "    FIN_DIVIDER: %u, FREF_DIVIDER: %u, IDRIVE: %u", ch.fin_divider, ch.fref_divider,
                  ch.idrive);
  }
}

void LDC1314Component::update() {
  if (this->is_failed())
    return;

  // Multi-channel readback race (docs/knowledge_base.md "Measurement flow"): DATAx is
  // overwritten by that channel's next conversion if not read before it completes. This driver
  // sidesteps that by always reading every active channel's DATAx unconditionally every cycle,
  // rather than gating reads on STATUS.UNREADCONVx -- worst case with a fast device / slow
  // update_interval is a redundant read of an unchanged value, not corrupted/skipped data. If
  // update_interval is configured shorter than the device's actual scan+settle time, channels
  // will simply report the same value across a couple of polls until the next real conversion
  // lands -- not a bug, but worth knowing when picking update_interval for real hardware.

  // Read STATUS first: it's read-to-clear and, per docs/knowledge_base.md "Error handling", the
  // driver's read order should be consistent so a second channel's error isn't dropped from
  // attribution by an out-of-order DATAx read. Per-channel error attribution itself comes from
  // each channel's own DATAx error bits (read below), not from STATUS.ERR_CHAN, since ERR_CHAN
  // only latches the *first* erroring channel each cycle.
  this->read_status_();

  for (uint8_t channel = 0; channel < MAX_CHANNELS; channel++) {
    if (this->channels_[channel].active()) {
      this->read_channel_(channel);
    }
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

bool LDC1314Component::configure_() {
  uint8_t active_count = this->active_channel_count_();
  uint8_t highest = this->highest_active_channel_();
  bool autoscan = active_count > 1;

  if (active_count == 0) {
    ESP_LOGW(TAG, "No channels configured -- nothing will be measured");
  } else if (autoscan) {
    // The device only supports scanning a contiguous run of channels starting at 0
    // (MUX_CONFIG.RR_SEQUENCE, register_map.md). Warn if the configured channels have a gap, since
    // the skipped channel(s) will still be measured (and their settling/conversion time spent) by
    // the hardware even though nothing is published for them.
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

  // ERROR_CONFIG: always embed error flags in each channel's own DATAx register (this is how
  // per-channel diagnostics and error-aware publishing work without wiring INTB -- see
  // design_decisions.md "PollingComponent, not interrupt-driven"). Additionally route errors to
  // INTB/STATUS-driven interrupts only if the user opted in.
  uint16_t error_config = ERROR_CONFIG_UR_ERR2OUT | ERROR_CONFIG_OR_ERR2OUT | ERROR_CONFIG_WD_ERR2OUT |
                           ERROR_CONFIG_AH_ERR2OUT | ERROR_CONFIG_AL_ERR2OUT;
  if (this->report_errors_on_intb_) {
    error_config |= ERROR_CONFIG_UR_ERR2INT | ERROR_CONFIG_OR_ERR2INT | ERROR_CONFIG_WD_ERR2INT |
                     ERROR_CONFIG_AH_ERR2INT | ERROR_CONFIG_AL_ERR2INT | ERROR_CONFIG_ZC_ERR2INT |
                     ERROR_CONFIG_DRDY_2INT;
  }
  if (!this->write_byte_16(REG_ERROR_CONFIG, error_config)) {
    ESP_LOGE(TAG, "Failed to write ERROR_CONFIG");
    return false;
  }

  // RESET_DEV: only the OUTPUT_GAIN field is meaningful to write here (the RESET bit always
  // reads/writes as a one-shot action, already handled in reset_()).
  uint16_t reset_dev = (static_cast<uint16_t>(this->output_gain_) & RESET_DEV_OUTPUT_GAIN_MASK)
                        << RESET_DEV_OUTPUT_GAIN_SHIFT;
  if (!this->write_byte_16(REG_RESET_DEV, reset_dev)) {
    ESP_LOGE(TAG, "Failed to write RESET_DEV (output gain)");
    return false;
  }

  for (uint8_t channel = 0; channel < MAX_CHANNELS; channel++) {
    if (!this->channels_[channel].active())
      continue;
    if (!this->write_channel_config_(channel)) {
      ESP_LOGE(TAG, "Failed to write channel %u configuration", channel);
      return false;
    }
  }

  // CONFIG is written last: it both applies the remaining global bits and, by clearing
  // SLEEP_MODE_EN, exits Sleep Mode and starts conversions. Registers cannot be changed once
  // conversions have started -- see docs/knowledge_base.md "Initialization sequence".
  uint16_t config = CONFIG_RESERVED_BITS;
  config |= CONFIG_RP_OVERRIDE_EN;  // fixed drive current, no auto-amplitude drift during operation
  config |= CONFIG_AUTO_AMP_DIS;    // ditto -- see docs/summaries/drive_configuration_summary.md
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
  // SLEEP_MODE_EN is left clear -- this is what exits Sleep Mode and starts conversions.

  if (!this->write_byte_16(REG_CONFIG, config)) {
    ESP_LOGE(TAG, "Failed to write CONFIG");
    return false;
  }

  return true;
}

bool LDC1314Component::write_channel_config_(uint8_t channel) {
  const Channel &ch = this->channels_[channel];

  if (!this->write_byte_16(rcount_register(channel), ch.rcount))
    return false;
  if (!this->write_byte_16(settlecount_register(channel), ch.settlecount))
    return false;
  if (!this->write_byte_16(offset_register(channel), ch.offset))
    return false;

  uint16_t clock_dividers = (static_cast<uint16_t>(ch.fin_divider) << CLOCK_DIVIDERS_FIN_DIVIDER_SHIFT) |
                             (ch.fref_divider & CLOCK_DIVIDERS_FREF_DIVIDER_MASK);
  if (!this->write_byte_16(clock_dividers_register(channel), clock_dividers))
    return false;

  uint16_t drive_current = static_cast<uint16_t>(ch.idrive) << DRIVE_CURRENT_IDRIVE_SHIFT;
  if (!this->write_byte_16(drive_current_register(channel), drive_current))
    return false;

  return true;
}

void LDC1314Component::read_channel_(uint8_t channel) {
  Channel &ch = this->channels_[channel];
  uint16_t raw = 0;
  if (!this->read_byte_16(data_register(channel), &raw)) {
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
  if (!this->read_byte_16(REG_STATUS, &status)) {
    ESP_LOGW(TAG, "Failed to read STATUS");
    this->status_set_warning();
    return;
  }

  static const uint16_t ERROR_BITS =
      STATUS_ERR_UR | STATUS_ERR_OR | STATUS_ERR_WD | STATUS_ERR_AHE | STATUS_ERR_ALE | STATUS_ERR_ZC;
  if (status & ERROR_BITS) {
    uint8_t err_chan = (status >> STATUS_ERR_CHAN_SHIFT) & STATUS_ERR_CHAN_MASK;
    ESP_LOGD(TAG,
             "STATUS reports an error, first attributed to channel %u (STATUS=0x%04X) -- see "
             "per-channel error flags for full multi-channel attribution",
             err_chan, status);
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

}  // namespace ldc1314
}  // namespace esphome
