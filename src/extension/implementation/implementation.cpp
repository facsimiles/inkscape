// SPDX-License-Identifier: GPL-2.0-or-later
/*
    Author:  Ted Gould <ted@gould.cx>
    Copyright (c) 2003-2005,2007

    Released under GNU GPL v2+, read the file 'COPYING' for more information.

    This file is the backend to the extensions system.  These are
    the parts of the system that most users will never see, but are
    important for implementing the extensions themselves.  This file
    contains the base class for all of that.
*/

#include "implementation.h"

#include <extension/effect.h>
#include <extension/input.h>
#include <extension/output.h>

#include "desktop.h"
#include "selection.h"
#include "object/sp-namedview.h"
#include "xml/attribute-record.h"
#include "xml/node.h"


namespace Inkscape {
namespace Extension {
namespace Implementation {

Gtk::Widget *Implementation::prefs_input(Inkscape::Extension::Input *module, gchar const * /*filename*/)
{
    return module->autogui(nullptr, nullptr);
}

Gtk::Widget *Implementation::prefs_output(Inkscape::Extension::Output *module)
{
    return module->autogui(nullptr, nullptr);
}

Gtk::Widget *Implementation::prefs_effect(Inkscape::Extension::Effect *module, sigc::signal<void> *changeSignal,
                                          std::shared_ptr<ImplementationDocumentCache> docCache)
{
    if (module->widget_visible_count() == 0) {
        return nullptr;
    }

    auto view = docCache->view();
    auto current_document = view->doc();

    auto selected = ((SPDesktop *)view)->getSelection()->items();
    Inkscape::XML::Node const *first_select = nullptr;
    if (!selected.empty()) {
        const SPItem *item = selected.front();
        first_select = item->getRepr();
    }

    // TODO deal with this broken const correctness:
    return module->autogui(current_document, const_cast<Inkscape::XML::Node *>(first_select), changeSignal);
} // Implementation::prefs_effect

/**
    \brief  A function to replace all the elements in an old document
            by those from a new document.
            document and repinserts them into an emptied old document.
    \param  oldroot  The root node of the old (destination) document.
    \param  newroot  The root node of the new (source) document.

    This function first deletes all the root attributes in the old document followed
    by copying all the root attributes from the new document to the old document.

    It then deletes all the elements in the old document by
    making two passes, the first to create a list of the old elements and
    the second to actually delete them. This two pass approach removes issues
    with the list being changed while parsing through it... lots of nasty bugs.

    Then, it copies all the element in the new document into the old document.

    Finally, it copies the attributes in namedview.
*/
void copy_doc(Inkscape::XML::Node *oldroot, Inkscape::XML::Node *newroot)
{
    if ((oldroot == nullptr) || (newroot == nullptr)) {
        g_warning("Error on copy_doc: NULL pointer input.");
        return;
    }

    // For copying attributes in root and in namedview
    using Inkscape::Util::List;
    using Inkscape::XML::AttributeRecord;
    std::vector<gchar const *> attribs;

    // Must explicitly copy root attributes. This must be done first since
    // copying grid lines calls "SPGuide::set()" which needs to know the
    // width, height, and viewBox of the root element.

    // Make a list of all attributes of the old root node.
    for (List<AttributeRecord const> iter = oldroot->attributeList(); iter; ++iter) {
        attribs.push_back(g_quark_to_string(iter->key));
    }

    // Delete the attributes of the old root node.
    for (std::vector<gchar const *>::const_iterator it = attribs.begin(); it != attribs.end(); ++it) {
        oldroot->removeAttribute(*it);
    }

    // Set the new attributes.
    for (List<AttributeRecord const> iter = newroot->attributeList(); iter; ++iter) {
        gchar const *name = g_quark_to_string(iter->key);
        oldroot->setAttribute(name, newroot->attribute(name));
    }

    // Question: Why is the "sodipodi:namedview" special? Treating it as a normal
    // element results in crashes.
    // Seems to be a bug:
    // http://inkscape.13.x6.nabble.com/Effect-that-modifies-the-document-properties-tt2822126.html

    std::vector<Inkscape::XML::Node *> delete_list;

    // Make list
    for (Inkscape::XML::Node *child = oldroot->firstChild(); child != nullptr; child = child->next()) {
        if (!strcmp("sodipodi:namedview", child->name())) {
            for (Inkscape::XML::Node *oldroot_namedview_child = child->firstChild(); oldroot_namedview_child != nullptr;
                 oldroot_namedview_child = oldroot_namedview_child->next()) {
                delete_list.push_back(oldroot_namedview_child);
            }
            break;
        }
    }

    // Unparent (delete)
    for (auto &i : delete_list) {
        sp_repr_unparent(i);
    }
    attribs.clear();
    oldroot->mergeFrom(newroot, "id", true, true);
}

void Implementation::replace_document(Inkscape::UI::View::View *view, SPDocument *mydoc)
{
    SPDocument* vd=view->doc();

    mydoc->changeUriAndHrefs(vd->getDocumentURI());

    vd->emitReconstructionStart();
    copy_doc(vd->getReprRoot(), mydoc->getReprRoot());
    vd->emitReconstructionFinish();

    // Getting the named view from the document generated by the extension
    SPNamedView *nv = sp_document_namedview(mydoc, nullptr);

    // Check if it has a default layer set up
    auto desktop = reinterpret_cast<SPDesktop *>(view);
    SPObject *layer = nullptr;
    if (nv != nullptr) {
        if (nv->default_layer_id != 0) {
            SPDocument *document = desktop->doc();
            // If so, get that layer
            if (document != nullptr) {
                layer = document->getObjectById(g_quark_to_string(nv->default_layer_id));
            }
        }
        desktop->showGrids(nv->grids_visible);
    }

    sp_namedview_update_layers_from_document(desktop);
    // If that layer exists,
    if (layer) {
        // set the current layer
        desktop->setCurrentLayer(layer);
    }
}


} /* namespace Implementation */
} /* namespace Extension */
} /* namespace Inkscape */

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
