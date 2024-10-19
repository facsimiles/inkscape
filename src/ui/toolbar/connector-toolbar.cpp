// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Connector aux toolbar
 */
/* Authors:
 *   MenTaLguY <mental@rydia.net>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Frank Felfe <innerspace@iname.com>
 *   John Cliff <simarilius@yahoo.com>
 *   David Turner <novalis@gnu.org>
 *   Josh Andler <scislac@scislac.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Maximilian Albert <maximilian.albert@gmail.com>
 *   Tavmjong Bah <tavmjong@free.fr>
 *   Abhishek Sharma
 *   Kris De Gussem <Kris.DeGussem@gmail.com>
 *   Vaibhav Malik <vaibhavmalik2018@gmail.com>
 *
 * Copyright (C) 2004 David Turner
 * Copyright (C) 2003 MenTaLguY
 * Copyright (C) 1999-2011 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "connector-toolbar.h"

#include <glibmm/i18n.h>
#include <gtkmm/separator.h>

#include "desktop.h"
#include "document-undo.h"
#include "live_effects/lpe-connector-line.h"
#include "live_effects/lpe-connector-avoid.h"
#include "selection.h"
#include "ui/builder-utils.h"
#include "ui/tools/connector-tool.h"
#include "ui/widget/canvas.h"
#include "ui/widget/spinbutton.h"

using Inkscape::DocumentUndo;
using Inkscape::LivePathEffect::LPEConnectorAvoid;
using Inkscape::UI::Tools::ConnectorTool;

namespace Inkscape::UI::Toolbar {

ConnectorToolbar::ConnectorToolbar(SPDesktop *desktop)
    : Toolbar(desktop)
    , _builder(create_builder("toolbar-connector.ui"))
    , _line_tool(get_widget<Gtk::ToggleButton>(_builder, "line_tool"))
    , _point_tool(get_widget<Gtk::ToggleButton>(_builder, "point_tool"))
    , _avoid(get_widget<Gtk::ToggleButton>(_builder, "avoid"))
    , _orthogonal(get_widget<Gtk::ToggleButton>(_builder, "orthogonal"))
    , _jump_type(get_widget<Gtk::ToggleButton>(_builder, "jump_type"))
{
    auto prefs = Preferences::get();

    // Values auto-calculated.
    _curvature_item.set_custom_numeric_menu_data({});
    _spacing_item.set_custom_numeric_menu_data({});
    _length_item.set_custom_numeric_menu_data({});

    _toolbar = &get_widget<Gtk::Box>(_builder, "connector-toolbar");
    set_child(*_toolbar);

    _line_tool.signal_toggled().connect(sigc::mem_fun(*this, &ConnectorToolbar::tool_toggled));
    _point_tool.signal_toggled().connect(sigc::mem_fun(*this, &ConnectorToolbar::tool_toggled));
    _avoid.signal_toggled().connect(sigc::mem_fun(*this, &ConnectorToolbar::path_set_avoid));

    // Orthogonal connectors toggle button
    _orthogonal.set_active(prefs->getBool("/tools/connector/orthogonal"));
    _orthogonal.signal_toggled().connect(sigc::mem_fun(*this, &ConnectorToolbar::orthogonal_toggled));

    // Curvature spinbox
    auto &curvature_item = get_derived_widget<UI::Widget::SpinButton>(_builder, "curvature_item");
    curvature_item.set_defocus_widget(desktop->getCanvas());
    _curvature_adj = curvature_item.get_adjustment();
    _curvature_adj->set_value(prefs->getDouble("/tools/connector/curvature", 1.0));
    _curvature_adj->signal_value_changed().connect(sigc::mem_fun(*this, &ConnectorToolbar::curvature_changed));

    // Steps spinbox
    auto &steps_val = get_derived_widget<UI::Widget::SpinButton>(_builder, "steps_item");
    steps_val.set_defocus_widget(desktop->getCanvas());
    _steps_adj = steps_val.get_adjustment();
    _steps_adj->set_value(prefs->getDouble("/tools/connector/steps", 1.0));
    _steps_adj->signal_value_changed().connect(sigc::mem_fun(*this, &ConnectorToolbar::steps_changed));

    // Spacing spinbox
    auto &spacing_item = get_derived_widget<UI::Widget::SpinButton>(_builder, "spacing_item");
    spacing_item.set_defocus_widget(desktop->getCanvas());
    _spacing_adj = spacing_item.get_adjustment();
    _spacing_adj->set_value(prefs->getDouble("/tools/connector/spacing", 0.0));
    _spacing_adj->signal_value_changed().connect(sigc::mem_fun(*this, &ConnectorToolbar::spacing_changed));

    // Jump size spinbox
    auto &jump_item = get_derived_widget<UI::Widget::SpinButton>(_builder, "jump_item");
    jump_item.set_defocus_widget(desktop->getCanvas());
    _jump_size_adj = jump_item.get_adjustment();
    _jump_size_adj->set_value(prefs->getDouble("/tools/connector/jump-size", 4.0));
    _jump_size_adj->signal_value_changed().connect(sigc::mem_fun(*this, &ConnectorToolbar::jump_size_changed));

    // Jump type toggle
    _jump_type.set_active(prefs->getBool("/tools/connector/jump-type"));
    _jump_type.signal_toggled().connect(sigc::mem_fun(*this, &ConnectorToolbar::jump_type_toggled));

    // Code to watch for changes to the connector-spacing attribute in the XML.
    _repr = _desktop->getDocument()->getReprNamedView();
    assert(_repr);
    Inkscape::GC::anchor(_repr);
    _repr->addObserver(*this);
    _repr->synthesizeEvents(*this);
}

ConnectorToolbar::~ConnectorToolbar()
{
    _repr->removeObserver(*this);
    Inkscape::GC::release(_repr);
}

void ConnectorToolbar::tool_toggled()
{
    auto tool = dynamic_cast<ConnectorTool *>(_desktop->getTool());
    if (!tool) {
        return;
    }

    if (_line_tool.get_active() && tool->tool_mode != Tools::CONNECTOR_LINE_MODE) {
        _point_tool.set_active(false);
        tool->setToolMode(Tools::CONNECTOR_LINE_MODE);
    }
    if (_point_tool.get_active() && tool->tool_mode != Tools::CONNECTOR_POINT_MODE) {
        _line_tool.set_active(false);
        tool->setToolMode(Tools::CONNECTOR_POINT_MODE);
    }
}

void ConnectorToolbar::path_set_avoid()
{
    if (!_desktop) {
        return;
    }

    bool set_avoid = _avoid.get_active();
    auto document = _desktop->getDocument();
    auto selection = _desktop->getSelection();

    int changed = false;
    for (auto item : selection->items()) {
        changed |= LPEConnectorAvoid::toggleAvoid(item, set_avoid);
    }
    if (changed && set_avoid) {
        DocumentUndo::done(document, _("Make connectors avoid selected objects"), "");
    } else if (changed && !set_avoid) {
        DocumentUndo::done(document, _("Make connectors ignore selected objects"), "");
    } else {
        _desktop->messageStack()->flash(Inkscape::WARNING_MESSAGE,
                                        _("Select <b>at least one non-connector object</b>."));
    }
}

void ConnectorToolbar::orthogonal_toggled()
{
    Preferences::get()->setBool("/tools/connector/orthogonal", _orthogonal.get_active());
}

void ConnectorToolbar::curvature_changed()
{
    Preferences::get()->setDouble(Glib::ustring("/tools/connector/curvature"), _curvature_adj->get_value());
}

void ConnectorToolbar::steps_changed()
{
    Preferences::get()->setDouble(Glib::ustring("/tools/connector/steps"), _steps_adj->get_value());
}

void ConnectorToolbar::spacing_changed()
{
    Preferences::get()->setDouble(Glib::ustring("/tools/connector/spacing"), _spacing_adj->get_value());
}

void ConnectorToolbar::jump_size_changed()
{
    Preferences::get()->setDouble(Glib::ustring("/tools/connector/jump-size"), _jump_size_adj->get_value());
}

void ConnectorToolbar::jump_type_toggled()
{
    Preferences::get()->setBool("/tools/connector/jump-type", _jump_type.get_active());
}

/**
 * Set the selected avoided (or not avoided) shapes.
 */
void ConnectorToolbar::select_avoided(Inkscape::Selection *selection)
{
    bool avoided = false;
    for (auto item : selection->items()) {
        if (!LivePathEffect::isConnector(item)) {
            avoided |= LivePathEffect::isAvoided(item);
        }
    }
    //set_toggle_active(_avoid, avoided);
}

/**
 * Set the selected lines (called from connector-tool.cpp)
 */
void ConnectorToolbar::select_lines(std::vector<SPShape *> const &lines)
{
    // Set the toolbar icons to the average of the selected items.
    bool conn_type = false;
    bool jump_type = false;
    int curve_count = 0;
    double spacing = 0.0;
    double curvature = 0.0;
    double jump_size = 0.0;
    for (auto item : lines) {
        if (LivePathEffect::isConnector(item)) {
            auto lpe = LivePathEffect::LPEConnectorLine::get(item);
            curve_count++;
            spacing += lpe->getSpacing();
            curvature += lpe->getCurvature();
            jump_size += lpe->getJumpSize();
            jump_type |= lpe->getJumpType() == LivePathEffect::JumpMode::Arc;
            conn_type |= lpe->getConnType() == Avoid::ConnType_Orthogonal;
        }
    }
    if (curve_count) {
        _spacing_adj->set_value(spacing / curve_count);
        _curvature_adj->set_value(curvature / curve_count);
        _jump_size_adj->set_value(jump_size / curve_count);
        //set_toggle_active(_jump_type, jump_type);
        //set_toggle_active(_orthogonal, conn_type);
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
