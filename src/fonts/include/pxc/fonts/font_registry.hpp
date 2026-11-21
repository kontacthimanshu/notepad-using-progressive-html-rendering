#pragma once
// Manifest-driven font pack registry with deterministic glyph resolution (data-model §3–§4, research R-07).

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "pxc/core/diagnostic.hpp"
#include "pxc/glyphcss/font_manifest.hpp"
#include "pxc/glyphcss/glyph_definition.hpp"

namespace pxc::fonts {

inline constexpr std::string_view kDefaultPackId = "verdana-style";

enum class PackState { Loading, Valid, Degraded, Invalid };
const char* packStateName(PackState s) noexcept;

struct FontMetrics {
    int lineHeight = 16;
    int ascent = 12;
    int descent = 3;
    int spaceAdvance = 4;
};

struct FontPackInfo {
    std::string id;
    std::string displayName;
    std::string styleDescription;
    std::string version;
    std::string authenticity;
    std::string origin;
    std::string author;
    std::string license;
    std::vector<std::string> aliases;
    int order = 1000;
    PackState state = PackState::Invalid;
    std::size_t glyphCount = 0;
    std::size_t coverageDeclared = 0;
    std::size_t coverageMissing = 0;
    std::filesystem::path dir;
    std::uint32_t revision = 0;
};

enum class GlyphSource { Primary, PackReplacement, EngineReplacement };

struct ResolvedGlyph {
    const glyphcss::GlyphDefinition* def = nullptr; // never null once the fallback chain is complete
    GlyphSource source = GlyphSource::Primary;
    std::string packUsed;
};

struct ReloadResult {
    std::vector<char32_t> changedCodePoints;
    PackState newState = PackState::Invalid;
};

class FontRegistry {
public:
    explicit FontRegistry(core::DiagnosticsLog& log);
    ~FontRegistry();
    FontRegistry(const FontRegistry&) = delete;
    FontRegistry& operator=(const FontRegistry&) = delete;

    // Scans <fontsDir>/*/manifest.css. Never throws; problems go to the diagnostics log.
    void loadAll(const std::filesystem::path& fontsDir);
    // Loads (or replaces) one pack directory. Returns the resulting pack state.
    PackState loadPack(const std::filesystem::path& packDir);
    // Re-reads a loaded pack; only definitions whose content hash changed are replaced.
    ReloadResult reload(std::string_view packId);

    // Valid and Degraded packs first by (order, id); Invalid packs are listed last.
    std::vector<FontPackInfo> packs() const;
    std::optional<FontPackInfo> pack(std::string_view id) const;
    bool isUsable(std::string_view id) const; // Valid or Degraded
    std::uint32_t packRevision(std::string_view id) const;

    // Metrics of the pack, or of the fallback pack / engine defaults when unavailable.
    FontMetrics metrics(std::string_view packId) const;
    // Resolves the glyph for `cp`, following the fallback chain of research R-07.
    ResolvedGlyph resolve(std::string_view packId, char32_t cp) const;
    // Pack id whose manifest lists `alias` (case-insensitive) in --aliases, or empty.
    std::string packForAlias(std::string_view alias) const;

    // Glyph definitions of a pack (for tools such as `validate` and `distinct`).
    std::vector<const glyphcss::GlyphDefinition*> glyphs(std::string_view packId) const;
    const glyphcss::FontManifest* manifest(std::string_view packId) const;
    // All diagnostics recorded while loading the pack (errors and warnings).
    std::vector<core::Diagnostic> packDiagnostics(std::string_view packId) const;

    core::DiagnosticsLog& diagnostics() const noexcept { return *log_; }

    struct Pack;

private:
    Pack* find(std::string_view id) const;
    const glyphcss::GlyphDefinition* engineReplacement(std::string_view packId) const;

    core::DiagnosticsLog* log_;
    std::vector<std::unique_ptr<Pack>> packs_;
    void reportOnce(const char* code, const std::string& key, const std::string& message) const;
    mutable std::map<std::string, std::unique_ptr<glyphcss::GlyphDefinition>, std::less<>> engineGlyphs_;
    mutable std::set<std::string, std::less<>> reported_;
};

} // namespace pxc::fonts
