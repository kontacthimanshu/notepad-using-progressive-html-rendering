// --bench-script runner (research R-16, contracts/ui-contract.md "Bench script format").
// Commands run one per presented frame through the real window, message queue, editor, layout, paint and
// BitBlt; each injected keystroke is stamped with QPC so keyToPresentMicros measures true end-to-end latency.
#include "app.hpp"

#include <commctrl.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

#include "pxc/core/unicode.hpp"
#include "pxc/update/preview_painter.hpp"
#include "resource.h"

namespace pxc::shell {

class BenchScript {
public:
    struct Step {
        std::string cmd;
        std::vector<std::string> args;
        int line = 0;
    };
    std::vector<Step> steps;
    std::size_t next = 0;
    std::filesystem::path dir;
    std::u32string pendingChars; // remaining characters of a `type` command (one per frame)
    bool waitingIdle = false;
};

App::App() = default;
App::~App() = default;

namespace {

bool parseScript(const std::filesystem::path& file, BenchScript& out, std::string& error) {
    std::ifstream f(file, std::ios::binary);
    if (!f) {
        error = "cannot open bench script";
        return false;
    }
    out.dir = file.parent_path();
    struct Frame {
        int count;
        std::vector<BenchScript::Step> body;
    };
    std::vector<Frame> stack{{1, {}}};
    std::string line;
    int lineNo = 0;
    while (std::getline(f, line)) {
        ++lineNo;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') continue;
        line = line.substr(first);
        std::istringstream is(line);
        BenchScript::Step st;
        st.line = lineNo;
        is >> st.cmd;
        if (st.cmd == "type") {
            const auto sp = line.find(' ');
            st.args.push_back(sp == std::string::npos ? "" : line.substr(sp + 1));
        } else {
            std::string a;
            while (is >> a) st.args.push_back(a);
        }
        static const char* known[] = {"open", "type", "key", "font", "wrap", "resize", "scroll", "new",
                                      "repeat", "end", "wait-idle", "sample", "exit", "source"};
        if (std::none_of(std::begin(known), std::end(known), [&](const char* k) { return st.cmd == k; })) {
            error = "line " + std::to_string(lineNo) + ": unknown command '" + st.cmd + "'";
            return false;
        }
        if (st.cmd == "repeat") {
            if (st.args.size() != 1) { error = "line " + std::to_string(lineNo) + ": repeat needs a count"; return false; }
            stack.push_back({std::max(0, std::atoi(st.args[0].c_str())), {}});
        } else if (st.cmd == "end") {
            if (stack.size() < 2) { error = "line " + std::to_string(lineNo) + ": 'end' without 'repeat'"; return false; }
            Frame fr = std::move(stack.back());
            stack.pop_back();
            for (int i = 0; i < fr.count; ++i) stack.back().body.insert(stack.back().body.end(), fr.body.begin(), fr.body.end());
        } else {
            stack.back().body.push_back(std::move(st));
        }
    }
    if (stack.size() != 1) {
        error = "unterminated 'repeat' block";
        return false;
    }
    out.steps = std::move(stack.back().body);
    return true;
}

WPARAM keyCode(const std::string& name) {
    static const std::pair<const char*, WPARAM> map[] = {
        {"left", VK_LEFT}, {"right", VK_RIGHT}, {"up", VK_UP}, {"down", VK_DOWN}, {"home", VK_HOME},
        {"end", VK_END}, {"pgup", VK_PRIOR}, {"pgdn", VK_NEXT}, {"enter", VK_RETURN}, {"back", VK_BACK},
        {"delete", VK_DELETE}};
    for (const auto& [n, vk] : map)
        if (name == n) return vk;
    return 0;
}

void resizeClient(int w, int h) {
    auto& a = app();
    RECT mr, sr;
    GetWindowRect(a.main, &mr);
    GetClientRect(a.surface, &sr);
    const int dw = w - (sr.right - sr.left), dh = h - (sr.bottom - sr.top);
    SetWindowPos(a.main, nullptr, 0, 0, (mr.right - mr.left) + dw, (mr.bottom - mr.top) + dh, SWP_NOMOVE | SWP_NOZORDER);
}

} // namespace

void startBenchScript(const std::filesystem::path& script) {
    auto& a = app();
    auto b = std::make_unique<BenchScript>();
    std::string error;
    if (!parseScript(script, *b, error)) {
        std::fprintf(stderr, "bench script: %s\n", error.c_str());
        PostQuitMessage(2);
        return;
    }
    a.benchMode = true;
    a.bench = std::move(b);
    PostMessageW(a.main, WM_APP_BENCH_NEXT, 0, 0);
}

void benchStep() {
    auto& a = app();
    if (!a.bench) return;
    BenchScript& b = *a.bench;
    auto continueSoon = [&] {
        // Commands that produce no frame continue immediately.
        if (!a.updates->needsPaint()) PostMessageW(a.main, WM_APP_BENCH_NEXT, 0, 0);
    };
    if (!b.pendingChars.empty()) {
        const char32_t cp = b.pendingChars.front();
        b.pendingChars.erase(b.pendingChars.begin());
        a.pendingInputQpc = qpcNow();
        const std::wstring w = core::toWide(std::u32string(1, cp));
        for (wchar_t u : w) PostMessageW(a.surface, WM_CHAR, static_cast<WPARAM>(u), 0);
        return;
    }
    if (b.waitingIdle) {
        if (a.layout->unmeasuredLines() > 0) {
            SetTimer(a.main, 2, 5, nullptr); // poll again shortly (WM_TIMER posts WM_APP_BENCH_NEXT)
            return;
        }
        b.waitingIdle = false;
    }
    if (b.next >= b.steps.size()) {
        a.bench.reset();
        PostMessageW(a.main, WM_CLOSE, 0, 0);
        return;
    }
    const BenchScript::Step st = b.steps[b.next++];
    if (st.cmd == "type") {
        b.pendingChars = *core::decodeUtf8(st.args[0], core::InvalidPolicy::Replace);
        PostMessageW(a.main, WM_APP_BENCH_NEXT, 0, 0);
    } else if (st.cmd == "key") {
        std::string spec = st.args.empty() ? "" : st.args[0];
        LPARAM mods = 0;
        if (auto p = spec.find("+shift"); p != std::string::npos) { mods |= 1; spec.erase(p, 6); }
        if (auto p = spec.find("+ctrl"); p != std::string::npos) { mods |= 2; spec.erase(p, 5); }
        const WPARAM vk = keyCode(spec);
        a.pendingInputQpc = qpcNow();
        if (vk == VK_RETURN) PostMessageW(a.surface, WM_CHAR, L'\r', 0);
        else PostMessageW(a.surface, WM_APP_BENCH_KEY, vk, mods);
    } else if (st.cmd == "open") {
        std::filesystem::path p = st.args.empty() ? "" : st.args[0];
        if (p.is_relative()) p = b.dir / p;
        fileOpen(p);
        continueSoon();
    } else if (st.cmd == "font") {
        a.setFont(st.args.empty() ? "" : st.args[0]);
        continueSoon();
    } else if (st.cmd == "wrap") {
        const bool on = !st.args.empty() && st.args[0] == "on";
        if (a.editor->state().wordWrap != on) a.command(IDM_WORDWRAP);
        continueSoon();
    } else if (st.cmd == "resize") {
        if (st.args.size() == 2) resizeClient(std::atoi(st.args[0].c_str()), std::atoi(st.args[1].c_str()));
        continueSoon();
    } else if (st.cmd == "scroll") {
        a.pendingInputQpc = qpcNow();
        scrollRows(st.args.empty() ? 1 : std::atoi(st.args[0].c_str()));
        continueSoon();
    } else if (st.cmd == "source") {
        toggleSource(); // View -> Page Source
        continueSoon();
    } else if (st.cmd == "new") {
        fileNew();
        continueSoon();
    } else if (st.cmd == "wait-idle") {
        b.waitingIdle = true;
        PostMessageW(a.main, WM_APP_BENCH_NEXT, 0, 0);
    } else if (st.cmd == "sample") {
        a.recordSample("script");
        PostMessageW(a.main, WM_APP_BENCH_NEXT, 0, 0);
    } else if (st.cmd == "exit") {
        a.bench.reset();
        PostMessageW(a.main, WM_CLOSE, 0, 0);
    }
}

} // namespace pxc::shell
