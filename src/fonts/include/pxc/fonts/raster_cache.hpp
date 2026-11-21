#pragma once
// LRU cache of rasterized glyphs keyed per research R-14.

#include <cstdint>
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>

#include "pxc/core/geometry.hpp"
#include "pxc/fonts/font_registry.hpp"
#include "pxc/render/raster_glyph.hpp"

namespace pxc::fonts {

struct RasterCacheKey {
    std::string packId;
    std::string style;
    std::uint32_t packRevision = 0;
    char32_t codePoint = 0;
    std::uint64_t glyphRevision = 0;
    int scale = 1;
    std::uint32_t fg = 0;
    std::uint32_t bg = 0;
    bool operator==(const RasterCacheKey&) const = default;
};

struct RasterCacheKeyHash {
    std::size_t operator()(const RasterCacheKey& k) const noexcept;
};

struct CacheStats {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t rasterizations = 0;
    std::uint64_t evictions = 0;
    double hitRate() const noexcept {
        const auto total = hits + misses;
        return total == 0 ? 1.0 : static_cast<double>(hits) / static_cast<double>(total);
    }
};

class RasterCache {
public:
    explicit RasterCache(const FontRegistry& registry, std::size_t budgetBytes = 16u << 20);

    // Returns the raster for (pack, cp) at `scale` with fg/bg resolved. The reference stays valid
    // until the next call that may evict (any get()).
    const render::RasterGlyph& get(std::string_view packId, char32_t cp, int scale, core::Color fg, core::Color bg);

    CacheStats stats() const noexcept { return stats_; }
    void resetStats() noexcept { stats_ = {}; }
    // Eagerly drops entries of `packId` whose revision no longer matches the registry.
    void evictStale(std::string_view packId);
    void clear();
    std::size_t entryCount() const noexcept { return map_.size(); }
    std::size_t bytesUsed() const noexcept { return bytes_; }

private:
    struct Entry {
        RasterCacheKey key;
        render::RasterGlyph glyph;
    };
    void evictToBudget();

    const FontRegistry* registry_;
    std::size_t budget_;
    std::size_t bytes_ = 0;
    std::list<Entry> lru_; // front = most recently used
    std::unordered_map<RasterCacheKey, std::list<Entry>::iterator, RasterCacheKeyHash> map_;
    CacheStats stats_;
    render::RasterGlyph empty_;
};

} // namespace pxc::fonts
