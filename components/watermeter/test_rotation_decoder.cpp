// Host test harness for RotationDecoder -- .plan "Validation" step 3: compile the class on the
// desktop and replay captured traces, checked against both known ground truth (the operator's
// ~10 L bucket draw) and the tools/ Python prototype's findings on the same windows.
//
// Build & run: see run_host_tests.sh in this directory.

#include "rotation_decoder.h"

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
  watermeter::RotationDecoder decoder;
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
}

// Full-file replay of both captures: nothing NaN/infinite, regardless of segment boundaries.
void test_no_nan_across_full_files(const std::string &capture_dir) {
  for (const char *name : {"2026-08-21_rest-slow-fast-bucket.csv", "2026-08-21_phaseb-gain8-offset.csv"}) {
    auto samples = load_csv(capture_dir + "/" + name);
    watermeter::RotationDecoder decoder;
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

}  // namespace

int main(int argc, char **argv) {
  std::string capture_dir = argc > 1 ? argv[1] : "../../captures";

  test_rest_slow_fast_bucket(capture_dir);
  test_phaseb_gain8_offset(capture_dir);
  test_no_nan_across_full_files(capture_dir);

  std::printf("\n%d check(s) failed\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}
