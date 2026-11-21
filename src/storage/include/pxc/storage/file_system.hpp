#pragma once
// File-system seam so tests can inject failures (contracts/engine-interfaces.md).

#include <filesystem>
#include <string>
#include <string_view>

#include "pxc/core/expected.hpp"
#include "pxc/storage/text_codec.hpp"

namespace pxc::storage {

struct IFileSystem {
    virtual core::Expected<std::string, FileError> readAll(const std::filesystem::path& path) = 0;
    // Writes <path>.pixelcss-tmp, flushes, then atomically replaces <path>. On failure the original file
    // is untouched and the temporary file is removed.
    virtual core::Expected<void, FileError> writeAtomic(const std::filesystem::path& path, std::string_view bytes) = 0;
    virtual ~IFileSystem() = default;
};

// Readable message for a failure kind, naming the file.
std::wstring describeFileError(FileError::Kind kind, const std::filesystem::path& path, bool writing);

} // namespace pxc::storage
