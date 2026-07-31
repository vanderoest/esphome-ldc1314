#pragma once

// A button that runs the datasheet-conformance report on demand. It only forwards to
// LDC1314Component::run_diagnostics(), which is trigger-agnostic -- an automation, or a GPIO
// binary_sensor calling the same method, would work identically.

#include "esphome/components/button/button.h"
#include "esphome/core/helpers.h"

#include "ldc1314.h"

namespace esphome {
namespace ldc1314 {

class LDC1314DiagnosticsButton : public button::Button, public Parented<LDC1314Component> {
 protected:
  void press_action() override;
};

}  // namespace ldc1314
}  // namespace esphome
