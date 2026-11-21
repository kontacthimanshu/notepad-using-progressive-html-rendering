// US2 independent test: type -> save -> reopen identical; unsaved-changes flow (FR-027, SC-005).
#include <doctest.h>

#include "pxc/editor/file_workflow.hpp"
#include "pxc/storage/win32_file_system.hpp"
#include "support/editor_harness.hpp"
#include "support/fs_util.hpp"

using namespace pxc;
using namespace pxc::editor;
using test::EditorHarness;
namespace fs = std::filesystem;

TEST_CASE("type a multiline paragraph, save, reopen: identical text and expected bytes") {
    test::TempDir dir;
    const fs::path p = dir.path() / "roundtrip.txt";
    storage::Win32FileSystem wfs;
    test::FakePromptService prompts;
    test::FakeFileDialogs dialogs;
    dialogs.savePath = p;
    {
        EditorHarness h;
        FileWorkflow wf(*h.editor, wfs, prompts, dialogs);
        h.type(U"First line of the paragraph\nSecond line, caf\u00e9\nThird \u6f22 line");
        h.editor->handle(cmd::Move{MoveKind::Up});
        h.editor->handle(cmd::Undo{});
        h.editor->handle(cmd::Redo{});
        REQUIRE(wf.save()); // untitled -> Save As
        CHECK_FALSE(h.doc.modified());
        CHECK(test::readAll(p) == "First line of the paragraph\r\nSecond line, caf\xc3\xa9\r\nThird \xe6\xbc\xa2 line");
    }
    {
        EditorHarness h2;
        FileWorkflow wf2(*h2.editor, wfs, prompts, dialogs);
        REQUIRE(wf2.open(p));
        CHECK(h2.doc.text() == U"First line of the paragraph\nSecond line, caf\u00e9\nThird \u6f22 line");
        CHECK_FALSE(h2.doc.modified());
        CHECK(wf2.documentName() == L"roundtrip.txt");
        REQUIRE(wf2.save());
        CHECK(test::readAll(p) == "First line of the paragraph\r\nSecond line, caf\xc3\xa9\r\nThird \xe6\xbc\xa2 line");
    }
}

namespace {
struct Flow {
    EditorHarness h;
    test::FakeFileSystem fs;
    test::FakePromptService prompts;
    test::FakeFileDialogs dialogs;
    FileWorkflow wf{*h.editor, fs, prompts, dialogs};
    Flow() {
        fs.files["C:/x/other.txt"] = "other file";
        dialogs.openPath = fs::path("C:/x/other.txt");
        dialogs.savePath = fs::path("C:/x/saved.txt");
        h.type(U"unsaved work");
    }
};
} // namespace

TEST_CASE("Cancel aborts New, Open and Exit and keeps the content") {
    for (int action = 0; action < 3; ++action) {
        CAPTURE(action);
        Flow f;
        f.prompts.saveAnswers = {PromptChoice::Cancel};
        const bool ok = action == 0 ? f.wf.newDocument() : action == 1 ? f.wf.open() : f.wf.requestExit();
        CHECK_FALSE(ok);
        CHECK(f.prompts.saveQuestions == 1);
        CHECK(f.h.doc.text() == U"unsaved work");
        CHECK(f.h.doc.modified());
    }
}

TEST_CASE("Don't Save discards and proceeds") {
    Flow f;
    f.prompts.saveAnswers = {PromptChoice::DontSave};
    CHECK(f.wf.open());
    CHECK(f.h.doc.text() == U"other file");
    CHECK(f.fs.writes == 0);
    Flow g;
    g.prompts.saveAnswers = {PromptChoice::DontSave};
    CHECK(g.wf.newDocument());
    CHECK(g.h.doc.text().empty());
    Flow x;
    x.prompts.saveAnswers = {PromptChoice::DontSave};
    CHECK(x.wf.requestExit());
}

TEST_CASE("Save writes first, then proceeds") {
    Flow f;
    f.prompts.saveAnswers = {PromptChoice::Save};
    CHECK(f.wf.newDocument());
    CHECK(f.fs.files.at("C:/x/saved.txt") == "unsaved work");
    CHECK(f.h.doc.text().empty());
}

TEST_CASE("a failed Save aborts the pending action without losing content") {
    for (int action = 0; action < 3; ++action) {
        CAPTURE(action);
        Flow f;
        f.fs.failWrite[fs::path("C:/x/saved.txt")] = storage::FileError::Kind::DiskFull;
        f.prompts.saveAnswers = {PromptChoice::Save};
        const bool ok = action == 0 ? f.wf.newDocument() : action == 1 ? f.wf.open() : f.wf.requestExit();
        CHECK_FALSE(ok);
        CHECK(f.h.doc.text() == U"unsaved work");
        CHECK(f.h.doc.modified());
        REQUIRE(f.prompts.errors.size() == 1);
        CHECK(f.prompts.errors[0].find(L"saved.txt") != std::wstring::npos);
    }
}

TEST_CASE("no prompt when the document is unmodified") {
    EditorHarness h;
    test::FakeFileSystem fs;
    test::FakePromptService prompts;
    test::FakeFileDialogs dialogs;
    FileWorkflow wf(*h.editor, fs, prompts, dialogs);
    CHECK(wf.requestExit());
    CHECK(prompts.saveQuestions == 0);
}

TEST_CASE("invalid UTF-8: cancel keeps the current document, replacement opens converted") {
    Flow f;
    f.fs.files["C:/x/bad.txt"] = std::string("ok \xff bytes");
    f.h.doc.markSaved();
    f.prompts.encodingAnswer = InvalidEncodingChoice::Cancel;
    CHECK_FALSE(f.wf.open(fs::path("C:/x/bad.txt")));
    CHECK(f.h.doc.text() == U"unsaved work");
    f.prompts.encodingAnswer = InvalidEncodingChoice::OpenWithReplacement;
    CHECK(f.wf.open(fs::path("C:/x/bad.txt")));
    CHECK(f.h.doc.text() == U"ok \uFFFD bytes");
    CHECK(f.h.doc.modified());
    CHECK(f.wf.windowTitle().find(L"(converted)") != std::wstring::npos);
}

TEST_CASE("HTML view source: the document holds the source in both views, and edits are prompted and saved") {
    EditorHarness h;
    test::FakeFileSystem fs;
    test::FakePromptService prompts;
    test::FakeFileDialogs dialogs;
    FileWorkflow wf(*h.editor, fs, prompts, dialogs);
    // The shell's hook routes .html to the preview and makes the editor read-only.
    wf.onOpenHook = [&](const fs::path& p, const std::u32string&) {
        const bool html = p.extension() == ".html";
        if (html) h.editor->setReadOnly(true);
        return html;
    };
    fs.files["C:/x/page.html"] = "<p>Hello</p>";
    REQUIRE(wf.open(fs::path("C:/x/page.html")));
    CHECK(wf.isHtmlFile());
    CHECK(wf.previewShown());
    CHECK(h.doc.text() == U"<p>Hello</p>"); // source is available for View -> Page Source
    CHECK_FALSE(h.doc.modified());
    CHECK(wf.windowTitle() == L"PixelCSS — page.html — Preview (read-only)");

    // Switch to the source view and edit it.
    wf.setPreviewShown(false);
    h.editor->setReadOnly(false);
    CHECK(wf.windowTitle() == L"PixelCSS — page.html — Source");
    h.editor->handle(cmd::Move{MoveKind::DocEnd});
    h.type(U"<p>World</p>");
    CHECK(wf.windowTitle() == L"PixelCSS — page.html* — Source");

    // Back to the preview: the unsaved edit is kept and Exit still asks to save it.
    wf.setPreviewShown(true);
    h.editor->setReadOnly(true);
    CHECK(wf.windowTitle() == L"PixelCSS — page.html* — Preview (read-only)");
    prompts.saveAnswers = {PromptChoice::Save};
    CHECK(wf.requestExit());
    CHECK(prompts.saveQuestions == 1);
    CHECK(fs.files.at("C:/x/page.html") == "<p>Hello</p><p>World</p>");
    CHECK_FALSE(h.doc.modified());

    // Opening a plain text file leaves HTML mode.
    fs.files["C:/x/a.txt"] = "plain";
    REQUIRE(wf.open(fs::path("C:/x/a.txt")));
    CHECK_FALSE(wf.isHtmlFile());
    CHECK_FALSE(wf.previewShown());
    CHECK(wf.windowTitle() == L"PixelCSS — a.txt");
}
