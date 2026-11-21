#pragma once
// Win32 implementation of IFileSystem (CreateFileW/ReadFile/WriteFile, ReplaceFileW/MoveFileExW).
// Declared without <windows.h>; only the implementation in storage/win32 includes it.

#include "pxc/storage/file_system.hpp"

namespace pxc::storage {

class Win32FileSystem final : public IFileSystem {
public:
    core::Expected<std::string, FileError> readAll(const std::filesystem::path& path) override;
    core::Expected<void, FileError> writeAtomic(const std::filesystem::path& path, std::string_view bytes) override;
};

} // namespace pxc::storage
