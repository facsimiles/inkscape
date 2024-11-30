// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_UI_WIDGET_STACK_H
#define INKSCAPE_UI_WIDGET_STACK_H

#include <gtkmm/widget.h>

namespace Inkscape::UI::Widget {

class Stack : public Gtk::Widget
{
public:
    Stack();

    void add(Gtk::Widget &widget);
    void remove(Gtk::Widget &widget);
    void setActive(Gtk::Widget *widget);

protected:
    void snapshot_vfunc(Glib::RefPtr<Gtk::Snapshot> const &snapshot) override;

private:
    Gtk::Widget *_active = nullptr;
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_STACK_H
