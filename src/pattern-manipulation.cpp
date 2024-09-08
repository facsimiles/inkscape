// SPDX-License-Identifier: GPL-2.0-or-later

#include <glibmm/fileutils.h>
#include <glibmm/i18n.h>
#include "pattern-manipulation.h"

#include "desktop-style.h"
#include "colors/color.h"
#include "document.h"
#include "fill-or-stroke.h"
#include "helper/stock-items.h"
#include "object/sp-pattern.h"
#include "xml/repr.h"

std::vector<SPDocument *> sp_get_stock_patterns() {
    auto patterns = StockPaintDocuments::get().get_paint_documents([](SPDocument* doc){
        return !sp_get_pattern_list(doc).empty();
    });
    if (patterns.empty()) {
        g_warning("No stock patterns!");
    }
    return patterns;
}

std::vector<SPPattern*> sp_get_pattern_list(SPDocument* source) {
    std::vector<SPPattern*> list;
    if (!source) return list;

    std::vector<SPObject*> patterns = source->getResourceList("pattern");
    for (auto pattern : patterns) {
        auto p = cast<SPPattern>(pattern);
        if (p && p == p->rootPattern() && p->hasChildren()) { // only if this is a vali root pattern
            list.push_back(cast<SPPattern>(pattern));
        }
    }

    return list;
}

void sp_pattern_set_color(SPPattern* pattern, Inkscape::Colors::Color const &c)
{
    if (!pattern) return;

    SPCSSAttr* css = sp_repr_css_attr_new();
    sp_repr_css_set_property_string(css, "fill", c.toString());
    pattern->changeCSS(css, "style");
    sp_repr_css_attr_unref(css);
}

void sp_pattern_set_transform(SPPattern* pattern, const Geom::Affine& transform) {
    if (!pattern) return;

    // for now, this is that simple
    pattern->transform_multiply(transform, true);
}

void sp_pattern_set_offset(SPPattern* pattern, const Geom::Point& offset) {
    if (!pattern) return;

    // TODO: verify
    pattern->setAttributeDouble("x", offset.x());
    pattern->setAttributeDouble("y", offset.y());
}

void sp_pattern_set_uniform_scale(SPPattern* pattern, bool uniform) {
    if (!pattern) return;

    //TODO: make smarter to keep existing value when possible
    pattern->setAttribute("preserveAspectRatio", uniform ? "xMidYMid" : "none");
}

void sp_pattern_set_gap(SPPattern* link_pattern, Geom::Scale gap_percent) {
    if (!link_pattern) return;
    auto root = link_pattern->rootPattern();
    if (!root || root == link_pattern) {
        g_assert(false && "Setting pattern gap requires link and root patterns objects");
        return;
    }

    auto set_gap = [=](double size, double percent, const char* attr) {
        if (percent == 0.0 || size <= 0.0) {
            // no gap
            link_pattern->removeAttribute(attr);
        }
        else if (percent > 0.0) {
            // positive gap
            link_pattern->setAttributeDouble(attr, size + size * percent / 100.0);
        }
        else if (percent < 0.0 && percent > -100.0) {
            // negative gap - overlap
            percent = -percent;
            link_pattern->setAttributeDouble(attr, size - size * percent / 100.0);
        }
    };

    set_gap(root->width(), gap_percent[Geom::X], "width");
    set_gap(root->height(), gap_percent[Geom::Y], "height");
}

Geom::Scale sp_pattern_get_gap(SPPattern* link_pattern) {
    Geom::Scale gap(0, 0);

    if (!link_pattern) return gap;
    auto root = link_pattern->rootPattern();
    if (!root || root == link_pattern) {
        g_assert(false && "Reading pattern gap requires link and root patterns objects");
        return gap;
    }

    auto get_gap = [=](double root_size, double link_size) {
        if (root_size > 0.0 && link_size > 0.0) {
            if (link_size > root_size) {
                return (link_size - root_size) / root_size;
            }
            else if (link_size < root_size) {
                return -link_size / root_size;
            }
        }
        return 0.0;
    };

    return Geom::Scale(
        get_gap(root->width(),  link_pattern->width())  * 100.0,
        get_gap(root->height(), link_pattern->height()) * 100.0
    );
}

std::string sp_get_pattern_label(SPPattern* pattern) {
    if (!pattern) return std::string();

    Inkscape::XML::Node* repr = pattern->getRepr();
    if (auto label = pattern->getAttribute("inkscape:label")) {
        if (*label) {
            return std::string(gettext(label));
        }
    }
    const char* stock_id = _(repr->attribute("inkscape:stockid"));
    const char* pat_id = stock_id ? stock_id : _(repr->attribute("id"));
    return std::string(pat_id ? pat_id : "");
}

void sp_item_set_pattern_style(SPItem* item, SPPattern* root_pattern, SPCSSAttr* css, FillOrStroke kind) {
    if (!item || !item->style || !item->getRepr()) {
        g_warning("No valid item provided to sp_item_set_pattern");
        return;
    }

    // auto selrepr = item->getRepr();
    // if (kind == STROKE && !selrepr) {
        // return;
    // }
    // SPObject *selobj = item;

    SPStyle* style = item->style;
    auto server = kind == FILL ? style->getFillPaintServer() : style->getStrokePaintServer();
    // if (style && (kind == FILL ? style->fill.isPaintserver() : style->stroke.isPaintserver())) {
        // SPPaintServer *server = (kind == FILL) ? item->style->getFillPaintServer()
                                               // : item->style->getStrokePaintServer();
    if (auto pattern = cast<SPPattern>(server); pattern->rootPattern() == root_pattern) {
        // only if this object's pattern is not rooted in our selected pattern, apply
        return;
    }

    if (kind == FILL) {
        sp_desktop_apply_css_recursive(item, css, true);
    }
    else {
        sp_repr_css_change_recursive(item->getRepr(), css, "style");
    }

    // create link to pattern right away, without waiting for object to be moved;
    // otherwise pattern editor may end up modifying pattern shared by different objects
    item->adjust_pattern(Geom::Affine());
}

// set pattern as item's fill or stroke; modify pattern's attributes
void sp_item_apply_pattern(SPItem* item, SPPattern* pattern, FillOrStroke kind, std::optional<Color> color, const Glib::ustring& label,
    const Geom::Affine& transform, const Geom::Point& offset, bool uniform_scale, const Geom::Scale& gap) {

    if (!pattern || !item) return;

    auto link_pattern = pattern;
    auto root_pattern = pattern->rootPattern();
    if (color) { //auto color = _psel->get_pattern_color()) {
        sp_pattern_set_color(root_pattern, color.value());
    }
    // pattern name is applied to the root
    root_pattern->setAttribute("inkscape:label", label.c_str()); // _psel->get_pattern_label().c_str());
    // remaining settings apply to link pattern
    if (link_pattern != root_pattern) {
        // auto transform = _psel->get_pattern_transform();
        sp_pattern_set_transform(link_pattern, transform);
        // auto offset = _psel->get_pattern_offset();
        sp_pattern_set_offset(link_pattern, offset);
        // auto uniform = _psel->is_pattern_scale_uniform();
        sp_pattern_set_uniform_scale(link_pattern, uniform_scale);
        // gap requires both patterns, but they are only created later by calling "adjust_pattern" below
        // it is OK to ignore it for now, during initial creation gap is 0,0
        // auto gap = _psel->get_pattern_gap();
        sp_pattern_set_gap(link_pattern, gap);
    }

    auto url = Glib::ustring::compose("url(#%1)", root_pattern->getRepr()->attribute("id"));
    // XML::Node* patrepr = root_pattern->getRepr();
    SPCSSAttr* css = sp_repr_css_attr_new();
    // gchar *urltext = g_strdup_printf("url(#%s)", patrepr->attribute("id"));
    sp_repr_css_set_property(css, kind == FILL ? "fill" : "stroke", url.c_str());

    sp_item_set_pattern_style(item, root_pattern, css, kind);

    // create link to the pattern right away, without waiting for this item to be moved;
    // otherwise pattern editor may end up modifying pattern shared by different objects
    item->adjust_pattern(Geom::Affine());
}
