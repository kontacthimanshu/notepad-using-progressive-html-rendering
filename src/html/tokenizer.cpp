#include "tokenizer.hpp"

#include <cctype>

#include "pxc/core/unicode.hpp"

namespace pxc::html::detail {

namespace {

bool isAsciiAlpha(char32_t c) { return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z'); }
bool isWs(char32_t c) { return c == U' ' || c == U'\t' || c == U'\n' || c == U'\r' || c == U'\f'; }
char32_t lowerAscii(char32_t c) { return (c >= U'A' && c <= U'Z') ? c + 32 : c; }

std::string asciiLower(std::u32string_view s) {
    std::string o;
    for (char32_t c : s) o += static_cast<char>(lowerAscii(c) < 0x80 ? lowerAscii(c) : '?');
    return o;
}

class Tok {
public:
    Tok(std::u32string_view s, std::vector<core::Diagnostic>& d) : s_(s), diags_(d) {}

    std::vector<Token> run() {
        std::vector<Token> out;
        std::u32string text;
        int textLine = 1, textCol = 1;
        auto flushText = [&] {
            if (text.empty()) return;
            Token t;
            t.kind = Token::Kind::Text;
            t.text = decodeEntities(text, diags_, textLine, textCol);
            t.line = textLine;
            t.column = textCol;
            out.push_back(std::move(t));
            text.clear();
        };
        while (i_ < s_.size()) {
            const char32_t c = s_[i_];
            if (c == U'<' && i_ + 1 < s_.size()) {
                const char32_t n = s_[i_ + 1];
                if (isAsciiAlpha(n) || (n == U'/' && i_ + 2 < s_.size() && isAsciiAlpha(s_[i_ + 2])) || n == U'!' || n == U'?') {
                    flushText();
                    Token t;
                    const bool ok = n == U'!' || n == U'?' ? markup(t) : tag(t);
                    if (!ok) {
                        out.push_back(std::move(t));
                        return out; // malformed: stop; the parsed prefix is kept
                    }
                    const bool raw = t.kind == Token::Kind::StartTag && !t.selfClosing && (t.name == "script" || t.name == "style");
                    const std::string rawName = t.name;
                    out.push_back(std::move(t));
                    if (raw) out.push_back(rawText(rawName));
                    continue;
                }
            }
            if (text.empty()) {
                textLine = line_;
                textCol = col_;
            }
            text += c;
            advance();
        }
        flushText();
        Token eof;
        eof.kind = Token::Kind::Eof;
        eof.line = line_;
        eof.column = col_;
        out.push_back(eof);
        return out;
    }

private:
    std::u32string_view s_;
    std::vector<core::Diagnostic>& diags_;
    std::size_t i_ = 0;
    int line_ = 1, col_ = 1;

    void advance() {
        if (i_ >= s_.size()) return;
        if (s_[i_] == U'\n') { ++line_; col_ = 1; }
        else ++col_;
        ++i_;
    }
    bool at(std::u32string_view lit) const {
        if (i_ + lit.size() > s_.size()) return false;
        for (std::size_t k = 0; k < lit.size(); ++k)
            if (lowerAscii(s_[i_ + k]) != lit[k]) return false;
        return true;
    }
    void malformed(Token& t, int line, int col) {
        t.kind = Token::Kind::Malformed;
        t.line = line;
        t.column = col;
        diags_.push_back(core::makeError("E-HTML-MALFORMED", "malformed tag runs to the end of the document (line " +
                                                                 std::to_string(line) + ", column " + std::to_string(col) + ")",
                                         nullptr, line, col));
    }

    // <!-- comment -->, <!DOCTYPE ...>, <? ... >
    bool markup(Token& t) {
        const int line = line_, col = col_;
        t.line = line;
        t.column = col;
        if (at(U"<!--")) {
            for (int k = 0; k < 4; ++k) advance();
            while (i_ < s_.size() && !at(U"-->")) advance();
            if (i_ >= s_.size()) { malformed(t, line, col); return false; }
            for (int k = 0; k < 3; ++k) advance();
            t.kind = Token::Kind::Comment;
            return true;
        }
        while (i_ < s_.size() && s_[i_] != U'>') advance();
        if (i_ >= s_.size()) { malformed(t, line, col); return false; }
        advance();
        t.kind = Token::Kind::Doctype;
        return true;
    }

    bool tag(Token& t) {
        const int line = line_, col = col_;
        t.line = line;
        t.column = col;
        advance(); // '<'
        t.kind = Token::Kind::StartTag;
        if (s_[i_] == U'/') {
            t.kind = Token::Kind::EndTag;
            advance();
        }
        std::u32string name;
        while (i_ < s_.size() && !isWs(s_[i_]) && s_[i_] != U'>' && s_[i_] != U'/') {
            name += s_[i_];
            advance();
        }
        t.name = asciiLower(name);
        for (;;) {
            while (i_ < s_.size() && isWs(s_[i_])) advance();
            if (i_ >= s_.size()) { malformed(t, line, col); return false; }
            if (s_[i_] == U'>') { advance(); return true; }
            if (s_[i_] == U'/') {
                advance();
                if (i_ < s_.size() && s_[i_] == U'>') { t.selfClosing = true; advance(); return true; }
                continue;
            }
            std::u32string an;
            while (i_ < s_.size() && !isWs(s_[i_]) && s_[i_] != U'=' && s_[i_] != U'>' && s_[i_] != U'/') {
                an += s_[i_];
                advance();
            }
            while (i_ < s_.size() && isWs(s_[i_])) advance();
            std::u32string value;
            if (i_ < s_.size() && s_[i_] == U'=') {
                advance();
                while (i_ < s_.size() && isWs(s_[i_])) advance();
                if (i_ >= s_.size()) { malformed(t, line, col); return false; }
                if (s_[i_] == U'"' || s_[i_] == U'\'') {
                    const char32_t q = s_[i_];
                    advance();
                    while (i_ < s_.size() && s_[i_] != q) { value += s_[i_]; advance(); }
                    if (i_ >= s_.size()) { malformed(t, line, col); return false; }
                    advance();
                } else {
                    while (i_ < s_.size() && !isWs(s_[i_]) && s_[i_] != U'>') { value += s_[i_]; advance(); }
                }
            }
            if (!an.empty()) t.attrs.emplace_back(asciiLower(an), decodeEntities(value, diags_, line, col));
        }
    }

    Token rawText(const std::string& name) {
        Token t;
        t.kind = Token::Kind::Text;
        t.line = line_;
        t.column = col_;
        const std::u32string close = U"</" + std::u32string(name.begin(), name.end());
        while (i_ < s_.size() && !at(close)) {
            t.text += s_[i_];
            advance();
        }
        t.name = "#raw"; // marks raw text (never entity-decoded, never rendered for script)
        return t;
    }
};

} // namespace

std::u32string decodeEntities(std::u32string_view s, std::vector<core::Diagnostic>& diags, int line, int column) {
    std::u32string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != U'&') {
            out += s[i];
            continue;
        }
        std::size_t j = i + 1;
        while (j < s.size() && j - i <= 10 && s[j] != U';' && !isWs(s[j]) && s[j] != U'&' && s[j] != U'<') ++j;
        if (j >= s.size() || s[j] != U';' || j == i + 1) {
            out += s[i];
            continue;
        }
        const std::u32string_view name = s.substr(i + 1, j - i - 1);
        char32_t cp = 0;
        bool ok = true;
        if (name == U"amp") cp = U'&';
        else if (name == U"lt") cp = U'<';
        else if (name == U"gt") cp = U'>';
        else if (name == U"quot") cp = U'"';
        else if (name == U"apos") cp = U'\'';
        else if (name == U"nbsp") cp = 0xA0;
        else if (name.size() > 1 && name[0] == U'#') {
            const bool hex = name[1] == U'x' || name[1] == U'X';
            std::uint32_t v = 0;
            const std::size_t start = hex ? 2 : 1;
            ok = name.size() > start;
            for (std::size_t k = start; ok && k < name.size(); ++k) {
                const char32_t d = name[k];
                int dv = -1;
                if (d >= U'0' && d <= U'9') dv = static_cast<int>(d - U'0');
                else if (hex && d >= U'a' && d <= U'f') dv = static_cast<int>(d - U'a' + 10);
                else if (hex && d >= U'A' && d <= U'F') dv = static_cast<int>(d - U'A' + 10);
                if (dv < 0 || v > 0x10FFFF) ok = false;
                else v = v * (hex ? 16u : 10u) + static_cast<std::uint32_t>(dv);
            }
            cp = ok && core::isValidScalar(v) && v != 0 ? v : core::kReplacementChar;
        } else {
            ok = false;
        }
        if (!ok) {
            diags.push_back(core::makeWarning("W-HTML-ENTITY", "unknown entity &" + core::encodeUtf8(name) + "; kept literally",
                                              nullptr, line, column));
            out += s.substr(i, j - i + 1);
        } else {
            out += cp;
        }
        i = j;
    }
    return out;
}

std::vector<Token> tokenize(std::u32string_view src, std::vector<core::Diagnostic>& diags) {
    return Tok(src, diags).run();
}

} // namespace pxc::html::detail
