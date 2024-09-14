// SPDX-License-Identifier: GPL-2.0-or-later
//
// Created by Michael Kowalski on 6/27/24.
//

#include "resizing-separator.h"

#include <gtkmm/cssprovider.h>
#include <gtkmm/gesturedrag.h>

namespace Inkscape::UI::Widget {

// language=CSS
auto resizing_separator_css = R"=====(
#ResizingSeparator{ border: 1px solid @unfocused_borders; border-radius: 1px; background-color: alpha(@unfocused_borders, 0.4); }
)=====";


ResizingSeparator::ResizingSeparator() {
    set_name("ResizingSeparator");
    set_size_request(-1, _size);
    set_hexpand();

    static Glib::RefPtr<Gtk::CssProvider> provider;
    if (!provider) {
        provider = Gtk::CssProvider::create();
        provider->load_from_data(resizing_separator_css);
        auto const display = Gdk::Display::get_default();
        Gtk::StyleContext::add_provider_for_display(display, provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 10);
    }

    set_cursor("ns-resize");

    _drag = Gtk::GestureDrag::create();
    _drag->signal_begin().connect(sigc::mem_fun(*this, &ResizingSeparator::on_drag_begin));
    _drag->signal_update().connect(sigc::mem_fun(*this, &ResizingSeparator::on_drag_update));
    _drag->signal_end().connect(sigc::mem_fun(*this, &ResizingSeparator::on_drag_end));
    _drag->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    add_controller(_drag);
}

void ResizingSeparator::resize(Gtk::Widget* widget, int max) {
    _resize = widget;
    _max_size = max;
}

void ResizingSeparator::on_drag_begin(Gdk::EventSequence* sequence) {
    _initial_size = _resize ? _resize->get_height() : 0;
    double x, y;
    _drag->get_start_point(x, y);
    auto start = compute_point(*get_parent(), Gdk::Graphene::Point(x, y));
    _initial_position = start->get_y();
}

void ResizingSeparator::on_drag_update(Gdk::EventSequence* sequence) {
    double dx = 0.0;
    double dy = 0.0;
    double x, y;
    if (_drag->get_offset(dx, dy) && _drag->get_start_point(x, y)) {
        auto end = compute_point(*get_parent(), Gdk::Graphene::Point(dx + x, y + dy));
        if (end.has_value()) {
            auto dist = end->get_y() - _initial_position;
            if (_resize) {
                auto size = int(dist) + _initial_size;
                size = std::clamp(size, 0, _max_size);
                _resize->set_size_request(-1, size);
                _signal_resized.emit(size);
            }
        }
    }
}

void ResizingSeparator::on_drag_end(Gdk::EventSequence* sequence) {
}

} // namespace
