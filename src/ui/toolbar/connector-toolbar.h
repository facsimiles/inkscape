// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_CONNECTOR_TOOLBAR_H
#define SEEN_CONNECTOR_TOOLBAR_H

/**
 * @file
 * Connector toolbar
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

#include <gtkmm/adjustment.h>

#include "toolbar.h"
#include "object/sp-shape.h"

#include "xml/node-observer.h"

class SPDesktop;

namespace Gtk {
class ToggleButton;
}

namespace Inkscape {
class Selection;

namespace XML {
class Node;
}

namespace UI {
namespace Toolbar {

class ConnectorToolbar
	: public Toolbar
    , private XML::NodeObserver
{
public:
    ConnectorToolbar(SPDesktop *desktop);
    ~ConnectorToolbar() override;

    void select_avoided(Inkscape::Selection *selection);
    void select_lines(std::vector<SPShape *> const &lines);

private:
    Glib::RefPtr<Gtk::Builder> _builder;

public:
    Gtk::ToggleButton &_line_tool;
    Gtk::ToggleButton &_point_tool;

private:
    Gtk::ToggleButton &_avoid;
    Gtk::ToggleButton &_orthogonal;

    Glib::RefPtr<Gtk::Adjustment> _curvature_adj;
    Glib::RefPtr<Gtk::Adjustment> _steps_adj;
    Glib::RefPtr<Gtk::Adjustment> _spacing_adj;
    Glib::RefPtr<Gtk::Adjustment> _jump_size_adj;

    Gtk::ToggleButton &_jump_type;

    Inkscape::XML::Node *_repr = nullptr;

    void tool_toggled();
    void path_set_avoid();
    void orthogonal_toggled();
    void graph_layout();
    void directed_graph_layout_toggled();
    void nooverlaps_graph_layout_toggled();
    void curvature_changed();
    void steps_changed();
    void spacing_changed();
    void jump_size_changed();
    void jump_type_toggled();
    void length_changed();
};

}
}
}

#endif // SEEN_CONNECTOR_TOOLBAR_H
