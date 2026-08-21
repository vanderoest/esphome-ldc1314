# tools/

Offline analysis of raw-trace captures (`TRACE,<millis>,<ch0>,<ch1>,<ch2>` lines, produced by the
`switch:` platform's trace toggle -- see README "Raw trace capture"). Settles the Phase A
questions in `TODO.md`: is the alpha/beta locus a clean circle, what are the real inter-coil
phase angles, which way does forward flow rotate theta, and how much harmonic content is
actually present.

## Setup

```bash
cd tools
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

## Usage

Run with the venv's Python, not the system one -- `matplotlib`/`numpy` are only installed there:

```bash
tools/.venv/bin/python3 tools/analyze_trace.py captures/your-capture.csv
```

Add `--window HH:MM:SS HH:MM:SS` to target a specific segment once you know its bounds from the
auto-detected segment table the script prints; with no `--window`, it picks the longest
continuous "active" segment automatically (typically the bucket draw). Plots (three-phase,
alpha/beta locus, unwrapped angle) are written to `<capture>_analysis/` next to the input file.
