# Debugging PixelCSS in Visual Studio

1. Configure once: `cmake --preset x64-debug`.
2. Open `out/build/x64-debug/PixelCSS.sln` (or `.slnx`) in Visual Studio 2022/2026.
3. `PixelCSS` is the startup project (set by CMake via `VS_STARTUP_PROJECT`); its debugger working directory is the
   executable folder, which contains the copied `fonts/` packs.
4. Press **F5**. Breakpoints in any engine module (`src/core` … `src/html`) work because every module is a static
   library linked into `PixelCSS.exe`.

Useful command-line arguments (Project → Properties → Debugging → Command Arguments):

| Argument | Effect |
|---|---|
| `<file>` | open a `.txt` file, or a `.html` file in read-only preview |
| `--fonts-dir <dir>` | also load font packs from `<dir>` (e.g. a pack you are designing) |
| `--no-preferences` | ignore and do not write `HKCU\Software\PixelCSS\Display` |
| `--stats-json <file>` | write per-frame render statistics and resource samples on exit |
| `--bench-script <file>` | drive the UI from a script (see `contracts/ui-contract.md`) |
| `--exit-after-first-present` | close right after the first frame (startup timing) |

In the running app press **F12** for the render-statistics overlay (cache hits, rasterizations, lines laid out,
dirty area, paint time, keystroke-to-present), drawn with the active pack's glyphs. **Help → Diagnostics** lists
every font/glyph/file problem with its file, line and column.

Debugging tests: each test executable under `out/build/x64-debug/tests/*/Debug/` is a doctest binary; run it with
`-tc="<test case name>"` to focus one case, or set it as the startup project in Visual Studio.
