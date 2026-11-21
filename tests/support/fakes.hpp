#pragma once
// Test doubles implementing the engine seams (contracts/engine-interfaces.md "Test doubles").

#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "pxc/editor/clipboard.hpp"
#include "pxc/editor/prompt_service.hpp"
#include "pxc/storage/file_system.hpp"

namespace pxc::test {

class FakeClipboard final : public editor::IClipboard {
public:
    std::optional<std::u32string> content;
    std::optional<std::u32string> getText() override { return content; }
    bool setText(std::u32string_view t) override {
        content = std::u32string(t);
        return true;
    }
};

// In-memory file system with per-path, per-operation failure injection.
class FakeFileSystem final : public storage::IFileSystem {
public:
    std::map<std::filesystem::path, std::string> files;
    std::map<std::filesystem::path, storage::FileError::Kind> failRead, failWrite;
    int writes = 0;

    core::Expected<std::string, storage::FileError> readAll(const std::filesystem::path& p) override {
        if (auto it = failRead.find(p); it != failRead.end())
            return core::Unexpected(storage::FileError{it->second, storage::describeFileError(it->second, p, false)});
        auto f = files.find(p);
        if (f == files.end())
            return core::Unexpected(storage::FileError{storage::FileError::Kind::NotFound,
                                                       storage::describeFileError(storage::FileError::Kind::NotFound, p, false)});
        return f->second;
    }
    core::Expected<void, storage::FileError> writeAtomic(const std::filesystem::path& p, std::string_view bytes) override {
        if (auto it = failWrite.find(p); it != failWrite.end())
            return core::Unexpected(storage::FileError{it->second, storage::describeFileError(it->second, p, true)});
        files[p] = std::string(bytes);
        ++writes;
        return {};
    }
};

class FakePromptService final : public editor::IPromptService {
public:
    std::deque<editor::PromptChoice> saveAnswers;
    editor::InvalidEncodingChoice encodingAnswer = editor::InvalidEncodingChoice::Cancel;
    bool convertAnswer = true;
    int saveQuestions = 0;
    std::vector<std::wstring> errors;

    editor::PromptChoice askSaveChanges(std::wstring_view) override {
        ++saveQuestions;
        if (saveAnswers.empty()) return editor::PromptChoice::Cancel;
        auto a = saveAnswers.front();
        saveAnswers.pop_front();
        return a;
    }
    editor::InvalidEncodingChoice askInvalidEncoding(std::wstring_view) override { return encodingAnswer; }
    bool confirmConvertToUtf8(std::wstring_view) override { return convertAnswer; }
    void showError(std::wstring_view m) override { errors.emplace_back(m); }
};

class FakeFileDialogs final : public editor::IFileDialogs {
public:
    std::optional<std::filesystem::path> openPath, savePath;
    std::optional<std::filesystem::path> pickOpen() override { return openPath; }
    std::optional<std::filesystem::path> pickSave(const std::filesystem::path&) override { return savePath; }
};

} // namespace pxc::test
