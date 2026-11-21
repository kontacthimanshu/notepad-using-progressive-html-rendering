// Help → About PixelCSS: version, toolchain and build configuration (US7-AS1 evidence).
#include "app.hpp"

#include <commctrl.h>

#include <string>

#ifndef PXC_VERSION
#define PXC_VERSION "0.0.0"
#endif
#ifndef PXC_SDK_VERSION
#define PXC_SDK_VERSION "unknown"
#endif

namespace pxc::shell {

std::wstring aboutText() {
    auto w = [](const std::string& s) { return std::wstring(s.begin(), s.end()); };
#ifdef _DEBUG
    const wchar_t* config = L"Debug";
#else
    const wchar_t* config = L"Release";
#endif
    std::wstring t;
    t += L"Version " + w(PXC_VERSION) + L" (x64 " + config + L")\n\n";
    t += L"Compiler: MSVC cl.exe " + std::to_wstring(_MSC_FULL_VER) + L"\n";
    t += L"C++ standard: _MSVC_LANG = " + std::to_wstring(_MSVC_LANG) + L"\n";
    t += L"Windows SDK: " + w(PXC_SDK_VERSION) + L"\n\n";
    t += L"Every document character is rendered by the PixelCSS software rasterizer from per-glyph CSS files. "
         L"No TrueType/OpenType, DirectWrite, GDI text drawing, WebView2 or browser engine is used for document text.";
    return t;
}

void showAbout() {
    const std::wstring content = aboutText();
    TASKDIALOGCONFIG c{sizeof(c)};
    c.hwndParent = app().main;
    c.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    c.pszWindowTitle = L"About PixelCSS";
    c.pszMainInstruction = L"PixelCSS Browser";
    c.pszContent = content.c_str();
    c.dwCommonButtons = TDCBF_OK_BUTTON;
    c.pszMainIcon = TD_INFORMATION_ICON;
    TaskDialogIndirect(&c, nullptr, nullptr, nullptr);
}

} // namespace pxc::shell
