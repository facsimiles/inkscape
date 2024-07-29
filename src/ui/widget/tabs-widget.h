// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Inkscape document tabs bar.
 */
/*
 * Authors:
 *   PBS <pbs3141@gmail.com>
 *
 * Copyright (C) 2024 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_TABS_WIDGET_H
#define INKSCAPE_UI_WIDGET_TABS_WIDGET_H

#include <memory>
#include <vector>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <2geom/point.h>

class SPDesktop;
class SPDesktopWidget;

namespace Inkscape::UI::Widget {

struct Tab;

class TabsWidget : public Gtk::Box
{
public:
    TabsWidget(SPDesktopWidget *desktop_widget);
    ~TabsWidget() override;

    void addTab(SPDesktop *desktop, int pos = -1);
    void removeTab(SPDesktop *desktop);
    void switchTab(SPDesktop *desktop);
    void refreshTitle(SPDesktop *desktop);

    int positionOfTab(SPDesktop *desktop) const;
    SPDesktop *tabAtPosition(int i) const;

private:
    SPDesktopWidget *const _desktop_widget;

    std::vector<std::shared_ptr<Tab>> _tabs;
    std::weak_ptr<Tab> _active;
    std::weak_ptr<Tab> _right_clicked;

    void _updateVisibility();
    Glib::ustring _getTitle(SPDesktop *desktop) const;
    void _setTooltip(SPDesktop *desktop, Glib::RefPtr<Gtk::Tooltip> const &tooltip);
    int _computeDropLocation(int tab_x) const;
    void _resetLayout();
    void _adjustLayoutForDropLocation(int i, int width);
    std::pair<std::weak_ptr<Tab>, Geom::Point> _tabAtPoint(Geom::Point const &pos);
    void _reorderTab(int from, int to);
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_TABS_WIDGET_H
