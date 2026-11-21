#pragma once
// Shared helpers for pixelcss-tool subcommands.

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace pxc::tool {

struct Args {
    std::vector<std::string> positional;
    std::vector<std::pair<std::string, std::string>> options; // --name value  (flags have value "")
    bool has(const std::string& name) const;
    std::optional<std::string> get(const std::string& name) const;
};

// Flags that take no value.
Args parseArgs(const std::vector<std::string>& argv, const std::vector<std::string>& flags);

std::filesystem::path exeDir();
std::filesystem::path defaultFontsDir();

int cmdRender(const Args& a);
int cmdCompare(const Args& a);
int cmdValidate(const Args& a);
int cmdDistinct(const Args& a);
int cmdHtml(const Args& a);

} // namespace pxc::tool
