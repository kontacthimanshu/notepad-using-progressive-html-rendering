#include "pxc/fonts/raster_cache.hpp"

#include <functional>

#include "pxc/fonts/rasterize.hpp"

namespace pxc::fonts {

std::size_t RasterCacheKeyHash::operator()(const RasterCacheKey& k) const noexcept {
    std::size_t h = std::hash<std::string>{}(k.packId);
    auto mix = [&](std::uint64_t v) { h ^= std::hash<std::uint64_t>{}(v) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2); };
    mix(std::hash<std::string>{}(k.style));
    mix(k.packRevision);
    mix(k.codePoint);
    mix(k.glyphRevision);
    mix(static_cast<std::uint64_t>(k.scale));
    mix((static_cast<std::uint64_t>(k.fg) << 32) | k.bg);
    return h;
}

RasterCache::RasterCache(const FontRegistry& registry, std::size_t budgetBytes)
    : registry_(&registry), budget_(budgetBytes) {}

const render::RasterGlyph& RasterCache::get(std::string_view packId, char32_t cp, int scale, core::Color fg,
                                            core::Color bg) {
    const ResolvedGlyph rg = registry_->resolve(packId, cp);
    if (!rg.def) {
        ++stats_.misses;
        return empty_;
    }
    RasterCacheKey key{rg.packUsed, rg.def->style, registry_->packRevision(rg.packUsed), cp, rg.def->contentHash,
                       scale, fg.toBGRA(), bg.toBGRA()};
    if (rg.source == GlyphSource::EngineReplacement) key.packRevision = 0;
    auto it = map_.find(key);
    if (it != map_.end()) {
        ++stats_.hits;
        lru_.splice(lru_.begin(), lru_, it->second);
        return it->second->glyph;
    }
    ++stats_.misses;
    ++stats_.rasterizations;
    lru_.push_front(Entry{key, rasterize(*rg.def, scale, fg, bg)});
    bytes_ += lru_.front().glyph.byteSize();
    map_.emplace(std::move(key), lru_.begin());
    evictToBudget();
    return lru_.front().glyph;
}

void RasterCache::evictToBudget() {
    while (bytes_ > budget_ && lru_.size() > 1) {
        auto& back = lru_.back();
        bytes_ -= back.glyph.byteSize();
        map_.erase(back.key);
        lru_.pop_back();
        ++stats_.evictions;
    }
}

void RasterCache::evictStale(std::string_view packId) {
    const std::uint32_t rev = registry_->packRevision(packId);
    for (auto it = lru_.begin(); it != lru_.end();) {
        if (it->key.packId == packId && it->key.packRevision != rev) {
            const ResolvedGlyph rg = registry_->resolve(packId, it->key.codePoint);
            if (!rg.def || rg.def->contentHash != it->key.glyphRevision) {
                bytes_ -= it->glyph.byteSize();
                map_.erase(it->key);
                it = lru_.erase(it);
                ++stats_.evictions;
                continue;
            }
            // Unchanged glyph: re-key to the new revision so it stays a cache hit.
            RasterCacheKey nk = it->key;
            nk.packRevision = rev;
            map_.erase(it->key);
            it->key = nk;
            map_.emplace(nk, it);
        }
        ++it;
    }
}

void RasterCache::clear() {
    lru_.clear();
    map_.clear();
    bytes_ = 0;
}

} // namespace pxc::fonts
