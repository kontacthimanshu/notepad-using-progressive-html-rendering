# ADR 0001 — Text buffer: piece table over `char32_t`

- **Status**: Accepted (ratified by measurement)
- **Date**: 2026-11-21
- **Context**: Constitution VIII requires "a suitable mutable text-buffer structure, such as a piece table or rope,
  with the choice justified through measured requirements". Research R-10 proposed a piece table of `char32_t`
  code points with a Fenwick-tree line index and set the ratification bar at **p95 edit cost < 1 ms** at 100k and
  1M characters.

## Decision

Keep the piece table (`src/document/piece_table.*`): an immutable original buffer, an append-only add buffer, a
`std::vector` of pieces with adjacent-append merging, and a cached last-located piece. Lines are indexed by a
Fenwick tree of per-line lengths (`src/document/line_index.*`), updated incrementally per edit. Offsets everywhere
are code-point indices, so one offset is one glyph lookup and caret/selection offsets survive font switches
exactly (SC-004).

## Measurement

`pixelcss-bench` scenario `buffer-edit` — 2,000 random inserts/erases (2:1) on a generated document, Release
build, reference machine (see `docs/perf/reference-machine.md`):

| Document size | p95 edit cost | Budget |
|---|---|---|
| 100,000 characters | **2.7 µs** | < 1 ms |
| 1,000,000 characters | **4.0 µs** | < 1 ms |

Both are more than two orders of magnitude inside the budget, so the follow-up (a balanced-tree piece table behind
the same `TextDocument` interface) is not needed.

## Consequences

- Undo/redo store piece-level records (the removed and inserted text), not document snapshots.
- Memory is about 4 bytes per character plus pieces; a 10M-character document idles at ~67 MB in total
  (`idleMemoryMB10M`), well within the 256 MB budget.
- If a future workload (for example millions of scattered edits without coalescing) makes the piece vector long,
  the vector can be replaced by a balanced tree without changing any caller.

## Alternatives considered

- **Rope**: more complex; no measurable benefit at these sizes.
- **Gap buffer**: excellent for local edits but awkward for undo records and multiple anchors.
- **UTF-8 storage**: makes offset ↔ code-point mapping a constant source of bugs; rejected.
