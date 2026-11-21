// pixelcss-tool: headless developer CLI (contracts/ui-contract.md).
#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

#include "pxc/core/unicode.hpp"
#include "tool.hpp"

namespace pxc::tool {

bool Args::has(const std::string& name) const {
    for (const auto& [k, v] : options)
        if (k == name) return true;
    return false;
}

std::optional<std::string> Args::get(const std::string& name) const {
    for (const auto& [k, v] : options)
        if (k == name) return v;
    return std::nullopt;
}

Args parseArgs(const std::vector<std::string>& argv, const std::vector<std::string>& flags) {
    Args a;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        const std::string& s = argv[i];
        if (s.size() > 2 && s[0] == '-' && s[1] == '-') {
            const std::string name = s.substr(2);
            bool isFlag = false;
            for (const auto& f : flags)
                if (f == name) isFlag = true;
            if (isFlag || i + 1 >= argv.size()) a.options.emplace_back(name, "");
            else a.options.emplace_back(name, argv[++i]);
        } else {
            a.positional.push_back(s);
        }
    }
    return a;
}

std::filesystem::path exeDir() {
    wchar_t buf[MAX_PATH * 4];
    const DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
    return std::filesystem::path(std::wstring(buf, n)).parent_path();
}

std::filesystem::path defaultFontsDir() { return exeDir() / "fonts"; }

} // namespace pxc::tool

namespace {

void usage() {
    std::fputs(
        "pixelcss-tool <command> [options]\n"
        "  validate [--strict] <fonts-dir|pack-dir>\n"
        "  render --pack <id> --text <s> [--scale N] [--fg #hex] [--bg #hex] --out <file.bmp> [--fonts-dir <dir>]\n"
        "  compare <a.bmp> <b.bmp> [--diff out.bmp]\n"
        "  distinct <fonts-dir>\n"
        "  html --in <file.html> --out <file.bmp> [--width W] [--pack <id>] [--fonts-dir <dir>]\n",
        stderr);
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.push_back(pxc::core::wideToUtf8(argv[i]));
    if (args.empty()) {
        usage();
        return 2;
    }
    const std::string cmd = args.front();
    args.erase(args.begin());
    using namespace pxc::tool;
    try {
        if (cmd == "render") return cmdRender(parseArgs(args, {}));
        if (cmd == "compare") return cmdCompare(parseArgs(args, {}));
        if (cmd == "validate") return cmdValidate(parseArgs(args, {"strict"}));
        if (cmd == "distinct") return cmdDistinct(parseArgs(args, {}));
        if (cmd == "html") return cmdHtml(parseArgs(args, {}));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "pixelcss-tool: internal error: %s\n", e.what());
        return 1;
    }
    usage();
    return 2;
}
