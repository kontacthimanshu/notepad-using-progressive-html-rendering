// Read-only HTML preview mode (FR-038, US5): .html/.htm files are parsed into the restricted document model,
// styled, laid out and painted with the same FontRegistry / RasterCache / Renderer as the editor (FR-037).
// Selection and copy work; editing keys do nothing; scripts never run and nothing is fetched (FR-040).
#include "app.hpp"

#include <algorithm>

#include "pxc/core/unicode.hpp"
#include "pxc/update/preview_painter.hpp"
#include "resource.h"

namespace pxc::shell {

namespace {

update::PreviewPaintState paintState(const App& a) {
    update::PreviewPaintState st;
    st.scrollY = a.preview->scrollY;
    st.selStart = std::min(a.preview->anchor, a.preview->caret);
    st.selEnd = std::max(a.preview->anchor, a.preview->caret);
    st.scale = a.scale;
    st.bannerPack = a.editor->state().activePackId;
    if (a.preview->doc.hasErrors()) st.banner = U"This document has HTML errors - see Help > Diagnostics";
    return st;
}

int contentHeight(const App& a) {
    return a.preview->layout.height + update::previewBannerHeight(paintState(a), *a.registry);
}

void clampScroll(App& a) {
    const int maxY = std::max(0, contentHeight(a) - a.viewport.clientHeight);
    a.preview->scrollY = std::clamp(a.preview->scrollY, 0, maxY);
}

} // namespace

bool isHtmlPath(const std::filesystem::path& p) {
    std::wstring ext = p.extension().wstring();
    for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));
    return ext == L".html" || ext == L".htm";
}

namespace {
// Parses `text` and shows it as the read-only rendered preview. The editor document keeps the HTML source.
void showPreview(const std::u32string& text, int scrollY) {
    auto& a = app();
    auto p = std::make_unique<PreviewState>();
    auto parsed = html::parseHtml(text);
    p->doc = std::move(*parsed);
    p->scrollY = scrollY;
    for (const auto& d : p->doc.diagnostics) a.log.add(d);
    a.preview = std::move(p);
    a.editor->setReadOnly(true);
    a.editor->setMode(editor::EditorMode::PreviewHtml);
    rebuildPreview(); // clamps scrollY to the new content height
}
} // namespace

bool openPreview(const std::filesystem::path& path, const std::u32string& text) {
    auto& a = app();
    a.lastPreviewScrollY = 0;
    if (!isHtmlPath(path)) {
        a.preview.reset();
        return false;
    }
    showPreview(text, 0);
    return true;
}

bool canToggleSource() {
    auto& a = app();
    return a.preview != nullptr || (a.workflow && a.workflow->isHtmlFile());
}

bool sourceViewShown() {
    auto& a = app();
    return !a.preview && a.workflow && a.workflow->isHtmlFile();
}

// View -> Page Source (Ctrl+U): switches an HTML file between the rendered preview and its editable source.
// The source is the editor document itself, so unsaved edits are rendered when switching back, kept by the
// modified flag and written by Save. Both views draw with the same CSS glyphs.
void toggleSource() {
    auto& a = app();
    if (!canToggleSource()) return;
    if (a.preview) {
        a.lastPreviewScrollY = a.preview->scrollY;
        a.preview.reset();
        a.editor->setReadOnly(false);
        a.editor->setMode(editor::EditorMode::EditPlainText);
        a.workflow->setPreviewShown(false);
        a.viewport.clampScroll(*a.layout);
        a.updates->invalidateAll();
    } else {
        a.mouseSelecting = false;
        showPreview(a.doc.text(), a.lastPreviewScrollY);
        a.workflow->setPreviewShown(true);
    }
    a.afterChange();
}

void closePreview() {
    auto& a = app();
    if (!a.preview) return;
    a.preview.reset();
    a.editor->setReadOnly(false);
    a.editor->setMode(editor::EditorMode::EditPlainText);
    a.updates->invalidateAll();
}

void rebuildPreview() {
    auto& a = app();
    if (!a.preview) return;
    // Restyle with the toolbar's pack: elements without a resolved font-family follow it.
    html::computeStyles(a.preview->doc, *a.registry, a.editor->state().activePackId, a.log);
    a.preview->layout = layout::layoutBlocks(html::toStyledBlocks(a.preview->doc), *a.registry,
                                             std::max(1, a.viewport.clientWidth), a.scale); // wrap is always on
    clampScroll(a);
    if (a.surface) InvalidateRect(a.surface, nullptr, FALSE);
}

void paintPreviewFrame(std::vector<core::Rect>& rects) {
    auto& a = app();
    if (!a.preview || a.fb.empty()) return;
    render::Renderer r;
    update::paintPreview(a.fb, a.fb.bounds(), a.preview->layout, paintState(a), *a.registry, *a.cache, r,
                         a.updates->theme());
    rects.push_back(a.fb.bounds());
}

void previewScroll(int dy) {
    auto& a = app();
    if (!a.preview) return;
    a.preview->scrollY += dy;
    clampScroll(a);
    a.updateScrollbars();
    InvalidateRect(a.surface, nullptr, FALSE);
}

bool previewKey(WPARAM vk, bool ctrl) {
    auto& a = app();
    if (!a.preview) return false;
    const int row = a.layout->rowHeight();
    switch (vk) {
    case VK_UP: previewScroll(-row); break;
    case VK_DOWN: previewScroll(row); break;
    case VK_PRIOR: previewScroll(-a.viewport.clientHeight + row); break;
    case VK_NEXT: previewScroll(a.viewport.clientHeight - row); break;
    case VK_HOME: if (ctrl) previewScroll(-(1 << 28)); break;
    case VK_END: if (ctrl) previewScroll(1 << 28); break;
    default: break; // editing keys do nothing in the read-only preview
    }
    return true;
}

void previewMouse(int x, int y, bool down, bool drag, bool extend) {
    auto& a = app();
    if (!a.preview) return;
    const int banner = update::previewBannerHeight(paintState(a), *a.registry);
    const std::size_t off = layout::previewHitTest(a.preview->layout, x, y + a.preview->scrollY - banner);
    if (down) {
        a.preview->caret = off;
        if (!extend) a.preview->anchor = off;
    } else if (drag) {
        a.preview->caret = off;
    }
    InvalidateRect(a.surface, nullptr, FALSE);
}

bool previewCommand(int id) {
    auto& a = app();
    if (!a.preview) return false;
    switch (id) {
    case IDM_SELECTALL:
        a.preview->anchor = 0;
        a.preview->caret = a.preview->layout.text.size();
        InvalidateRect(a.surface, nullptr, FALSE);
        return true;
    case IDM_COPY: {
        const std::size_t s = std::min(a.preview->anchor, a.preview->caret);
        const std::size_t e = std::max(a.preview->anchor, a.preview->caret);
        if (e > s) a.clipboard.setText(a.preview->layout.text.substr(s, e - s));
        return true;
    }
    case IDM_UNDO: case IDM_REDO: case IDM_CUT: case IDM_PASTE: case IDM_DELETE: case IDM_WORDWRAP:
        return true; // read-only preview: wrap is always on and nothing can be edited (Save writes the source)
    default: return false;
    }
}

void previewScrollbars() {
    auto& a = app();
    SCROLLINFO si{sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL};
    si.nMax = contentHeight(a);
    si.nPage = static_cast<UINT>(std::max(0, a.viewport.clientHeight));
    si.nPos = a.preview->scrollY;
    SetScrollInfo(a.surface, SB_VERT, &si, TRUE);
    ShowScrollBar(a.surface, SB_HORZ, FALSE);
}

std::wstring previewTitle() {
    auto& a = app();
    std::wstring name = a.preview->doc.title.empty() ? a.workflow->documentName() : core::toWide(a.preview->doc.title);
    return L"PixelCSS — " + name + (a.doc.modified() ? L"*" : L"") + L" — Preview (read-only)";
}

} // namespace pxc::shell
