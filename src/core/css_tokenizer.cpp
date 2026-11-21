#include "pxc/core/css_tokenizer.hpp"

#include <charconv>

#include "pxc/core/unicode.hpp"

namespace pxc::core {

const char* tokenKindName(CssTokenKind k) noexcept {
    switch (k) {
    case CssTokenKind::Ident: return "ident";
    case CssTokenKind::AtKeyword: return "at-keyword";
    case CssTokenKind::String: return "string";
    case CssTokenKind::Hash: return "hash";
    case CssTokenKind::Number: return "number";
    case CssTokenKind::Percentage: return "percentage";
    case CssTokenKind::Dimension: return "dimension";
    case CssTokenKind::UnicodeRange: return "unicode-range";
    case CssTokenKind::Function: return "function";
    case CssTokenKind::Delim: return "delim";
    case CssTokenKind::Colon: return "colon";
    case CssTokenKind::Semicolon: return "semicolon";
    case CssTokenKind::Comma: return "comma";
    case CssTokenKind::LeftBrace: return "{";
    case CssTokenKind::RightBrace: return "}";
    case CssTokenKind::LeftParen: return "(";
    case CssTokenKind::RightParen: return ")";
    case CssTokenKind::LeftBracket: return "[";
    case CssTokenKind::RightBracket: return "]";
    case CssTokenKind::Whitespace: return "whitespace";
    case CssTokenKind::BadString: return "bad-string";
    case CssTokenKind::Eof: return "eof";
    }
    return "?";
}

namespace {

bool isHex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return c - 'A' + 10;
}
bool isDigit(char c) { return c >= '0' && c <= '9'; }
bool isNameStart(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || static_cast<unsigned char>(c) >= 0x80;
}
bool isNameChar(char c) { return isNameStart(c) || isDigit(c) || c == '-'; }
bool isWs(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'; }

class Tokenizer {
public:
    Tokenizer(std::string_view in, std::vector<Diagnostic>& diags, const std::filesystem::path* file)
        : s_(in), diags_(diags), file_(file) {}

    std::vector<CssToken> run() {
        std::vector<CssToken> out;
        for (;;) {
            CssToken t = next();
            const bool eof = t.kind == CssTokenKind::Eof;
            out.push_back(std::move(t));
            if (eof) break;
        }
        return out;
    }

private:
    std::string_view s_;
    std::vector<Diagnostic>& diags_;
    const std::filesystem::path* file_;
    std::size_t i_ = 0;
    int line_ = 1, col_ = 1;

    char peek(std::size_t k = 0) const { return i_ + k < s_.size() ? s_[i_ + k] : '\0'; }
    bool atEnd() const { return i_ >= s_.size(); }

    void advance() {
        if (atEnd()) return;
        const char c = s_[i_++];
        if (c == '\n') { ++line_; col_ = 1; }
        else if (c == '\r') {
            if (peek() == '\n') { ++i_; }
            ++line_; col_ = 1;
        } else if (c == '\f') { ++line_; col_ = 1; }
        else { ++col_; }
    }

    void error(const std::string& msg, int line, int col) {
        diags_.push_back(makeError("E-CSS-SYNTAX", msg, file_, line, col));
    }

    bool startsIdent(std::size_t k = 0) const {
        const char c0 = peek(k);
        if (c0 == '-') {
            const char c1 = peek(k + 1);
            return isNameStart(c1) || c1 == '-' || (c1 == '\\' && peek(k + 2) != '\n');
        }
        if (c0 == '\\') return peek(k + 1) != '\n' && peek(k + 1) != '\0';
        return isNameStart(c0);
    }

    bool startsNumber() const {
        const char c0 = peek();
        if (isDigit(c0)) return true;
        if (c0 == '.' && isDigit(peek(1))) return true;
        if (c0 == '+' || c0 == '-') {
            return isDigit(peek(1)) || (peek(1) == '.' && isDigit(peek(2)));
        }
        return false;
    }

    // Consumes an escape after the backslash has been consumed.
    std::string consumeEscape() {
        if (isHex(peek())) {
            char32_t cp = 0;
            for (int k = 0; k < 6 && isHex(peek()); ++k) {
                cp = cp * 16 + static_cast<char32_t>(hexValue(peek()));
                advance();
            }
            if (isWs(peek())) advance();
            if (cp == 0 || !isValidScalar(cp)) cp = kReplacementChar;
            return encodeUtf8(cp);
        }
        if (atEnd()) return encodeUtf8(kReplacementChar);
        std::string out(1, peek());
        advance();
        return out;
    }

    std::string consumeName() {
        std::string out;
        for (;;) {
            const char c = peek();
            if (isNameChar(c)) { out += c; advance(); }
            else if (c == '\\' && peek(1) != '\n' && peek(1) != '\0') { advance(); out += consumeEscape(); }
            else break;
        }
        return out;
    }

    CssToken make(CssTokenKind k, int line, int col) {
        CssToken t;
        t.kind = k;
        t.line = line;
        t.column = col;
        return t;
    }

    CssToken next() {
        // Skip comments (possibly several), but report unterminated ones.
        for (;;) {
            if (peek() == '/' && peek(1) == '*') {
                const int l = line_, c = col_;
                advance(); advance();
                bool closed = false;
                while (!atEnd()) {
                    if (peek() == '*' && peek(1) == '/') { advance(); advance(); closed = true; break; }
                    advance();
                }
                if (!closed) error("unterminated comment", l, c);
                continue;
            }
            break;
        }
        const int line = line_, col = col_;
        if (atEnd()) return make(CssTokenKind::Eof, line, col);

        const char c = peek();
        if (isWs(c)) {
            while (isWs(peek())) advance();
            return make(CssTokenKind::Whitespace, line, col);
        }
        if (c == '"' || c == '\'') return consumeString(c, line, col);
        if (c == '#') {
            if (isNameChar(peek(1)) || (peek(1) == '\\' && peek(2) != '\n')) {
                advance();
                CssToken t = make(CssTokenKind::Hash, line, col);
                t.text = consumeName();
                return t;
            }
            advance();
            CssToken t = make(CssTokenKind::Delim, line, col);
            t.text = "#";
            return t;
        }
        if ((c == 'u' || c == 'U') && peek(1) == '+' && (isHex(peek(2)) || peek(2) == '?')) {
            return consumeUnicodeRange(line, col);
        }
        if (startsNumber()) return consumeNumeric(line, col);
        if (startsIdent()) {
            std::string name = consumeName();
            if (peek() == '(') {
                advance();
                CssToken t = make(CssTokenKind::Function, line, col);
                t.text = std::move(name);
                return t;
            }
            CssToken t = make(CssTokenKind::Ident, line, col);
            t.text = std::move(name);
            return t;
        }
        if (c == '@' && startsIdent(1)) {
            advance();
            CssToken t = make(CssTokenKind::AtKeyword, line, col);
            t.text = consumeName();
            return t;
        }

        advance();
        switch (c) {
        case ':': return make(CssTokenKind::Colon, line, col);
        case ';': return make(CssTokenKind::Semicolon, line, col);
        case ',': return make(CssTokenKind::Comma, line, col);
        case '{': return make(CssTokenKind::LeftBrace, line, col);
        case '}': return make(CssTokenKind::RightBrace, line, col);
        case '(': return make(CssTokenKind::LeftParen, line, col);
        case ')': return make(CssTokenKind::RightParen, line, col);
        case '[': return make(CssTokenKind::LeftBracket, line, col);
        case ']': return make(CssTokenKind::RightBracket, line, col);
        default: {
            CssToken t = make(CssTokenKind::Delim, line, col);
            t.text = std::string(1, c);
            return t;
        }
        }
    }

    CssToken consumeString(char quote, int line, int col) {
        advance(); // opening quote
        CssToken t = make(CssTokenKind::String, line, col);
        for (;;) {
            if (atEnd()) {
                error("unterminated string", line, col);
                t.kind = CssTokenKind::BadString;
                return t;
            }
            const char c = peek();
            if (c == quote) { advance(); return t; }
            if (c == '\n' || c == '\r' || c == '\f') {
                error("unterminated string (newline inside string)", line, col);
                t.kind = CssTokenKind::BadString;
                return t;
            }
            if (c == '\\') {
                advance();
                if (atEnd()) continue;
                if (peek() == '\n' || peek() == '\r') { advance(); continue; } // line continuation
                t.text += consumeEscape();
                continue;
            }
            t.text += c;
            advance();
        }
    }

    CssToken consumeNumeric(int line, int col) {
        std::string repr;
        bool isInt = true;
        if (peek() == '+' || peek() == '-') { repr += peek(); advance(); }
        while (isDigit(peek())) { repr += peek(); advance(); }
        if (peek() == '.' && isDigit(peek(1))) {
            isInt = false;
            repr += '.'; advance();
            while (isDigit(peek())) { repr += peek(); advance(); }
        }
        double value = 0.0;
        {
            const char* b = repr.data();
            const char* e = repr.data() + repr.size();
            if (*b == '+') ++b;
            std::from_chars(b, e, value);
        }
        CssToken t;
        t.line = line;
        t.column = col;
        t.number = value;
        t.isInteger = isInt;
        t.text = repr;
        if (peek() == '%') {
            advance();
            t.kind = CssTokenKind::Percentage;
        } else if (startsIdent()) {
            t.kind = CssTokenKind::Dimension;
            t.unit = consumeName();
        } else {
            t.kind = CssTokenKind::Number;
        }
        return t;
    }

    CssToken consumeUnicodeRange(int line, int col) {
        advance(); advance(); // "U+"
        CssToken t = make(CssTokenKind::UnicodeRange, line, col);
        char32_t start = 0;
        int digits = 0;
        while (isHex(peek()) && digits < 6) { start = start * 16 + static_cast<char32_t>(hexValue(peek())); advance(); ++digits; }
        char32_t end = start;
        if (peek() == '-' && isHex(peek(1))) {
            advance();
            end = 0;
            digits = 0;
            while (isHex(peek()) && digits < 6) { end = end * 16 + static_cast<char32_t>(hexValue(peek())); advance(); ++digits; }
        }
        t.rangeStart = start;
        t.rangeEnd = end;
        return t;
    }
};

} // namespace

std::vector<CssToken> tokenizeCss(std::string_view input, std::vector<Diagnostic>& diags,
                                  const std::filesystem::path* file) {
    // Skip a UTF-8 BOM if present.
    if (input.size() >= 3 && static_cast<unsigned char>(input[0]) == 0xEF &&
        static_cast<unsigned char>(input[1]) == 0xBB && static_cast<unsigned char>(input[2]) == 0xBF) {
        input.remove_prefix(3);
    }
    return Tokenizer(input, diags, file).run();
}

} // namespace pxc::core
