// SPDX-License-Identifier: GPL-2.0-or-later
//
// Created by Michael Kowalski on 6/27/24.
//

#ifndef RESIZING_SEPARATOR_H
#define RESIZING_SEPARATOR_H

#include <gtkmm/gesturedrag.h>
#include <gtkmm/widget.h>

namespace Inkscape::UI::Widget {

// This separator can be dragged vertically to resize associated widget, typically a sibling

class ResizingSeparator : public Gtk::Widget {
public:
    ResizingSeparator();
    ~ResizingSeparator() override = default;

    void resize(Gtk::Widget* widget, int max);
    sigc::signal<void (int)>& get_signal_resized() { return _signal_resized; }

private:
    int _size = 4;
    int _initial_position = 0;
    int _initial_size = 0;
    int _max_size = 0;
    Gtk::Widget* _resize = nullptr;
    sigc::signal<void (int)> _signal_resized;
    Glib::RefPtr<Gtk::GestureDrag> _drag;
    void on_drag_begin(Gdk::EventSequence* sequence);
    void on_drag_update(Gdk::EventSequence* sequence);
    void on_drag_end(Gdk::EventSequence* sequence);
};

} // namespace

#endif //RESIZING_SEPARATOR_H
