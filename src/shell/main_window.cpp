// Main window: menus, accelerators, toolbar, status bar and the custom PixelCSSSurface (FR-024).
#include "app.hpp"

#include <commctrl.h>

#include <algorithm>

#include "pxc/core/unicode.hpp"
#include "pxc/storage/win32_preferences.hpp"
#include "resource.h"

namespace pxc::shell {

namespace {
constexpr wchar_t kMainClass[] = L"PixelCSS.MainWindow";

std::filesystem::path exeDirectory() {
    wchar_t buf[MAX_PATH * 4];
    const DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
    return std::filesystem::path(std::wstring(buf, n)).parent_path();
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
} // namespace

void App::initEngine() {
    registry = std::make_unique<fonts::FontRegistry>(log);
    registry->loadAll(exeDirectory() / "fonts");
    if (options.fontsDir) registry->loadAll(*options.fontsDir);
    cache = std::make_unique<fonts::RasterCache>(*registry);
    layout = std::make_unique<layout::LayoutEngine>(*registry);
    viewport.clientWidth = 640;
    viewport.clientHeight = 480;
    updates = std::make_unique<update::UpdateManager>(doc, *layout, *cache, viewport);
    editor = std::make_unique<editor::EditorController>(doc, *layout, viewport, *updates, clipboard);
    workflow = std::make_unique<editor::FileWorkflow>(*editor, fileSystem, prompts, dialogs);
    workflow->onOpenHook = [](const std::filesystem::path& p, const std::u32string& text) { return openPreview(p, text); };
    recordSample("startup");
    // Display preferences (research R-20): loaded before the toolbar is populated.
    if (options.noPreferences) prefs = std::make_unique<storage::NullPreferencesStore>();
    else prefs = std::make_unique<storage::RegistryPreferencesStore>();
    const storage::Preferences pref = prefs->load(log);
    const std::string startPack =
        storage::resolveStartupPack(pref, [&](std::string_view id) { return registry->isUsable(id); });
    if (startPack != editor->state().activePackId) editor->handle(editor::cmd::SetFont{startPack});
    if (pref.wordWrap != editor->state().wordWrap) editor->handle(editor::cmd::ToggleWrap{});
    if (!registry->isUsable(editor->state().activePackId)) {
        // Pick the first usable pack so the editor still renders with CSS glyphs.
        for (const auto& p : registry->packs())
            if (p.state != fonts::PackState::Invalid) {
                editor->handle(editor::cmd::SetFont{p.id});
                break;
            }
    }
}

bool registerWindowClasses(HINSTANCE inst) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszMenuName = MAKEINTRESOURCEW(IDR_MAINMENU);
    wc.lpszClassName = kMainClass;
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APPICON));
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    if (!RegisterClassExW(&wc)) return false;
    return registerSurfaceClass(inst);
}

HWND createMainWindow(HINSTANCE inst) {
    auto& a = app();
    a.accel = LoadAcceleratorsW(inst, MAKEINTRESOURCEW(IDR_ACCEL));
    a.main = CreateWindowExW(0, kMainClass, L"PixelCSS", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT,
                             CW_USEDEFAULT, 960, 680, nullptr, nullptr, inst, nullptr);
    return a.main;
}

void App::layoutChildren() {
    if (!main) return;
    RECT rc;
    GetClientRect(main, &rc);
    int top = 0, bottom = rc.bottom;
    if (toolbar) {
        SendMessageW(toolbar, TB_AUTOSIZE, 0, 0);
        RECT tr;
        GetWindowRect(toolbar, &tr);
        top = tr.bottom - tr.top;
    }
    if (status) {
        SendMessageW(status, WM_SIZE, 0, 0);
        RECT sr;
        GetWindowRect(status, &sr);
        bottom -= sr.bottom - sr.top;
        const int w = rc.right;
        const int parts[] = {w * 22 / 100, w * 40 / 100, w * 52 / 100, w * 86 / 100, -1};
        SendMessageW(status, SB_SETPARTS, std::size(parts), reinterpret_cast<LPARAM>(parts));
    }
    if (surface) MoveWindow(surface, 0, top, rc.right, std::max(0, bottom - top), TRUE);
}

void App::updateTitle() {
    if (!main || !workflow) return;
    SetWindowTextW(main, preview ? previewTitle().c_str() : workflow->windowTitle().c_str());
}

void App::updateStatus() {
    if (!status || !editor) return;
    const auto lc = doc.toLineCol(editor->state().caret);
    const std::wstring pos = preview ? std::wstring(L"HTML preview (read-only)")
                                     : L"Ln " + std::to_wstring(lc.line + 1) + L", Col " + std::to_wstring(lc.col + 1);
    const auto& info = workflow->fileInfo();
    std::wstring font = L"(no font)";
    if (auto p = registry->pack(editor->state().activePackId)) {
        font = core::utf8ToWide(p->displayName);
        if (p->authenticity == "style-interpretation") font += L" (style)";
        if (p->state == fonts::PackState::Degraded) font += L" ⚠";
    }
    const std::size_t problems = log.errorCount() + log.warningCount();
    const std::wstring scaleText = std::to_wstring(scale * 100) + L"%" +
                                   (problems ? L"  |  " + std::to_wstring(problems) + L" font problems — Help → Diagnostics" : L"");
    SendMessageW(status, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(pos.c_str()));
    SendMessageW(status, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(core::utf8ToWide(storage::encodingName(info.encoding)).c_str()));
    SendMessageW(status, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(core::utf8ToWide(storage::eolName(info.eol)).c_str()));
    SendMessageW(status, SB_SETTEXTW, 3, reinterpret_cast<LPARAM>(font.c_str()));
    SendMessageW(status, SB_SETTEXTW, 4, reinterpret_cast<LPARAM>(scaleText.c_str()));
}

void App::afterChange() {
    updateTitle();
    updateStatus();
    updateScrollbars();
    resetCaretBlink();
    requestPaint();
}

bool App::idle() {
    if (!layout || !editor || preview) return false;
    // Progressive layout: measure remaining lines in <= 4 ms slices (research R-12).
    const std::int64_t before = layout->totalHeight();
    const bool more = layout->layoutIdleSlice(doc, std::chrono::milliseconds(4), viewport.anchor.line);
    if (layout->totalHeight() != before) {
        viewport.clampScroll(*layout);
        updateScrollbars();
    }
    return more;
}

void App::command(int id) {
    if (!editor) return;
    using namespace editor;
    if (id != IDM_NEW && id != IDM_OPEN && id != IDM_EXIT && previewCommand(id)) return;
    switch (id) {
    case IDM_NEW: fileNew(); return;
    case IDM_OPEN: fileOpen(); return;
    case IDM_SAVE: fileSave(); return;
    case IDM_SAVEAS: fileSaveAs(); return;
    case IDM_EXIT: SendMessageW(main, WM_CLOSE, 0, 0); return;
    case IDM_UNDO: editor->handle(cmd::Undo{}); break;
    case IDM_REDO: editor->handle(cmd::Redo{}); break;
    case IDM_CUT: editor->handle(cmd::Cut{}); break;
    case IDM_COPY: editor->handle(cmd::Copy{}); break;
    case IDM_PASTE: editor->handle(cmd::Paste{}); break;
    case IDM_DELETE: editor->handle(cmd::Delete{}); break;
    case IDM_SELECTALL: editor->handle(cmd::SelectAll{}); break;
    case IDM_WORDWRAP:
        editor->handle(cmd::ToggleWrap{});
        savePreferences();
        syncToolbar();
        break;
    case IDM_VIEWSOURCE: toggleSource(); return;
    case IDM_STATS:
        overlayOn = !overlayOn;
        updates->invalidateRect(overlayRect());
        requestPaint();
        return;
    case IDM_RELOADFONTS: {
        // View -> Reload Fonts: re-read every pack; only glyphs whose CSS changed are re-rasterized.
        for (const auto& p : registry->packs()) {
            registry->reload(p.id);
            cache->evictStale(p.id);
        }
        if (!registry->isUsable(editor->state().activePackId)) {
            log.add(core::makeWarning("W-FONT-PACK-UNAVAILABLE",
                                      "the active font pack is no longer valid; switched to verdana-style"));
            editor->handle(cmd::SetFont{std::string(fonts::kDefaultPackId)});
        }
        layout->setParams(layout->params()); // refresh pack revision and metrics
        layout->reset(doc);
        updates->invalidateAll();
        populateFontCombo();
        syncToolbar();
        if (preview) rebuildPreview();
        break;
    }
    case IDM_ABOUTFONTS: showAboutFonts(); return;
    case IDM_DIAGNOSTICS: showDiagnostics(); return;
    case IDM_ABOUT: showAbout(); return;
    default:
        MessageBoxW(main, L"This command is not available in this build.", L"PixelCSS", MB_ICONINFORMATION);
        return;
    }
    afterChange();
}

namespace {

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto& a = app();
    switch (msg) {
    case WM_CREATE: {
        a.main = hwnd;
        a.scale = scaleForDpi(GetDpiForWindow(hwnd)); // initial integer document scale (research R-08)
        a.toolbar = createToolbar(hwnd);
        a.status = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
                                   hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUS)), a.instance, nullptr);
        a.surface = createSurface(hwnd);
        a.layoutChildren();
        populateFontCombo();
        syncToolbar();
        a.updateTitle();
        a.updateStatus();
        SetFocus(a.surface);
        return 0;
    }
    case WM_SIZE: a.layoutChildren(); return 0;
    case WM_DPICHANGED: {
        // Runtime DPI change (window moved to another monitor): recompute the integer document scale with the
        // same policy as at startup (research R-08), apply the suggested rectangle and relayout everything.
        const int s = scaleForDpi(HIWORD(wp));
        const auto* r = reinterpret_cast<const RECT*>(lp);
        if (a.uiFont) {
            DeleteObject(a.uiFont);
            a.uiFont = nullptr;
        }
        SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        if (s != a.scale) {
            a.scale = s;
            a.editor->applyLayoutParams(s);
            a.updates->onDpiChanged(s);
            if (a.preview) rebuildPreview();
        }
        a.layoutChildren();
        a.afterChange();
        return 0;
    }
    case WM_SETFOCUS:
        if (a.surface) SetFocus(a.surface);
        return 0;
    case WM_COMMAND: {
        const int id = LOWORD(wp);
        const int code = HIWORD(wp);
        if (id == IDC_WRAPBUTTON) { a.command(IDM_WORDWRAP); SetFocus(a.surface); return 0; }
        if (id == IDC_FONTCOMBO) {
            if (code == CBN_SELCHANGE) {
                const LRESULT sel = SendMessageW(a.fontCombo, CB_GETCURSEL, 0, 0);
                if (sel >= 0) {
                    const auto* packId = reinterpret_cast<const std::string*>(
                        SendMessageW(a.fontCombo, CB_GETITEMDATA, static_cast<WPARAM>(sel), 0));
                    if (packId) a.setFont(*packId);
                }
                SetFocus(a.surface);
            }
            return 0;
        }
        a.command(id);
        return 0;
    }
    case WM_INITMENUPOPUP: {
        HMENU m = reinterpret_cast<HMENU>(wp);
        const bool ro = a.editor && a.editor->readOnly();
        const bool sel = a.editor && a.editor->state().hasSelection();
        CheckMenuItem(m, IDM_WORDWRAP, MF_BYCOMMAND | (a.editor && a.editor->state().wordWrap ? MF_CHECKED : MF_UNCHECKED));
        CheckMenuItem(m, IDM_VIEWSOURCE, MF_BYCOMMAND | (sourceViewShown() ? MF_CHECKED : MF_UNCHECKED));
        EnableMenuItem(m, IDM_VIEWSOURCE, MF_BYCOMMAND | (canToggleSource() ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(m, IDM_UNDO, MF_BYCOMMAND | (!ro && a.doc.canUndo() ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(m, IDM_REDO, MF_BYCOMMAND | (!ro && a.doc.canRedo() ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(m, IDM_CUT, MF_BYCOMMAND | (!ro && sel ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(m, IDM_COPY, MF_BYCOMMAND | (sel ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(m, IDM_PASTE, MF_BYCOMMAND | (!ro ? MF_ENABLED : MF_GRAYED));
        EnableMenuItem(m, IDM_DELETE, MF_BYCOMMAND | (!ro ? MF_ENABLED : MF_GRAYED));
        return 0;
    }
    case WM_APP_BENCH_NEXT: benchStep(); return 0;
    case WM_TIMER:
        if (wp == 2) {
            KillTimer(hwnd, 2);
            benchStep();
        }
        return 0;
    case WM_CLOSE:
        if (fileRequestExit()) DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        a.recordSample("exit");
        a.writeStatsJson();
        a.presenter.release();
        if (a.uiFont) DeleteObject(a.uiFont);
        a.uiFont = nullptr;
        PostQuitMessage(0);
        return 0;
    default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

} // namespace pxc::shell
