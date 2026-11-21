#include "pxc/core/diagnostic.hpp"

#include <algorithm>

namespace pxc::core {

namespace {
Diagnostic make(Severity s, std::string code, std::string message, const std::filesystem::path* file, int line,
                int column) {
    Diagnostic d;
    d.severity = s;
    d.code = std::move(code);
    d.message = std::move(message);
    if (file) d.file = *file;
    d.line = line;
    d.column = column;
    return d;
}
} // namespace

Diagnostic makeError(std::string code, std::string message, const std::filesystem::path* file, int line,
                     int column) {
    return make(Severity::Error, std::move(code), std::move(message), file, line, column);
}

Diagnostic makeWarning(std::string code, std::string message, const std::filesystem::path* file, int line,
                       int column) {
    return make(Severity::Warning, std::move(code), std::move(message), file, line, column);
}

const char* severityName(Severity s) noexcept {
    switch (s) {
    case Severity::Info: return "info";
    case Severity::Warning: return "warning";
    case Severity::Error: return "error";
    }
    return "error";
}

std::string format(const Diagnostic& d) {
    std::string out;
    if (d.file) {
        out += d.file->generic_string();
        out += ':';
        out += std::to_string(d.line);
        out += ':';
        out += std::to_string(d.column);
        out += ": ";
    }
    out += severityName(d.severity);
    out += ' ';
    out += d.code;
    out += ": ";
    out += d.message;
    return out;
}

bool DiagnosticsLog::add(Diagnostic d) {
    const bool dup = std::any_of(entries_.begin(), entries_.end(), [&](const Diagnostic& e) {
        return e.code == d.code && e.file == d.file && e.line == d.line && e.column == d.column &&
               e.message == d.message;
    });
    if (dup) return false;
    entries_.push_back(std::move(d));
    return true;
}

void DiagnosticsLog::addAll(const std::vector<Diagnostic>& ds) {
    for (const auto& d : ds) add(d);
}

std::size_t DiagnosticsLog::errorCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(),
                                                  [](const Diagnostic& d) { return d.severity == Severity::Error; }));
}

std::size_t DiagnosticsLog::warningCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        entries_.begin(), entries_.end(), [](const Diagnostic& d) { return d.severity == Severity::Warning; }));
}

} // namespace pxc::core
