# Release Checklist: PixelCSS Browser v0.1.0

**Date**: 2026-11-21 | **Build**: `scripts/build-all.ps1 -Force -Perf` from a clean `out/` | **Feature**: [spec.md](../spec.md)

## Quickstart validation (quickstart.md §1–§10)

- [x] **§1 Configure, build, test** — clean `build-all.ps1 -Force -Perf`: configure/build/test x64-debug and x64-release all OK under `/W4 /WX`; **160/160 tests pass in Debug and in Release**; configure log prints `PixelCSS: compiler = MSVC 19.51.36252.0 (cl.exe)`. Debug solution opens and runs under the debugger (`VS_STARTUP_PROJECT` = PixelCSS).
- [x] **§2 Validate the packs** — `pixelcss-tool validate --strict fonts`: verdana-style, arial-narrow-style, comic-style all **Valid, 95/95 ASCII + U+FFFD, 285 ASCII glyph files**, provenance complete. `distinct`: 9.5 % / 4.2 % / 0 % identical (≤ 60 %). (CTests `validate_bundled_fonts`, `fonts_distinct`.)
- [x] **§3 Render one glyph** — `tool_render_A` renders `A` and compares pixel-exactly with the golden; `tool_validate_invalid_glyph` reports `E-GLYPH-ROW-LENGTH` with file:line:column and exits 1.
- [x] **§4 Editor round-trip** — automated (`integration_files`: type → save → reopen identical bytes; New/Open/Exit prompt with Save / Don't Save / Cancel; failed save keeps content). Real app exercised: typing, Enter, Shift-selection, title `Untitled*` (screenshots during implementation). *Not performed manually:* the copy/paste round-trip with Windows Notepad (step 3) — `Win32Clipboard` uses `CF_UNICODETEXT`; clipboard logic is covered with a fake clipboard.
- [x] **§5 Font switching** — `integration_font_switch` (text, caret, anchor and document revision unchanged; Arial Narrow wraps to fewer rows than Verdana; per-pack goldens). Real app: dropdown switched to Comic Sans through the combo box, document re-rendered, selection preserved. About Fonts / tooltip disclose "style interpretation — not the original typeface".
- [x] **§6 Incremental rendering** — `integration_incremental` (stable keystroke: 1 line laid out, ≤ 1 rasterization, dirty area within the edited row; oracle: incremental == full repaint after 200 random operations) and `pixelcss-bench`: keystroke-to-present p95 **1.58 ms**, first viewport **1.40 ms**, startup **52 ms**, cache hit rate **1.0** (see [reference machine](../../../docs/perf/reference-machine.md)). F12 overlay verified live in the app.
- [x] **§7 HTML preview** — HTML fixtures render pixel-exactly (`visual_html`, `tool_html_basic`); real app opens `styles.html` read-only with `<title>`/filename + "Preview (read-only)", typing ignored, select-all/copy work. `unsupported.html`: script/`onclick`/`img`/`link`/`@import` reported and ignored. `integration_binary` proves the executable imports no WinHTTP/WinINet/Winsock/URLMon/WebView2/DirectWrite/Direct2D and no text-drawing functions.
- [x] **§8 Failure handling** — missing pack, corrupted glyph, locked file (sharing violation), read-only target, disk-full injection: readable messages, no crash, content preserved (`integration_files`, `contract_font_registry`, `integration_pack_reload`, `integration_binary` (c)).
- [~] **§9 Package** — `cpack` produces `out/package/PixelCSS-0.1.0-win64.zip`; `package_layout` verifies executables, 3 manifests + licenses, **288 glyph files**, README, no DLLs, no doctest. The ZIP was extracted into an empty folder and `PixelCSS.exe` started and presented its first frame; `pixelcss-tool validate --strict` passed from inside the package. *Not performed:* launching on a separate clean Windows VM without the developer toolchain (static CRT and the import scan indicate no extra runtime is needed).
- [x] **§10 Add a font pack** — a copy of verdana-style renamed `my-pack` passed `validate --strict` and appeared as the 4th dropdown entry via `--fonts-dir`, with no rebuild; `contract_pack_validation` registers `extra-pack` the same way.

## Constitution release gates (spec.md)

- [x] Custom CSS-defined document glyphs never fall back to OS text rendering — engine replacement glyph is generated in code; PE import scan forbids `DrawText*`/`TextOut*`/`ExtTextOut*`/DirectWrite.
- [x] Exactly one CSS file per glyph/code point/family/style, complete ASCII coverage for the three packs — `validate --strict`, `E-PACK-DUP-GLYPH` fixture.
- [x] Font switching is presentation-only and preserves all logical text positions — `integration_font_switch` (SC-004).
- [x] Progressive first paint, dirty regions, viewport-based layout and glyph caching are demonstrable — `integration_incremental`, `integration_large_document` (10M chars: 58 MB in Debug), F12 overlay, bench.
- [x] Application chrome and rendering internals are separated; HTML and CSS parsers independently testable — `render` links only `core`; separate `glyphcss` / `presentcss` / `html` contract tests.
- [x] MSVC `cl.exe`, C++20, Visual Studio-compatible Debug/Release builds — `MsvcGuard.cmake`, presets, clean build above.
- [x] Font assets have documented origin and permissible distribution rights — [licensing review](../../../docs/fonts/licensing-review.md): 288/288 original-artwork headers, OFL-1.1.
- [x] Tests include visual comparison, document correctness, MSVC build reproducibility and measurable performance — 160 tests + perf gate.
- [x] Ships as a native notepad-first browser-like renderer, not a general-purpose web browser — no networking or scripting code exists.

## Notes

- Visual goldens were regenerated only after review (ASCII/PNG previews of every new golden were inspected).
- `[~]` marks an item verified as far as possible on the development machine; the remaining manual step is listed.
