// pixelcss-bench: reproducible performance scenarios (research R-17) and the perf regression gate.
//
//   pixelcss-bench --generate <out> --chars N --seed S [--single-line]
//   pixelcss-bench --fonts-dir <dir> --fixtures <dir> [--app <PixelCSS.exe>] [--scripts <dir>] --out <file.json>
//                  [--check <baseline.json>]
//   pixelcss-bench --scenario memory --chars N --fonts-dir <dir> --out <file.json>     (child process)
//
// Absolute budgets are enforced only when PIXELCSS_REFERENCE_MACHINE=1; the 20% regression check needs a
// measured baseline ("measured": true); resource growth is always enforced.
#include <windows.h>
#include <psapi.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "pxc/core/unicode.hpp"
#include "pxc/document/text_document.hpp"
#include "pxc/editor/editor_controller.hpp"
#include "pxc/fonts/font_registry.hpp"
#include "pxc/fonts/raster_cache.hpp"
#include "pxc/fonts/rasterize.hpp"

namespace fs = std::filesystem;
using namespace pxc;
using Clock = std::chrono::steady_clock;

namespace {

double ms(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }
double us(Clock::duration d) { return std::chrono::duration<double, std::micro>(d).count(); }

double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t i = std::min(v.size() - 1, static_cast<std::size_t>(p * static_cast<double>(v.size() - 1) + 0.5));
    return v[i];
}

std::string readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ---- deterministic document generator ----
std::string generate(std::size_t chars, unsigned seed, bool singleLine) {
    std::mt19937 rng(seed);
    static const char* words[] = {"the", "quick", "brown", "fox", "jumps", "over", "lazy", "dog", "pixel", "glyph",
                                  "css", "render", "layout", "cache", "viewport", "caret", "wrap", "line", "font", "pack"};
    std::string out;
    std::size_t lineLen = 0, lineTarget = 20 + rng() % 100;
    while (out.size() < chars) {
        out += words[rng() % std::size(words)];
        lineLen = out.size() - (out.rfind('\n') == std::string::npos ? 0 : out.rfind('\n') + 1);
        if (!singleLine && lineLen >= lineTarget) {
            out += '\n';
            lineTarget = 20 + rng() % 100;
        } else {
            out += ' ';
        }
    }
    return out;
}

struct NullClipboard final : editor::IClipboard {
    std::optional<std::u32string> getText() override { return std::nullopt; }
    bool setText(std::u32string_view) override { return true; }
};

struct Headless {
    core::DiagnosticsLog log;
    fonts::FontRegistry registry{log};
    std::unique_ptr<fonts::RasterCache> cache;
    document::TextDocument doc;
    std::unique_ptr<layout::LayoutEngine> layout;
    layout::Viewport viewport;
    std::unique_ptr<update::UpdateManager> updates;
    NullClipboard clipboard;
    std::unique_ptr<editor::EditorController> editor;
    render::Framebuffer fb;

    Headless(const fs::path& fontsDir, int w, int h, bool wrap) {
        registry.loadAll(fontsDir);
        cache = std::make_unique<fonts::RasterCache>(registry);
        layout = std::make_unique<layout::LayoutEngine>(registry);
        viewport.clientWidth = w;
        viewport.clientHeight = h;
        updates = std::make_unique<update::UpdateManager>(doc, *layout, *cache, viewport);
        editor = std::make_unique<editor::EditorController>(doc, *layout, viewport, *updates, clipboard);
        if (!wrap) editor->handle(editor::cmd::ToggleWrap{});
        editor->applyLayoutParams(1);
        fb = render::Framebuffer(w, h);
        updates->onResize(w, h);
    }
    update::RenderStats paint() { return updates->paint(fb, editor->paintState(), nullptr); }
};

std::u32string decode(const std::string& s) { return *core::decodeUtf8(s, core::InvalidPolicy::Replace); }

unsigned long long workingSet() {
    PROCESS_MEMORY_COUNTERS pmc{sizeof(pmc)};
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    return pmc.WorkingSetSize;
}

// ---- e2e helpers ----
struct StatsFile {
    std::vector<long long> keyToPresent;
    long long firstPresentQpc = 0;
    std::vector<std::map<std::string, long long>> samples;
};

StatsFile parseStats(const fs::path& p) {
    StatsFile s;
    std::ifstream f(p);
    std::string line;
    const std::regex num("\"(\\w+)\":(-?\\d+)");
    while (std::getline(f, line)) {
        std::map<std::string, long long> kv;
        for (auto it = std::sregex_iterator(line.begin(), line.end(), num); it != std::sregex_iterator(); ++it)
            kv[(*it)[1]] = std::stoll((*it)[2]);
        if (line.find("\"frame\"") != std::string::npos && kv.count("keyToPresentMicros") && kv["keyToPresentMicros"] >= 0)
            s.keyToPresent.push_back(kv["keyToPresentMicros"]);
        if (line.find("first-present") != std::string::npos && kv.count("firstPresentQpc")) s.firstPresentQpc = kv["firstPresentQpc"];
        if (line.find("\"sample\"") != std::string::npos && line.find("\"script\"") != std::string::npos) s.samples.push_back(kv);
    }
    return s;
}

bool runProcess(const std::wstring& cmdLine, DWORD timeoutMs, long long* startQpc = nullptr) {
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    std::wstring cl = cmdLine;
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    if (startQpc) *startQpc = q.QuadPart;
    if (!CreateProcessW(nullptr, cl.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return false;
    const DWORD w = WaitForSingleObject(pi.hProcess, timeoutMs);
    if (w != WAIT_OBJECT_0) TerminateProcess(pi.hProcess, 1);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return w == WAIT_OBJECT_0 && code == 0;
}

std::wstring quote(const fs::path& p) { return L"\"" + p.wstring() + L"\""; }

} // namespace

int wmain(int argc, wchar_t** argv) {
    std::map<std::string, std::string> opt;
    std::vector<std::string> flags;
    for (int i = 1; i < argc; ++i) {
        std::string a = core::wideToUtf8(argv[i]);
        if (a.rfind("--", 0) == 0) {
            const std::string k = a.substr(2);
            if (k == "single-line") { flags.push_back(k); continue; }
            opt[k] = i + 1 < argc ? core::wideToUtf8(argv[++i]) : "";
        }
    }
    auto has = [&](const char* k) { return opt.count(k) > 0; };

    // --generate
    if (has("generate")) {
        const std::size_t chars = std::stoull(opt.count("chars") ? opt["chars"] : "100000");
        const unsigned seed = static_cast<unsigned>(std::stoul(opt.count("seed") ? opt["seed"] : "1"));
        const bool single = std::find(flags.begin(), flags.end(), "single-line") != flags.end();
        std::ofstream f(opt["generate"], std::ios::binary | std::ios::trunc);
        const std::string s = generate(chars, seed, single);
        f.write(s.data(), static_cast<std::streamsize>(s.size()));
        std::printf("generated %zu bytes\n", s.size());
        return 0;
    }

    const fs::path fontsDir = has("fonts-dir") ? fs::path(opt["fonts-dir"]) : fs::path("fonts");
    const fs::path out = has("out") ? fs::path(opt["out"]) : fs::path("bench.json");

    // Child: memory scenario (isolated process so earlier scenarios do not pollute the working set).
    if (opt["scenario"] == "memory") {
        const std::size_t chars = std::stoull(opt["chars"]);
        Headless h(fontsDir, 1280, 800, true);
        const std::string text = generate(chars, 7, false);
        h.editor->loadText(decode(text));
        h.paint();
        while (h.layout->layoutIdleSlice(h.doc, std::chrono::milliseconds(50))) {}
        std::ofstream f(out, std::ios::trunc);
        f << "{\"workingSetMB\":" << static_cast<double>(workingSet()) / (1024.0 * 1024.0) << "}\n";
        return 0;
    }

    const fs::path fixtures = has("fixtures") ? fs::path(opt["fixtures"]) : fs::path("tests/fixtures");
    std::map<std::string, double> m;
    std::uint64_t hits = 0, misses = 0;
    auto addHits = [&](const update::RenderStats& st) { hits += st.cacheHits; misses += st.cacheMisses; };
    const std::string text100k = readFile(fixtures / "perf/100k.txt");

    // first-viewport (decode -> load -> first paint)
    {
        std::vector<double> runs;
        for (int r = 0; r < 5; ++r) {
            Headless h(fontsDir, 1280, 800, true);
            const auto t0 = Clock::now();
            h.editor->loadText(decode(text100k));
            h.paint();
            runs.push_back(ms(Clock::now() - t0));
        }
        m["firstViewportMs"] = percentile(runs, 0.5);
    }
    // stable-keystroke and non-reflow layout cost
    {
        Headless h(fontsDir, 1280, 800, false);
        h.editor->loadText(decode(text100k));
        h.paint();
        h.editor->handle(editor::cmd::Move{editor::MoveKind::Down});
        h.editor->handle(editor::cmd::Move{editor::MoveKind::End});
        h.paint();
        std::vector<double> total, layoutOnly;
        for (int i = 0; i < 1000; ++i) {
            const auto t0 = Clock::now();
            h.editor->handle(editor::cmd::Type{U"k"});
            const auto t1 = Clock::now();
            addHits(h.paint());
            total.push_back(ms(Clock::now() - t0));
            layoutOnly.push_back(ms(t1 - t0));
        }
        m["stableKeystrokeP50Ms"] = percentile(total, 0.5);
        m["stableKeystrokeP95Ms"] = percentile(total, 0.95);
        m["nonReflowEditLayoutP95Ms"] = percentile(layoutOnly, 0.95);
    }
    // reflow-keystroke (wrapped paragraph, insert in the middle)
    {
        Headless h(fontsDir, 600, 800, true);
        h.editor->loadText(decode(text100k));
        h.paint();
        for (int i = 0; i < 3; ++i) h.editor->handle(editor::cmd::Move{editor::MoveKind::Down});
        for (int i = 0; i < 10; ++i) h.editor->handle(editor::cmd::Move{editor::MoveKind::Right});
        h.paint();
        std::vector<double> v;
        for (int i = 0; i < 500; ++i) {
            const auto t0 = Clock::now();
            h.editor->handle(editor::cmd::Type{i % 6 == 5 ? U" " : U"r"});
            addHits(h.paint());
            v.push_back(ms(Clock::now() - t0));
        }
        m["reflowKeystrokeP95Ms"] = percentile(v, 0.95);
    }
    // scroll (500 page-downs, incremental pixel shift + exposed band)
    {
        Headless h(fontsDir, 1280, 800, true);
        h.editor->loadText(decode(text100k));
        h.paint();
        std::vector<double> v;
        for (int i = 0; i < 500; ++i) {
            const auto t0 = Clock::now();
            const auto oldY = h.viewport.scrollY(*h.layout);
            h.viewport.scrollBy(*h.layout, h.viewport.clientHeight / 2);
            h.updates->onScroll(oldY, h.viewport.scrollX);
            addHits(h.paint());
            v.push_back(ms(Clock::now() - t0));
        }
        m["scrollFrameP95Ms"] = percentile(v, 0.95);
    }
    // font-switch
    {
        Headless h(fontsDir, 1280, 800, true);
        h.editor->loadText(decode(text100k));
        h.paint();
        const char* packs[] = {"arial-narrow-style", "comic-style", "verdana-style"};
        std::vector<double> v;
        for (int i = 0; i < 150; ++i) {
            const auto t0 = Clock::now();
            h.editor->handle(editor::cmd::SetFont{packs[i % 3]});
            h.paint();
            v.push_back(ms(Clock::now() - t0));
        }
        m["fontSwitchP95Ms"] = percentile(v, 0.95);
    }
    // raster-miss
    {
        core::DiagnosticsLog log;
        fonts::FontRegistry reg(log);
        reg.loadAll(fontsDir);
        std::vector<double> v;
        for (const char* pack : {"verdana-style", "arial-narrow-style", "comic-style"})
            for (const auto* g : reg.glyphs(pack)) {
                const auto t0 = Clock::now();
                auto r = fonts::rasterize(*g, 2, core::kBlack, core::kWhite);
                v.push_back(us(Clock::now() - t0));
                (void)r;
            }
        m["rasterMissP95Us"] = percentile(v, 0.95);
    }
    // buffer-edit at 100k and 1M characters
    for (std::size_t n : {std::size_t{100000}, std::size_t{1000000}}) {
        document::TextDocument d;
        d.setText(decode(generate(n, 3, false)));
        std::mt19937 rng(9);
        std::vector<double> v;
        for (int i = 0; i < 2000; ++i) {
            const std::size_t at = rng() % d.length();
            const auto t0 = Clock::now();
            if (i % 3 == 2) d.erase(at, 1);
            else d.insert(at, U"e");
            v.push_back(us(Clock::now() - t0));
        }
        m[n == 100000 ? "bufferEditP95Us100k" : "bufferEditP95Us1M"] = percentile(v, 0.95);
    }
    // long-line-keystroke (layout cost only)
    {
        const std::string line = readFile(fixtures / "perf/longline-100k.txt");
        double worst = 0;
        for (bool wrap : {true, false}) {
            Headless h(fontsDir, 1280, 800, wrap);
            h.editor->loadText(decode(line));
            h.paint();
            for (int i = 0; i < 40; ++i) h.editor->handle(editor::cmd::Move{editor::MoveKind::Right});
            h.paint();
            std::vector<double> v;
            for (int i = 0; i < 1000; ++i) {
                const auto t0 = Clock::now();
                h.editor->handle(editor::cmd::Type{U"y"});
                v.push_back(ms(Clock::now() - t0));
                if (i % 50 == 0) h.paint();
            }
            worst = std::max(worst, percentile(v, 0.95));
        }
        m["longLineKeystrokeLayoutP95Ms"] = worst;
    }
    m["cacheHitRate"] = hits + misses ? static_cast<double>(hits) / static_cast<double>(hits + misses) : 1.0;

    // memory (child processes)
    {
        wchar_t self[MAX_PATH];
        GetModuleFileNameW(nullptr, self, MAX_PATH);
        for (std::size_t n : {std::size_t{100000}, std::size_t{10000000}}) {
            const fs::path tmp = fs::temp_directory_path() / ("pxc-bench-mem-" + std::to_string(n) + ".json");
            const std::wstring cl = quote(self) + L" --scenario memory --chars " + std::to_wstring(n) + L" --fonts-dir " +
                                    quote(fontsDir) + L" --out " + quote(tmp);
            if (runProcess(cl, 300000)) {
                std::smatch mm;
                const std::string j = readFile(tmp);
                if (std::regex_search(j, mm, std::regex("\"workingSetMB\":([0-9.]+)")))
                    m[n == 100000 ? "idleMemoryMB100k" : "idleMemoryMB10M"] = std::stod(mm[1]);
            }
        }
    }

    // end-to-end scenarios through the real application
    if (has("app")) {
        const fs::path app = opt["app"];
        const fs::path scripts = has("scripts") ? fs::path(opt["scripts"]) : fixtures.parent_path() / "perf/scripts";
        const fs::path tmp = fs::temp_directory_path();
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        std::vector<double> startups;
        for (int r = 0; r < 10; ++r) {
            const fs::path js = tmp / "pxc-bench-startup.json";
            long long start = 0;
            if (runProcess(quote(app) + L" --no-preferences --exit-after-first-present --stats-json " + quote(js), 60000, &start)) {
                const auto s = parseStats(js);
                if (s.firstPresentQpc > start)
                    startups.push_back(static_cast<double>(s.firstPresentQpc - start) * 1000.0 / static_cast<double>(freq.QuadPart));
            }
        }
        m["startupMs"] = percentile(startups, 0.5);
        auto runScript = [&](const char* name) {
            const fs::path js = tmp / (std::string("pxc-bench-") + name + ".json");
            std::error_code ec;
            fs::remove(js, ec);
            runProcess(quote(app) + L" --no-preferences --bench-script " + quote(scripts / name) + L" --stats-json " + quote(js), 600000);
            return parseStats(js);
        };
        {
            const auto s = runScript("typing.pxs");
            std::vector<double> v;
            for (long long x : s.keyToPresent) v.push_back(static_cast<double>(x) / 1000.0);
            m["keyToPresentP50Ms"] = percentile(v, 0.5);
            m["keyToPresentP95Ms"] = percentile(v, 0.95);
            m["keyToPresentSamples"] = static_cast<double>(v.size());
        }
        {
            const auto s = runScript("long-line.pxs");
            std::vector<double> v;
            for (long long x : s.keyToPresent) v.push_back(static_cast<double>(x) / 1000.0);
            m["e2eLongLineP95Ms"] = percentile(v, 0.95);
        }
        {
            const auto s = runScript("resource-cycles.pxs");
            if (s.samples.size() >= 2) {
                const auto& a = s.samples.front();
                const auto& b = s.samples.back();
                auto d = [&](const char* k) { return static_cast<double>(b.at(k) - a.at(k)); };
                m["resourceGrowthGdi"] = d("gdiObjects");
                m["resourceGrowthUser"] = d("userObjects");
                m["resourceGrowthHandles"] = d("handleCount");
            } else {
                m["resourceGrowthHandles"] = 1e9; // script did not complete
            }
        }
    }

    // write results
    {
        std::ofstream f(out, std::ios::trunc);
        f << "{\n  \"metrics\": {\n";
        std::size_t i = 0;
        for (const auto& [k, v] : m) f << "    \"" << k << "\": " << v << (++i < m.size() ? ",\n" : "\n");
        f << "  }\n}\n";
    }
    for (const auto& [k, v] : m) std::printf("%-30s %12.4f\n", k.c_str(), v);

    // --check: budgets and regressions
    int failures = 0;
    if (has("check")) {
        const std::string base = readFile(opt["check"]);
        const bool measured = base.find("\"measured\": true") != std::string::npos;
        char* env = nullptr;
        std::size_t envLen = 0;
        bool reference = false;
        if (_dupenv_s(&env, &envLen, "PIXELCSS_REFERENCE_MACHINE") == 0 && env) {
            reference = std::string(env) == "1";
            std::free(env);
        }
        auto fail = [&](const std::string& what) {
            std::printf("FAIL: %s\n", what.c_str());
            ++failures;
        };
        // Resource growth: enforced on every machine (tolerance +/-2 handles for OS-owned noise).
        if (m.count("resourceGrowthGdi") && m["resourceGrowthGdi"] != 0) fail("GDI object growth");
        if (m.count("resourceGrowthUser") && m["resourceGrowthUser"] != 0) fail("USER object growth");
        if (m.count("resourceGrowthHandles") && std::abs(m["resourceGrowthHandles"]) > 2) fail("handle growth");
        const std::map<std::string, std::pair<double, bool>> budgets = {
            // metric -> (budget, higherIsBetter)
            {"keyToPresentP95Ms", {50, false}},      {"firstViewportMs", {1000, false}},
            {"startupMs", {750, false}},             {"rasterMissP95Us", {20, false}},
            {"nonReflowEditLayoutP95Ms", {1, false}}, {"longLineKeystrokeLayoutP95Ms", {5, false}},
            {"cacheHitRate", {0.99, true}},          {"idleMemoryMB100k", {64, false}},
            {"idleMemoryMB10M", {256, false}},
        };
        if (reference) {
            for (const auto& [k, b] : budgets) {
                if (!m.count(k)) continue;
                const bool ok = b.second ? m[k] >= b.first : m[k] <= b.first;
                if (!ok) fail(k + " = " + std::to_string(m[k]) + " misses budget " + std::to_string(b.first));
            }
        } else {
            std::puts("absolute budgets: informational (set PIXELCSS_REFERENCE_MACHINE=1 on the reference machine)");
        }
        if (measured) {
            const std::regex kv("\"(\\w+)\":\\s*(-?[0-9.eE+]+)");
            for (auto it = std::sregex_iterator(base.begin(), base.end(), kv); it != std::sregex_iterator(); ++it) {
                const std::string k = (*it)[1];
                if (!m.count(k) || k.rfind("resourceGrowth", 0) == 0 || k == "keyToPresentSamples") continue;
                const double b = std::stod((*it)[2]);
                const bool higher = budgets.count(k) && budgets.at(k).second;
                // A regression must exceed 20% AND an absolute noise floor (timer granularity / OS scheduling
                // noise): 0.5 ms for *Ms metrics, 25 ms for startup (process creation), 2 us for *Us, 4 MB for *MB.
                double floor = 0.0;
                if (k == "startupMs") floor = 25.0;
                else if (k.size() > 2 && k.compare(k.size() - 2, 2, "Ms") == 0) floor = 0.5;
                else if (k.find("Us") != std::string::npos) floor = 2.0;
                else if (k.find("MB") != std::string::npos) floor = 4.0;
                const bool regressed = higher ? m[k] < b * 0.8 : (b > 0 && m[k] > b * 1.2 && m[k] - b > floor);
                if (regressed) fail(k + " regressed > 20% (baseline " + std::to_string(b) + ", now " + std::to_string(m[k]) + ")");
            }
        } else {
            std::puts("regression check: SKIPPED (no measured baseline)");
        }
        std::printf("perf check: %s\n", failures ? "FAILED" : "passed");
    }
    return failures ? 1 : 0;
}
