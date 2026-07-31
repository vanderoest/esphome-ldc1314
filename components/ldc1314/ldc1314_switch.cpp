#include "ldc1314_switch.h"

namespace esphome {
namespace ldc1314 {

void LDC1314Switch::setup() { this->publish_state(this->parent_->override_armed()); }

void LDC1314Switch::write_state(bool state) {
  this->parent_->set_override_armed(state);
  this->publish_state(state);
}

}  // namespace ldc1314
}  // namespace esphome
