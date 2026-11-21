# Contract: Application UI and CLI

**Satisfies**: FR-024–FR-034, US2, US3, US7, Constitution IV and VIII.

## Main window

```text
┌ PixelCSS — Untitled* ─────────────────────────────────────────┐
│ File  Edit  View  Help                                        │
├───────────────────────────────────────────────────────────────┤
│ Font: [ Verdana        ▾]   [↵ Word Wrap]                      │  ← Win32 toolbar (system font OK)
├───────────────────────────────────────────────────────────────┤
│                                                               │
│   custom PixelCSSSurface — all document text is CSS glyphs    │  ← no EDIT/RichEdit control (FR-033)
│                                                               │
├───────────────────────────────────────────────────────────────┤
│ Ln 1, Col 1 │ UTF-8 │ CRLF │ Verdana (style) │ 100%           │  ← status bar
└───────────────────────────────────────────────────────────────┘
```

- **Font dropdown**: a `CBS_DROPDOWNLIST` combo box. Its items are the `Valid` and `Degraded` packs, in manifest `--order` (Verdana, Arial Narrow, Comic Sans). The initial selection is Verdana. Degraded packs show a ⚠ suffix. Invalid packs are omitted, and the problem is listed in Diagnostics. Changing the selection sends `SetFont` (FR-025, FR-026).
- The title shows `*` when the document is modified.

## Menus and shortcuts

| Menu | Item | Shortcut | Behaviour |
|---|---|---|---|
| File | New | Ctrl+N | prompts if modified |
| | Open… | Ctrl+O | `.txt` (edit) or `.html/.htm` (preview); prompts if modified |
| | Save | Ctrl+S | Save As if untitled; for an HTML file it writes the HTML source (from either view) |
| | Save As… | Ctrl+Shift+S | |
| | Exit | Alt+F4 | prompts if modified |
| Edit | Undo / Redo | Ctrl+Z / Ctrl+Y (also Ctrl+Shift+Z) | |
| | Cut / Copy / Paste | Ctrl+X / Ctrl+C / Ctrl+V | Unicode text only |
| | Delete | Del | |
| | Select All | Ctrl+A | |
| View | Page Source | Ctrl+U (toggle, checked in the source view) | HTML files only: switches between the read-only rendered preview and the editable HTML source, drawn with the same CSS glyphs; switching back re-renders the current, possibly unsaved, source; the preview scroll position is restored. Title shows "— Preview (read-only)" or "— Source" |
| | Word Wrap | — (toggle) | mirrors the toolbar button; persisted (research R-20) |
| | Reload Fonts | Ctrl+Shift+R | re-reads all packs; keeps the active pack if still valid, otherwise falls back to Verdana with a status notice |
| | Render Statistics | F12 | overlay drawn with pack glyphs |
| Help | Diagnostics… | — | lists DiagnosticsLog entries |
| | About Fonts… | — | provenance and authenticity per pack (FR-013) |
| | About PixelCSS… | — | version, toolchain |

**Navigation keys**: arrows, Home/End (visual row), Ctrl+Home/End (document), PgUp/PgDn, Ctrl+←/→ (word). Holding Shift extends the selection with any of these. Mouse click, drag-select and double-click word select are supported, as are the mouse wheel (vertical) and Shift+wheel (horizontal) and both scrollbars.

## Unsaved-changes prompt (FR-027, US2-AS5)

A modal dialog: **"Save changes to <name>?"** with the buttons [Save] [Don't Save] [Cancel]. *Cancel* aborts the pending New, Open or Exit. *Save* that fails shows the error and aborts the pending action, so content is never lost (FR-034).

## Error presentation (FR-034, SC-008)

- File errors are shown in a modal message box with a readable message (e.g. "PixelCSS could not save 'notes.txt' because another program is using it."). The document stays intact.
- Asset errors (glyph or pack) show a non-modal status-bar notice ("2 font problems — Help → Diagnostics"), and rendering continues using the fallback.
- HTML errors show a yellow banner row inside the preview surface, drawn with pack glyphs.

## Command line

`PixelCSS.exe [file] [--fonts-dir <dir>] [--stats-json <out.json>] [--bench-script <script>] [--exit-after-first-present] [--no-preferences]`

- `--stats-json` writes JSON lines containing per-frame `RenderStats` plus process samples (`gdiObjects`, `userObjects`, `handleCount`, `workingSetBytes`) at startup, after first present and at exit. The first-present record includes `firstPresentQpc`, the raw QPC value, so external tools can compute startup time on the same clock.
- `--exit-after-first-present` posts `WM_CLOSE` right after the first frame is presented. This is used for the startup benchmark.
- `--no-preferences` neither reads nor writes `HKCU` preferences, which keeps benchmarks and tests deterministic.

### Bench script format (`--bench-script`)

The file is UTF-8 with one command per line. Lines starting with `#` are comments. Commands run in order. Each command runs only after the previous one's frame has been presented.

| Command | Effect |
|---|---|
| `open <path>` | opens a file (prompts are auto-answered "Don't Save"). A relative path resolves against the script file's directory |
| `type <text>` | posts one `WM_CHAR` per code point; each keystroke is timed for `keyToPresentMicros` |
| `key <name>[+shift][+ctrl]` | posts `WM_KEYDOWN` for `left right up down home end pgup pgdn enter back delete` |
| `font <pack-id>` | selects the pack via the dropdown path |
| `wrap on\|off` | sets word wrap |
| `resize <w> <h>` | resizes the client area |
| `scroll <rows>` | scrolls by visual rows |
| `new` | sends File → New (auto "Don't Save") |
| `repeat <n>` … `end` | repeats the enclosed block |
| `wait-idle` | waits until idle layout completes |
| `source` | sends View → Page Source (toggles an HTML file between preview and source) |
| `sample` | records a process resource sample |
| `exit` | closes the app |

Timestamps use QPC at the moment of injection, so latency covers the message queue, editing, layout, paint and `BitBlt`.

## pixelcss-tool.exe (developer CLI, also packaged)

| Command | Purpose | Exit code |
|---|---|---|
| `validate [--strict] <fonts-dir or pack-dir>` | validates manifests and glyphs; prints diagnostics and a coverage table | 0 ok, 1 errors, 2 usage |
| `render --pack <id> --text "<s>" [--scale N] [--fg #hex] [--bg #hex] --out <file.bmp>` | headless render through the real engine | 0 / 1 |
| `compare <a.bmp> <b.bmp> [--diff out.bmp]` | pixel-exact comparison | 0 equal, 1 different |
| `distinct <fonts-dir>` | FR-009 geometry-distinctness report | 0 / 1 |
| `html --in <file.html> --out <file.bmp> [--width W]` | headless HTML preview render | 0 / 1 |

Output images are 32-bit BMP files with top-down rows, written by our own code with no imaging dependency.
