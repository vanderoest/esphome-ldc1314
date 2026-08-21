#!/usr/bin/env python3
"""Offline analysis of an ldc1314 raw-trace capture (TRACE,<millis>,<ch0>,<ch1>,<ch2> lines).

Settles the open questions from .plan Part 1 / TODO.md Phase A:
  - is the alpha/beta locus a clean circle (Clarke applies) or not (fall back to ellipse fit)?
  - what are the actual inter-coil phase angles and amplitudes (vs. the assumed ideal 120 deg)?
  - which channel leads, and does forward flow give increasing or decreasing theta?
  - how much 5th-harmonic content is actually present, vs. the ~2% sensor_head.md predicts?

Usage:
    python3 analyze_trace.py CAPTURE.csv [--window HH:MM:SS HH:MM:SS] [--out-dir DIR]

With no --window, the capture is auto-segmented by per-channel activity and the longest
sustained "active" segment (typically the bucket draw) is analyzed. Pass --window to target a
different segment, e.g. the slow-draw window, once you know its bounds from the segment table.
"""

import argparse
import json
import re
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

# dataviz skill reference palette, categorical slots 1-3 (blue/orange/aqua) -- the only three
# slots validated all-pairs CVD-safe together, which is exactly what a 3-channel overlay needs.
CHANNEL_COLORS_LIGHT = ["#2a78d6", "#eb6834", "#1baf7a"]
CHANNEL_COLORS_DARK = ["#3987e5", "#d95926", "#199e70"]

TRACE_RE = re.compile(r"TRACE,(\d+),(\d+),(\d+),(\d+)")
WALL_RE = re.compile(r"\[(\d\d):(\d\d):(\d\d)\.\d+\]")


def parse_trace(path: Path):
    """Returns (wall_seconds, millis, ch0, ch1, ch2) arrays. wall_seconds is None per-row if the
    line has no esphome log prefix (i.e. the capture was already stripped to bare TRACE, lines)."""
    wall, millis, c0, c1, c2 = [], [], [], [], []
    with open(path) as f:
        for line in f:
            m = TRACE_RE.search(line)
            if not m:
                continue  # truncated trailing line from a capture stopped mid-write, or non-TRACE noise
            ts, a, b, c = m.groups()
            millis.append(int(ts))
            c0.append(int(a))
            c1.append(int(b))
            c2.append(int(c))
            w = WALL_RE.search(line)
            wall.append(int(w.group(1)) * 3600 + int(w.group(2)) * 60 + int(w.group(3)) if w else None)
    return (
        np.array(wall, dtype=object),
        np.array(millis, dtype=np.float64),
        np.array(c0, dtype=np.float64),
        np.array(c1, dtype=np.float64),
        np.array(c2, dtype=np.float64),
    )


def format_hms(sec: int) -> str:
    return f"{sec // 3600:02d}:{(sec % 3600) // 60:02d}:{sec % 60:02d}"


def auto_segments(wall, c0, c1, c2, bucket_s=5, active_threshold=5):
    """5s-bucket activity table, like the ad-hoc check this script formalizes. Returns a list of
    (start_sec, end_sec, active) runs, merging adjacent buckets of the same activity class."""
    if wall[0] is None:
        raise ValueError("capture has no wall-clock prefix -- pass --window in millis instead, or "
                          "re-capture with the esphome log prefix intact")
    buckets = {}
    for w, a, b, c in zip(wall, c0, c1, c2):
        key = (w // bucket_s) * bucket_s
        buckets.setdefault(key, []).append((a, b, c))

    runs = []
    for sec in sorted(buckets):
        vals = np.array(buckets[sec])
        span = vals.max(axis=0) - vals.min(axis=0)
        active = bool(np.max(span) >= active_threshold)
        if runs and runs[-1][2] == active and sec - runs[-1][1] <= bucket_s:
            runs[-1] = (runs[-1][0], sec + bucket_s, active)
        else:
            runs.append((sec, sec + bucket_s, active))
    return runs


def envelope(x):
    return (x.max() + x.min()) / 2, (x.max() - x.min()) / 2


def ideal_clarke(c0, c1, c2, mid, amp):
    x0 = (c0 - mid[0]) / amp[0]
    x1 = (c1 - mid[1]) / amp[1]
    x2 = (c2 - mid[2]) / amp[2]
    alpha = (2 / 3) * (x0 - x1 / 2 - x2 / 2)
    beta = (x1 - x2) / np.sqrt(3)
    return alpha, beta, np.arctan2(beta, alpha)


def unwrap_angle(theta):
    return np.unwrap(theta)


def fit_channel_phase(theta_boot, x_i):
    """Least-squares fit of x_i(t) = P*cos(theta_boot) + Q*sin(theta_boot). Returns
    (amplitude, phase_rad) of channel i relative to the bootstrap zero -- an arbitrary reference,
    but the *differences* between channels are the real inter-coil electrical angles."""
    design = np.column_stack([np.cos(theta_boot), np.sin(theta_boot)])
    (p, q), *_ = np.linalg.lstsq(design, x_i, rcond=None)
    return float(np.hypot(p, q)), float(np.arctan2(q, p))


def harmonic_content(theta_unwrapped, x, n_harmonics=7, samples_per_rev=64):
    """FFT-based harmonic ratios computed in the ANGLE domain, not the time domain: x(t) is
    resampled onto a uniform theta grid spanning an integer number of complete revolutions, then
    averaged coherently across those revolutions before the FFT.

    This is deliberate, not a style choice: a naive time-domain FFT assumes constant rotation
    rate to map its frequency bins onto rotation harmonics, and real draws don't hold constant
    rate (this is exactly what caught out the first version of this function -- a ~2.67-revolution
    fast-draw window with mildly varying speed produced a nonsensical ~30% "2nd harmonic" against
    a design that predicts none at all, which was spectral leakage from the rate variation, not a
    real signal feature). Resampling onto theta and averaging across revolutions removes the rate
    dependence entirely and cancels revolution-to-revolution noise as a side benefit.
    """
    n_revs = int(np.floor((theta_unwrapped[-1] - theta_unwrapped[0]) / (2 * np.pi)))
    if n_revs < 1:
        return {}
    theta0 = theta_unwrapped[0]
    theta_grid = theta0 + np.linspace(0, n_revs * 2 * np.pi, n_revs * samples_per_rev, endpoint=False)
    x_resampled = np.interp(theta_grid, theta_unwrapped, x)
    x_by_rev = x_resampled.reshape(n_revs, samples_per_rev)
    x_avg_rev = x_by_rev.mean(axis=0)  # coherent average -- cancels non-periodic noise

    x_avg_rev = x_avg_rev - x_avg_rev.mean()
    spectrum = np.abs(np.fft.rfft(x_avg_rev))
    fundamental_mag = spectrum[1]  # exact bin: one cycle per revolution, by construction
    ratios = {}
    for h in range(2, n_harmonics + 1):
        if h < len(spectrum):
            ratios[h] = spectrum[h] / fundamental_mag
    return ratios, n_revs


def decimate(*arrays, max_points=20000):
    n = len(arrays[0])
    if n <= max_points:
        return arrays
    idx = np.linspace(0, n - 1, max_points).astype(int)
    return tuple(a[idx] for a in arrays)


def write_html_report(out_dir, ctx):
    """Single self-contained HTML report: the three raw phases over time (the actual disc
    movement under the coils, as measured), the alpha/beta locus, and the accumulated angle --
    interactive (pan/zoom/hover) via Plotly, since a multi-thousand-sample trace is unreadable as
    a static image. Colors are the dataviz skill's reference categorical palette, slots 1-3
    (blue/orange/aqua) -- the only three slots validated colorblind-safe against each other in
    every pairing, which is exactly the 3-channel case here.
    """
    t_s, w0, w1, w2 = decimate(ctx["t_s"], ctx["w0"], ctx["w1"], ctx["w2"])
    alpha, beta = decimate(ctx["alpha"], ctx["beta"])
    t_theta, theta_u = decimate(ctx["t_s"], ctx["theta_unwrapped"])

    harmonics_rows = ""
    if ctx["harmonics"]:
        for h, ratio in ctx["harmonics"].items():
            note = ""
            if h == 5:
                note = " (predicted ~2%)"
            elif h % 2 == 0:
                note = " (design predicts ~0%, half-wave symmetric)"
            harmonics_rows += f"<tr><td>{h}th</td><td>{100 * ratio:.2f}%{note}</td></tr>\n"

    segments_rows = ""
    for start, end, active in ctx["segments"]:
        label = "active" if active else "flat"
        segments_rows += (
            f"<tr><td>{format_hms(start)}–{format_hms(end)}</td><td>{end - start}s</td><td>{label}</td></tr>\n"
        )

    data = {
        "t_phases": t_s.tolist(),
        "ch0": w0.tolist(),
        "ch1": w1.tolist(),
        "ch2": w2.tolist(),
        "alpha": alpha.tolist(),
        "beta": beta.tolist(),
        "t_theta": t_theta.tolist(),
        "theta": theta_u.tolist(),
        "colors_light": CHANNEL_COLORS_LIGHT,
        "colors_dark": CHANNEL_COLORS_DARK,
    }

    html = f"""<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>ldc1314 trace analysis -- {ctx['capture_name']}</title>
<script src="https://cdn.plot.ly/plotly-2.35.2.min.js" charset="utf-8"></script>
<style>
  :root {{
    color-scheme: light;
    --surface-1: #fcfcfb;
    --page: #f9f9f7;
    --text-primary: #0b0b0b;
    --text-secondary: #52514e;
    --muted: #898781;
    --grid: #e1e0d9;
    --border: rgba(11,11,11,0.10);
  }}
  @media (prefers-color-scheme: dark) {{
    :root:not([data-theme="light"]) {{
      color-scheme: dark;
      --surface-1: #1a1a19;
      --page: #0d0d0d;
      --text-primary: #ffffff;
      --text-secondary: #c3c2b7;
      --muted: #898781;
      --grid: #2c2c2a;
      --border: rgba(255,255,255,0.10);
    }}
  }}
  * {{ box-sizing: border-box; }}
  body {{
    margin: 0; padding: 32px;
    background: var(--page); color: var(--text-primary);
    font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
  }}
  h1 {{ font-size: 1.3rem; margin: 0 0 4px; }}
  .subtitle {{ color: var(--text-secondary); margin: 0 0 24px; font-size: 0.9rem; }}
  .stats {{
    display: grid; grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
    gap: 12px; margin-bottom: 28px;
  }}
  .stat {{
    background: var(--surface-1); border: 1px solid var(--border); border-radius: 10px;
    padding: 14px 16px;
  }}
  .stat .label {{ color: var(--text-secondary); font-size: 0.78rem; margin-bottom: 4px; }}
  .stat .value {{ font-size: 1.35rem; font-weight: 600; }}
  .card {{
    background: var(--surface-1); border: 1px solid var(--border); border-radius: 12px;
    padding: 16px; margin-bottom: 20px;
  }}
  .card h2 {{ font-size: 1rem; margin: 0 0 4px; }}
  .card .note {{ color: var(--text-secondary); font-size: 0.82rem; margin: 0 0 12px; }}
  .row {{ display: flex; gap: 20px; flex-wrap: wrap; }}
  .row .card {{ flex: 1 1 380px; }}
  table {{ border-collapse: collapse; width: 100%; font-size: 0.85rem; }}
  th, td {{ text-align: left; padding: 6px 10px; border-bottom: 1px solid var(--grid); }}
  th {{ color: var(--text-secondary); font-weight: 500; }}
</style>
</head>
<body>
<h1>ldc1314 raw-trace analysis</h1>
<p class="subtitle">{ctx['capture_name']} &mdash; window {ctx['window_label']} &mdash; {ctx['n_samples']} samples,
  {ctx['duration']:.1f}s, {ctx['sample_rate']:.1f} samples/s</p>

<div class="stats">
  <div class="stat"><div class="label">Net rotation</div><div class="value">{ctx['revs']:+.2f} rev</div></div>
  <div class="stat"><div class="label">Direction (this window)</div><div class="value">{ctx['sign']} &theta;</div></div>
  <div class="stat"><div class="label">Locus circularity (CV)</div><div class="value">{100 * ctx['r_std'] / ctx['r_mean']:.1f}%</div></div>
  <div class="stat"><div class="label">ch1 phase vs ch0</div><div class="value">{ctx['phase1_deg']:.1f}&deg;</div></div>
  <div class="stat"><div class="label">ch2 phase vs ch0</div><div class="value">{ctx['phase2_deg']:.1f}&deg;</div></div>
</div>

<div class="card">
  <h2>Three phases &mdash; disc movement under the coils</h2>
  <p class="note">Raw 12-bit conversion code per channel over time. Pan/zoom/hover; drag to zoom, double-click to reset.</p>
  <div id="phases-chart" style="height:420px;"></div>
</div>

<div class="row">
  <div class="card">
    <h2>&alpha;/&beta; Lissajous locus</h2>
    <p class="note">Ideal Clarke transform of the three phases. A clean circle confirms ~120&deg; coil spacing.</p>
    <div id="locus-chart" style="height:420px;"></div>
  </div>
  <div class="card">
    <h2>Accumulated angle</h2>
    <p class="note">Unwrapped &theta;(t) &mdash; net slope gives rotation direction and revolution count.</p>
    <div id="angle-chart" style="height:420px;"></div>
  </div>
</div>

<div class="row">
  <div class="card">
    <h2>Segments (auto-detected)</h2>
    <table><thead><tr><th>Window</th><th>Duration</th><th>Activity</th></tr></thead>
    <tbody>{segments_rows}</tbody></table>
  </div>
  <div class="card">
    <h2>Harmonic content (ch0, ratio to fundamental)</h2>
    <table><thead><tr><th>Harmonic</th><th>Ratio</th></tr></thead>
    <tbody>{harmonics_rows or '<tr><td colspan="2">window too short (&lt;1 revolution)</td></tr>'}</tbody></table>
  </div>
</div>

<script>
const DATA = {json.dumps(data)};

function theme() {{
  const dark = window.matchMedia('(prefers-color-scheme: dark)').matches;
  return {{
    dark,
    colors: dark ? DATA.colors_dark : DATA.colors_light,
    paper: dark ? '#1a1a19' : '#fcfcfb',
    text: dark ? '#ffffff' : '#0b0b0b',
    muted: '#898781',
    grid: dark ? '#2c2c2a' : '#e1e0d9',
  }};
}}

function baseLayout(t, xlabel, ylabel) {{
  const th = theme();
  return {{
    title: {{text: '', font: {{size: 1}}}},
    margin: {{l: 56, r: 20, t: 10, b: 44}},
    paper_bgcolor: th.paper,
    plot_bgcolor: th.paper,
    font: {{color: th.text, family: 'system-ui, -apple-system, "Segoe UI", sans-serif', size: 12}},
    xaxis: {{title: xlabel, gridcolor: th.grid, zeroline: false, linecolor: th.grid}},
    yaxis: {{title: ylabel, gridcolor: th.grid, zeroline: false, linecolor: th.grid}},
    legend: {{orientation: 'h', y: 1.12}},
    hovermode: 'x unified',
  }};
}}

function renderAll() {{
  const th = theme();
  const config = {{responsive: true, displaylogo: false}};

  Plotly.newPlot('phases-chart', [
    {{x: DATA.t_phases, y: DATA.ch0, name: 'ch0', mode: 'lines', line: {{color: th.colors[0], width: 2}}}},
    {{x: DATA.t_phases, y: DATA.ch1, name: 'ch1', mode: 'lines', line: {{color: th.colors[1], width: 2}}}},
    {{x: DATA.t_phases, y: DATA.ch2, name: 'ch2', mode: 'lines', line: {{color: th.colors[2], width: 2}}}},
  ], baseLayout(th, 'time (s)', 'raw code'), config);

  Plotly.newPlot('locus-chart', [
    {{x: DATA.alpha, y: DATA.beta, mode: 'lines', line: {{color: th.colors[0], width: 1.5}}, showlegend: false}},
  ], Object.assign(baseLayout(th, 'alpha', 'beta'), {{yaxis: Object.assign(baseLayout(th,'','').yaxis, {{scaleanchor: 'x'}})}}), config);

  Plotly.newPlot('angle-chart', [
    {{x: DATA.t_theta, y: DATA.theta, mode: 'lines', line: {{color: th.colors[0], width: 2}}, showlegend: false}},
  ], baseLayout(th, 'time (s)', 'theta (rad)'), config);
}}

renderAll();
window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', renderAll);
</script>
</body>
</html>
"""
    (out_dir / "report.html").write_text(html)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("capture", type=Path)
    ap.add_argument("--window", nargs=2, metavar=("START", "END"), help="HH:MM:SS HH:MM:SS")
    ap.add_argument("--out-dir", type=Path, default=None)
    args = ap.parse_args()

    out_dir = args.out_dir or (args.capture.parent / (args.capture.stem + "_analysis"))
    out_dir.mkdir(parents=True, exist_ok=True)

    wall, millis, c0, c1, c2 = parse_trace(args.capture)
    print(f"parsed {len(millis)} rows from {args.capture}")

    segments = auto_segments(wall, c0, c1, c2)
    print("\nauto-detected segments (activity threshold: 5 codes range per 5s bucket):")
    for start, end, active in segments:
        label = "active" if active else "flat"
        print(f"  {format_hms(start)}-{format_hms(end)}  ({end - start:4d}s)  {label}")

    if args.window:
        start_h, end_h = args.window
        h, m, s = (int(x) for x in start_h.split(":"))
        start_sec = h * 3600 + m * 60 + s
        h, m, s = (int(x) for x in end_h.split(":"))
        end_sec = h * 3600 + m * 60 + s
    else:
        active_runs = [(s, e) for s, e, a in segments if a]
        if not active_runs:
            raise SystemExit("no active segment auto-detected -- pass --window explicitly")
        start_sec, end_sec = max(active_runs, key=lambda r: r[1] - r[0])
        print(f"\nno --window given -- using longest active segment: {format_hms(start_sec)}-{format_hms(end_sec)}")

    mask = np.array([w is not None and start_sec <= w <= end_sec for w in wall])
    n = mask.sum()
    if n < 10:
        raise SystemExit(f"only {n} rows in the requested window -- check --window bounds")
    t_ms, w0, w1, w2 = millis[mask], c0[mask], c1[mask], c2[mask]
    t_s = (t_ms - t_ms[0]) / 1000.0
    print(f"analyzing {n} rows, {t_s[-1]:.1f}s, {n / t_s[-1]:.1f} samples/s")

    mid0, amp0 = envelope(w0)
    mid1, amp1 = envelope(w1)
    mid2, amp2 = envelope(w2)
    mid = (mid0, mid1, mid2)
    amp = (amp0, amp1, amp2)
    print(f"\nper-channel envelope (this window): "
          f"ch0 mid={mid0:.1f} amp={amp0:.1f}  ch1 mid={mid1:.1f} amp={amp1:.1f}  ch2 mid={mid2:.1f} amp={amp2:.1f}")

    alpha, beta, theta_boot = ideal_clarke(w0, w1, w2, mid, amp)
    theta_unwrapped = unwrap_angle(theta_boot)

    # --- circularity: is the locus close to a circle (ideal 120 deg, balanced amplitude)? ---
    r = np.hypot(alpha, beta)
    r_mean, r_std = r.mean(), r.std()
    print(f"\nlocus circularity: r mean={r_mean:.3f} std={r_std:.3f} "
          f"(coefficient of variation {100 * r_std / r_mean:.1f}%)")
    if r_std / r_mean < 0.15:
        print("  -> reasonable circle: proceed with Clarke as planned (.plan decision gate)")
    else:
        print("  -> distorted locus: consider the ellipse-fit fallback noted in .plan")

    # --- real inter-coil phase angles and amplitudes, fit against the bootstrap angle ---
    x0 = (w0 - mid0) / amp0
    x1 = (w1 - mid1) / amp1
    x2 = (w2 - mid2) / amp2
    fit_amp0, fit_phi0 = fit_channel_phase(theta_boot, x0)
    fit_amp1, fit_phi1 = fit_channel_phase(theta_boot, x1)
    fit_amp2, fit_phi2 = fit_channel_phase(theta_boot, x2)

    def deg(rad):
        return np.degrees(rad) % 360

    print("\nfitted per-channel amplitude and phase (relative to channel 0):")
    print(f"  ch0: amplitude {fit_amp0:.3f} (ref)")
    print(f"  ch1: amplitude {fit_amp1:.3f}  phase {deg(fit_phi1 - fit_phi0):.1f} deg  (ideal: 120 or 240)")
    print(f"  ch2: amplitude {fit_amp2:.3f}  phase {deg(fit_phi2 - fit_phi0):.1f} deg  (ideal: 120 or 240)")

    # --- rotation sign over this window ---
    net_rad = theta_unwrapped[-1] - theta_unwrapped[0]
    revs = net_rad / (2 * np.pi)
    sign = "increasing" if net_rad > 0 else "decreasing"
    print(f"\nnet rotation this window: {revs:+.2f} revolutions, theta {sign} "
          f"-> if this window is known-forward flow, forward = {sign} theta with channel order (0,1,2)")

    # --- harmonic content on one representative channel, in the angle domain (see
    # harmonic_content()'s docstring for why time-domain FFT gives nonsense here) ---
    result = harmonic_content(theta_unwrapped, x0)
    ratios = None
    if not result:
        print("\nharmonic content: window covers less than one full revolution, skipping")
    else:
        ratios, n_revs_used = result
        fifth = ratios.get(5)
        print(f"\nharmonic content (channel 0, ratio to fundamental, averaged over {n_revs_used} revolutions):")
        for h, ratio in ratios.items():
            flag = "  <-- check vs ~2% predicted (sensor_head.md)" if h == 5 else ""
            flag += "  <-- even harmonic; design predicts ~0% (half-wave symmetric coverage)" if h % 2 == 0 else ""
            print(f"  {h}th: {100 * ratio:.2f}%{flag}")
        if fifth is not None:
            print(f"\n5th harmonic vs sensor_head.md prediction: measured {100 * fifth:.2f}%, predicted ~2%"
                  f" -- {'consistent' if abs(fifth - 0.02) < 0.015 else 'MISMATCH, coil geometry estimate may be off'}")

    # --- plots ---
    fig, ax = plt.subplots(figsize=(10, 4))
    ax.plot(t_s, w0, label="ch0", linewidth=0.8)
    ax.plot(t_s, w1, label="ch1", linewidth=0.8)
    ax.plot(t_s, w2, label="ch2", linewidth=0.8)
    ax.set_xlabel("time (s)")
    ax.set_ylabel("raw code")
    ax.set_title(f"three phases -- {args.capture.name} [{format_hms(start_sec)}-{format_hms(end_sec)}]")
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_dir / "phases.png", dpi=120)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(6, 6))
    ax.plot(alpha, beta, linewidth=0.5)
    ax.set_xlabel("alpha")
    ax.set_ylabel("beta")
    ax.set_aspect("equal")
    ax.set_title(f"alpha/beta Lissajous locus (CV={100 * r_std / r_mean:.1f}%)")
    fig.tight_layout()
    fig.savefig(out_dir / "locus.png", dpi=120)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(10, 4))
    ax.plot(t_s, theta_unwrapped)
    ax.set_xlabel("time (s)")
    ax.set_ylabel("unwrapped theta (rad)")
    ax.set_title(f"accumulated angle -- {revs:+.2f} revolutions")
    fig.tight_layout()
    fig.savefig(out_dir / "angle.png", dpi=120)
    plt.close(fig)

    write_html_report(out_dir, {
        "capture_name": args.capture.name,
        "window_label": f"{format_hms(start_sec)}-{format_hms(end_sec)}",
        "n_samples": n,
        "duration": t_s[-1],
        "sample_rate": n / t_s[-1],
        "t_s": t_s, "w0": w0, "w1": w1, "w2": w2,
        "alpha": alpha, "beta": beta, "theta_unwrapped": theta_unwrapped,
        "revs": revs, "sign": sign,
        "r_mean": r_mean, "r_std": r_std,
        "phase1_deg": deg(fit_phi1 - fit_phi0), "phase2_deg": deg(fit_phi2 - fit_phi0),
        "harmonics": ratios,
        "segments": segments,
    })

    print(f"\nplots written to {out_dir}/")
    print(f"interactive report: {out_dir / 'report.html'}")


if __name__ == "__main__":
    main()
