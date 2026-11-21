# Quickstart & Validation Guide: PixelCSS Browser

**Feature**: [spec.md](./spec.md) | **Plan**: [plan.md](./plan.md)

This guide proves the feature works end to end. Formats and behaviours are defined in [contracts/](./contracts/) and [data-model.md](./data-model.md) and are not repeated here.

## Prerequisites

- Windows 11 x64 (Windows 10 x64 should also work).
- Visual Studio 2022 17.10+ **or** Visual Studio 2026 / Build Tools 18.x with the **Desktop development with C++** workload. Reference toolset: MSVC 14.51.
- Windows SDK 10.0.26100.0.
- CMake ≥ 3.28, on PATH or the copy bundled with VS.
- No network access is required after cloning. All dependencies are vendored.

Run everything from a **Developer PowerShell for VS** at the repository root.

## 1. Configure, build, test (US7, SC-001)

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug                 # unit + contract + integration + visual
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release
```

**Expected**: both builds succeed with zero warnings (`/W4 /WX`). The configure log prints `PixelCSS: compiler = MSVC <version> (cl.exe)`. Configuring with clang-cl or MinGW fails with a clear error (FR-042). All CTest labels `unit`, `contract`, `integration` and `visual` pass.

**Debugging**: open `out/build/x64-debug/PixelCSS.sln`, set `PixelCSS` as the startup project, and press F5. The app launches under the debugger.

## 2. Validate the font packs (SC-002, US6-AS1/AS5)

```powershell
.\out\build\x64-release\Release\pixelcss-tool.exe validate --strict fonts
.\out\build\x64-release\Release\pixelcss-tool.exe distinct fonts
```

**Expected**:
- `validate` reports `verdana-style`, `arial-narrow-style` and `comic-style` as **Valid**, each with `95/95 ASCII + U+FFFD`, for 285 ASCII glyph files in total. Each pack's provenance fields are printed and the exit code is 0.
- `distinct` reports every pack pair below the similarity threshold and exits with 0.

## 3. Render one glyph from its CSS file (US1)

```powershell
$t = ".\out\build\x64-release\Release\pixelcss-tool.exe"
& $t render --pack verdana-style --text "A" --scale 1 --out $env:TEMP\A.bmp
& $t compare $env:TEMP\A.bmp tests\golden\glyphs\verdana-style\U+0041.s1.bmp
```

**Expected**: `compare` exits with 0 (pixel-exact). Edit one cell in a *copy* of a glyph file so its row has the wrong length, then run `validate` on that copy. It reports `E-GLYPH-ROW-LENGTH` with `file:line:column`, and the tool exits with 1 without crashing.

## 4. Editor round-trip (US2, SC-005)

1. Launch `PixelCSS.exe`. The toolbar shows **Verdana**.
2. Type two lines, use arrows, Home/End and Shift-selection, then press Ctrl+X and Ctrl+V. Press Ctrl+Z three times and Ctrl+Y once.
3. Copy text, paste it into Windows Notepad and back. The text is identical.
4. Save as `%TEMP%\rt.txt`, close the app, then reopen the file. The contents are identical. Run `fc /b` against a copy saved before reopening to confirm byte equality.
5. Modify the document and press Ctrl+N. The **Save / Don't Save / Cancel** prompt appears.

Automated equivalent: `ctest --preset x64-release -L integration -R editor`.

## 5. Font switching preserves positions (US3, SC-004)

1. Type `Aa0!? Hello World`, select `Hello`, and place the caret after it.
2. Switch the dropdown to Arial Narrow, then Comic Sans, then back to Verdana.

**Expected**: the glyph shapes and line widths change visibly. The status bar's Ln/Col and the selected text stay unchanged. The tooltip and **Help → About Fonts** say "style interpretation — not the original typeface".
Automated: `ctest -L visual -R font_switch` compares three goldens and asserts the caret and selection offsets.

## 6. Incremental rendering (US4, SC-006, SC-009, SC-010)

```powershell
.\out\build\x64-release\Release\pixelcss-bench.exe --doc tests\fixtures\perf\100k.txt --out $env:TEMP\bench.json
```

**Expected** (on the reference machine, see `docs/perf/reference-machine.md`):
- `firstViewportMs ≤ 1000` and `startupMs ≤ 750`.
- `keyToPresentP95Ms ≤ 50`, measured end to end through `PixelCSS.exe --bench-script tests/perf/scripts/typing.pxs`.
- `longLineKeystrokeLayoutP95Ms ≤ 5` and `cacheHitRate ≥ 0.99`.
- For the "stable keystroke" scenario, `rasterizations ≤ 1`, `linesLaidOut = 1`, and `dirtyPixelArea` stays below 2 rows' area.
- The `resource-cycles` scenario reports zero growth in GDI objects, USER objects and handles.

In the app, press **F12** to show the live counters while typing.

## 7. Restricted HTML preview (US5, SC-007)

Open `tests\fixtures\html\basic.html` and `tests\fixtures\html\unsupported.html` via **File → Open**.

**Expected**: a read-only preview rendered with pack glyphs. For `unsupported.html`, no script runs and no network traffic occurs (verify with Resource Monitor → Network, or the `no_network_imports` test). Diagnostics list the ignored `script`, `onclick`, `img` and `link`.

## 8. Failure handling (SC-008)

Run `ctest -L integration -R failure`. It covers a missing pack directory, a corrupted glyph, a locked file on save (sharing violation) and a read-only target. Each one shows a readable message, the process keeps running, and the document is preserved.

## 9. Package (FR-048, SC-011)

```powershell
cpack --preset x64-release        # produces out/package/PixelCSS-<ver>-win64.zip
```

Extract the ZIP on a clean Windows 10/11 x64 VM with no VC++ Redistributable and no network, then launch `PixelCSS.exe`. It runs and all three fonts render.

## 10. Add a font pack (US6-AS3, SC-012)

Copy `fonts\verdana-style` to `fonts\my-pack`, then set `--id: "my-pack"` and `--display-name: "My Pack"` in its `manifest.css`. Change one glyph and run `pixelcss-tool validate fonts\my-pack`, then restart the app. **My Pack** appears in the dropdown, with no engine rebuild needed. See `docs/fonts/adding-a-pack.md`.
