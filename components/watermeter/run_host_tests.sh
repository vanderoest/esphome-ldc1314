#!/usr/bin/env bash
# Compiles and runs the RotationDecoder host test harness -- .plan "Validation" step 3.
# No ESPHome, no board: this is what makes the decoder testable before it ever touches hardware.
set -euo pipefail
cd "$(dirname "$0")"

CXX="${CXX:-c++}"
"$CXX" -std=c++17 -O2 -Wall -Wextra -o /tmp/watermeter_host_tests rotation_decoder.cpp test_rotation_decoder.cpp

# Default capture directory is the repo's captures/, relative to this script's location.
/tmp/watermeter_host_tests "${1:-../../captures}"
