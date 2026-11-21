# Phase 0 Research: PixelCSS Browser

**Feature**: [spec.md](./spec.md) | **Plan**: [plan.md](./plan.md) | **Date**: 2026-11-21

Each entry records a decision that resolves an open item from the Technical Context, the spec's Assumptions ("will be pinned/detailed in planning"), or checklist Note 3.

---

## R-01 Toolchain pinning

- **Decision**: Minimum supported toolchain is Visual Studio 2022 17.10+ (MSVC v143, 14.40+). The **reference toolchain** used for golden images, benchmarks and CI is Visual Studio 2026 Build Tools 18.8 with MSVC **14.51** (v145), Windows SDK **10.0.26100.0**, CMake **≥ 3.28** (reference machine: 4.4.2). The SDK version is pinned through `CMAKE_SYSTEM_VERSION=10.0.26100.0` in `CMakePresets.json`.
- **Rationale**: The reference machine already has these installed (detected via `vswhere`). Pinning a toolchain for goldens satisfies FR-022/FR-044, and keeping v143 as the minimum still meets the "VS 2022 or newer" requirement. The rasterizer uses only integer arithmetic, so output does not depend on the compiler's floating-point code generation.
- **Alternatives considered**: Pinning only v143 was rejected because v143 is not installed on the reference machine. An unpinned "latest" toolchain was rejected because it breaks reproducibility.

## R-02 Build system and generator

- **Decision**: CMake with `CMakePresets.json`. Presets are `x64-debug` and `x64-release`, using the generator `Visual Studio 18 2026` (alternate presets use `Visual Studio 17 2022`), plus `-A x64` and `-T host=x64`. The presets also emit a `.sln`/`.slnx` that can be opened in Visual Studio for debugging. A configure-time check rejects any compiler where `MSVC` is false or that reports a non-`cl` frontend (`CMAKE_CXX_COMPILER_FRONTEND_VARIANT` must be `MSVC` and the compiler ID must be `MSVC`, which rejects clang-cl).
- **Compiler flags** (own targets): `/std:c++20 /permissive- /W4 /WX /utf-8 /Zc:__cplusplus /Zc:preprocessor /EHsc /sdl /guard:cf`. Release adds `/O2 /GL` with `/LTCG`. Vendored third-party headers are included as `SYSTEM` with `/external:W0`.
- **Rationale**: FR-042–FR-046. `/WX` keeps the `/W4` quality bar enforced instead of merely advisory. Any warnings that are suppressed are listed in `docs/build/warnings.md`, as FR-046 requires.
- **Alternatives considered**: A hand-maintained `.vcxproj` was rejected because it is harder to diff and reproduce. Ninja with MSVC was not rejected and is offered as an optional preset, but it is not the primary one because Visual Studio debugging is a stated user story.

## R-03 Runtime linkage and packaging

- **Decision**: Release builds link the static CRT (`MSVC_RUNTIME_LIBRARY = MultiThreaded`, and `MultiThreadedDebug` for Debug). Packaging uses CPack to produce a ZIP containing `PixelCSS.exe`, `pixelcss-tool.exe`, `fonts/**` (manifests and all glyph CSS), `LICENSES/`, and `README.txt`.
- **Rationale**: FR-048 and SC-011. Linking the CRT statically removes the VC++ Redistributable prerequisite, so the ZIP runs on a clean Windows 10/11 x64 machine. No installer is needed for v1.
- **Alternatives considered**: Dynamic CRT plus a bundled `vc_redist.x64.exe` was rejected as an extra install step. MSIX was rejected because signing adds friction and the spec does not require it.

## R-04 Test framework

- **Decision**: **doctest 2.4.11**, a single header under the MIT license, vendored at `third_party/doctest/`. It is registered with CTest through `doctest_discover_tests`, so tests run from `ctest --preset x64-debug` and from Visual Studio Test Explorer (CTest adapter).
- **Rationale**: The constitution prefers small, well-justified dependencies. doctest is one header, needs no network at configure time, and compiles clean under `/W4 /WX` when included as SYSTEM.
- **Alternatives considered**: GoogleTest via FetchContent was rejected because it needs network access at configure time and is heavier. The Microsoft C++ Unit Test Framework was rejected because it ties tests to the VS IDE and makes CLI/CI use awkward. An in-house harness was rejected as reinventing a solved problem.

## R-05 Glyph CSS grammar (FR-001–FR-006)

- **Decision**: Every glyph file is **syntactically valid CSS**. It holds one `glyph { … }` rule whose declarations are custom properties. The schema version is declared with `--schema: "pixelcss-glyph/1"`. Rows are a sequence of CSS strings, and each string holds space-separated cell tokens: `#RRGGBB`, `#RRGGBBAA`, `transparent` (short alias `.`), `currentcolor` (short alias `@`), or `backgroundcolor` (short alias `~`). Optional per-character metadata uses `--meta-<name>` properties. Glyphs are authored as tight ink boxes (see the contract's "Authoring convention"). The full grammar is in [contracts/glyph-css-schema.md](./contracts/glyph-css-schema.md).
- **Rationale**: Ordinary CSS tooling (editors, linters) can open the files without errors, while a dedicated parser gives them their glyph meaning, satisfying FR-006 and Constitution II. `currentcolor` and `backgroundcolor` provide the constitution's "optional foreground **and background** colour substitution", so HTML `color`/`background-color` and selection inversion work without separate glyph variants. Literal hex cells still satisfy FR-004 and multi-colour glyphs.
- **Alternatives considered**: A `box-shadow` pixel-art encoding was rejected because it is hard to validate row lengths and cannot express a declared width or height. Custom at-rules (`@glyph`) were rejected because generic CSS tools flag them. A palette or indexed-colour section was deferred because literal tokens plus the two aliases are enough for v1.

## R-06 Font manifest format (FR-012)

- **Decision**: Each pack ships `fonts/<pack-id>/manifest.css`, a `font-pack { … }` rule written in the same tokenizer dialect as glyph files (see [contracts/font-manifest-schema.md](./contracts/font-manifest-schema.md)). Packs are discovered by scanning `fonts/*/manifest.css` next to the executable. A pack is registered only after its manifest validates.
- **Rationale**: Reusing the CSS tokenizer means no JSON parser dependency. It stays human-editable and keeps one tokenizer under test. The manifest, glyph and presentation dialects have **separate** parsers on top of the shared tokenizer, which satisfies FR-041.
- **Alternatives considered**: JSON with nlohmann/json was rejected as an extra dependency for about ten fields. A custom INI format was rejected because it is a second hand-written lexer.

## R-07 Fallback chain (FR-010, US6-AS4)

- **Decision**: The resolution order for code point `cp` in requested pack `P` is:
  1. `P/glyphs/U+cp.css`, if it is present and valid.
  2. `P/glyphs/U+FFFD.css`, which is P's own replacement glyph. Each bundled pack ships one in addition to its 95 ASCII glyphs.
  3. The **engine replacement glyph**: a hollow box whose size comes from P's metrics. The engine builds it in code as a `GlyphDefinition`, so it can never fail to load.

  If pack `P` itself is unavailable, the family falls back to `verdana-style`, and then to an engine-only "box font" in which every code point maps to step 3. Each distinct `(pack, cp, reason)` failure is recorded once in the `DiagnosticsLog`, which the user can view from **Help → Diagnostics…**.
- **Rationale**: The fallback is deterministic, it cannot fail in the final step, and it shows the problem visibly (FR-010).
- **Alternatives considered**: Falling back to another pack's real glyph for `cp` was rejected because silently mixing families is confusing and makes visual regression results nondeterministic across pack states.

## R-08 Pixel scale and DPI (edge case: 100/125/150 % DPI; FR-015)

- **Decision**: The process is Per-Monitor-V2 DPI aware. The document render scale is an **integer** `s = max(1, floor(dpi / 96 + 0.5))`, so 100 % → 1, 125 % → 1, 150 % → 2, 175 % → 2, 200 % → 2 and 250 % → 3. Glyph rasters are produced at scale `s` by nearest-neighbour cell replication. All document coordinates are integers in device pixels. **The reference scale for pixel-exact tests is s = 1.** Scale-2 goldens cover replication. The initial scale is computed from `GetDpiForWindow` when the surface is created (US2). `WM_DPICHANGED` updates it later when the window moves between monitors.
- **Rationale**: Integer scaling keeps cells crisp and deterministic (FR-022) and avoids filtering.
- **Alternatives considered**: Fractional scaling with filtering was rejected because it is nondeterministic across machines and violates "predictable nearest-neighbour". A user zoom setting is deferred because it is not in the spec.

## R-09 Framebuffer and presentation

- **Decision**: The renderer owns a `Framebuffer` holding 32-bit **straight-alpha** BGRA pixels in top-down rows, sized to the client area. Compositing uses integer "source-over" blending of glyph cells onto an opaque background: `out = (src·a + dst·(255−a) + 127) / 255`. The shell presents dirty rectangles by copying them into a DIB section and calling `BitBlt` inside `WM_PAINT`. That is a pixel copy and does not draw any text.
- **Rationale**: Integer math gives bit-exact output across CPUs (FR-022). A GDI pixel blit is allowed because only *text drawing* APIs are banned. Keeping it in the shell keeps the rasterizer independent of windowing (FR-045).
- **Alternatives considered**: Direct2D composition is allowed by Constitution X but is not needed for v1 and adds device-loss handling, so it is deferred behind the `IPresenter` seam. Premultiplied alpha was rejected because straight alpha keeps the golden-image comparison simpler and the blend count is low.

## R-10 Text buffer (Constitution VIII: "piece table or rope, justified by measurement")

- **Decision**: A **piece table** over `char32_t` code points (an original buffer plus an append buffer), with the pieces held in a `std::vector`. A **line-start index** (a Fenwick tree of per-line lengths) is maintained incrementally on each edit. Logical offsets everywhere are code-point indices.
- **Rationale**: At the 100 k-character target, insertion into the vector-of-pieces is O(pieces), and pieces stay in the low thousands because typing coalesces. Undo stores piece operations instead of copying text. The Fenwick line index gives O(log n) offset↔line lookups for caret, viewport and invalidation. Using `char32_t` makes "one offset = one code point = one glyph lookup", which keeps caret and selection preservation trivially exact (SC-004). Memory for 100 k chars is about 400 KB. The benchmark task **T-bench-buffer** must confirm p95 edit cost below 1 ms at 100 k and 1 M characters before this is ratified. If it is too slow, swap to a balanced-tree piece table behind the same `ITextBuffer` interface.
- **Alternatives considered**: A rope is more complex and offers no benefit at this scale. A gap buffer is fine for local edits but poor for undo snapshots and multiple selection anchors. UTF-8 storage was rejected because it makes code-point offset ↔ byte mapping a constant source of bugs.

## R-11 Line endings, tabs, encodings (FR-032, edge cases)

- **Decision**:
  - Internally only `\n` is stored. On open, the file's EOL style is detected (`CRLF`, `LF`, or `Mixed`) and remembered. Save writes the remembered style. For `Mixed` files the user is warned once in the status bar and CRLF is used. New documents use CRLF.
  - Encoding: UTF-8 with or without a BOM. Whether a BOM was present is remembered and preserved. UTF-16 LE/BE with a BOM is decoded and reported in the status bar, and saving converts it to UTF-8 after a confirmation prompt. If UTF-8 is invalid, a dialog offers **Open with replacement (U+FFFD)** or **Cancel**. When opened with replacement, the document is marked modified and the title shows "(converted)".
  - Saving is atomic: write to `<name>.pixelcss-tmp`, flush, then `ReplaceFileW` or `MoveFileExW(MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)`. Failure reasons (access denied, sharing violation, disk full) are mapped to readable messages, and the buffer is never cleared (FR-034).
  - A tab advances to the next multiple of `4 × advance(U+0020)` of the active pack and is rendered as blank space.
- **Rationale**: Byte-exact round-tripping for the common cases, explicit handling for the rest.
- **Alternatives considered**: Always normalising to CRLF was rejected because it silently changes LF files.

## R-12 Layout, wrapping and progressive first paint (FR-017–FR-020, SC-010)

- **Decision**:
  - The **LayoutCache** has two tiers, both keyed by `(lineRevision, fontPackId, packRevision, scale, wrapWidth|NoWrap)`:
    - **Full tier**: a bounded LRU of complete `LayoutLine`s (visual rows with `PositionedGlyph`s), capped at **4 096 logical lines** or **32 MiB**, whichever is reached first. It holds the lines around the viewport and recently edited lines.
    - **Metrics tier**: a compact `LineMetrics { visualRowCount, height, exact }` array (about 16 bytes per logical line) for every line.

    Idle background layout computes exact **metrics** for every line, but keeps full glyph positions only while a line is in the full tier. Lines evicted from the full tier keep their metrics and are re-laid out on demand when they scroll back into view.
  - Wrap positions are greedy: break at the last space that fits, otherwise break mid-word.
  - Total document height for the scrollbar uses **estimated** visual-line counts for lines not yet laid out: `ceil(chars × avgAdvance / wrapWidth)`, or 1 when wrap is off. Exact values replace the estimates as layout progresses. The scroll anchor is the logical (line, offset) at the top of the viewport, so refinement never makes the content jump.
  - **First paint** lays out and rasterizes only the visible lines plus one screen of overscan. Remaining lines are laid out in **idle time-slices** (≤ 4 ms per slice, driven from the message loop with `MsgWaitForMultipleObjectsEx`) on the UI thread.
- **Rationale**: This gives viewport-first behaviour (FR-018) with deterministic, single-threaded state. 100 k chars is about 2 k lines, so full background layout finishes within a few hundred ms of idle time. Memory for layout is bounded by the full tier plus about 16 bytes per line, so a 10 M-character document (around 200 k lines) needs only about 3 MB of metrics.
- **Alternatives considered**: A worker-thread layout was rejected because it needs synchronisation and complicates determinism, with little gain at this scale. It can be revisited behind the same interface if SC-009 fails.

## R-13 Invalidation and dirty regions (FR-019–FR-021, SC-006)

- **Decision**: The `UpdateManager` turns an edit `(offset, removed, inserted)` into:
  1. The affected logical lines (for a newline insert/delete: that line plus a shift of the line index).
  2. A re-layout of the affected lines only, done **incrementally inside the line**:
     - With wrap on, re-wrapping starts at the visual row that contains the edit offset. It stops as soon as a newly produced row break equals an old row break shifted by the edit delta (resync), and all later rows are reused with their offsets shifted.
     - With wrap off, the row's glyph vector is spliced in place, and the x-positions after the edit are shifted by the advance delta.

     Either way, a keystroke in a 100 k-character single-line document costs O(row length) at worst, never a re-resolve and re-rasterize of the whole line. If an affected line's visual-line count is unchanged and later lines only shift vertically, the shifted visible rows are marked dirty. Otherwise everything from the affected row to the viewport bottom is marked dirty.
  3. Within a single non-reflowing visual row, the dirty rectangle runs from the edit's x-position to the end of the row's previously or newly inked extent, plus the old and new caret rectangles.

  Font change, wrap toggle, DPI change and resize invalidate the LayoutCache entries for visible lines (and let idle re-layout handle the rest) and mark the full viewport dirty. Dirty rectangles are merged into a `DirtyRegion` (at most 16 rectangles, then they collapse into a bounding box).
- **Rationale**: These are exactly the observable guarantees in US4 and SC-006, and they are measurable through `RenderStats`.
- **Alternatives considered**: Repainting the whole viewport on every keystroke is simpler but fails FR-019 and SC-006.

## R-14 Glyph raster cache (FR-011, Constitution VII)

- **Decision**: `RasterCacheKey = { packId, style, packRevision, codePoint, glyphRevision, scale, fgColor, bgColor }`. `style` is always `"regular"` in v1 but is part of the key so future multi-style packs cannot collide (Constitution VII). The value is a `RasterGlyph` (BGRA pixels with `currentcolor` resolved to fg and `backgroundcolor` resolved to bg). Transparent cells always stay transparent, and the background is **never** pre-composited into the raster. This gives a single, deterministic cache content. The cache is an LRU with a 16 MiB budget, which is far above three packs × 96 glyphs × a few colours. A pack reload bumps `glyphRevision` only for files whose content hash (FNV-1a 64) changed, so only those entries become unreachable and are evicted (US6 independent test).
- **Rationale**: The key fields are what the constitution lists. Hash-based revisions make "only relevant entries invalidated" provable in tests.
- **Alternatives considered**: Caching per `(cp, pack)` only and tinting at blit time is cheaper on memory but adds per-pixel work every paint, and it does not serve multi-colour glyphs any better.

## R-15 Restricted HTML and presentation CSS (FR-035–FR-041)

- **Decision**:
  - The HTML tokenizer follows a simplified version of the HTML5 tokenizer states: data, tag open, attributes, comment, and raw text for `script`/`style`. The tree builder supports `html, head, body, title, div, p, span, br`, plus `style` in head. Unknown elements become **transparent containers**: their children are rendered inline and a diagnostic is recorded. `script`, `noscript`, `iframe`, `object`, `embed` and `img` are dropped along with their content. `on*` attributes are discarded. `href` and `src` are kept as inert strings and never fetched. The parser recovers from mis-nesting by auto-closing to the nearest matching open element. Entities supported: `&amp; &lt; &gt; &quot; &#39; &#NNN; &#xHH;`.
  - Presentation CSS: selectors are type, `.class`, `#id`, and the `style` attribute, with specificity from CSS 2.1 and source order. Properties: `color`, `background-color`, `font-family` (maps `Verdana`, `"Arial Narrow"` and `"Comic Sans MS"`/`"Comic Sans"` to the packs), `margin-top`, `margin-bottom`, `line-height` (a number or `px`). `color` and `font-family` are inherited. Anything else is ignored with a diagnostic. Contract: [contracts/html-subset.md](./contracts/html-subset.md).
  - The HTML preview is a read-only `PreviewDocument` that lays out through the same `LayoutEngine` and `Renderer` as the editor (FR-037).
- **Rationale**: This covers the fixture in US5 with predictable degradation and no network or script surface (FR-040).
- **Alternatives considered**: Embedding a third-party HTML parser (for example Gumbo) was rejected as unnecessary for the subset and an added dependency.

## R-16 Instrumentation and test hooks (FR-023, SC-006)

- **Decision**: `RenderStats` holds per-frame counters: `cacheHits`, `cacheMisses`, `cacheHitRate`, `rasterizations`, `linesLaidOut`, `dirtyRectCount`, `dirtyPixelArea`, `paintMicros`, `layoutMicros`, `keyToPresentMicros`. Counters reset at each `beginFrame()`. Process-level samples (`gdiObjects`, `userObjects`, `handleCount`, `workingSetBytes`, `firstPresentQpc`) are recorded at startup, after first present and at exit.

  They are exposed:
  - (a) through the `IRenderObserver` interface to tests;
  - (b) through **View → Render Statistics** (an overlay drawn in the custom framebuffer, using a pack glyph set, not OS text);
  - (c) as `--stats-json <file>` on `PixelCSS.exe` for scripted benchmarks.

  `keyToPresentMicros` is measured with **QPC**. The timestamp is taken when the input message is received in the window procedure (or when a `--bench-script` step is injected), and the end time is taken after the last `BitBlt` of the frame completes. It is never derived from `GetMessageTime`, whose resolution is too coarse.
  - `--bench-script <file>` runs a deterministic script of UI actions through the real window and presenter. The script format is in [contracts/ui-contract.md](./contracts/ui-contract.md). It provides true end-to-end latency for SC-009 and drives the resource-cleanup cycles.
- **Rationale**: One source of truth for tests, users and benchmarks.

## R-17 Performance budgets (SC-009, SC-010)

- **Decision**: The budgets are **ratified as MUST targets** (spec SC-009 and SC-010 updated accordingly):
  - p95 keystroke-to-present ≤ 50 ms, measured end to end through the real window using `--bench-script`.
  - First viewport ≤ 1 s for a 100 k-character document.

  They are measured by `pixelcss-bench` on the reference machine (recorded in `docs/perf/reference-machine.md`). Additional internal budgets are pass/fail in benchmark CI:
  - **Startup** (process creation to first present, empty document) ≤ 750 ms.
  - p95 single-glyph raster miss ≤ 20 µs.
  - p95 non-reflowing edit layout ≤ 1 ms.
  - p95 keystroke layout in a 100 k-character **single-line** document ≤ 5 ms.
  - Steady-state glyph-cache **hit rate** ≥ 99 % during the scroll and typing scenarios.
  - Idle memory ≤ 64 MB with a 100 k-character document open.
  - Idle memory ≤ 256 MB with a 10 M-character document open.
  - Zero growth in GDI objects, USER objects and handles across 50 open/close/font-switch/resize cycles. Benchmarks report results and are not part of the unit-test pass/fail gate. The `perf` CTest label enforces three things:
  - **Absolute budgets** apply only on the reference machine, signalled by `PIXELCSS_REFERENCE_MACHINE=1`, because the budgets are defined for that hardware.
  - **A regression greater than 20 %** against a *measured* committed baseline fails on any machine. The initial baseline is marked `"measured": false` until the reference run is committed, and the regression check is skipped until then.
  - **Resource-growth checks** apply on every machine.
- **Rationale**: Constitution XI requires reproducible benchmarks before budgets are finalised. These numbers are conservative for a CPU rasterizer of tiny glyphs.

## R-18 Font artwork and licensing (FR-008, FR-009, FR-012, FR-013, Constitution XII)

- **Decision**: All 3 × (95 + 1 replacement) glyphs are **original pixel artwork** authored for this project, licensed under the **SIL Open Font License 1.1**, author "PixelCSS contributors". Each manifest declares `--authenticity: "style-interpretation"`. Nominal metrics:

  | Pack | Display label | Line height | Ascent / Descent | Cap height | Ascender | x-height | Typical advance | Character |
  |------|---------------|-------------|------------------|------------|----------|----------|-----------------|-----------|
  | `verdana-style` | Verdana | 15 | 11 / 3 | 10 | 11 | 7 | 7–8 | wide, open counters, large x-height |
  | `arial-narrow-style` | Arial Narrow | 15 | 11 / 3 | 10 | 11 | 6 | 4–6 | condensed, tight sidebearings |
  | `comic-style` | Comic Sans | 16 | 12 / 3 | 11 | 12 | 7 | 6–8 | rounded terminals, irregular stroke |

  All glyphs follow the tight-ink-box authoring convention in the glyph contract.

  No proprietary outlines are traced or converted. The dropdown shows the label with a "(style)" suffix in its tooltip, and **Help → About Fonts** shows the manifest provenance (FR-013, US3-AS6). A `pixelcss-tool distinct` check compares the packs and fails if any pair has more than 60 % identical glyph matrices after normalising to the same size (FR-009 evidence).
- **Rationale**: Safe to distribute and verifiably independent.
- **Alternatives considered**: Licensing the authentic Microsoft fonts was rejected because it is not available for conversion and redistribution in this project.

## R-19 Native UI shell

- **Decision**: A Win32 top-level window with a classic menu bar (File, Edit, View, Help) and a toolbar built from a `ComboBox` (`CBS_DROPDOWNLIST`) for the font family plus a "Word Wrap" toggle button. A custom child window `PixelCSSSurface` receives keyboard, IME-free `WM_CHAR`, mouse and scroll input and presents the framebuffer. A status bar shows Ln/Col, encoding, EOL style and font. The UI contract is in [contracts/ui-contract.md](./contracts/ui-contract.md). The common controls used for chrome rely on the system font, which the spec allows.
- **Rationale**: FR-024, FR-025, FR-033 and FR-045. No `EDIT` or `RichEdit` control is used for the document.

## R-20 Display preferences (Constitution IX "File Storage … associated display preferences")

- **Decision**: `pxc_storage` defines an `IPreferencesStore` with `load() -> Preferences{lastPackId, wordWrap}` and `save(Preferences)`. The Win32 implementation stores the values under `HKCU\Software\PixelCSS\Display`. A missing or corrupt value falls back to defaults (Verdana-style, wrap on) and records a diagnostic. A failure to save is non-fatal. The production `NullPreferencesStore` (defaults, no-op save) backs `--no-preferences`, and tests use the separate `MemoryPreferencesStore` double. If the stored pack is no longer valid at startup, the default Verdana-style is used (FR-025 governs a *new install* default).
- **Rationale**: It is part of the constitution's storage module responsibility, and it is cheap and isolated behind an interface so tests use an in-memory fake.
- **Alternatives considered**: An INI file beside the executable was rejected because the install folder may be read-only.

## R-21 Shared headless text-line rendering

- **Decision**: One function, `pxc::fonts::renderTextLine(const FontRegistry&, RasterCache&, std::string_view packId, std::u32string_view text, int scale, Color fg, Color bg) -> Framebuffer`, defines the canonical canvas used by `pixelcss-tool render`, the glyph goldens and tests:
  - width = `(Σ advance + 2) · s` plus extra columns for any negative left bearing of the first glyph;
  - height = `lineHeight · s`;
  - background filled with `bg`;
  - pen starting at `x = s`;
  - `baselineY = ascent · s`.
- **Rationale**: A single definition guarantees that CLI output and goldens are byte-comparable (quickstart §3).

## Resolved items summary

| Open item (source) | Resolved by |
|---|---|
| MSVC / SDK pinning (spec Assumptions) | R-01 |
| Glyph schema, manifest schema (spec Assumptions; checklist Note 3) | R-05, R-06 |
| Fallback chain order (checklist Note 3) | R-07 |
| Reference rendering scale (checklist Note 3) | R-08 |
| Paint backend (spec Assumptions) | R-09 |
| Text buffer choice (spec Assumptions; Constitution VIII) | R-10 |
| Cache key (spec Assumptions) | R-14 |
| Presentation CSS property list (checklist Note 3) | R-15 |
| SC-009 / SC-010 ratification | R-17 |
| Resource limits (spec Assumptions) | R-14 (16 MiB cache), R-12 (layout full tier 4 096 lines / 32 MiB), R-17 (64 MB / 256 MB idle), R-13 (16 dirty rects) |
| Display-preference persistence (Constitution IX) | R-20 |
| Canonical headless render canvas | R-21 |
