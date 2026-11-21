# ADR 0003 — Integer document scale for display scaling

- **Status**: Accepted
- **Date**: 2026-11-21
- **Context**: Windows display scaling may be 100 %, 125 %, 150 %, 175 %, 200 % or more (spec edge case). FR-015
  requires predictable nearest-neighbour pixel treatment and FR-022 pixel-identical output for identical inputs.
  Research R-08.

## Decision

`s = max(1, floor(dpi / 96 + 0.5))` — implemented once in `pxc::core::scaleForDpi` (`src/core/include/pxc/core/scale.hpp`)
and used by the shell at window creation (`GetDpiForWindow`) and on `WM_DPICHANGED`.

| Display scaling | DPI | Document scale |
|---|---|---|
| 100 % | 96 | 1 |
| 125 % | 120 | 1 |
| 150 % | 144 | 2 |
| 175 % | 168 | 2 |
| 200 % | 192 | 2 |
| 250 % | 240 | 3 |

Each glyph cell becomes an `s × s` block. The process is Per-Monitor-V2 DPI aware (`app.manifest`). Application chrome
(menus, toolbar, status bar) is scaled by Windows as usual.

## Consequences

- Glyphs stay crisp and are bit-identical at a given scale; the scale-2 golden is verified to be an exact 2×2
  replication of the scale-1 golden (`tests/visual/test_scale.cpp`).
- At 125 % and 175 % the document text is slightly smaller than "native" chrome text — an accepted trade-off for
  crispness and determinism.
- Pixel-exact tests use the reference scale 1; scale-2 goldens cover replication.

## Alternatives considered

- **Fractional scaling with filtering**: blurry pixel art and non-deterministic across machines; rejected.
- **User zoom**: not in scope for v1; the same integer policy would apply if added.
