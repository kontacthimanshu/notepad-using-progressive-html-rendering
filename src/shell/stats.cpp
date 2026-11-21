// Instrumentation (research R-16): QPC timing, per-frame RenderStats, process resource samples, the F12
// "Render Statistics" overlay (drawn with pack glyphs into the framebuffer — never OS text) and --stats-json.
#include "app.hpp"

#include <psapi.h>

#include <cstdio>
#include <fstream>

#include "pxc/render/renderer.hpp"

namespace pxc::shell {

long long qpcNow() noexcept {
    LARGE_INTEGER v;
    QueryPerformanceCounter(&v);
    return v.QuadPart;
}

namespace {
long long qpcFrequency() {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return f.QuadPart;
}
long long qpcToMicros(long long ticks) { return ticks * 1'000'000 / qpcFrequency(); }
} // namespace

void App::recordSample(const char* label) {
    ProcessSample s;
    s.label = label;
    HANDLE proc = GetCurrentProcess();
    s.gdiObjects = GetGuiResources(proc, GR_GDIOBJECTS);
    s.userObjects = GetGuiResources(proc, GR_USEROBJECTS);
    DWORD handles = 0;
    GetProcessHandleCount(proc, &handles);
    s.handleCount = handles;
    PROCESS_MEMORY_COUNTERS pmc{sizeof(pmc)};
    if (GetProcessMemoryInfo(proc, &pmc, sizeof(pmc))) s.workingSetBytes = pmc.WorkingSetSize;
    stats.samples.push_back(std::move(s));
}

void App::onPresented(const update::RenderStats& frame, bool painted) {
    const long long now = qpcNow();
    update::RenderStats f = frame;
    if (pendingInputQpc != 0 && painted) {
        f.keyToPresentMicros = qpcToMicros(now - pendingInputQpc);
        pendingInputQpc = 0;
    }
    if (painted) {
        lastFrame = f;
        if (f.keyToPresentMicros >= 0) lastKeyToPresentMicros = f.keyToPresentMicros;
        if (options.statsJson) stats.frames.push_back(f);
    }
    if (!firstPresentDone) {
        firstPresentDone = true;
        stats.qpcFrequency = qpcFrequency();
        stats.firstPresentQpc = now;
        recordSample("first-present");
        if (options.exitAfterFirstPresent) PostMessageW(main, WM_CLOSE, 0, 0);
    }
    if (openDecodedQpc != 0 && painted) {
        stats.firstViewportMicros = qpcToMicros(now - openDecodedQpc);
        openDecodedQpc = 0;
    }
    if (bench) PostMessageW(main, WM_APP_BENCH_NEXT, 0, 0);
}

void App::writeStatsJson() {
    if (!options.statsJson) return;
    std::ofstream f(*options.statsJson, std::ios::binary | std::ios::trunc);
    if (!f) return;
    char buf[512];
    std::snprintf(buf, sizeof buf,
                  "{\"type\":\"first-present\",\"firstPresentQpc\":%lld,\"qpcFrequency\":%lld,\"firstViewportMicros\":%lld}\n",
                  stats.firstPresentQpc, stats.qpcFrequency, stats.firstViewportMicros);
    f << buf;
    for (const auto& s : stats.samples) {
        std::snprintf(buf, sizeof buf,
                      "{\"type\":\"sample\",\"label\":\"%s\",\"gdiObjects\":%lu,\"userObjects\":%lu,\"handleCount\":%lu,"
                      "\"workingSetBytes\":%llu}\n",
                      s.label.c_str(), s.gdiObjects, s.userObjects, s.handleCount, s.workingSetBytes);
        f << buf;
    }
    for (const auto& r : stats.frames) {
        std::snprintf(buf, sizeof buf,
                      "{\"type\":\"frame\",\"frame\":%llu,\"cacheHits\":%llu,\"cacheMisses\":%llu,\"cacheHitRate\":%.4f,"
                      "\"rasterizations\":%llu,\"linesLaidOut\":%llu,\"rowsRecomputed\":%llu,\"glyphBlits\":%llu,"
                      "\"dirtyRectCount\":%llu,\"dirtyPixelArea\":%llu,\"fullRepaint\":%s,\"paintMicros\":%lld,"
                      "\"keyToPresentMicros\":%lld}\n",
                      static_cast<unsigned long long>(r.frame), static_cast<unsigned long long>(r.cacheHits),
                      static_cast<unsigned long long>(r.cacheMisses), r.cacheHitRate,
                      static_cast<unsigned long long>(r.rasterizations), static_cast<unsigned long long>(r.linesLaidOut),
                      static_cast<unsigned long long>(r.rowsRecomputed), static_cast<unsigned long long>(r.glyphBlits),
                      static_cast<unsigned long long>(r.dirtyRectCount), static_cast<unsigned long long>(r.dirtyPixelArea),
                      r.fullRepaint ? "true" : "false", r.paintMicros, r.keyToPresentMicros);
        f << buf;
    }
}

// ---- F12 overlay ----

core::Rect App::overlayRect() const {
    const int s = std::max(1, scale);
    const int w = 230 * s, h = 9 * (layout ? layout->rowHeight() : 15 * s) + 8 * s;
    return {std::max(0, fb.width() - w - 6 * s), 4 * s, std::min(w, fb.width()), h};
}

void App::drawOverlay(render::Framebuffer& target, std::vector<core::Rect>& presented,
                      const update::RenderStats& current) {
    if (!overlayOn || !layout) return;
    const core::Rect box = overlayRect().intersect(target.bounds());
    if (box.empty()) return;
    // Translucent panel (#000000B0) blended over the document pixels.
    const std::uint32_t panel = core::Color{0, 0, 0, 0xB0}.toBGRA();
    for (int y = box.y; y < box.bottom(); ++y) {
        auto row = target.row(y);
        for (int x = box.x; x < box.right(); ++x) row[static_cast<std::size_t>(x)] = render::blendPixel(row[static_cast<std::size_t>(x)], panel);
    }
    const update::RenderStats& last = current;
    char lines[9][96];
    std::snprintf(lines[0], 96, "Render statistics (F12)");
    std::snprintf(lines[1], 96, "frame %llu  full %s", static_cast<unsigned long long>(last.frame), last.fullRepaint ? "yes" : "no");
    std::snprintf(lines[2], 96, "cache hits %llu  misses %llu", static_cast<unsigned long long>(last.cacheHits), static_cast<unsigned long long>(last.cacheMisses));
    std::snprintf(lines[3], 96, "hit rate %.3f  raster %llu", last.cacheHitRate, static_cast<unsigned long long>(last.rasterizations));
    std::snprintf(lines[4], 96, "lines laid out %llu  rows %llu", static_cast<unsigned long long>(last.linesLaidOut), static_cast<unsigned long long>(last.rowsRecomputed));
    std::snprintf(lines[5], 96, "dirty rects %llu  area %llu px", static_cast<unsigned long long>(last.dirtyRectCount), static_cast<unsigned long long>(last.dirtyPixelArea));
    std::snprintf(lines[6], 96, "paint %.2f ms", static_cast<double>(last.paintMicros) / 1000.0);
    if (lastKeyToPresentMicros >= 0) std::snprintf(lines[7], 96, "key->present %.2f ms", static_cast<double>(lastKeyToPresentMicros) / 1000.0);
    else std::snprintf(lines[7], 96, "key->present -");
    std::snprintf(lines[8], 96, "unmeasured lines %zu", layout->unmeasuredLines());

    render::Renderer r;
    const int s = std::max(1, scale);
    const std::string& pack = editor->state().activePackId;
    int y = box.y + 4 * s;
    for (const auto& text : lines) {
        int pen = box.x + 6 * s;
        const int baseline = y + layout->ascentPx();
        for (const char* p = text; *p; ++p) {
            const char32_t cp = static_cast<unsigned char>(*p);
            const auto rg = registry->resolve(pack, cp);
            if (!rg.def) continue;
            if (cp != U' ') r.blitGlyph(target, cache->get(pack, cp, s, core::kWhite, core::kBlack), pen, baseline, box);
            pen += rg.def->advance * s;
        }
        y += layout->rowHeight();
    }
    presented.push_back(box);
}

} // namespace pxc::shell
