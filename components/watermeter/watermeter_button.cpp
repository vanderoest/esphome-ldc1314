#include "watermeter_button.h"

namespace esphome {
namespace watermeter {

void WatermeterLearnButton::press_action() { this->parent_->start_learn_pass(this->duration_s_); }

}  // namespace watermeter
}  // namespace esphome
