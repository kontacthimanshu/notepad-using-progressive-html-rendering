# Contract: Font Pack Manifest `pixelcss-pack/1`

**Satisfies**: FR-007, FR-008, FR-012, FR-013, US6, Constitution III and XII.

## Pack layout

```text
fonts/<pack-id>/
├── manifest.css
├── LICENSE.txt            # referenced by --license-file
└── glyphs/
    ├── U+0020.css … U+007E.css   # 95 printable ASCII (bundled packs)
    └── U+FFFD.css                # pack replacement glyph
```

Packs are discovered as `<exe-dir>/fonts/*/manifest.css`. Developer builds also accept `--fonts-dir <path>`. Adding a directory registers a new family without any engine code change (US6-AS3).

## manifest.css

```css
font-pack {
  --schema: "pixelcss-pack/1";
  --id: "verdana-style";
  --display-name: "Verdana";
  --style-description: "Wide, open, screen-oriented sans-serif pixel interpretation";
  --version: "1.0.0";
  --authenticity: "style-interpretation";   /* or "licensed-original" */
  --origin: "Original pixel artwork created for PixelCSS Browser";
  --author: "PixelCSS contributors";
  --license: "OFL-1.1";
  --license-file: "LICENSE.txt";
  --line-height: 15;
  --ascent: 11;
  --descent: 3;
  --coverage: U+0020-007E, U+FFFD;
  --replacement-glyph: U+FFFD;              /* optional, default U+FFFD */
  --glyph-dir: "glyphs";                    /* optional */
  --aliases: "Verdana", "verdana-style";    /* optional, for presentation CSS font-family */
  --order: 1;                               /* optional dropdown order */
}
```

## Validation (performed by `FontRegistry::load` and `pixelcss-tool validate`)

| Code | Condition |
|---|---|
| `E-PACK-SCHEMA` | `--schema` is missing or not `pixelcss-pack/1` |
| `E-PACK-MISSING-PROP` | any of id, display-name, style-description, version, authenticity, origin, author, license, license-file, line-height, ascent, descent, coverage is absent |
| `E-PACK-ID-MISMATCH` | `--id` ≠ directory name |
| `E-PACK-DUP-ID` | two packs declare the same id |
| `E-PACK-METRICS` | `ascent + descent > line-height`, or any value is negative |
| `E-PACK-LICENSE-FILE` | the license file is missing |
| `E-PACK-COVERAGE-MISSING` | a covered code point has no glyph file (pack becomes `Degraded`) |
| `E-PACK-DUP-GLYPH` | more than one file maps to the same code point |
| `W-PACK-EXTRA-GLYPH` | a glyph file outside the declared coverage (loaded, warning only) |
| `E-PACK-AUTHENTICITY` | `licensed-original` without a non-empty `--license` naming the rights holder |
| `W-GLYPH-EXCEEDS-METRICS` | a glyph's `baseline > ascent` or `height − baseline > descent` (error under `--strict`) |
| `W-GLYPH-UNTRIMMED` | a glyph has fully transparent edge rows or columns (warning; error under `--strict`) |

**Bundled-pack gate** (SC-002): `pixelcss-tool validate --strict fonts/` must report the three packs as `Valid`, each with 95/95 ASCII coverage plus U+FFFD, for a total of ≥ 285 ASCII glyph files.

## UI disclosure (FR-013, US3-AS6)

When `authenticity = style-interpretation`:
- The dropdown tooltip reads `"<display-name> (style interpretation — not the original typeface)"`.
- **Help → About Fonts** lists, for each pack: display name, id, version, author, origin, licence and authenticity.
