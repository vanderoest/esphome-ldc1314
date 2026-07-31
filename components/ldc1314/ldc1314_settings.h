#pragma once

// Persisted state for the three-layer value resolution model (see .plan "Value resolution
// model"). Two independent NVS records:
//
//  CalibrationRecord ("record A") -- written only by a completed characterization run.
//  OverrideRecord    ("record B") -- always complete, edited any time by the `number` entities,
//                                    applied only while `armed` (the `switch` entity's state).
//
// Both are trivially-copyable PODs so they can go straight into ESPPreferenceObject::save/load
// (esphome/core/preferences.h) with no serialization code. Neither struct is ever partially
// written: each is saved and loaded as one unit, which is what keeps "layer + its arm/coverage
// bit" from ever landing in an inconsistent state after a reboot mid-write.

#include <cstdint>

#include "ldc1314_registers.h"

namespace esphome {
namespace ldc1314 {

static const uint8_t LDC1314_SETTINGS_VERSION = 1;

struct CalibrationRecord {
  uint8_t version{0};
  uint8_t output_gain{0};   // OutputGain enum value (0..3)
  uint8_t channel_mask{0};  // bit i set => channel i was covered by this run
  uint8_t flags{0};         // reserved
  uint8_t idrive[MAX_CHANNELS]{};
  uint16_t offset[MAX_CHANNELS]{};
} __attribute__((packed));

struct OverrideRecord {
  uint8_t version{0};
  uint8_t armed{0};
  uint8_t output_gain{0};  // OutputGain enum value (0..3)
  uint8_t idrive[MAX_CHANNELS]{};
  uint16_t offset[MAX_CHANNELS]{};
} __attribute__((packed));

}  // namespace ldc1314
}  // namespace esphome
