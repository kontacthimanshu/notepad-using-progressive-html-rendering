#pragma once
// Incremental update manager: turns edits / caret moves / scrolls into dirty regions and repaints only
// those regions of the framebuffer (research R-13).

#include <cstddef>
#include <optional>

#include "pxc/document/text_document.hpp"
#include "pxc/fonts/raster_cache.hpp"
#include "pxc/layout/layout_engine.hpp"
#include "pxc/layout/viewport.hpp"
#include "pxc/render/framebuffer.hpp"
#include "pxc/render/renderer.hpp"
#include "pxc/update/dirty_region.hpp"
#include "pxc/update/render_stats.hpp"

namespace pxc::update {

struct Theme {
    core::Color background{255, 255, 255, 255};
    core::Color text{0, 0, 0, 255};
    core::Color selectionFill{0x33, 0x66, 0xCC, 255};
    core::Color selectionText{255, 255, 255, 255};
    core::Color caret{0, 0, 0, 255};
};

// What the painter needs to know about the editor state.
struct PaintState {
    std::size_t caret = 0;
    std::size_t selStart = 0;
    std::size_t selEnd = 0;
    bool caretVisible = true;
};

class UpdateManager {
public:
    UpdateManager(const document::TextDocument& doc, layout::LayoutEngine& layout, fonts::RasterCache& cache,
                  layout::Viewport& viewport);

    void setTheme(const Theme& t) { theme_ = t; dirty_.markFull(); }
    const Theme& theme() const noexcept { return theme_; }

    // Notifications.
    // `rows` is the changed visual-row range of e.firstLine returned by LayoutEngine::onEdit.
    void onEdit(const document::EditResult& e, std::pair<std::size_t, std::size_t> rows, const PaintState& before,
                const PaintState& after);
    void onCaretMoved(const PaintState& before, const PaintState& after);
    void onFontChanged();
    void onWrapToggled();
    void onDpiChanged(int scale);
    void onResize(int w, int h);
    void onScroll(std::int64_t oldScrollY, int oldScrollX);
    void onDocumentReplaced();
    void invalidateAll() { dirty_.markFull(); }
    void invalidateRect(core::Rect r) { dirty_.add(r); }

    const DirtyRegion& dirty() const noexcept { return dirty_; }
    bool needsPaint() const noexcept { return !dirty_.empty() || pendingScrollDy_ != 0; }

    // Re-lays out and rasterizes the dirty part of the viewport into `fb`, then clears the dirty region.
    // `presented` receives the rectangles that changed (for the presenter).
    RenderStats paint(render::Framebuffer& fb, const PaintState& state, IRenderObserver* observer,
                      std::vector<core::Rect>* presented = nullptr);

    // Rows (viewport-relative rectangles) covering [from, to) offsets of the document, clipped to the view.
    void addRangeRects(std::size_t from, std::size_t to);
    core::Rect caretRect(std::size_t offset);

    render::Renderer& renderer() noexcept { return renderer_; }

private:
    void paintRect(render::Framebuffer& fb, core::Rect clip, const PaintState& state);
    void addRowsFrom(std::size_t line, std::size_t row);

    const document::TextDocument* doc_;
    layout::LayoutEngine* layout_;
    fonts::RasterCache* cache_;
    layout::Viewport* viewport_;
    render::Renderer renderer_;
    DirtyRegion dirty_;
    Theme theme_;
    std::uint64_t frame_ = 0;
    std::optional<PaintState> lastPainted_;
    std::int64_t pendingScrollDy_ = 0;
    void addSelectionDelta(const PaintState& before, const PaintState& after);
};

} // namespace pxc::update
