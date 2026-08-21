#include "ldc1314_switch.h"

namespace esphome {
namespace ldc1314 {

static const char *const TAG = "ldc1314.switch";

void LDC1314TraceSwitch::setup() {
  // ALWAYS_OFF restore mode (switch.py) makes get_initial_state_with_restore_mode() moot here,
  // but going through write_state() rather than assuming false keeps this correct if that default
  // ever changes.
  this->write_state(this->get_initial_state_with_restore_mode().value_or(false));
}

void LDC1314TraceSwitch::dump_config() { LOG_SWITCH("", "LDC1314 Trace Capture", this); }

void LDC1314TraceSwitch::write_state(bool state) {
  this->parent_->set_trace_enabled(state);
  this->publish_state(state);
}

}  // namespace ldc1314
}  // namespace esphome
