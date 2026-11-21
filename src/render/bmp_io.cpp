#include "pxc/render/bmp_io.hpp"

#include <cstring>
#include <fstream>
#include <iterator>

namespace pxc::render {

namespace {

constexpr std::size_t kFileHeader = 14;
constexpr std::size_t kInfoHeader = 40;
constexpr std::size_t kHeaders = kFileHeader + kInfoHeader;

void put16(std::string& s, std::uint16_t v) {
    s.push_back(static_cast<char>(v & 0xFF));
    s.push_back(static_cast<char>((v >> 8) & 0xFF));
}
void put32(std::string& s, std::uint32_t v) {
    for (int k = 0; k < 4; ++k) s.push_back(static_cast<char>((v >> (8 * k)) & 0xFF));
}
std::uint32_t get32(std::string_view s, std::size_t off) {
    std::uint32_t v = 0;
    for (int k = 3; k >= 0; --k) v = (v << 8) | static_cast<unsigned char>(s[off + static_cast<std::size_t>(k)]);
    return v;
}
std::uint16_t get16(std::string_view s, std::size_t off) {
    return static_cast<std::uint16_t>(static_cast<unsigned char>(s[off]) |
                                      (static_cast<unsigned char>(s[off + 1]) << 8));
}

} // namespace

std::string encodeBmp32(const Framebuffer& fb) {
    const std::uint32_t pixelBytes = static_cast<std::uint32_t>(fb.width()) * static_cast<std::uint32_t>(fb.height()) * 4u;
    std::string out;
    out.reserve(kHeaders + pixelBytes);
    // BITMAPFILEHEADER
    out += "BM";
    put32(out, static_cast<std::uint32_t>(kHeaders) + pixelBytes);
    put16(out, 0);
    put16(out, 0);
    put32(out, static_cast<std::uint32_t>(kHeaders));
    // BITMAPINFOHEADER (negative height = top-down)
    put32(out, static_cast<std::uint32_t>(kInfoHeader));
    put32(out, static_cast<std::uint32_t>(fb.width()));
    put32(out, static_cast<std::uint32_t>(-fb.height()));
    put16(out, 1);  // planes
    put16(out, 32); // bpp
    put32(out, 0);  // BI_RGB
    put32(out, pixelBytes);
    put32(out, 2835); // 72 dpi
    put32(out, 2835);
    put32(out, 0);
    put32(out, 0);
    for (std::uint32_t px : fb.pixels()) put32(out, px); // little-endian BGRA
    return out;
}

core::Expected<Framebuffer, std::string> decodeBmp32(std::string_view s) {
    if (s.size() < kHeaders || s[0] != 'B' || s[1] != 'M') return core::Unexpected(std::string("not a BMP file"));
    const std::uint32_t dataOff = get32(s, 10);
    const std::uint32_t infoSize = get32(s, 14);
    if (infoSize < kInfoHeader) return core::Unexpected(std::string("unsupported BMP header"));
    const auto w = static_cast<std::int32_t>(get32(s, 18));
    const auto hRaw = static_cast<std::int32_t>(get32(s, 22));
    const std::uint16_t bpp = get16(s, 28);
    const std::uint32_t compression = get32(s, 30);
    if (bpp != 32 || compression != 0) return core::Unexpected(std::string("only 32-bit BI_RGB BMP is supported"));
    if (w < 0 || w > kMaxFramebufferDim || hRaw == INT32_MIN) return core::Unexpected(std::string("bad BMP size"));
    const bool topDown = hRaw < 0;
    const int h = topDown ? -hRaw : hRaw;
    if (h > kMaxFramebufferDim) return core::Unexpected(std::string("bad BMP size"));
    const std::size_t need = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u;
    if (dataOff > s.size() || s.size() - dataOff < need) return core::Unexpected(std::string("truncated BMP data"));
    auto fbx = Framebuffer::create(w, h);
    if (!fbx) return core::Unexpected(fbx.error());
    Framebuffer fb = std::move(*fbx);
    for (int y = 0; y < h; ++y) {
        const int srcY = topDown ? y : (h - 1 - y);
        auto rw = fb.row(y);
        for (int x = 0; x < w; ++x) {
            rw[static_cast<std::size_t>(x)] =
                get32(s, dataOff + (static_cast<std::size_t>(srcY) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)) * 4u);
        }
    }
    return fb;
}

core::Expected<void, std::string> writeBmp32(const Framebuffer& fb, const std::filesystem::path& path) {
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return core::Unexpected("cannot open for writing: " + path.generic_string());
    const std::string bytes = encodeBmp32(fb);
    f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!f) return core::Unexpected("write failed: " + path.generic_string());
    return {};
}

core::Expected<Framebuffer, std::string> readBmp32(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return core::Unexpected("cannot open: " + path.generic_string());
    std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return decodeBmp32(bytes);
}

} // namespace pxc::render
