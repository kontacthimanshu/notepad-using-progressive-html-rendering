// Cascade, inheritance and font-family alias mapping (contracts/html-subset.md), and flattening of the styled
// tree into block boxes of styled runs for layout::layoutBlocks.
#include "pxc/html/document_node.hpp"

#include <algorithm>
#include <cmath>

namespace pxc::html {

using presentcss::Declaration;
using presentcss::DisplayValue;
using presentcss::Property;

namespace {

struct Candidate {
    int specificity;
    std::size_t order;
    const Declaration* decl;
};

DisplayValue defaultDisplay(Tag t) {
    switch (t) {
    case Tag::Html:
    case Tag::Body:
    case Tag::Div:
    case Tag::P: return DisplayValue::Block;
    case Tag::Head:
    case Tag::Title:
    case Tag::Style: return DisplayValue::None;
    default: return DisplayValue::Inline;
    }
}

struct Cascade {
    const fonts::FontRegistry* registry;
    std::vector<presentcss::Rule> rules;
    core::DiagnosticsLog* log;

    void apply(DocumentNode& n, const ComputedStyle& parent) {
        ComputedStyle s;
        // Inherited properties.
        s.color = parent.color;
        s.packId = parent.packId;
        s.fontExplicit = parent.fontExplicit;
        s.lineHeightMultiplier = parent.lineHeightMultiplier;
        s.lineHeightPx = parent.lineHeightPx;
        if (n.tag == Tag::Text) {
            s.background = parent.background;
            s.display = DisplayValue::Inline;
            n.style = s;
            return;
        }
        // Non-inherited defaults.
        s.display = defaultDisplay(n.tag);
        bool pDefaultMargins = n.tag == Tag::P;

        std::vector<Candidate> cands;
        const std::string tag = n.tag == Tag::Unknown ? n.rawTag : tagName(n.tag);
        for (const auto& r : rules)
            for (const auto& sel : r.selectors)
                if (sel.matches(tag, n.id, n.classes))
                    for (const auto& d : r.declarations) cands.push_back({sel.specificity(), r.order, &d});
        std::vector<Declaration> inlineDecls;
        if (!n.inlineStyle.empty()) inlineDecls = presentcss::parseInlineStyle(n.inlineStyle, *log);
        for (const auto& d : inlineDecls) cands.push_back({1000, static_cast<std::size_t>(-1), &d});
        std::stable_sort(cands.begin(), cands.end(), [](const Candidate& a, const Candidate& b) {
            if (a.specificity != b.specificity) return a.specificity < b.specificity;
            return a.order < b.order;
        });

        // font-family first so that em units resolve against the element's own pack.
        for (const auto& c : cands) {
            if (c.decl->property != Property::FontFamily) continue;
            for (const auto& fam : c.decl->families) {
                const std::string pack = registry->packForAlias(fam);
                if (!pack.empty()) {
                    s.packId = pack;
                    s.fontExplicit = true;
                    break;
                }
            }
        }
        const double em = registry->metrics(s.packId).lineHeight;
        double marginTop = pDefaultMargins ? em : 0, marginBottom = pDefaultMargins ? em : 0;
        for (const auto& c : cands) {
            const Declaration& d = *c.decl;
            switch (d.property) {
            case Property::Color: s.color = *d.color; break;
            case Property::BackgroundColor: s.background = *d.color; break;
            case Property::FontFamily: break;
            case Property::MarginTop:
            case Property::MarginBottom: {
                const double px = d.length->unit == presentcss::Unit::Em ? d.length->value * em : d.length->value;
                (d.property == Property::MarginTop ? marginTop : marginBottom) = px;
                break;
            }
            case Property::LineHeight:
                if (d.lineHeightNormal) { s.lineHeightMultiplier = 0; s.lineHeightPx = 0; }
                else if (d.length->unit == presentcss::Unit::Px) { s.lineHeightPx = d.length->value; s.lineHeightMultiplier = 0; }
                else { s.lineHeightMultiplier = d.length->value; s.lineHeightPx = 0; }
                break;
            case Property::Display: s.display = d.display; break;
            }
        }
        s.marginTopPx = marginTop;
        s.marginBottomPx = marginBottom;
        n.style = s;
        for (auto& c : n.children) apply(*c, n.style);
    }
};

// ---- flattening ----

struct Flattener {
    std::vector<layout::StyledBlock> blocks;
    layout::StyledBlock cur;
    bool haveCur = false;
    double pendingTop = 0;
    core::Color curBlockBg{0, 0, 0, 0};

    void begin() {
        if (!haveCur) {
            cur = layout::StyledBlock{};
            cur.background = curBlockBg;
            cur.marginTopPx = static_cast<int>(std::lround(pendingTop));
            pendingTop = 0;
            haveCur = true;
        }
    }
    void flush() {
        if (!haveCur) return;
        // Trim the trailing space of the block.
        while (!cur.runs.empty()) {
            auto& t = cur.runs.back().text;
            while (!t.empty() && t.back() == U' ') t.pop_back();
            if (t.empty()) cur.runs.pop_back();
            else break;
        }
        if (!cur.runs.empty() || cur.marginTopPx || cur.marginBottomPx) blocks.push_back(std::move(cur));
        haveCur = false;
    }
    bool endsWithSpaceOrEmpty() const {
        for (auto it = cur.runs.rbegin(); it != cur.runs.rend(); ++it)
            if (!it->text.empty()) return it->text.back() == U' ' || it->text.back() == U'\n';
        return true;
    }
    void addText(const DocumentNode& t, const DocumentNode& parent) {
        std::u32string text = t.text;
        for (auto& c : text)
            if (c == 0xA0) c = U' '; // &nbsp; renders as a (non-collapsing) space
        begin();
        if (!text.empty() && t.text.front() == U' ' && endsWithSpaceOrEmpty()) text.erase(text.begin());
        if (text.empty()) return;
        layout::StyledRun r;
        r.text = std::move(text);
        r.fg = t.style.color;
        r.bg = parent.style.display == DisplayValue::Inline ? parent.style.background : core::Color{0, 0, 0, 0};
        r.packId = t.style.packId;
        r.lineHeightMultiplier = t.style.lineHeightMultiplier;
        r.lineHeightPx = t.style.lineHeightPx;
        cur.runs.push_back(std::move(r));
    }
    void walk(const DocumentNode& n) {
        for (const auto& c : n.children) {
            if (c->tag == Tag::Text) {
                addText(*c, n);
                continue;
            }
            if (c->style.display == DisplayValue::None) continue;
            if (c->tag == Tag::Br) {
                begin();
                layout::StyledRun r;
                r.text = U"\n";
                r.packId = n.style.packId;
                r.fg = n.style.color;
                r.lineHeightMultiplier = n.style.lineHeightMultiplier;
                r.lineHeightPx = n.style.lineHeightPx;
                cur.runs.push_back(std::move(r));
                continue;
            }
            if (c->style.display == DisplayValue::Block) {
                flush();
                const core::Color saved = curBlockBg;
                if (c->style.background.a > 0) curBlockBg = c->style.background;
                pendingTop = std::max(pendingTop, c->style.marginTopPx);
                walk(*c);
                flush();
                if (!blocks.empty())
                    blocks.back().marginBottomPx = std::max(blocks.back().marginBottomPx,
                                                            static_cast<int>(std::lround(c->style.marginBottomPx)));
                curBlockBg = saved;
                continue;
            }
            walk(*c); // inline element (span, unknown transparent container)
        }
    }
};

} // namespace

void computeStyles(PreviewDocument& doc, const fonts::FontRegistry& registry, std::string_view activePackId,
                   core::DiagnosticsLog& log) {
    Cascade c;
    c.registry = &registry;
    c.log = &log;
    std::size_t order = 0;
    for (const auto& css : doc.styleSheets) {
        auto sheet = presentcss::parseStyleSheet(css, log, order);
        order += sheet.rules.size();
        for (auto& r : sheet.rules) c.rules.push_back(std::move(r));
    }
    ComputedStyle rootStyle;
    rootStyle.packId = std::string(activePackId);
    rootStyle.color = core::Color{0, 0, 0, 255};
    if (doc.root) c.apply(*doc.root, rootStyle);
}

std::vector<layout::StyledBlock> toStyledBlocks(const PreviewDocument& doc) {
    Flattener f;
    if (!doc.root) return {};
    for (const auto& c : doc.root->children)
        if (c->tag == Tag::Body) f.walk(*c);
    f.flush();
    return f.blocks;
}

} // namespace pxc::html
