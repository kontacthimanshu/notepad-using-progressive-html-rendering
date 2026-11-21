#include "pxc/storage/file_system.hpp"

namespace pxc::storage {

std::wstring describeFileError(FileError::Kind kind, const std::filesystem::path& path, bool writing) {
    const std::wstring name = L"'" + path.filename().wstring() + L"'";
    const std::wstring verb = writing ? L"save " : L"open ";
    switch (kind) {
    case FileError::Kind::NotFound:
        return L"PixelCSS could not " + verb + name + L" because the file or folder does not exist.";
    case FileError::Kind::AccessDenied:
        return L"PixelCSS could not " + verb + name +
               L" because access was denied. The file may be read-only or you may not have permission.";
    case FileError::Kind::SharingViolation:
        return L"PixelCSS could not " + verb + name + L" because another program is using it.";
    case FileError::Kind::DiskFull:
        return L"PixelCSS could not " + verb + name + L" because the disk is full.";
    case FileError::Kind::InvalidEncoding:
        return L"PixelCSS could not " + verb + name + L" because it is not valid UTF-8 text.";
    case FileError::Kind::Other: break;
    }
    return L"PixelCSS could not " + verb + name + L" because of an unexpected file error.";
}

} // namespace pxc::storage
