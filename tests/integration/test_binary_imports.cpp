// The shipped executable must not depend on a browser engine, a network stack or OS text rendering for document
// text (FR-014, FR-033, FR-040, FR-049, SC-011). The PE import directory (normal and delay-load) is parsed by hand.
#include <doctest.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>

#include "support/fs_util.hpp"

namespace {

struct Imports {
    std::set<std::string> dlls;      // lower-case
    std::set<std::string> functions; // as named
};

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

template <class T> T rd(const std::string& b, std::size_t off) {
    T v{};
    if (off + sizeof(T) <= b.size()) std::memcpy(&v, b.data() + off, sizeof(T));
    return v;
}

Imports parseImports(const std::string& pe) {
    Imports out;
    REQUIRE(pe.size() > 0x40);
    REQUIRE(pe[0] == 'M');
    REQUIRE(pe[1] == 'Z');
    const auto nt = rd<std::uint32_t>(pe, 0x3C);
    REQUIRE(rd<std::uint32_t>(pe, nt) == 0x00004550); // "PE\0\0"
    const std::size_t fileHeader = nt + 4;
    const auto numSections = rd<std::uint16_t>(pe, fileHeader + 2);
    const auto optSize = rd<std::uint16_t>(pe, fileHeader + 16);
    const std::size_t opt = fileHeader + 20;
    REQUIRE(rd<std::uint16_t>(pe, opt) == 0x20B); // PE32+ (x64)
    const std::size_t dataDirs = opt + 112;
    const auto importRva = rd<std::uint32_t>(pe, dataDirs + 1 * 8);
    const auto delayRva = rd<std::uint32_t>(pe, dataDirs + 13 * 8);
    const std::size_t sections = opt + optSize;

    auto rvaToOff = [&](std::uint32_t rva) -> std::size_t {
        for (std::uint16_t i = 0; i < numSections; ++i) {
            const std::size_t sh = sections + i * 40u;
            const auto va = rd<std::uint32_t>(pe, sh + 12);
            const auto vsize = std::max(rd<std::uint32_t>(pe, sh + 8), rd<std::uint32_t>(pe, sh + 16));
            const auto raw = rd<std::uint32_t>(pe, sh + 20);
            if (rva >= va && rva < va + vsize) return raw + (rva - va);
        }
        return 0;
    };
    auto cstr = [&](std::uint32_t rva) {
        const std::size_t o = rvaToOff(rva);
        std::string s;
        for (std::size_t i = o; o && i < pe.size() && pe[i]; ++i) s += pe[i];
        return s;
    };
    auto walkThunks = [&](std::uint32_t thunkRva) {
        for (std::size_t t = rvaToOff(thunkRva); t && t + 8 <= pe.size(); t += 8) {
            const auto v = rd<std::uint64_t>(pe, t);
            if (v == 0) break;
            if (v & (1ull << 63)) continue; // import by ordinal
            out.functions.insert(cstr(static_cast<std::uint32_t>(v) + 2));
        }
    };
    if (importRva) {
        for (std::size_t d = rvaToOff(importRva); d && d + 20 <= pe.size(); d += 20) {
            const auto nameRva = rd<std::uint32_t>(pe, d + 12);
            if (nameRva == 0) break;
            out.dlls.insert(lower(cstr(nameRva)));
            const auto ilt = rd<std::uint32_t>(pe, d);
            walkThunks(ilt ? ilt : rd<std::uint32_t>(pe, d + 16));
        }
    }
    if (delayRva) {
        for (std::size_t d = rvaToOff(delayRva); d && d + 32 <= pe.size(); d += 32) {
            const auto nameRva = rd<std::uint32_t>(pe, d + 4);
            if (nameRva == 0) break;
            out.dlls.insert(lower(cstr(nameRva)));
            walkThunks(rd<std::uint32_t>(pe, d + 16));
        }
    }
    return out;
}

} // namespace

TEST_CASE("PixelCSS.exe imports no browser engine, network stack or text-rendering API") {
    const auto imports = parseImports(pxc::test::readAll(PXC_APP_EXE));
    REQUIRE_FALSE(imports.dlls.empty());
    CHECK(imports.dlls.count("gdi32.dll") == 1);  // pixel BitBlt presentation is expected
    CHECK(imports.dlls.count("user32.dll") == 1);
    for (const char* banned : {"winhttp.dll", "wininet.dll", "ws2_32.dll", "urlmon.dll", "webio.dll",
                               "webview2loader.dll", "dwrite.dll", "d2d1.dll", "usp10.dll"}) {
        CAPTURE(banned);
        CHECK(imports.dlls.count(banned) == 0);
    }
    for (const char* fn : {"DrawTextW", "DrawTextA", "DrawTextExW", "DrawTextExA", "TextOutW", "TextOutA",
                           "ExtTextOutW", "ExtTextOutA", "PolyTextOutW", "PolyTextOutA", "TabbedTextOutW",
                           "TabbedTextOutA", "DWriteCreateFactory"}) {
        CAPTURE(fn);
        CHECK(imports.functions.count(fn) == 0);
    }
    CHECK(imports.functions.count("BitBlt") == 1);
}
