<!--
SYNC IMPACT REPORT
==================
Version change: (unversioned template) → 1.0.0
Bump rationale: Initial ratification. All template placeholders replaced with concrete
project governance for PixelCSS Browser.

Modified principles (template placeholder → new title):
  - [PRINCIPLE_1_NAME] → I. Independent Character Rendering
  - [PRINCIPLE_2_NAME] → II. CSS as the Source of Character Geometry
  - [PRINCIPLE_3_NAME] → III. Multiple Independent Font Families
  - [PRINCIPLE_4_NAME] → IV. Mandatory Font Selection Interface
  - [PRINCIPLE_5_NAME] → V. Minimal HTML Document Model

Added principles:
  - VI. Progressive and Incremental Rendering
  - VII. Pixel Rendering Without Excessive DOM Elements
  - VIII. Notepad-First User Experience
  - IX. Modular Architecture
  - X. Language and Platform Standards
  - XI. Deterministic Rendering and Quality Assurance
  - XII. Font Asset Integrity and Licensing
  - XIII. Simplicity and Controlled Scope

Added sections:
  - Project Mission (new, precedes Core Principles)
  - [SECTION_2_NAME] → Technical and Product Constraints
  - [SECTION_3_NAME] → Development Workflow and Quality Gates
  - Governance (filled from [GOVERNANCE_RULES])

Removed sections: none

Editorial normalizations (no semantic change):
  - "MUST not" → "MUST NOT" (Principle VIII, content-loss rule).

Templates requiring updates:
  - .specify/templates/plan-template.md      ✅ no edit needed (Constitution Check gate
    reads this file at runtime)
  - .specify/templates/spec-template.md      ✅ no edit needed
  - .specify/templates/tasks-template.md     ✅ no edit needed
  - .specify/templates/checklist-template.md ✅ no edit needed

Follow-up TODOs: none. Ratification date supplied by author (2026-11-20).
-->

# PixelCSS Browser Constitution

## Project Mission

Develop a lightweight, standalone, native desktop browser-like rendering engine capable of
interpreting a deliberately restricted subset of HTML and CSS.

- The browser's primary purpose is to render editable textual documents using a custom
  pixel-based font rendering system.
- Unlike conventional browsers, the software MUST NOT depend on an operating system font
  rasterizer or conventional web browser engine for rendering document characters.
- Characters MUST be constructed from hexadecimal color definitions arranged in
  two-dimensional pixel matrices and stored in CSS files.
- Every character in every supported custom font family MUST have its own CSS file.
- The browser MUST provide an integrated notepad-like editing experience with progressive
  document rendering, efficient incremental layout, and partial-screen repainting.
- The initial platform MUST be Microsoft Windows, using modern native C++.
- The project MUST prioritize architectural simplicity, modularity, deterministic rendering,
  low memory consumption, extensibility, and measurable rendering performance.

## Core Principles

### I. Independent Character Rendering

The rendering engine MUST implement its own character rasterization mechanism.

1. Each character MUST be represented as a two-dimensional arrangement of pixel color values.
2. Pixel colors MUST support hexadecimal RGB/RGBA representations and transparency.
3. A CSS file MUST contain the declarative definition of exactly one character within one
   font family and style variant.
4. The engine MUST interpret these CSS definitions and convert them into rasterized
   character images.
5. Character rendering MUST NOT require TrueType, OpenType, DirectWrite, GDI text drawing,
   or any external font rasterizer.
6. Font files MUST NOT be required to render the custom character packs.
7. The engine MUST support glyph dimensions, baseline, advance width, spacing, and
   transparent pixels.
8. Glyph definitions MUST be independent of the document and usable across multiple
   documents.

**Rationale:** The central purpose of this project is to demonstrate an independently
programmable character rendering technology.

### II. CSS as the Source of Character Geometry

CSS MUST be the authoritative source for the visual definition of each character. The
project MUST implement a restricted, documented CSS-compatible glyph schema.

The schema MUST support:

- Character identifier expressed as a Unicode code point.
- Glyph width and height.
- Two-dimensional color matrix.
- Transparent and opaque pixels.
- Character advance width.
- Baseline and vertical alignment.
- Optional per-character metadata.
- Font-family association.
- Optional foreground and background color substitution.

Additional rules:

- Individual CSS files MUST be independently loadable and testable.
- Example layout: a font family may contain `fonts/verdana-style/glyphs/U+0041.css` for the
  character `A`.
- The renderer MUST recognize the custom glyph properties through an explicit parser. It
  MUST NOT assume that ordinary browser CSS automatically interprets pixel matrices.
- Malformed CSS glyph definitions MUST produce meaningful validation errors without
  crashing the application.

**Rationale:** CSS becomes a programmable, human-readable character-definition format rather
than merely a styling layer.

### III. Multiple Independent Font Families

The browser MUST support multiple custom CSS-based font families.

The initial target families MUST be:

1. **Verdana-style** — broad, readable, screen-oriented sans-serif characters.
2. **Arial Narrow-style** — condensed, space-efficient sans-serif characters.
3. **Comic Sans-style** — informal, rounded, playful handwritten-style characters.

Additional rules:

- Each family MUST have an independent directory containing its own character CSS files,
  font-level metadata, metrics, and manifest.
- The renderer MUST NOT merely scale the same glyph matrix horizontally or vertically to
  simulate all three families.
- Each family MUST have distinct original character geometry reflecting its intended visual
  characteristics.
- Character advances and layout metrics MUST be family-specific.
- The architecture MUST permit additional font families without modifications to the core
  renderer.
- Font loading MUST be driven by a registry or manifest.
- Exact reproduction of proprietary font outlines MUST require appropriate licensing.
  Otherwise the implementation MUST use independently designed glyphs and identify them as
  stylistic interpretations rather than authentic proprietary fonts.

**Rationale:** Font families must remain modular, extensible, and visually distinguishable.

### IV. Mandatory Font Selection Interface

The browser MUST display a font-family dropdown in the notepad toolbar.

Its initial choices MUST correspond to:

- Verdana
- Arial Narrow
- Comic Sans

These are user-facing stylistic selections; the application MUST clearly identify custom
interpretations where authentic licensed font assets are not used. Verdana-style MUST be the
initial default.

When the user changes the selected font:

1. The active document MUST use the corresponding glyph family.
2. The entire visible document MUST be relaid out using the new font metrics.
3. The viewport MUST be repainted using the selected glyph definitions.
4. The underlying document text MUST remain unchanged.
5. The caret and selection MUST remain anchored to their logical text positions.
6. Scrolling and line wrapping MUST remain consistent with the new layout.
7. The selected font MUST be visibly reflected in the toolbar.

Additional rules:

- Font changes MUST NOT require restarting the browser or reopening the document.
- The UI MUST provide predictable fallback behavior when a font pack or individual glyph is
  unavailable.

**Rationale:** Font selection is a fundamental interactive capability of the browser, not an
external font rendering operation.

### V. Minimal HTML Document Model

The browser MUST implement a deliberately restricted HTML-like document model.

The initial supported constructs SHOULD include:

- `html`
- `head`
- `body`
- `div`
- `span`
- Text nodes
- Paragraph and line-break elements
- A restricted set of document and layout properties

Additional rules:

- The renderer MUST distinguish document structure from presentation.
- The application MUST implement its own lightweight representation of document nodes.
- It MUST NOT embed Chromium, Edge WebView2, Firefox, or another full browser engine as its
  document renderer.
- Unsupported HTML elements and CSS properties MUST have clearly documented behavior.
- JavaScript execution, remote navigation, complex page layouts, and general web
  compatibility are explicitly outside the initial scope.
- The implementation MUST remain extensible enough to introduce additional HTML/CSS
  capabilities in future versions.

**Rationale:** The objective is to build a minimal browser architecture, not reproduce an
entire web standards platform.

### VI. Progressive and Incremental Rendering

The browser MUST use a staged rendering pipeline:

**Input → Document Update → Layout → Glyph Resolution → Rasterization → Composition →
Display**

- The engine MUST distinguish between initial progressive rendering and incremental
  rendering following an edit.
- Initial progressive rendering MUST prioritize currently visible content.
- Incremental rendering MUST track changes and repaint only affected portions of the
  document whenever possible.

The system MUST implement:

- Dirty-region tracking.
- Glyph rasterization caching.
- Layout invalidation.
- Viewport-based rendering.
- Line-level repainting.
- Selective framebuffer updates.
- Incremental text insertion and deletion.
- Appropriate reflow when editing affects subsequent characters or lines.

Prohibitions and guarantees:

- The browser MUST NOT rasterize every character in the document after each individual
  keystroke.
- It MUST NOT rebuild the entire document tree unnecessarily.
- Rendering MUST be visually deterministic for identical input, font packs, display
  settings, and renderer versions.

**Rationale:** Progressive and incremental rendering are essential to achieving a responsive
editing experience.

### VII. Pixel Rendering Without Excessive DOM Elements

- The renderer MUST use an internal pixel framebuffer or equivalent raster surface.
- The browser MUST NOT create one HTML element per colored glyph pixel.
- Each character MUST be converted into a reusable cached bitmap or raster representation.
- Glyph caches SHOULD be keyed using the relevant font family, code point, size, style,
  color configuration, and glyph revision.
- The initial rendering backend SHOULD use CPU-based software rendering.
- Hardware acceleration MAY be introduced behind a separate rendering abstraction after
  baseline correctness is established.
- The renderer MUST support clipping, scaling, line positioning, and efficient transfer of
  updated pixel regions to the application window.

**Rationale:** A compact raster-based renderer avoids the complexity and memory overhead of
pixel-level DOM trees.

### VIII. Notepad-First User Experience

The browser MUST provide an integrated text editor.

The initial editor MUST support:

- New document.
- Open plain-text file.
- Save and Save As.
- Keyboard-based character entry.
- Backspace and Delete.
- Cursor navigation.
- Multiline text.
- Enter and newline handling.
- Text selection.
- Copy, Cut, and Paste.
- Font selection.
- Vertical and horizontal scrolling.
- Configurable text wrapping.
- Basic undo and redo.

Additional rules:

- The document's textual data MUST remain independent of the visual glyph representation.
- The editor MUST use a suitable mutable text-buffer structure, such as a piece table or
  rope, with the choice justified through measured requirements.
- Clipboard operations MUST exchange ordinary text rather than pixel data.
- The initial release MUST support printable ASCII and essential editing control characters.
- The internal text model MUST be Unicode-capable, with additional script support added only
  after layout, glyph coverage, and input requirements are explicitly specified.
- The browser MUST NOT lose or corrupt document content when fonts change, the window
  resizes, or rendering errors occur.

**Rationale:** A working notepad provides a concrete environment for validating the
character-rendering engine.

### IX. Modular Architecture

The project MUST maintain distinct modules for:

1. **Application Shell:** Native window, menus, toolbar, file dialogs, and input routing.
2. **HTML Parser:** Restricted HTML parsing and validation.
3. **Document Model:** Logical text and document elements.
4. **CSS Glyph Parser:** Custom CSS character-file interpretation.
5. **Font Manager:** Font-family registration, glyph lookup, fallback, and caching.
6. **Layout Engine:** Character advances, line layout, wrapping, alignment, and viewport
   positions.
7. **Rendering Engine:** Pixel composition and framebuffer output.
8. **Incremental Update Manager:** Dirty-region and layout invalidation management.
9. **Editor Controller:** Keyboard operations, selections, undo/redo, and document commands.
10. **File Storage:** Plain-text persistence and associated display preferences.

Boundary rules:

- The modules MUST communicate through explicit, testable interfaces.
- The renderer MUST NOT depend directly on notepad toolbar controls.
- The CSS parser MUST NOT own the document text buffer.
- The font manager MUST NOT implement keyboard editing behavior.
- Circular dependencies between architectural modules MUST be avoided.

**Rationale:** Strict separation of concerns allows individual subsystems to evolve without
destabilizing the complete browser.

### X. Language and Platform Standards

- The initial implementation MUST use C++20 or a later stable C++ standard supported by the
  selected compiler toolchain.
- The Windows application SHOULD use Win32 APIs for window management and native input.
- Direct2D MAY be used for surface composition, but MUST NOT replace the custom glyph
  rasterization engine.
- The project MUST prefer the C++ standard library and small, well-justified dependencies.
- Build configuration MUST be reproducible, preferably using CMake.
- The codebase MUST use clear ownership semantics, RAII, bounded memory access, and explicit
  error handling.
- Platform-specific APIs MUST be isolated from core parsing, glyph, layout, and
  rasterization logic.
- The initial application MUST run locally and SHOULD require no network access.

**Rationale:** Native C++ provides direct control over memory, rendering, and window behavior
while allowing a small application footprint.

### XI. Deterministic Rendering and Quality Assurance

Every major rendering component MUST have automated tests.

Tests MUST cover:

- Valid and invalid glyph CSS parsing.
- Correct pixel-color interpretation.
- Transparency and clipping.
- Character metrics and placement.
- Glyph-cache correctness.
- Font-family switching.
- Line wrapping and reflow.
- Text editing.
- Caret and selection preservation.
- Incremental repainting.
- File opening and saving.
- Unicode-safe document storage.
- Resource cleanup.

Visual validation:

- Automated visual regression tests MUST compare rasterized glyphs against approved
  reference outputs.
- Each of the three font families MUST include a representative character-validation suite.

Performance tests MUST measure:

- Startup time.
- Initial document rendering time.
- Keystroke-to-visible-paint latency.
- Glyph-cache hit rate.
- Memory consumption.
- Repaint area.
- Large-document scrolling responsiveness.

Performance policy:

- Performance budgets MUST be established with reproducible benchmarks before optimization
  decisions are finalized.
- The project MUST prioritize correctness before micro-optimization.

**Rationale:** An independent rendering engine must be validated at both the logical and
pixel levels.

### XII. Font Asset Integrity and Licensing

- Each font pack MUST declare its origin, author, usage rights, and any applicable license.
- Unlicensed proprietary font outlines or rasterized glyph conversions MUST NOT be committed
  to the project.
- Font packs based on original designs MUST be clearly identified as independent stylistic
  interpretations.
- A font family MAY use authentic proprietary glyphs only when the project has explicit
  rights for the intended conversion, embedding, and distribution.
- The CSS glyph format MUST remain independent of any one font vendor.

**Rationale:** The project must remain safe to distribute and must not confuse stylistic
similarity with ownership or licensing rights.

### XIII. Simplicity and Controlled Scope

The first production milestone MUST NOT attempt to implement a complete standards-compliant
browser.

The following features are deferred until independently justified:

- JavaScript execution.
- HTTP and HTTPS navigation.
- General-purpose web page compatibility.
- Complex CSS layout systems.
- CSS animations and transitions.
- Browser extensions.
- Multimedia playback.
- GPU-based glyph generation.
- Advanced multilingual shaping.

Additional rules:

- No new subsystem MAY be added solely for speculative future usage.
- Every feature MUST demonstrate value to the custom rendering engine or the notepad
  experience.

**Rationale:** A narrow, fully working implementation is preferable to an incomplete
general-purpose browser.

## Technical and Product Constraints

The project MUST preserve these architectural invariants:

- A glyph CSS file is the authoritative visual definition of one character in one font
  family.
- The document buffer stores characters, not rendered pixels.
- Font-family changes affect presentation, not stored text.
- Pixel composition occurs in the custom rendering subsystem.
- Parsing and rendering remain independently testable.
- Only the visible portion of a document must be rasterized for screen display.
- Editing must trigger targeted rendering invalidation.
- Unsupported glyphs must fail safely using an explicit fallback representation.
- User-facing controls must remain separate from document-character rendering.
- Every supported character must have a documented geometry and layout contract.

Glyph coverage:

- The complete three-font target MUST include all 95 printable ASCII characters in each
  family.
- Initial development MAY use a smaller representative glyph subset, provided the intended
  coverage and staged expansion are explicitly tracked.

## Development Workflow and Quality Gates

Development MUST follow GitHub Spec Kit's specification-driven workflow. Every feature MUST
be introduced through a specification describing its observable behavior, acceptance
criteria, constraints, and dependencies.

The recommended development order is:

1. Define the CSS glyph schema and create an independent character parser.
2. Implement the software rasterization engine.
3. Implement the reusable glyph cache.
4. Create the first original CSS font family.
5. Build the minimal HTML-like document model.
6. Implement document and character layout.
7. Implement progressive and incremental rendering.
8. Integrate the native notepad interface.
9. Implement the font dropdown and remaining font families.
10. Expand character coverage, editing capabilities, performance testing, and packaging.

Quality gates:

- Each feature MUST include automated tests, a demonstrable output, and a documented
  validation procedure.
- An implementation phase MUST NOT be accepted solely because the application compiles.
- Pixel-level output MUST be demonstrated for rendering features.
- End-to-end editing behavior MUST be demonstrated for editor features.
- The three font families MUST be visually distinguishable, and switching among them MUST
  preserve document contents.
- Architecture decisions MUST be recorded where alternatives or tradeoffs materially affect
  the project.

## Governance

- This constitution is authoritative for all generated specifications, plans, tasks,
  architectural decisions, and implementation changes.
- Every Spec Kit plan MUST include a Constitution Check.
- Any implementation that violates a mandatory principle MUST either be corrected or require
  a formally approved amendment before implementation proceeds.
- Every major feature review MUST verify continued compliance with this constitution.

**Amendment procedure.** Amendments MUST contain:

- The principle or constraint being changed.
- The rationale for the change.
- The anticipated architectural and behavioral consequences.
- Any required updates to specifications, plans, and tests.
- The updated constitution version.

**Versioning policy.** Constitution versions MUST follow semantic versioning:

- **MAJOR:** Removal or incompatible redefinition of a core principle.
- **MINOR:** Addition of principles or materially expanded governance.
- **PATCH:** Clarifications without substantive changes.

**Success measure.** The primary measure of project success is a working, minimal, native
browser-like notepad in which text is rendered entirely through independently defined CSS
pixel glyphs, with three selectable font families and efficient progressive rendering.

**Version**: 1.0.0 | **Ratified**: 2026-11-20 | **Last Amended**: 2026-11-20
