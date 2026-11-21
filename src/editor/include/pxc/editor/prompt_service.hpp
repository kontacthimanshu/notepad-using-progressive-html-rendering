#pragma once
// User-prompt and file-dialog seams; the shell implements them with TaskDialogIndirect / IFileDialog.

#include <filesystem>
#include <optional>
#include <string_view>

namespace pxc::editor {

enum class PromptChoice { Save, DontSave, Cancel };
enum class InvalidEncodingChoice { OpenWithReplacement, Cancel };

struct IPromptService {
    virtual PromptChoice askSaveChanges(std::wstring_view documentName) = 0;
    virtual InvalidEncodingChoice askInvalidEncoding(std::wstring_view fileName) = 0;
    virtual bool confirmConvertToUtf8(std::wstring_view fileName) = 0;
    virtual void showError(std::wstring_view message) = 0;
    virtual ~IPromptService() = default;
};

struct IFileDialogs {
    virtual std::optional<std::filesystem::path> pickOpen() = 0;
    virtual std::optional<std::filesystem::path> pickSave(const std::filesystem::path& suggested) = 0;
    virtual ~IFileDialogs() = default;
};

} // namespace pxc::editor
