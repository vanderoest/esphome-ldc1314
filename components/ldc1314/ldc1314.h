#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include "ldc1314_registers.h"

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

/// Driver for the Texas Instruments LDC1314 4-channel inductance-to-digital converter.
///
/// This component only talks to the device and publishes raw per-channel conversion codes; it
/// deliberately does not compute distance, flow, or any other physical/application-specific
/// quantity -- see design_decisions.md ("Publish raw values, not liters/flow") and .plan §1.
class LDC1314Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  // Global (hub-level) configuration -- set from __init__.py before setup() runs.
  void set_reference_clock_source(ReferenceClockSource source) { this->reference_clock_source_ = source; }
  void set_deglitch(Deglitch deglitch) { this->deglitch_ = deglitch; }
  void set_output_gain(OutputGain gain) { this->output_gain_ = gain; }
  void set_high_current_drive(bool enabled) { this->high_current_drive_ = enabled; }
  void set_sensor_activation_mode(SensorActivationMode mode) { this->sensor_activation_mode_ = mode; }
  void set_report_errors_on_intb(bool enabled) { this->report_errors_on_intb_ = enabled; }

  // Per-channel configuration and entity registration -- set from sensor.py/binary_sensor.py
  // before setup() runs. `channel` is 0-based and must be < MAX_CHANNELS.
  void set_channel_sensor(uint8_t channel, sensor::Sensor *sensor) { this->channels_[channel].sensor = sensor; }
  void set_channel_error_binary_sensor(uint8_t channel, binary_sensor::BinarySensor *binary_sensor) {
    this->channels_[channel].error_binary_sensor = binary_sensor;
  }
  void set_channel_rcount(uint8_t channel, uint16_t rcount) { this->channels_[channel].rcount = rcount; }
  void set_channel_settlecount(uint8_t channel, uint16_t settlecount) {
    this->channels_[channel].settlecount = settlecount;
  }
  void set_channel_offset(uint8_t channel, uint16_t offset) { this->channels_[channel].offset = offset; }
  void set_channel_fin_divider(uint8_t channel, uint8_t fin_divider) {
    this->channels_[channel].fin_divider = fin_divider;
  }
  void set_channel_fref_divider(uint8_t channel, uint16_t fref_divider) {
    this->channels_[channel].fref_divider = fref_divider;
  }
  void set_channel_idrive(uint8_t channel, uint8_t idrive) { this->channels_[channel].idrive = idrive; }

 protected:
  struct Channel {
    sensor::Sensor *sensor{nullptr};
    binary_sensor::BinarySensor *error_binary_sensor{nullptr};
    uint16_t rcount{0x0080};
    uint16_t settlecount{0x0000};
    uint16_t offset{0x0000};
    uint8_t fin_divider{1};
    uint16_t fref_divider{1};
    uint8_t idrive{0};

    bool active() const { return this->sensor != nullptr || this->error_binary_sensor != nullptr; }
  };

  bool verify_identity_();
  bool reset_();
  bool configure_();
  bool write_channel_config_(uint8_t channel);
  void read_channel_(uint8_t channel);
  void read_status_();
  uint8_t active_channel_count_() const;
  uint8_t highest_active_channel_() const;

  Channel channels_[MAX_CHANNELS];

  ReferenceClockSource reference_clock_source_{LDC1314_REFERENCE_CLOCK_INTERNAL};
  Deglitch deglitch_{LDC1314_DEGLITCH_33MHZ};
  OutputGain output_gain_{LDC1314_OUTPUT_GAIN_1};
  bool high_current_drive_{false};
  SensorActivationMode sensor_activation_mode_{LDC1314_SENSOR_ACTIVATE_FULL_CURRENT};
  bool report_errors_on_intb_{false};
};

}  // namespace ldc1314
}  // namespace esphome
