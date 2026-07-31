#pragma once

// One class for all three buttons this hub can expose -- characterize / clear calibration /
// clear overrides -- distinguished by `action_` rather than three near-identical C++ classes.
// Each just forwards to the same trigger-agnostic entry point a physical button or an action
// would also call -- see .plan "The engine is independent of its trigger".

#include "esphome/components/button/button.h"
#include "esphome/core/helpers.h"

#include "ldc1314.h"

namespace esphome {
namespace ldc1314 {

enum LDC1314ButtonAction : uint8_t {
  LDC1314_BUTTON_CHARACTERIZE = 0,
  LDC1314_BUTTON_CLEAR_CHARACTERIZATION,
  LDC1314_BUTTON_CLEAR_OVERRIDES,
};

class LDC1314Button : public button::Button, public Parented<LDC1314Component> {
 public:
  void set_action(LDC1314ButtonAction action) { this->action_ = action; }

 protected:
  void press_action() override;

  LDC1314ButtonAction action_{LDC1314_BUTTON_CHARACTERIZE};
};

}  // namespace ldc1314
}  // namespace esphome
