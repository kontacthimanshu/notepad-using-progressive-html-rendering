// Resource cleanup (Constitution XI): no GDI/USER/handle growth in the real app, no engine heap leaks, and no
// temporary files left behind by failed saves.
#include <doctest.h>

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <vector>

#include "pxc/storage/text_file.hpp"
#include "pxc/storage/win32_file_system.hpp"
#include "support/editor_harness.hpp"
#include "support/fakes.hpp"
#include "support/fs_util.hpp"

using namespace pxc;
namespace fs = std::filesystem;

TEST_CASE("(a) process level: 50 open/font/wrap/resize/new cycles do not grow GDI, USER or handle counts") {
    test::TempDir dir;
    const fs::path json = dir.path() / "stats.json";
    const fs::path script = fs::path(PXC_SOURCE_DIR) / "tests/perf/scripts/resource-cycles.pxs";
    std::wstring cmd = L"\"" + fs::path(PXC_APP_EXE).wstring() + L"\" --no-preferences --bench-script \"" + script.wstring() +
                       L"\" --stats-json \"" + json.wstring() + L"\"";
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    REQUIRE(CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi));
    const DWORD wait = WaitForSingleObject(pi.hProcess, 120000);
    if (wait != WAIT_OBJECT_0) TerminateProcess(pi.hProcess, 1);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    REQUIRE(wait == WAIT_OBJECT_0);
    CHECK(code == 0);

    std::vector<std::map<std::string, long long>> samples;
    std::ifstream f(json);
    std::string line;
    const std::regex num("\"(\\w+)\":(-?\\d+)");
    while (std::getline(f, line)) {
        if (line.find("\"label\":\"script\"") == std::string::npos) continue;
        std::map<std::string, long long> kv;
        for (auto it = std::sregex_iterator(line.begin(), line.end(), num); it != std::sregex_iterator(); ++it)
            kv[(*it)[1]] = std::stoll((*it)[2]);
        samples.push_back(kv);
    }
    REQUIRE(samples.size() == 2);
    const auto& a = samples[0];
    const auto& b = samples[1];
    CHECK(b.at("gdiObjects") == a.at("gdiObjects"));
    CHECK(b.at("userObjects") == a.at("userObjects"));
    // +/-2 handles tolerance for OS-owned noise (e.g. thread-pool or COM internals created on demand).
    CHECK(std::llabs(b.at("handleCount") - a.at("handleCount")) <= 2);
}

TEST_CASE("(b) engine level: 1000 create/destroy cycles of the full editor stack do not leak") {
    std::u32string text;
    for (int i = 0; i < 400; ++i) text += U"resource cleanup line " + std::u32string(1, static_cast<char32_t>(U'a' + i % 26)) + U"\n";
    for (int i = 0; i < 1000; ++i) {
        test::EditorHarness h(200, 120, i % 2 == 0);
        h.editor->loadText(text);
        h.type(U"xyz");
        h.paint();
        if (i % 3 == 0) h.editor->handle(editor::cmd::SetFont{"comic-style"});
        h.paint();
    }
    // Leaks are reported by the CRT leak-check listener (tests/support/leak_check.cpp) for this test case.
    CHECK(true);
}

TEST_CASE("(c) files: failed atomic saves leave no temp file and keep the original intact") {
    document::TextDocument doc;
    doc.setText(U"new content");
    // Injected failure.
    test::FakeFileSystem ffs;
    ffs.files["C:/x/doc.txt"] = "original";
    ffs.failWrite["C:/x/doc.txt"] = storage::FileError::Kind::DiskFull;
    CHECK_FALSE(storage::saveTextFileAtomic(ffs, "C:/x/doc.txt", doc, storage::FileInfo{}));
    CHECK(ffs.files.at("C:/x/doc.txt") == "original");
    // Real read-only target.
    test::TempDir dir;
    const fs::path p = dir.path() / "ro.txt";
    test::writeAll(p, "original bytes");
    REQUIRE(SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_READONLY));
    storage::Win32FileSystem w;
    CHECK_FALSE(storage::saveTextFileAtomic(w, p, doc, storage::FileInfo{}));
    CHECK(test::readAll(p) == "original bytes");
    CHECK_FALSE(fs::exists(fs::path(p.wstring() + L".pixelcss-tmp")));
    SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL);
}
