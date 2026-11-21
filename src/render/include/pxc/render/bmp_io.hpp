#pragma once
// 32-bit top-down BMP read/write, implemented by hand (no <windows.h>, no imaging dependency).

#include <filesystem>
#include <string>

#include "pxc/core/expected.hpp"
#include "pxc/render/framebuffer.hpp"

namespace pxc::render {

core::Expected<void, std::string> writeBmp32(const Framebuffer& fb, const std::filesystem::path& path);
core::Expected<Framebuffer, std::string> readBmp32(const std::filesystem::path& path);

// In-memory variants (used by tests and tools).
std::string encodeBmp32(const Framebuffer& fb);
core::Expected<Framebuffer, std::string> decodeBmp32(std::string_view bytes);

} // namespace pxc::render
