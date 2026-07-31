// The six-stage characterization state machine -- see .plan "The staged procedure". Kept in its
// own translation unit since it's a self-contained, non-blocking engine bolted onto the hub: it
// only reaches into LDC1314Component for raw register I/O and the settings layers, never for
// anything application-specific.
//
// Non-blocking rule: every function here either returns almost immediately (a single sample, a
// register write, a log line) or is pure arithmetic (char_stage_derive_). Nothing in this file may
// call delay() or otherwise block loop() -- see .plan "Sampling cadence".

#include "ldc1314.h"

#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include <cmath>
#include <cstdio>

namespace esphome {
namespace ldc1314 {

static const char *const TAG = "ldc1314";

// A real "target held still" idle baseline should show only device noise, a handful of codes at
// most. Anything wider than this during Stage 0 means the user started moving the target before
// being prompted to -- worth aborting on rather than silently folding into later measurements.
static const uint16_t IDLE_MOTION_THRESHOLD = 8;

static const uint8_t MAX_VERIFY_RETRIES = 2;

// How long the auto-amplitude loop must sit pinned at an endpoint, with the matching amplitude
// error asserted, before Stage 1 gives up instead of running out its full stage_duration.
//
// Not a YAML option on purpose. The condition has to hold *continuously* -- any sample that breaks
// it resets the timer -- so the loop is always granted the full window to move from wherever it
// is, and there is nothing a user could usefully tune. TI's auto-calibration traverses its 32
// codes in far less than this, so a stall this long is a genuine dead end rather than slow
// settling.
static const uint32_t PINNED_DRIVE_ABORT_MS = 8000;

bool LDC1314Component::char_write_registers_(OutputGain gain, const uint8_t *idrive, const uint16_t *offset,
                                              bool rp_override, bool auto_amp_dis) {
  if (!this->enter_sleep_())
    return false;

  uint16_t reset_dev = (static_cast<uint16_t>(gain) & RESET_DEV_OUTPUT_GAIN_MASK) << RESET_DEV_OUTPUT_GAIN_SHIFT;
  if (!this->write_byte_16(REG_RESET_DEV, reset_dev))
    return false;

  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    if (!this->channels_[ch].active())
      continue;
    if (!this->write_channel_config_(ch, idrive[ch], offset[ch]))
      return false;
  }

  uint16_t config = this->compose_config_(false, rp_override, auto_amp_dis);
  if (!this->exit_sleep_(config))
    return false;

  this->last_reset_dev_ = reset_dev;
  this->last_config_ = config;
  return true;
}

void LDC1314Component::char_enter_stage_(CharacterizationStage stage) {
  uint32_t now = millis();
  this->char_stage_ = stage;
  this->char_stage_start_ms_ = now;
  this->char_last_sample_ms_ = 0;  // force an immediate first sample in the new stage
  this->char_last_progress_ms_ = now;

  uint8_t idrive[MAX_CHANNELS] = {};
  uint16_t zero_offset[MAX_CHANNELS] = {};

  switch (stage) {
    case CHAR_STAGE_PREPARE: {
      ESP_LOGI(TAG, "Stage 0/5  Preparing...");
      if (this->override_.armed) {
        ESP_LOGW(TAG, "Manual override is armed -- its values are ignored during this run");
      }

      // Snapshot the effective configuration in force right now, so any abort can restore it
      // exactly -- see .plan "Abort path".
      for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
        this->char_snapshot_idrive_[ch] = this->effective_idrive_(ch);
        this->char_snapshot_offset_[ch] = this->effective_offset_(ch);
      }
      this->char_snapshot_output_gain_ = this->effective_output_gain_();

      for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
        this->char_accum_[ch] = ChannelAccum{};
      }
      this->char_status_or_ = 0;
      this->char_idle_samples_ = 0;
      this->char_drive_samples_ = 0;
      this->char_envelope_samples_ = 0;
      this->char_verify_samples_ = 0;
      this->char_i2c_failures_ = 0;
      this->char_verify_retries_ = 0;
      this->char_verify_ur_ = 0;
      this->char_verify_or_ = 0;
      this->char_pinned_since_ms_ = 0;
      this->char_pinned_high_ = false;
      this->char_fail_amplitude_ = false;
      this->preflight_ = PreflightResult{};

      // Unclipped, unshifted baseline: gain 1, offset 0, fixed drive at whatever was already
      // effective (the value doesn't matter yet -- only gain/offset matter for the idle read).
      if (!this->char_write_registers_(LDC1314_OUTPUT_GAIN_1, this->char_snapshot_idrive_, zero_offset, true, true)) {
        this->char_abort_("I2C write failed while preparing");
        return;
      }
      ESP_LOGI(TAG, "  Keep the target still.");
      break;
    }

    case CHAR_STAGE_AUTO_IDRIVE: {
      ESP_LOGI(TAG, "Stage 1/5  Auto drive...");
      ESP_LOGI(TAG, ">>> %s", this->char_prompt_start_.c_str());
      // rp_override=false, auto_amp_dis=false: TI's auto-amplitude mode. IDRIVEx is ignored by
      // the device while RP_OVERRIDE_EN=0, so the `idrive` argument here is a don't-care.
      if (!this->char_write_registers_(LDC1314_OUTPUT_GAIN_1, idrive, zero_offset, false, false)) {
        this->char_abort_("I2C write failed entering auto-amplitude mode");
        return;
      }
      break;
    }

    case CHAR_STAGE_ENVELOPE: {
      ESP_LOGI(TAG, "Stage 2/5  Measuring envelope...");
      for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++)
        idrive[ch] = this->char_result_idrive_;
      if (!this->char_write_registers_(LDC1314_OUTPUT_GAIN_1, idrive, zero_offset, true, true)) {
        this->char_abort_("I2C write failed fixing drive current");
        return;
      }
      break;
    }

    case CHAR_STAGE_DERIVE:
      ESP_LOGI(TAG, "Stage 3/5  Deriving parameters...");
      // Pure arithmetic, no I/O and no wait -- transitions onward (or aborts) itself before
      // returning, so CHAR_STAGE_DERIVE is never the persisted stage between loop() calls.
      this->char_stage_derive_();
      return;

    case CHAR_STAGE_VERIFY: {
      ESP_LOGI(TAG, "Stage 4/5  Verification...");
      this->char_verify_samples_ = 0;
      this->char_verify_ur_ = 0;
      this->char_verify_or_ = 0;
      for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
        this->char_accum_[ch].verify_min = 0x0FFF;
        this->char_accum_[ch].verify_max = 0;
        idrive[ch] = this->char_result_idrive_;
      }
      if (!this->char_write_registers_(this->char_result_output_gain_, idrive, this->char_result_offset_, true,
                                        true)) {
        this->char_abort_("I2C write failed applying the final configuration");
        return;
      }
      break;
    }

    case CHAR_STAGE_COMMIT:
      ESP_LOGI(TAG, "Stage 5/5  Storing...");
      // Also self-contained: writes the calibration record, publishes diagnostics, emits the
      // report, restores normal operation, and returns to IDLE before returning.
      this->char_stage_commit_();
      return;

    default:
      break;
  }
}

void LDC1314Component::char_abort_(const std::string &reason) {
  ESP_LOGE(TAG, "Characterization aborted: %s", reason.c_str());
  // Restore exactly what was effective before the run started. Both NVS records are left
  // untouched -- see .plan "Abort path". Production drive mode is always restored, even if the
  // snapshot happened to be taken mid-auto-amplitude (it can't be: PREPARE always establishes a
  // fixed-drive baseline first).
  this->char_write_registers_(this->char_snapshot_output_gain_, this->char_snapshot_idrive_,
                               this->char_snapshot_offset_, true, true);
  this->char_report_(false, reason);
  this->char_stage_ = CHAR_STAGE_IDLE;
}

void LDC1314Component::char_progress_() {
  const char *label = nullptr;
  uint32_t total_ms = 0;
  switch (this->char_stage_) {
    case CHAR_STAGE_AUTO_IDRIVE:
      label = "Auto drive";
      total_ms = this->char_stage_duration_ms_;
      break;
    case CHAR_STAGE_ENVELOPE:
      label = "Measuring envelope";
      total_ms = this->char_stage_duration_ms_;
      break;
    case CHAR_STAGE_VERIFY:
      label = "Verification";
      total_ms = this->char_verify_duration_ms_;
      break;
    default:
      return;
  }

  uint32_t elapsed_s = (millis() - this->char_stage_start_ms_) / 1000;
  ESP_LOGI(TAG, "Stage %s...  [%3us / %3us]", label, static_cast<unsigned>(elapsed_s),
           static_cast<unsigned>(total_ms / 1000));

  std::string line = "  ";
  char buf[48];
  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    if (!this->channels_[ch].active())
      continue;
    const ChannelAccum &acc = this->char_accum_[ch];
    if (this->char_stage_ == CHAR_STAGE_AUTO_IDRIVE) {
      snprintf(buf, sizeof(buf), "CH%u INIT_IDRIVE %u..%u   ", ch, acc.idrive_min, acc.idrive_max);
    } else if (this->char_stage_ == CHAR_STAGE_ENVELOPE) {
      snprintf(buf, sizeof(buf), "CH%u %u..%u   ", ch, acc.code_min, acc.code_max);
    } else {
      snprintf(buf, sizeof(buf), "CH%u %u..%u   ", ch, acc.verify_min, acc.verify_max);
    }
    line += buf;
  }
  ESP_LOGI(TAG, "%s", line.c_str());
}

void LDC1314Component::char_tick_() {
  uint32_t now = millis();
  bool do_sample = (now - this->char_last_sample_ms_) >= this->char_sample_interval_ms_;
  bool do_progress = (now - this->char_last_progress_ms_) >= this->char_progress_interval_ms_;

  switch (this->char_stage_) {
    case CHAR_STAGE_IDLE:
    case CHAR_STAGE_DERIVE:
    case CHAR_STAGE_COMMIT:
      // Transient/resting stages -- never the persisted state between loop() calls.
      return;

    case CHAR_STAGE_PREPARE: {
      if (do_sample) {
        this->char_last_sample_ms_ = now;
        for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
          if (!this->channels_[ch].active())
            continue;
          uint16_t raw = 0;
          if (!this->read_channel_raw_(ch, &raw)) {
            this->char_i2c_failures_++;
            continue;
          }
          uint16_t code = raw & DATA_RESULT_MASK;
          ChannelAccum &acc = this->char_accum_[ch];
          if (this->char_idle_samples_ == 0 || code < acc.code_min)
            acc.code_min = code;
          if (this->char_idle_samples_ == 0 || code > acc.code_max)
            acc.code_max = code;
          acc.idle_code = code;
        }
        this->char_idle_samples_++;
      }
      if (now - this->char_stage_start_ms_ >= this->char_idle_duration_ms_) {
        if (this->char_idle_samples_ == 0) {
          this->char_abort_("no samples collected during the idle baseline -- I2C communication failure");
          return;
        }
        bool already_moving = false;
        for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
          if (!this->channels_[ch].active())
            continue;
          const ChannelAccum &acc = this->char_accum_[ch];
          if ((acc.code_max - acc.code_min) > IDLE_MOTION_THRESHOLD)
            already_moving = true;
        }
        if (already_moving) {
          this->char_abort_("target was already moving during the idle baseline -- keep it still until prompted");
          return;
        }
        ESP_LOGI(TAG, "Stage 0/5  done.  idle baseline captured.");

        // Pre-flight belongs here and nowhere else: it needs a controlled, target-still baseline
        // to mean anything. Run at boot instead, it would describe whatever arbitrary position
        // the target happened to be resting in. It never aborts -- a configuration already
        // outside the datasheet recommendations is still worth characterizing, so the run can be
        // compared against a corrected one.
        if (this->run_preflight_(&this->preflight_)) {
          this->log_preflight_(this->preflight_);
        } else {
          ESP_LOGW(TAG, "Pre-flight skipped: no channel could be read");
        }

        this->char_enter_stage_(CHAR_STAGE_AUTO_IDRIVE);
      }
      break;
    }

    case CHAR_STAGE_AUTO_IDRIVE: {
      if (do_sample) {
        this->char_last_sample_ms_ = now;
        uint16_t sample_status = 0;
        uint8_t sampled_channels = 0;
        for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
          if (!this->channels_[ch].active())
            continue;
          uint8_t init_idrive = 0;
          uint16_t status = 0;
          if (!this->read_init_idrive_(ch, &init_idrive) || !this->read_status_raw_(&status)) {
            this->char_i2c_failures_++;
            continue;
          }
          ChannelAccum &acc = this->char_accum_[ch];
          if (this->char_drive_samples_ == 0 || init_idrive < acc.idrive_min)
            acc.idrive_min = init_idrive;
          if (this->char_drive_samples_ == 0 || init_idrive > acc.idrive_max)
            acc.idrive_max = init_idrive;
          acc.idrive_last = init_idrive;
          this->char_status_or_ |= status;
          sample_status |= status;
          sampled_channels++;
        }
        this->char_drive_samples_++;

        // Stuck-drive early abort. Judged on this sample's own values -- idrive_last and
        // sample_status, never the min/max envelope or char_status_or_ -- because those latch: an
        // envelope that once touched 31 says nothing about where the loop is now.
        //
        // Requires a complete batch: a channel whose read failed still holds its previous
        // idrive_last, and deciding "every channel is pinned" partly on stale values could abort a
        // healthy run. An incomplete batch resets the timer rather than being skipped, so the
        // abort only ever fires on an unbroken run of fully-observed samples.
        if (sampled_channels < this->active_channel_count_()) {
          this->char_pinned_since_ms_ = 0;
        } else {
          bool all_at_max = true;
          bool all_at_min = true;
          for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
            if (!this->channels_[ch].active())
              continue;
            if (this->char_accum_[ch].idrive_last != 31)
              all_at_max = false;
            if (this->char_accum_[ch].idrive_last != 0)
              all_at_min = false;
          }
          bool stuck_low = all_at_max && (sample_status & STATUS_ERR_ALE) != 0;
          bool stuck_high = all_at_min && (sample_status & STATUS_ERR_AHE) != 0;

          if (!stuck_low && !stuck_high) {
            this->char_pinned_since_ms_ = 0;
          } else if (this->char_pinned_since_ms_ == 0 || this->char_pinned_high_ != stuck_high) {
            // First observation, or the loop flipped to the other endpoint -- either way the
            // clock starts now.
            this->char_pinned_since_ms_ = now;
            this->char_pinned_high_ = stuck_high;
          } else if (now - this->char_pinned_since_ms_ >= PINNED_DRIVE_ABORT_MS) {
            char reason[160];
            snprintf(reason, sizeof(reason),
                     "%s persists at %s drive: INIT_IDRIVE %u on every active channel with %s "
                     "asserted continuously for %us",
                     stuck_high ? "amplitude-high" : "amplitude-low", stuck_high ? "minimum" : "maximum",
                     stuck_high ? 0 : 31, stuck_high ? "ERR_AHE" : "ERR_ALE",
                     static_cast<unsigned>((now - this->char_pinned_since_ms_) / 1000));
            this->char_fail_amplitude_ = true;
            this->char_abort_(reason);
            return;
          }
        }
      }
      if (do_progress) {
        this->char_last_progress_ms_ = now;
        this->char_progress_();
      }
      if (now - this->char_stage_start_ms_ >= this->char_stage_duration_ms_) {
        if (this->char_drive_samples_ == 0) {
          this->char_abort_("no samples collected -- I2C communication failure");
          return;
        }

        uint8_t global_min = 31;
        bool any_at_max = false;
        bool any_at_min = false;
        for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
          if (!this->channels_[ch].active())
            continue;
          const ChannelAccum &acc = this->char_accum_[ch];
          if (acc.idrive_min < global_min)
            global_min = acc.idrive_min;
          if (acc.idrive_max >= 31)
            any_at_max = true;
          if (acc.idrive_min <= 0)
            any_at_min = true;
        }

        // Failure detection -- see .plan Stage 1 "Failure detection". The continuous case is
        // caught far earlier by the stuck-drive abort above; these catch the intermittent one,
        // where the loop reached an endpoint with the matching error at some point during the
        // stage but never sat there long enough to trip it.
        if (any_at_max && (this->char_status_or_ & STATUS_ERR_ALE)) {
          this->char_fail_amplitude_ = true;
          this->char_abort_(
              "amplitude-low observed at maximum drive: INIT_IDRIVE reached 31 with ERR_ALE "
              "asserted during the stage, so the derived drive current would sit at the top of "
              "the range with no headroom");
          return;
        }
        if (any_at_min && (this->char_status_or_ & STATUS_ERR_AHE)) {
          this->char_fail_amplitude_ = true;
          this->char_abort_(
              "amplitude-high observed at minimum drive: INIT_IDRIVE reached 0 with ERR_AHE "
              "asserted during the stage, so the derived drive current would sit at the bottom of "
              "the range with no headroom");
          return;
        }

        // idrive[ch] = min(INIT_IDRIVE observed) -- see .plan Stage 1 for the full reasoning
        // (weakest coupling = highest RP = the device's own auto-cal settling on the lowest
        // code, matching TI's "target at maximum operating distance" condition; also the safe
        // side, since over-amplitude risks the ESD clamp with no guaranteed error flag).
        this->char_result_idrive_ = global_min;

        ESP_LOGI(TAG, "Stage 1/5  done.  INIT_IDRIVE -> recommended IDRIVE %u", this->char_result_idrive_);
        this->char_enter_stage_(CHAR_STAGE_ENVELOPE);
      }
      break;
    }

    case CHAR_STAGE_ENVELOPE: {
      if (do_sample) {
        this->char_last_sample_ms_ = now;
        for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
          if (!this->channels_[ch].active())
            continue;
          uint16_t raw = 0;
          if (!this->read_channel_raw_(ch, &raw)) {
            this->char_i2c_failures_++;
            continue;
          }
          if (raw & DATA_ERR_WD)
            continue;  // watchdog data is explicitly invalid -- discard, per the datasheet
          uint16_t code = raw & DATA_RESULT_MASK;
          ChannelAccum &acc = this->char_accum_[ch];
          if (this->char_envelope_samples_ == 0 || code < acc.code_min)
            acc.code_min = code;
          if (this->char_envelope_samples_ == 0 || code > acc.code_max)
            acc.code_max = code;
        }
        this->char_envelope_samples_++;
      }
      if (do_progress) {
        this->char_last_progress_ms_ = now;
        this->char_progress_();
      }
      if (now - this->char_stage_start_ms_ >= this->char_stage_duration_ms_) {
        if (this->char_envelope_samples_ == 0) {
          this->char_abort_("no samples collected -- I2C communication failure");
          return;
        }
        ESP_LOGI(TAG, "Stage 2/5  done.");
        this->char_enter_stage_(CHAR_STAGE_DERIVE);
      }
      break;
    }

    case CHAR_STAGE_VERIFY: {
      if (do_sample) {
        this->char_last_sample_ms_ = now;
        for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
          if (!this->channels_[ch].active())
            continue;
          uint16_t raw = 0;
          uint16_t status = 0;
          if (!this->read_channel_raw_(ch, &raw) || !this->read_status_raw_(&status)) {
            this->char_i2c_failures_++;
            continue;
          }
          if (raw & DATA_ERR_UR)
            this->char_verify_ur_++;
          if (raw & DATA_ERR_OR)
            this->char_verify_or_++;
          this->char_status_or_ |= status;
          if (raw & DATA_ERR_WD)
            continue;
          uint16_t code = raw & DATA_RESULT_MASK;
          ChannelAccum &acc = this->char_accum_[ch];
          if (this->char_verify_samples_ == 0 || code < acc.verify_min)
            acc.verify_min = code;
          if (this->char_verify_samples_ == 0 || code > acc.verify_max)
            acc.verify_max = code;
        }
        this->char_verify_samples_++;
      }
      if (do_progress) {
        this->char_last_progress_ms_ = now;
        this->char_progress_();
      }
      if (now - this->char_stage_start_ms_ >= this->char_verify_duration_ms_) {
        this->char_stage_verify_();
      }
      break;
    }

    default:
      break;
  }
}

void LDC1314Component::char_stage_derive_() {
  // Guard: the target must have actually moved during the envelope measurement -- not a
  // termination condition, a sanity check on the collected data (.plan Stage 3).
  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    if (!this->channels_[ch].active())
      continue;
    if (this->char_accum_[ch].code_max <= this->char_accum_[ch].code_min) {
      this->char_abort_("target did not move during characterization");
      return;
    }
  }

  static const uint8_t GAINS[4] = {16, 8, 4, 1};
  static const uint8_t SHIFTS[4] = {4, 3, 2, 0};

  double ratio_min[MAX_CHANNELS] = {};
  double ratio_max[MAX_CHANNELS] = {};
  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    if (!this->channels_[ch].active())
      continue;
    ratio_min[ch] = static_cast<double>(this->char_accum_[ch].code_min) / 4096.0;
    ratio_max[ch] = static_cast<double>(this->char_accum_[ch].code_max) / 4096.0;
  }

  // Gain first, and globally: OUTPUT_GAIN is one RESET_DEV field shared by all active channels,
  // so the widest-span channel constrains everyone (.plan Stage 3 "Gain first, and globally").
  double scale_limit = (1.0 - static_cast<double>(this->char_headroom_)) * 4095.0;
  uint8_t chosen_gain_idx = 3;  // fall back to gain 1 if nothing fits within headroom
  uint8_t binding_channel = 0;
  for (uint8_t gi = 0; gi < 4; gi++) {
    double scale = static_cast<double>(1u << (12 + SHIFTS[gi]));
    double max_span = 0;
    uint8_t max_span_channel = 0;
    bool fits = true;
    for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
      if (!this->channels_[ch].active())
        continue;
      double span = (ratio_max[ch] - ratio_min[ch]) * scale;
      if (span > max_span) {
        max_span = span;
        max_span_channel = ch;
      }
      if (span > scale_limit)
        fits = false;
    }
    if (fits) {
      chosen_gain_idx = gi;
      binding_channel = max_span_channel;
      break;
    }
  }
  this->char_binding_channel_ = binding_channel;

  // Offset second, per channel, centring each channel's span -- with a bounded retry that steps
  // the gain down if any predicted endpoint would clip (.plan Stage 3 "Offset second").
  OutputGain gain = LDC1314_OUTPUT_GAIN_1;
  uint16_t computed_offset[MAX_CHANNELS] = {};
  for (; chosen_gain_idx < 4; chosen_gain_idx++) {
    uint8_t shift = SHIFTS[chosen_gain_idx];
    switch (GAINS[chosen_gain_idx]) {
      case 16:
        gain = LDC1314_OUTPUT_GAIN_16;
        break;
      case 8:
        gain = LDC1314_OUTPUT_GAIN_8;
        break;
      case 4:
        gain = LDC1314_OUTPUT_GAIN_4;
        break;
      default:
        gain = LDC1314_OUTPUT_GAIN_1;
        break;
    }
    double scale = static_cast<double>(1u << (12 + shift));
    bool all_ok = true;

    for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
      if (!this->channels_[ch].active())
        continue;
      double span = (ratio_max[ch] - ratio_min[ch]) * scale;
      double target_low = (4095.0 - span) / 2.0;
      double offset_fraction = ratio_min[ch] - (target_low / scale);
      // Enforce OFFSET/2^16 < ratio_min (datasheet §8.1.3.1) -- exceeding it drives the output
      // negative into under-range.
      if (offset_fraction < 0)
        offset_fraction = 0;
      if (offset_fraction >= ratio_min[ch])
        offset_fraction = ratio_min[ch] * 0.5;

      double offset_reg = offset_fraction * 65536.0 + 0.5;  // manual round-to-nearest
      if (offset_reg > 65535.0)
        offset_reg = 65535.0;
      computed_offset[ch] = static_cast<uint16_t>(offset_reg);

      double predicted_min = (ratio_min[ch] - offset_fraction) * scale;
      double predicted_max = (ratio_max[ch] - offset_fraction) * scale;
      if (predicted_min <= 0 || predicted_max >= 4095)
        all_ok = false;
    }

    if (all_ok || chosen_gain_idx == 3)
      break;
  }

  this->char_result_output_gain_ = gain;
  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++)
    this->char_result_offset_[ch] = computed_offset[ch];

  ESP_LOGI(TAG, "  GAIN %u (channel %u binding)", output_gain_to_value_(this->char_result_output_gain_),
           this->char_binding_channel_);
  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    if (this->channels_[ch].active())
      ESP_LOGI(TAG, "  OFFSET CH%u 0x%04X", ch, this->char_result_offset_[ch]);
  }

  this->char_enter_stage_(CHAR_STAGE_VERIFY);
}

void LDC1314Component::char_stage_verify_() {
  bool clipped = false;
  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    if (!this->channels_[ch].active())
      continue;
    const ChannelAccum &acc = this->char_accum_[ch];
    if (acc.verify_min == 0 || acc.verify_max >= DATA_RESULT_MASK)
      clipped = true;
  }
  bool has_errors = (this->char_verify_ur_ > 0) || (this->char_verify_or_ > 0) || clipped;

  if (has_errors && this->char_verify_retries_ < MAX_VERIFY_RETRIES) {
    uint8_t current = output_gain_to_value_(this->char_result_output_gain_);
    uint8_t next = current;
    if (current == 16)
      next = 8;
    else if (current == 8)
      next = 4;
    else if (current == 4)
      next = 1;
    if (next != current) {
      this->char_verify_retries_++;
      ESP_LOGW(TAG, "Stage 4/5  clipping detected at gain %u -- retrying at gain %u (attempt %u/%u)", current, next,
               this->char_verify_retries_, MAX_VERIFY_RETRIES);
      this->char_result_output_gain_ = output_gain_from_value_(next);
      this->char_enter_stage_(CHAR_STAGE_VERIFY);
      return;
    }
  }

  if (has_errors) {
    this->char_abort_("verification failed: clipping or range errors persisted at the lowest gain");
    return;
  }

  ESP_LOGI(TAG, "Stage 4/5  done.  pass.");
  ESP_LOGI(TAG, ">>> %s", this->char_prompt_stop_.c_str());
  this->char_enter_stage_(CHAR_STAGE_COMMIT);
}

void LDC1314Component::char_stage_commit_() {
  if (!this->char_restore_) {
    // characterization.restore: false -- measure and report only, never persist or apply. Used
    // for a dry run / diagnostic pass. Nothing about calibration or override state changes.
    ESP_LOGW(TAG, "restore: false -- measurement only, nothing stored or applied");
    this->char_report_(true, "");
    this->char_write_registers_(this->char_snapshot_output_gain_, this->char_snapshot_idrive_,
                                 this->char_snapshot_offset_, true, true);
    this->char_stage_ = CHAR_STAGE_IDLE;
    return;
  }

  CalibrationRecord rec{};
  rec.version = LDC1314_SETTINGS_VERSION;
  rec.output_gain = static_cast<uint8_t>(this->char_result_output_gain_);
  rec.channel_mask = 0;
  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    if (!this->channels_[ch].active())
      continue;
    rec.channel_mask |= static_cast<uint8_t>(1 << ch);
    rec.idrive[ch] = this->char_result_idrive_;
    rec.offset[ch] = this->char_result_offset_[ch];
  }
  this->save_calibration_(rec);

  // Interaction rule (.plan "A new run vs an armed override"): a disarmed override is refreshed
  // to the new calibration; an armed one is left alone, with a loud warning that it now masks a
  // fresh run rather than silently discarding a deliberate manual value.
  if (!this->override_.armed) {
    this->seed_override_from_calibration_();
  } else {
    ESP_LOGW(TAG,
             "Manual override is armed -- the new calibration is stored but masked; disarm it to use the "
             "values just measured");
  }

  if (this->calibrated_output_gain_sensor_ != nullptr)
    this->calibrated_output_gain_sensor_->publish_state(output_gain_to_value_(this->char_result_output_gain_));
  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    if (this->channels_[ch].calibrated_idrive_sensor != nullptr)
      this->channels_[ch].calibrated_idrive_sensor->publish_state(this->char_result_idrive_);
    if (this->channels_[ch].calibrated_offset_sensor != nullptr)
      this->channels_[ch].calibrated_offset_sensor->publish_state(this->char_result_offset_[ch]);
  }

  this->char_report_(true, "");

  // record A (and possibly B) just changed -- re-resolve and re-apply so normal operation resumes
  // on the freshly-derived values instead of whatever was in force during the run.
  this->apply_config_();

  this->char_stage_ = CHAR_STAGE_IDLE;
}

void LDC1314Component::char_report_(bool success, const std::string &failure_reason) {
  ESP_LOGI(TAG, "================================================================");
  ESP_LOGI(TAG, " Characterization %s", success ? "finished" : "aborted");
  ESP_LOGI(TAG, "================================================================");

  ESP_LOGI(TAG, " Board");
  ESP_LOGI(TAG, "   LDC1314 @ 0x%02X, %u channel(s) active", this->get_i2c_address(),
           static_cast<unsigned>(this->active_channel_count_()));

  uint32_t total_samples =
      this->char_idle_samples_ + this->char_drive_samples_ + this->char_envelope_samples_ + this->char_verify_samples_;
  ESP_LOGI(TAG, " Samples");
  ESP_LOGI(TAG, "   %u total (%u idle + %u drive + %u envelope + %u verify), %u I2C failure(s)",
           static_cast<unsigned>(total_samples), static_cast<unsigned>(this->char_idle_samples_),
           static_cast<unsigned>(this->char_drive_samples_), static_cast<unsigned>(this->char_envelope_samples_),
           static_cast<unsigned>(this->char_verify_samples_), static_cast<unsigned>(this->char_i2c_failures_));

  ESP_LOGI(TAG, " Idle baseline (target still)");
  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    if (this->channels_[ch].active())
      ESP_LOGI(TAG, "   CH%u  %u", ch, this->char_accum_[ch].idle_code);
  }

  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    if (!this->channels_[ch].active())
      continue;
    ESP_LOGI(TAG, " Channel %u", ch);
    ESP_LOGI(TAG, "   INIT_IDRIVE  %u..%u", this->char_accum_[ch].idrive_min, this->char_accum_[ch].idrive_max);
  }

  if (!success) {
    ESP_LOGI(TAG, " Failure reason");
    ESP_LOGI(TAG, "   %s", failure_reason.c_str());

    // Everything below is stated as measurement and datasheet rule, never as a diagnosis: the
    // driver lists which constraints are not met and which others bear on the symptom, and lets
    // the user pick the next experiment. See .plan "The driver reports facts, not conclusions".
    if (this->preflight_.valid) {
      bool any_failed = (this->preflight_.fclk_known && !this->preflight_.deglitch_ok) ||
                        !this->preflight_.data_limit_ok;
      if (any_failed) {
        ESP_LOGI(TAG, " Datasheet checks not met");
        if (this->preflight_.fclk_known && !this->preflight_.deglitch_ok) {
          ESP_LOGI(TAG, "   8.1.7   DEGLITCH %.1f MHz vs %.2f MHz max sensor frequency",
                   this->preflight_.deglitch_hz / 1e6, this->preflight_.max_fsensor_hz / 1e6);
        }
        if (!this->preflight_.data_limit_ok) {
          ESP_LOGI(TAG, "   8.1.6   max DATA %u is at or above the 1024 clock limit",
                   static_cast<unsigned>(this->preflight_.max_code));
        }
      }

      if (this->char_fail_amplitude_) {
        ESP_LOGI(TAG, " Other constraints bearing on amplitude");
        ESP_LOGI(TAG, "           the device swept its full auto-calibration range without");
        ESP_LOGI(TAG, "           reaching the 1.2-1.8 V target, so no IDRIVE value resolves it");
        ESP_LOGI(TAG, "   8.1.6   SETTLECOUNT supports coil Q up to %.1f (CH%u binding);",
                 this->preflight_.min_q_max, this->preflight_.q_binding_channel);
        ESP_LOGI(TAG, "           the coil's actual Q is not measurable here");
        ESP_LOGI(TAG, "   8.1.5   RP below ~0.6 kOhm cannot reach 1.2 V even at IDRIVE 31");
      }

      ESP_LOGI(TAG, " Suggested next experiment");
      ESP_LOGI(TAG, "   %s", this->preflight_suggestion_(this->preflight_).c_str());
      ESP_LOGI(TAG, "   Change one variable per run.");
    }

    ESP_LOGI(TAG, "================================================================");
    return;
  }

  ESP_LOGI(TAG, " Recommended");
  ESP_LOGI(TAG, "   IDRIVE  %u   (lowest INIT_IDRIVE observed across all channels)", this->char_result_idrive_);
  ESP_LOGI(TAG, "   GAIN    %u   (global; channel %u is the binding channel)",
           output_gain_to_value_(this->char_result_output_gain_), this->char_binding_channel_);

  ESP_LOGI(TAG, " Offsets");
  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    if (this->channels_[ch].active())
      ESP_LOGI(TAG, "   CH%u  0x%04X", ch, this->char_result_offset_[ch]);
  }

  ESP_LOGI(TAG, " Signal quality");
  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    if (!this->channels_[ch].active())
      continue;
    const ChannelAccum &acc = this->char_accum_[ch];
    uint16_t span = acc.code_max - acc.code_min;
    uint16_t verify_span = (acc.verify_max > acc.verify_min) ? (acc.verify_max - acc.verify_min) : 0;
    double enob_before = span > 0 ? std::log2(static_cast<double>(span)) : 0.0;
    double enob_after = verify_span > 0 ? std::log2(static_cast<double>(verify_span)) : 0.0;
    ESP_LOGI(TAG, "   CH%u  min/max %u/%u  span %u  ENOB %.2f  ->  %u codes, ENOB %.2f", ch, acc.code_min,
             acc.code_max, span, enob_before, verify_span, enob_after);
  }

  ESP_LOGI(TAG, " Verification");
  ESP_LOGI(TAG, "   %u under-range, %u over-range, over %u second(s), clean",
           static_cast<unsigned>(this->char_verify_ur_), static_cast<unsigned>(this->char_verify_or_),
           static_cast<unsigned>(this->char_verify_duration_ms_ / 1000));

  ESP_LOGI(TAG, " Suggested YAML   (optional -- these values are already stored)");
  ESP_LOGI(TAG, "   ldc1314:");
  ESP_LOGI(TAG, "     - id: ldc");
  ESP_LOGI(TAG, "       output_gain: %u", output_gain_to_value_(this->char_result_output_gain_));
  ESP_LOGI(TAG, "   sensor:");
  for (uint8_t ch = 0; ch < MAX_CHANNELS; ch++) {
    if (!this->channels_[ch].active())
      continue;
    ESP_LOGI(TAG, "     - platform: ldc1314");
    ESP_LOGI(TAG, "       channel: %u", ch);
    ESP_LOGI(TAG, "       idrive: %u", this->char_result_idrive_);
    ESP_LOGI(TAG, "       offset: 0x%04X", this->char_result_offset_[ch]);
  }
  ESP_LOGI(TAG, "================================================================");
}

}  // namespace ldc1314
}  // namespace esphome
