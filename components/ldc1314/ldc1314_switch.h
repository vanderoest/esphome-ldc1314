#pragma once

// The single entity that arms/disarms the manual override layer (record B). Disarming is the
// exact, non-destructive "revert to calibrated" -- see .plan "Value resolution model".

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "ldc1314.h"

namespace esphome {
namespace ldc1314 {

class LDC1314Switch : public switch_::Switch, public Parented<LDC1314Component>, public Component {
 public:
  void setup() override;

 protected:
  void write_state(bool state) override;
};

}  // namespace ldc1314
}  // namespace esphome
