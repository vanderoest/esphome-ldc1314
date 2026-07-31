#include "ldc1314_number.h"

namespace esphome {
namespace ldc1314 {

void LDC1314Number::setup() {
  // Publish record B's current value on boot -- the number always shows the manual layer,
  // whether or not the override is currently armed (arming is the switch entity's job).
  switch (this->field_) {
    case LDC1314_NUMBER_IDRIVE:
      this->publish_state(this->parent_->manual_idrive(this->channel_));
      break;
    case LDC1314_NUMBER_OFFSET:
      this->publish_state(this->parent_->manual_offset(this->channel_));
      break;
    case LDC1314_NUMBER_OUTPUT_GAIN:
      this->publish_state(this->parent_->manual_output_gain_value());
      break;
  }
}

void LDC1314Number::control(float value) {
  switch (this->field_) {
    case LDC1314_NUMBER_IDRIVE:
      this->parent_->set_manual_idrive(this->channel_, static_cast<uint8_t>(value));
      break;
    case LDC1314_NUMBER_OFFSET:
      this->parent_->set_manual_offset(this->channel_, static_cast<uint16_t>(value));
      break;
    case LDC1314_NUMBER_OUTPUT_GAIN:
      // Only 1/4/8/16 are valid OUTPUT_GAIN settings (datasheet §7.6.26); anything else snaps to
      // 1x -- see LDC1314Component::output_gain_from_value_().
      this->parent_->set_manual_output_gain(static_cast<uint8_t>(value));
      break;
  }
  this->publish_state(value);
}

}  // namespace ldc1314
}  // namespace esphome
