# Font licensing review (Constitution XII)

- **Date**: 2026-11-21
- **Scope**: the three bundled packs in `fonts/`, the repository license and third-party code.
- **Result**: PASS.

## Glyph artwork

| Check | Method | Result |
|---|---|---|
| Every glyph file carries the original-artwork header `/* <pack> <char> — original artwork, OFL-1.1 */` | scripted scan of `fonts/*/glyphs/U+*.css` | **288 / 288** files carry it |
| No glyph is derived from proprietary outlines, bitmaps or screenshots | review of the authoring process: every glyph was drawn as an original pixel sketch for this project and converted to CSS; no font file, font rasterizer or screenshot was used as input | no derived artwork |
| The three packs are independent designs, not resized copies (FR-009) | `pixelcss-tool distinct fonts` | 9.5 % / 4.2 % / 0 % identical (gate: ≤ 60 %) |

## Manifests

| Pack | `--authenticity` | `--license` | `--origin` | `--author` | `LICENSE.txt` |
|---|---|---|---|---|---|
| verdana-style | style-interpretation | OFL-1.1 | Original pixel artwork created for PixelCSS Browser | PixelCSS contributors | full SIL OFL 1.1 text + trademark disclaimer |
| arial-narrow-style | style-interpretation | OFL-1.1 | Original pixel artwork created for PixelCSS Browser | PixelCSS contributors | full SIL OFL 1.1 text + trademark disclaimer |
| comic-style | style-interpretation | OFL-1.1 | Original pixel artwork created for PixelCSS Browser | PixelCSS contributors | full SIL OFL 1.1 text + trademark disclaimer |

The names "Verdana", "Arial Narrow" and "Comic Sans" are used only as user-facing style labels (FR-025). The UI
discloses that the packs are not the original typefaces: the dropdown tooltip, the status bar ("(style)"),
**Help → About Fonts**, the packaged `README.txt` and each pack's `LICENSE.txt`.

## Repository and third-party code

| Component | License | Shipped in the release ZIP? |
|---|---|---|
| PixelCSS application and tools | GPL-3.0 (`LICENSE`, packaged as `LICENSES/PixelCSS-LICENSE.txt`) | yes |
| Bundled font packs | SIL OFL 1.1 (packaged as `LICENSES/PixelCSS-fonts-OFL.txt` and per pack) | yes |
| doctest 2.4.11 (`third_party/doctest`) | MIT | **no** — test-only; `tests/package/check_package.cmake` fails if it is shipped |

OFL-licensed fonts may be bundled with GPL software; the fonts remain under the OFL.
