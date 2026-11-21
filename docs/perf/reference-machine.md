# Reference machine and performance results

The absolute performance budgets of research R-17 (spec SC-009, SC-010) are defined for this machine. The perf gate
(`ctest --preset x64-release-perf`) enforces them only when `PIXELCSS_REFERENCE_MACHINE=1`; on other machines it
checks for regressions (> 20 %) against `tests/perf/baseline.json` and for resource growth.

## Machine

| Item | Value |
|---|---|
| CPU | Intel Core Ultra 7 258V (8 cores / 8 threads) |
| RAM | 31.5 GB |
| GPU | Intel Arc 140V (not used: PixelCSS rasterizes on the CPU) |
| OS | Windows 11 Pro 10.0.26300 (build 26300), x64 |
| Display scaling | 150 % (document scale 2) |
| Toolchain | Visual Studio 2026 Build Tools 18.8, MSVC 19.51.36252 (toolset 14.51.36231, v145) |
| Windows SDK | 10.0.26100.0 |
| CMake | 4.4.2, generator "Visual Studio 18 2026", x64 Release, static CRT |

## Results (2026-11-21, `pixelcss-bench`, Release)

| Metric | Result | Budget (R-17) | Status |
|---|---|---|---|
| **Keystroke-to-present p95** (end to end through the real window, 497 keystrokes, `typing.pxs`) | **1.58 ms** | ≤ 50 ms (SC-009) | ✅ |
| Keystroke-to-present p50 | 1.08 ms | — | |
| **First viewport, 100k-character document** (decode → layout of visible lines → paint) | **1.40 ms** | ≤ 1 000 ms (SC-010) | ✅ |
| Startup (process creation → first present, median of 10) | 52 ms | ≤ 750 ms | ✅ |
| Raster miss p95 (one glyph, scale 2) | 1.4 µs | ≤ 20 µs | ✅ |
| Non-reflowing edit layout p95 | 0.027 ms | ≤ 1 ms | ✅ |
| Long single-line (100k chars) keystroke layout p95 | 2.76 ms | ≤ 5 ms | ✅ |
| Long single-line keystroke-to-present p95 (end to end) | 3.11 ms | — | |
| Glyph-cache hit rate (steady state: typing + scrolling) | 1.000 | ≥ 0.99 | ✅ |
| Idle memory, 100k-character document | 12.3 MB | ≤ 64 MB | ✅ |
| Idle memory, 10M-character document (layout complete) | 66.9 MB | ≤ 256 MB | ✅ |
| GDI / USER / handle growth over 50 open–font–wrap–resize–new cycles | 0 / 0 / 0 | 0 | ✅ |
| Stable keystroke (edit + paint) p95, headless | 0.035 ms | — | |
| Reflowing keystroke p95, headless | 0.052 ms | — | |
| Scroll frame p95 (half-page, pixel shift + exposed band) | 0.42 ms | — | |
| Font switch p95 (relayout + full repaint, 1280×800) | 1.02 ms | — | |
| Text-buffer edit p95 at 100k / 1M characters | 2.8 µs / 4.0 µs | < 1 ms (ADR 0001) | ✅ |

## Reproducing

```powershell
cmake --preset x64-release; cmake --build --preset x64-release
$env:PIXELCSS_REFERENCE_MACHINE = "1"
ctest --preset x64-release-perf -V          # writes out/build/x64-release/perf-results.json
```

A regression is reported only when a metric is more than 20 % worse than the baseline **and** the difference
exceeds an absolute noise floor (timer granularity and OS scheduling noise): 0.5 ms for `*Ms` metrics, 25 ms for
`startupMs` (dominated by process creation), 2 µs for `*Us` metrics and 4 MB for `*MB` metrics. Small-sample p95
values otherwise flap by tens of percent between identical runs.

To re-baseline after an intentional change, copy the metrics from `perf-results.json` into
`tests/perf/baseline.json` (keep `"measured": true`) and record the new numbers here.
