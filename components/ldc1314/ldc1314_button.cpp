#include "ldc1314_button.h"

namespace esphome {
namespace ldc1314 {

void LDC1314DiagnosticsButton::press_action() { this->parent_->run_diagnostics(); }

}  // namespace ldc1314
}  // namespace esphome
