# Bundled pack design notes

All three bundled packs are **original pixel artwork** created for PixelCSS Browser and licensed under the SIL Open
Font License 1.1. They are independent *stylistic interpretations*: they are **not** Verdana, Arial Narrow or
Comic Sans, contain no outlines, bitmaps or data derived from those typefaces, and are identified as
`--authenticity: "style-interpretation"` in their manifests and in **Help → About Fonts** (FR-013).

| Pack | Dropdown label | Line height | Ascent / descent | Cap / ascender / x-height | Advances | Character |
|---|---|---|---|---|---|---|
| `verdana-style` | Verdana | 15 | 11 / 3 | 10 / 11 / 7 | caps 5–10, lower 3–10, digits 8 | wide, open counters, large x-height, serifed `I`, slashed zero |
| `arial-narrow-style` | Arial Narrow | 15 | 11 / 3 | 10 / 11 / 6 | caps 2–8, lower 2–6, digits 5 | condensed, tight sidebearings, plain `I`, narrow bowls |
| `comic-style` | Comic Sans | 16 | 12 / 3 | 11 / 12 / 7 | caps 5–10, lower 3–10, digits 7 | rounded corners, informal strokes, single-storey `a` and `g` |

Each pack also ships `U+FFFD` (a framed question mark using `backgroundcolor` knock-out cells), used for any
character the pack does not cover (FR-010).

## Independence evidence (FR-009)

`pixelcss-tool distinct fonts` compares every shared glyph pair after cropping to the ink box:

| Pair | Identical glyphs |
|---|---|
| verdana-style / arial-narrow-style | 9.5 % (mostly 1-cell punctuation such as `'`, `.`, `|`) |
| verdana-style / comic-style | 4.2 % |
| arial-narrow-style / comic-style | 0 % |

The gate fails above 60 %.

## Conventions

- Glyphs use the tight-ink-box convention (see [glyph-schema.md](glyph-schema.md)); `validate --strict` rejects
  untrimmed edges and glyphs that leave the pack metrics.
- Strokes are one cell wide at scale 1; integer scaling (×2, ×3) keeps them crisp on high-DPI displays.
- Each file starts with `/* <pack> <char> — original artwork, OFL-1.1 */`.
