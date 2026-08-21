#pragma once

// A button that starts a learn pass -- let water run continuously over the configured duration,
// and WatermeterComponent::start_learn_pass() captures per-phase mid/amp from whatever it sees.
// Doesn't need to be exactly one revolution (an installed meter can't be hand-turned to order);
// it only needs to cover at least one. It only forwards to the hub, same as ldc1314's diagnostics
// button -- an automation calling start_learn_pass() directly would work identically.

#include "esphome/components/button/button.h"
#include "esphome/core/helpers.h"

#include "watermeter.h"

namespace esphome {
namespace watermeter {

class WatermeterLearnButton : public button::Button, public Parented<WatermeterComponent> {
 public:
  void set_duration(float duration_s) { this->duration_s_ = duration_s; }

 protected:
  void press_action() override;

  float duration_s_{60.0f};
};

}  // namespace watermeter
}  // namespace esphome
