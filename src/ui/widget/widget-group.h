// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef WIDGETGROUP_H
#define WIDGETGROUP_H

namespace Inkscape::UI::Widget {

class WidgetGroup {
public:
    void add(Gtk::Widget* widget) {
        assert(widget);
        _widgets.push_back(widget);
    }

    void set_visible(bool show) {
        for_each([=](auto w){ w->set_visible(show); });
    }

    void set_sensitive(bool enabled) {
        for_each([=](auto w){ w->set_sensitive(enabled); });
    }

    template <typename F>
    void for_each(F&& f) {
        for (auto w : _widgets) {
            f(w);
        }
    }
private:
    // non-owning pointers to widgets
    std::vector<Gtk::Widget*> _widgets;
};

}

#endif //WIDGETGROUP_H
