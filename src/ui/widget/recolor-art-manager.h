// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef INKSCAPE_UI_WIDGET_RECOLOR_ART_MANAGER_H
#define INKSCAPE_UI_WIDGET_RECOLOR_ART_MANAGER_H
/*
 * Authors:
 *   Fatma Omara <ftomara647@gmail.com>
 *
 * Copyright (C) 2025 authors
 */

#include <gtkmm/popover.h>

#include "ui/widget/recolor-art.h"
#include "selection.h"

namespace Inkscape::UI::Widget {

class RecolorArtManager
{
public:
    static RecolorArtManager &get();

    Gtk::Popover &getPopOver();

    static bool checkSelection(Inkscape::Selection *selection);
    static bool checkMeshObject(Inkscape::Selection *selection);

    void setDesktop(SPDesktop *desktop);
    void performUpdate();
    void performMarkerUpdate(SPMarker *marker);

private:
    RecolorArtManager();

    RecolorArt _recolor_widget;
    Gtk::Popover _recolorPopOver;
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_RECOLOR_ART_MANAGER_H
