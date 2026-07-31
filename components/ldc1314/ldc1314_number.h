#pragma once

// The manual override layer (record B, .plan "Value resolution model") exposed as `number`
// entities. One class handles all three kinds of number this hub can expose -- per-channel
// IDRIVE/OFFSET and the hub-level OUTPUT_GAIN -- distinguished by `field_` rather than by three
// near-identical C++ classes.

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "ldc1314.h"

namespace esphome {
namespace ldc1314 {

enum LDC1314NumberField : uint8_t {
  LDC1314_NUMBER_IDRIVE = 0,
  LDC1314_NUMBER_OFFSET,
  LDC1314_NUMBER_OUTPUT_GAIN,
};

class LDC1314Number : public number::Number, public Parented<LDC1314Component>, public Component {
 public:
  void set_field(LDC1314NumberField field) { this->field_ = field; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }

  void setup() override;

 protected:
  void control(float value) override;

  LDC1314NumberField field_{LDC1314_NUMBER_IDRIVE};
  uint8_t channel_{0};
};

}  // namespace ldc1314
}  // namespace esphome
