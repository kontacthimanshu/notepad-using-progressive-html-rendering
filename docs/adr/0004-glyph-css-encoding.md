# ADR 0004 — Glyph and manifest encoding as valid CSS custom properties

- **Status**: Accepted
- **Date**: 2026-11-21
- **Context**: Constitution II makes CSS the authoritative source of glyph geometry, with one file per character,
  read by an explicit parser (FR-006), with location-aware validation (FR-005). Research R-05, R-06.

## Decision

- A glyph file is **one** `glyph { … }` rule of custom properties: `--schema`, `--code-point`, `--family`,
  `--width`, `--height`, `--baseline`, `--advance`, optional `--bearing-x`, `--style`, `--note`, `--meta-*`, and
  `--rows` as a sequence of CSS strings of space-separated cell tokens (`.`, `@`, `~`, `#RRGGBB`, `#RRGGBBAA`).
- A pack manifest is one `font-pack { … }` rule in the same dialect.
- Both parsers (`src/glyphcss`) share the CSS Syntax-Level-3-subset tokenizer in `src/core` with the separate
  presentation-CSS parser (`src/presentcss`) — the three dialects never share parsing logic (FR-041).
- Glyphs are authored as tight ink boxes; `--baseline` may exceed `--height` for floating glyphs (`-`, `^`, quotes).
- Glyph identity for caching/reload is the FNV-1a 64 hash of the file text with CRLF normalised to LF.

## Consequences

- Ordinary CSS tooling opens every glyph file without errors; diffs are human-readable.
- `currentcolor`/`backgroundcolor` cells give foreground and background substitution (Constitution II) without
  per-colour glyph variants; literal colours still allow multi-colour glyphs.
- No JSON dependency is needed for manifests.

## Alternatives considered

- **`box-shadow` pixel art**: cannot express declared dimensions or validate row lengths.
- **Custom at-rules (`@glyph`)**: flagged by generic CSS tools.
- **JSON manifests**: an extra dependency for about a dozen fields.
- **Palette/indexed colours**: unnecessary with the `@`/`~` aliases; deferred.
