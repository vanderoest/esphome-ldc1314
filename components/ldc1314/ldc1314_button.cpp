#include "ldc1314_button.h"

namespace esphome {
namespace ldc1314 {

void LDC1314Button::press_action() {
  switch (this->action_) {
    case LDC1314_BUTTON_CHARACTERIZE:
      this->parent_->start_characterization();
      break;
    case LDC1314_BUTTON_CLEAR_CHARACTERIZATION:
      this->parent_->clear_characterization();
      break;
    case LDC1314_BUTTON_CLEAR_OVERRIDES:
      this->parent_->clear_overrides();
      break;
  }
}

}  // namespace ldc1314
}  // namespace esphome
