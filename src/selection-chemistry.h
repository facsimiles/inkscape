// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SELECTION_CHEMISTRY_H
#define SEEN_SELECTION_CHEMISTRY_H

/*
 * Miscellaneous operations on selected items
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Frank Felfe <innerspace@iname.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 1999-2012 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <vector>
#include <2geom/forward.h>
#include <glibmm/ustring.h>

class SPCSSAttr;
class SPDesktop;
class SPDocument;
class SPItem;
class SPObject;
class SPGroup;

namespace Inkscape {

class Selection;
class ObjectSet;
namespace LivePathEffect { class PathParam; }

namespace SelectionHelper {

void selectAll(SPDesktop *desktop);
void selectAll(SPDocument *document, SPGroup *layer);
void selectAllInAll(SPDesktop *desktop);
void selectAllInAll(SPDocument *document, SPGroup *layer);
void selectNone(SPDesktop *desktop);
void selectNone(SPDocument *document);
void selectSameFillStroke(SPDesktop *desktop);
void selectSameFillStroke(SPDocument *document);
void selectSameFillColor(SPDesktop *desktop);
void selectSameFillColor(SPDocument *document);
void selectSameStrokeColor(SPDesktop *desktop);
void selectSameStrokeColor(SPDocument *document);
void selectSameStrokeStyle(SPDesktop *desktop);
void selectSameStrokeStyle(SPDocument *document);
void selectSameObjectType(SPDesktop *desktop);
void selectSameObjectType(SPDocument *document);
void invert(SPDesktop *desktop);
void invert(SPDocument *document, SPGroup *layer);
void invertAllInAll(SPDesktop *desktop);
void invertAllInAll(SPDocument *document, SPGroup *layer);
void reverse(SPDesktop *desktop);
void reverse(SPDocument *document);
void fixSelection(SPDesktop *desktop);
void fixSelection(SPDocument *document);

} // namespace SelectionHelper

} // namespace Inkscape

void sp_edit_select_all(SPDesktop *desktop);
void sp_edit_select_all_in_all_layers (SPDesktop *desktop);
void sp_edit_invert (SPDesktop *desktop);
void sp_edit_invert_in_all_layers (SPDesktop *desktop);

void sp_edit_select_all(SPDocument *document, SPGroup *layer);
void sp_edit_select_all_in_all_layers (SPDocument *document, SPGroup *layer);
void sp_edit_invert (SPDocument *document, SPGroup *layer);
void sp_edit_invert_in_all_layers (SPDocument *document, SPGroup *layer);



SPCSSAttr *take_style_from_item(SPObject *object);

void sp_selection_paste(SPDesktop *desktop, bool in_place, bool on_page = false);

void sp_set_style_clipboard(SPCSSAttr *css);


void sp_selection_item_next(SPDesktop *desktop);
void sp_selection_item_prev(SPDesktop *desktop);

void sp_selection_next_patheffect_param(SPDesktop *desktop);

void sp_selection_item_next(SPDocument *document);
void sp_selection_item_prev(SPDocument *document, Inkscape::Selection *selection_display_message);

enum SPSelectStrokeStyleType
{
    SP_FILL_COLOR = 0,
    SP_STROKE_COLOR = 1,
    SP_STROKE_STYLE_WIDTH = 2,
    SP_STROKE_STYLE_DASHES = 3,
    SP_STROKE_STYLE_MARKERS = 4,
    SP_STROKE_STYLE_ALL = 5,
    SP_STYLE_ALL = 6
};

void sp_select_same_fill_stroke_style(SPDesktop *desktop, gboolean fill, gboolean strok, gboolean style);
void sp_select_same_object_type(SPDesktop *desktop);

void sp_select_same_fill_stroke_style(SPDocument *document, gboolean fill, gboolean strok, gboolean style);
void sp_select_same_object_type(SPDocument *document);


std::vector<SPItem*> sp_get_same_style(SPItem *sel, std::vector<SPItem*> &src, SPSelectStrokeStyleType type = SP_STYLE_ALL);
std::vector<SPItem*> sp_get_same_object_type(SPItem *sel, std::vector<SPItem*> &src);

void scroll_to_show_item(SPDesktop *desktop, SPItem *item);

void sp_undo(SPDesktop *desktop);
void sp_redo(SPDesktop *desktop);

void sp_undo(SPDocument *document);
void sp_redo(SPDocument *document);


bool fit_canvas_to_drawing(SPDocument *, bool with_margins = false);
void unlock_all(SPDesktop *desktop);
void unlock_all_in_all_layers(SPDesktop *desktop);
void unhide_all(SPDesktop *desktop);
void unhide_all_in_all_layers(SPDesktop *desktop);

void unlock_all(SPDocument *document);
void unlock_all_in_all_layers(SPDocument *document);
void unhide_all(SPDocument *document);
void unhide_all_in_all_layers(SPDocument *documentr);


std::vector<SPItem*> get_all_items(SPObject *from, SPDesktop *desktop, bool onlyvisible, bool onlysensitive, bool ingroups, std::vector<SPItem*> const &exclude = {});
std::vector<SPItem*> get_all_items(SPObject *from, SPDocument *document, bool onlyvisible, bool onlysensitive, bool ingroups, std::vector<SPItem*> const &exclude = {});


/* selection cycling */
enum SPCycleType
{
    SP_CYCLE_SIMPLE,
    SP_CYCLE_VISIBLE, // cycle only visible items
    SP_CYCLE_FOCUS // readjust visible area to view selected item
};

// TODO FIXME: This should be moved into preference repr
extern SPCycleType SP_CYCLING;

#endif // SEEN_SELECTION_CHEMISTRY_H
