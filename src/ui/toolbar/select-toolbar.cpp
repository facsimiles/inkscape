// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Select toolbar
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *   Vaibhav Malik <vaibhavmalik2018@gmail.com>
 *
 * Copyright (C) 2003-2005 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "select-toolbar.h"

#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/enums.h>
#include <gtkmm/image.h>
#include <gtkmm/togglebutton.h>
#include <2geom/rect.h>

#include "desktop.h"
#include "document-undo.h"
#include "document.h"
#include "message-stack.h"
#include "object/sp-item-transform.h"
#include "object/sp-namedview.h"
#include "page-manager.h"
#include "preferences.h"
#include "selection.h"
#include "ui/builder-utils.h"
#include "ui/icon-names.h"
#include "ui/util.h"
#include "ui/widget/combo-tool-item.h"
#include "ui/widget/spinbutton.h"
#include "ui/widget/unit-tracker.h"
#include "widgets/widget-sizes.h"

using Inkscape::UI::Widget::UnitTracker;
using Inkscape::Util::Unit;
using Inkscape::Util::Quantity;
using Inkscape::DocumentUndo;

namespace Inkscape::UI::Toolbar {

SelectToolbar::SelectToolbar()
    : SelectToolbar{create_builder("toolbar-select.ui")}
{}

SelectToolbar::SelectToolbar(Glib::RefPtr<Gtk::Builder> const &builder)
    : Toolbar{get_widget<Gtk::Box>(builder, "select-toolbar")}
    , _tracker{std::make_unique<UnitTracker>(Util::UNIT_TYPE_LINEAR)}
    , _action_prefix{"selector:toolbar:"}
    , _select_touch_btn{get_widget<Gtk::ToggleButton>(builder, "_select_touch_btn")}
    , _transform_stroke_btn{get_widget<Gtk::ToggleButton>(builder, "_transform_stroke_btn")}
    , _transform_corners_btn{get_widget<Gtk::ToggleButton>(builder, "_transform_corners_btn")}
    , _transform_gradient_btn{get_widget<Gtk::ToggleButton>(builder, "_transform_gradient_btn")}
    , _transform_pattern_btn{get_widget<Gtk::ToggleButton>(builder, "_transform_pattern_btn")}
{
    auto prefs = Preferences::get();

    _select_touch_btn.set_active(prefs->getBool("/tools/select/touch_box", false));
    _select_touch_btn.signal_toggled().connect(sigc::mem_fun(*this, &SelectToolbar::toggle_touch));

    _tracker->addUnit(Util::UnitTable::get().getUnit("%"));

    // Use StyleContext to check if the child is a context item (an item that is disabled if there is no selection).
    auto children = UI::get_children(_toolbar);
    for (auto const child : children) {
        if (child->has_css_class("context_item")) {
            _context_items.push_back(child);
        }
    }

    _transform_stroke_btn.set_active(prefs->getBool("/options/transform/stroke", true));
    _transform_stroke_btn.signal_toggled().connect(sigc::mem_fun(*this, &SelectToolbar::toggle_stroke));

    _transform_corners_btn.set_active(prefs->getBool("/options/transform/rectcorners", true));
    _transform_corners_btn.signal_toggled().connect(sigc::mem_fun(*this, &SelectToolbar::toggle_corners));

    _transform_gradient_btn.set_active(prefs->getBool("/options/transform/gradient", true));
    _transform_gradient_btn.signal_toggled().connect(sigc::mem_fun(*this, &SelectToolbar::toggle_gradient));

    _transform_pattern_btn.set_active(prefs->getBool("/options/transform/pattern", true));
    _transform_pattern_btn.signal_toggled().connect(sigc::mem_fun(*this, &SelectToolbar::toggle_pattern));

    _initMenuBtns();
}

SelectToolbar::~SelectToolbar() = default;

void SelectToolbar::setDesktop(SPDesktop *desktop)
{
    if (_desktop) {
        _selection_changed_conn.disconnect();
        _selection_modified_conn.disconnect();
    }

    Toolbar::setDesktop(desktop);

    if (_desktop) {
        auto sel = _desktop->getSelection();

        // Force update when selection changes.
        _selection_changed_conn = sel->connectChanged(sigc::mem_fun(*this, &SelectToolbar::_selectionChanged));
        _selection_modified_conn = sel->connectModified(sigc::mem_fun(*this, &SelectToolbar::_selectionModified));
    }
}

void SelectToolbar::_sensitize()
{
    auto const selection = _desktop->getSelection();
    bool const sensitive = selection && !selection->isEmpty();
    for (auto item : _context_items) {
        item->set_sensitive(sensitive);
    }
}

void SelectToolbar::layout_widget_update(Selection *sel) {}

void SelectToolbar::_selectionChanged(Selection *selection)
{
    assert(_desktop->getSelection() == selection);
    layout_widget_update(selection);
    _sensitize();
}

void SelectToolbar::_selectionModified(Selection *selection, unsigned flags)
{
    assert(_desktop->getSelection() == selection);
    if (flags & (SP_OBJECT_MODIFIED_FLAG        |
                 SP_OBJECT_PARENT_MODIFIED_FLAG |
                 SP_OBJECT_CHILD_MODIFIED_FLAG  ))
    {
        layout_widget_update(selection);
    }
}


void SelectToolbar::toggle_touch()
{
    Preferences::get()->setBool("/tools/select/touch_box", _select_touch_btn.get_active());
}

void SelectToolbar::toggle_stroke()
{
    bool active = _transform_stroke_btn.get_active();
    Preferences::get()->setBool("/options/transform/stroke", active);
    if (active) {
        _desktop->messageStack()->flash(INFORMATION_MESSAGE, _("Now <b>stroke width</b> is <b>scaled</b> when objects are scaled."));
    } else {
        _desktop->messageStack()->flash(INFORMATION_MESSAGE, _("Now <b>stroke width</b> is <b>not scaled</b> when objects are scaled."));
    }
}

void SelectToolbar::toggle_corners()
{
    bool active = _transform_corners_btn.get_active();
    Preferences::get()->setBool("/options/transform/rectcorners", active);
    if (active) {
        _desktop->messageStack()->flash(INFORMATION_MESSAGE, _("Now <b>rounded rectangle corners</b> are <b>scaled</b> when rectangles are scaled."));
    } else {
        _desktop->messageStack()->flash(INFORMATION_MESSAGE, _("Now <b>rounded rectangle corners</b> are <b>not scaled</b> when rectangles are scaled."));
    }
}

void SelectToolbar::toggle_gradient()
{
    bool active = _transform_gradient_btn.get_active();
    Preferences::get()->setBool("/options/transform/gradient", active);
    if (active) {
        _desktop->messageStack()->flash(INFORMATION_MESSAGE, _("Now <b>gradients</b> are <b>transformed</b> along with their objects when those are transformed (moved, scaled, rotated, or skewed)."));
    } else {
        _desktop->messageStack()->flash(INFORMATION_MESSAGE, _("Now <b>gradients</b> remain <b>fixed</b> when objects are transformed (moved, scaled, rotated, or skewed)."));
    }
}

void SelectToolbar::toggle_pattern()
{
    bool active = _transform_pattern_btn.get_active();
    Preferences::get()->setInt("/options/transform/pattern", active);
    if (active) {
        _desktop->messageStack()->flash(INFORMATION_MESSAGE, _("Now <b>patterns</b> are <b>transformed</b> along with their objects when those are transformed (moved, scaled, rotated, or skewed)."));
    } else {
        _desktop->messageStack()->flash(INFORMATION_MESSAGE, _("Now <b>patterns</b> remain <b>fixed</b> when objects are transformed (moved, scaled, rotated, or skewed)."));
    }
}

} // namespace Inkscape::UI::Toolbar

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
