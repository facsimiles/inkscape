#ifndef INKSCAPE_UI_DESKTOP_MENUBAR_WINDOWS
#define INKSCAPE_UI_DESKTOP_MENUBAR_WINDOWS

#include <gtkmm/window.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>

// This class displays a Windows-style titlebar button
class WindowsTitlebarButton final : public Gtk::Button {
public:
    WindowsTitlebarButton();
    ~WindowsTitlebarButton() = default;
};

// This class displays three Windows-style buttons for closing, minimizing, and maximizing a Window
class WindowsTitlebar final : public Gtk::Box {
public:
    WindowsTitlebar(Gtk::Window *window);
    ~WindowsTitlebar() = default;
private:
    Gtk::Window *window = nullptr;
    WindowsTitlebarButton *close_btn;
    WindowsTitlebarButton *maximize_btn;
    WindowsTitlebarButton *minimize_btn;
    void close_btn_pressed();
    void maximize_btn_pressed();
    void minimize_btn_pressed();
};

#endif // INKSCAPE_UI_DESKTOP_MENUBAR_WINDOWS
