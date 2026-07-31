// Datasheet-conformance check, run on demand from the `button:` platform.
//
// This is the component's diagnostic half: it measures what the sensors are actually doing under
// the configuration currently in force, states the datasheet rule that bears on each measurement,
// and says whether the rule is met. It deliberately reports facts rather than conclusions -- it
// never claims a root cause, and where an input is genuinely unavailable it reports NOT EVALUATED
// instead of guessing a value to fill the column.
//
// It is a snapshot: the codes it reads describe wherever the target happens to be at the moment
// the button is pressed. That is exact for the configuration checks (which depend on the sensor
// frequency, not the target position) but means the amplitude-error line reflects one instant.
//
// Two of the three checks cancel fCLK out entirely and are therefore exact regardless of
// oscillator tolerance or reference-clock source:
//
//   DATA/4096 is by definition fIN/fREF, so the "fIN < fREF/4" clock constraint is exactly
//   "DATA < 1024" and needs no frequency at all.
//
//   Q_max = SETTLECOUNT * 16 * fSENSOR/fREF = SETTLECOUNT * 16 * ratio * FIN_DIVIDER, so the
//   settling constraint is likewise frequency-free.
//
// Only the DEGLITCH check needs an absolute frequency, and hence an assumption about fCLK.

#include "ldc1314.h"

#include "esphome/core/log.h"

#include <cstdio>
#include <string>

namespace esphome {
namespace ldc1314 {

static const char *const TAG = "ldc1314";

// Datasheet §4: "internal oscillator typical frequency ~43 MHz". Nominal only -- it carries a
// temperature coefficient and part-to-part spread, which is why the report labels every frequency
// derived from it as nominal, and why the ratio-based checks are preferred where a choice exists.
static const double INTERNAL_OSCILLATOR_HZ = 43.4e6;

// §8.1.6, multi-channel: valid fINx is < fREFx/4. DATA is the fIN/fREF ratio scaled by 2^12, so
// the constraint is exactly this code ceiling.
static const uint16_t DATA_CLOCK_LIMIT = 1024;

static const char *const VERDICT_MET = "MET";
static const char *const VERDICT_NOT_MET = "NOT MET";
static const char *const VERDICT_NOT_EVALUATED = "NOT EVALUATED";

double LDC1314Component::deglitch_frequency_hz_(Deglitch deglitch) {
  switch (deglitch) {
    case LDC1314_DEGLITCH_1MHZ:
      return 1.0e6;
    case LDC1314_DEGLITCH_3_3MHZ:
      return 3.3e6;
    case LDC1314_DEGLITCH_10MHZ:
      return 10.0e6;
    case LDC1314_DEGLITCH_33MHZ:
      return 33.0e6;
  }
  return 0;
}

// §8.1.7: "the lowest setting exceeding the highest active sensor frequency". Returns the YAML
// spelling so the suggestion can be pasted straight into a config.
static const char *lowest_deglitch_above(double fsensor_hz) {
  if (fsensor_hz < 1.0e6)
    return "1MHZ";
  if (fsensor_hz < 3.3e6)
    return "3_3MHZ";
  if (fsensor_hz < 10.0e6)
    return "10MHZ";
  return "33MHZ";
}

bool LDC1314Component::run_preflight_(PreflightResult *out) {
  *out = PreflightResult{};

  // An external CLKIN's frequency is not discoverable from the device, and there is no YAML option
  // carrying it -- so absolute frequencies stay unevaluated rather than being invented.
  out->fclk_known = this->reference_clock_source_ == LDC1314_REFERENCE_CLOCK_INTERNAL;
  out->fclk_hz = out->fclk_known ? INTERNAL_OSCILLATOR_HZ : 0.0;
  out->deglitch_hz = deglitch_frequency_hz_(this->deglitch_);
  out->amplitude_clean = true;

  bool any = false;
  bool first_q = true;

  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    if (!this->channels_[ch].active())
      continue;
    uint16_t raw = 0;
    if (!this->read_channel_raw_(ch, &raw))
      continue;

    const Channel &conf = this->channels_[ch];
    PreflightChannel &pc = out->ch[ch];
    pc.active = true;
    pc.code = raw & DATA_RESULT_MASK;
    pc.amplitude_error = (raw & DATA_ERR_AE) != 0;
    pc.ratio = static_cast<double>(pc.code) / 4096.0;
    any = true;

    if (pc.amplitude_error)
      out->amplitude_clean = false;
    if (pc.code > out->max_code)
      out->max_code = pc.code;

    uint8_t fin = conf.fin_divider == 0 ? 1 : conf.fin_divider;
    // §7.6.14: SETTLECOUNT 0x0000 and 0x0001 both give tS = 32/fREF, i.e. an effective count of 2.
    double settle = conf.settlecount <= 1 ? 2.0 : static_cast<double>(conf.settlecount);
    pc.q_max = settle * 16.0 * pc.ratio * static_cast<double>(fin);

    if (out->fclk_known) {
      uint16_t fref_div = conf.fref_divider == 0 ? 1 : conf.fref_divider;
      pc.fref_hz = out->fclk_hz / static_cast<double>(fref_div);
      pc.fsensor_hz = pc.ratio * pc.fref_hz * static_cast<double>(fin);
      if (pc.fsensor_hz > out->max_fsensor_hz)
        out->max_fsensor_hz = pc.fsensor_hz;
    }

    if (first_q || pc.q_max < out->min_q_max) {
      out->min_q_max = pc.q_max;
      out->q_binding_channel = ch;
      first_q = false;
    }
  }

  if (!any)
    return false;

  out->valid = true;
  out->data_limit_ok = out->max_code < DATA_CLOCK_LIMIT;
  out->deglitch_ok = out->fclk_known && out->deglitch_hz > out->max_fsensor_hz;
  return true;
}

std::string LDC1314Component::preflight_suggestion_(const PreflightResult &result) const {
  char buf[160];

  // Fixed order, most-constraining first. Only checks the driver actually evaluated can produce a
  // suggestion; an unevaluated check yields nothing rather than a guess.
  if (result.fclk_known && !result.deglitch_ok) {
    snprintf(buf, sizeof(buf), "Set  deglitch: %s   -- lowest setting exceeding %.2f MHz.",
             lowest_deglitch_above(result.max_fsensor_hz), result.max_fsensor_hz / 1e6);
    return buf;
  }

  if (!result.data_limit_ok) {
    snprintf(buf, sizeof(buf),
             "Raise fREF (lower fref_divider) until DATA drops below %u -- currently %u.",
             static_cast<unsigned>(DATA_CLOCK_LIMIT), static_cast<unsigned>(result.max_code));
    return buf;
  }

  if (!result.amplitude_clean) {
    snprintf(buf, sizeof(buf),
             "No datasheet violation detected, but an amplitude error is present. Raising "
             "settlecount (supports Q up to %.1f now) is the next untested constraint.",
             result.min_q_max);
    return buf;
  }

  return "No datasheet violation detected in the current configuration.";
}

void LDC1314Component::log_preflight_(const PreflightResult &result) {
  char buf[160];

  ESP_LOGI(TAG, "================================================================");
  ESP_LOGI(TAG, " Pre-flight  (datasheet conformance)");
  ESP_LOGI(TAG, "================================================================");

  ESP_LOGI(TAG, " Observed");
  if (result.fclk_known) {
    ESP_LOGI(TAG, "   fCLK  %.2f MHz  (internal oscillator, nominal)", result.fclk_hz / 1e6);
  } else {
    ESP_LOGI(TAG, "   fCLK  unknown  (reference_clock: external -- fCLKIN is not discoverable)");
  }
  ESP_LOGI(TAG, "              fREF    DATA    ratio     fSENSOR    Q max");
  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    const PreflightChannel &pc = result.ch[ch];
    if (!pc.active)
      continue;
    if (result.fclk_known) {
      snprintf(buf, sizeof(buf), "   CH%u  %6.2f MHz   %5u   %6.4f   %5.3f MHz   %6.1f", ch, pc.fref_hz / 1e6,
               static_cast<unsigned>(pc.code), pc.ratio, pc.fsensor_hz / 1e6, pc.q_max);
    } else {
      snprintf(buf, sizeof(buf), "   CH%u         n/a   %5u   %6.4f         n/a   %6.1f", ch,
               static_cast<unsigned>(pc.code), pc.ratio, pc.q_max);
    }
    ESP_LOGI(TAG, "%s", buf);
  }
  ESP_LOGI(TAG, "   DEGLITCH configured   %.1f MHz", result.deglitch_hz / 1e6);

  std::string amplitude_channels;
  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    if (result.ch[ch].active && result.ch[ch].amplitude_error) {
      snprintf(buf, sizeof(buf), " CH%u", ch);
      amplitude_channels += buf;
    }
  }
  if (amplitude_channels.empty()) {
    ESP_LOGI(TAG, "   amplitude error   none asserted");
  } else {
    ESP_LOGI(TAG, "   amplitude error   currently asserted on%s", amplitude_channels.c_str());
  }

  ESP_LOGI(TAG, " Datasheet");

  ESP_LOGI(TAG, "   8.1.7   DEGLITCH must be the lowest setting exceeding the highest");
  ESP_LOGI(TAG, "           active sensor frequency.");
  if (result.fclk_known) {
    snprintf(buf, sizeof(buf), "%.1f MHz vs %.2f MHz max sensor freq", result.deglitch_hz / 1e6,
             result.max_fsensor_hz / 1e6);
    ESP_LOGI(TAG, "           %-44s %s", buf, result.deglitch_ok ? VERDICT_MET : VERDICT_NOT_MET);
  } else {
    ESP_LOGI(TAG, "           %-44s %s", "needs fCLKIN, which the driver cannot read", VERDICT_NOT_EVALUATED);
  }

  ESP_LOGI(TAG, "   8.1.6   Multi-channel requires fIN < fREF/4, i.e. DATA < 1024.");
  snprintf(buf, sizeof(buf), "max DATA %u, %d codes of margin (%.1f%%)", static_cast<unsigned>(result.max_code),
           static_cast<int>(DATA_CLOCK_LIMIT) - static_cast<int>(result.max_code),
           100.0 * static_cast<double>(result.max_code) / static_cast<double>(DATA_CLOCK_LIMIT));
  ESP_LOGI(TAG, "           %-44s %s", buf, result.data_limit_ok ? VERDICT_MET : VERDICT_NOT_MET);

  ESP_LOGI(TAG, "   8.1.6   SETTLECOUNT >= Q*fREF/(16*fSENSOR).");
  snprintf(buf, sizeof(buf), "supports coil Q up to %.1f (CH%u binding)", result.min_q_max,
           result.q_binding_channel);
  ESP_LOGI(TAG, "           %s", buf);
  ESP_LOGI(TAG, "           %-44s %s", "the coil's actual Q is not measurable here", VERDICT_NOT_EVALUATED);

  ESP_LOGI(TAG, "   7.6.14  Amplitude that has not settled before conversion start");
  ESP_LOGI(TAG, "           generates an amplitude error.");

  ESP_LOGI(TAG, " Suggested next experiment");
  ESP_LOGI(TAG, "   %s", this->preflight_suggestion_(result).c_str());
  ESP_LOGI(TAG, "   Change one variable per run and re-run this report.");

  // The composed register words, decoded. Printed here as well as raw in the CH[n]|/GLOBAL| trace
  // so the conformance figures above can be read against the bits that produced them without
  // scrolling back to boot.
  uint16_t config = this->last_config_;
  ESP_LOGI(TAG, " CONFIG 0x%04X", config);
  ESP_LOGI(TAG, "   RP_OVERRIDE_EN=%u  AUTO_AMP_DIS=%u  SENSOR_ACTIVATE_SEL=%u (%s)",
           (config & CONFIG_RP_OVERRIDE_EN) ? 1 : 0, (config & CONFIG_AUTO_AMP_DIS) ? 1 : 0,
           (config & CONFIG_SENSOR_ACTIVATE_SEL) ? 1 : 0,
           (config & CONFIG_SENSOR_ACTIVATE_SEL) ? "low power" : "full current");
  ESP_LOGI(TAG, "   REF_CLK_SRC=%u (%s)  INTB_DIS=%u  HIGH_CURRENT_DRV=%u  ACTIVE_CHAN=%u",
           (config & CONFIG_REF_CLK_SRC) ? 1 : 0, (config & CONFIG_REF_CLK_SRC) ? "external" : "internal",
           (config & CONFIG_INTB_DIS) ? 1 : 0, (config & CONFIG_HIGH_CURRENT_DRV) ? 1 : 0,
           static_cast<unsigned>((config >> CONFIG_ACTIVE_CHAN_SHIFT) & 0x3));

  uint16_t mux = this->last_mux_config_;
  uint8_t rr = (mux >> MUX_CONFIG_RR_SEQUENCE_SHIFT) & 0x3;
  // §7.6.25: b00 and b11 both mean Ch0,Ch1.
  static const char *const RR_LABELS[4] = {"Ch0-Ch1", "Ch0-Ch2", "Ch0-Ch3", "Ch0-Ch1"};
  ESP_LOGI(TAG, " MUX_CONFIG 0x%04X", mux);
  ESP_LOGI(TAG, "   AUTOSCAN_EN=%u  RR_SEQUENCE=0b%u%u (%s)  DEGLITCH=0b%u%u%u (%.1f MHz)",
           (mux & MUX_CONFIG_AUTOSCAN_EN) ? 1 : 0, (rr >> 1) & 1, rr & 1, RR_LABELS[rr],
           (mux >> 2) & 1, (mux >> 1) & 1, mux & 1, result.deglitch_hz / 1e6);

  ESP_LOGI(TAG, "================================================================");
}

}  // namespace ldc1314
}  // namespace esphome
