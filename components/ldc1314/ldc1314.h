#pragma once

#include <string>

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

// Datasheet-conformance diagnostic, run on demand from the `button:` platform.
//
// It reports facts, never conclusions: measured values, the datasheet rule bearing on each, and
// whether that rule is met. It never claims a root cause, and where an input is genuinely
// unavailable it reports NOT EVALUATED rather than guessing (coil Q is not measurable from
// registers; fCLKIN is unknown on an external reference clock).
struct PreflightChannel {
  bool active{false};
  uint16_t code{0};                 // DATAx & DATA_RESULT_MASK
  bool amplitude_error{false};      // DATAx & DATA_ERR_AE
  double ratio{0};                  // code / 4096 -- exact, independent of oscillator accuracy
  double fref_hz{0};                // fCLK / FREF_DIVIDER (0 when fCLK is unknown)
  double fsensor_hz{0};             // ratio * fREF * FIN_DIVIDER (0 when fCLK is unknown)
  double q_max{0};                  // highest coil Q this channel's SETTLECOUNT supports
};

struct PreflightResult {
  bool valid{false};
  // False on an external reference clock: the driver has no way to know fCLKIN, so every
  // absolute-frequency figure (and with it the DEGLITCH check) is left unevaluated. The ratio and
  // Q checks stay exact regardless -- both cancel fCLK out.
  bool fclk_known{false};
  double fclk_hz{0};
  double deglitch_hz{0};
  uint16_t max_code{0};
  double max_fsensor_hz{0};
  double min_q_max{0};
  uint8_t q_binding_channel{0};
  bool deglitch_ok{false};      // §8.1.7: DEGLITCH exceeds the highest active sensor frequency
  bool data_limit_ok{false};    // §8.1.6: fIN < fREF/4, i.e. DATA < 1024
  bool amplitude_clean{false};  // no DATAx.ERR_AE asserted on any active channel
  PreflightChannel ch[MAX_CHANNELS];
};

/// Driver for the Texas Instruments LDC1314 4-channel inductance-to-digital converter.
///
/// This component only talks to the device and publishes raw per-channel conversion codes; it
/// deliberately does not compute distance, flow, or any other physical/application-specific
/// quantity -- see design_decisions.md ("Publish raw values, not liters/flow").
///
/// Configuration comes from YAML and nothing else: the registers written are exactly what the
/// config says, with no stored calibration layer in between.
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

  /// Measure the active channels and log the datasheet-conformance report. Trigger-agnostic: the
  /// `button:` entity calls this, and so would any automation or physical pushbutton wired to it.
  void run_diagnostics();

 protected:
  struct Channel {
    sensor::Sensor *sensor{nullptr};
    binary_sensor::BinarySensor *error_binary_sensor{nullptr};

    uint16_t rcount{0x0080};
    uint16_t settlecount{0x0000};
    uint8_t fin_divider{1};
    uint16_t fref_divider{1};
    uint16_t offset{0x0000};
    uint8_t idrive{0};

    bool active() const { return this->sensor != nullptr || this->error_binary_sensor != nullptr; }
  };

  // --- identity / reset / configuration ---
  bool verify_identity_();
  bool reset_();
  bool enter_sleep_();
  bool exit_sleep_(uint16_t config);
  // Writes the full register set (sleep -> globals -> per-channel -> wake). The device requires
  // CONFIG.SLEEP_MODE_EN=1 before any register may change, which is why this is a single call
  // rather than a set of independent writes.
  bool apply_config_();
  uint16_t compose_config_(bool sleep) const;
  bool write_channel_config_(uint8_t channel, uint8_t idrive, uint16_t offset);
  void read_channel_(uint8_t channel);
  void read_status_();
  uint8_t active_channel_count_() const;
  uint8_t highest_active_channel_() const;
  static uint8_t output_gain_to_value_(OutputGain gain);

  // --- raw I/O helpers ---
  bool read_channel_raw_(uint8_t channel, uint16_t *raw);
  bool read_status_raw_(uint16_t *status);

  // --- datasheet conformance diagnostic, implemented in ldc1314_preflight.cpp ---
  //
  // Kept in its own translation unit: it has no state, it is pure measure-and-render, and it is
  // the one part of the driver that reasons about the configuration rather than applying it.
  bool run_preflight_(PreflightResult *out);
  void log_preflight_(const PreflightResult &result);
  // The single line printed under "Suggested next experiment", derived mechanically from the
  // failed checks in a fixed order.
  std::string preflight_suggestion_(const PreflightResult &result) const;
  static double deglitch_frequency_hz_(Deglitch deglitch);

  Channel channels_[MAX_CHANNELS];

  ReferenceClockSource reference_clock_source_{LDC1314_REFERENCE_CLOCK_INTERNAL};
  Deglitch deglitch_{LDC1314_DEGLITCH_10MHZ};
  OutputGain output_gain_{LDC1314_OUTPUT_GAIN_1};
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
};

}  // namespace ldc1314
}  // namespace esphome
