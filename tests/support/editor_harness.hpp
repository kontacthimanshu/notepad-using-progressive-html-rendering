#pragma once
// Headless editor stack (registry, cache, layout, viewport, update manager, controller) for tests.

#include <memory>

#include "pxc/editor/editor_controller.hpp"
#include "pxc/fonts/font_registry.hpp"
#include "pxc/fonts/raster_cache.hpp"
#include "pxc/render/framebuffer.hpp"
#include "support/fakes.hpp"

namespace pxc::test {

struct EditorHarness {
    core::DiagnosticsLog log;
    fonts::FontRegistry registry{log};
    std::unique_ptr<fonts::RasterCache> cache;
    document::TextDocument doc;
    std::unique_ptr<layout::LayoutEngine> layout;
    layout::Viewport viewport;
    std::unique_ptr<update::UpdateManager> updates;
    FakeClipboard clipboard;
    std::unique_ptr<editor::EditorController> editor;
    render::Framebuffer fb;

    explicit EditorHarness(int width = 640, int height = 480, bool wrap = true, int scale = 1) {
        registry.loadAll(PXC_FONTS_DIR);
        cache = std::make_unique<fonts::RasterCache>(registry);
        layout = std::make_unique<layout::LayoutEngine>(registry);
        viewport.clientWidth = width;
        viewport.clientHeight = height;
        updates = std::make_unique<update::UpdateManager>(doc, *layout, *cache, viewport);
        editor = std::make_unique<editor::EditorController>(doc, *layout, viewport, *updates, clipboard);
        if (!wrap) editor->handle(editor::cmd::ToggleWrap{});
        editor->applyLayoutParams(scale);
        fb = render::Framebuffer(width, height);
        updates->onResize(width, height);
    }

    void type(std::u32string_view s) { editor->handle(editor::cmd::Type{std::u32string(s)}); }
    void move(editor::MoveKind k, bool extend = false) { editor->handle(editor::cmd::Move{k, extend}); }
    update::RenderStats paint(update::IRenderObserver* obs = nullptr) {
        return updates->paint(fb, editor->paintState(), obs);
    }
    std::size_t caret() const { return editor->state().caret; }
    std::size_t anchor() const { return editor->state().anchor; }
};

} // namespace pxc::test
