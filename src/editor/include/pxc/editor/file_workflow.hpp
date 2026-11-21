#pragma once
// New / Open / Save / Save As / Exit flow with the unsaved-changes prompt (FR-027, FR-034, US2-AS5).
// Platform-independent: the shell supplies IPromptService, IFileDialogs and an IFileSystem.

#include <filesystem>
#include <functional>
#include <string>

#include "pxc/editor/editor_controller.hpp"
#include "pxc/editor/prompt_service.hpp"
#include "pxc/storage/file_system.hpp"
#include "pxc/storage/text_codec.hpp"

namespace pxc::editor {

class FileWorkflow {
public:
    FileWorkflow(EditorController& editor, storage::IFileSystem& fs, IPromptService& prompts, IFileDialogs& dialogs);

    // Each returns true when the action completed (false = cancelled or failed; content is never lost).
    bool newDocument();
    bool open(std::optional<std::filesystem::path> path = std::nullopt);
    bool save();
    bool saveAs();
    bool requestExit(); // true = OK to close

    const storage::FileInfo& fileInfo() const noexcept { return info_; }
    std::wstring documentName() const; // file name or "Untitled"
    std::wstring windowTitle() const;  // "PixelCSS — <name>*"

    // HTML files: true while the rendered preview is shown, false in the source view (View -> Page Source).
    // In both views the document holds the HTML source; Save and the unsaved-changes prompt write it.
    bool previewShown() const noexcept { return preview_; }
    void setPreviewShown(bool on) noexcept { preview_ = on; }
    bool isHtmlFile() const; // the current file has a .html / .htm extension

    // Hook called after a file is opened (path, decoded text) so the shell can route HTML to preview mode.
    // Returning true means the content is shown as a rendered preview (the text is already in the document).
    std::function<bool(const std::filesystem::path&, const std::u32string&)> onOpenHook;

private:
    bool confirmDiscard(); // asks Save/Don't Save/Cancel when modified
    bool writeTo(const std::filesystem::path& path);

    EditorController* editor_;
    storage::IFileSystem* fs_;
    IPromptService* prompts_;
    IFileDialogs* dialogs_;
    storage::FileInfo info_;
    bool preview_ = false;
};

} // namespace pxc::editor
