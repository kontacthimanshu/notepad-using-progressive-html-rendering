#pragma once
// UTF-8 / UTF-16 <-> UTF-32 codecs. The document model stores char32_t code points.

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include "pxc/core/expected.hpp"

namespace pxc::core {

inline constexpr char32_t kReplacementChar = U'�';

enum class InvalidPolicy { Fail, Replace };

struct Utf8Error {
    std::size_t byteOffset = 0;
};

// Rejects overlong encodings, surrogates (U+D800..U+DFFF) and values above U+10FFFF.
Expected<std::u32string, Utf8Error> decodeUtf8(std::string_view bytes, InvalidPolicy policy);
std::string encodeUtf8(std::u32string_view text);
std::string encodeUtf8(char32_t cp);

// Lone surrogates decode to U+FFFD.
std::u32string decodeUtf16(std::span<const char16_t> units);
std::u16string encodeUtf16(std::u32string_view text);

// Wide-string helpers for Win32 (wchar_t is UTF-16 on Windows).
std::wstring toWide(std::u32string_view text);
std::u32string fromWide(std::wstring_view text);
std::wstring utf8ToWide(std::string_view utf8);
std::string wideToUtf8(std::wstring_view wide);

constexpr bool isValidScalar(char32_t cp) noexcept {
    return cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF);
}

} // namespace pxc::core
