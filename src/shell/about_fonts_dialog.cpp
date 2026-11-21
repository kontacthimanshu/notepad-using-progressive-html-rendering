// Help → About Fonts: provenance and authenticity of every pack (FR-013, US3-AS6).
#include "app.hpp"

#include "pxc/core/unicode.hpp"

namespace pxc::shell {

void showAboutFonts() {
    auto& a = app();
    std::vector<std::vector<std::wstring>> rows;
    for (const auto& p : a.registry->packs()) {
        const std::wstring auth = p.authenticity == "style-interpretation"
                                      ? L"Style interpretation — NOT the original typeface"
                                      : core::utf8ToWide(p.authenticity);
        rows.push_back({core::utf8ToWide(p.displayName), core::utf8ToWide(p.id), core::utf8ToWide(p.version),
                        core::utf8ToWide(p.author), core::utf8ToWide(p.license),
                        core::utf8ToWide(fonts::packStateName(p.state)), core::utf8ToWide(p.origin), auth});
    }
    showInfoTable(L"About Fonts",
                  {L"Font", L"Pack id", L"Version", L"Author", L"License", L"State", L"Origin", L"Authenticity"},
                  rows,
                  L"The bundled Verdana, Arial Narrow and Comic Sans entries are original pixel artwork drawn in the "
                  L"style of those typefaces. They are not the proprietary fonts and contain no data derived from them.");
}

} // namespace pxc::shell
