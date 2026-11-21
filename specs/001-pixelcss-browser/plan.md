# Implementation Plan: PixelCSS Browser — Custom CSS Glyph Rendering and Notepad

**Branch**: `001-pixelcss-browser` | **Date**: 2026-11-21 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/001-pixelcss-browser/spec.md`

**Note**: This template is filled in by the `/speckit-plan` command; its definition describes the execution workflow.

## Summary

Build a native Win32 C++20 notepad-first, browser-like application, compiled with MSVC, in which **every document character is rasterized by our own engine** from per-code-point CSS glyph files. Three original pixel-font packs ship with it: Verdana-style, Arial Narrow-style and Comic Sans-style, each with 95 ASCII glyphs plus a replacement glyph.

The technical approach (see [research.md](./research.md)):
- Glyph files are valid CSS. Each is one `glyph {}` rule of custom properties, read by a dedicated parser. Cells support foreground (`currentcolor`) and background (`backgroundcolor`) substitution, and glyphs are authored as tight ink boxes.
- A manifest-driven `FontRegistry` resolves glyphs through a deterministic fallback chain.
- An LRU `RasterCache` is keyed by pack, style, revision, code point, scale and colours. Rasterization lives in `pxc_fonts`, so `pxc_render` depends only on `pxc_core`.
- An integer-math software `Framebuffer` is presented with dirty-rect `BitBlt`.
- A piece-table `TextDocument` of `char32_t` has a Fenwick line index.
- A two-tier `LayoutCache` (a bounded set of full lines plus per-line metrics) provides viewport-first progressive layout, with incremental re-wrapping inside long lines.
- An `UpdateManager` emits minimal dirty regions.
- Display preferences are persisted through `IPreferencesStore`.
- An end-to-end `--bench-script` mode measures latency, startup and resource stability through the real window.
- A restricted HTML and presentation-CSS read-only preview uses the same pipeline.
- The build is CMake with Visual Studio generator presets, `/W4 /WX`, a static CRT, doctest tests, golden BMP visual regression and a benchmark harness.

## Technical Context

**Language/Version**: C++20 (`/std:c++20 /permissive-`), MSVC `cl.exe`. Minimum supported: VS 2022 17.10 / v143. Reference: VS 2026 Build Tools 18.8, MSVC 14.51 (v145). (R-01)

**Primary Dependencies**: Windows SDK 10.0.26100.0 (Win32 user32/gdi32 pixel blit/comctl32/comdlg32/shell32 only, with no text-drawing APIs for documents) and the C++ standard library. The only third-party code is doctest 2.4.11 (tests only, vendored, MIT). (R-02, R-04)

**Storage**: Local files. Plain text in UTF-8 (BOM preserved; UTF-16 with BOM is read). Font packs are CSS files under `fonts/`. Display preferences (last font, word wrap) are stored in `HKCU\Software\PixelCSS\Display` through `IPreferencesStore`; failures are non-fatal. (R-11, R-20)

**Testing**: doctest + CTest, with labels `unit`, `contract`, `integration`, `visual` (golden 32-bit BMP, pixel-exact at s=1 and s=2) and `perf` (`pixelcss-bench`, regression vs baseline). (R-04, R-16, R-17)

**Target Platform**: Windows 11 x64 (Windows 10 x64 best-effort), Per-Monitor-V2 DPI aware.

**Project Type**: Native desktop application, plus a developer CLI (`pixelcss-tool`) and a benchmark executable (`pixelcss-bench`).

**Performance Goals**: p95 keystroke-to-present ≤ 50 ms (end to end) and first viewport ≤ 1 s for a 100 k-character document, both ratified in R-17. Internal budgets:
- startup to first present ≤ 750 ms;
- raster miss ≤ 20 µs;
- non-reflowing edit layout ≤ 1 ms;
- long single-line keystroke layout ≤ 5 ms;
- cache hit rate ≥ 99 % in steady state.

**Constraints**: Offline. No WebView2, Chromium or browser engine. No TrueType/OpenType/DirectWrite/GDI text for document glyphs. Deterministic pixel output. Memory limits:
- idle memory ≤ 64 MB with a 100 k-character document, and ≤ 256 MB with a 10 M-character document;
- raster cache ≤ 16 MiB;
- layout full tier ≤ 4 096 lines or 32 MiB;
- no GDI, USER or handle growth across repeated open/close cycles.

**Scale/Scope**: 3 bundled packs × 96 glyph files (288 files; ≥ 285 ASCII). About 10 engine modules. Documents up to 100 k characters as the performance target, and 10 M characters must still open with progressive layout. An HTML subset of 8 elements and 7 presentation properties.

All former NEEDS CLARIFICATION items are resolved in [research.md](./research.md) (summary table at its end).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| # | Principle / gate | How this plan complies | Pre-design | Post-design |
|---|---|---|---|---|
| I | Independent character rendering | `render` + `fonts` rasterize glyph matrices with integer blending into our `Framebuffer`. No font files and no OS rasterizer for document text. The engine replacement glyph is generated in code (R-07, R-09) | ✅ | ✅ |
| II | CSS as source of geometry | One valid-CSS `glyph {}` rule per file, with an explicit parser and location-aware diagnostics. It supports foreground (`currentcolor`) and background (`backgroundcolor`) substitution, `--meta-*` per-character metadata, baseline and vertical alignment through the tight-ink-box convention ([glyph-css-schema](./contracts/glyph-css-schema.md)) | ✅ | ✅ |
| III | Multiple independent families | Three independent directories and manifests. Original artwork with family-specific metrics (R-18). `distinct` tool check. New packs are discovered by directory scan | ✅ | ✅ |
| IV | Font dropdown | Toolbar `CBS_DROPDOWNLIST` defaulting to Verdana. Font switch is presentation-only, caret and selection are preserved, and fallback is predictable ([ui-contract](./contracts/ui-contract.md), data-model §11) | ✅ | ✅ |
| V | Minimal HTML model | Own tokenizer, tree builder and `DocumentNode`. Documented unsupported behaviour. No script, no network ([html-subset](./contracts/html-subset.md)) | ✅ | ✅ |
| VI | Progressive & incremental | Viewport-first layout with idle slices, a bounded two-tier layout cache, incremental re-wrap within a line, dirty regions and selective blits. Never re-rasterizes the whole document per keystroke (R-12, R-13) | ✅ | ✅ |
| VII | Pixel framebuffer, no per-pixel DOM | Raster cache of glyph bitmaps keyed by family, code point, size, **style**, colours and glyph revision, as the constitution lists. CPU backend. Direct2D deferred behind `IPresenter` (R-09, R-14) | ✅ | ✅ |
| VIII | Notepad-first | Full editing command set, a piece table justified by benchmark (R-10), Unicode-safe `char32_t` model, text-only clipboard, atomic save | ✅ | ✅ |
| IX | Modular architecture | 10 modules as static libraries with an acyclic graph. `render` depends only on `core`. Win32 confined to `shell`/`storage-win32`. File Storage also owns display preferences (R-20) ([engine-interfaces](./contracts/engine-interfaces.md)) | ✅ | ✅ |
| X | Language & platform | C++20, MSVC, Win32, CMake presets, RAII, `Expected<>` error handling, no network | ✅ | ✅ |
| XI | Deterministic QA | Unit, contract, integration, golden-image and perf suites covering every listed area, **including resource cleanup** (CRT leak checks, GDI/USER/handle stability). Perf measures **startup time**, first render, end-to-end keystroke latency, **cache hit rate**, memory, repaint area and scrolling. Budgets established before optimisation (R-16, R-17) | ✅ | ✅ |
| XII | Font licensing | OFL-1.1 original artwork, provenance in manifests, `authenticity` field shown in the UI (R-18) | ✅ | ✅ |
| XIII | Controlled scope | No JS, HTTP, flex/grid, GPU or shaping. The only extras are a CLI and a bench tool, both needed for the validation gates | ✅ | ✅ |
| Spec gates | One CSS file per glyph, MSVC `cl.exe` C++20, Debug/Release, notepad-first renderer | Enforced by the `validate --strict` CTest, the configure-time compiler check, and presets | ✅ | ✅ |

**Result**: PASS. No violations, so Complexity Tracking is empty.

## Project Structure

### Documentation (this feature)

```text
specs/001-pixelcss-browser/
├── plan.md              # This file (/speckit-plan command output)
├── research.md          # Phase 0 output (/speckit-plan command)
├── data-model.md        # Phase 1 output (/speckit-plan command)
├── quickstart.md        # Phase 1 output (/speckit-plan command)
├── contracts/           # Phase 1 output (/speckit-plan command)
│   ├── glyph-css-schema.md
│   ├── font-manifest-schema.md
│   ├── html-subset.md
│   ├── engine-interfaces.md
│   └── ui-contract.md
├── checklists/requirements.md
└── tasks.md             # Phase 2 output (/speckit-tasks command - NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
CMakeLists.txt
CMakePresets.json                 # x64-debug, x64-release (VS 18 2026 / VS 17 2022), package preset
cmake/
├── MsvcGuard.cmake               # rejects non-MSVC / clang-cl (FR-042)
├── CompilerWarnings.cmake        # /W4 /WX /permissive- /utf-8 …
└── Packaging.cmake               # CPack ZIP layout (FR-048)

src/
├── core/          # pxc_core: Expected, Diagnostic(s), Rect, Color, UTF codecs, Clock
├── glyphcss/      # pxc_glyphcss: shared CSS tokenizer + glyph & manifest parsers
├── render/        # pxc_render (→ core only): Framebuffer, Renderer (blend, clip, caret), RasterGlyph, BMP I/O, IPresenter
├── fonts/         # pxc_fonts: FontRegistry, fallback chain, rasterize(), RasterCache, renderTextLine()
├── document/      # pxc_document: piece table TextDocument, line index, undo/redo
├── storage/       # pxc_storage: load/save, encodings, EOL, IPreferencesStore; win32/ IFileSystem + HKCU preferences impl
├── layout/        # pxc_layout: LayoutEngine, LayoutCache, Viewport, hit-testing, idle layout
├── update/        # pxc_update: UpdateManager, DirtyRegion, RenderStats
├── editor/        # pxc_editor: EditorController, EditorState, commands, IClipboard
├── presentcss/    # pxc_presentcss: presentation-CSS parser + cascade
├── html/          # pxc_html: tokenizer, tree builder, PreviewDocument, style computation
└── shell/         # PixelCSS.exe: WinMain, main window, toolbar, menus, dialogs, PixelCSSSurface,
                   #   Win32 clipboard, GdiPresenter, DPI handling, Diagnostics/About dialogs, resources (.rc)

tools/
├── pixelcss-tool/ # validate, render, compare, distinct, html (headless; links engine libs)
└── pixelcss-bench/# perf scenarios → JSON

fonts/
├── verdana-style/       { manifest.css, LICENSE.txt, glyphs/U+0020.css … U+007E.css, U+FFFD.css }
├── arial-narrow-style/  { … same layout … }
└── comic-style/         { … same layout … }

tests/
├── support/       # golden harness, fakes (clipboard, file system, prompts, preferences), CRT leak-check listener
├── unit/          # per-module internals
├── contract/      # one file per public module header
├── integration/   # editor flows, file round-trip, font switch, failure injection, no-network imports
├── visual/        # golden comparisons (glyphs per pack, transparency, baseline, clipping, wrapping, HTML)
├── perf/          # bench baselines + regression check
├── fixtures/      # glyphs/{valid,invalid/<code>}, packs/, html/, text/ (CRLF, LF, mixed, BOM, invalid UTF-8), perf/100k.txt
└── golden/        # approved BMPs; regenerate with PIXELCSS_UPDATE_GOLDENS=1 (review required)

third_party/doctest/     # doctest.h + LICENSE
docs/
├── build/{building.md, debugging.md, warnings.md}
├── fonts/{glyph-schema.md, adding-a-pack.md, design-notes.md}
├── perf/reference-machine.md
└── adr/                 # architecture decision records (text buffer, presenter, scale policy)
```

**Structure Decision**: One CMake project with one static library per constitutional module (`src/<module>`), a thin `shell` executable, two tool executables and a single `tests/` tree. Only `src/shell`, `src/storage/win32` and the tools' entry points include `<windows.h>`. This keeps the parsing, glyph, layout and rasterization logic platform-independent and unit-testable (Constitution IX, X; FR-045).

## Milestone mapping (input for `/speckit-tasks`)

| Milestone | Modules / artefacts | Exit evidence |
|---|---|---|
| M1 Glyph contract & renderer | core, glyphcss, render, `pixelcss-tool render/compare/validate`, first glyphs (A, digits, punctuation) | glyph fixtures pass; `U+0041` golden pixel-exact (US1) |
| M2 Core editor | document, storage, layout (no-wrap + wrap), editor, shell window/menus/surface | quickstart §4 manual + integration tests (US2) |
| M3 Three fonts | 288 glyph files, manifests, fonts registry & fallback, dropdown, About Fonts | `validate --strict` and `distinct` pass; font-switch goldens (US3, US6) |
| M4 Incremental engine | RasterCache stats, update, two-tier idle layout, intra-line re-wrap, RenderStats overlay, `--bench-script`, bench | SC-006 counters; SC-009/010, startup, hit rate, resource stability on reference machine (US4) |
| M5 HTML preview | presentcss, html, preview mode | HTML goldens + no-network test (US5) |
| M6 Release readiness | CPack, docs, licensing review, perf baselines, clean-VM check | quickstart §1, §9, §10 (US7, SC-011, SC-012) |

## Complexity Tracking

No constitution violations. This section is intentionally empty.
