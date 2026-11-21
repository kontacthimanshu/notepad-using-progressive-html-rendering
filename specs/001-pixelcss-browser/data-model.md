# Phase 1 Data Model: PixelCSS Browser

**Feature**: [spec.md](./spec.md) | **Research**: [research.md](./research.md) | **Date**: 2026-11-21

Conventions:
- **Offsets** are code-point indices into the `TextDocument` (`std::size_t`).
- **Pixel coordinates** are integer device pixels.
- **Glyph-space coordinates** are integer cells at scale 1.

The owning module (Constitution IX) is given in brackets for each entity.

---

## 1. GlyphDefinition `[CSS Glyph Parser]`

The immutable parse result of one glyph CSS file. Schema: [contracts/glyph-css-schema.md](./contracts/glyph-css-schema.md).

| Field | Type | Rules |
|---|---|---|
| `schema` | string | must equal `pixelcss-glyph/1` (FR-005) |
| `codePoint` | `char32_t` | 0x0000–0x10FFFF, excluding surrogates; must match the filename `U+XXXX.css` (FR-002) |
| `family` | string | must equal the owning manifest's `--id` |
| `style` | string | `regular` in v1 |
| `width`, `height` | `uint16` | 0–64; `width = 0` is allowed only if `height = 0` or all cells are transparent (the space glyph) |
| `baseline` | `int16` | 0–64: number of glyph rows above the baseline, counted from the glyph's top row. It may exceed `height` for glyphs that float above the baseline (`-`, `^`, quotes); descender rows = max(0, height − baseline) |
| `advance` | `int16` | 0–128; the pen moves this many cells horizontally |
| `bearingX` | `int16` | −16 to 16; default 0 |
| `cells` | `vector<Cell>` | exactly `width × height` entries, row-major; every row has exactly `width` tokens (FR-004) |
| `metadata` | `vector<pair<string,string>>` | from `--meta-<name>` properties, kept in source order; ignored by the renderer |
| `contentHash` | `uint64` | FNV-1a 64 of the normalised file bytes, used for glyph revisions |
| `sourcePath` | path | used in diagnostics |

`Cell` = `{ kind: Transparent | Literal | CurrentColor | BackgroundColor, rgba: uint32 }`. A literal `#RRGGBB` has alpha 255. A literal `#RRGGBBAA` with alpha 00 is treated as `Transparent`. `CurrentColor` is foreground substitution and `BackgroundColor` is background substitution (Constitution II).

Authoring follows the tight-ink-box convention in the glyph contract. `baseline ≤ manifest.ascent` and `height − baseline ≤ manifest.descent` are checked by the pack loader and reported as `W-GLYPH-EXCEEDS-METRICS`.

**Validation errors** (all carry `file:line:column`): `E-GLYPH-SCHEMA`, `E-GLYPH-MISSING-PROP`, `E-GLYPH-DUP-PROP`, `E-GLYPH-BAD-DIM`, `E-GLYPH-ROW-COUNT`, `E-GLYPH-ROW-LENGTH`, `E-GLYPH-BAD-COLOR`, `E-GLYPH-CP-MISMATCH`, `E-GLYPH-FAMILY-MISMATCH`, `E-GLYPH-SYNTAX`. Unknown non-required properties produce warning `W-GLYPH-UNKNOWN-PROP` and are ignored.

## 2. FontManifest `[Font Manager]`

Schema: [contracts/font-manifest-schema.md](./contracts/font-manifest-schema.md).

| Field | Type | Rules |
|---|---|---|
| `id` | string | `[a-z0-9-]+`, unique across the registry, and equal to the directory name |
| `displayName` | string | the dropdown label, e.g. `Verdana` |
| `styleDescription` | string | non-empty |
| `version` | semver string | required |
| `authenticity` | enum | `style-interpretation` or `licensed-original` (FR-013) |
| `origin`, `author`, `license` | string | required, non-empty (FR-012) |
| `licenseFile` | relative path | must exist inside the pack |
| `lineHeight` | `int16` | ≥ the tallest glyph extent |
| `ascent`, `descent` | `int16` | `ascent + descent ≤ lineHeight` |
| `coverage` | range list | e.g. `U+0020-007E, U+FFFD`; every listed code point must have exactly one file (FR-008) |
| `glyphDir` | relative path | default `glyphs` |
| `replacementGlyph` | code point | default `U+FFFD` |
| `aliases` | string list | names accepted by presentation-CSS `font-family` |

## 3. FontPack (runtime) `[Font Manager]`

`{ manifest, glyphs: map<char32_t, GlyphDefinition>, packRevision: uint32, state, diagnostics }`

**State transitions**: `Discovered → Loading → Valid | Degraded | Invalid`. A `Reload` from `Valid`, `Degraded` or `Invalid` returns to `Loading`.
- `Valid`: the manifest is valid and every glyph in its coverage parsed.
- `Degraded`: the manifest is valid but one or more glyphs failed. The pack is usable, and failed code points take the fallback chain.
- `Invalid`: the manifest is missing or invalid. The pack is not shown in the dropdown, or is shown disabled with a tooltip giving the reason.

`packRevision` increments on each successful reload. The `glyphRevision` for a code point is the `contentHash` of its definition.

## 4. FontRegistry `[Font Manager]`

| Field | Notes |
|---|---|
| `packs` | ordered by manifest `--order`, then `id` |
| `activePackId` | default `verdana-style` (FR-025) |
| `fallbackPackId` | `verdana-style` |
| `engineBoxFont` | built-in, always valid |
| `diagnostics` | `DiagnosticsLog` reference |

Operation `resolve(packId, cp) → ResolvedGlyph { def*, source: Primary | PackReplacement | EngineReplacement, packUsed }`. It follows research R-07 and is deterministic.

## 5. RasterGlyph and RasterCache `[Font Manager produces; Rendering Engine consumes]`

`RasterCacheKey = { packId, style, packRevision, codePoint, glyphRevision, scale, fg, bg }`
`RasterGlyph = { width·scale, height·scale, originX = bearingX·scale, originY = −baseline·scale, pixels: BGRA[] }`

`RasterGlyph` is a plain pixel type defined in `pxc_render`, which knows nothing about glyph CSS. The `GlyphDefinition → RasterGlyph` conversion (`rasterize`) lives in `pxc_fonts`. `CurrentColor` cells resolve to `fg` and `BackgroundColor` cells resolve to `bg`. Transparent cells stay at alpha 0, and the background is never pre-composited.

The cache is an LRU with a 16 MiB budget. Statistics: `hits`, `misses`, `evictions`. Entries whose key revision no longer matches the registry are unreachable and are evicted lazily.

## 6. TextDocument `[Document Model]`

| Field | Notes |
|---|---|
| `buffer` | a piece table of `char32_t`: original + append buffers, `vector<Piece>` |
| `lineIndex` | a Fenwick tree of line lengths, counting `\n` as the last char of its line |
| `revision` | `uint64`, incremented on each mutation |
| `lineRevisions` | `std::vector<uint64_t>`, one entry per logical line, spliced on line-count changes; values come from a global counter. Used for the layout cache key |
| `modified` | bool; cleared on save and set on any edit or undo/redo away from the saved point |
| `fileInfo` | `{ path?, encoding: Utf8 \| Utf8Bom \| Utf16LE \| Utf16BE, eol: CRLF \| LF \| Mixed, converted: bool }` |
| `undoStack`, `redoStack` | `vector<EditRecord>` |

`EditRecord = { offset, removedText: u32string, insertedText: u32string, caretBefore, caretAfter, selectionBefore, timestamp, coalesceGroup }`. Consecutive single-character inserts or backspaces within 1 s that are contiguous and do not cross whitespace→word coalesce into one group. Undo history is capped at 10 000 groups.

**Invariants**: the text contains no `\r` (normalised on load and paste). The document never stores pixels (FR-016). Font changes never mutate it (FR-026).

## 7. DocumentNode `[HTML Parser / Document Model]`

`DocumentNode = { kind: Element | Text, tag: enum{Html,Head,Body,Title,Div,P,Span,Br,Style,Unknown}, rawTag, id, classes, inlineStyle, children, text (Text only), computedStyle }`

`ComputedStyle = { color, backgroundColor, fontPackId, marginTop, marginBottom, lineHeight, display: Block|Inline|None }`

`PreviewDocument = { root, diagnostics, sourcePath }` is read-only (FR-038). The layout flattens the tree into a sequence of styled runs grouped into block boxes.

## 8. LayoutLine `[Layout Engine]`

| Field | Notes |
|---|---|
| `logicalLine` | index |
| `key` | `{ lineRevision, packId, packRevision, scale, wrapWidth }` |
| `visualRows` | `vector<VisualRow>` |
| `estimated` | true while the line is not yet laid out (height comes from the estimate in R-12) |

`VisualRow = { startOffset, endOffset, y (relative to line top), height, inkRight, glyphs: vector<PositionedGlyph> }`
`PositionedGlyph = { offset, codePoint, x, advance, resolvedSource }`

**Two-tier cache (research R-12)**:
- Full `LayoutLine`s live in a bounded LRU: at most 4 096 lines or 32 MiB.
- Every logical line also has a compact `LineMetrics { visualRowCount: uint32, height: int32, exact: bool }` (about 16 bytes) stored in a vector parallel to the line index.
- An evicted line keeps its `LineMetrics` and is re-laid out on demand.

**Incremental re-layout within a line (R-13)**: an edit re-wraps from the visual row that contains the edit offset until a row break resynchronises with the old breaks shifted by the edit delta. With wrap off, the row's glyph vector is spliced and the later x-positions are shifted.

## 9. Viewport `[Layout Engine]`

`{ anchor: { logicalLine, rowInLine, pixelOffset }, scrollX, clientWidth, clientHeight, scale, wrapEnabled, firstVisibleRow, lastVisibleRow }`

Scrolling moves the anchor. The document's total height is `Σ exact or estimated row heights`.

## 10. DirtyRegion `[Incremental Update Manager]`

`{ rects: small_vector<Rect, 16>, layoutLines: set<lineIndex>, fullViewport: bool }`. Adding a 17th rectangle collapses the set to its bounding box. The region is cleared after `present()`.

## 11. EditorState `[Editor Controller]`

| Field | Notes |
|---|---|
| `caret` | offset |
| `anchor` | offset; the selection is `[min(anchor,caret), max(anchor,caret))` |
| `preferredX` | pixel x, kept across vertical moves and **recomputed after a font change** from the caret offset |
| `activeFile` | from `TextDocument.fileInfo` |
| `wordWrap` | bool, default true |
| `activePackId` | mirrors `FontRegistry.activePackId` |
| `mode` | `EditPlainText` or `PreviewHtml` |

Font switch rule: `caret` and `anchor` are unchanged (SC-004). Only `preferredX` and the scroll anchor's `rowInLine`/`pixelOffset` are recomputed, keeping the caret line visible.

## 12. RenderStats and DiagnosticsLog `[cross-cutting, read-only views]`

`RenderStats` fields, including `cacheHitRate` and the process-level resource samples, are listed in research R-16.

## 12a. Preferences `[File Storage]`

`Preferences = { lastPackId: string, wordWrap: bool }`, persisted through `IPreferencesStore` (research R-20). It is loaded at startup and saved whenever the font or wrap setting changes. Failures are non-fatal and produce diagnostics.
`Diagnostic = { severity: Info|Warning|Error, code, message, sourcePath?, line?, column?, timestamp }`. Entries are de-duplicated by `(code, sourcePath, line, column)`.

## Relationships

```text
FontRegistry 1──* FontPack 1──1 FontManifest
FontPack 1──* GlyphDefinition ──(rasterized at key)──> RasterGlyph (RasterCache)
TextDocument 1──* LayoutLine 1──* VisualRow 1──* PositionedGlyph ──> ResolvedGlyph
PreviewDocument 1──* DocumentNode ──(styled runs)──> LayoutLine
Viewport ──selects──> VisualRow range ;  UpdateManager ──produces──> DirtyRegion
EditorState ──references──> TextDocument offsets, FontRegistry.activePackId
```
