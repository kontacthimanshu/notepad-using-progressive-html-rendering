---

description: "Task list for PixelCSS Browser — Custom CSS Glyph Rendering and Notepad"
---

# Tasks: PixelCSS Browser — Custom CSS Glyph Rendering and Notepad

**Input**: Design documents from `/specs/001-pixelcss-browser/`

**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md), [data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md)

**Tests**: **Included.** The spec requires them: FR-047, FR-051, FR-052, SC-003 and Constitution XI. Within each story, write the test tasks first and confirm they fail before implementing.

**Organization**: Tasks are grouped by user story. P1 stories come first (US1 → US2 → US3 → US4 → US7), then the P2 stories (US5, US6).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on incomplete tasks)
- **[Story]**: The user story the task belongs to (US1–US7)
- Paths are relative to the repository root. Module layout follows [plan.md § Project Structure](./plan.md#project-structure).

## Global conventions (apply to every task)

- Each `src/<module>/` is a static library `pxc_<module>`. Public headers go in `src/<module>/include/pxc/<module>/` and sources in `src/<module>/`. Namespace is `pxc::<module>`.
- Each module's `CMakeLists.txt` calls `pxc_set_warnings(<target>)` from `cmake/CompilerWarnings.cmake`. Every task that creates or changes a module must keep the module compiling under `/W4 /WX`.
- Among production code, only `src/shell/`, `src/storage/win32/` and `tools/*/main.cpp` may include `<windows.h>`. The engine modules `src/core`, `src/glyphcss`, `src/render`, `src/fonts`, `src/document`, `src/storage` (portable part), `src/layout`, `src/update`, `src/editor`, `src/presentcss` and `src/html` must never include it. Test code may include it only where it exercises Win32 behaviour directly: `tests/integration/**`, `tests/contract/test_preferences.cpp` and `tests/support/leak_check.cpp` (`<crtdbg.h>`). Never call `DrawText*`, `TextOut*`, `ExtTextOut*`, DirectWrite, or create EDIT/RichEdit controls for document content.
- Errors cross module boundaries as `pxc::core::Expected<T, E>` or `Diagnostic`. Exceptions must not escape a public function.
- Register tests with `pxc_add_test(<name> LABEL <unit|contract|integration|visual|perf> SOURCES …)` from `tests/CMakeLists.txt`.
- Goldens are pixel-exact 32-bit BMPs under `tests/golden/`. Set `PIXELCSS_UPDATE_GOLDENS=1` to regenerate them, and review every regenerated image by eye before committing it.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create a repository skeleton that configures and builds with MSVC.

- [X] T001 Create the directory skeleton from plan.md: `src/{core,glyphcss,render,fonts,document,storage/win32,layout,update,editor,presentcss,html,shell}`, `tools/{pixelcss-tool,pixelcss-bench}`, `fonts/`, `tests/{unit,contract,integration,visual,perf,fixtures,golden,support}`, `docs/{build,fonts,perf,adr}`, `third_party/`, `cmake/`. Add an empty `CMakeLists.txt` to each `src/<module>` and `tools/<tool>` directory.
- [X] T002 Write the root `CMakeLists.txt`. It needs `cmake_minimum_required(VERSION 3.28)` and `project(PixelCSS VERSION 0.1.0 LANGUAGES CXX)`. Set `CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON` and `CMAKE_CXX_EXTENSIONS OFF`. Set `CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"` (static CRT, R-03). Include `cmake/MsvcGuard.cmake` and `cmake/CompilerWarnings.cmake`, call `enable_testing()`, and `add_subdirectory` for `src`, `tools` and `tests`. Create `src/CMakeLists.txt` and `tools/CMakeLists.txt` that add each subdirectory in dependency order (core, glyphcss, render, fonts, document, storage, layout, update, editor, presentcss, html, shell).
- [X] T003 [P] Create `cmake/MsvcGuard.cmake`. It stops with `FATAL_ERROR` unless `CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"` and `CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"`, and also when `CMAKE_SIZEOF_VOID_P` ≠ 8. It prints `PixelCSS: compiler = MSVC ${CMAKE_CXX_COMPILER_VERSION} (cl.exe)` (FR-042).
- [X] T004 [P] Create `cmake/CompilerWarnings.cmake` with `function(pxc_set_warnings target)`. It applies `/W4 /WX /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor /EHsc /sdl /guard:cf /external:anglebrackets /external:W0`. Release adds `/O2 /GL`, and the linker adds `/LTCG /guard:cf`. Leave a commented placeholder list for any documented `/wdNNNN` suppressions (R-02).
- [X] T005 Create `CMakePresets.json` (version 6) with these presets:
  - Configure presets `x64-debug` and `x64-release`: generator `Visual Studio 18 2026`, `architecture: x64`, `toolset: host=x64`, `binaryDir: ${sourceDir}/out/build/${presetName}`, cacheVariables `CMAKE_SYSTEM_VERSION=10.0.26100.0`.
  - Variants `vs2022-x64-debug` and `vs2022-x64-release` using `Visual Studio 17 2022`.
  - Build presets with `configuration: Debug` or `Release`.
  - Test presets with `output.outputOnFailure: true` and `filter.exclude.label: "perf|package"`. Also add test presets `x64-release-perf` (`filter.include.label: "perf"`) and `x64-release-package` (`filter.include.label: "package"`).
  - A package preset `x64-release` that uses CPack generator `ZIP`.
- [X] T006 [P] Vendor doctest 2.4.11: `third_party/doctest/doctest.h`, `third_party/doctest/LICENSE.txt`, and `third_party/doctest/doctest.cmake` (the upstream `scripts/cmake/doctest.cmake`). Add `third_party/CMakeLists.txt` that defines the INTERFACE target `pxc_doctest` with `SYSTEM` include dirs, and add it from the root CMakeLists.
- [X] T007 [P] Add `.gitignore` (`out/`, `.vs/`, `*.user`, `CMakeUserPresets.json`), `.editorconfig` (UTF-8, LF for `*.css`/`*.md`/`*.cmake`, 4-space C++ indent), and a `.clang-format` (formatting only, never compilation).
- [X] T008 Create `tests/CMakeLists.txt`. It includes `third_party/doctest/doctest.cmake` and defines `function(pxc_add_test name)` with arguments `LABEL` and `SOURCES`. That function builds an executable that links `pxc_doctest` and the shared `tests/test_main.cpp` (`#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`), passes the compile definitions `PXC_FIXTURES_DIR`, `PXC_GOLDEN_DIR` and `PXC_FONTS_DIR` (absolute source paths), and calls `doctest_discover_tests(... PROPERTIES LABELS <label>)`. Also add `add_subdirectory` for `unit`, `contract`, `integration`, `visual` and `perf`, each with its own CMakeLists.txt.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared core types, the CSS tokenizer, framebuffer, BMP I/O and the golden-test harness. Every story depends on these.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [X] T009 [P] Create `src/core/include/pxc/core/expected.hpp`. When `__cpp_lib_expected` is defined, alias `pxc::core::Expected<T,E>` to `std::expected` and `Unexpected` to `std::unexpected`. Otherwise provide a minimal polyfill with `has_value`, `operator bool`, `value`, `error`, `operator*`, `operator->` and a `void` specialisation.
- [X] T010 [P] Implement `src/core/include/pxc/core/diagnostic.hpp` and `src/core/diagnostic.cpp` per data-model §12:
  - `enum class Severity {Info, Warning, Error}` and `struct Diagnostic`.
  - `std::string format(const Diagnostic&)` producing `<path>:<line>:<column>: error <CODE>: <message>`.
  - `class DiagnosticsLog` with `add` (de-duplicated on `(code, file, line, column)`), `all()`, `clear()`, `errorCount()`.
- [X] T011 [P] Implement `src/core/include/pxc/core/geometry.hpp`:
  - `Rect{x,y,w,h}` with `empty()`, `area()`, `intersect`, `unite`, `contains`.
  - `Color{r,g,b,a}` with `static std::optional<Color> parseHex(std::string_view)`, accepting `#RRGGBB` and `#RRGGBBAA` case-insensitively, and `uint32_t toBGRA() const`.
- [X] T012 [P] Implement `src/core/include/pxc/core/unicode.hpp` and `src/core/unicode.cpp`:
  - `decodeUtf8(std::string_view, InvalidPolicy{Fail, Replace}) -> Expected<std::u32string, Utf8Error{byteOffset}>`, which rejects overlong forms, surrogates and values > 0x10FFFF.
  - `encodeUtf8(std::u32string_view)`.
  - `decodeUtf16(std::span<const char16_t>)` with surrogate-pair handling and replacement for lone surrogates.
  - `encodeUtf16(std::u32string_view)`.
- [X] T013 [P] Create `src/core/include/pxc/core/clock.hpp` with the interface `IClock { virtual std::chrono::microseconds now() const = 0; }`, `SteadyClock` (on `std::chrono::steady_clock`), and `ManualClock` for tests.
- [X] T014 Implement the shared CSS tokenizer in `src/core/include/pxc/core/css_tokenizer.hpp` and `src/core/css_tokenizer.cpp`. It is a CSS Syntax Level 3 subset that emits `{kind, text, line, column}` tokens. Kinds: Ident, String (both quote styles, with escapes), Hash, Number, Percentage, Dimension, UnicodeRange (`U+XXXX` and `U+XXXX-YYYY`), Delim, Colon, Semicolon, Comma, LeftBrace, RightBrace, LeftParen, RightParen, Function, Whitespace and EOF. Comments are skipped. Line numbers are tracked across LF and CRLF. An unterminated string or comment produces an `E-CSS-SYNTAX` diagnostic. This tokenizer is shared, but the glyph, manifest and presentation parsers stay separate (FR-041).
- [X] T015 Create `src/core/CMakeLists.txt` defining the static library `pxc_core` (headers in `include/`, sources from T010–T014) and apply `pxc_set_warnings`.
- [X] T016 [P] Write unit tests `tests/unit/core/test_unicode.cpp` (valid, overlong, surrogate, truncated and >U+10FFFF UTF-8; UTF-16 pairs and lone surrogates), `tests/unit/core/test_geometry.cpp` and `tests/unit/core/test_diagnostics.cpp`, and register them in `tests/unit/CMakeLists.txt` with label `unit`.
- [X] T017 [P] Write unit test `tests/unit/core/test_css_tokenizer.cpp` covering every token kind, comments between tokens, CRLF line and column tracking, unicode-range tokens, and unterminated string and comment diagnostics.
- [X] T018 [P] Implement `src/render/include/pxc/render/framebuffer.hpp` and `src/render/framebuffer.cpp`. `Framebuffer` holds 32-bit straight-alpha BGRA top-down pixels in a `std::vector<uint32_t>` and provides `resize(w,h)`, `width()`, `height()`, `row(y) -> std::span<uint32_t>`, `pixel(x,y)` and `fillRect(Rect, Color)` (clipped). The constructor and `resize` cap the pixel count at 16384×16384 and report an `Expected` error beyond that.
- [X] T019 [P] Implement `src/render/include/pxc/render/bmp_io.hpp` and `src/render/bmp_io.cpp`: `writeBmp32(const Framebuffer&, path)` and `readBmp32(path) -> Expected<Framebuffer, std::string>`. Write `BITMAPFILEHEADER` and `BITMAPINFOHEADER` by hand as packed structs with negative height (top-down) and `BI_RGB` 32 bpp, including alpha. No `<windows.h>`.
- [X] T020 Create `src/render/CMakeLists.txt` defining the static library `pxc_render`, which links **only** `pxc_core` and has sources from T018–T019. Later render tasks add files to it, but `pxc_render` must never link or include `pxc_glyphcss`, `pxc_fonts` or any document or text module (engine-interfaces dependency graph).
- [X] T021 Implement the golden harness in `tests/support/golden.hpp` and `tests/support/golden.cpp`. `CHECK_GOLDEN(fb, "relative/name.bmp")` compares the framebuffer pixel-exactly with `PXC_GOLDEN_DIR/relative/name.bmp`. When `PIXELCSS_UPDATE_GOLDENS=1` it writes the golden instead. On mismatch it writes `actual.bmp` and `diff.bmp` (mismatches in magenta) to `${CMAKE_BINARY_DIR}/golden-failures/` and reports the first differing pixel. Add it to the `pxc_add_test` link set as the static library `pxc_testsupport`.
- [X] T022 Implement the CRT leak-check listener in `tests/support/leak_check.cpp` (Constitution XI resource cleanup). In Debug builds only, register a doctest `IReporter` listener (`REGISTER_LISTENER("leakcheck", 1, LeakCheckListener)`) that takes `_CrtMemCheckpoint` in `test_case_start` and compares in `test_case_end` with `_CrtMemDifference`. If any normal-block bytes leaked, it fails the test with `_CrtMemDumpStatistics` output. Tests that intentionally keep static caches can opt out with the doctest decorator `* doctest::description("leakcheck:off")`. Link it into every `pxc_add_test` executable through `pxc_testsupport`.
- [X] T023 [P] Write unit test `tests/unit/render/test_framebuffer_bmp.cpp`: fill and clip behaviour, BMP write→read round-trip identical, and reading a truncated BMP returns an error.

**Checkpoint**: `cmake --preset x64-debug && cmake --build --preset x64-debug && ctest --preset x64-debug -L unit` passes.

---

## Phase 3: User Story 1 — Render a character entirely from its CSS glyph file (Priority: P1) 🎯 MVP

**Goal**: Parse a glyph CSS file ([contracts/glyph-css-schema.md](./contracts/glyph-css-schema.md)), rasterize it with our own engine, place glyphs by baseline and advance, and cache rasters. Ship the complete Verdana-style pack.

**Independent Test**: `pixelcss-tool render --pack verdana-style --text "A" --scale 1 --out A.bmp`, then `pixelcss-tool compare A.bmp tests/golden/glyphs/verdana-style/U+0041.s1.bmp`, exits with 0. A malformed glyph produces a `file:line:column` diagnostic without crashing (quickstart §3).

### Tests for User Story 1 ⚠️ (write first, must fail)

- [X] T024 [P] [US1] Create the valid glyph fixtures in `tests/fixtures/glyphs/valid/`:
  - `U+0041.css` (the contract example).
  - `U+0020.css` (width 0 or all-transparent, advance 4).
  - `multicolor.css`, mapping `--code-point: U+0042` with three literal colours.
  - `nonsquare.css` (5×9).
  - `transparent-rows.css` (first and last rows all `.`).
  - `alpha-zero.css` (uses `#FF000000`).
  - `mixed-case-hex.css`.
  - `crlf.css` (CRLF line endings).
  - `comments-between-rows.css`.
  - `long-tokens.css` (uses `transparent` and `currentcolor` instead of the aliases).
  - `bgcolor.css` (uses `~` and `backgroundcolor` cells as knock-outs inside `@` ink).
  - `meta.css` (has `--meta-designer: "x"; --meta-version: 2;`).

  Next to each fixture, add a `<name>.expected.txt` listing width, height, baseline, advance and bearing, plus the cells as a compact grid.
- [X] T025 [P] [US1] Create the invalid glyph fixtures in `tests/fixtures/glyphs/invalid/<CODE>/case1.css`, with at least one file per code: `E-GLYPH-SCHEMA`, `E-GLYPH-MISSING-PROP`, `E-GLYPH-DUP-PROP`, `E-GLYPH-BAD-DIM`, `E-GLYPH-ROW-COUNT`, `E-GLYPH-ROW-LENGTH`, `E-GLYPH-BAD-COLOR` (including `#GG0000` and `#FFF`), `E-GLYPH-CP-MISMATCH`, `E-GLYPH-FAMILY-MISMATCH`, `E-GLYPH-SYNTAX` (missing `}`). Each case gets a `case1.expected.txt` holding the expected `code line column`.
- [X] T026 [P] [US1] Write contract test `tests/contract/test_glyphcss.cpp`. It iterates over `PXC_FIXTURES_DIR/glyphs/valid` and asserts the parsed fields match `*.expected.txt`, and over `invalid/*` asserting the exact code, line and column. It also checks that unknown properties give `W-GLYPH-UNKNOWN-PROP` while still parsing successfully, that `contentHash` is identical for the LF and CRLF versions of the same glyph, and that no input (including random byte strings run through a 1,000-iteration seeded fuzz loop) throws or crashes. Register it with label `contract`.
- [X] T027 [P] [US1] Write unit test `tests/unit/render/test_blend.cpp` covering:
  - The source-over formula `(s·a + d·(255−a) + 127)/255` for specific vectors (a = 0, 1, 128, 254, 255).
  - Transparent cells leave the destination untouched.
  - `blitGlyph` clips at all four framebuffer edges and at an arbitrary clip rectangle.
  - The caret rectangle is drawn correctly.
- [X] T028 [P] [US1] Write visual test `tests/visual/test_glyph_render.cpp`, which renders through `pxc_fonts` and `pxc_render` and checks:
  - (a) `tests/fixtures/glyphs/valid/U+0041.css` at s=1 and s=2 against goldens `glyphs/fixture/U+0041.s1.bmp` and `.s2.bmp`.
  - (b) "AA" placed consecutively: the second glyph's origin is exactly `advance·s` to the right on the same baseline (asserted by pixel positions and a golden).
  - (c) `multicolor.css` and `alpha-zero.css` over a non-white background (transparency), and `bgcolor.css` over a `#FFEE88` background, where the knock-out cells must equal `#FFEE88` exactly (background substitution).
  - (d) a glyph clipped by a rectangle (FR-052 clipping).
  - (e) baseline alignment of a descender glyph next to a cap glyph.
  - (f) the canonical `renderTextLine` canvas: width `(Σadvance+2)·s`, height `lineHeight·s`, pen start `x = s`, baseline `ascent·s` (research R-21).

  Register it with label `visual`.
- [X] T029 [P] [US1] Write unit test `tests/unit/fonts/test_raster_cache.cpp`. The first `get` is a miss with `rasterizations == 1`, and the next 100 identical `get`s are hits with no new rasterization (US1-AS4). Changing the scale, fg or bg creates a new entry. The LRU evicts the oldest entry when its budget is exceeded, using a tiny test budget.

### Implementation for User Story 1

- [X] T030 [US1] Define `src/glyphcss/include/pxc/glyphcss/glyph_definition.hpp` per data-model §1: `enum class CellKind {Transparent, Literal, CurrentColor, BackgroundColor}`, `struct Cell{CellKind kind; Color rgba;}`, and `struct GlyphDefinition` (fields exactly as in data-model §1, including `metadata`, plus `cellAt(col,row)`).
- [X] T031 [US1] Implement `parseGlyph(std::string_view cssText, const std::filesystem::path& source)` in `src/glyphcss/glyph_parser.cpp` (header `src/glyphcss/include/pxc/glyphcss/glyph_parser.hpp`). It uses `core::CssTokenizer` and must:
  - Accept exactly one `glyph { … }` rule.
  - Parse every required and optional property from the contract tables.
  - Treat `--rows` as `height` String tokens and split each string on spaces into cell tokens (`#RRGGBB`, `#RRGGBBAA`, `transparent`, `.`, `currentcolor`, `@`, `backgroundcolor`, `~`, case-insensitive), with `AA=00` treated as transparent.
  - Collect `--meta-<name>` properties (name `[a-z0-9-]+`, string or number value) into `metadata` in source order.
  - Report all applicable `E-GLYPH-*` codes with the line and column of the offending token. Column positions inside a row string are offset by the token's index within the string.
  - Compute `contentHash` as FNV-1a 64 over the input with CRLF normalised to LF.

  It returns `Expected<GlyphDefinition, std::vector<Diagnostic>>` and has no filesystem access.
- [X] T032 [US1] Implement `parseManifest(std::string_view, path)` in `src/glyphcss/manifest_parser.cpp` (header `src/glyphcss/include/pxc/glyphcss/font_manifest.hpp`, struct per data-model §2) for the `font-pack { … }` rule in [contracts/font-manifest-schema.md](./contracts/font-manifest-schema.md). It must parse every listed property (coverage as a comma-separated UnicodeRange list, aliases as a comma-separated String list) and emit the property-level codes `E-PACK-SCHEMA`, `E-PACK-MISSING-PROP`, `E-PACK-METRICS` and `E-PACK-AUTHENTICITY`. Filesystem-level codes are handled in T036.
- [X] T033 [US1] Create `src/glyphcss/CMakeLists.txt` defining the static library `pxc_glyphcss`, which links `pxc_core` and has sources from T031–T032.
- [X] T034 [US1] Define the plain pixel type `RasterGlyph{width, height, originX, originY, std::vector<uint32_t> pixels}` in `src/render/include/pxc/render/raster_glyph.hpp`. It has no glyph-CSS dependency. Then implement `rasterize(const GlyphDefinition&, int scale, Color fg, Color bg) -> render::RasterGlyph` in **`src/fonts/include/pxc/fonts/rasterize.hpp` and `src/fonts/rasterize.cpp`**:
  - Each cell is replicated into an s×s block (nearest neighbour).
  - `CurrentColor` resolves to `fg` and `BackgroundColor` resolves to `bg` (both opaque).
  - Literal cells keep their RGBA, and transparent cells keep alpha 0.
  - The background is **never** pre-composited.
  - `originX = bearingX·s` and `originY = −baseline·s` (data-model §5).

  Create `src/fonts/CMakeLists.txt` defining the static library `pxc_fonts`, which links `pxc_glyphcss` and `pxc_render`, with this source. Later fonts tasks add files to it.
- [X] T035 [US1] Implement `src/render/include/pxc/render/renderer.hpp` and `src/render/renderer.cpp`:
  - `Renderer::fill(Framebuffer&, Rect, Color)`.
  - `Renderer::blitGlyph(Framebuffer&, const RasterGlyph&, int penX, int baselineY, Rect clip)`, using the contract geometry and integer source-over blend with clipping.
  - `Renderer::drawCaret(Framebuffer&, Rect, Color)`.
  - A `RendererCounters` struct (`glyphBlits`, `pixelsWritten`) that is reset by the caller.

  Add the files to `src/render/CMakeLists.txt`.
- [X] T036 [US1] Implement `src/fonts/font_pack.cpp` and `src/fonts/include/pxc/fonts/font_registry.hpp` with the US1 subset of `FontRegistry`:
  - `loadAll(fontsDir)` scans `*/manifest.css`, parses the manifest, and checks `E-PACK-ID-MISMATCH`, `E-PACK-DUP-ID` and `E-PACK-LICENSE-FILE`.
  - It enumerates `glyphs/U+*.css`, maps filenames to code points with the strict regex `^U\+[0-9A-F]{4,6}\.css$` (anything else is ignored with a warning), parses each file, and detects `E-PACK-DUP-GLYPH`, `E-PACK-COVERAGE-MISSING` and `W-PACK-EXTRA-GLYPH`. Against the manifest metrics it reports `W-GLYPH-EXCEEDS-METRICS` (`baseline > ascent` or `height − baseline > descent`) and `W-GLYPH-UNTRIMMED` (fully transparent edge rows or columns).
  - It sets the pack state (Valid, Degraded or Invalid; data-model §3).
  - It provides `packs()`, `metrics(packId)` (lineHeight, ascent, descent, spaceAdvance) and `resolve(packId, cp)`. In this task `resolve` returns only primary glyphs or `nullptr`; the fallback chain is T083.

  File reads go through `std::ifstream` in binary mode, and failures are recorded as diagnostics, never thrown. Add the sources to `src/fonts/CMakeLists.txt` (created in T034).
- [X] T037 [US1] Implement `src/fonts/include/pxc/fonts/raster_cache.hpp` and `src/fonts/raster_cache.cpp`. `RasterCacheKey` is defined per R-14 (including `style`) with a hash. The LRU is built from `std::list` plus `std::unordered_map`, with a byte budget defaulting to 16 MiB. `get(packId, cp, scale, fg, bg)` calls `FontRegistry::resolve` and `fonts::rasterize` on a miss. Expose `CacheStats{hits, misses, rasterizations, evictions}` with `hitRate()`, plus `stats()`, `resetStats()` and `evictStale(packId)`.
- [X] T038 [US1] Implement the canonical headless renderer `renderTextLine(const FontRegistry&, RasterCache&, packId, std::u32string_view, int scale, Color fg, Color bg) -> render::Framebuffer` in `src/fonts/include/pxc/fonts/text_line.hpp` and `src/fonts/text_line.cpp`, exactly per research R-21:
  - canvas width `(Σadvance + 2)·s`, plus `−bearingX·s` of the first glyph if it is negative;
  - height `lineHeight·s`, filled with `bg`;
  - pen starting at `x = s`, and `baselineY = ascent·s`;
  - glyphs blitted with `Renderer::blitGlyph`.

  This is the only function that `pixelcss-tool render`, the glyph goldens (T045, T084) and test (f) of T028 may use to produce single-line images.
- [X] T039 [US1] Create `tools/pixelcss-tool/main.cpp`, `tools/pixelcss-tool/cmd_render.cpp`, `cmd_compare.cpp`, `cmd_validate.cpp` and `tools/pixelcss-tool/CMakeLists.txt` (console executable `pixelcss-tool`). The commands follow the [contracts/ui-contract.md](./contracts/ui-contract.md) CLI table:
  - `render`: calls `fonts::renderTextLine` (T038) with `--fg`/`--bg` defaulting to `#000000`/`#FFFFFF`, and writes a BMP. It must not compute its own canvas.
  - `compare`: exits 0 if equal, 1 if different, and accepts `--diff`.
  - `validate`: prints diagnostics and a per-pack state line, exiting 0 when there are no errors and 1 otherwise.

  The fonts directory defaults to `<exe-dir>/fonts`, and `--fonts-dir` overrides it. Add a post-build step that copies `fonts/` next to the tool.
- [X] T040 [P] [US1] Author `fonts/verdana-style/manifest.css` per the contract example with the R-18 metrics (`--line-height: 15; --ascent: 11; --descent: 3; --order: 1`) and `--aliases: "Verdana", "verdana-style"`. Add `fonts/verdana-style/LICENSE.txt` containing the full SIL OFL 1.1 text with "Copyright 2026 PixelCSS contributors".
- [X] T041 [P] [US1] Author Verdana-style glyphs for `U+0020` and the digits `U+0030`–`U+0039` (11 files) as `fonts/verdana-style/glyphs/U+XXXX.css`. Use original pixel art following the **tight-ink-box authoring convention** in the glyph contract: digits have height 10 and baseline 10, advance 8, wide open forms, and `@` cells only. The space is width 0, height 0, baseline 0, advance 4. The header comment in each file reads `/* verdana-style <char> — original artwork, OFL-1.1 */`.
- [X] T042 [P] [US1] Author Verdana-style uppercase `U+0041`–`U+005A` (26 files) in `fonts/verdana-style/glyphs/` using the tight-ink-box convention: height 10, baseline 10 (Q may use height 11, baseline 10), advances 7–9, wide proportions, open counters.
- [X] T043 [P] [US1] Author Verdana-style lowercase `U+0061`–`U+007A` (26 files) in `fonts/verdana-style/glyphs/` using the tight-ink-box convention:
  - x-height letters: height 7, baseline 7.
  - Ascender letters (`b d f h k l`): height 11, baseline 11. `t`: height 9, baseline 9.
  - `i`: height 10, baseline 10, made of 1 dot row, 2 transparent gap rows and a 7-row stem.
  - Descender letters (`g p q y`): height 10, baseline 7.
  - `j`: height 13, baseline 10, made of 1 dot row, 2 gap rows, a 7-row stem above the baseline and 3 descender rows.
- [X] T044 [P] [US1] Author Verdana-style punctuation and symbols `U+0021`–`U+002F`, `U+003A`–`U+0040`, `U+005B`–`U+0060` and `U+007B`–`U+007E` (32 files), plus the replacement glyph `U+FFFD.css` (a framed question mark, height 11, baseline 11, using `~` knock-out cells for the question mark inside an `@` frame, which exercises background substitution), in `fonts/verdana-style/glyphs/`. Use the tight-ink-box convention, for example `,` with height 3 and baseline 1, `_` with height 1 and baseline 0, and `-` with height 1 and baseline 4. Every glyph must pass `validate --strict` with no `W-GLYPH-EXCEEDS-METRICS` or `W-GLYPH-UNTRIMMED`.
- [X] T045 [US1] Write visual test `tests/visual/test_pack_goldens.cpp`. For every pack under `PXC_FONTS_DIR` and every covered code point, it renders the glyph at s=1 with `fonts::renderTextLine` (T038; black on white, canonical canvas) and runs `CHECK_GOLDEN` against `tests/golden/glyphs/<pack>/U+XXXX.s1.bmp` (SC-003). Generate and visually review the 96 Verdana-style goldens with `PIXELCSS_UPDATE_GOLDENS=1`.
- [X] T046 [US1] Register a CTest `tool_render_A` in `tests/integration/CMakeLists.txt` that runs `pixelcss-tool render --pack verdana-style --text A` and then `pixelcss-tool compare` against the golden (an end-to-end check of the US1 independent test), and a CTest `tool_validate_invalid_glyph` that expects exit code 1 and the `E-GLYPH-ROW-LENGTH` text in the output for a pack fixture in `tests/fixtures/packs/bad-row/`. Create that fixture as a copy of a minimal two-glyph pack with one bad row.

**Checkpoint**: US1 is complete. Quickstart §3 passes, and `ctest -L "unit|contract|visual"` is green.

---

## Phase 4: User Story 2 — Edit and save a plain-text document (Priority: P1)

**Goal**: A native Win32 notepad covering typing, navigation, selection, clipboard, undo/redo, New/Open/Save/Save As and the unsaved-changes prompt. It is rendered entirely on the custom surface using the Verdana-style pack.

**Independent Test**: Type a multiline paragraph, edit it, undo and redo, save, reopen, and confirm the content is identical (quickstart §4).

### Tests for User Story 2 ⚠️

- [X] T047 [P] [US2] Write contract test `tests/contract/test_document.cpp` for `TextDocument`:
  - `insert` and `erase` at the start, middle and end of the document and across lines; `lineCount`, `lineStart` and `toLineCol` after each.
  - `revision` increments on every change, and `lineRevision` changes only for affected lines.
  - The no-`\r` invariant: inserting `"\r\n"` stores `"\n"`.
  - Undo and redo restore text, caret and selection snapshots.
  - Coalescing: contiguous typing within 1 s is one group, a whitespace→word boundary splits groups, and the history is capped at 10 000 groups.
  - The `modified` flag after `markSaved`, then undo back to the saved point.
  - Non-BMP code points (U+1F600) count as one offset.
- [X] T048 [P] [US2] Create the text fixtures in `tests/fixtures/text/` (`lf.txt`, `crlf.txt`, `mixed.txt`, `utf8-bom.txt`, `utf16le-bom.txt`, `utf16be-bom.txt`, `invalid-utf8.txt`, `trailing-newline.txt`, `no-trailing-newline.txt`, `empty.txt`, `tabs.txt`, `unicode.txt`). Write unit test `tests/unit/storage/test_text_codec.cpp` asserting decoded text and `FileInfo` (encoding, eol, BOM), and that encoding back reproduces the original bytes exactly for the lf, crlf, utf8-bom, trailing-newline, empty, tabs and unicode fixtures (R-11).
- [X] T049 [P] [US2] Write integration test `tests/integration/test_file_failures.cpp` using a `FakeFileSystem` (in `tests/support/fake_file_system.hpp`) that injects `AccessDenied`, `SharingViolation`, `DiskFull` and `NotFound`. Each must map to the right `FileError::kind` with a non-empty `userMessage`. The document's text and `modified` flag must be unchanged after a failed save, and a failed atomic save must leave the original target file intact. Also run one real-filesystem test with a read-only temp file.
- [X] T050 [P] [US2] Write contract test `tests/contract/test_editor.cpp` driving `EditorController` with a `FakeClipboard` (in `tests/support/fake_clipboard.hpp`). It covers:
  - Typing printable characters and Enter.
  - Backspace at offset 0, Delete at end of document, and Backspace and Delete across a newline.
  - Arrows, Home/End (visual row), Ctrl+Home/End, Ctrl+←/→ word moves, and PgUp/PgDn.
  - Shift-extended selection, then typing to replace the selection.
  - Cut, Copy and Paste of ASCII, of `"é漢😀"` (preserved even without glyphs, FR-031) and of `"a\r\nb"` (normalised to `\n`).
  - Undo and redo restoring the caret and selection.
  - The `modified` state transitions.
- [X] T051 [P] [US2] Write contract test `tests/contract/test_layout.cpp` for `LayoutEngine` with the Verdana-style pack:
  - No-wrap glyph x-positions equal cumulative advances.
  - Wrap breaks at the last fitting space, and a long word breaks mid-word.
  - An explicit newline starts a new logical line, and an empty line has one visual row.
  - A tab advances to the next multiple of `4·spaceAdvance`.
  - `hitTest` and `caretAt` round-trip for every offset of a sample.
  - Toggling wrap relays out correctly.
  - A 10 000-character single line without wrap has the correct `scrollX` extent.
  - Two-tier cache: after laying out 5 000 lines, at most 4 096 full `LayoutLine`s are resident, every line has `LineMetrics`, and an evicted line re-lays out identically on demand.
  - Intra-line incremental re-layout: inserting into the middle of a wrapped 100 000-character line produces the same rows as laying out from scratch, while re-wrapping only from the edited row to the resync point (assert the count of rows recomputed through a test counter).
- [X] T052 [P] [US2] Write visual test `tests/visual/test_editor_surface.cpp`. It renders a 3-line document with a selection and caret through `UpdateManager::paint` into a 320×120 framebuffer and checks goldens `editor/basic.bmp`, `editor/wrapped.bmp` (wrap on, narrow width; FR-052 wrapping), `editor/clipped.bmp` (horizontal scroll clipping glyphs at the left edge) and `editor/selection.bmp`.
- [X] T053 [P] [US2] Write integration test `tests/integration/test_editor_roundtrip.cpp`. Type a multiline paragraph through `EditorController`, save with the real `Win32FileSystem` to a temp path, reload into a new document, and assert the text is equal and the file bytes match the expected UTF-8 with CRLF. Then drive the unsaved-changes flow through an `IPromptService` fake (in `tests/support/fake_prompt.hpp`): Save, Don't Save and Cancel for each of New, Open and Exit, asserting that Cancel aborts the action and a failed Save aborts it without losing content.

### Implementation for User Story 2

- [X] T054 [US2] Implement the piece table in `src/document/piece_table.hpp` and `src/document/piece_table.cpp`: original and append `std::u32string` buffers, `std::vector<Piece{buffer, start, length}>`, `insert(offset, u32string_view)`, `erase(offset, count)`, `text(offset, count)`, `at(offset)`, `length()`, and adjacent-append merging (R-10).
- [X] T055 [US2] Implement the Fenwick-tree line index in `src/document/line_index.hpp` and `src/document/line_index.cpp`. It stores per-line lengths with `\n` included, and supports `lineOfOffset`, `lineStart` and `lineCount`, plus `applyEdit(firstLine, oldLines, newLineLengths)` (rebuilding only the suffix tree when the line count changes). Add `rebuild(from text)` for loads.
- [X] T056 [US2] Implement `TextDocument` in `src/document/include/pxc/document/text_document.hpp` and `src/document/text_document.cpp` per the engine-interfaces contract and data-model §6. It strips `\r` on insert, maintains `revision` and `lineRevisions` (a `std::vector<uint64_t>` updated for affected lines, with values taken from a global counter), and provides `EditResult`, `EditRecord` undo/redo with the coalescing rules and the 10 000-group cap, `modified()` and `markSaved()` (tracking the saved undo depth). Create `src/document/CMakeLists.txt` for `pxc_document`.
- [X] T057 [P] [US2] Implement `src/storage/include/pxc/storage/text_codec.hpp` and `src/storage/text_codec.cpp` per R-11. `decode(bytes, InvalidUtf8Policy) -> Expected<LoadedText{u32 text, FileInfo}, FileError>` detects BOMs (UTF-8, UTF-16LE, UTF-16BE) and detects the EOL style (CRLF, LF or Mixed, stripping `\r`). `encode(u32 text, FileInfo) -> std::string` writes UTF-8 (with a BOM if `Utf8Bom`) and uses the remembered EOL, with Mixed and new documents writing CRLF.
- [X] T058 [P] [US2] Define `src/storage/include/pxc/storage/file_system.hpp` with `IFileSystem` (`readAll(path)`, `writeAtomic(path, bytes)`, each returning `Expected` with `FileError`) and implement `src/storage/win32/win32_file_system.cpp`:
  - `readAll` uses `CreateFileW` with `GENERIC_READ` and `FILE_SHARE_READ|FILE_SHARE_WRITE`, then `ReadFile`.
  - `writeAtomic` writes `<path>.pixelcss-tmp`, calls `FlushFileBuffers`, then `ReplaceFileW` if the target exists and `MoveFileExW(MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)` otherwise, and deletes the temp file on failure.
  - `GetLastError()` maps to `FileError::kind`: `ERROR_ACCESS_DENIED` → AccessDenied, `ERROR_SHARING_VIOLATION`/`ERROR_LOCK_VIOLATION` → SharingViolation, `ERROR_DISK_FULL`/`ERROR_HANDLE_DISK_FULL` → DiskFull, `ERROR_FILE_NOT_FOUND`/`ERROR_PATH_NOT_FOUND` → NotFound. Each kind gets a readable `userMessage` that includes the file name.
- [X] T059 [US2] Implement `loadTextFile(IFileSystem&, path, InvalidUtf8Policy)` and `saveTextFileAtomic(IFileSystem&, path, const TextDocument&, const FileInfo&)` in `src/storage/text_file.cpp`, with the header `src/storage/include/pxc/storage/text_file.hpp`. Create `src/storage/CMakeLists.txt` with `pxc_storage` (portable part) and `pxc_storage_win32`.
- [X] T060 [US2] Implement `LayoutEngine` in `src/layout/include/pxc/layout/layout_engine.hpp` and `src/layout/layout_engine.cpp` per data-model §8 and the engine-interfaces contract:
  - `LayoutParams{packId, scale, wrapWidth or NoWrap}`, and the **two-tier** cache from research R-12 / data-model §8, keyed on `{lineRevision, packId, packRevision, scale, wrapWidth}`:
    - a full tier as an LRU of `LayoutLine`s capped at 4 096 lines or 32 MiB;
    - a metrics tier as a `std::vector<LineMetrics>` parallel to the line index, spliced on line-count changes.
  - Greedy wrapping (R-12) and tab stops (R-11).
  - `relayoutEdit(const EditResult&)` performs the intra-line incremental re-wrap (R-13). It starts at the visual row containing the edit and stops at the resync point. With wrap off, it splices the glyph vector and shifts the later x-positions. It returns the affected row range.
  - `PositionedGlyph` resolution through `FontRegistry::resolve`.
  - `invalidateLines`, `invalidateAll`, `hitTest`, `caretAt` and a `linesLaidOut` counter.

  Create `src/layout/CMakeLists.txt` for `pxc_layout`, linking `pxc_fonts` and `pxc_document`.
- [X] T061 [US2] Implement `Viewport` in `src/layout/include/pxc/layout/viewport.hpp` and `src/layout/viewport.cpp`. It holds the anchor `{logicalLine, rowInLine, pixelOffset}` and `scrollX` and provides `scrollBy(dy)`, `scrollToMakeVisible(caretRect)`, and `visibleRows(LayoutEngine&, doc)` yielding `(line, row, y)`. Total height uses estimated row counts for unlaid lines (R-12), and `contentWidth` serves horizontal scrolling.
- [X] T062 [US2] Implement the initial `UpdateManager` in `src/update/include/pxc/update/update_manager.hpp` and `src/update/update_manager.cpp`. `paint(Framebuffer&, const Viewport&, IRenderObserver*)` fills the background `#FFFFFF`, iterates over the visible rows, blits each glyph via `RasterCache` (fg `#000000`, bg = the run's background, so `backgroundcolor` cells match the page or the selection fill), and draws the selection (fill `#3366CC`, glyph fg `#FFFFFF`) and the 1·s-wide caret. Every `on*` method marks `fullViewport` dirty; this is refined in US4. Create `src/update/CMakeLists.txt` for `pxc_update`.
- [X] T063 [US2] Implement `EditorController` in `src/editor/include/pxc/editor/editor_controller.hpp` and `src/editor/editor_controller.cpp`. It has an `EditorCommand` variant (Type, Enter, Backspace, Delete, Move{Left, Right, Up, Down, Home, End, DocStart, DocEnd, WordLeft, WordRight, PageUp, PageDown; extend}, SelectAll, Undo, Redo, Copy, Cut, Paste, ToggleWrap, SetFont) and `EditorState` per data-model §11, with `preferredX` for vertical moves. It uses the `IClipboard` interface (`src/editor/include/pxc/editor/clipboard.hpp`) and forwards `EditResult` to `UpdateManager::onEdit`. Create `src/editor/CMakeLists.txt` for `pxc_editor`.
- [X] T064 [US2] Implement `src/shell/main.cpp`:
  - `wWinMain` with `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)` as a fallback to `src/shell/app.manifest` (which declares PerMonitorV2, the common-controls v6 dependency and supportedOS Windows 10/11).
  - `InitCommonControlsEx`.
  - Command-line parsing for `[file] [--fonts-dir] [--stats-json] [--bench-script]`.
  - A message loop based on `MsgWaitForMultipleObjectsEx` with an idle hook (used in US4).
  - Top-level `try/catch` that logs and shows a fatal message box without losing the document; on fatal error it attempts an emergency save to `%TEMP%\PixelCSS-recovery-<timestamp>.txt`.
- [X] T065 [US2] Implement `src/shell/main_window.hpp` and `src/shell/main_window.cpp`:
  - The top-level window class.
  - File, Edit, View and Help menus and an accelerator table, both defined in `src/shell/resources.rc` and `src/shell/resource.h` exactly per the ui-contract table.
  - A status bar (`STATUSCLASSNAMEW`) with parts for Ln/Col, encoding, EOL, font and scale.
  - A title showing `PixelCSS — <name>` with `*` when modified.
  - Layout of toolbar, surface and status bar on `WM_SIZE`.
  - The **initial document scale** is computed at window creation from `GetDpiForWindow` as `s = max(1, floor(dpi/96 + 0.5))` (research R-08) and passed to the surface, so P1 stories render at the correct integer scale on 125 %, 150 % and 200 % displays from the start. The status-bar scale part shows `s·100 %`.
- [X] T066 [US2] Implement `src/shell/pixel_surface.hpp` and `src/shell/pixel_surface.cpp`, the `PixelCSSSurface` child window. Input mapping:
  - `WM_KEYDOWN` maps to Move, Delete and the like, honouring Shift and Ctrl.
  - `WM_CHAR` maps to Type, ignoring control characters below 0x20 except Tab and Enter, and combining surrogate pairs.
  - Mouse down, move and up give click and drag selection with capture; a double-click selects a word.
  - `WM_MOUSEWHEEL` scrolls vertically and Shift+wheel or `WM_MOUSEHWHEEL` scrolls horizontally; `WM_VSCROLL`/`WM_HSCROLL` drive the scrollbars, updated from Viewport metrics.
  - Caret blink uses a 530 ms timer that marks only the caret rectangle dirty.

  Implement `GdiPresenter : IPresenter` in `src/shell/gdi_presenter.cpp`: a DIB section sized to the client area, with the framebuffer copied for dirty rectangles only and `BitBlt` per dirty rectangle in `WM_PAINT`. No text APIs are used.
- [X] T067 [P] [US2] Implement `Win32Clipboard : IClipboard` in `src/shell/win32_clipboard.cpp`. It uses `CF_UNICODETEXT` via `OpenClipboard`, `GetClipboardData`, `GlobalLock` and `SetClipboardData` with UTF-16 conversion from `core::unicode`, converts `\n` to `\r\n` when writing, and retries `OpenClipboard` up to 5 times at 10 ms intervals (FR-030).
- [X] T068 [US2] Implement `src/shell/file_commands.cpp` and `src/shell/prompt_service.cpp` (`IPromptService` defined in `src/editor/include/pxc/editor/prompt_service.hpp`):
  - New, Open and Save As use `IFileOpenDialog`/`IFileSaveDialog` with filters `Text (*.txt)`, `HTML (*.html;*.htm)` and `All files`.
  - Save routes through Save As when the document is untitled.
  - The unsaved-changes dialog uses `TaskDialogIndirect` with the buttons Save, Don't Save and Cancel.
  - The invalid UTF-8 dialog offers Open with replacement or Cancel, and the UTF-16 conversion confirmation runs on save.
  - File error message boxes use `FileError::userMessage`.
  - `WM_CLOSE` and Exit go through the prompt.
- [X] T069 [US2] Implement `src/shell/toolbar.cpp`: a toolbar strip (a `TOOLBARCLASSNAMEW` with an embedded control area) holding the checkable **Word Wrap** button, synchronised with **View → Word Wrap** and sending `ToggleWrap` (FR-029). Leave a reserved slot for the font combo added in T086.
- [X] T070 [US2] Create `src/shell/CMakeLists.txt` defining `PixelCSS` as a `WIN32` executable with sources T064–T069, `resources.rc` and `app.manifest`. It links `pxc_editor`, `pxc_update`, `pxc_layout`, `pxc_fonts`, `pxc_storage_win32`, `comctl32`, `comdlg32`, `shell32`, `ole32` and `uxtheme`, and has a post-build step `copy_directory fonts → $<TARGET_FILE_DIR:PixelCSS>/fonts`. Set `VS_DEBUGGER_WORKING_DIRECTORY` and make `PixelCSS` the `VS_STARTUP_PROJECT` (US7-AS1).

**Checkpoint**: US2 is complete. Quickstart §4 passes manually, and `ctest -L "contract|integration|visual"` is green.

---

## Phase 5: User Story 3 — Switch among three CSS-defined font families (Priority: P1)

**Goal**: A toolbar dropdown offering Verdana, Arial Narrow and Comic Sans (default Verdana). Two more original packs, a deterministic fallback chain, presentation-only font switching, and disclosure of authenticity.

**Independent Test**: Type `Aa0!? Hello World`, cycle through all three fonts, match the goldens, and confirm the buffer, caret and selection offsets are unchanged (quickstart §5).

### Tests for User Story 3 ⚠️

- [X] T071 [P] [US3] **Create** the fixture packs `tests/fixtures/packs/missing-glyph/` (a minimal valid pack whose manifest covers `U+0041-0043, U+FFFD` but has no `U+0042.css`) and `tests/fixtures/packs/no-replacement/` (covers `U+0041-0043` with no `U+FFFD.css`; manifest `--replacement-glyph: U+FFFD`), each with a `manifest.css`, `LICENSE.txt` and simple 3×5 test glyphs. Then write contract test `tests/contract/test_font_registry.cpp` covering:
  - Three bundled packs load as Valid in `--order` order, and the default active pack is `verdana-style`.
  - The R-07 fallback chain, using those fixture packs: a missing glyph resolves to the pack's U+FFFD (`source == PackReplacement`), and with no pack replacement it resolves to the engine glyph (`EngineReplacement`).
  - An unknown pack id resolves through `verdana-style`, then the engine box font.
  - Non-ASCII U+00E9 resolves to the replacement.
  - Each `(pack, cp, reason)` diagnostic is logged exactly once even after 100 resolves.
- [X] T072 [P] [US3] Write integration test `tests/integration/test_font_switch.cpp`. It types `Aa0!? Hello World` (plus a long line that wraps) into `EditorController`, selects `Hello`, and switches `SetFont` across arial-narrow-style, comic-style and verdana-style. After each switch it asserts that the text, `caret` and `anchor` are unchanged (SC-004), that the wrapped visual row count differs between packs for the long line, and that the framebuffer matches golden `font_switch/<pack>.bmp`. It also asserts that `preferredX` was recomputed from the caret offset.
- [X] T073 [P] [US3] Write visual test `tests/visual/test_fallback.cpp` with goldens `fallback/missing-glyph.bmp` (a fixture pack missing `B`, rendering `ABC`), `fallback/non-ascii.bmp` (`aé漢b` in each bundled pack) and `fallback/engine-box.bmp` (the no-replacement fixture).
- [X] T074 [P] [US3] Write contract test `tests/contract/test_preferences.cpp` against `MemoryPreferencesStore` (in `tests/support/memory_preferences.hpp`) and against the real `RegistryPreferencesStore`, using a test-only key `HKCU\Software\PixelCSS-Test-<random>` that is deleted at the end. It covers the defaults on first run (`verdana-style`, wrap on), a save→load round-trip, a corrupt value (wrong registry type) falling back to defaults with a diagnostic, and a stored pack id that no longer exists making the startup selection fall back to Verdana-style. It also covers the production `NullPreferencesStore` (from T089): `load()` returns the defaults (`verdana-style`, wrap on) without adding a diagnostic, `save()` returns true, and after a save the test registry key is still absent, so nothing was written.

### Implementation for User Story 3

- [X] T075 [P] [US3] Author `fonts/arial-narrow-style/manifest.css` (id `arial-narrow-style`, display name `Arial Narrow`, line height 15, ascent 11, descent 3, `--order: 2`, aliases `"Arial Narrow", "arial-narrow-style"`, authenticity `style-interpretation`, OFL-1.1) and `fonts/arial-narrow-style/LICENSE.txt`.
- [X] T076 [P] [US3] Author Arial Narrow-style `U+0020` and digits `U+0030`–`U+0039` in `fonts/arial-narrow-style/glyphs/`: original condensed artwork using the tight-ink-box convention (digits height 10, baseline 10), advances 5–6 (space 3) and tight sidebearings. All Arial Narrow-style glyphs (T076–T079) use cap height 10, ascender 11, x-height 6 and descender 3, and must pass `validate --strict`. Do not derive them from the Verdana-style matrices (FR-009).
- [X] T077 [P] [US3] Author Arial Narrow-style uppercase `U+0041`–`U+005A` in `fonts/arial-narrow-style/glyphs/`, condensed with advances 4–7.
- [X] T078 [P] [US3] Author Arial Narrow-style lowercase `U+0061`–`U+007A` in `fonts/arial-narrow-style/glyphs/`, with x-height 6, narrow bowls and advances 3–6.
- [X] T079 [P] [US3] Author Arial Narrow-style punctuation and symbols (the same 32 code points as T044) plus `U+FFFD.css` in `fonts/arial-narrow-style/glyphs/`.
- [X] T080 [P] [US3] Author `fonts/comic-style/manifest.css` (id `comic-style`, display name `Comic Sans`, line height 16, ascent 12, descent 3, `--order: 3`, aliases `"Comic Sans MS", "Comic Sans", "comic-style"`, authenticity `style-interpretation`, OFL-1.1) and `fonts/comic-style/LICENSE.txt`.
- [X] T081 [P] [US3] Author Comic Sans-style `U+0020`, digits, and uppercase `U+0041`–`U+005A` in `fonts/comic-style/glyphs/`. The artwork is original and informal, using the tight-ink-box convention. All Comic Sans-style glyphs (T081–T082) use cap height 11, ascender 12, x-height 7 and descender 3, and must pass `validate --strict`. Draw rounded terminals, a slightly irregular baseline wobble of ±1 row on selected letters while staying within the metrics, and advances 6–9.
- [X] T082 [P] [US3] Author Comic Sans-style lowercase `U+0061`–`U+007A`, the 32 punctuation and symbol code points, and `U+FFFD.css` in `fonts/comic-style/glyphs/`, with x-height 7 and a single-storey `a` and `g`.
- [X] T083 [US3] Complete `FontRegistry::resolve` in `src/fonts/font_registry.cpp` with the full R-07 fallback chain and `ResolvedGlyph{def, source, packUsed}`. Implement `src/fonts/engine_glyph.cpp` to build the engine replacement `GlyphDefinition` in code: a hollow box sized `width = max(3, spaceAdvance)`, `height = ascent`, `baseline = height` (sitting on the baseline), `bearingX = 0`, a 1-cell `currentcolor` border with a `backgroundcolor` interior, and `advance = width + 1`. Implement `engineBoxFont` for invalid packs. Record diagnostics once per `(pack, cp, reason)` with the codes `W-GLYPH-FALLBACK-PACK-REPLACEMENT` and `W-GLYPH-FALLBACK-ENGINE`.
- [X] T084 [US3] Generate and review goldens for all 96 glyphs of `arial-narrow-style` and `comic-style` in `tests/golden/glyphs/<pack>/` using the T045 test, and fix any glyph that fails `pixelcss-tool validate`.
- [X] T085 [US3] Implement the `distinct` command in `tools/pixelcss-tool/cmd_distinct.cpp` (FR-009). For each pair of packs and each shared code point, it normalises both matrices to an ink bitmap (literal or currentcolor = 1) cropped to the ink bounding box. If the sizes differ the glyph counts as distinct; otherwise it compares cells for an exact match. It reports the identical percentage per pair and exits 1 if any pair exceeds 60 %. Register CTest `fonts_distinct` running it on `fonts/`.
- [X] T086 [US3] Implement font switching end to end:
  - `EditorController::handle(SetFont{packId})` updates `activePackId`, recomputes `preferredX` from `caretAt(caret)`, and keeps the caret line visible through `Viewport::scrollToMakeVisible`.
  - `UpdateManager::onFontChanged()` calls `LayoutEngine::invalidateAll()` and marks the full viewport dirty (in `src/editor/editor_controller.cpp` and `src/update/update_manager.cpp`).
  - The text and offsets are never touched (FR-026).
- [X] T087 [US3] Add the font dropdown to `src/shell/toolbar.cpp`. It is a `WC_COMBOBOXW` with `CBS_DROPDOWNLIST` and the label `Font:`, populated from `FontRegistry::packs()` (Valid and Degraded only; Degraded packs get the suffix ` ⚠`), with Verdana initially selected. `CBN_SELCHANGE` sends `SetFont`. The tooltip (`TOOLTIPS_CLASSW`) reads `"<display-name> (style interpretation — not the original typeface)"` for `style-interpretation` packs. Update the status-bar font part to `"<display-name> (style)"`.
- [X] T088 [US3] Implement `src/shell/about_fonts_dialog.cpp` (**Help → About Fonts**, a ListView with display name, id, version, author, origin, license and authenticity per pack; FR-013, US3-AS6) and `src/shell/diagnostics_dialog.cpp` (**Help → Diagnostics…**, listing `DiagnosticsLog` entries with severity, code, message and location). Add a status-bar notice `"N font problems — Help → Diagnostics"` whenever the log has asset warnings or errors.
- [X] T089 [US3] Implement display-preference persistence (research R-20, Constitution IX):
  - `IPreferencesStore`, `Preferences` and `NullPreferencesStore` in `src/storage/include/pxc/storage/preferences.hpp` (portable; no Win32).
  - `RegistryPreferencesStore` in `src/storage/win32/registry_preferences.cpp`, using `RegCreateKeyExW`, `RegGetValueW` and `RegSetValueExW` under `HKCU\Software\PixelCSS\Display`, with values `LastPack` (REG_SZ) and `WordWrap` (REG_DWORD).
  - In `src/shell/main.cpp`, add the command-line flag `--no-preferences` (this task owns its parsing; see [contracts/ui-contract.md](./contracts/ui-contract.md)). When it is given, the production `NullPreferencesStore` is used instead of `RegistryPreferencesStore`. That store lives in `src/storage/include/pxc/storage/preferences.hpp`, its `load()` returns the defaults (`verdana-style`, wrap on), and its `save()` does nothing and returns true. Never use the test double `MemoryPreferencesStore` in production code.
  - Load preferences at startup before the toolbar is populated (falling back to Verdana-style when the stored pack is not Valid or Degraded) and save after every `SetFont` and `ToggleWrap`. A save failure only adds a diagnostic.

**Checkpoint**: US1–US3 work together. Quickstart §5 passes, and `validate` and `distinct` pass for all three packs.

---

## Phase 6: User Story 4 — Render progressively and repaint only affected content (Priority: P1)

**Goal**: Viewport-first progressive layout, minimal dirty regions per edit, cache reuse, instrumentation, and benchmarks that meet SC-006, SC-009 and SC-010.

**Independent Test**: Open the 100k fixture, measure the first viewport and scrolling, type one character on a stable line, and confirm that only that line and the caret are repainted with at most 1 rasterization (quickstart §6).

### Tests for User Story 4 ⚠️

- [X] T090 [P] [US4] Write unit test `tests/unit/update/test_dirty_region.cpp`: adding rectangles, merging overlaps, collapsing to the bounding box on the 17th rectangle, `fullViewport` overriding, and clearing after present.
- [X] T091 [P] [US4] Write integration test `tests/integration/test_incremental_paint.cpp` with a headless `EditorController`, `UpdateManager` and a 640×480 framebuffer, using a `RecordingObserver` for `RenderStats`:
  - (a) After a warm paint, typing `x` at the end of a stable line gives `linesLaidOut == 1`, `rasterizations <= 1`, every dirty rectangle inside the edited row plus the caret rectangles, and `dirtyPixelArea <= 2·rowHeight·width` (SC-006).
  - (b) An insertion that changes wrapping dirties the edited row through the viewport bottom.
  - (c) Inserting a newline shifts the following rows and repaints them.
  - (d) A font change, a resize and a wrap toggle each mark the full viewport.
  - (e) **Oracle**: after a scripted sequence of 200 random edits, scrolls and font switches with incremental paints, the framebuffer equals a fresh full repaint of the same state, pixel for pixel (FR-020, no stale pixels).
  - (f) **Long line**: in a 100 000-character single-line document (wrap on, then wrap off), typing one character re-lays out only from the edited visual row to the resync point. Rasterizations stay ≤ 1, the dirty area is bounded by the affected rows, and the result equals a full repaint.
- [X] T092 [P] [US4] Write integration test `tests/integration/test_progressive_open.cpp`. Load `tests/fixtures/perf/100k.txt`. The first paint lays out at most `visibleRows + one screen of overscan` lines (`linesLaidOut` bounded) and the rasterizations do not exceed the distinct glyphs on screen. `layoutIdleSlice(4ms)` called repeatedly completes layout of all lines, the estimated total height converges to the exact height, and the caret line stays anchored on screen throughout (R-12). Scrolling 50 rows causes cache hits and no re-rasterization of already-seen glyphs (US4-AS4).

### Implementation for User Story 4

- [X] T093 [US4] Implement `DirtyRegion` in `src/update/include/pxc/update/dirty_region.hpp` and `src/update/dirty_region.cpp` per data-model §10.
- [X] T094 [US4] Define `RenderStats` and `IRenderObserver` in `src/update/include/pxc/update/render_stats.hpp` with the R-16 fields. Wire the counters from `RasterCache::stats()`, the `LayoutEngine` `linesLaidOut` count, `RendererCounters` and `SteadyClock` timing around layout and paint, and call `beginFrame()`/`endFrame()` in `UpdateManager::paint`.
- [X] T095 [US4] Refine `UpdateManager` in `src/update/update_manager.cpp` to the R-13 rules:
  - `onEdit(EditResult)` invalidates only the affected logical lines, re-lays them out through `LayoutEngine::relayoutEdit` (intra-line incremental re-wrap, R-13) and compares visual row counts. If the counts are equal, it dirties the edited row from the edit x-position to max(old, new) `inkRight`. If they differ, it dirties from the edited row to the viewport bottom. A newline insert or delete dirties the shifted rows.
  - `onCaretMoved` dirties the old and new caret rectangles.
  - `onScroll` moves the existing framebuffer pixels with `memmove` of rows and dirties only the newly exposed rows.
  - `paint()` re-renders only within the dirty rectangles, using clip rects.
- [X] T096 [US4] Implement `LayoutEngine::layoutIdleSlice(budget)` in `src/layout/layout_engine.cpp`. It computes exact `LineMetrics` for the next unmeasured lines outward from the viewport until the budget is used. Full `LayoutLine`s are retained only within the bounded full tier; off-screen lines keep metrics only (R-12). Also implement the R-12 height estimation and refinement in `src/layout/viewport.cpp`, keeping the anchor stable. Hook the idle slice into the `src/shell/main.cpp` message loop: when the queue is empty, call one 4 ms slice and update the scrollbar if the height changed.
- [X] T097 [US4] Implement the first-paint path in `src/shell/file_commands.cpp` and `src/update/update_manager.cpp`. After decoding, lay out only the visible lines and overscan, paint, present, and record `firstViewportMicros` from decode-complete to present (SC-010). The rest of the layout continues in idle slices.
- [X] T098 [US4] Implement `src/shell/stats_overlay.cpp`. **View → Render Statistics** (F12) toggles an overlay in the surface's top-right corner listing the `RenderStats` fields, drawn with `RasterCache` glyphs of the active pack on a translucent `#000000B0` panel. Add the `--stats-json <file>` option to `src/shell/main.cpp`, which writes per-frame stats (including `cacheHitRate`) as JSON lines. It also writes process samples (`GetGuiResources` GR_GDIOBJECTS/GR_USEROBJECTS, `GetProcessHandleCount`, `GetProcessMemoryInfo` working set) at startup, after first present (with `firstPresentQpc`) and at exit (research R-16). Record `keyToPresentMicros` with **QPC**: the start is taken when `WM_KEYDOWN`/`WM_CHAR` is received in the surface's window procedure, and the end after the frame's last `BitBlt` returns. Do **not** use `GetMessageTime`.
- [X] T099 [US4] Implement `--bench-script <file>` and `--exit-after-first-present` (reusing the existing `--no-preferences` flag from T089) in `src/shell/bench_script.hpp`, `src/shell/bench_script.cpp` and `src/shell/main.cpp`, exactly per the "Bench script format" in [contracts/ui-contract.md](./contracts/ui-contract.md):
  - Parse the script with a line-numbered error on the first bad command; the app then exits with code 2.
  - Execute one command per presented frame by posting real window messages (`WM_CHAR`, `WM_KEYDOWN`, `CBN_SELCHANGE` through the combo box, `SetWindowPos`, `WM_VSCROLL`, `WM_COMMAND` for New and Open) and stamping each with QPC at injection.
  - Auto-answer prompts with Don't Save through a scripted `IPromptService`.
  - Support `repeat`/`end`, `wait-idle` and `sample`.
  - Write all timings and samples to `--stats-json`, and exit with code 0 at `exit` or EOF.

  Add the scripts `tests/perf/scripts/typing.pxs` (`open ../../fixtures/perf/100k.txt`, then type 500 characters at the end of a stable line), `tests/perf/scripts/long-line.pxs` (`open ../../fixtures/perf/longline-100k.txt`, then type 200 characters in the middle) and `tests/perf/scripts/resource-cycles.pxs`. The resource-cycles script runs `sample`, then a `repeat 50` block containing `open ../../fixtures/perf/100k.txt`, `font arial-narrow-style`, `font comic-style`, `font verdana-style`, `wrap off`, `wrap on`, `resize 800 600`, `resize 1024 768` and `new`, then `sample` and `exit`. Relative paths in scripts resolve against the script file's own directory, and absolute paths are used as given. All committed scripts under `tests/perf/scripts/` therefore reference fixtures as `../../fixtures/...`, and `typing.pxs` and `long-line.pxs` must use that form too.
- [X] T100 [US4] Create `tools/pixelcss-bench/main.cpp` and its `CMakeLists.txt`, a console program that drives the headless engine with a no-op presenter. `--generate <out> --chars N --seed S` produces a deterministic mixed-length ASCII document. The scenarios are:
  - `first-viewport`: decode, then first paint.
  - `stable-keystroke`: 1 000 end-of-line inserts, reporting p50 and p95.
  - `reflow-keystroke`.
  - `scroll`: 500 page-downs.
  - `font-switch`.
  - `raster-miss`.
  - `buffer-edit` at 100k and 1M characters.
  - `long-line-keystroke`: 1 000 mid-line inserts into a 100 000-character single-line document, with wrap on and off, reporting layout p95.
  - `cache-hit-rate`: the steady-state `cacheHitRate` during the `scroll` and `stable-keystroke` scenarios.
  - `memory`: `GetProcessMemoryInfo` working set with 100k and with 10M characters loaded and idle layout complete.
  - **End-to-end scenarios** that launch the real `PixelCSS.exe` (path from `--app`) with `--no-preferences --stats-json`:
    - `startup`: QPC before `CreateProcessW` to `firstPresentQpc` with `--exit-after-first-present` and an empty document, median of 10 runs.
    - `e2e-typing`: `--bench-script tests/perf/scripts/typing.pxs`, reporting `keyToPresent` p50 and p95 (SC-009).
    - `e2e-long-line`: `long-line.pxs`.
    - `resource-cycles`: `resource-cycles.pxs`, reporting the deltas of GDI objects, USER objects and handles between the first and last samples.

  It writes JSON to `--out`. Generate and commit `tests/fixtures/perf/100k.txt` (100 000 characters, seed 1) and `tests/fixtures/perf/longline-100k.txt` (one line of 100 000 characters with spaces every 4–9 characters, seed 2).
- [X] T101 [US4] Create `tests/perf/CMakeLists.txt` and `tests/perf/baseline.json`. The CTest `perf_regression` (label `perf`; excluded from the default test presets by the filter defined in T005 and run through the `x64-release-perf` preset) runs `pixelcss-bench --app $<TARGET_FILE:PixelCSS>`.

  **Initial baseline**: create `tests/perf/baseline.json` with `"measured": false` and the absolute R-17 budgets as the metric values. While `measured` is false, the 20 % regression comparison is skipped and reported as `SKIPPED (no measured baseline)`. T133 replaces the file with measured values and sets `"measured": true`.

  **Machine gating**: the absolute R-17 budgets below are enforced only when the environment variable `PIXELCSS_REFERENCE_MACHINE=1` is set, on the machine documented in `docs/perf/reference-machine.md`. On other machines only the regression check runs (once a measured baseline exists), and the absolute results are printed for information. The resource-growth check is machine-independent and always enforced.

  The test fails if any metric is more than 20 % worse than the measured baseline, or (on the reference machine) if any absolute R-17 budget is missed:
- e2e p95 keystroke-to-present > 50 ms;
- first viewport > 1 000 ms;
- startup > 750 ms;
- p95 raster miss > 20 µs;
- p95 non-reflowing edit layout > 1 ms;
- p95 long-line keystroke layout > 5 ms;
- cache hit rate < 0.99;
- idle memory > 64 MB with 100k characters or > 256 MB with 10M characters;
- any GDI, USER or handle growth in `resource-cycles` (enforced on every machine).
- [X] T102 [US4] Ratify the text-buffer choice. Run `pixelcss-bench --scenario buffer-edit` at 100k and 1M characters and record the results and the decision in `docs/adr/0001-text-buffer.md` (piece table kept if p95 is below 1 ms; otherwise open a follow-up to move to a balanced-tree piece table behind `TextDocument`) (R-10, Constitution VIII).

**Checkpoint**: SC-006 is proven by T091. SC-009 (end to end), SC-010, startup, cache hit rate and resource stability are measured on the reference machine and recorded in `docs/perf/reference-machine.md`.

---

## Phase 7: User Story 7 — Build, debug, test, and package with MSVC (Priority: P1)

**Goal**: Reproducible Debug and Release builds with MSVC, a debuggable VS solution, an offline ZIP package, and proof that the binary uses no browser engine, network stack or OS text rendering.

**Independent Test**: On a clean machine, run `scripts/build-all.ps1`. It must configure, build and test both configurations and produce a ZIP that runs on a clean VM (quickstart §1, §9).

### Tests for User Story 7 ⚠️

- [X] T103 [P] [US7] Write integration test `tests/integration/test_binary_imports.cpp`, which receives the `PixelCSS.exe` path as the compile definition `PXC_APP_EXE`. It parses the PE import directory by hand (DOS header → NT headers → `IMAGE_DIRECTORY_ENTRY_IMPORT` and delay-load imports) and asserts:
  - No imports from `winhttp`, `wininet`, `ws2_32`, `urlmon`, `webio`, `WebView2Loader`, `dwrite`, `d2d1` or `usp10` (FR-040, FR-049, SC-011).
  - No imports of `DrawTextW/A`, `DrawTextExW/A`, `TextOutW/A`, `ExtTextOutW/A`, `PolyTextOutW/A` or `TabbedTextOutW/A` (FR-014, FR-033).
- [X] T104 [P] [US7] Create `tests/package/check_package.cmake`, a `cmake -P` script that unzips the CPack ZIP to a temp directory and asserts it contains `PixelCSS.exe`, `pixelcss-tool.exe`, `fonts/{verdana-style,arial-narrow-style,comic-style}/manifest.css` and `LICENSE.txt`, exactly 96 `glyphs/U+*.css` per pack (288 in total), `LICENSES/doctest.txt` is **absent** (doctest is used only in tests), `LICENSES/PixelCSS-fonts-OFL.txt` and `README.txt`. Register it as CTest `package_layout` (label `package`), which depends on a `cpack` fixture.
- [X] T105 [P] [US7] Write integration test `tests/integration/test_resource_cleanup.cpp` (label `integration`; Constitution XI "Resource cleanup"). It has three parts:
  - **(a) Process-level**: launch `PXC_APP_EXE --no-preferences --bench-script tests/perf/scripts/resource-cycles.pxs --stats-json <tmp>` (from T099), wait up to 120 s, and parse the JSON. The app exits with code 0, and between the first and last `sample`, `gdiObjects`, `userObjects` and `handleCount` deltas are 0, allowing a tolerance of ±2 handles for OS-owned noise, which is documented in the test.
  - **(b) Engine-level**, in-process: 1 000 iterations of create and destroy for `FontRegistry`, `RasterCache`, `LayoutEngine`, `UpdateManager`, `Framebuffer` and `TextDocument` with a 10 k-character document. This runs under the T022 CRT leak-check listener and must report no leaked blocks.
  - **(c) Files**: after a failed `saveTextFileAtomic` (injected through `FakeFileSystem`, and for real against a read-only target), no `*.pixelcss-tmp` file remains and the original target file is byte-identical.

### Implementation for User Story 7

- [X] T106 [US7] Create `cmake/Packaging.cmake`, included from the root CMakeLists. It installs `PixelCSS.exe` and `pixelcss-tool.exe` to `.`, `fonts/` to `fonts/`, `docs/user/README.txt` to `README.txt` and the OFL to `LICENSES/`. CPack uses generator `ZIP` with `CPACK_PACKAGE_FILE_NAME PixelCSS-${PROJECT_VERSION}-win64` and output directory `out/package` (FR-048). Write `docs/user/README.txt` covering what the app is, the font disclosure, system requirements and that it needs no runtime install.
- [X] T107 [US7] Implement `src/shell/version.rc`, a `VERSIONINFO` with the product version from CMake through `configure_file`, and **Help → About PixelCSS…** in `src/shell/about_dialog.cpp`, which shows the version, `_MSC_FULL_VER`, the C++ standard (`_MSVC_LANG`), the Windows SDK version macro and the build configuration (US7-AS1 evidence).
- [X] T108 [US7] Create `scripts/build-all.ps1`. It removes `out/` (with confirmation unless `-Force`) and runs `cmake --preset`, `cmake --build --preset` and `ctest --preset` for x64-debug and x64-release, then `cpack --preset x64-release` and `ctest -L package`. It stops on the first failure (`$LASTEXITCODE` check) and prints a summary table (SC-001, FR-044).
- [X] T109 [P] [US7] Write `docs/build/building.md` (prerequisites with exact VS workload and component IDs `Microsoft.VisualStudio.Workload.VCTools`/`NativeDesktop`, `Microsoft.VisualStudio.Component.Windows11SDK.26100`, plus the presets and the clean-build steps), `docs/build/debugging.md` (opening the generated solution, startup project, F5, `--fonts-dir`, the F12 overlay) and `docs/build/warnings.md` (the warning policy `/W4 /WX`, an empty suppression list with a template for adding entries with a justification, and why clang-cl and MinGW are rejected) (FR-046, FR-050).

**Checkpoint**: All P1 stories (US1, US2, US3, US4, US7) are done. This is the minimum releasable product.

---

## Phase 8: User Story 5 — Render a deliberately restricted local HTML document (Priority: P2)

**Goal**: A read-only preview of the HTML subset with the presentation-CSS subset ([contracts/html-subset.md](./contracts/html-subset.md)), rendered through the same glyph pipeline, with no script execution and no network access.

**Independent Test**: Open `tests/fixtures/html/basic.html` and check the structure, line breaks, nesting and glyph output against the golden. `unsupported.html` runs no script and fetches nothing (quickstart §7).

### Tests for User Story 5 ⚠️

- [X] T110 [P] [US5] Create the HTML fixtures in `tests/fixtures/html/`:
  - `basic.html` (`html/head/title/body/div/p/span/br` and text with entities).
  - `nested.html` (div > p > span > span, inline-in-block ordering).
  - `styles.html` (a `<style>` block with type, `.class` and `#id` selectors; inline `style`; every supported property; inheritance of `color` and `font-family`; `font-family: "Comic Sans MS"` and `"Arial Narrow"`).
  - `unsupported.html` (`<script>alert(1)</script>`, `onclick`, `<img src="http://example.invalid/x.png">`, `<link rel=stylesheet href=...>`, `@import`, `display:flex`, `<a href>`).
  - `malformed-unclosed-attr.html`, `malformed-misnest.html`, `malformed-stray-end.html`.

  Next to each, add `<name>.expected-tree.txt` (an indented tree dump) and `<name>.expected-diagnostics.txt`.
- [X] T111 [P] [US5] Write contract test `tests/contract/test_html_parser.cpp`. For each fixture, `parseHtml` produces a tree whose dump equals `expected-tree.txt` and diagnostics equal to `expected-diagnostics.txt`. Entity decoding covers every listed entity. Script and style contents never become text nodes, `on*` attributes are absent from the tree, and a seeded 1 000-iteration fuzz loop never crashes.
- [X] T112 [P] [US5] Write contract test `tests/contract/test_presentcss.cpp`. It covers specificity ordering (inline > id > class > type, then source order), every supported property and value form, inheritance, `font-family` alias resolution to pack ids with fallback to the active pack, unsupported selectors, properties and values ignored with the right `W-CSS-*` codes, and `@import`/`url()` ignored.
- [X] T113 [P] [US5] Write visual test `tests/visual/test_html_preview.cpp`, which renders each fixture at width 480 and s=1 and checks goldens `html/<name>.bmp`, including the diagnostics banner for the malformed fixtures (FR-037).

### Implementation for User Story 5

- [X] T114 [US5] Implement the presentation-CSS parser in `src/presentcss/include/pxc/presentcss/stylesheet.hpp` and `src/presentcss/presentation_css.cpp` on top of `core::CssTokenizer`, as a parser separate from glyphcss (FR-041). It handles rules, the selector list (`tag`, `.class`, `#id`, `tag.class`), declarations, value parsing for the property table (colours including the 16 CSS 2.1 names and `rgb()`, `px`/`em`/number, `normal`) and `W-CSS-*` diagnostics. Create `src/presentcss/CMakeLists.txt` for `pxc_presentcss`.
- [X] T115 [US5] Implement the HTML tokenizer in `src/html/tokenizer.hpp` and `src/html/tokenizer.cpp`. Its states are data, tag open, end tag open, tag name, attribute name and value (all three quoting forms), self-closing, comment, doctype (skipped) and raw text for `script`/`style`. It decodes entities and tracks line and column. An unterminated tag at EOF produces `E-HTML-MALFORMED`.
- [X] T116 [US5] Implement the tree builder in `src/html/tree_builder.cpp`, header `src/html/include/pxc/html/document_node.hpp` (data-model §7). It provides implied `html`, `head` and `body`, the element table from the contract (dropped elements with their content, transparent unknown containers), the attribute whitelist (`id`, `class`, `style`), `on*` removal with diagnostics, implied end for `p`, stray-end-tag recovery, auto-close at EOF, whitespace collapsing, and `title` text extraction. `parseHtml(u32string_view) -> Expected<PreviewDocument, …>`. Create `src/html/CMakeLists.txt` for `pxc_html`, linking `pxc_presentcss` and `pxc_core`.
- [X] T117 [US5] Implement `computeStyles(PreviewDocument&, stylesheets, const FontRegistry&, activePackId)` in `src/html/style.cpp`: the cascade with specificity and source order, inheritance of `color`, `font-family` and `line-height`, the default `p` margins of 1em, alias-to-pack mapping via the manifest `--aliases`, and `display:none` removal.
- [X] T118 [US5] Implement block layout for the preview in `src/layout/include/pxc/layout/block_layout.hpp` and `src/layout/block_layout.cpp`. It flattens the styled tree into block boxes made of styled inline runs `{text, fg, bg, packId}`, wraps the runs across packs using the same greedy rules and `FontRegistry::resolve`, applies `margin-top`/`margin-bottom` and `line-height`, and emits `LayoutLine`s with per-glyph colour and pack plus block background rectangles. Extend `UpdateManager::paint` to draw run colours and backgrounds from the layout (shared pipeline, FR-037).
- [X] T119 [US5] Add preview mode across `src/editor/editor_controller.cpp`, `src/shell/file_commands.cpp` and `src/shell/main_window.cpp`:
  - Opening a `.html` or `.htm` file decodes it (R-11), parses it, computes styles and sets `mode = PreviewHtml`.
  - Editing commands are disabled; selection and copy work over the preview text.
  - Save is disabled, and the title reads `"<name> — Preview (read-only)"` (or the `<title>` text followed by that suffix).
  - A yellow diagnostics banner row is drawn with pack glyphs when there are errors.
  - A toolbar font change restyles the elements whose `font-family` did not resolve.
  - Wrap is forced on.
- [X] T120 [US5] Implement the `html` command in `tools/pixelcss-tool/cmd_html.cpp` (`--in`, `--out`, `--width`, `--pack`), a headless preview render to BMP, and register CTest `tool_html_basic` comparing it against the `html/basic.bmp` golden.

**Checkpoint**: US5 is complete. Quickstart §7 passes, and T103 still confirms no network imports.

---

## Phase 9: User Story 6 — Register, validate, and extend font packs (Priority: P2)

**Goal**: Strict pack validation, hot reload that invalidates only the changed glyphs, and adding a pack without engine changes.

**Independent Test**: Run `pixelcss-tool validate --strict fonts`, edit one glyph in a copied pack and reload it, and confirm only that glyph's cache entries are invalidated. A new pack directory appears in the dropdown (quickstart §2, §10).

### Tests for User Story 6 ⚠️

- [X] T121 [P] [US6] Create the pack fixtures in `tests/fixtures/packs/`:
  - `extra-pack/`: a conforming 95+1 pack. Generate it by script `tests/fixtures/packs/make_extra_pack.ps1` from simple 3×5 block glyphs, so it is clearly test-only.
  - `dup-id/`, `bad-metrics/`, `missing-license/`, `dup-glyph/` (with both `U+041.css` and `U+0041.css`), `extra-glyph/`, `bad-authenticity/`, `exceeds-metrics/` (a glyph with `baseline > ascent`), `untrimmed/` (a glyph with a transparent edge row). The `missing-glyph/` and `no-replacement/` fixtures already exist from T071 and are reused here, not recreated.
- [X] T122 [P] [US6] Write contract test `tests/contract/test_pack_validation.cpp`. Each fixture yields its expected `E-PACK-*` or `W-PACK-*` code and state (Valid, Degraded or Invalid). The bundled packs are each Valid with exactly 95 ASCII code points plus U+FFFD, for 285 ASCII files in total (SC-002). Every bundled manifest has non-empty origin, author, version, license and authenticity (US6-AS5). `extra-pack` registers next to the bundled packs without any code change (US6-AS3).
- [X] T123 [P] [US6] Write integration test `tests/integration/test_pack_reload.cpp`. Copy `fonts/verdana-style` to a temp directory, load it, and warm the cache with `ABC`. Rewrite `U+0042.css` with a changed cell and call `reload("verdana-style")`. The returned changed set is `{U+0042}`, `get` for `A` and `C` is a hit, `get` for `B` is a miss followed by a new raster that differs from the old one, and `packRevision` is incremented. Then corrupt `U+0043.css` and reload: the pack becomes Degraded and `C` resolves to the pack replacement with a diagnostic (US6-AS4).

### Implementation for User Story 6

- [X] T124 [US6] Implement `FontRegistry::reload(packId) -> ReloadResult{changedCodePoints, newState}` in `src/fonts/font_registry.cpp`. It re-reads the manifest and glyphs, compares each `contentHash`, replaces only the changed definitions, bumps `packRevision`, and makes `RasterCache` drop keys whose `glyphRevision` no longer matches (lazy check on `get` plus an eager `evictStale(packId)`) (FR-011).
- [X] T125 [US6] Extend `tools/pixelcss-tool/cmd_validate.cpp` with `--strict`, which treats warnings as errors and requires full coverage of the manifest's declared ranges. Print a coverage table per pack (`id | state | covered/declared | ASCII 95/95 | U+FFFD | provenance`) and a footer with the total ASCII glyph files. Register CTest `validate_bundled_fonts` running `validate --strict fonts` (SC-002).
- [X] T126 [US6] Support `--fonts-dir <dir>` in `src/shell/main.cpp`, with packs also merged from `<exe-dir>/fonts`, and sort the dropdown by `--order` then id in `src/shell/toolbar.cpp`. Add **View → Reload Fonts** (Ctrl+Shift+R, as listed in the ui-contract menu table) to `src/shell/resources.rc`. It calls `reload` for every pack, invalidates layout, repaints and refreshes the dropdown, while preserving the active pack when it is still valid and otherwise falling back to Verdana with a status notice.
- [X] T127 [P] [US6] Write `docs/fonts/glyph-schema.md` (a user-facing copy of the glyph contract with worked examples), `docs/fonts/adding-a-pack.md` (copy a pack, edit the manifest, author glyphs, `validate --strict`, `distinct`, generate goldens, restart or Reload Fonts) and `docs/fonts/design-notes.md` (the design intent and metrics of each bundled family, and the statement that all three are original artwork, not proprietary fonts) (FR-050, SC-012).

**Checkpoint**: All seven user stories are independently functional.

---

## Phase 10: Polish & Cross-Cutting Concerns

**Purpose**: DPI behaviour, robustness at scale, documentation, licensing review and final validation.

- [X] T128 Handle **runtime** DPI changes. The initial scale already comes from T065. Handle `WM_DPICHANGED` in `src/shell/main_window.cpp` and `src/shell/pixel_surface.cpp`: recompute the integer scale `s = max(1, floor(dpi/96 + 0.5))` with the same helper as T065 (R-08), apply the suggested window rectangle, and call `UpdateManager::onDpiChanged(s)`, which invalidates the layout and full viewport. Add visual test `tests/visual/test_scale.cpp` with s=2 goldens for `editor/basic.s2.bmp` and `glyphs/verdana-style/U+0041.s2.bmp`, and a unit test of the scale mapping for 96, 120, 144, 168, 192 and 240 DPI.
- [X] T129 [P] Add integration test `tests/integration/test_large_document.cpp` that generates a 10 000 000-character document in memory and asserts that opening it and painting the first viewport rasterizes no more than the on-screen distinct glyphs, and that after idle layout completes, at most 4 096 full `LayoutLine`s are resident (all other lines hold only `LineMetrics`, as the two-tier cache of R-12 requires). The process working set must stay ≤ 256 MB (R-17), and the test must complete within the 60 s test timeout.
- [X] T130 [P] Write the ADRs `docs/adr/0002-presenter-gdi-bitblt.md` (R-09), `docs/adr/0003-integer-scale-policy.md` (R-08) and `docs/adr/0004-glyph-css-encoding.md` (R-05, R-06) using the format Context, Decision, Consequences, Alternatives.
- [X] T131 [P] Rewrite the root `README.md`: project summary, the font authenticity disclosure, a quick build (presets), running, the testing labels, the repository layout, and links to `docs/` and `specs/001-pixelcss-browser/`.
- [X] T132 Run the licensing review and record it in `docs/fonts/licensing-review.md`: confirm every one of the 288 glyph files carries the original-artwork header comment, that no file was derived from proprietary outlines, that every manifest has `authenticity: style-interpretation` and OFL-1.1, and that third-party notices (doctest, test-only) are listed (Constitution XII).
- [X] T133 Fill `docs/perf/reference-machine.md` with the CPU, RAM, GPU (not used), Windows build, VS/MSVC/SDK versions and the `pixelcss-bench` results for every scenario on the reference machine, and replace `tests/perf/baseline.json` with the measured values from that run, setting `"measured": true` so the 20 % regression check becomes active (SC-009, SC-010, R-17). Run it with `PIXELCSS_REFERENCE_MACHINE=1` and confirm every absolute budget passes.
- [X] T134 Run `scripts/build-all.ps1` from a clean checkout and execute every section of `specs/001-pixelcss-browser/quickstart.md` (§1–§10), including the clean-VM ZIP check (§9). Record pass or fail per section in `specs/001-pixelcss-browser/checklists/release.md` and the Constitution release gates from spec.md as checked items with evidence links.
- [X] T135 [US5] View Source (FR-038 amendment): `View → Page Source` (Ctrl+U, `IDM_VIEWSOURCE`) in `src/shell/preview_mode.cpp`, `src/shell/main_window.cpp`, `src/shell/resources.rc`; `FileWorkflow` keeps the HTML source in the document in both views (`previewShown`/`setPreviewShown`/`isHtmlFile`, title "— Source"); bench command `source`; test "HTML view source…" in `tests/integration/test_editor_roundtrip.cpp`; docs in `contracts/ui-contract.md` and `README.md`.

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (Phase 1)** has no dependencies.
- **Foundational (Phase 2)** depends on Setup and **blocks all stories**.
- **US1 (Phase 3)** depends on Foundational. It provides the glyph parser, rasterizer, cache, the canonical `renderTextLine` (T038), the registry and the Verdana-style pack used by every later story. T034 creates the `pxc_fonts` target that T036, T037 and T038 extend.
- **US2 (Phase 4)** depends on US1, which supplies the rendering pipeline and Verdana glyphs.
- **US3 (Phase 5)** depends on US1. Its shell parts (T086–T089) also depend on US2's shell (T064–T070). The glyph artwork tasks T075–T082 can start right after US1's T036 and T039 exist.
- **US4 (Phase 6)** depends on US2 (UpdateManager, LayoutEngine, shell loop). The T091 oracle subtest uses font switching from US3.
- **US7 (Phase 7)**: T103 needs the `PixelCSS.exe` from US2. T104 and T106 need all three packs (US3). T105 needs the `--bench-script` mode from US4 (T099) and the leak-check listener (T022). The docs (T109) can start any time after Setup.
- **US5 (Phase 8)** depends on US1 and US2's layout and update. It is independent of US3, US4 and US6, but the `font-family` mapping test uses all three packs from US3.
- **US6 (Phase 9)** depends on US1. T126 depends on US3's toolbar.
- **Polish (Phase 10)** depends on all desired stories.

### Story completion order

```text
Setup → Foundational → US1 ─┬─> US2 ─┬─> US4 ──┐
                            │        ├─> US5    ├─> US7 (package) → Polish
                            ├─> US3 ─┴──────────┤
                            └─> US6 ────────────┘
```

### Within each story

- Write the tests first and confirm they fail.
- Order: data types → parsers and models → services (registry, layout, update) → shell integration.
- Generate goldens only after the implementation looks correct, and review each one visually.

### Parallel opportunities

- Setup: T003, T004, T006 and T007 in parallel after T001.
- Foundational: T009–T013 in parallel; T016, T017, T018, T019 and T023 in parallel after T014 and T015.
- US1: all test tasks T024–T029 in parallel; glyph artwork T040–T044 in parallel with each other and with T030–T039.
- US2: tests T047–T053 in parallel; T057, T058 and T067 in parallel with the document and layout work.
- US3: the **eight artwork tasks T075–T082 are fully parallel**, and tests T071–T074 are parallel.
- US4: tests T090–T092 are parallel.
- US5: fixtures and tests T110–T113 are parallel. The parsers T114 and T115 are parallel because they are separate modules.
- US6: T121–T123 and T127 are parallel.
- With multiple developers, US5 and US6 can proceed in parallel with US3 and US4 once US2 is done.

---

## Parallel Example: User Story 1

```text
# Tests together (all must fail first):
Task: "T024 Valid glyph fixtures in tests/fixtures/glyphs/valid/"
Task: "T025 Invalid glyph fixtures in tests/fixtures/glyphs/invalid/<CODE>/"
Task: "T026 Contract test tests/contract/test_glyphcss.cpp"
Task: "T027 Blend unit test tests/unit/render/test_blend.cpp"
Task: "T028 Glyph render visual test tests/visual/test_glyph_render.cpp"
Task: "T029 Raster cache unit test tests/unit/fonts/test_raster_cache.cpp"

# Artwork together, alongside parser and renderer implementation:
Task: "T041 verdana-style space + digits"
Task: "T042 verdana-style uppercase"
Task: "T043 verdana-style lowercase"
Task: "T044 verdana-style punctuation + U+FFFD"
```

## Parallel Example: User Story 3

```text
Task: "T076 arial-narrow-style space + digits"     Task: "T081 comic-style space, digits, uppercase"
Task: "T077 arial-narrow-style uppercase"          Task: "T082 comic-style lowercase, punctuation, U+FFFD"
Task: "T078 arial-narrow-style lowercase"          Task: "T071 font registry contract test"
Task: "T079 arial-narrow-style punctuation"        Task: "T072 font switch integration test"
```

---

## Implementation Strategy

### MVP first (User Story 1 only)

1. Phase 1 Setup, then Phase 2 Foundational.
2. Phase 3 US1: glyph parser, rasterizer, cache, Verdana-style pack and `pixelcss-tool render/compare/validate`.
3. **Stop and validate**: run quickstart §3. The core differentiator works: characters rendered purely from CSS.

### Incremental delivery

1. US1, the CSS glyph engine (MVP demo through the CLI).
2. US2, the working notepad in Verdana-style (first usable app).
3. US3, three fonts and the dropdown (meets the explicit product requirement).
4. US4, incremental and progressive rendering with benchmarks (constitutional performance gates).
5. US7, the reproducible package (**all P1 stories, the minimum releasable product**).
6. US5, the HTML preview, then US6, pack extensibility (completes v1 scope).
7. Polish, then the release checklist (T134).

### Parallel team strategy

After US2: Developer A takes US3 shell work and US4. Developers B and C take the artwork tasks T075–T082. Developer D takes US5. Developer E takes US6 and US7 docs and packaging.

---

## Notes

- `[P]` means different files with no dependency on incomplete tasks. `[USn]` maps the task to its spec story for traceability.
- Glyph artwork tasks must produce **original** pixel art. Never trace, sample or convert proprietary font outlines or screenshots (Constitution XII).
- Commit after each task or logical group, and do not commit goldens that have not been visually reviewed.
- At every checkpoint, run `ctest --preset x64-debug` and the matching quickstart section.
