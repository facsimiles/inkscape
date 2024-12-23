#include "menubar-windows.h"

#include <gtkmm/cssprovider.h>
#include <gtkmm/stylecontext.h>

#include "io/resource.h"

WindowsTitlebarButton::WindowsTitlebarButton() {
    this->set_has_frame(false);

    Gtk::CssProvider *cssProvider;
}

WindowsTitlebar::WindowsTitlebar(Gtk::Window *window) {
    this->window = window;
}

void WindowsTitlebar::close_btn_pressed() {}
void WindowsTitlebar::minimize_btn_pressed() {}
void WindowsTitlebar::maximize_btn_pressed() {}
