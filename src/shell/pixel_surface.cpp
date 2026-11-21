// PixelCSSSurface: the custom document surface. All document text is drawn by the PixelCSS engine into an
// owned framebuffer and presented with BitBlt; no EDIT/RichEdit control or OS text API is involved (FR-033).
#include "app.hpp"

#include <windowsx.h>

#include <algorithm>

#include "resource.h"

namespace pxc::shell {

namespace {
constexpr wchar_t kSurfaceClass[] = L"PixelCSS.Surface";
constexpr UINT_PTR kCaretTimer = 1;
constexpr UINT kCaretBlinkMs = 530;
char16_t g_pendingHighSurrogate = 0;

using namespace pxc::editor;

} // namespace

void handleKeyDown(WPARAM vk, bool shift, bool ctrl) {
    auto& a = app();
    if (a.preview) {
        previewKey(vk, ctrl);
        return;
    }
    auto move = [&](MoveKind k) { a.editor->handle(cmd::Move{k, shift}); };
    switch (vk) {
    case VK_LEFT: move(ctrl ? MoveKind::WordLeft : MoveKind::Left); break;
    case VK_RIGHT: move(ctrl ? MoveKind::WordRight : MoveKind::Right); break;
    case VK_UP: move(MoveKind::Up); break;
    case VK_DOWN: move(MoveKind::Down); break;
    case VK_HOME: move(ctrl ? MoveKind::DocStart : MoveKind::Home); break;
    case VK_END: move(ctrl ? MoveKind::DocEnd : MoveKind::End); break;
    case VK_PRIOR: move(MoveKind::PageUp); break;
    case VK_NEXT: move(MoveKind::PageDown); break;
    case VK_DELETE:
        if (shift) a.editor->handle(cmd::Cut{});
        else a.editor->handle(cmd::Delete{});
        break;
    case VK_BACK: a.editor->handle(cmd::Backspace{}); break;
    case VK_INSERT:
        if (ctrl) a.editor->handle(cmd::Copy{});
        else if (shift) a.editor->handle(cmd::Paste{});
        else return;
        break;
    default: return;
    }
    a.afterChange();
}

namespace {

void stampInput() {
    auto& a = app();
    if (a.pendingInputQpc == 0) a.pendingInputQpc = qpcNow();
}

void handleChar(WPARAM ch) {
    auto& a = app();
    if (a.preview) return; // read-only preview
    const auto u = static_cast<char16_t>(ch);
    if (u >= 0xD800 && u <= 0xDBFF) {
        g_pendingHighSurrogate = u;
        return;
    }
    char32_t cp = u;
    if (u >= 0xDC00 && u <= 0xDFFF) {
        if (!g_pendingHighSurrogate) return;
        cp = 0x10000 + ((static_cast<char32_t>(g_pendingHighSurrogate) - 0xD800) << 10) + (u - 0xDC00);
        g_pendingHighSurrogate = 0;
    }
    if (cp == U'\r') a.editor->handle(cmd::Enter{});
    else if (cp == U'\t') a.editor->handle(cmd::Type{U"\t"});
    else if (cp < 0x20 || cp == 0x7F) return; // control characters (Ctrl+letter, Backspace) are handled elsewhere
    else a.editor->handle(cmd::Type{std::u32string(1, cp)});
    a.afterChange();
}

} // namespace

void scrollRows(int rows) {
    auto& a = app();
    if (a.preview) {
        previewScroll(rows * a.layout->rowHeight());
        return;
    }
    const std::int64_t oldY = a.viewport.scrollY(*a.layout);
    const int oldX = a.viewport.scrollX;
    a.viewport.scrollBy(*a.layout, static_cast<std::int64_t>(rows) * a.layout->rowHeight());
    a.updates->onScroll(oldY, oldX);
    a.updateScrollbars();
    a.requestPaint();
}

namespace {

void scrollHorizontal(int px) {
    auto& a = app();
    if (a.editor->state().wordWrap) return;
    const std::int64_t oldY = a.viewport.scrollY(*a.layout);
    const int oldX = a.viewport.scrollX;
    const int maxX = std::max(0, a.layout->contentWidth() + 2 * a.viewport.marginLeft - a.viewport.clientWidth);
    a.viewport.scrollX = std::clamp(a.viewport.scrollX + px, 0, maxX);
    a.updates->onScroll(oldY, oldX);
    a.updateScrollbars();
    a.requestPaint();
}

void onScrollBar(HWND hwnd, int bar, WPARAM wp) {
    auto& a = app();
    SCROLLINFO si{sizeof(si), SIF_ALL};
    GetScrollInfo(hwnd, bar, &si);
    const int line = bar == SB_VERT ? a.layout->rowHeight() : a.layout->spaceAdvancePx() * 4;
    int pos = si.nPos;
    switch (LOWORD(wp)) {
    case SB_LINEUP: pos -= line; break;
    case SB_LINEDOWN: pos += line; break;
    case SB_PAGEUP: pos -= static_cast<int>(si.nPage); break;
    case SB_PAGEDOWN: pos += static_cast<int>(si.nPage); break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: pos = si.nTrackPos; break;
    case SB_TOP: pos = si.nMin; break;
    case SB_BOTTOM: pos = si.nMax; break;
    default: return;
    }
    if (a.preview) {
        if (bar == SB_VERT) previewScroll(pos - si.nPos);
        return;
    }
    if (bar == SB_VERT) {
        const std::int64_t oldY = a.viewport.scrollY(*a.layout);
        a.viewport.setScrollY(*a.layout, pos);
        a.updates->onScroll(oldY, a.viewport.scrollX);
        a.updateScrollbars();
        a.requestPaint();
    } else {
        scrollHorizontal(pos - a.viewport.scrollX);
    }
}

void onResize(HWND hwnd, int w, int h) {
    auto& a = app();
    if (w <= 0 || h <= 0) return;
    (void)a.fb.resize(w, h, a.updates->theme().background);
    a.viewport.clientWidth = w;
    a.viewport.clientHeight = h;
    HDC dc = GetDC(hwnd);
    a.presenter.resize(dc, w, h);
    ReleaseDC(hwnd, dc);
    a.editor->applyLayoutParams(a.scale);
    a.updates->onResize(w, h);
    if (a.preview) rebuildPreview();
    a.viewport.clampScroll(*a.layout);
    a.updateScrollbars();
    InvalidateRect(hwnd, nullptr, FALSE);
}

void onPaint(HWND hwnd) {
    auto& a = app();
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    std::vector<core::Rect> rects;
    update::RenderStats st;
    bool painted = false;
    if (a.preview) {
        paintPreviewFrame(rects);
        a.presenter.upload(a.fb, rects);
        a.presenter.blit(dc, ps.rcPaint);
        for (const auto& r : rects) {
            const RECT wr{r.x, r.y, r.right(), r.bottom()};
            a.presenter.blit(dc, wr);
        }
        EndPaint(hwnd, &ps);
        a.onPresented(st, true);
        return;
    }
    if (a.overlayOn) a.updates->invalidateRect(a.overlayRect()); // repaint the document under the overlay
    if (a.updates->needsPaint() && !a.fb.empty()) {
        auto ps2 = a.editor->paintState();
        ps2.caretVisible = a.caretOn && GetFocus() == hwnd;
        st = a.updates->paint(a.fb, ps2, nullptr, &rects);
        a.drawOverlay(a.fb, rects, st);
        a.presenter.upload(a.fb, rects);
        painted = true;
    }
    a.presenter.blit(dc, ps.rcPaint);
    for (const auto& r : rects) {
        const RECT wr{r.x, r.y, r.right(), r.bottom()};
        a.presenter.blit(dc, wr);
    }
    EndPaint(hwnd, &ps);
    a.onPresented(st, painted);
}

LRESULT CALLBACK SurfaceWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto& a = app();
    switch (msg) {
    case WM_CREATE: return 0;
    case WM_ERASEBKGND: return 1; // the framebuffer covers everything
    case WM_SIZE: onResize(hwnd, LOWORD(lp), HIWORD(lp)); return 0;
    case WM_PAINT: onPaint(hwnd); return 0;
    case WM_SETFOCUS:
        a.resetCaretBlink();
        a.requestPaint();
        return 0;
    case WM_KILLFOCUS:
        KillTimer(hwnd, kCaretTimer);
        a.caretOn = false;
        a.updates->invalidateRect(a.updates->caretRect(a.editor->state().caret));
        a.requestPaint();
        return 0;
    case WM_TIMER:
        if (wp == kCaretTimer) {
            a.caretOn = !a.caretOn;
            a.updates->invalidateRect(a.updates->caretRect(a.editor->state().caret));
            a.requestPaint();
        }
        return 0;
    case WM_KEYDOWN:
        stampInput();
        handleKeyDown(wp, GetKeyState(VK_SHIFT) < 0, GetKeyState(VK_CONTROL) < 0);
        return 0;
    case WM_CHAR:
        stampInput();
        handleChar(wp);
        return 0;
    case WM_APP_BENCH_KEY: // bench-script key injection: wParam = VK, lParam bit0 = shift, bit1 = ctrl
        handleKeyDown(wp, (lp & 1) != 0, (lp & 2) != 0);
        return 0;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd);
        SetCapture(hwnd);
        a.mouseSelecting = true;
        if (a.preview) {
            previewMouse(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), true, false, (wp & MK_SHIFT) != 0);
            return 0;
        }
        a.editor->handle(cmd::Click{GET_X_LPARAM(lp), GET_Y_LPARAM(lp), (wp & MK_SHIFT) != 0});
        a.afterChange();
        return 0;
    case WM_MOUSEMOVE:
        if (a.mouseSelecting && (wp & MK_LBUTTON)) {
            if (a.preview) {
                previewMouse(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), false, true, false);
                return 0;
            }
            a.editor->handle(cmd::Drag{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
            a.afterChange();
        }
        return 0;
    case WM_LBUTTONUP:
        a.mouseSelecting = false;
        ReleaseCapture();
        return 0;
    case WM_LBUTTONDBLCLK:
        if (a.preview) return 0;
        a.editor->handle(cmd::SelectWordAt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
        a.afterChange();
        return 0;
    case WM_MOUSEWHEEL: {
        const int delta = GET_WHEEL_DELTA_WPARAM(wp);
        if (GET_KEYSTATE_WPARAM(wp) & MK_SHIFT) scrollHorizontal(-delta / WHEEL_DELTA * a.layout->spaceAdvancePx() * 8);
        else scrollRows(-delta * 3 / WHEEL_DELTA);
        return 0;
    }
    case WM_MOUSEHWHEEL:
        scrollHorizontal(GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA * a.layout->spaceAdvancePx() * 8);
        return 0;
    case WM_VSCROLL: onScrollBar(hwnd, SB_VERT, wp); return 0;
    case WM_HSCROLL: onScrollBar(hwnd, SB_HORZ, wp); return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
            return TRUE;
        }
        break;
    case WM_GETDLGCODE: return DLGC_WANTALLKEYS | DLGC_WANTCHARS | DLGC_WANTTAB;
    default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

bool registerSurfaceClass(HINSTANCE inst) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = SurfaceWndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
    wc.lpszClassName = kSurfaceClass;
    return RegisterClassExW(&wc) != 0;
}

HWND createSurface(HWND parent) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, kSurfaceClass, nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
                           0, 0, 100, 100, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SURFACE)),
                           app().instance, nullptr);
}

void App::resetCaretBlink() {
    caretOn = true;
    if (surface && GetFocus() == surface) SetTimer(surface, kCaretTimer, kCaretBlinkMs, nullptr);
    if (updates && editor) updates->invalidateRect(updates->caretRect(editor->state().caret));
}

void App::requestPaint() {
    if (surface && preview) {
        InvalidateRect(surface, nullptr, FALSE);
        return;
    }
    if (!surface || !updates || !updates->needsPaint()) return;
    const auto& d = updates->dirty();
    if (d.full()) {
        InvalidateRect(surface, nullptr, FALSE);
        return;
    }
    for (const auto& r : d.rects()) {
        const RECT wr{r.x, r.y, r.right(), r.bottom()};
        InvalidateRect(surface, &wr, FALSE);
    }
}

void App::updateScrollbars() {
    if (!surface || !layout) return;
    if (preview) {
        previewScrollbars();
        return;
    }
    SCROLLINFO si{sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL};
    si.nMin = 0;
    si.nMax = static_cast<int>(std::min<std::int64_t>(layout->totalHeight(), INT_MAX - 1));
    si.nPage = static_cast<UINT>(std::max(0, viewport.clientHeight));
    si.nPos = static_cast<int>(viewport.scrollY(*layout));
    SetScrollInfo(surface, SB_VERT, &si, TRUE);
    const bool wrap = editor && editor->state().wordWrap;
    ShowScrollBar(surface, SB_HORZ, !wrap);
    if (!wrap) {
        SCROLLINFO hs{sizeof(hs), SIF_RANGE | SIF_PAGE | SIF_POS};
        hs.nMax = layout->contentWidth() + 2 * viewport.marginLeft;
        hs.nPage = static_cast<UINT>(std::max(0, viewport.clientWidth));
        hs.nPos = viewport.scrollX;
        SetScrollInfo(surface, SB_HORZ, &hs, TRUE);
    }
}

// ---- GdiPresenter ----

GdiPresenter::~GdiPresenter() { release(); }

void GdiPresenter::release() {
    if (memDC_) {
        if (oldBitmap_) SelectObject(memDC_, oldBitmap_);
        DeleteDC(memDC_);
        memDC_ = nullptr;
    }
    if (bitmap_) {
        DeleteObject(bitmap_);
        bitmap_ = nullptr;
    }
    oldBitmap_ = nullptr;
    bits_ = nullptr;
    w_ = h_ = 0;
}

void GdiPresenter::resize(HDC reference, int w, int h) {
    if (w == w_ && h == h_ && bitmap_) return;
    release();
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // top-down, matching the framebuffer
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    bitmap_ = CreateDIBSection(reference, &bi, DIB_RGB_COLORS, &bits_, nullptr, 0);
    if (!bitmap_) return;
    memDC_ = CreateCompatibleDC(reference);
    oldBitmap_ = SelectObject(memDC_, bitmap_);
    w_ = w;
    h_ = h;
}

void GdiPresenter::upload(const render::Framebuffer& fb, const std::vector<core::Rect>& rects) {
    if (!bits_ || fb.width() != w_ || fb.height() != h_) return;
    GdiFlush();
    auto* dst = static_cast<std::uint32_t*>(bits_);
    for (const auto& r0 : rects) {
        const core::Rect r = r0.intersect(fb.bounds());
        for (int y = r.y; y < r.bottom(); ++y) {
            const auto src = fb.row(y);
            std::copy(src.begin() + r.x, src.begin() + r.right(), dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(w_) + r.x);
        }
    }
}

void GdiPresenter::blit(HDC dst, const RECT& r) const {
    if (!memDC_ || r.right <= r.left || r.bottom <= r.top) return;
    BitBlt(dst, r.left, r.top, r.right - r.left, r.bottom - r.top, memDC_, r.left, r.top, SRCCOPY);
}

} // namespace pxc::shell
