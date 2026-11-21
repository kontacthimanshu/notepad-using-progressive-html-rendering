// Help → Diagnostics: every recorded diagnostic with severity, code, message and location (FR-034).
#include "app.hpp"

#include "pxc/core/unicode.hpp"

namespace pxc::shell {

void showDiagnostics() {
    auto& a = app();
    std::vector<std::vector<std::wstring>> rows;
    for (const auto& d : a.log.all()) {
        std::wstring where;
        if (d.file) where = d.file->wstring() + L":" + std::to_wstring(d.line) + L":" + std::to_wstring(d.column);
        rows.push_back({core::utf8ToWide(core::severityName(d.severity)), core::utf8ToWide(d.code),
                        core::utf8ToWide(d.message), where});
    }
    const std::wstring footer = rows.empty() ? L"No problems were recorded."
                                             : std::to_wstring(a.log.errorCount()) + L" error(s), " +
                                                   std::to_wstring(a.log.warningCount()) + L" warning(s).";
    showInfoTable(L"Diagnostics", {L"Severity", L"Code", L"Message", L"Location"}, rows, footer);
}

} // namespace pxc::shell
