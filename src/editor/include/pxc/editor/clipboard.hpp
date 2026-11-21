#pragma once
// Clipboard seam: ordinary Unicode text only, never glyph pixels (FR-030).

#include <optional>
#include <string>
#include <string_view>

namespace pxc::editor {

struct IClipboard {
    virtual std::optional<std::u32string> getText() = 0;
    virtual bool setText(std::u32string_view text) = 0;
    virtual ~IClipboard() = default;
};

} // namespace pxc::editor
