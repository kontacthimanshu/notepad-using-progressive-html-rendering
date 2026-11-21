#pragma once
// Display preferences (research R-20): last font pack and word wrap. Portable; no Win32.

#include <functional>
#include <string>
#include <string_view>

#include "pxc/core/diagnostic.hpp"

namespace pxc::storage {

struct Preferences {
    std::string lastPackId = "verdana-style";
    bool wordWrap = true;
    bool operator==(const Preferences&) const = default;
};

struct IPreferencesStore {
    virtual Preferences load(core::DiagnosticsLog& log) = 0;
    virtual bool save(const Preferences& p) = 0; // false on failure, never throws
    virtual ~IPreferencesStore() = default;
};

// Production store for --no-preferences: defaults on load, no-op save.
struct NullPreferencesStore final : IPreferencesStore {
    Preferences load(core::DiagnosticsLog&) override { return {}; }
    bool save(const Preferences&) override { return true; }
};

// The pack to activate at startup: the stored one if it is usable, otherwise verdana-style.
inline std::string resolveStartupPack(const Preferences& p, const std::function<bool(std::string_view)>& usable) {
    if (!p.lastPackId.empty() && usable(p.lastPackId)) return p.lastPackId;
    return "verdana-style";
}

} // namespace pxc::storage
