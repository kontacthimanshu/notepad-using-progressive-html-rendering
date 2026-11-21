// Contract test for display-preference persistence (research R-20, Constitution IX).
#include <doctest.h>

#include <windows.h>

#include <random>

#include "pxc/storage/preferences.hpp"
#include "pxc/storage/win32_preferences.hpp"
#include "support/memory_preferences.hpp"

using namespace pxc;
using namespace pxc::storage;

namespace {
std::wstring testKey() {
    std::random_device rd;
    return L"Software\\PixelCSS-Test-" + std::to_wstring(rd()) + std::to_wstring(rd());
}
struct KeyCleanup {
    std::wstring key;
    ~KeyCleanup() { RegDeleteTreeW(HKEY_CURRENT_USER, key.c_str()); RegDeleteKeyW(HKEY_CURRENT_USER, key.c_str()); }
};
bool keyExists(const std::wstring& k) {
    HKEY h = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, k.c_str(), 0, KEY_READ, &h) != ERROR_SUCCESS) return false;
    RegCloseKey(h);
    return true;
}
} // namespace

TEST_CASE("defaults on first run") {
    core::DiagnosticsLog log;
    test::MemoryPreferencesStore mem;
    const auto p = mem.load(log);
    CHECK(p.lastPackId == "verdana-style");
    CHECK(p.wordWrap);
    KeyCleanup k{testKey()};
    RegistryPreferencesStore reg(k.key);
    const auto r = reg.load(log);
    CHECK(r.lastPackId == "verdana-style");
    CHECK(r.wordWrap);
    CHECK(log.size() == 0);
}

TEST_CASE("save then load round-trips through the registry") {
    core::DiagnosticsLog log;
    KeyCleanup k{testKey()};
    RegistryPreferencesStore reg(k.key);
    REQUIRE(reg.save(Preferences{"comic-style", false}));
    const auto r = reg.load(log);
    CHECK(r.lastPackId == "comic-style");
    CHECK_FALSE(r.wordWrap);
}

TEST_CASE("a corrupt value falls back to defaults with a diagnostic") {
    core::DiagnosticsLog log;
    KeyCleanup k{testKey()};
    const std::wstring display = k.key + L"\\Display";
    HKEY h = nullptr;
    REQUIRE(RegCreateKeyExW(HKEY_CURRENT_USER, display.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &h, nullptr) == ERROR_SUCCESS);
    const DWORD wrongType = 7;
    RegSetValueExW(h, L"LastPack", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&wrongType), sizeof(wrongType));
    const wchar_t* notDword = L"yes";
    RegSetValueExW(h, L"WordWrap", 0, REG_SZ, reinterpret_cast<const BYTE*>(notDword), 8);
    RegCloseKey(h);
    RegistryPreferencesStore reg(k.key);
    const auto r = reg.load(log);
    CHECK(r.lastPackId == "verdana-style");
    CHECK(r.wordWrap);
    CHECK(log.size() >= 1);
}

TEST_CASE("a stored pack that no longer exists falls back to Verdana-style") {
    test::MemoryPreferencesStore mem;
    mem.stored = Preferences{"deleted-pack", true};
    core::DiagnosticsLog log;
    const auto p = mem.load(log);
    CHECK(resolveStartupPack(p, [](std::string_view id) { return id == "verdana-style" || id == "comic-style"; }) ==
          "verdana-style");
    CHECK(resolveStartupPack(Preferences{"comic-style", true},
                             [](std::string_view id) { return id == "verdana-style" || id == "comic-style"; }) ==
          "comic-style");
}

TEST_CASE("NullPreferencesStore returns defaults and writes nothing") {
    core::DiagnosticsLog log;
    NullPreferencesStore null;
    const auto p = null.load(log);
    CHECK(p.lastPackId == "verdana-style");
    CHECK(p.wordWrap);
    CHECK(log.size() == 0);
    CHECK(null.save(Preferences{"comic-style", false}));
    CHECK_FALSE(keyExists(L"Software\\PixelCSS-Null-Should-Not-Exist"));
}
