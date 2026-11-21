// Tree builder for the restricted HTML subset (contracts/html-subset.md): implied html/head/body, dropped
// unsupported elements, transparent unknown containers, attribute whitelist, implied </p>, stray-end-tag
// recovery, auto-close at EOF and whitespace collapsing. Scripts never run; nothing is fetched (FR-040).
#include "pxc/html/document_node.hpp"

#include <algorithm>
#include <cstdio>

#include "pxc/core/unicode.hpp"
#include "tokenizer.hpp"

namespace pxc::html {

using detail::Token;

const char* tagName(Tag t) noexcept {
    switch (t) {
    case Tag::Html: return "html";
    case Tag::Head: return "head";
    case Tag::Body: return "body";
    case Tag::Title: return "title";
    case Tag::Div: return "div";
    case Tag::P: return "p";
    case Tag::Span: return "span";
    case Tag::Br: return "br";
    case Tag::Style: return "style";
    case Tag::Unknown: return "unknown";
    case Tag::Text: return "#text";
    }
    return "?";
}

bool PreviewDocument::hasErrors() const noexcept {
    return std::any_of(diagnostics.begin(), diagnostics.end(),
                       [](const core::Diagnostic& d) { return d.severity == core::Severity::Error; });
}

namespace {

bool isWs(char32_t c) { return c == U' ' || c == U'\t' || c == U'\n' || c == U'\r' || c == U'\f'; }
bool isBlock(const DocumentNode& n) { return n.tag == Tag::Div || n.tag == Tag::P; }

std::unique_ptr<DocumentNode> makeNode(Tag tag, std::string raw, DocumentNode* parent, int line = 0, int col = 0) {
    auto n = std::make_unique<DocumentNode>();
    n->tag = tag;
    n->rawTag = std::move(raw);
    n->parent = parent;
    n->line = line;
    n->column = col;
    return n;
}

Tag knownTag(const std::string& name) {
    if (name == "div") return Tag::Div;
    if (name == "p") return Tag::P;
    if (name == "span") return Tag::Span;
    if (name == "br") return Tag::Br;
    return Tag::Unknown;
}

bool isDroppedElement(const std::string& n) {
    static const char* dropped[] = {"script", "noscript", "iframe", "object", "embed", "img",
                                    "video", "audio", "link", "meta", "base"};
    return std::any_of(std::begin(dropped), std::end(dropped), [&](const char* d) { return n == d; });
}
bool isVoidDropped(const std::string& n) {
    return n == "img" || n == "link" || n == "meta" || n == "base" || n == "embed";
}

void appendText(DocumentNode* parent, const std::u32string& text) {
    if (text.empty()) return;
    if (!parent->children.empty() && parent->children.back()->tag == Tag::Text) {
        parent->children.back()->text += text;
        return;
    }
    auto t = makeNode(Tag::Text, "#text", parent);
    t->text = text;
    parent->children.push_back(std::move(t));
}

std::u32string collapse(const std::u32string& s) {
    std::u32string o;
    bool space = false;
    for (char32_t c : s) {
        if (isWs(c)) {
            if (!space) o += U' ';
            space = true;
        } else {
            o += c;
            space = false;
        }
    }
    return o;
}

// Collapses whitespace in text nodes and removes whitespace-only nodes that sit next to block boundaries.
void normalize(DocumentNode& n) {
    for (auto& c : n.children)
        if (c->tag == Tag::Text) c->text = collapse(c->text);
    for (std::size_t i = 0; i < n.children.size();) {
        auto& c = n.children[i];
        const bool wsOnly = c->tag == Tag::Text && std::all_of(c->text.begin(), c->text.end(), [](char32_t ch) { return ch == U' '; });
        if (wsOnly) {
            const DocumentNode* prev = i > 0 ? n.children[i - 1].get() : nullptr;
            const DocumentNode* next = i + 1 < n.children.size() ? n.children[i + 1].get() : nullptr;
            const bool parentInline = n.tag == Tag::Span || n.tag == Tag::Unknown;
            const bool keep = (prev && !isBlock(*prev) && next && !isBlock(*next)) || (parentInline && (prev || next));
            if (!keep || c->text.empty()) {
                n.children.erase(n.children.begin() + static_cast<std::ptrdiff_t>(i));
                continue;
            }
        }
        if (c->tag != Tag::Text) normalize(*c);
        ++i;
    }
    // Leading/trailing collapsible whitespace at the edges of a block never renders: trim it.
    if (n.tag == Tag::Body || n.tag == Tag::Div || n.tag == Tag::P) {
        if (!n.children.empty() && n.children.front()->tag == Tag::Text) {
            auto& t = n.children.front()->text;
            if (!t.empty() && t.front() == U' ') t.erase(t.begin());
        }
        if (!n.children.empty() && n.children.back()->tag == Tag::Text) {
            auto& t = n.children.back()->text;
            if (!t.empty() && t.back() == U' ') t.pop_back();
        }
        std::erase_if(n.children, [](const std::unique_ptr<DocumentNode>& c) { return c->tag == Tag::Text && c->text.empty(); });
    }
}

std::string escapeDump(const std::u32string& s) {
    std::string o;
    for (char ch : core::encodeUtf8(s)) {
        if (ch == '"' || ch == '\\') o += '\\';
        if (ch == '\n') { o += "\\n"; continue; }
        o += ch;
    }
    return o;
}

void dump(const DocumentNode& n, int depth, std::string& out) {
    out.append(static_cast<std::size_t>(depth) * 2, ' ');
    if (n.tag == Tag::Text) {
        out += "#text \"" + escapeDump(n.text) + "\"\n";
        return;
    }
    out += n.tag == Tag::Unknown ? n.rawTag : tagName(n.tag);
    if (!n.id.empty()) out += "#" + n.id;
    for (const auto& c : n.classes) out += "." + c;
    if (!n.inlineStyle.empty()) out += " style=\"" + n.inlineStyle + "\"";
    out += "\n";
    for (const auto& c : n.children) dump(*c, depth + 1, out);
}

} // namespace

std::string dumpTree(const PreviewDocument& doc) {
    std::string out;
    if (doc.root) dump(*doc.root, 0, out);
    return out;
}

core::Expected<PreviewDocument, std::vector<core::Diagnostic>> parseHtml(std::u32string_view source) {
    PreviewDocument doc;
    std::vector<core::Diagnostic> diags;
    const auto tokens = detail::tokenize(source, diags);

    doc.root = makeNode(Tag::Html, "html", nullptr);
    DocumentNode* html = doc.root.get();
    html->children.push_back(makeNode(Tag::Head, "head", html));
    DocumentNode* head = html->children.back().get();
    html->children.push_back(makeNode(Tag::Body, "body", html));
    DocumentNode* body = html->children.back().get();

    bool inHead = true;
    std::vector<DocumentNode*> stack{body};
    DocumentNode* title = nullptr;
    int skipDepth = 0;          // inside a dropped element with content (noscript, iframe, object, video, audio)
    std::string skipName;
    bool skipRawScript = false; // next raw-text token belongs to a dropped <script>

    auto warn = [&](const char* code, const std::string& msg, const Token& t) {
        diags.push_back(core::makeWarning(code, msg + " (line " + std::to_string(t.line) + ", column " +
                                                    std::to_string(t.column) + ")",
                                          nullptr, t.line, t.column));
    };

    for (std::size_t ti = 0; ti < tokens.size(); ++ti) {
        const Token& t = tokens[ti];
        if (t.kind == Token::Kind::Eof || t.kind == Token::Kind::Malformed) break;
        if (t.kind == Token::Kind::Comment || t.kind == Token::Kind::Doctype) continue;

        if (skipDepth > 0) {
            if (t.kind == Token::Kind::StartTag && t.name == skipName) ++skipDepth;
            if (t.kind == Token::Kind::EndTag && t.name == skipName) --skipDepth;
            continue;
        }

        if (t.kind == Token::Kind::Text) {
            if (t.name == "#raw") {
                if (skipRawScript) { skipRawScript = false; continue; }
                // <style> contents: collected as a style sheet, never rendered.
                doc.styleSheets.push_back(core::encodeUtf8(t.text));
                continue;
            }
            if (title) {
                appendText(title, t.text);
                continue;
            }
            const bool wsOnly = std::all_of(t.text.begin(), t.text.end(), isWs);
            if (inHead && wsOnly) continue;
            inHead = false;
            appendText(stack.back(), t.text);
            continue;
        }

        if (t.kind == Token::Kind::EndTag) {
            const std::string& n = t.name;
            if (n == "html" || n == "body") continue;
            if (n == "head") { inHead = false; continue; }
            if (n == "title") { title = nullptr; continue; }
            if (n == "style" || n == "script") continue;
            if (n == "br") { // </br> is treated as <br>
                stack.back()->children.push_back(makeNode(Tag::Br, "br", stack.back(), t.line, t.column));
                continue;
            }
            auto it = std::find_if(stack.rbegin(), stack.rend() - 1, [&](DocumentNode* e) { return e->rawTag == n; });
            if (it == stack.rend() - 1) {
                warn("W-HTML-STRAY-END-TAG", "end tag </" + n + "> has no matching open element; ignored", t);
                continue;
            }
            const auto keep = static_cast<std::size_t>(std::distance(it, stack.rend())) - 1;
            stack.resize(keep);
            continue;
        }

        // Start tags.
        const std::string& n = t.name;
        if (n == "html") continue;
        if (n == "head") { inHead = true; continue; }
        if (n == "body") { inHead = false; continue; }
        if (n == "title") {
            head->children.push_back(makeNode(Tag::Title, "title", head, t.line, t.column));
            title = head->children.back().get();
            continue;
        }
        if (n == "style") {
            head->children.push_back(makeNode(Tag::Style, "style", head, t.line, t.column));
            continue; // the following raw-text token is captured as a style sheet
        }
        if (isDroppedElement(n)) {
            warn("W-HTML-UNSUPPORTED-ELEMENT", "<" + n + "> is not supported; dropped with its content", t);
            if (n == "script") skipRawScript = !t.selfClosing;
            else if (!isVoidDropped(n) && !t.selfClosing) { skipDepth = 1; skipName = n; }
            continue;
        }
        inHead = false;
        const Tag tag = knownTag(n);
        if (tag == Tag::Br) {
            stack.back()->children.push_back(makeNode(Tag::Br, "br", stack.back(), t.line, t.column));
            continue;
        }
        if (tag == Tag::Div || tag == Tag::P) {
            // Implied end of an open <p> before a block element.
            auto pIt = std::find_if(stack.rbegin(), stack.rend() - 1, [](DocumentNode* e) { return e->tag == Tag::P; });
            if (pIt != stack.rend() - 1) stack.resize(static_cast<std::size_t>(std::distance(pIt, stack.rend())) - 1);
        }
        if (tag == Tag::Unknown) warn("W-HTML-UNKNOWN-ELEMENT", "<" + n + "> is not supported; its content is shown inline", t);
        auto el = makeNode(tag, n, stack.back(), t.line, t.column);
        for (const auto& [an, av] : t.attrs) {
            if (an == "id") el->id = core::encodeUtf8(av);
            else if (an == "class") {
                std::string cur;
                for (char32_t c : av) {
                    if (isWs(c)) { if (!cur.empty()) el->classes.push_back(cur); cur.clear(); }
                    else cur += core::encodeUtf8(c);
                }
                if (!cur.empty()) el->classes.push_back(cur);
            } else if (an == "style") el->inlineStyle = core::encodeUtf8(av);
            else if (an.size() > 2 && an[0] == 'o' && an[1] == 'n')
                warn("W-HTML-EVENT-HANDLER-IGNORED", "event handler attribute '" + an + "' removed; scripts never run", t);
            // href, src and every other attribute are discarded: nothing is ever resolved or fetched.
        }
        DocumentNode* raw = el.get();
        stack.back()->children.push_back(std::move(el));
        if (!t.selfClosing) stack.push_back(raw);
    }

    normalize(*body);
    if (!head->children.empty())
        for (auto& c : head->children)
            if (c->tag == Tag::Title) {
                for (auto& tc : c->children) tc->text = collapse(tc->text);
                for (auto& tc : c->children) doc.title += tc->text;
            }
    // Trim the title.
    while (!doc.title.empty() && doc.title.front() == U' ') doc.title.erase(doc.title.begin());
    while (!doc.title.empty() && doc.title.back() == U' ') doc.title.pop_back();
    doc.diagnostics = std::move(diags);
    return doc;
}

} // namespace pxc::html
