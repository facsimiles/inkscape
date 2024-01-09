// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 *  Actions for Editing an object
 *
 * Authors:
 *   Sushant A A <sushant.co19@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "actions-edit.h"

#include <giomm.h>
#include <glibmm/i18n.h>

#include "actions-helper.h"
#include "desktop.h"
#include "inkscape-application.h"
#include "document-undo.h"
#include "selection.h"
#include "selection-chemistry.h"

#include "object/sp-guide.h"
#include "ui/icon-names.h"
#include "ui/tools/node-tool.h"
#include "ui/tools/text-tool.h"

namespace ActionsEdit {

void
object_to_pattern(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Objects to Pattern
    selection->tile();
}

void
pattern_to_object(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Pattern to Objects
    selection->untile();
}

void
object_to_marker(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Objects to Marker
    selection->toMarker();
}

void
object_to_guides(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Objects to Guides
    selection->toGuides();
}

void
cut(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    // Cut
    selection->cut();
}

void
copy(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Copy
    selection->copy();
}

void
paste_style(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Paste Style
    selection->pasteStyle();
}

void
paste_size(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Paste Size
    selection->pasteSize(true,true);
}

void
paste_width(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Paste Width
    selection->pasteSize(true, false);
}

void
paste_height(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Paste Height
    selection->pasteSize(false, true);
}

void
paste_size_separately(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Paste Size Separately
    selection->pasteSizeSeparately(true, true);
}

void
paste_width_separately(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Paste Width Separately
    selection->pasteSizeSeparately(true, false);
}

void
paste_height_separately(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Paste Height Separately
    selection->pasteSizeSeparately(false, true);
}

void
duplicate(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Duplicate
    selection->duplicate();
}

void duplicate_transform(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    selection->duplicate(true);
    selection->reapplyAffine();
    Inkscape::DocumentUndo::done(document, _("Duplicate and Transform"),
                                 INKSCAPE_ICON("edit-duplicate"));
}

void
clone(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Create Clone
    selection->clone();
}

void
clone_unlink(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Unlink Clone
    selection->unlink();
}

void
clone_unlink_recursively(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Unlink Clones recursively
    selection->unlinkRecursive(false, true);
}

void
clone_link(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Relink to Copied
    selection->relink();
}

void
select_original(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Select Original
    selection->cloneOriginal();
}

void
clone_link_lpe(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Clone original path (LPE)
    selection->cloneOriginalPathLPE();
}

void
edit_delete(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    // For text and node too special handling.
    if (auto desktop = selection->desktop()) {
        if (auto const text_tool = dynamic_cast<Inkscape::UI::Tools::TextTool *>(desktop->getTool())) {
            text_tool->deleteSelected();
            return;
        }
        if (auto const node_tool = dynamic_cast<Inkscape::UI::Tools::NodeTool *>(desktop->getTool())) {
            // This means we delete items is no nodes are selected.
            if (node_tool->_selected_nodes) {
                node_tool->deleteSelected();
                return;
            }
        }
    }

    //  Delete select objects only.
    selection->deleteItems();
}

void
edit_delete_selection(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }
    selection->deleteItems();
}

void
paste_path_effect(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Paste Path Effect
    selection->pastePathEffect();
}

void
remove_path_effect(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    //  Remove Path Effect
    selection->removeLPE();
}

void
swap_fill_and_stroke(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    // Swap fill and Stroke
    selection->swapFillStroke();
}

void
fit_canvas_to_selection(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    // Fit Page to Selection
    selection->fitCanvas(true);
}

std::vector<std::vector<Glib::ustring>> raw_data_edit = {
    // clang-format off
    {"app.object-to-pattern",                   N_("Objects to Pattern"),               "Edit",     N_("Convert selection to a rectangle with tiled pattern fill")},
    {"app.pattern-to-object",                   N_("Pattern to Objects"),               "Edit",     N_("Extract objects from a tiled pattern fill")},
    {"app.object-to-marker",                    N_("Objects to Marker"),                "Edit",     N_("Convert selection to a line marker")},
    {"app.object-to-guides",                    N_("Objects to Guides"),                "Edit",     N_("Convert selected objects to a collection of guidelines aligned with their edges")},
    {"app.cut",                                 N_("Cut"),                              "Edit",     N_("Cut selection to clipboard")},
    {"app.copy",                                N_("Copy"),                             "Edit",     N_("Copy selection to clipboard")},
    {"app.paste-style",                         N_("Paste Style"),                      "Edit",     N_("Apply the style of the copied object to selection")},
    {"app.paste-size",                          N_("Paste Size"),                       "Edit",     N_("Scale selection to match the size of the copied object")},
    {"app.paste-width",                         N_("Paste Width"),                      "Edit",     N_("Scale selection horizontally to match the width of the copied object")},
    {"app.paste-height",                        N_("Paste Height"),                     "Edit",     N_("Scale selection vertically to match the height of the copied object")},
    {"app.paste-size-separately",               N_("Paste Size Separately"),            "Edit",     N_("Scale each selected object to match the size of the copied object")},
    {"app.paste-width-separately",              N_("Paste Width Separately"),           "Edit",     N_("Scale each selected object horizontally to match the width of the copied object")},
    {"app.paste-height-separately",             N_("Paste Height Separately"),          "Edit",     N_("Scale each selected object vertically to match the height of the copied object")},
    {"app.duplicate",                           N_("Duplicate"),                        "Edit",     N_("Duplicate Selected Objects")},
    {"app.duplicate-transform",                 N_("Duplicate and Transform"),          "Edit",     N_("Duplicate selected objects and reapply last transformation")},
    {"app.clone",                               N_("Create Clone"),                     "Edit",     N_("Create a clone (a copy linked to the original) of selected object")},
    {"app.clone-unlink",                        N_("Unlink Clone"),                     "Edit",     N_("Cut the selected clones' links to the originals, turning them into standalone objects")},
    {"app.clone-unlink-recursively",            N_("Unlink Clones recursively"),        "Edit",     N_("Unlink all clones in the selection, even if they are in groups.")},
    {"app.clone-link",                          N_("Relink to Copied"),                 "Edit",     N_("Relink the selected clones to the object currently on the clipboard")},
    {"app.select-original",                     N_("Select Original"),                  "Edit",     N_("Select the object to which the selected clone is linked")},
    {"app.clone-link-lpe",                      N_("Clone original path (LPE)"),        "Edit",     N_("Creates a new path, applies the Clone original LPE, and refers it to the selected path")},
    {"app.delete",                              N_("Delete"),                           "Edit",     N_("Delete selected items, nodes or text.")},
    {"app.delete-selection",                    N_("Delete Items"),                     "Edit",     N_("Delete selected items")},
    {"app.paste-path-effect",                   N_("Paste Path Effect"),                "Edit",     N_("Apply the path effect of the copied object to selection")},
    {"app.remove-path-effect",                  N_("Remove Path Effect"),               "Edit",     N_("Remove any path effects from selected objects")},
    {"app.swap-fill-and-stroke",                N_("Swap fill and stroke"),             "Edit",     N_("Swap fill and stroke of an object")},
    {"app.fit-canvas-to-selection",             N_("Fit Page to Selection"),            "Edit",     N_("Fit the page to the current selection")}
    // clang-format on
};

} // namespace ActionsEdit

using namespace ActionsEdit;

void
add_actions_edit(InkscapeApplication* app)
{
    auto *gapp = app->gio_app();

    // clang-format off
    gapp->add_action( "object-to-pattern",               sigc::bind(sigc::ptr_fun(&object_to_pattern), app));
    gapp->add_action( "pattern-to-object",               sigc::bind(sigc::ptr_fun(&pattern_to_object), app));
    gapp->add_action( "object-to-marker",                sigc::bind(sigc::ptr_fun(&object_to_marker), app));
    gapp->add_action( "object-to-guides",                sigc::bind(sigc::ptr_fun(&object_to_guides), app));
    gapp->add_action( "cut",                             sigc::bind(sigc::ptr_fun(&cut), app));
    gapp->add_action( "copy",                            sigc::bind(sigc::ptr_fun(&copy), app));
    gapp->add_action( "paste-style",                     sigc::bind(sigc::ptr_fun(&paste_style), app));
    gapp->add_action( "paste-size",                      sigc::bind(sigc::ptr_fun(&paste_size), app));
    gapp->add_action( "paste-width",                     sigc::bind(sigc::ptr_fun(&paste_width), app));
    gapp->add_action( "paste-height",                    sigc::bind(sigc::ptr_fun(&paste_height), app));
    gapp->add_action( "paste-size-separately",           sigc::bind(sigc::ptr_fun(&paste_size_separately), app));
    gapp->add_action( "paste-width-separately",          sigc::bind(sigc::ptr_fun(&paste_width_separately), app));
    gapp->add_action( "paste-height-separately",         sigc::bind(sigc::ptr_fun(&paste_height_separately), app));
    gapp->add_action( "duplicate",                       sigc::bind(sigc::ptr_fun(&duplicate), app));
    gapp->add_action( "duplicate-transform",             sigc::bind(sigc::ptr_fun(&duplicate_transform), app));
    // explicit namespace reference added because NetBSD provides a conflicting clone() function in its libc headers
    gapp->add_action( "clone",                           sigc::bind(sigc::ptr_fun(&ActionsEdit::clone), app));
    gapp->add_action( "clone-unlink",                    sigc::bind(sigc::ptr_fun(&clone_unlink), app));
    gapp->add_action( "clone-unlink-recursively",        sigc::bind(sigc::ptr_fun(&clone_unlink_recursively), app));
    gapp->add_action( "clone-link",                      sigc::bind(sigc::ptr_fun(&clone_link), app));
    gapp->add_action( "select-original",                 sigc::bind(sigc::ptr_fun(&select_original), app));
    gapp->add_action( "clone-link-lpe",                  sigc::bind(sigc::ptr_fun(&clone_link_lpe), app));
    gapp->add_action( "delete",                          sigc::bind(sigc::ptr_fun(&edit_delete), app));
    gapp->add_action( "delete-selection",                sigc::bind(sigc::ptr_fun(&edit_delete_selection), app));
    gapp->add_action( "paste-path-effect",               sigc::bind(sigc::ptr_fun(&paste_path_effect), app));
    gapp->add_action( "remove-path-effect",              sigc::bind(sigc::ptr_fun(&remove_path_effect), app));
    gapp->add_action( "swap-fill-and-stroke",            sigc::bind(sigc::ptr_fun(&swap_fill_and_stroke), app));
    gapp->add_action( "fit-canvas-to-selection",         sigc::bind(sigc::ptr_fun(&fit_canvas_to_selection), app));
    // clang-format on

    if (!app) {
        show_output("add_actions_edit: no app!");
        return;
    }
    app->get_action_extra_data().add_data(raw_data_edit);
}
