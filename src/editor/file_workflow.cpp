#include "pxc/editor/file_workflow.hpp"

#include "pxc/storage/text_file.hpp"

namespace pxc::editor {

FileWorkflow::FileWorkflow(EditorController& editor, storage::IFileSystem& fs, IPromptService& prompts,
                           IFileDialogs& dialogs)
    : editor_(&editor), fs_(&fs), prompts_(&prompts), dialogs_(&dialogs) {}

std::wstring FileWorkflow::documentName() const {
    return info_.path ? info_.path->filename().wstring() : std::wstring(L"Untitled");
}

std::wstring FileWorkflow::windowTitle() const {
    std::wstring t = L"PixelCSS — " + documentName();
    if (editor_->document().modified()) t += L"*";
    if (info_.converted) t += L" (converted)";
    if (preview_) t += L" — Preview (read-only)";
    else if (isHtmlFile()) t += L" — Source";
    return t;
}

bool FileWorkflow::isHtmlFile() const {
    if (!info_.path) return false;
    std::wstring ext = info_.path->extension().wstring();
    for (auto& c : ext)
        if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
    return ext == L".html" || ext == L".htm";
}

bool FileWorkflow::confirmDiscard() {
    if (!editor_->document().modified()) return true;
    switch (prompts_->askSaveChanges(documentName())) {
    case PromptChoice::Save: return save();
    case PromptChoice::DontSave: return true;
    case PromptChoice::Cancel: return false;
    }
    return false;
}

bool FileWorkflow::newDocument() {
    if (!confirmDiscard()) return false;
    editor_->setReadOnly(false);
    editor_->setMode(EditorMode::EditPlainText);
    editor_->loadText(U"");
    info_ = {};
    preview_ = false;
    return true;
}

bool FileWorkflow::open(std::optional<std::filesystem::path> path) {
    if (!confirmDiscard()) return false;
    if (!path) path = dialogs_->pickOpen();
    if (!path) return false;
    auto loaded = storage::loadTextFile(*fs_, *path, storage::InvalidUtf8Policy::Fail);
    if (!loaded && loaded.error().kind == storage::FileError::Kind::InvalidEncoding) {
        if (prompts_->askInvalidEncoding(path->filename().wstring()) == InvalidEncodingChoice::Cancel) return false;
        loaded = storage::loadTextFile(*fs_, *path, storage::InvalidUtf8Policy::Replace);
    }
    if (!loaded) {
        prompts_->showError(loaded.error().userMessage);
        return false;
    }
    // The decoded text always becomes the document, also for HTML: the preview renders from it and
    // View -> Page Source shows and edits it, so the modified flag and Save cover both views.
    editor_->setReadOnly(false);
    editor_->setMode(EditorMode::EditPlainText);
    editor_->loadText(loaded->text);
    info_ = loaded->info;
    preview_ = onOpenHook && onOpenHook(*path, loaded->text);
    if (info_.converted) editor_->document().markModified();
    return true;
}

bool FileWorkflow::save() {
    if (!info_.path) return saveAs();
    return writeTo(*info_.path);
}

bool FileWorkflow::saveAs() {
    const std::filesystem::path suggested = info_.path ? *info_.path : std::filesystem::path(L"Untitled.txt");
    auto p = dialogs_->pickSave(suggested);
    if (!p) return false;
    return writeTo(*p);
}

bool FileWorkflow::writeTo(const std::filesystem::path& path) {
    storage::FileInfo out = info_;
    if (out.encoding == storage::Encoding::Utf16LE || out.encoding == storage::Encoding::Utf16BE) {
        if (!prompts_->confirmConvertToUtf8(path.filename().wstring())) return false;
        out.encoding = storage::Encoding::Utf8;
    }
    auto r = storage::saveTextFileAtomic(*fs_, path, editor_->document(), out);
    if (!r) {
        prompts_->showError(r.error().userMessage);
        return false;
    }
    out.path = path;
    out.converted = false;
    info_ = out;
    editor_->document().markSaved();
    return true;
}

bool FileWorkflow::requestExit() { return confirmDiscard(); }

} // namespace pxc::editor
