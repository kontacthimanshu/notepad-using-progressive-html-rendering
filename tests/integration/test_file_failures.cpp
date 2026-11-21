// File failure handling: readable errors, no content loss, original file intact (FR-034, SC-008).
#include <doctest.h>

#include <windows.h>

#include <filesystem>

#include "pxc/storage/text_file.hpp"
#include "pxc/storage/win32_file_system.hpp"
#include "support/fakes.hpp"
#include "support/fs_util.hpp"

using namespace pxc;
using namespace pxc::storage;
namespace fs = std::filesystem;

TEST_CASE("injected read failures map to the right kinds with readable messages") {
    test::FakeFileSystem ffs;
    const fs::path p = "C:/docs/notes.txt";
    for (auto kind : {FileError::Kind::AccessDenied, FileError::Kind::SharingViolation, FileError::Kind::NotFound}) {
        ffs.failRead[p] = kind;
        auto r = loadTextFile(ffs, p, InvalidUtf8Policy::Fail);
        REQUIRE_FALSE(r);
        CHECK(r.error().kind == kind);
        CHECK(r.error().userMessage.find(L"notes.txt") != std::wstring::npos);
    }
}

TEST_CASE("a failed save keeps the document text and modified flag") {
    test::FakeFileSystem ffs;
    const fs::path p = "C:/docs/out.txt";
    document::TextDocument doc;
    doc.setText(U"precious");
    doc.insert(8, U" data");
    REQUIRE(doc.modified());
    for (auto kind : {FileError::Kind::AccessDenied, FileError::Kind::SharingViolation, FileError::Kind::DiskFull}) {
        ffs.failWrite[p] = kind;
        auto r = saveTextFileAtomic(ffs, p, doc, FileInfo{});
        REQUIRE_FALSE(r);
        CHECK(r.error().kind == kind);
        CHECK_FALSE(r.error().userMessage.empty());
        CHECK(doc.text() == U"precious data");
        CHECK(doc.modified());
    }
    CHECK(ffs.files.count(p) == 0);
}

TEST_CASE("real file system: saving over a read-only file fails and leaves it intact") {
    test::TempDir dir;
    const fs::path p = dir.path() / "readonly.txt";
    test::writeAll(p, "original bytes");
    REQUIRE(SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_READONLY));
    Win32FileSystem w;
    document::TextDocument doc;
    doc.setText(U"new content");
    auto r = saveTextFileAtomic(w, p, doc, FileInfo{});
    CHECK_FALSE(r);
    if (!r) CHECK(r.error().kind == FileError::Kind::AccessDenied);
    CHECK(test::readAll(p) == "original bytes");
    CHECK_FALSE(fs::exists(fs::path(p.wstring() + L".pixelcss-tmp")));
    SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL);
}

TEST_CASE("real file system: a file locked by another handle reports a sharing violation") {
    test::TempDir dir;
    const fs::path p = dir.path() / "locked.txt";
    test::writeAll(p, "locked");
    HANDLE h = CreateFileW(p.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    REQUIRE(h != INVALID_HANDLE_VALUE);
    Win32FileSystem w;
    auto r = w.readAll(p);
    CHECK_FALSE(r);
    if (!r) CHECK(r.error().kind == FileError::Kind::SharingViolation);
    CloseHandle(h);
    auto ok = w.readAll(p);
    REQUIRE(ok);
    CHECK(*ok == "locked");
}

TEST_CASE("real file system: atomic save writes the new content and removes the temp file") {
    test::TempDir dir;
    const fs::path p = dir.path() / "doc.txt";
    test::writeAll(p, "old");
    Win32FileSystem w;
    document::TextDocument doc;
    doc.setText(U"line1\nline2");
    REQUIRE(saveTextFileAtomic(w, p, doc, FileInfo{}));
    CHECK(test::readAll(p) == "line1\r\nline2");
    CHECK_FALSE(fs::exists(fs::path(p.wstring() + L".pixelcss-tmp")));
}
