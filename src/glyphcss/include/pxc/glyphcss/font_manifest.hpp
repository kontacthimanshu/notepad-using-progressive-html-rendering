#pragma once
// Font pack manifest `pixelcss-pack/1` (data-model §2, contracts/font-manifest-schema.md).

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "pxc/core/expected.hpp"
#include "pxc/core/diagnostic.hpp"

namespace pxc::glyphcss {

inline constexpr const char* kPackSchema = "pixelcss-pack/1";

struct CodePointRange {
    char32_t first = 0, last = 0;
    bool contains(char32_t cp) const noexcept { return cp >= first && cp <= last; }
};

struct FontManifest {
    std::string schema;
    std::string id;
    std::string displayName;
    std::string styleDescription;
    std::string version;
    std::string authenticity; // "style-interpretation" | "licensed-original"
    std::string origin;
    std::string author;
    std::string license;
    std::string licenseFile;
    int lineHeight = 0;
    int ascent = 0;
    int descent = 0;
    std::vector<CodePointRange> coverage;
    char32_t replacementGlyph = U'�';
    std::string glyphDir = "glyphs";
    std::vector<std::string> aliases;
    int order = 1000;
    std::vector<core::Diagnostic> warnings;

    bool covers(char32_t cp) const noexcept {
        for (const auto& r : coverage)
            if (r.contains(cp)) return true;
        return false;
    }
    std::size_t coverageCount() const noexcept {
        std::size_t n = 0;
        for (const auto& r : coverage) n += static_cast<std::size_t>(r.last - r.first + 1);
        return n;
    }
};

core::Expected<FontManifest, std::vector<core::Diagnostic>> parseManifest(std::string_view cssText,
                                                                          const std::filesystem::path& source);

} // namespace pxc::glyphcss
