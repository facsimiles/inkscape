// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Ted Gould <ted@gould.cx>
 *
 * Copyright (C) 2008 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "filter.h"

#include "desktop.h"
#include "document.h"
#include "selection.h"

#include "extension/extension.h"
#include "extension/effect.h"
#include "extension/system.h"
#include "object/sp-defs.h"
#include "xml/repr.h"
#include "xml/simple-node.h"
#include "xml/attribute-record.h"


namespace Inkscape {
namespace Extension {
namespace Internal {
namespace Filter {

Filter::Filter() :
    Inkscape::Extension::Implementation::Implementation(),
    _filter(nullptr) {
    return;
}

Filter::Filter(gchar const * filter) :
    Inkscape::Extension::Implementation::Implementation(),
    _filter(filter) {
    return;
}

Filter::~Filter () {
    if (_filter != nullptr) {
        _filter = nullptr;
    }

    return;
}

bool Filter::load(Inkscape::Extension::Extension * /*module*/)
{
    return true;
}

Inkscape::Extension::Implementation::ImplementationDocumentCache *
Filter::newDocCache(Inkscape::Extension::Extension * /*ext*/, SPDesktop * /*desktop*/)
{
    return nullptr;
}

gchar const *Filter::get_filter_text(Inkscape::Extension::Extension * /*ext*/)
{
    return _filter;
}

Inkscape::XML::Document *
Filter::get_filter (Inkscape::Extension::Extension * ext) {
    gchar const * filter = get_filter_text(ext);
    return sp_repr_read_mem(filter, strlen(filter), nullptr);
}

void merge_filters( Inkscape::XML::Node * to, Inkscape::XML::Node * from,
		       Inkscape::XML::Document * doc,
		       gchar const * srcGraphic = nullptr, gchar const * srcGraphicAlpha = nullptr)
{
    if (from == nullptr) return;

    // copy attributes
    for ( const auto & iter : from->attributeList()) {
        gchar const * attr = g_quark_to_string(iter.key);

        if (!strcmp(attr, "id")) continue; // nope, don't copy that one!
        to->setAttribute(attr, from->attribute(attr));

        if (!strcmp(attr, "in") || !strcmp(attr, "in2") || !strcmp(attr, "in3")) {
            if (srcGraphic != nullptr && !strcmp(from->attribute(attr), "SourceGraphic")) {
                to->setAttribute(attr, srcGraphic);
            }

            if (srcGraphicAlpha != nullptr && !strcmp(from->attribute(attr), "SourceAlpha")) {
                to->setAttribute(attr, srcGraphicAlpha);
            }
        }
    }

    // for each child call recursively
    for (Inkscape::XML::Node * from_child = from->firstChild();
         from_child != nullptr ; from_child = from_child->next()) {
        Glib::ustring name = "svg:";
        name += from_child->name();

        Inkscape::XML::Node * to_child = doc->createElement(name.c_str());
        to->appendChild(to_child);
        merge_filters(to_child, from_child, doc, srcGraphic, srcGraphicAlpha);

        if (from_child == from->firstChild() && !strcmp("filter", from->name()) && srcGraphic != nullptr && to_child->attribute("in") == nullptr) {
            to_child->setAttribute("in", srcGraphic);
        }
        Inkscape::GC::release(to_child);
    }
}

void create_and_apply_filter(SPItem* item, Inkscape::XML::Document* filterdoc) {
    if (!item) return;

    Inkscape::XML::Node* node = item->getRepr();
    auto document = item->document;
    Inkscape::XML::Document * xmldoc = document->getReprDoc();
    Inkscape::XML::Node * defsrepr = document->getDefs()->getRepr();
    Inkscape::XML::Node * newfilterroot = xmldoc->createElement("svg:filter");
    merge_filters(newfilterroot, filterdoc->root(), xmldoc);
    defsrepr->appendChild(newfilterroot);
    document->resources_changed_signals[g_quark_from_string("filter")].emit();

    Glib::ustring url = "url(#"; url += newfilterroot->attribute("id"); url += ")";
    Inkscape::GC::release(newfilterroot);

    SPCSSAttr* css = sp_repr_css_attr(node, "style");
    sp_repr_css_set_property(css, "filter", url.c_str());
    sp_repr_css_set(node, css, "style");
}

#define FILTER_SRC_GRAPHIC       "fbSourceGraphic"
#define FILTER_SRC_GRAPHIC_ALPHA "fbSourceGraphicAlpha"

void Filter::effect(Inkscape::Extension::Effect *module, ExecutionEnv * /*executionEnv*/, SPDesktop *desktop,
                    Inkscape::Extension::Implementation::ImplementationDocumentCache * /*docCache*/)
{
    Inkscape::XML::Document *filterdoc = get_filter(module);
    if (filterdoc == nullptr) {
        return; // could not parse the XML source of the filter; typically parser will stderr a warning
    }

    Inkscape::Selection * selection = desktop->getSelection();

    // TODO need to properly refcount the items, at least
    std::vector<SPItem*> items(selection->items().begin(), selection->items().end());

    Inkscape::XML::Document * xmldoc = desktop->doc()->getReprDoc();
    Inkscape::XML::Node * defsrepr = desktop->doc()->getDefs()->getRepr();

    for(auto spitem : items) {
        Inkscape::XML::Node *node = spitem->getRepr();

        SPCSSAttr * css = sp_repr_css_attr(node, "style");
        gchar const * filter = sp_repr_css_property(css, "filter", nullptr);

        if (filter == nullptr) {
            create_and_apply_filter(spitem, filterdoc);
        } else {
            if (strncmp(filter, "url(#", strlen("url(#")) || filter[strlen(filter) - 1] != ')') {
                // This is not url(#id) -- we can't handle it
                continue;
            }

            gchar * lfilter = g_strndup(filter + 5, strlen(filter) - 6);
            Inkscape::XML::Node * filternode = nullptr;
            for (Inkscape::XML::Node * child = defsrepr->firstChild(); child != nullptr; child = child->next()) {
                const char * child_id = child->attribute("id");
                if (child_id != nullptr && !strcmp(lfilter, child_id)) {
                    filternode = child;
                    break;
                }
            }
            g_free(lfilter);

            // no filter
            if (filternode == nullptr) {
                g_warning("no assigned filter found!");
                continue;
            }

            if (filternode->lastChild() == nullptr) {
                // empty filter, we insert
                merge_filters(filternode, filterdoc->root(), xmldoc);
            } else {
                // existing filter, we merge
                filternode->lastChild()->setAttribute("result", FILTER_SRC_GRAPHIC);
                Inkscape::XML::Node * alpha = xmldoc->createElement("svg:feColorMatrix");
                alpha->setAttribute("result", FILTER_SRC_GRAPHIC_ALPHA);
                alpha->setAttribute("in", FILTER_SRC_GRAPHIC); // not required, but we're being explicit
                alpha->setAttribute("values", "0 0 0 -1 0 0 0 0 -1 0 0 0 0 -1 0 0 0 0 1 0");

                filternode->appendChild(alpha);

                merge_filters(filternode, filterdoc->root(), xmldoc, FILTER_SRC_GRAPHIC, FILTER_SRC_GRAPHIC_ALPHA);

                Inkscape::GC::release(alpha);
            }
        }
    }

    return;
}

#include "extension/internal/clear-n_.h"

void
Filter::filter_init (gchar const * id, gchar const * name, gchar const * submenu, gchar const * tip, gchar const * filter)
{
    // clang-format off
    gchar * xml_str = g_strdup_printf(
        "<inkscape-extension xmlns=\"" INKSCAPE_EXTENSION_URI "\">\n"
        "<name>%s</name>\n"
        "<id>org.inkscape.effect.filter.%s</id>\n"
        "<effect>\n"
        "<object-type>all</object-type>\n"
        "<effects-menu>\n"
        "<submenu name=\"" N_("Filters") "\" />\n"
        "<submenu name=\"%s\"/>\n"
        "</effects-menu>\n"
        "<menu-tip>%s</menu-tip>\n"
        "</effect>\n"
        "</inkscape-extension>\n", name, id, submenu, tip);
    // clang-format on
    Inkscape::Extension::build_from_mem(xml_str, std::make_unique<Filter>(filter));
    g_free(xml_str);
    return;
}

bool Filter::apply_filter(Inkscape::Extension::Effect* module, SPItem* item) {
    if (!item) return false;

    Inkscape::XML::Document* filterdoc = get_filter(module);
    if (!filterdoc) return false;

    create_and_apply_filter(item, filterdoc);
    return true;
}

}; /* namespace Filter */
}; /* namespace Internal */
}; /* namespace Extension */
}; /* namespace Inkscape */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
