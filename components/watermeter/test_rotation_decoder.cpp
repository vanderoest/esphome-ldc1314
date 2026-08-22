// Host test harness for RotationDecoder -- .plan "Validation" step 3: compile the class on the
// desktop and replay captured traces, checked against both known ground truth (the operator's
// ~10 L bucket draw) and the tools/ Python prototype's findings on the same windows.
//
// Build & run: see run_host_tests.sh in this directory.

#include "rotation_decoder.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <regex>
#include <string>
#include <vector>

namespace {

struct Sample {
  int hh = -1, mm = -1, ss = -1;  // wall clock, from the esphome log prefix; -1 if absent
  double t_s = 0;                 // driver's own millis() timestamp, seconds
  float ch[3] = {0, 0, 0};
};

bool parse_line(const std::string &line, Sample *out) {
  static const std::regex trace_re(R"(TRACE,(\d+),(\d+),(\d+),(\d+))");
  static const std::regex wall_re(R"(\[(\d\d):(\d\d):(\d\d)\.\d+\])");
  std::smatch m;
  if (!std::regex_search(line, m, trace_re))
    return false;
  out->t_s = std::stol(m[1]) / 1000.0;
  out->ch[0] = std::stof(m[2]);
  out->ch[1] = std::stof(m[3]);
  out->ch[2] = std::stof(m[4]);
  std::smatch wm;
  if (std::regex_search(line, wm, wall_re)) {
    out->hh = std::stoi(wm[1]);
    out->mm = std::stoi(wm[2]);
    out->ss = std::stoi(wm[3]);
  }
  return true;
}

std::vector<Sample> load_csv(const std::string &path) {
  std::ifstream f(path);
  std::vector<Sample> out;
  std::string line;
  while (std::getline(f, line)) {
    Sample s;
    if (parse_line(line, &s))
      out.push_back(s);
  }
  return out;
}

int wall_seconds(int hh, int mm, int ss) { return hh * 3600 + mm * 60 + ss; }

// Not guaranteed by the standard -- rotation_decoder.cpp avoids M_PI for the same reason.
constexpr double kTwoPi = 6.28318530717958647692;

int g_failures = 0;

void check(bool cond, const std::string &msg) {
  if (cond) {
    std::printf("ok:   %s\n", msg.c_str());
  } else {
    std::printf("FAIL: %s\n", msg.c_str());
    g_failures++;
  }
}

// Runs the whole file through a fresh decoder, capturing revolutions() the first time each of
// four target wall-clock seconds is reached. Returns false if any target second was never seen
// (e.g. because the file doesn't cover it) -- callers must check before trusting the deltas.
bool revolutions_at(const std::vector<Sample> &samples, int t0, int t1, int t2, int t3, double *r0,
                     double *r1, double *r2, double *r3) {
  watermeter_core::RotationDecoder decoder;
  bool have0 = false, have1 = false, have2 = false, have3 = false;
  for (const auto &s : samples) {
    decoder.update(s.ch, s.t_s);
    if (s.hh < 0)
      continue;
    int ws = wall_seconds(s.hh, s.mm, s.ss);
    if (!have0 && ws == t0) {
      *r0 = decoder.revolutions();
      have0 = true;
    }
    if (!have1 && ws == t1) {
      *r1 = decoder.revolutions();
      have1 = true;
    }
    if (!have2 && ws == t2) {
      *r2 = decoder.revolutions();
      have2 = true;
    }
    if (!have3 && ws == t3) {
      *r3 = decoder.revolutions();
      have3 = true;
    }
  }
  return have0 && have1 && have2 && have3;
}

void test_rest_slow_fast_bucket(const std::string &capture_dir) {
  auto samples = load_csv(capture_dir + "/2026-08-21_rest-slow-fast-bucket.csv");
  check(!samples.empty(), "rest-slow-fast-bucket.csv: loaded");
  if (samples.empty())
    return;

  // Wall-clock bounds from the auto-detected segment table (tools/analyze_trace.py): at-rest
  // 19:04:36-19:04:59, bucket draw 19:06:35-19:08:10.
  double rest_start, rest_end, bucket_start, bucket_end;
  bool ok = revolutions_at(samples, 19 * 3600 + 4 * 60 + 40, 19 * 3600 + 4 * 60 + 58,
                            19 * 3600 + 6 * 60 + 36, 19 * 3600 + 8 * 60 + 8, &rest_start, &rest_end,
                            &bucket_start, &bucket_end);
  check(ok, "rest-slow-fast-bucket.csv: all four target timestamps found in the capture");
  if (!ok)
    return;

  double rest_drift = std::fabs(rest_end - rest_start);
  std::printf("      at-rest drift: %.4f rev\n", rest_drift);
  check(rest_drift < 0.02, "at-rest segment: near-zero drift (hysteresis rejects LSB dither)");

  double bucket_revs = bucket_end - bucket_start;
  std::printf("      bucket-draw revolutions: %.3f (operator-reported ~10 L; tools/ prototype found 10.0-10.3)\n",
              bucket_revs);
  check(bucket_revs > 9.0 && bucket_revs < 11.5, "bucket-draw window: revolutions in [9.0, 11.5]");
}

void test_phaseb_gain8_offset(const std::string &capture_dir) {
  auto samples = load_csv(capture_dir + "/2026-08-21_phaseb-gain8-offset.csv");
  check(!samples.empty(), "phaseb-gain8-offset.csv: loaded");
  if (samples.empty())
    return;

  // Segments (tools/analyze_trace.py auto-detection): rest 19:49:05-19:49:30, pour
  // 19:49:30-19:50:05, rest 19:50:05-19:50:25. Checkpoints sit a couple seconds inside each
  // segment's boundary, not right at it, so a transition doesn't leak into the wrong bucket.
  double rest1_start, rest1_end, pour_end, rest2_end;
  bool ok = revolutions_at(samples, 19 * 3600 + 49 * 60 + 10, 19 * 3600 + 49 * 60 + 28,
                            19 * 3600 + 50 * 60 + 7, 19 * 3600 + 50 * 60 + 22, &rest1_start, &rest1_end,
                            &pour_end, &rest2_end);
  check(ok, "phaseb-gain8-offset.csv: all four target timestamps found in the capture");
  if (!ok)
    return;

  double rest1_drift = std::fabs(rest1_end - rest1_start);
  std::printf("      pre-pour rest drift: %.4f rev\n", rest1_drift);
  check(rest1_drift < 0.02, "gain-8 capture, pre-pour rest: near-zero drift");

  double pour_revs = pour_end - rest1_end;
  std::printf("      pour revolutions: %.3f (not volume-measured -- just needs to be clearly positive)\n",
              pour_revs);
  check(pour_revs > 1.0, "gain-8 capture: pour segment shows clear positive rotation");

  double rest2_drift = std::fabs(rest2_end - pour_end);
  std::printf("      post-pour rest drift: %.4f rev\n", rest2_drift);
  check(rest2_drift < 0.02, "gain-8 capture, post-pour rest: near-zero drift");

  // Regression check for the field bug found after real calibration: signal_quality reading
  // 1.15-1.20+ isn't itself wrong (the locus has ~20% ripple, TODO.md Phase A), but a "rotating"
  // false-positive from noise letting the envelope erode would show up as r climbing unbounded
  // over an extended rest period, not settling. Replay the whole post-pour rest segment on a
  // single decoder and track the max -- this doesn't reproduce the exact session-dependent noise
  // floor that triggered the bug (this capture's dither didn't happen to cross that threshold),
  // but it does catch any regression that makes r diverge instead of staying bounded near the
  // locus's own known ripple.
  watermeter_core::RotationDecoder decoder2;
  float max_r_post_pour = 0.0f;
  for (const auto &s : samples) {
    decoder2.update(s.ch, s.t_s);
    if (s.hh < 0)
      continue;
    int ws = wall_seconds(s.hh, s.mm, s.ss);
    if (ws >= 19 * 3600 + 50 * 60 + 7 && ws <= 19 * 3600 + 50 * 60 + 22)
      max_r_post_pour = std::max(max_r_post_pour, decoder2.signal_quality());
  }
  std::printf("      max signal_quality during post-pour rest: %.3f\n", max_r_post_pour);
  check(max_r_post_pour < 1.5, "gain-8 capture: signal quality stays bounded (no unbounded drift) through rest");
}

// Full-file replay of both captures: nothing NaN/infinite, regardless of segment boundaries.
void test_no_nan_across_full_files(const std::string &capture_dir) {
  for (const char *name : {"2026-08-21_rest-slow-fast-bucket.csv", "2026-08-21_phaseb-gain8-offset.csv"}) {
    auto samples = load_csv(capture_dir + "/" + name);
    watermeter_core::RotationDecoder decoder;
    bool finite = true;
    for (const auto &s : samples) {
      decoder.update(s.ch, s.t_s);
      if (!std::isfinite(decoder.revolutions()) || !std::isfinite(decoder.signal_quality()) ||
          !std::isfinite(decoder.angle_rad())) {
        finite = false;
        break;
      }
    }
    check(finite, std::string(name) + ": revolutions/signal_quality/angle stay finite throughout");
  }
}

// TODO.md Phase D: "accumulate in double, never float" -- the failure mode (a float accumulator
// silently freezing once its ULP exceeds the hysteresis quantum) is invisible on a bench test
// starting from zero, so it has to be checked at a realistic install reading instead. This is not
// exercising WatermeterComponent itself (it depends on ESPHome's runtime, not host-testable) --
// it's checking the specific arithmetic claim design_decisions.md makes, directly.
void test_accumulator_precision_at_realistic_reading() {
  constexpr double kIncrementL = 0.02792527;  // 10 deg hysteresis quantum at 1 L/rev (design_decisions.md)
  constexpr int kIncrements = 100000;         // ~2792 L of simulated flow

  double acc_double = 1200000.0;  // a realistic multi-year install reading
  float acc_float = 1200000.0f;
  for (int i = 0; i < kIncrements; i++) {
    acc_double += kIncrementL;
    acc_float += static_cast<float>(kIncrementL);
  }

  double expected = 1200000.0 + kIncrements * kIncrementL;
  double double_error = std::fabs(acc_double - expected);
  double float_error = std::fabs(static_cast<double>(acc_float) - expected);

  std::printf("      double accumulator: %.4f (error %.6f L)\n", acc_double, double_error);
  std::printf("      float accumulator:  %.4f (error %.6f L, for comparison -- not what's shipped)\n",
              static_cast<double>(acc_float), float_error);
  check(double_error < 0.01, "double accumulator stays accurate to <0.01 L at a 1.2 M L reading");
  // This is the failure mode itself, captured as a check: confirms the float column above is
  // demonstrating the real bug design_decisions.md describes, not a hypothetical one.
  check(float_error > 100.0, "float accumulator (for comparison) demonstrably loses most of the total here");
}

// Found in code review (Codex), not testing: timestamp_s is millis()/1000.0, which hard-resets to
// ~0 every ~49.7 days. Sweep the decoder through real, confirmed motion (so rotating_ is
// genuinely true), then feed a sample whose timestamp jumps far backward -- simulating the wrap --
// and confirm rotating_ drops to false immediately rather than latching true from a bare
// `elapsed < window` comparison that a large negative elapsed also satisfies.
void test_rotating_survives_timestamp_wrap() {
  watermeter_core::RotationDecoder decoder;
  const float base[3] = {150.0f, 150.0f, 150.0f};
  const float amp = 40.0f;
  const double phase_offset[3] = {0.0, kTwoPi / 3.0, 2.0 * kTwoPi / 3.0};

  // ~2 revolutions over 5s -- comfortably fills the bootstrap (amp 40 >> the 5.0-code threshold)
  // and produces many real hysteresis-confirmed steps well inside the 2s rotating_recent_window_s.
  double t = 0.0;
  for (int i = 0; i < 500; i++) {
    t += 0.01;
    double theta = t * (2.0 * kTwoPi / 5.0);
    float raw[3];
    for (int c = 0; c < 3; c++)
      raw[c] = base[c] + amp * static_cast<float>(std::cos(theta - phase_offset[c]));
    decoder.update(raw, t);
  }
  check(decoder.rotating(), "wrap test setup: decoder reports rotating after a real sweep");

  // One more sample, same position (no new motion), timestamp still advancing normally --
  // rotating_ should still read true here, well inside the window.
  float raw_last[3];
  for (int c = 0; c < 3; c++)
    raw_last[c] = base[c] + amp * static_cast<float>(std::cos(t * (2.0 * kTwoPi / 5.0) - phase_offset[c]));
  decoder.update(raw_last, t + 0.01);
  check(decoder.rotating(), "wrap test setup: still rotating one sample later, no wrap yet");

  // Simulate the millis() wrap: same raw values (no real motion), but timestamp collapses back
  // near zero instead of continuing to advance.
  decoder.update(raw_last, 0.001);
  check(!decoder.rotating(), "rotating_ drops to false immediately after a timestamp wrap, not stuck true");
}

// Envelope persistence (Phase D, code review): get_envelope()/restore_envelope() round-trip. A
// freshly-constructed decoder fed a restored envelope should behave as already-calibrated --
// no bootstrap needed -- and preserve the learned/auto-adapting distinction.
void test_envelope_persistence_round_trip() {
  watermeter_core::RotationDecoder original;
  const float base[3] = {150.0f, 150.0f, 150.0f};
  const float amp = 40.0f;
  const double phase_offset[3] = {0.0, kTwoPi / 3.0, 2.0 * kTwoPi / 3.0};
  double t = 0.0;
  for (int i = 0; i < 300; i++) {
    t += 0.01;
    double theta = t * (2.0 * kTwoPi / 5.0);
    float raw[3];
    for (int c = 0; c < 3; c++)
      raw[c] = base[c] + amp * static_cast<float>(std::cos(theta - phase_offset[c]));
    original.update(raw, t);
  }
  check(original.envelope_filled(), "persistence setup: original decoder's envelope is filled");
  check(!original.envelope_learned(), "persistence setup: filled by auto-bootstrap, not a learn pass");

  float saved_mid[3], saved_amp[3];
  original.get_envelope(saved_mid, saved_amp);

  watermeter_core::RotationDecoder restored;
  check(!restored.envelope_filled(), "a fresh decoder starts unfilled");
  restored.restore_envelope(saved_mid, saved_amp, original.envelope_learned());
  check(restored.envelope_filled(), "restore_envelope() marks the envelope filled immediately");
  check(!restored.envelope_learned(), "restoring an auto-bootstrapped envelope keeps it auto-adapting, not locked");

  // A single sample right at a known position should decode to a sane, calibrated r immediately
  // -- no multi-sample bootstrap needed, unlike a cold decoder which would read r=0 here.
  float raw[3];
  for (int c = 0; c < 3; c++)
    raw[c] = base[c] + amp * static_cast<float>(std::cos(0.0 - phase_offset[c]));
  restored.update(raw, 1000.0);
  std::printf("      restored decoder's first-sample signal_quality: %.3f\n", restored.signal_quality());
  check(restored.signal_quality() > 0.5f, "restored decoder trusts its envelope on the very first sample");
}

// Regression test for a real bug found on hardware, not by any prior test: a meter sitting
// completely still for an extended period (no real rotation ever) could satisfy the envelope
// bootstrap's old, too-low threshold from LSB dither alone, lock in a calibration ~30-50x smaller
// than the true signal amplitude, and then report wild phantom rotation from that same dither --
// `Signal quality` reading 0.667 and `Revolutions` jumping by -0.5/+0.167 rev with raw channel
// codes barely moving at all (captures/log3.txt from the field). Neither existing capture
// exercises "sits at rest indefinitely, never rotates even once" -- both eventually show real
// motion -- so this synthesizes exactly that: pure +-1 code dither, symmetric, no net drift,
// matching the measured diff histogram from TODO.md Phase B (ch0: 189 down / 190 up).
void test_no_phantom_rotation_from_pure_dither() {
  watermeter_core::RotationDecoder decoder;
  const float base[3] = {105.0f, 159.0f, 118.0f};  // realistic gain-8 resting values (log3.txt)
  double t = 0.0;
  double max_abs_revs = 0.0;
  float max_r = 0.0f;

  for (int i = 0; i < 5000; i++) {  // 50s simulated at 100 Hz -- far longer than the bootstrap window
    // Bounded oscillation around a FIXED center -- toggles between base and base+1, exactly the
    // shape measured at rest on real hardware (e.g. ch0: 146<->147, TODO.md Phase B). Not a
    // cumulative random walk: `+=` here would drift unboundedly over 5000 samples and isn't what
    // real dither does (a first version of this test made exactly that mistake).
    float raw[3] = {
        base[0] + (((i * 7) % 5 == 0) ? 1.0f : 0.0f),
        base[1] + (((i * 11) % 6 == 0) ? 1.0f : 0.0f),
        base[2] + (((i * 13) % 4 == 0) ? 1.0f : 0.0f),
    };

    t += 0.01;
    decoder.update(raw, t);

    max_abs_revs = std::max(max_abs_revs, std::fabs(decoder.revolutions()));
    max_r = std::max(max_r, decoder.signal_quality());
  }

  std::printf("      pure dither, 50s: max |revolutions| %.4f, max signal_quality (r) %.3f\n", max_abs_revs, max_r);
  check(max_abs_revs < 0.02, "pure dither never accumulates meaningful phantom rotation");
  check(max_r < 0.3, "pure dither never reads a misleadingly high signal quality (stays below r_min)");
}

}  // namespace

int main(int argc, char **argv) {
  std::string capture_dir = argc > 1 ? argv[1] : "../../captures";

  test_rest_slow_fast_bucket(capture_dir);
  test_phaseb_gain8_offset(capture_dir);
  test_no_nan_across_full_files(capture_dir);
  test_accumulator_precision_at_realistic_reading();
  test_no_phantom_rotation_from_pure_dither();
  test_rotating_survives_timestamp_wrap();
  test_envelope_persistence_round_trip();

  std::printf("\n%d check(s) failed\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}
