#pragma once
// Plain-text decoding/encoding with encoding and line-ending detection (research R-11, FR-032).

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "pxc/core/expected.hpp"

namespace pxc::storage {

enum class Encoding { Utf8, Utf8Bom, Utf16LE, Utf16BE };
enum class Eol { CRLF, LF, Mixed };

const char* encodingName(Encoding e) noexcept;
const char* eolName(Eol e) noexcept;

struct FileInfo {
    std::optional<std::filesystem::path> path;
    Encoding encoding = Encoding::Utf8;
    Eol eol = Eol::CRLF; // new documents use CRLF
    bool converted = false; // invalid bytes were replaced with U+FFFD on load
    bool operator==(const FileInfo&) const = default;
};

enum class InvalidUtf8Policy { Fail, Replace };

struct FileError {
    enum class Kind { NotFound, AccessDenied, SharingViolation, DiskFull, InvalidEncoding, Other };
    Kind kind = Kind::Other;
    std::wstring userMessage;
};

struct LoadedText {
    std::u32string text; // never contains '\r'
    FileInfo info;
};

core::Expected<LoadedText, FileError> decodeText(std::string_view bytes, InvalidUtf8Policy policy,
                                                 const std::wstring& displayName = L"the file");

// Encodes as UTF-8 (with BOM for Utf8Bom); UTF-16 sources are written as UTF-8. Line breaks use
// info.eol (Mixed writes CRLF).
std::string encodeText(std::u32string_view text, const FileInfo& info);

} // namespace pxc::storage
