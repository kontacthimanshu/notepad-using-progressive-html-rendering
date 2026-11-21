#pragma once
// Internal definition of FontRegistry::Pack (shared by font_pack.cpp and font_registry.cpp).

#include <map>
#include <memory>

#include "pxc/fonts/font_registry.hpp"

namespace pxc::fonts {

struct FontRegistry::Pack {
    std::string id;
    std::filesystem::path dir;
    glyphcss::FontManifest manifest;
    std::map<char32_t, glyphcss::GlyphDefinition> glyphs;
    PackState state = PackState::Invalid;
    std::uint32_t revision = 1;
    std::size_t coverageMissing = 0;
    std::vector<core::Diagnostic> diagnostics;
};

std::unique_ptr<FontRegistry::Pack> loadPackFromDisk(const std::filesystem::path& dir);

} // namespace pxc::fonts
