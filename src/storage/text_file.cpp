#include "pxc/storage/text_file.hpp"

namespace pxc::storage {

core::Expected<LoadedText, FileError> loadTextFile(IFileSystem& fs, const std::filesystem::path& path,
                                                   InvalidUtf8Policy policy) {
    auto bytes = fs.readAll(path);
    if (!bytes) return core::Unexpected(bytes.error());
    auto decoded = decodeText(*bytes, policy, L"'" + path.filename().wstring() + L"'");
    if (!decoded) return core::Unexpected(decoded.error());
    decoded->info.path = path;
    return decoded;
}

core::Expected<void, FileError> saveTextFileAtomic(IFileSystem& fs, const std::filesystem::path& path,
                                                   const document::TextDocument& doc, const FileInfo& info) {
    return fs.writeAtomic(path, encodeText(doc.text(), info));
}

} // namespace pxc::storage
