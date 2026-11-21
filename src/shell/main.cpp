// PixelCSS.exe entry point: native Win32 shell for the notepad-first CSS-glyph browser.
#include "app.hpp"

#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>

#include <cstdio>
#include <exception>
#include <fstream>

#include "pxc/core/scale.hpp"
#include "pxc/core/unicode.hpp"

namespace pxc::shell {

App& app() {
    static App instance;
    return instance;
}

int scaleForDpi(UINT dpi) noexcept { return core::scaleForDpi(dpi); }

void App::emergencySave() noexcept {
    try {
        wchar_t tmp[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        SYSTEMTIME t;
        GetLocalTime(&t);
        wchar_t name[128];
        swprintf_s(name, L"PixelCSS-recovery-%04d%02d%02d-%02d%02d%02d.txt", t.wYear, t.wMonth, t.wDay, t.wHour,
                   t.wMinute, t.wSecond);
        const std::filesystem::path p = std::filesystem::path(tmp) / name;
        std::ofstream f(p, std::ios::binary);
        const std::string bytes = core::encodeUtf8(doc.text());
        f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        const std::wstring msg = L"PixelCSS encountered an unexpected error and must close.\n\nYour document was saved to:\n" + p.wstring();
        MessageBoxW(main, msg.c_str(), L"PixelCSS", MB_ICONERROR | MB_OK);
    } catch (...) {
        MessageBoxW(main, L"PixelCSS encountered an unexpected error and must close.", L"PixelCSS", MB_ICONERROR | MB_OK);
    }
}

} // namespace pxc::shell

namespace {

pxc::shell::Options parseCommandLine() {
    pxc::shell::Options o;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv && i < argc; ++i) {
        const std::wstring a = argv[i];
        auto next = [&]() -> std::optional<std::filesystem::path> {
            if (i + 1 < argc) return std::filesystem::path(argv[++i]);
            return std::nullopt;
        };
        if (a == L"--fonts-dir") o.fontsDir = next();
        else if (a == L"--stats-json") o.statsJson = next();
        else if (a == L"--bench-script") o.benchScript = next();
        else if (a == L"--exit-after-first-present") o.exitAfterFirstPresent = true;
        else if (a == L"--no-preferences") o.noPreferences = true;
        else if (!a.empty() && a[0] != L'-') o.file = std::filesystem::path(a);
    }
    if (argv) LocalFree(argv);
    return o;
}

int runMessageLoop() {
    auto& a = pxc::shell::app();
    MSG msg{};
    for (;;) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return static_cast<int>(msg.wParam);
            if (a.accel && TranslateAcceleratorW(a.main, a.accel, &msg)) continue;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        // Idle: progressive layout slices and other background work (research R-12).
        if (a.idle()) continue;
        MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int show) {
    // Per-Monitor-V2 is declared in app.manifest; this call is a fallback when the manifest is absent.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icc);
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    auto& a = pxc::shell::app();
    a.instance = inst;
    a.options = parseCommandLine();
    int rc = 0;
    try {
        if (!pxc::shell::registerWindowClasses(inst)) return 1;
        a.initEngine();
        if (!pxc::shell::createMainWindow(inst)) return 1;
        ShowWindow(a.main, show);
        UpdateWindow(a.main);
        if (a.options.file) pxc::shell::fileOpen(*a.options.file);
        if (a.options.benchScript) pxc::shell::startBenchScript(*a.options.benchScript);
        rc = runMessageLoop();
    } catch (const std::exception&) {
        a.emergencySave();
        rc = 3;
    } catch (...) {
        a.emergencySave();
        rc = 3;
    }
    if (SUCCEEDED(com)) CoUninitialize();
    return rc;
}
