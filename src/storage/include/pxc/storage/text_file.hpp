#pragma once
// Load/save plain-text documents through an IFileSystem.

#include <filesystem>

#include "pxc/document/text_document.hpp"
#include "pxc/storage/file_system.hpp"

namespace pxc::storage {

core::Expected<LoadedText, FileError> loadTextFile(IFileSystem& fs, const std::filesystem::path& path,
                                                   InvalidUtf8Policy policy);

core::Expected<void, FileError> saveTextFileAtomic(IFileSystem& fs, const std::filesystem::path& path,
                                                   const document::TextDocument& doc, const FileInfo& info);

} // namespace pxc::storage
