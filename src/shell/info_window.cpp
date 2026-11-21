// Small modal window with a report-view ListView, used by Help → About Fonts and Help → Diagnostics.
// Chrome only: the ListView draws its own captions with the system UI font.
#include "app.hpp"

#include <commctrl.h>

namespace pxc::shell {

namespace {

constexpr wchar_t kInfoClass[] = L"PixelCSS.InfoWindow";
constexpr int kOkId = 1;

struct InfoState {
    HWND list = nullptr;
    HWND footer = nullptr;
    HWND ok = nullptr;
    bool done = false;
};

LRESULT CALLBACK InfoWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<InfoState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_SIZE:
        if (st) {
            const int w = LOWORD(lp), h = HIWORD(lp);
            const UINT dpi = GetDpiForWindow(hwnd);
            const int pad = MulDiv(8, static_cast<int>(dpi), 96), btnW = MulDiv(80, static_cast<int>(dpi), 96),
                      btnH = MulDiv(26, static_cast<int>(dpi), 96), footH = MulDiv(36, static_cast<int>(dpi), 96);
            MoveWindow(st->list, pad, pad, w - 2 * pad, h - 3 * pad - btnH - footH, TRUE);
            MoveWindow(st->footer, pad, h - 2 * pad - btnH - footH, w - 2 * pad, footH, TRUE);
            MoveWindow(st->ok, w - pad - btnW, h - pad - btnH, btnW, btnH, TRUE);
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == kOkId || LOWORD(wp) == IDCANCEL) DestroyWindow(hwnd);
        return 0;
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        if (st) st->done = true;
        return 0;
    default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

void showInfoTable(const std::wstring& title, const std::vector<std::wstring>& columns,
                   const std::vector<std::vector<std::wstring>>& rows, const std::wstring& footer) {
    auto& a = app();
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = InfoWndProc;
        wc.hInstance = a.instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = kInfoClass;
        registered = RegisterClassExW(&wc) != 0;
    }
    const UINT dpi = GetDpiForWindow(a.main);
    InfoState st;
    HWND w = CreateWindowExW(WS_EX_DLGMODALFRAME, kInfoClass, title.c_str(),
                             WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, CW_USEDEFAULT, CW_USEDEFAULT,
                             MulDiv(860, static_cast<int>(dpi), 96), MulDiv(380, static_cast<int>(dpi), 96), a.main,
                             nullptr, a.instance, nullptr);
    if (!w) return;
    SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
    st.list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                              0, 0, 10, 10, w, nullptr, a.instance, nullptr);
    ListView_SetExtendedListViewStyle(st.list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    st.footer = CreateWindowExW(0, L"STATIC", footer.c_str(), WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, w, nullptr, a.instance, nullptr);
    st.ok = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 10, 10, w,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kOkId)), a.instance, nullptr);
    if (a.uiFont) {
        for (HWND c : {st.list, st.footer, st.ok}) SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(a.uiFont), TRUE);
    }
    for (std::size_t c = 0; c < columns.size(); ++c) {
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = MulDiv(c == columns.size() - 1 ? 260 : 120, static_cast<int>(dpi), 96);
        col.pszText = const_cast<wchar_t*>(columns[c].c_str());
        ListView_InsertColumn(st.list, static_cast<int>(c), &col);
    }
    for (std::size_t r = 0; r < rows.size(); ++r) {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(r);
        item.pszText = const_cast<wchar_t*>(rows[r].empty() ? L"" : rows[r][0].c_str());
        ListView_InsertItem(st.list, &item);
        for (std::size_t c = 1; c < rows[r].size(); ++c)
            ListView_SetItemText(st.list, static_cast<int>(r), static_cast<int>(c), const_cast<wchar_t*>(rows[r][c].c_str()));
    }
    // Centre over the main window and run modally.
    RECT mr, wr;
    GetWindowRect(a.main, &mr);
    GetWindowRect(w, &wr);
    SetWindowPos(w, nullptr, mr.left + ((mr.right - mr.left) - (wr.right - wr.left)) / 2,
                 mr.top + ((mr.bottom - mr.top) - (wr.bottom - wr.top)) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    RECT cr;
    GetClientRect(w, &cr);
    SendMessageW(w, WM_SIZE, 0, MAKELPARAM(cr.right, cr.bottom));
    EnableWindow(a.main, FALSE);
    ShowWindow(w, SW_SHOW);
    MSG msg;
    while (!st.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(w, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(a.main, TRUE);
    SetForegroundWindow(a.main);
}

} // namespace pxc::shell
