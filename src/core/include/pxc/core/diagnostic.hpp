#pragma once
// Diagnostics shared by every parser and loader (data-model §12).

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pxc::core {

enum class Severity { Info, Warning, Error };

struct Diagnostic {
    Severity severity = Severity::Error;
    std::string code;    // e.g. "E-GLYPH-ROW-LENGTH"
    std::string message; // human readable
    std::optional<std::filesystem::path> file;
    int line = 0;
    int column = 0;
};

Diagnostic makeError(std::string code, std::string message, const std::filesystem::path* file = nullptr,
                     int line = 0, int column = 0);
Diagnostic makeWarning(std::string code, std::string message, const std::filesystem::path* file = nullptr,
                       int line = 0, int column = 0);

// "<path>:<line>:<column>: error <CODE>: <message>"
std::string format(const Diagnostic& d);
const char* severityName(Severity s) noexcept;

class DiagnosticsLog {
public:
    // Adds a diagnostic unless one with the same (code, file, line, column) is already present.
    // Returns true when it was added.
    bool add(Diagnostic d);
    void addAll(const std::vector<Diagnostic>& ds);
    std::span<const Diagnostic> all() const noexcept { return entries_; }
    void clear() noexcept { entries_.clear(); }
    std::size_t errorCount() const noexcept;
    std::size_t warningCount() const noexcept;
    std::size_t size() const noexcept { return entries_.size(); }

private:
    std::vector<Diagnostic> entries_;
};

} // namespace pxc::core
