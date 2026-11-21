#pragma once
// parseGlyph: pure function from glyph CSS text to GlyphDefinition (no filesystem access).

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include "pxc/core/expected.hpp"
#include "pxc/glyphcss/glyph_definition.hpp"

namespace pxc::glyphcss {

// `source` is used for diagnostics and for the filename code-point check (when its stem has the
// form U+XXXX). When `expectedFamily` is given, --family must equal it (E-GLYPH-FAMILY-MISMATCH).
core::Expected<GlyphDefinition, std::vector<core::Diagnostic>>
parseGlyph(std::string_view cssText, const std::filesystem::path& source,
           std::optional<std::string_view> expectedFamily = std::nullopt);

// Parses a filename stem such as "U+0041" (4–6 uppercase hex digits). Returns nullopt otherwise.
std::optional<char32_t> codePointFromFileName(const std::filesystem::path& file);

} // namespace pxc::glyphcss
