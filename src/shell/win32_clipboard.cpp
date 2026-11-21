// Win32 clipboard: CF_UNICODETEXT only — ordinary Unicode text, never glyph pixels (FR-030).
#include "app.hpp"

#include <cstring>

#include "pxc/core/unicode.hpp"

namespace pxc::shell {

namespace {
bool openClipboardWithRetry(HWND owner) {
    for (int i = 0; i < 5; ++i) {
        if (OpenClipboard(owner)) return true;
        Sleep(10);
    }
    return false;
}
} // namespace

std::optional<std::u32string> Win32Clipboard::getText() {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return std::nullopt;
    if (!openClipboardWithRetry(owner_ ? *owner_ : nullptr)) return std::nullopt;
    std::optional<std::u32string> out;
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        if (const auto* p = static_cast<const wchar_t*>(GlobalLock(h))) {
            const std::size_t maxChars = GlobalSize(h) / sizeof(wchar_t);
            std::size_t n = 0;
            while (n < maxChars && p[n] != 0) ++n;
            out = core::fromWide(std::wstring_view(p, n));
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return out;
}

bool Win32Clipboard::setText(std::u32string_view text) {
    std::u32string crlf;
    crlf.reserve(text.size() + text.size() / 16);
    for (char32_t c : text) {
        if (c == U'\n') crlf.push_back(U'\r');
        crlf.push_back(c);
    }
    const std::wstring w = core::toWide(crlf);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t));
    if (!mem) return false;
    if (auto* p = static_cast<wchar_t*>(GlobalLock(mem))) {
        std::memcpy(p, w.c_str(), (w.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(mem);
    }
    if (!openClipboardWithRetry(owner_ ? *owner_ : nullptr)) {
        GlobalFree(mem);
        return false;
    }
    EmptyClipboard();
    const bool ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
    CloseClipboard();
    if (!ok) GlobalFree(mem);
    return ok;
}

} // namespace pxc::shell
