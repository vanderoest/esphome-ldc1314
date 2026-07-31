#pragma once

// Actions/condition for the characterization framework's trigger-agnostic control surface (see
// .plan "The engine is independent of its trigger"). Every one of these is a thin wrapper around
// a public LDC1314Component method -- an ESPHome button, a physical GPIO button's automation, and
// a script all end up calling exactly the same code.

#include "esphome/core/automation.h"
#include "ldc1314.h"

namespace esphome {
namespace ldc1314 {

#define LDC1314_SIMPLE_ACTION(ACTION_CLASS, ACTION_METHOD)                                    \
  template<typename... Ts> class ACTION_CLASS : public Action<Ts...>, public Parented<LDC1314Component> { \
    void play(const Ts &...x) override { this->parent_->ACTION_METHOD(); }                    \
  };

LDC1314_SIMPLE_ACTION(CharacterizeAction, start_characterization)
LDC1314_SIMPLE_ACTION(ClearCharacterizationAction, clear_characterization)
LDC1314_SIMPLE_ACTION(ClearOverridesAction, clear_overrides)

template<typename... Ts> class SetOverrideAction : public Action<Ts...>, public Parented<LDC1314Component> {
 public:
  TEMPLATABLE_VALUE(bool, armed)

  void play(const Ts &...x) override { this->parent_->set_override_armed(this->armed_.value(x...)); }
};

template<typename... Ts>
class IsCharacterizingCondition : public Condition<Ts...>, public Parented<LDC1314Component> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_characterizing(); }
};

}  // namespace ldc1314
}  // namespace esphome
