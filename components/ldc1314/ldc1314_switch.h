#pragma once

// A switch that enables/disables the raw-trace log line (LDC1314Component::log_trace_()) at
// runtime -- flip it on, capture with `esphome logs`, flip it off. No reflash needed either way,
// and unlike raising `logger:` to VERY_VERBOSE, it doesn't touch any other component's logging.

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

#include "ldc1314.h"

namespace esphome {
namespace ldc1314 {

class LDC1314TraceSwitch : public switch_::Switch, public Component, public Parented<LDC1314Component> {
 public:
  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;
};

}  // namespace ldc1314
}  // namespace esphome
