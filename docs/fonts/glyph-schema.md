# Glyph CSS schema `pixelcss-glyph/1`

Every character of every PixelCSS font is one plain CSS file: `fonts/<pack-id>/glyphs/U+XXXX.css`. The file is valid
CSS (editors and linters open it without errors), and PixelCSS gives it meaning with its own parser — no browser CSS
engine is involved. The normative contract is
[`specs/001-pixelcss-browser/contracts/glyph-css-schema.md`](../../specs/001-pixelcss-browser/contracts/glyph-css-schema.md).

## A complete example

```css
/* verdana-style A — original artwork, OFL-1.1 */
glyph {
  --schema: "pixelcss-glyph/1";
  --code-point: U+0041;          /* must match the file name U+0041.css */
  --family: "verdana-style";     /* must match the pack id */
  --width: 7;                    /* cells per row */
  --height: 10;                  /* rows */
  --baseline: 10;                /* rows above the baseline */
  --advance: 8;                  /* pen movement in cells */
  --rows:
    ". . . @ . . ."
    ". . @ . @ . ."
    ". . @ . @ . ."
    ". . @ . @ . ."
    ". @ . . . @ ."
    ". @ . . . @ ."
    ". @ @ @ @ @ ."
    "@ . . . . . @"
    "@ . . . . . @"
    "@ . . . . . @";
}
```

## Cell tokens

| Token | Meaning |
|---|---|
| `.` or `transparent` | no ink; the background shows through |
| `@` or `currentcolor` | ink in the text colour (black text, white when selected, any HTML `color`) |
| `~` or `backgroundcolor` | ink in the background colour (knock-outs, e.g. the `?` inside `U+FFFD`) |
| `#RRGGBB` / `#RRGGBBAA` | a literal colour (multi-colour glyphs); `AA = 00` is transparent |

Optional properties: `--bearing-x` (left side bearing, −16..16), `--style` (default `"regular"`), `--note`, and any
number of `--meta-<name>` metadata entries (kept, ignored by the renderer).

## Geometry: the tight ink box

Draw only the inked rectangle. `--width`/`--height` cover the ink; `--bearing-x` is the empty space on the left;
`--advance` is the whole pen step; `--baseline` counts the glyph's rows that are **above** the baseline.

| Glyph (Verdana-style) | width × height | baseline | meaning |
|---|---|---|---|
| `A` | 7 × 10 | 10 | sits on the baseline, cap height 10 |
| `b` | 6 × 11 | 11 | ascender reaches one row above the caps |
| `g` | 6 × 10 | 7 | 7 rows above the baseline + 3 descender rows |
| `-` | 4 × 1 | 4 | floats 3 rows above the baseline (baseline may exceed height) |
| `_` | 7 × 1 | 0 | entirely in the first row below the baseline |
| space | 0 × 0 | 0 | no ink, `--advance: 4`, no `--rows` |

A cell `(col, row)` is drawn at `x = pen + (bearing + col)·s`, `y = baseline + (row − baseline)·s` with an integer
scale `s` (1 at 100–125 % display scaling, 2 at 150–200 %, 3 at 250 %).

## Errors you may see

`pixelcss-tool validate <pack-dir>` (or Help → Diagnostics in the app) reports problems with file, line and column:

```
fonts/my-pack/glyphs/U+0042.css:14:5: error E-GLYPH-ROW-LENGTH: row 3 has 6 cells, expected 7
```

| Code | Cause |
|---|---|
| `E-GLYPH-SCHEMA` | `--schema` is not `"pixelcss-glyph/1"` |
| `E-GLYPH-MISSING-PROP` / `E-GLYPH-DUP-PROP` | a required property is missing / declared twice |
| `E-GLYPH-BAD-DIM` | a number out of range (width/height 0–64, advance 0–128, …) |
| `E-GLYPH-ROW-COUNT` / `E-GLYPH-ROW-LENGTH` | wrong number of rows / cells in a row |
| `E-GLYPH-BAD-COLOR` | an unknown cell token such as `#GG0000` or `#FFF` |
| `E-GLYPH-CP-MISMATCH` / `E-GLYPH-FAMILY-MISMATCH` | `--code-point` / `--family` disagree with the file name / pack id |
| `E-GLYPH-SYNTAX` | not one well-formed `glyph { … }` rule |
| `W-GLYPH-EXCEEDS-METRICS` | the glyph reaches outside the pack's ascent/descent |
| `W-GLYPH-UNTRIMMED` | fully transparent edge rows/columns (trim the ink box) |

A broken glyph never crashes the app: the pack becomes *Degraded* and that character shows the pack's `U+FFFD`
replacement glyph (or the engine's hollow box).
