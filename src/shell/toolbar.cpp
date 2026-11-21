// Toolbar strip: the checkable "Word Wrap" button (FR-029) and a reserved slot that hosts the font-family
// dropdown (FR-025). Common controls draw their own captions with the system UI font, which the spec allows
// for application chrome; document text never goes through them.
#include "app.hpp"

#include <commctrl.h>

#include <algorithm>

#include "resource.h"

namespace pxc::shell {

namespace {
constexpr int kFontSlotWidth = 230; // separator width reserved for the "Font:" label + combo box
constexpr int kFontSlotIndex = 0;
} // namespace

HWND createToolbar(HWND parent) {
    auto& a = app();
    HWND tb = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr,
                              WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_LIST | CCS_TOP | CCS_NODIVIDER, 0, 0, 0,
                              0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TOOLBAR)), a.instance, nullptr);
    SendMessageW(tb, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessageW(tb, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS);
    SendMessageW(tb, TB_SETBITMAPSIZE, 0, MAKELPARAM(0, 0));
    TBBUTTON buttons[3]{};
    // [0] separator placeholder for the font dropdown (US3)
    buttons[0].iBitmap = kFontSlotWidth;
    buttons[0].fsStyle = BTNS_SEP;
    // [1] spacer
    buttons[1].iBitmap = 8;
    buttons[1].fsStyle = BTNS_SEP;
    // [2] Word Wrap check button
    buttons[2].iBitmap = I_IMAGENONE;
    buttons[2].idCommand = IDC_WRAPBUTTON;
    buttons[2].fsState = TBSTATE_ENABLED;
    buttons[2].fsStyle = BTNS_CHECK | BTNS_AUTOSIZE | BTNS_SHOWTEXT;
    buttons[2].iString = reinterpret_cast<INT_PTR>(L"↵ Word Wrap");
    SendMessageW(tb, TB_ADDBUTTONSW, 3, reinterpret_cast<LPARAM>(buttons));
    SendMessageW(tb, TB_AUTOSIZE, 0, 0);
    return tb;
}

// Rectangle of the reserved font slot in toolbar client coordinates.
RECT fontSlotRect() {
    RECT r{};
    SendMessageW(app().toolbar, TB_GETITEMRECT, kFontSlotIndex, reinterpret_cast<LPARAM>(&r));
    return r;
}

namespace {

HFONT uiFont() {
    auto& a = app();
    if (!a.uiFont) {
        NONCLIENTMETRICSW ncm{sizeof(ncm)};
        SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, GetDpiForWindow(a.main));
        a.uiFont = CreateFontIndirectW(&ncm.lfMessageFont);
    }
    return a.uiFont;
}

std::wstring tooltipFor(const fonts::FontPackInfo& p) {
    std::wstring name = std::wstring(p.displayName.begin(), p.displayName.end());
    if (p.authenticity == "style-interpretation") return name + L" (style interpretation — not the original typeface)";
    return name + L" (licensed original)";
}

void setTooltip(const std::wstring& text) {
    auto& a = app();
    if (!a.fontTip || !a.fontCombo) return;
    static std::wstring s_text;
    s_text = text;
    TOOLINFOW ti{sizeof(ti)};
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd = a.main;
    ti.uId = reinterpret_cast<UINT_PTR>(a.fontCombo);
    ti.lpszText = s_text.data();
    SendMessageW(a.fontTip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&ti));
}

} // namespace

// The font-family dropdown (FR-025): CBS_DROPDOWNLIST hosted in the toolbar's reserved slot.
void populateFontCombo() {
    auto& a = app();
    if (!a.toolbar || !a.editor) return;
    if (!a.fontCombo) {
        const RECT slot = fontSlotRect();
        const int labelW = 38;
        a.fontLabel = CreateWindowExW(0, L"STATIC", L"Font:", WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_RIGHT,
                                      slot.left + 2, slot.top, labelW, slot.bottom - slot.top, a.toolbar, nullptr,
                                      a.instance, nullptr);
        a.fontCombo = CreateWindowExW(0, WC_COMBOBOXW, nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
                                      slot.left + labelW + 8, slot.top + 1, slot.right - slot.left - labelW - 12, 240,
                                      a.toolbar, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_FONTCOMBO)),
                                      a.instance, nullptr);
        SendMessageW(a.fontLabel, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont()), TRUE);
        SendMessageW(a.fontCombo, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont()), TRUE);
        // Re-centre the combo vertically in the toolbar row.
        RECT cr;
        GetWindowRect(a.fontCombo, &cr);
        const int ch = cr.bottom - cr.top;
        SetWindowPos(a.fontCombo, nullptr, slot.left + labelW + 8, slot.top + std::max(0, (int(slot.bottom - slot.top) - ch) / 2),
                     0, 0, SWP_NOSIZE | SWP_NOZORDER);
        a.fontTip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP, CW_USEDEFAULT,
                                    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, a.main, nullptr, a.instance, nullptr);
        TOOLINFOW ti{sizeof(ti)};
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = a.main;
        ti.uId = reinterpret_cast<UINT_PTR>(a.fontCombo);
        ti.lpszText = const_cast<wchar_t*>(L"");
        SendMessageW(a.fontTip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
        SendMessageW(a.fontTip, TTM_SETMAXTIPWIDTH, 0, 420);
    }
    SendMessageW(a.fontCombo, CB_RESETCONTENT, 0, 0);
    a.comboIds.clear();
    int selected = -1;
    for (const auto& p : a.registry->packs()) {
        if (p.state != fonts::PackState::Valid && p.state != fonts::PackState::Degraded) continue; // invalid: omitted
        std::wstring label(p.displayName.begin(), p.displayName.end());
        if (p.state == fonts::PackState::Degraded) label += L" ⚠";
        const LRESULT idx = SendMessageW(a.fontCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        a.comboIds.push_back(std::make_unique<std::string>(p.id));
        SendMessageW(a.fontCombo, CB_SETITEMDATA, static_cast<WPARAM>(idx), reinterpret_cast<LPARAM>(a.comboIds.back().get()));
        if (p.id == a.editor->state().activePackId) {
            selected = static_cast<int>(idx);
            setTooltip(tooltipFor(p));
        }
    }
    SendMessageW(a.fontCombo, CB_SETCURSEL, static_cast<WPARAM>(selected), 0);
}

void syncToolbar() {
    auto& a = app();
    if (!a.toolbar || !a.editor) return;
    SendMessageW(a.toolbar, TB_CHECKBUTTON, IDC_WRAPBUTTON, MAKELPARAM(a.editor->state().wordWrap ? TRUE : FALSE, 0));
    if (a.fontCombo) {
        const LRESULT n = SendMessageW(a.fontCombo, CB_GETCOUNT, 0, 0);
        for (LRESULT i = 0; i < n; ++i) {
            const auto* id = reinterpret_cast<const std::string*>(SendMessageW(a.fontCombo, CB_GETITEMDATA, static_cast<WPARAM>(i), 0));
            if (id && *id == a.editor->state().activePackId) {
                SendMessageW(a.fontCombo, CB_SETCURSEL, static_cast<WPARAM>(i), 0);
                if (auto p = a.registry->pack(*id)) setTooltip(tooltipFor(*p));
            }
        }
    }
}

void App::setFont(const std::string& packId) {
    if (!editor) return;
    if (editor->handle(editor::cmd::SetFont{packId})) {
        if (preview) rebuildPreview(); // restyle elements that follow the toolbar font
        savePreferences();
        syncToolbar();
        afterChange();
    }
}

void App::savePreferences() {
    if (!prefs || !editor) return;
    if (!prefs->save(storage::Preferences{editor->state().activePackId, editor->state().wordWrap}))
        log.add(core::makeWarning("W-PREFS-SAVE", "display preferences could not be saved"));
}

} // namespace pxc::shell
