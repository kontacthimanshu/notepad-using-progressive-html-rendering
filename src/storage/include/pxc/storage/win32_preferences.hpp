#pragma once
// HKCU-backed preferences store: <baseKey>\Display { LastPack: REG_SZ, WordWrap: REG_DWORD }.

#include <string>

#include "pxc/storage/preferences.hpp"

namespace pxc::storage {

class RegistryPreferencesStore final : public IPreferencesStore {
public:
    explicit RegistryPreferencesStore(std::wstring baseKey = L"Software\\PixelCSS") : base_(std::move(baseKey)) {}
    Preferences load(core::DiagnosticsLog& log) override;
    bool save(const Preferences& p) override;

private:
    std::wstring base_;
};

} // namespace pxc::storage
