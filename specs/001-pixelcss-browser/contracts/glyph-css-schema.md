# Contract: Glyph CSS Schema `pixelcss-glyph/1`

**Satisfies**: FR-001–FR-006, Constitution I and II. **Parser**: `pxc_glyphcss` (independent of the HTML and presentation-CSS parsers, FR-041).

## File placement

```text
fonts/<pack-id>/glyphs/U+XXXX.css
```

`XXXX` is the code point as 4–6 uppercase hex digits, zero-padded to at least 4 (`U+0041.css`, `U+1F600.css`). Exactly one file per code point per pack. A duplicate (for example `U+041.css` alongside `U+0041.css`, or a case variant) is an `E-PACK-DUP-GLYPH` error.

## Syntax

Every file must be valid CSS 2.1/3 syntax (UTF-8, no BOM required). It holds exactly one rule with the type selector `glyph`. Comments `/* … */` are allowed anywhere.

```css
/* Verdana-style "A" — original artwork, OFL-1.1 */
glyph {
  --schema: "pixelcss-glyph/1";
  --code-point: U+0041;
  --family: "verdana-style";
  --style: "regular";
  --width: 7;
  --height: 10;
  --baseline: 10;      /* rows above the baseline; 10 = glyph sits on the baseline */
  --advance: 8;
  --bearing-x: 0;      /* optional, default 0 */
  --rows:
    ". . . @ . . ."
    ". . . @ . . ."
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

### Required properties

| Property | Value grammar | Notes |
|---|---|---|
| `--schema` | `<string>` | must be `"pixelcss-glyph/1"`; any other value → `E-GLYPH-SCHEMA` |
| `--code-point` | `U+<hex>` (CSS unicode-range token, single value) | must equal the filename |
| `--family` | `<string>` | must equal the manifest `--id` |
| `--width`, `--height` | `<integer>` 0–64 | |
| `--baseline` | `<integer>` 0–64 | may exceed `--height` when the glyph floats above the baseline |
| `--advance` | `<integer>` 0–128 | |
| `--rows` | `<string>{height}` | whitespace- or newline-separated CSS strings; omitted or empty when `height = 0` |

### Optional properties

`--style` (`<string>`, default `"regular"`), `--bearing-x` (`<integer>` −16..16, default 0), `--note` (`<string>`, ignored by the renderer), and **per-character metadata** `--meta-<name>` (`<string>` or `<number>`; `<name>` matches `[a-z0-9-]+`), which is stored verbatim in `GlyphDefinition::metadata` and ignored by the renderer. Other unknown properties produce `W-GLYPH-UNKNOWN-PROP` and are ignored. Standard (non-custom) CSS properties inside `glyph {}` produce the same warning.

### Cell tokens (inside each row string, separated by one or more spaces)

| Token | Meaning |
|---|---|
| `#RRGGBB` | opaque literal colour |
| `#RRGGBBAA` | literal colour with alpha; `AA = 00` is treated as transparent |
| `transparent` or `.` | no ink; never overwrites the background |
| `currentcolor` or `@` | **foreground substitution**: ink in the run's foreground colour (text colour, or selection text colour) |
| `backgroundcolor` or `~` | **background substitution**: ink in the run's background colour (page, element `background-color`, or selection fill). It is used for knock-outs and counters that must match the background, and is opaque even over other ink |

Hex is case-insensitive. Any other token → `E-GLYPH-BAD-COLOR` at that token's line and column. A row token count ≠ `--width` → `E-GLYPH-ROW-LENGTH`. A string count ≠ `--height` → `E-GLYPH-ROW-COUNT`.

## Geometry

Pen position `(penX, baselineY)` is in device pixels and `s` is the integer scale. Cell `(col, row)` covers the rectangle

```text
x = penX + (bearingX + col) * s,   y = baselineY + (row - baseline) * s,   size s × s
```

After the glyph, `penX += advance * s`. The line box height is `manifest.lineHeight * s`, and `baselineY = lineTop + ascent * s`.

## Authoring convention (normative for bundled packs)

Glyphs are authored as a **tight ink box**:

- `--width` is the number of columns from the leftmost to the rightmost inked column. `--bearing-x` is the left side bearing, and `--advance` is the full pen advance including both side bearings.
- `--height` is the number of rows from the topmost to the bottommost inked row. `--baseline` is the number of those rows that lie **above** the baseline. The remaining `height − baseline` rows are descender rows.
- Examples for Verdana-style (cap height 10, ascender 11, x-height 7, descender 3):
  - `A` → height 10, baseline 10.
  - `b` → height 11, baseline 11.
  - `x` → height 7, baseline 7.
  - `g` → height 10, baseline 7.
  - `-` (hyphen) → height 1, baseline 4.
  - space → width 0, height 0, baseline 0, advance 4, with `--rows` omitted.
- A glyph must fit the manifest metrics. `baseline ≤ ascent` and `height − baseline ≤ descent`, otherwise the warning `W-GLYPH-EXCEEDS-METRICS` is reported (it is an error under `validate --strict`).
- Rows and columns that are entirely transparent at the edges of the box should be removed. `validate --strict` warns with `W-GLYPH-UNTRIMMED` when they are present.

## Diagnostics format

```text
<path>:<line>:<column>: error E-GLYPH-ROW-LENGTH: row 4 has 6 cells, expected 7
```

The parser never throws across the module boundary. It returns `Expected<GlyphDefinition, std::vector<Diagnostic>>`.

## Conformance fixtures

`tests/fixtures/glyphs/valid/*` and `tests/fixtures/glyphs/invalid/<error-code>/*`. Each error code has at least one fixture, plus edge cases: an empty space glyph, a transparent first/last row, a non-square glyph, a multi-colour glyph, `#RRGGBB00`, mixed-case hex, CRLF files, comments between rows, `backgroundcolor`/`~` cells, and `--meta-*` properties.
