#include "pxc/storage/text_codec.hpp"

#include "pxc/core/unicode.hpp"

namespace pxc::storage {

const char* encodingName(Encoding e) noexcept {
    switch (e) {
    case Encoding::Utf8: return "UTF-8";
    case Encoding::Utf8Bom: return "UTF-8 BOM";
    case Encoding::Utf16LE: return "UTF-16 LE";
    case Encoding::Utf16BE: return "UTF-16 BE";
    }
    return "UTF-8";
}

const char* eolName(Eol e) noexcept {
    switch (e) {
    case Eol::CRLF: return "CRLF";
    case Eol::LF: return "LF";
    case Eol::Mixed: return "Mixed";
    }
    return "CRLF";
}

namespace {

// Strips '\r' (CRLF -> LF, lone CR -> LF) and detects the line-ending style.
std::u32string normalize(std::u32string&& in, Eol& eol) {
    std::size_t crlf = 0, lf = 0, cr = 0;
    std::u32string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        const char32_t c = in[i];
        if (c == U'\r') {
            if (i + 1 < in.size() && in[i + 1] == U'\n') {
                ++crlf;
                ++i;
            } else {
                ++cr;
            }
            out.push_back(U'\n');
        } else {
            if (c == U'\n') ++lf;
            out.push_back(c);
        }
    }
    const int kinds = (crlf > 0) + (lf > 0) + (cr > 0);
    if (kinds > 1 || cr > 0) eol = Eol::Mixed;
    else if (lf > 0) eol = Eol::LF;
    else eol = Eol::CRLF; // CRLF, or no line breaks at all (new-document default)
    return out;
}

} // namespace

core::Expected<LoadedText, FileError> decodeText(std::string_view bytes, InvalidUtf8Policy policy,
                                                 const std::wstring& displayName) {
    LoadedText out;
    std::u32string decoded;
    auto u8 = [](std::size_t i, std::string_view b) { return static_cast<unsigned char>(b[i]); };
    if (bytes.size() >= 2 && u8(0, bytes) == 0xFF && u8(1, bytes) == 0xFE) {
        out.info.encoding = Encoding::Utf16LE;
        std::u16string units;
        for (std::size_t i = 2; i + 1 < bytes.size(); i += 2)
            units.push_back(static_cast<char16_t>(u8(i, bytes) | (u8(i + 1, bytes) << 8)));
        decoded = core::decodeUtf16(units);
    } else if (bytes.size() >= 2 && u8(0, bytes) == 0xFE && u8(1, bytes) == 0xFF) {
        out.info.encoding = Encoding::Utf16BE;
        std::u16string units;
        for (std::size_t i = 2; i + 1 < bytes.size(); i += 2)
            units.push_back(static_cast<char16_t>((u8(i, bytes) << 8) | u8(i + 1, bytes)));
        decoded = core::decodeUtf16(units);
    } else {
        std::string_view body = bytes;
        if (bytes.size() >= 3 && u8(0, bytes) == 0xEF && u8(1, bytes) == 0xBB && u8(2, bytes) == 0xBF) {
            out.info.encoding = Encoding::Utf8Bom;
            body.remove_prefix(3);
        }
        auto r = core::decodeUtf8(body, policy == InvalidUtf8Policy::Fail ? core::InvalidPolicy::Fail
                                                                         : core::InvalidPolicy::Replace);
        if (!r) {
            return core::Unexpected(FileError{
                FileError::Kind::InvalidEncoding,
                L"PixelCSS could not open " + displayName + L" because it is not valid UTF-8 text (invalid byte at "
                    L"offset " + std::to_wstring(r.error().byteOffset) + L")."});
        }
        decoded = std::move(*r);
        if (policy == InvalidUtf8Policy::Replace) {
            // Mark converted only if replacement actually happened.
            auto strict = core::decodeUtf8(body, core::InvalidPolicy::Fail);
            out.info.converted = !strict.has_value();
        }
    }
    out.text = normalize(std::move(decoded), out.info.eol);
    return out;
}

std::string encodeText(std::u32string_view text, const FileInfo& info) {
    std::string out;
    if (info.encoding == Encoding::Utf8Bom) out = "\xEF\xBB\xBF";
    const bool crlf = info.eol != Eol::LF;
    out.reserve(out.size() + text.size() + text.size() / 16);
    for (char32_t c : text) {
        if (c == U'\n') {
            if (crlf) out += '\r';
            out += '\n';
        } else {
            out += core::encodeUtf8(c);
        }
    }
    return out;
}

} // namespace pxc::storage
