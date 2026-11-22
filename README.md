# Niche Notepad (Because one can write niche in Nice Notepad easily and other do not need Niche Notepad to read it, they just need a web browser.)

PixelCSS Browser is a lightweight native Windows notepad and restricted HTML viewer whose **document text is
rendered entirely by its own software engine**. Each character of each font is a plain CSS file
(`fonts/<pack>/glyphs/U+0041.css`) holding a grid of pixel colours; the engine parses it, rasterizes it into its own
framebuffer and copies the pixels to the window. No TrueType/OpenType, DirectWrite, GDI text drawing, WebView2,
Chromium or other browser engine is used for document characters.

It is written in C++20 and built with Microsoft Visual C++ (`cl.exe`) for Windows x64.

> **Fonts disclaimer.** The toolbar offers **Verdana**, **Arial Narrow** and **Comic Sans**. These are *original
> pixel artwork drawn in the style of* those typefaces — they are **not** the proprietary Microsoft fonts and
> contain no data derived from them. They are licensed under the SIL Open Font License 1.1. **Help → About Fonts**
> shows each pack's origin, author, license and authenticity.

---

## Salient features

- **Characters defined in CSS.** Every glyph is one human-readable, valid CSS file of pixel cells
  (`@` ink, `.` transparent, `~` background knock-out, or literal `#RRGGBB` / `#RRGGBBAA` colours). Edit a file,
  press **Ctrl+Shift+R**, and the change appears — no rebuild.
- **Three independent pixel font families**, 96 glyphs each (all 95 printable ASCII characters plus a replacement
  glyph): wide *Verdana-style*, condensed *Arial Narrow-style* and rounded *Comic Sans-style*. Switching fonts is
  instant and never changes the text, caret or selection.
- **Its own rendering engine**: integer alpha compositing into an owned framebuffer, nearest-neighbour integer
  scaling for high-DPI displays (×1 at 100–125 %, ×2 at 150–200 %, ×3 at 250 %), glyph raster cache, and
  pixel-identical output for identical input.
- **Progressive and incremental rendering**: only the visible lines are laid out before the first paint (a
  100,000-character file shows in ~1.4 ms); the rest is measured in the background. A keystroke repaints only the
  affected part of its row; scrolling moves existing pixels and paints only the newly exposed band. Very long lines
  are re-wrapped only from the edited row.
- **A real notepad**: New/Open/Save/Save As with an unsaved-changes prompt, typing, Backspace/Delete, arrows,
  Home/End, word and page navigation, mouse selection, cut/copy/paste of ordinary Unicode text, coalescing
  undo/redo, word wrap, and horizontal/vertical scrolling. Files round-trip byte-for-byte as UTF-8 (BOM and line
  endings preserved); UTF-16 files open; invalid UTF-8 can be opened with replacement characters; saves are atomic.
- **Unicode-safe**: characters the fonts cannot draw (é, 漢, 😀 …) are kept exactly in the document and shown as a
  replacement glyph.
- **Restricted HTML preview**: `.html` files open read-only with `html/head/body/div/p/span/br`, a small CSS subset
  (`color`, `background-color`, `font-family`, `margin-top/bottom`, `line-height`, `display`, with `<style>` and
  `style=""`, type/class/id selectors and specificity). Scripts never run, links are never followed and nothing is
  ever downloaded — the executable contains no networking or scripting code at all.
- **View source**: **View → Page Source** (Ctrl+U) flips an HTML file between the rendered preview and its HTML
  source, drawn with the same CSS glyphs. The source is editable; press Ctrl+U again to see your changes rendered,
  and Ctrl+S to save them.
- **Robust**: malformed glyphs, missing packs and file errors never crash the app; problems are listed with file,
  line and column in **Help → Diagnostics**, and the affected characters fall back to a replacement glyph.
- **Measured**: an **F12** overlay shows cache hits, rasterizations, lines laid out, dirty area, paint time and
  keystroke-to-present latency, drawn with the app's own glyphs. On the reference machine the end-to-end p95
  keystroke-to-present latency is **1.6 ms** (budget 50 ms) and startup is ~52 ms
  ([results](docs/perf/reference-machine.md)).
- **Extensible**: add a font by adding a folder with a `manifest.css` and glyph files — no engine change
  ([guide](docs/fonts/adding-a-pack.md)).
- **Self-contained**: a ~650 KB executable with a statically linked C runtime; runs offline with no installer and
  no redistributable.

---

## Launching the software

### Option A — from a release package

1. Build the package (see below) or obtain `PixelCSS-<version>-win64.zip`.
2. Extract it anywhere. Keep the `fonts` folder next to `PixelCSS.exe`.
3. Double-click **`PixelCSS.exe`** (or run it from a terminal).

### Option B — from source

Prerequisites (details in [docs/build/building.md](docs/build/building.md)):

- Windows 10/11 x64
- Visual Studio 2022 (17.10+) or Visual Studio 2026 / Build Tools with the **Desktop development with C++** workload
  (MSVC v143 or newer) and Windows SDK 10.0.26100
- CMake 3.28+

```powershell
git clone <this repository>
cd notepad-using-progressive-html-rendering

cmake --preset x64-release
cmake --build --preset x64-release
.\out\build\x64-release\src\shell\Release\PixelCSS.exe
```

Or do everything — clean configure, build and test Debug and Release, package, and verify the package — in one go:

```powershell
.\scripts\build-all.ps1 -Force          # add -Perf to also run the performance gate
```

The release ZIP is written to `out\package\PixelCSS-<version>-win64.zip`.

To debug in Visual Studio: `cmake --preset x64-debug`, open `out\build\x64-debug\PixelCSS.sln`, press **F5**
([docs/build/debugging.md](docs/build/debugging.md)).

### Command line

```text
PixelCSS.exe [file]                          open a .txt file, or a .html/.htm file in read-only preview
PixelCSS.exe --fonts-dir <dir>               also load font packs from another folder
PixelCSS.exe --no-preferences                do not read/write remembered font and word-wrap settings
PixelCSS.exe --stats-json <file>             write per-frame render statistics on exit
PixelCSS.exe --bench-script <file>           drive the UI from a script (used by the benchmarks)
```

---

## Using it

| Action | How |
|---|---|
| Change font | **Font** dropdown in the toolbar (Verdana, Arial Narrow, Comic Sans) |
| View HTML source / back to preview | **View → Page Source** (Ctrl+U), HTML files only |
| Word wrap | toolbar **Word Wrap** button or **View → Word Wrap** |
| New / Open / Save / Save As | Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S |
| Undo / Redo | Ctrl+Z / Ctrl+Y (or Ctrl+Shift+Z) |
| Cut / Copy / Paste / Select All | Ctrl+X / Ctrl+C / Ctrl+V / Ctrl+A |
| Word / document navigation | Ctrl+←/→, Ctrl+Home/End, PgUp/PgDn; add Shift to select |
| Reload font packs from disk | **View → Reload Fonts** (Ctrl+Shift+R) |
| Render statistics overlay | **F12** |
| Font provenance | **Help → About Fonts** |
| Problems (bad glyphs, HTML errors, file errors) | **Help → Diagnostics** |

Your last font and word-wrap choice are remembered in `HKCU\Software\PixelCSS\Display`.

### Developer tool

```powershell
pixelcss-tool validate --strict fonts                       # check every pack (all 3 bundled packs: Valid, 285 ASCII glyphs)
pixelcss-tool render --pack comic-style --text "Hello" --scale 3 --out hello.bmp
pixelcss-tool compare a.bmp b.bmp --diff diff.bmp           # pixel-exact comparison
pixelcss-tool distinct fonts                                # prove the packs are independent designs
pixelcss-tool html --in page.html --out page.bmp --width 480
```

---

## Testing

```powershell
ctest --preset x64-debug                 # unit, contract, integration and visual tests (Debug adds CRT leak checks)
ctest --preset x64-release-perf          # performance gate (absolute budgets with PIXELCSS_REFERENCE_MACHINE=1)
ctest --preset x64-release-package       # build the ZIP and verify its contents
```

| Label | What it covers |
|---|---|
| `unit` | CSS tokenizer, Unicode codecs, framebuffer/BMP, blending, raster cache, dirty regions, text codecs |
| `contract` | glyph & manifest parsers, text document, layout, editor, font registry, preferences, HTML parser, presentation CSS, pack validation |
| `integration` | file round-trips and failures, font switching, incremental-paint oracle (incremental == full repaint after 200 random operations), progressive open, 10M-character document, pack reload, binary import scan (no network / text-drawing APIs), resource cleanup, CLI end-to-end |
| `visual` | pixel-exact golden images of every glyph of every pack, editor surface, wrapping, clipping, selection, fallbacks, scale 2, HTML previews |
| `perf` | `pixelcss-bench` budgets and regressions |
| `package` | release ZIP layout |

---

## Repository layout

```text
fonts/                     the three bundled font packs (manifest.css, LICENSE.txt, glyphs/U+XXXX.css)
src/core                   Expected, diagnostics, geometry, Unicode, CSS tokenizer, scale policy
src/glyphcss               glyph and manifest CSS parsers
src/render                 framebuffer, integer compositing, BMP I/O
src/fonts                  font registry and fallback chain, rasterizer, raster cache
src/document               piece-table text document, line index, undo/redo
src/storage                encodings, line endings, atomic saves, preferences (Win32 parts in storage/win32)
src/layout                 incremental line layout, viewport, HTML block layout
src/update                 dirty regions, incremental painter, HTML preview painter, render statistics
src/editor                 editor commands, file workflow (New/Open/Save/Exit)
src/presentcss, src/html   restricted presentation CSS and HTML preview model
src/shell                  Win32 application (window, toolbar, surface, dialogs, bench runner)
tools/                     pixelcss-tool (developer CLI), pixelcss-bench (benchmarks)
tests/                     unit, contract, integration, visual (goldens), perf, package tests and fixtures
docs/                      build, fonts, performance and architecture decision records
specs/001-pixelcss-browser specification, plan, research, data model, contracts, tasks (GitHub Spec Kit)
```

## Documentation

- Build, debug and warning policy: [docs/build](docs/build)
- Glyph format: [docs/fonts/glyph-schema.md](docs/fonts/glyph-schema.md) · Adding a pack:
  [docs/fonts/adding-a-pack.md](docs/fonts/adding-a-pack.md) · Design notes:
  [docs/fonts/design-notes.md](docs/fonts/design-notes.md) · Licensing review:
  [docs/fonts/licensing-review.md](docs/fonts/licensing-review.md)
- Performance: [docs/perf/reference-machine.md](docs/perf/reference-machine.md)
- Architecture decisions: [docs/adr](docs/adr)
- Specification and design: [specs/001-pixelcss-browser](specs/001-pixelcss-browser)

## License

The application and tools are licensed under the GNU General Public License v3 ([LICENSE](LICENSE)). The bundled
font packs are original artwork licensed under the SIL Open Font License 1.1 (see each pack's `LICENSE.txt`).
