#!/usr/bin/env bash
# Validates every ESPHome config in this repo against the *local* components/ directory,
# so CI checks the code being pushed, not the published GitHub copy.
#
# `esphome config` runs full YAML/schema validation, but stops short of C++ codegen:
# codegen and compile errors are covered by the host tests, real hardware builds, and
# (optionally) an `esphome compile` job if this repo ever grows one.
set -euo pipefail
cd "$(dirname "$0")/../.."   # repo root

CONFIGS=(
  watermeter.yaml
  tests/test_ldc1314.yaml
  tests/test_ldc1314_single_channel.yaml
  tests/watermeter-test.yaml
  tests/watermeter_component_test.yaml
)

rm -rf .ci
mkdir -p .ci

# Placeholder secrets so the !secret references validate. Never real credentials.
cat > .ci/secrets.yaml <<'EOF'
wifi_ssid: "CHANGE_ME_SSID"
wifi_password: "CHANGE_ME_WIFI_PASSWORD"
ota_password: "CHANGE_ME_OTA_PASSWORD"
api_encryption_key: "xtD5YFsmicBXv3swNy7Dwxuwob2vW0HOeWRbzrmGIe0="  # dummy
web_username: "CHANGE_ME_USER"
web_password: "CHANGE_ME_WEB_PASSWORD"
EOF

# Copies of the configs live in .ci/ so the local external_components path is unambiguous
# regardless of whether ESPHome resolves it against the config directory or the cwd,
# and so the production secrets.yaml at the repo root is never touched.
for src in "${CONFIGS[@]}"; do
  dst=".ci/$(basename "$src")"
  sed -E 's#source: github://vanderoest/esphome-ldc1314#source:\n      type: local\n      path: ../components#' "$src" > "$dst"
  echo "=== validating $src (local components) ==="
  esphome config "$dst"
done

rm -rf .ci
echo "All configs validated."
