#pragma once
// In-memory IPreferencesStore test double.

#include "pxc/storage/preferences.hpp"

namespace pxc::test {

class MemoryPreferencesStore final : public storage::IPreferencesStore {
public:
    std::optional<storage::Preferences> stored;
    int saves = 0;
    storage::Preferences load(core::DiagnosticsLog&) override { return stored.value_or(storage::Preferences{}); }
    bool save(const storage::Preferences& p) override {
        stored = p;
        ++saves;
        return true;
    }
};

} // namespace pxc::test
