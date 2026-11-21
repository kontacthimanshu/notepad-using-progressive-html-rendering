# Feature Specification: PixelCSS Browser — Custom CSS Glyph Rendering and Notepad

**Feature Branch**: `001-pixelcss-browser`

**Created**: 2026-11-21

**Status**: Draft

**Input**: User description: "PixelCSS Browser — Custom CSS Glyph Rendering and Notepad. A lightweight native Windows browser-like application whose primary workspace is a Notepad-style text editor; every document character is rendered by a custom software engine from pixel-color matrices defined in individual `.css` glyph files; three original font families (Verdana-style, Arial Narrow-style, Comic Sans-style); progressive painting, incremental layout, dirty-region repaints, glyph caching; restricted local HTML preview; reproducible MSVC builds."

**Governing Document**: PixelCSS Browser Constitution v1.0.0 (ratified 2026-11-20) — `.specify/memory/constitution.md`

**Target Platform**: Windows 11 x64 (Windows 10 compatibility where practical)

**Mandatory Toolchain** *(constitutional constraint)*: Microsoft Visual C++ (MSVC), Visual Studio 2022 or newer, C++20, Windows SDK

**Initial Delivery**: Native Windows x64 executable with three original CSS-pixel font families

## Overview

Build a lightweight native Windows browser-like application whose primary workspace is a Notepad-style text editor. The application interprets a restricted local HTML/CSS document model, but every document character is rendered by a custom software engine from pixel-color matrices defined in individual `.css` glyph files. The application does not use TrueType/OpenType rendering, GDI text drawing, DirectWrite, WebView2, Chromium, or another browser engine to draw document characters. Native Windows UI controls may use system fonts for application chrome only.

The initial font selector offers **Verdana**, **Arial Narrow**, and **Comic Sans**. Unless appropriately licensed assets are supplied, these labels map to independently designed Verdana-style, Arial Narrow-style, and Comic Sans-style pixel-font packs; the application must clearly disclose that they are custom interpretations, not authentic Microsoft fonts.

The program supports editing plain text, opening limited local HTML for rendering, progressive initial painting, incremental layout, dirty-region repaints, glyph caching, and reproducible MSVC builds. It must be a functioning application rather than a static demonstration.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Render a character entirely from its CSS glyph file (Priority: P1)

As a user, I want a character displayed from a color matrix stored in a CSS file so that document typography is controlled by the project's own rendering system.

**Why this priority**: The custom glyph pipeline is the core product capability; all other stories depend on it.

**Independent Test**: Load the original CSS definition of `U+0041` (A), render it on a blank native window, and compare the resulting pixels to the declared matrix without invoking an OS font renderer for the glyph.

**Acceptance Scenarios**:

1. **Given** a valid `U+0041.css` with dimensions, metrics, and RGB/RGBA cells, **When** the engine loads and renders it, **Then** each opaque pixel appears at its specified relative position and transparent cells do not overwrite the background.
2. **Given** a valid glyph file with a baseline and advance width, **When** two characters are placed consecutively, **Then** their horizontal positions and baseline follow those metrics.
3. **Given** a malformed glyph matrix, **When** it is parsed, **Then** the file is rejected with a location-aware diagnostic and the application continues running.
4. **Given** repeated instances of a character, **When** the same rendering settings are used, **Then** the glyph is served from the raster cache after its first rasterization.

---

### User Story 2 - Edit and save a plain-text document (Priority: P1)

As a user, I want to type, edit, select, copy, paste, and save ordinary text in a Notepad-like native application.

**Why this priority**: A practical editor is the required validation environment for the rendering engine.

**Independent Test**: Start the executable, type a multiline paragraph, move the caret, edit text, undo and redo changes, save the file, reopen it, and confirm that the text is identical.

**Acceptance Scenarios**:

1. **Given** an empty document, **When** I type printable characters or press Enter, **Then** the logical buffer and custom-rendered viewport update correctly.
2. **Given** text on multiple lines, **When** I use arrow keys, Home, End, Backspace, Delete, and selections, **Then** the correct text positions and screen regions are updated.
3. **Given** selected text, **When** I copy or cut and paste through the Windows clipboard, **Then** ordinary Unicode text—not glyph bitmap data—is exchanged.
4. **Given** an edited document, **When** I save and reopen it, **Then** its contents are preserved independently of the selected display font.
5. **Given** unsaved changes, **When** I close, replace, or open another document, **Then** the application requests a save/discard/cancel decision.

---

### User Story 3 - Switch among three CSS-defined font families (Priority: P1)

As a user, I want to choose Verdana, Arial Narrow, or Comic Sans from a toolbar dropdown and immediately see my document rendered in the selected custom pixel style.

**Why this priority**: The three-font selector is an explicit product requirement.

**Independent Test**: Type `Aa0!? Hello World`, cycle through all three choices, compare approved screenshots, and verify that the document buffer, selection, and caret offsets are unchanged.

**Acceptance Scenarios**:

1. **Given** a new document, **When** the editor opens, **Then** the toolbar visibly defaults to Verdana, mapped to the Verdana-style custom pack.
2. **Given** a document in Verdana, **When** I select Arial Narrow, **Then** the glyph geometry and layout metrics change and visible lines are relaid out without altering stored text.
3. **Given** the same document, **When** I select Comic Sans, **Then** it renders with a visibly distinct independently designed glyph family.
4. **Given** an active selection and caret, **When** I switch fonts, **Then** their logical text positions are preserved even if line wrapping changes.
5. **Given** a missing glyph or unavailable font pack, **When** rendering is attempted, **Then** a deterministic fallback appears and an understandable diagnostic is available without crashing.
6. **Given** a custom-style font, **When** the user inspects font information, **Then** the UI makes clear that this is not the original proprietary typeface.

---

### User Story 4 - Render progressively and repaint only affected content (Priority: P1)

As a user, I want the editor to remain responsive when I type, scroll, resize, or open a substantial document.

**Why this priority**: Incremental layout and rasterization are foundational constitutional requirements.

**Independent Test**: Load a long document, measure first-viewport paint and scroll performance, insert one character in a stable line, and use paint instrumentation to verify that unchanged off-screen glyphs are not rerasterized.

**Acceptance Scenarios**:

1. **Given** a large document, **When** it opens, **Then** currently visible lines are prioritized instead of first rasterizing the entire file.
2. **Given** an insertion at the end of a line that does not reflow adjacent lines, **When** I type one character, **Then** only the affected glyph/line and caret regions are repainted.
3. **Given** an insertion that changes wrapping, **When** layout changes, **Then** the affected line and downstream visible lines are correctly relaid out and repainted.
4. **Given** rapid scrolling, **When** new lines enter the viewport, **Then** cached glyphs are reused where valid.
5. **Given** a font or window-size change, **When** it changes layout, **Then** the engine invalidates the necessary visible layout and produces a correct complete visible result.

---

### User Story 5 - Render a deliberately restricted local HTML document (Priority: P2)

As a user, I want to open a small local HTML document and see its supported elements laid out with the same CSS-glyph text renderer.

**Why this priority**: This validates the browser-like document model without expanding into general web browsing.

**Independent Test**: Open a local HTML fixture containing `html`, `head`, `body`, `div`, `span`, `p`, `br`, and text nodes, and verify document structure, line breaks, nesting, and custom-glyph text output.

**Acceptance Scenarios**:

1. **Given** a valid supported local HTML file, **When** it opens, **Then** the document is rendered in a read-only preview using the project's own glyph engine.
2. **Given** nested supported elements, **When** the document is laid out, **Then** block and inline content appear in their expected order.
3. **Given** an unsupported element or CSS property, **When** it is encountered, **Then** documented fallback/ignore behavior applies, without executing scripts or loading network resources.
4. **Given** malformed HTML, **When** parsing fails or recovers, **Then** an understandable diagnostic appears and no application crash occurs.

---

### User Story 6 - Register, validate, and extend font packs (Priority: P2)

As a font designer, I want independent per-family packs and per-character CSS files so that I can add or update a glyph without modifying the rendering engine.

**Why this priority**: Font extensibility and per-character source files are architectural invariants.

**Independent Test**: Validate all 95 printable ASCII glyphs for each bundled font, change one test glyph's CSS file, reload its font pack, and verify only the relevant cache entries become invalid.

**Acceptance Scenarios**:

1. **Given** each of the three built-in packs, **When** validation runs, **Then** it finds precisely one primary CSS definition for each printable ASCII code point `U+0020` through `U+007E`.
2. **Given** three installed packs, **When** an identical text sample is rendered, **Then** each pack has independently authored geometry and its own metrics.
3. **Given** an additional conforming font manifest, **When** it is registered, **Then** the font manager can expose it without changing core engine code.
4. **Given** a missing or invalid glyph, **When** it is requested, **Then** the resolver follows the documented fallback chain and records the failure.
5. **Given** a font pack, **When** its manifest is inspected, **Then** origin, author, version, license, and custom-style/true-typeface status are present.

---

### User Story 7 - Build, debug, test, and package with MSVC (Priority: P1)

As a developer, I want a solution that builds, runs, and can be debugged in Microsoft Visual Studio using the MSVC toolchain.

**Why this priority**: MSVC is a non-negotiable implementation requirement.

**Independent Test**: From a clean Windows machine with the documented Visual Studio C++ workload and Windows SDK, configure and build an x64 Debug and Release version, run automated tests, and launch the packaged executable.

**Acceptance Scenarios**:

1. **Given** the supported Visual Studio installation, **When** the solution is built as x64 Debug, **Then** `cl.exe` compiles the C++ source and the application runs under the Visual Studio debugger.
2. **Given** the same source, **When** built as x64 Release, **Then** the deliverable launches without requiring an embedded browser runtime.
3. **Given** a clean checkout, **When** tests execute, **Then** parser, glyph, rendering, editing, and layout tests produce repeatable results.
4. **Given** a packaged distribution, **When** copied to a supported Windows machine, **Then** it includes the executable, required runtime/dependencies, font manifests, and all CSS glyph files.

---

### Edge Cases

- `U+0020` (space) has an advance width but may have zero opaque pixels.
- A glyph may have transparent rows, unusual bearings, non-square dimensions, or multiple colors.
- A CSS row may contain too many/few cells, invalid hexadecimal text, invalid alpha, or unsupported properties.
- A font family or a single ASCII glyph may be missing, inaccessible, or corrupted at startup or while loading.
- A character may not exist in the selected font, including non-ASCII Unicode input.
- Font switching may change wrapping, scroll offsets, and viewport height without altering logical selection offsets.
- Inserting/deleting at a line boundary may propagate layout invalidation into subsequent visible lines.
- The document may contain tabs, CRLF/LF line endings, empty lines, very long lines, Unicode characters, or a trailing newline.
- Clipboard content may include characters that the selected font cannot render; the stored/pasted text must still survive.
- Windows display scaling may be 100%, 125%, 150%, or another DPI value; nearest-neighbor/integer pixel treatment must be predictable.
- File open/save may fail because of access permissions, sharing conflicts, invalid encoding, or disk-full conditions.
- An HTML file may contain scripts, event-handler attributes, remote URLs, or unsupported external stylesheets; none may execute or trigger remote fetching.
- Extremely large documents must not require all glyphs or all lines to be converted into bitmaps up front.

## Requirements *(mandatory)*

### Functional Requirements — Glyph Schema and Font Management

- **FR-001**: The system MUST define and document a restricted CSS-compatible glyph schema with a version identifier.
- **FR-002**: Every supported code point in a font family/style MUST have one corresponding `.css` file; filenames MUST unambiguously identify Unicode code points, such as `U+0041.css`.
- **FR-003**: Each glyph file MUST declare its code point, family/style identity, pixel width/height, baseline, horizontal advance, and color-matrix rows.
- **FR-004**: Matrix cells MUST support literal `#RRGGBB`, `#RRGGBBAA`, and transparent values; rows MUST have exactly the declared number of cells.
- **FR-005**: The parser MUST reject invalid dimensions, invalid colors, inconsistent row lengths, duplicate glyph identities, and unsupported required schema versions with actionable diagnostics.
- **FR-006**: The parser MUST read glyph CSS directly; it MUST NOT require a conventional web CSS rendering engine.
- **FR-007**: The font manager MUST load manifests and register independent Verdana-style, Arial Narrow-style, and Comic Sans-style packs.
- **FR-008**: Each bundled pack MUST ship independently authored glyphs for all 95 printable ASCII characters (`U+0020`–`U+007E`), totaling at least 285 primary glyph CSS files across the three packs.
- **FR-009**: The three packs MUST have distinct geometry and family-specific metrics, not merely resized copies of a single design.
- **FR-010**: The application MUST provide a documented missing-glyph fallback, with a visible replacement glyph and diagnostic when the selected glyph cannot be loaded.
- **FR-011**: Glyph definitions MUST be cached as reusable raster representations; modified packs or relevant rendering settings MUST invalidate affected cache entries.
- **FR-012**: Each font manifest MUST declare name, style description, version, glyph coverage, origin, authorship, and license/distribution permissions.
- **FR-013**: The bundled font pack and application UI MUST distinguish independent style interpretations from authentic proprietary font assets.

### Functional Requirements — Rendering and Layout

- **FR-014**: The renderer MUST place colored cells into an internally owned software framebuffer or equivalent raster surface without OS font APIs for document text.
- **FR-015**: The renderer MUST support alpha transparency, clipping, baseline positioning, per-glyph advance width, line spacing, and predictable nearest-neighbor pixel scaling.
- **FR-016**: Document text, caret, selections, and wrapping MUST be positioned using logical text offsets and font metrics, not bitmap-encoded text storage.
- **FR-017**: The layout engine MUST calculate visible lines and glyph positions for the viewport and MUST handle line wrapping and explicit newlines.
- **FR-018**: First render MUST prioritize the visible viewport; off-screen content MUST NOT be rasterized merely to display the first page.
- **FR-019**: A text edit MUST invalidate only its affected layout/paint regions whenever the edit does not require wider reflow.
- **FR-020**: Changes that alter wrapping, font metrics, or window size MUST propagate invalidation far enough to prevent stale pixels or incorrect layout.
- **FR-021**: The renderer MUST reuse cached glyph rasterizations and MUST NOT rebuild the entire document representation after every keystroke.
- **FR-022**: Identical font packs, settings, text, and renderer versions MUST produce pixel-identical results on the same defined rendering configuration.
- **FR-023**: Diagnostic instrumentation MUST expose glyph-cache hits/misses, rasterizations, layout work, dirty-region area, and paint timing for automated tests.

### Functional Requirements — Native Editor and Font Picker

- **FR-024**: The program MUST provide a native Windows window with a notepad workspace, menu or equivalent file controls, and a visible font-family dropdown.
- **FR-025**: The dropdown MUST offer Verdana, Arial Narrow, and Comic Sans, mapped to the corresponding original style packs; the initial selection MUST be Verdana.
- **FR-026**: Changing the dropdown selection MUST reflow and repaint the visible document without changing underlying text, caret offsets, or selection offsets.
- **FR-027**: The application MUST support New, Open, Save, Save As, and an unsaved-changes warning.
- **FR-028**: The editor MUST support typing, Enter, Backspace, Delete, arrows, Home/End, selection, copy/cut/paste, undo/redo, and horizontal/vertical scrolling.
- **FR-029**: The editor MUST support configurable word wrap, including correct relayout when toggled.
- **FR-030**: Clipboard text MUST interoperate with native Windows text applications using standard Unicode text formats.
- **FR-031**: The document model MUST be capable of preserving Unicode text even when the selected font can only display a fallback glyph for some code points.
- **FR-032**: Plain-text files MUST round-trip as UTF-8; the program MUST explicitly handle or clearly report unsupported/invalid encodings and preserve meaningful line breaks.
- **FR-033**: Document rendering MUST take place on the custom editor surface, not in an operating-system text-edit control that renders its own glyphs.
- **FR-034**: The application MUST report file, parsing, and asset-loading failures in user-comprehensible form without losing unsaved document content.

### Functional Requirements — Restricted HTML/CSS

- **FR-035**: The application MUST parse a documented local HTML subset including `html`, `head`, `body`, `div`, `span`, `p`, `br`, and text nodes.
- **FR-036**: HTML parsing MUST produce a lightweight document tree logically separate from glyph bitmaps and paint surfaces.
- **FR-037**: Supported HTML content MUST be presented using the same CSS-glyph rendering and layout pipeline as the plain-text editor.
- **FR-038**: Local HTML MUST open as a read-only rendered preview. *(Amended 2026-11-21: View Source.)* A **View → Page Source** toggle (Ctrl+U) MUST switch an HTML file between the preview and its HTML source shown as plain text in the editor with the same CSS glyphs; the source MAY be edited and saved like any text file, and switching back MUST render the current, possibly unsaved, source. The preview itself stays read-only; no HTML is serialized from the rendered tree.
- **FR-039**: A small supported CSS presentation subset MUST be documented (at minimum text color, background color, font-family selection, and basic line/block spacing), with predictable behavior for unsupported declarations.
- **FR-040**: The system MUST NOT execute JavaScript, follow hyperlinks as network navigation, download remote assets, or run HTML event handlers.
- **FR-041**: Glyph CSS interpretation MUST be kept distinct from ordinary supported document presentation CSS.

### Functional Requirements — MSVC Build, Distribution, and QA

- **FR-042**: All native C++ source MUST be compiled with the Microsoft Visual C++ (MSVC) compiler, `cl.exe`, not GCC, MinGW, or Clang, for the primary supported builds.
- **FR-043**: The project MUST target C++20 or later and provide a Visual Studio 2022-or-newer solution/workspace with Windows SDK integration and x64 Debug/Release configurations.
- **FR-044**: The build MUST be reproducible from a clean checkout; CMake with a Visual Studio generator MAY generate the solution, but the actual compiler MUST remain MSVC.
- **FR-045**: The native Windows shell SHOULD use Win32 APIs; the rasterizer MUST remain independent of the native windowing controls.
- **FR-046**: The build MUST enable high diagnostic quality (at least MSVC `/W4` or equivalent); warnings and compiler compatibility decisions MUST be documented.
- **FR-047**: The project MUST supply MSVC-compatible unit and integration tests and image-based rendering regression tests.
- **FR-048**: A Release package MUST include executable(s), required redistributables or an accurately documented runtime prerequisite, the three font manifests, and all required CSS files.
- **FR-049**: The Release application MUST NOT require WebView2, Chromium, a browser installation, or internet connectivity for the supported functionality.
- **FR-050**: Documentation MUST explain how to configure, build, debug, test, run, package, and add a font pack using MSVC/Visual Studio.
- **FR-051**: Tests MUST verify correct font changes, caret/selection preservation, file round-tripping, invalid-asset handling, cache behavior, and dirty-region painting.
- **FR-052**: Visual regression fixtures MUST cover each initial font family, transparency, baseline alignment, clipping, and wrapping.

### Key Entities *(include if feature involves data)*

- **GlyphDefinition**: The immutable parsed CSS source for one Unicode code point in one family/style, including dimensions, color cells, baseline, and advance.
- **FontManifest**: Family identity, version, authorship/license status, metrics defaults, glyph coverage, and glyph-file locations.
- **FontRegistry**: Available families, active family, pack validation state, and missing-font fallback policy.
- **RasterGlyph**: Cached pixel representation of a glyph at a specific render configuration, distinct from its CSS definition.
- **TextDocument**: Logical Unicode characters, line boundaries, modification state, and undo/redo information; never stores glyph pixels as text.
- **DocumentNode**: Parsed restricted HTML structure or an equivalent text-editor document node.
- **LayoutLine**: A logical-to-visual mapping of glyph positions, line boundaries, advances, and wrapping state.
- **Viewport**: Scroll position, client dimensions, visible line range, and scale.
- **DirtyRegion**: The portion of layout and/or framebuffer requiring recomputation after a change.
- **EditorState**: Caret, selections, active file, wrapping preference, and active font-family choice.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A clean MSVC x64 Debug and Release build completes in Visual Studio 2022 or newer, and the executable runs on a supported Windows machine.
- **SC-002**: All three original CSS font packs pass validation for 95 printable ASCII code points each (at least 285 independent glyph CSS files).
- **SC-003**: Every bundled glyph used in the reference suite passes pixel-exact output comparison at the reference rendering scale.
- **SC-004**: Switching among the three toolbar fonts changes visible glyph geometry and layout while preserving 100% of the logical document characters, caret offsets, and selection offsets.
- **SC-005**: New/Open/Save/Save As, selection, clipboard operations, and undo/redo pass their automated acceptance tests with no document loss.
- **SC-006**: For a stable, non-reflowing keystroke, instrumentation confirms the engine does not rerasterize the whole document or repaint off-screen lines.
- **SC-007**: A local HTML fixture using the supported subset renders correctly without script execution or network requests.
- **SC-008**: Malformed glyph files, missing packs, and file-access failures are handled without an unhandled application crash.
- **SC-009**: On the documented Windows 11 x64 reference machine, the p95 keystroke-to-present latency MUST be at most 50 ms for a 100,000-character plain-text document with normal viewport dimensions, measured end-to-end from input receipt to completed screen presentation (budget ratified in planning, research R-17).
- **SC-010**: On that reference machine, the editor MUST display the first viewport of a 100,000-character local document within 1 second after file decoding (budget ratified in planning, research R-17).
- **SC-011**: The Release package works offline and contains no dependency on a general-purpose browser engine or OS font rasterizer for document text.
- **SC-012**: Developer documentation demonstrates a clean MSVC build, a CSS-glyph edit, a rendering validation, and adding a conforming font pack.

## Assumptions

- The first supported production architecture is Windows x64; ARM64 and non-Windows support are deferred.
- Visual Studio 2022 or newer, the Desktop development with C++ workload, MSVC v143 or a successor toolset, and a compatible Windows SDK are available to developers.
- The program will use C++20. Exact MSVC minor version and Windows SDK version will be pinned during `/speckit-plan` to make reproducible builds possible.
- Native Win32 toolbar controls may use system UI font rendering; only document characters must be custom-rasterized.
- Plain text is editable; local HTML is initially a read-only rendering format. The document model is extensible to future richer editing.
- The initial target is a deliberately restricted, documented CSS glyph grammar, not complete CSS syntax or `@font-face` compatibility.
- The bundled three font packs are original pixel-font artwork inspired by the named styles, unless separate rights to distribute authentic glyphs are documented. Their pack identifiers are `verdana-style` (Verdana-style), `arial-narrow-style` (Arial Narrow-style), and `comic-style` (Comic Sans-style).
- The final glyph schema, manifest schema, cache key, text-buffer selection, paint backend, and resource limits will be detailed in `/speckit-plan` without weakening these user-observable requirements.
- Technology and toolchain terms (MSVC, C++20, Win32, Visual Studio) appear in this specification because the constitution makes them binding product constraints, not because the spec prescribes internal design.

## Out of Scope

- Full web browsing or internet navigation
- JavaScript execution
- Chromium/WebView2 integration
- CSS Flexbox/Grid and arbitrary web CSS
- Browser extensions and multimedia
- General-purpose HTML editing
- TrueType/OpenType font rasterization of document characters
- GPU glyph rendering as a first-release requirement
- Full international text shaping
- Cross-platform delivery

## Constitution Check and Release Gates

This feature is governed by PixelCSS Browser Constitution v1.0.0. `/speckit-plan` MUST explicitly verify:

- [ ] Custom CSS-defined document glyphs never fall back to OS text rendering.
- [ ] Exactly one CSS file per glyph/code-point/family/style, with complete ASCII coverage for the three bundled packs.
- [ ] Font switching is a presentation-only action and preserves all logical text positions.
- [ ] Progressive first-paint, dirty regions, viewport-based layout, and glyph caching are demonstrable.
- [ ] Application chrome and rendering internals are separated; HTML and CSS parsers remain independently testable.
- [ ] The Windows toolchain uses MSVC `cl.exe` with C++20 and delivers Visual Studio-compatible Debug/Release builds.
- [ ] Font assets have documented origin and permissible distribution rights.
- [ ] Tests include visual comparison, document correctness, MSVC build reproducibility, and measurable performance.
- [ ] The application ships as a native notepad-first browser-like renderer, not a replacement general-purpose web browser.

## Delivery Milestones

*(Input for later `/speckit-plan` and `/speckit-tasks`)*

| Milestone | Name | Scope |
|-----------|------|-------|
| **M1** | Glyph Contract and Renderer | CSS schema; parser; framebuffer; render `A`, digits, punctuation; pixel regression tests |
| **M2** | Core Editor | Text buffer; native Win32 window; editing, caret, selection, scrolling, save/open |
| **M3** | Three Fonts | 95 printable ASCII glyphs per family; manifests; dropdown; layout reflow and fallback |
| **M4** | Incremental Engine | Cache, dirty regions, incremental layout, viewport prioritization, instrumentation |
| **M5** | Minimal HTML Preview | Restricted parser, local document preview, presentation CSS subset, safe unsupported-content behavior |
| **M6** | MSVC Release Readiness | Automated tests, performance baselines, packaging, font licensing review, developer/user documentation |

**Release Definition of Done**: All P1 stories, all applicable FR requirements, required three-font ASCII coverage, constitutional gates, automated tests, and a reproducible MSVC x64 Release distribution are satisfied. P2 HTML preview and pack extensibility are included in the complete v1 scope as specified above.
