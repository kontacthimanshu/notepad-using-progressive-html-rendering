# ADR 0002 — Presentation: owned framebuffer + DIB section + BitBlt of dirty rectangles

- **Status**: Accepted
- **Date**: 2026-11-21
- **Context**: FR-014 requires an internally owned raster surface and no OS font APIs for document text;
  Constitution VII asks for a CPU backend first, with hardware acceleration only behind a separate abstraction.
  Research R-09.

## Decision

- `pxc::render::Framebuffer` owns 32-bit **straight-alpha BGRA** pixels (top-down rows). Glyph cells are composited
  with integer source-over: `out = (s·a + d·(255−a) + 127) / 255` — bit-exact on every CPU (FR-022).
- The shell's `GdiPresenter` keeps a top-down 32-bpp **DIB section** of the same size. After each frame only the dirty
  rectangles are copied into the DIB (`upload`) and blitted to the window with `BitBlt` inside `WM_PAINT`.
- `BitBlt` is a pixel copy; no text API (`DrawText*`, `TextOut*`, `ExtTextOut*`, DirectWrite) is ever called for
  document text. `tests/integration/test_binary_imports.cpp` parses the PE import table and fails if any of those
  functions — or WinHTTP/WinINet/Winsock/URLMon/WebView2/DirectWrite/Direct2D DLLs — are imported.

## Consequences

- Scrolling moves already-rendered rows inside the framebuffer and repaints only the exposed band; a keystroke
  repaints one row segment. Measured end-to-end keystroke-to-present p95: 1.7 ms (Release, reference machine).
- Direct2D composition remains possible later behind the same upload/blit seam without touching the rasterizer.
- Device loss handling is unnecessary (GDI DIBs live in system memory).

## Alternatives considered

- **Direct2D / DXGI swap chain**: allowed by the constitution but adds device-loss handling and a GPU dependency for
  no measurable benefit at these frame sizes; deferred.
- **Premultiplied alpha**: marginally faster blending, but straight alpha keeps golden comparison and cell literals
  simpler; the blend count is low.
