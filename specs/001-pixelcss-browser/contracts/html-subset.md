# Contract: Restricted HTML and Presentation CSS Subset

**Satisfies**: FR-035–FR-041, US5, Constitution V. **Parsers**: `pxc_html` (HTML) and `pxc_presentcss` (presentation CSS). Both are independent of `pxc_glyphcss`.

## Input

Local files with the extension `.html` or `.htm`, opened via **File → Open**. The file is decoded per research R-11 (UTF-8 by default; `<meta charset>` is ignored). The result is shown in **read-only preview mode**: the title bar reads `"<name> — Preview (read-only)"`, editing keys are disabled, and selection and copy work.

## Elements

| Element | Behaviour |
|---|---|
| `html`, `head`, `body` | structural; implied when missing |
| `title` | its text sets the window title; it is not rendered |
| `div`, `p` | block boxes; `p` has a default `margin-top/bottom: 1em` (= 1 line height) |
| `span` | inline run |
| `br` | forced line break |
| `style` (in `head`) | its contents are parsed as presentation CSS |
| text nodes | whitespace collapsed (CSS `white-space: normal`); entities decoded |
| `script`, `noscript`, `iframe`, `object`, `embed`, `img`, `video`, `audio`, `link`, `meta`, `base` | **dropped with their content**; `W-HTML-UNSUPPORTED-ELEMENT` recorded |
| any other element (e.g. `a`, `b`, `ul`) | **transparent inline container**: its children are rendered and the tag is ignored; `W-HTML-UNKNOWN-ELEMENT` recorded |

Attributes kept: `id`, `class`, `style`. All other attributes are discarded. `on*` attributes are discarded and produce `W-HTML-EVENT-HANDLER-IGNORED`. `href` and `src` are never resolved, followed or fetched. The engine has no networking code path at all (FR-040, SC-007). This is verified by a test that asserts the import table of `PixelCSS.exe` contains no `winhttp`, `wininet`, `ws2_32` or `urlmon`.

Entities: `&amp; &lt; &gt; &quot; &apos; &#39; &nbsp;` and numeric `&#NNN; &#xHH;`. Unknown named entities are kept literally and produce `W-HTML-ENTITY`.

## Error recovery

- An end tag with no matching open element is ignored (`W-HTML-STRAY-END-TAG`).
- An unclosed element at EOF is auto-closed.
- Mis-nesting (`<p><div>`) closes the open `p` first, following HTML5 implied-end rules for `p` only.
- A malformed tag (`<div class="x>` running to EOF) produces `E-HTML-MALFORMED` with line and column. The parsed prefix is still rendered and a diagnostics banner is shown. The application never crashes (US5-AS4).

## Presentation CSS

**Sources**: `<style>` blocks in document order, then the `style=""` attribute. Specificity follows CSS 2.1 (`style` attribute > `#id` > `.class` > type), with ties broken by later source order. `!important` is ignored (`W-CSS-IMPORTANT-IGNORED`). `@import` and `url(...)` are ignored and never fetched (`W-CSS-EXTERNAL-IGNORED`).

**Selectors**: a single simple selector, or a comma-separated list of them: `tag`, `.class`, `#id`, `tag.class`. Combinators and pseudo-classes cause the whole rule to be ignored (`W-CSS-UNSUPPORTED-SELECTOR`).

| Property | Values | Inherited | Initial |
|---|---|---|---|
| `color` | `#rgb`, `#rrggbb`, `#rrggbbaa`, `rgb()`, basic named colours (16 CSS 2.1 names) | yes | `#000000` |
| `background-color` | same as `color`, plus `transparent` | no | `transparent` (page: `#FFFFFF`) |
| `font-family` | comma list; the first name matching a pack alias wins, otherwise the active pack | yes | the toolbar's active pack |
| `margin-top`, `margin-bottom` | `<n>px`, `<n>em` (em = line height of the element's pack), `0` | no | 0 (`p`: 1em) |
| `line-height` | `normal`, a `<number>` multiplier, or `<n>px` | yes | `normal` (= the pack's line height) |
| `display` | `block`, `inline`, `none` | no | per element |

Any other property is ignored with `W-CSS-UNSUPPORTED-PROPERTY`. Invalid values are ignored with `W-CSS-INVALID-VALUE`.

## Rendering

The preview uses the same `LayoutEngine`, `FontRegistry`, `RasterCache` and `Renderer` as the editor (FR-037). Changing the toolbar font changes the font of every element whose `font-family` did not resolve to a specific pack. Word wrap is always on in preview mode.

## Fixtures

`tests/fixtures/html/`: `basic.html` (all supported elements), `nested.html`, `styles.html`, `unsupported.html` (script, onclick, img src=http, link stylesheet), `malformed-*.html`. Each has a golden framebuffer at s = 1 and an expected diagnostics list.
