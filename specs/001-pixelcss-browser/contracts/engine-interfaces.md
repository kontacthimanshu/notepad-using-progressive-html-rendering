# Contract: Engine Module Interfaces (C++20)

**Satisfies**: Constitution IX (explicit, testable interfaces with no cycles), FR-014, FR-023, FR-045.

These are the **public headers** each static library exposes. Signatures are normative in shape. Exact parameter types may be refined during implementation if the contract tests still hold. Namespaces are `pxc::<module>`.

## Dependency graph (acyclic; arrows mean "depends on")

```text
shell ──> editor ──> update ──> layout ──> fonts ──> glyphcss ──> core
  │         │          │          │          └──────> render  ──> core
  │         │          └──> render│
  │         │                     └──> document ──> core
  │         ├──> document
  │         └──> storage ──> document
  └──> html ──> presentcss ──> core
render depends ONLY on core: it knows pixels, rectangles and RasterGlyph, never glyph CSS, text or windows.
fonts owns rasterize(GlyphDefinition → RasterGlyph), RasterCache and renderTextLine.
render, glyphcss, presentcss, html, document, storage (portable part): no Win32 headers.
Only `shell`, `storage/win32` and `tools/*/main.cpp` include <windows.h>.
```

`core` provides `Expected<T,E>` (wrapping `std::expected` when available, otherwise a polyfill), `Diagnostic`, `Rect`, `Color`, a `Utf8/Utf16 ↔ u32` codec, and a monotonic `Clock` interface (injectable for tests).

## core

```cpp
struct Diagnostic { Severity severity; std::string code, message; std::optional<std::filesystem::path> file; int line = 0, column = 0; };
class DiagnosticsLog { public: void add(Diagnostic); std::span<const Diagnostic> all() const; void clear(); };
struct Rect { int x, y, w, h; };   struct Color { uint8_t r, g, b, a; };
```

## glyphcss

```cpp
Expected<GlyphDefinition, std::vector<Diagnostic>> parseGlyph(std::string_view cssText, const std::filesystem::path& sourceForDiagnostics);
Expected<FontManifest,   std::vector<Diagnostic>> parseManifest(std::string_view cssText, const std::filesystem::path& source);
```

These are pure functions with no filesystem access. Callers pass the text in.

## fonts

```cpp
class FontRegistry {
public:
  explicit FontRegistry(DiagnosticsLog&);
  void loadAll(const std::filesystem::path& fontsDir);           // discover + validate; never throws
  ReloadResult reload(std::string_view packId);                   // returns changed code points
  std::span<const FontPackInfo> packs() const;                     // id, displayName, state, authenticity, provenance
  const FontMetrics& metrics(std::string_view packId) const;      // lineHeight, ascent, descent, spaceAdvance
  ResolvedGlyph resolve(std::string_view packId, char32_t cp) const;   // R-07 fallback chain
};
RasterGlyph rasterize(const GlyphDefinition&, int scale, Color fg, Color bg);   // currentcolor→fg, backgroundcolor→bg, never pre-composites
class RasterCache {
public:
  RasterCache(const FontRegistry&, std::size_t budgetBytes = 16u << 20);
  const RasterGlyph& get(std::string_view packId, char32_t cp, int scale, Color fg, Color bg);  // key includes style (R-14)
  CacheStats stats() const;  void resetStats();  void evictStale(std::string_view packId);
};
Framebuffer renderTextLine(const FontRegistry&, RasterCache&, std::string_view packId,
                           std::u32string_view text, int scale, Color fg, Color bg);         // canonical canvas, research R-21
```

## render

```cpp
class Framebuffer { public: void resize(int w, int h); std::span<uint32_t> row(int y); int width() const; int height() const; };
class Renderer {
public:
  void fill(Framebuffer&, Rect, Color);
  void blitGlyph(Framebuffer&, const RasterGlyph&, int penX, int baselineY, Rect clip);  // source-over, integer blend
  void drawCaret(Framebuffer&, Rect, Color);
};
struct IPresenter { virtual void present(const Framebuffer&, std::span<const Rect> dirty) = 0; virtual ~IPresenter() = default; };
```

`render` has no knowledge of text, documents or windows (FR-045).

## document

```cpp
class TextDocument {
public:
  std::size_t length() const;  std::size_t lineCount() const;
  std::size_t lineStart(std::size_t line) const;  LineCol toLineCol(std::size_t offset) const;
  std::u32string text(std::size_t offset, std::size_t count) const;
  EditResult insert(std::size_t offset, std::u32string_view);
  EditResult erase(std::size_t offset, std::size_t count);
  bool undo(EditorSnapshot*); bool redo(EditorSnapshot*);
  uint64_t revision() const;  uint64_t lineRevision(std::size_t line) const;
  bool modified() const;  void markSaved();
};
struct EditResult { std::size_t offset, removed, inserted; std::size_t firstLine, oldLineCount, newLineCount; };
```

## storage

```cpp
Expected<LoadedText, FileError> loadTextFile(const std::filesystem::path&, InvalidUtf8Policy);
Expected<void, FileError>       saveTextFileAtomic(const std::filesystem::path&, const TextDocument&, const FileInfo&);
// FileError { kind: NotFound|AccessDenied|SharingViolation|DiskFull|InvalidEncoding|Other; std::wstring userMessage; }
```

```cpp
struct IFileSystem {
  virtual Expected<std::string, FileError> readAll(const std::filesystem::path&) = 0;
  virtual Expected<void, FileError>        writeAtomic(const std::filesystem::path&, std::string_view bytes) = 0;
  virtual ~IFileSystem() = default;
};
struct Preferences { std::string lastPackId = "verdana-style"; bool wordWrap = true; };
struct IPreferencesStore {                       // research R-20; Win32 impl: HKCU\Software\PixelCSS\Display
  virtual Preferences load(DiagnosticsLog&) = 0;
  virtual bool save(const Preferences&) = 0;      // false on failure, never throws
  virtual ~IPreferencesStore() = default;
};
struct NullPreferencesStore final : IPreferencesStore {   // production; used for --no-preferences
  Preferences load(DiagnosticsLog&) override { return {}; }   // defaults: verdana-style, wrap on
  bool save(const Preferences&) override { return true; }     // no-op
};
```

Windows file APIs and the registry are reached through these seams, implemented in `storage/win32`, so tests can inject failures.

## layout

```cpp
class LayoutEngine {
public:
  LayoutEngine(const FontRegistry&);
  const LayoutLine& layoutLine(const TextDocument&, std::size_t line, const LayoutParams&);  // cached by key
  void invalidateLines(std::size_t first, std::size_t count);  void invalidateAll();
  HitTest hitTest(const Viewport&, int x, int y) const;  CaretGeometry caretAt(std::size_t offset) const;
  bool layoutIdleSlice(std::chrono::microseconds budget);  // progressive background layout; false when done
};
```

## update (Incremental Update Manager)

```cpp
class UpdateManager {
public:
  void onEdit(const EditResult&);  void onFontChanged();  void onResize(int w, int h);  void onScroll(const Viewport&);
  void onWrapToggled();  void onDpiChanged(int scale);  void onCaretMoved(std::size_t oldOffset, std::size_t newOffset);
  const DirtyRegion& dirty() const;
  void paint(Framebuffer&, const Viewport&, IRenderObserver*);   // relayout + rasterize dirty only
};
struct IRenderObserver { virtual void onFrame(const RenderStats&) = 0; virtual ~IRenderObserver() = default; };
```

## editor

```cpp
class EditorController {
public:
  void handle(const EditorCommand&);   // Type, Enter, Backspace, Delete, Move{dir, extend}, Home, End, SelectAll, Undo, Redo, Copy, Cut, Paste, SetFont, ToggleWrap
  const EditorState& state() const;
};
struct IClipboard { virtual std::optional<std::u32string> getText() = 0; virtual bool setText(std::u32string_view) = 0; virtual ~IClipboard() = default; };
```

```cpp
enum class PromptChoice { Save, DontSave, Cancel };
enum class InvalidEncodingChoice { OpenWithReplacement, Cancel };
struct IPromptService {                          // shell implements with TaskDialogIndirect
  virtual PromptChoice askSaveChanges(std::wstring_view documentName) = 0;
  virtual InvalidEncodingChoice askInvalidEncoding(std::wstring_view fileName) = 0;
  virtual bool confirmConvertToUtf8(std::wstring_view fileName) = 0;
  virtual void showError(std::wstring_view message) = 0;
  virtual ~IPromptService() = default;
};
```

The Win32 `IClipboard` implementation uses `CF_UNICODETEXT` (FR-030).

**Test doubles** live in `tests/support/`: `FakeClipboard`, `FakeFileSystem` (failure injection per path and operation), `FakePromptService` (scripted answers, records calls), `MemoryPreferencesStore` and `ManualClock`. They implement exactly the interfaces above.

## html / presentcss

```cpp
Expected<PreviewDocument, std::vector<Diagnostic>> parseHtml(std::u32string_view source);   // always returns a tree unless input is empty; diagnostics attached
StyleSheet parsePresentationCss(std::u32string_view, DiagnosticsLog&);
void computeStyles(PreviewDocument&, std::span<const StyleSheet>, const FontRegistry&);
```

## Contract tests

`tests/contract/` holds one test file per module. Each one exercises only that module's public header, which enforces that modules are independently testable (Constitution "Parsing and rendering remain independently testable").
