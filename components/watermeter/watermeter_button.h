#pragma once

// A button that starts a learn pass -- rotate the meter through exactly one known revolution
// over the configured duration, and WatermeterComponent::start_learn_pass() captures per-phase
// mid/amp from whatever it sees. It only forwards to the hub, same as ldc1314's diagnostics
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

  float duration_s_{10.0f};
};

}  // namespace watermeter
}  // namespace esphome
