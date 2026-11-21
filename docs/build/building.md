# Building PixelCSS with MSVC

PixelCSS must be built with the Microsoft Visual C++ compiler (`cl.exe`, FR-042). CMake generates a Visual Studio
solution; GCC, MinGW, Clang and clang-cl are rejected at configure time by `cmake/MsvcGuard.cmake`.

## Prerequisites

| Component | Minimum | Reference machine |
|---|---|---|
| Windows | 10 x64 | 11 x64 |
| Visual Studio or Build Tools | 2022 17.10 (MSVC v143) | 2026 Build Tools 18.8 (MSVC 14.51, v145) |
| Workload | `Microsoft.VisualStudio.Workload.NativeDesktop` (VS) or `Microsoft.VisualStudio.Workload.VCTools` (Build Tools) | VCTools |
| Windows SDK | `Microsoft.VisualStudio.Component.Windows11SDK.26100` | 10.0.26100.0 |
| CMake | 3.28 | 4.4.2 |

Install from the command line, for example:

```powershell
winget install Microsoft.VisualStudio.2022.BuildTools --override "--add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --includeRecommended --passive"
```

No network access is needed after cloning: the only third-party code (doctest, tests only) is vendored.

## Presets

| Preset | Generator | Use |
|---|---|---|
| `x64-debug`, `x64-release` | Visual Studio 18 2026 | default |
| `vs2022-x64-debug`, `vs2022-x64-release` | Visual Studio 17 2022 | VS 2022 machines |
| test `x64-release-perf` | — | performance gate (`perf` label) |
| test `x64-release-package` | — | ZIP layout check (`package` label) |

## Clean build

From a Developer PowerShell (or any PowerShell where `cmake` is on PATH):

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug

cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release
cpack --preset x64-release          # out/package/PixelCSS-<version>-win64.zip
```

Or run everything, including the package check, with `scripts/build-all.ps1 -Force` (add `-Perf` for the
performance gate).

Outputs:

- `out/build/<preset>/src/shell/<Config>/PixelCSS.exe` — the application (font packs are copied next to it)
- `out/build/<preset>/tools/pixelcss-tool/<Config>/pixelcss-tool.exe` — developer CLI
- `out/build/<preset>/tools/pixelcss-bench/<Config>/pixelcss-bench.exe` — benchmarks

## Test labels

| Label | What |
|---|---|
| `unit` | module internals (tokenizer, codecs, framebuffer, raster cache, dirty regions) |
| `contract` | each public module header (glyph CSS, document, layout, editor, fonts, preferences) |
| `integration` | editor flows, files, font switching, incremental paint oracle, binary imports, resource cleanup |
| `visual` | pixel-exact golden images (`tests/golden`, regenerate with `PIXELCSS_UPDATE_GOLDENS=1`) |
| `perf` | `pixelcss-bench` budgets/regressions (absolute budgets only with `PIXELCSS_REFERENCE_MACHINE=1`) |
| `package` | CPack ZIP layout |

Debug test runs also execute the CRT leak-check listener (`tests/support/leak_check.cpp`).
