#include "pxc/core/unicode.hpp"

namespace pxc::core {

Expected<std::u32string, Utf8Error> decodeUtf8(std::string_view bytes, InvalidPolicy policy) {
    std::u32string out;
    out.reserve(bytes.size());
    const auto* p = reinterpret_cast<const unsigned char*>(bytes.data());
    const std::size_t n = bytes.size();
    std::size_t i = 0;
    while (i < n) {
        const unsigned char b0 = p[i];
        if (b0 < 0x80) {
            out.push_back(b0);
            ++i;
            continue;
        }
        int len = 0;
        char32_t cp = 0, minCp = 0;
        if ((b0 & 0xE0) == 0xC0) { len = 2; cp = b0 & 0x1F; minCp = 0x80; }
        else if ((b0 & 0xF0) == 0xE0) { len = 3; cp = b0 & 0x0F; minCp = 0x800; }
        else if ((b0 & 0xF8) == 0xF0) { len = 4; cp = b0 & 0x07; minCp = 0x10000; }

        bool ok = len != 0 && i + static_cast<std::size_t>(len) <= n;
        int consumed = 1;
        if (ok) {
            for (int k = 1; k < len; ++k) {
                const unsigned char bk = p[i + static_cast<std::size_t>(k)];
                if ((bk & 0xC0) != 0x80) { ok = false; break; }
                cp = (cp << 6) | (bk & 0x3F);
                consumed = k + 1;
            }
        } else if (len != 0) {
            // Truncated sequence: consume valid continuation bytes so replacement is one char.
            for (std::size_t k = i + 1; k < n && (p[k] & 0xC0) == 0x80; ++k) ++consumed;
        }
        if (ok && (cp < minCp || !isValidScalar(cp))) ok = false;
        if (ok) {
            out.push_back(cp);
            i += static_cast<std::size_t>(len);
        } else {
            if (policy == InvalidPolicy::Fail) return Unexpected(Utf8Error{i});
            out.push_back(kReplacementChar);
            i += static_cast<std::size_t>(consumed);
        }
    }
    return out;
}

std::string encodeUtf8(char32_t cp) {
    std::string out;
    if (!isValidScalar(cp)) cp = kReplacementChar;
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    return out;
}

std::string encodeUtf8(std::u32string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char32_t cp : text) out += encodeUtf8(cp);
    return out;
}

std::u32string decodeUtf16(std::span<const char16_t> u) {
    std::u32string out;
    out.reserve(u.size());
    for (std::size_t i = 0; i < u.size(); ++i) {
        const char16_t c = u[i];
        if (c >= 0xD800 && c <= 0xDBFF) {
            if (i + 1 < u.size() && u[i + 1] >= 0xDC00 && u[i + 1] <= 0xDFFF) {
                out.push_back(0x10000 + ((static_cast<char32_t>(c) - 0xD800) << 10) +
                              (static_cast<char32_t>(u[i + 1]) - 0xDC00));
                ++i;
            } else {
                out.push_back(kReplacementChar);
            }
        } else if (c >= 0xDC00 && c <= 0xDFFF) {
            out.push_back(kReplacementChar);
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::u16string encodeUtf16(std::u32string_view text) {
    std::u16string out;
    out.reserve(text.size());
    for (char32_t cp : text) {
        if (!isValidScalar(cp)) cp = kReplacementChar;
        if (cp < 0x10000) {
            out.push_back(static_cast<char16_t>(cp));
        } else {
            cp -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
        }
    }
    return out;
}

std::wstring toWide(std::u32string_view text) {
    const std::u16string u16 = encodeUtf16(text);
    return std::wstring(u16.begin(), u16.end());
}

std::u32string fromWide(std::wstring_view text) {
    std::u16string u16(text.begin(), text.end());
    return decodeUtf16(u16);
}

std::wstring utf8ToWide(std::string_view utf8) {
    auto d = decodeUtf8(utf8, InvalidPolicy::Replace);
    return toWide(*d);
}

std::string wideToUtf8(std::wstring_view wide) { return encodeUtf8(fromWide(wide)); }

} // namespace pxc::core
