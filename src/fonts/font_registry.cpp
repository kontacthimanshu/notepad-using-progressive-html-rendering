#include "pack_internal.hpp"
#include "engine_glyph.hpp"

#include <algorithm>
#include <cctype>

namespace pxc::fonts {

namespace fs = std::filesystem;

FontRegistry::FontRegistry(core::DiagnosticsLog& log) : log_(&log) {}
FontRegistry::~FontRegistry() = default;

void FontRegistry::loadAll(const fs::path& fontsDir) {
    std::error_code ec;
    if (!fs::is_directory(fontsDir, ec)) {
        log_->add(core::makeError("E-PACK-MANIFEST", "fonts directory not found: " + fontsDir.generic_string()));
        return;
    }
    std::vector<fs::path> dirs;
    for (const auto& e : fs::directory_iterator(fontsDir, ec))
        if (e.is_directory(ec) && fs::exists(e.path() / "manifest.css", ec)) dirs.push_back(e.path());
    std::sort(dirs.begin(), dirs.end());
    for (const auto& d : dirs) loadPack(d);
}

PackState FontRegistry::loadPack(const fs::path& packDir) {
    auto pack = loadPackFromDisk(packDir);
    // Duplicate id from a different directory?
    if (pack->state != PackState::Invalid) {
        for (const auto& p : packs_) {
            if (p->id == pack->id && fs::weakly_canonical(p->dir) != fs::weakly_canonical(packDir) &&
                p->state != PackState::Invalid) {
                const fs::path mp = packDir / "manifest.css";
                pack->diagnostics.push_back(core::makeError(
                    "E-PACK-DUP-ID", "pack id \"" + pack->id + "\" is already registered from " + p->dir.generic_string(),
                    &mp, 1, 1));
                pack->state = PackState::Invalid;
                pack->id = pack->id + "#dup:" + packDir.filename().string();
                break;
            }
        }
    }
    log_->addAll(pack->diagnostics);
    const PackState st = pack->state;
    // Replace an existing pack loaded from the same directory.
    auto it = std::find_if(packs_.begin(), packs_.end(), [&](const auto& p) { return p->dir == packDir; });
    if (it != packs_.end()) {
        pack->revision = (*it)->revision + 1;
        *it = std::move(pack);
    } else {
        packs_.push_back(std::move(pack));
    }
    return st;
}

ReloadResult FontRegistry::reload(std::string_view packId) {
    ReloadResult result;
    Pack* old = find(packId);
    if (!old) return result;
    auto fresh = loadPackFromDisk(old->dir);
    log_->addAll(fresh->diagnostics);
    // Keep unchanged definitions identical; report changed / added / removed code points.
    for (const auto& [cp, g] : fresh->glyphs) {
        auto o = old->glyphs.find(cp);
        if (o == old->glyphs.end() || o->second.contentHash != g.contentHash) result.changedCodePoints.push_back(cp);
    }
    for (const auto& [cp, g] : old->glyphs)
        if (!fresh->glyphs.count(cp)) result.changedCodePoints.push_back(cp);
    std::sort(result.changedCodePoints.begin(), result.changedCodePoints.end());
    fresh->revision = old->revision + 1;
    result.newState = fresh->state;
    *old = std::move(*fresh);
    return result;
}

FontRegistry::Pack* FontRegistry::find(std::string_view id) const {
    for (const auto& p : packs_)
        if (p->id == id) return p.get();
    return nullptr;
}

std::vector<FontPackInfo> FontRegistry::packs() const {
    std::vector<FontPackInfo> out;
    for (const auto& p : packs_) {
        FontPackInfo i;
        i.id = p->id;
        i.state = p->state;
        i.dir = p->dir;
        i.revision = p->revision;
        i.glyphCount = p->glyphs.size();
        i.coverageMissing = p->coverageMissing;
        const auto& m = p->manifest;
        i.displayName = m.displayName.empty() ? p->id : m.displayName;
        i.styleDescription = m.styleDescription;
        i.version = m.version;
        i.authenticity = m.authenticity;
        i.origin = m.origin;
        i.author = m.author;
        i.license = m.license;
        i.aliases = m.aliases;
        i.order = m.order;
        i.coverageDeclared = m.coverageCount();
        out.push_back(std::move(i));
    }
    std::stable_sort(out.begin(), out.end(), [](const FontPackInfo& a, const FontPackInfo& b) {
        const bool ua = a.state != PackState::Invalid, ub = b.state != PackState::Invalid;
        if (ua != ub) return ua;
        if (a.order != b.order) return a.order < b.order;
        return a.id < b.id;
    });
    return out;
}

std::optional<FontPackInfo> FontRegistry::pack(std::string_view id) const {
    for (auto& i : packs())
        if (i.id == id) return i;
    return std::nullopt;
}

bool FontRegistry::isUsable(std::string_view id) const {
    const Pack* p = find(id);
    return p && (p->state == PackState::Valid || p->state == PackState::Degraded);
}

std::uint32_t FontRegistry::packRevision(std::string_view id) const {
    const Pack* p = find(id);
    return p ? p->revision : 0;
}

FontMetrics FontRegistry::metrics(std::string_view packId) const {
    const Pack* p = find(packId);
    if (!p || !isUsable(packId)) {
        if (packId != kDefaultPackId && isUsable(kDefaultPackId)) return metrics(kDefaultPackId);
        return FontMetrics{};
    }
    FontMetrics fm;
    fm.lineHeight = p->manifest.lineHeight;
    fm.ascent = p->manifest.ascent;
    fm.descent = p->manifest.descent;
    auto sp = p->glyphs.find(U' ');
    fm.spaceAdvance = sp != p->glyphs.end() ? std::max(1, sp->second.advance) : std::max(1, fm.lineHeight / 4);
    return fm;
}

namespace {
std::string cpName(char32_t cp) {
    static constexpr char d[] = "0123456789ABCDEF";
    std::string s;
    for (int shift = (cp > 0xFFFF ? 20 : 12); shift >= 0; shift -= 4) s += d[(cp >> shift) & 0xF];
    return "U+" + s;
}
} // namespace

void FontRegistry::reportOnce(const char* code, const std::string& key, const std::string& message) const {
    if (!reported_.insert(std::string(code) + "|" + key).second) return;
    log_->add(core::makeWarning(code, message));
}

ResolvedGlyph FontRegistry::resolve(std::string_view packId, char32_t cp) const {
    ResolvedGlyph r;
    const Pack* p = find(packId);
    if (!p || !isUsable(packId)) {
        // Unavailable pack: fall back to the default family, then to the engine box font (research R-07).
        if (packId != kDefaultPackId && isUsable(kDefaultPackId)) {
            reportOnce("W-FONT-PACK-UNAVAILABLE", std::string(packId),
                       "font pack \"" + std::string(packId) + "\" is unavailable; using " + std::string(kDefaultPackId));
            return resolve(kDefaultPackId, cp);
        }
        reportOnce("W-GLYPH-FALLBACK-ENGINE", std::string(packId) + "|*",
                   "no usable font pack for \"" + std::string(packId) + "\"; using the engine box font");
        r.def = engineReplacement("engine-box");
        r.source = GlyphSource::EngineReplacement;
        r.packUsed = "engine-box";
        return r;
    }
    if (auto it = p->glyphs.find(cp); it != p->glyphs.end()) {
        r.def = &it->second;
        r.source = GlyphSource::Primary;
        r.packUsed = p->id;
        return r;
    }
    if (cp == U'\t' || cp == U'\n') return r; // layout handles control characters; never a fallback glyph
    if (auto rep = p->glyphs.find(p->manifest.replacementGlyph); rep != p->glyphs.end()) {
        reportOnce("W-GLYPH-FALLBACK-PACK-REPLACEMENT", p->id + "|" + cpName(cp),
                   p->id + ": " + cpName(cp) + " has no glyph; showing the pack replacement glyph");
        r.def = &rep->second;
        r.source = GlyphSource::PackReplacement;
        r.packUsed = p->id;
        return r;
    }
    reportOnce("W-GLYPH-FALLBACK-ENGINE", p->id + "|" + cpName(cp),
               p->id + ": " + cpName(cp) + " has no glyph and the pack has no replacement glyph; showing the engine box");
    r.def = engineReplacement(p->id);
    r.source = GlyphSource::EngineReplacement;
    r.packUsed = p->id;
    return r;
}

std::string FontRegistry::packForAlias(std::string_view alias) const {
    auto lower = [](std::string_view s) {
        std::string o(s);
        for (char& c : o) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return o;
    };
    const std::string a = lower(alias);
    for (const auto& info : packs()) {
        if (info.state == PackState::Invalid) continue;
        if (lower(info.id) == a) return info.id;
        for (const auto& al : info.aliases)
            if (lower(al) == a) return info.id;
    }
    return {};
}

std::vector<const glyphcss::GlyphDefinition*> FontRegistry::glyphs(std::string_view packId) const {
    std::vector<const glyphcss::GlyphDefinition*> out;
    if (const Pack* p = find(packId))
        for (const auto& [cp, g] : p->glyphs) out.push_back(&g);
    return out;
}

const glyphcss::FontManifest* FontRegistry::manifest(std::string_view packId) const {
    const Pack* p = find(packId);
    return p && p->state != PackState::Invalid ? &p->manifest : nullptr;
}

std::vector<core::Diagnostic> FontRegistry::packDiagnostics(std::string_view packId) const {
    const Pack* p = find(packId);
    return p ? p->diagnostics : std::vector<core::Diagnostic>{};
}

const glyphcss::GlyphDefinition* FontRegistry::engineReplacement(std::string_view packId) const {
    auto it = engineGlyphs_.find(packId);
    if (it == engineGlyphs_.end()) {
        const FontMetrics m = find(packId) ? metrics(packId) : FontMetrics{};
        auto g = std::make_unique<glyphcss::GlyphDefinition>(makeEngineReplacement(std::string(packId), m));
        it = engineGlyphs_.emplace(std::string(packId), std::move(g)).first;
    }
    return it->second.get();
}

} // namespace pxc::fonts
