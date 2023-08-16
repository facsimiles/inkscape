// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Johan B. C. Engelen
 *
 * Copyright (C) 2011 Author
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_SPINBUTTON2_H
#define INKSCAPE_UI_WIDGET_SPINBUTTON2_H

#include <iomanip>
#include <gtkmm.h>

namespace Inkscape {
namespace UI {
namespace Widget {
namespace Ink2 {

#if GTK_CHECK_VERSION(4, 0, 0)
  // Work-around do to missing signal_activate() in GTK4.
  void on_activate_c(GtkEntry* entry, gpointer user_data);
#endif

#if GTK_CHECK_VERSION(4, 0, 0)
  class SpinButton : public Gtk::Box {

    using parent_type = Gtk::Box;
#else
  class SpinButton : public Gtk::EventBox {  // Gtk3 needs an EventBox to capture enter/leave events.  
                                             // (But hiding/unhiding +/- buttons doesn't work... so
                                             // maybe this isn't needed.)
    using parent_type = Gtk::EventBox;
#endif

  public:
#if GTK_CHECK_VERSION(4, 0, 0)
    SpinButton(Gtk::Orientation orientation = Gtk::Orientation::HORIZONTAL);
#else
    SpinButton(Gtk::Orientation orientation = Gtk::ORIENTATION_HORIZONTAL);
#endif
    virtual ~SpinButton() {};

    void set_adjustment(const Glib::RefPtr<Gtk::Adjustment>& adjustment);
    Glib::RefPtr<Gtk::Adjustment> get_adjustment() { return m_adjustment; }
    void set_digits(int digits);
    int get_digits();

    void update();
  
    // ----------- PROPERTIES ------------
    Glib::PropertyProxy<int> property_digits()  { return prop_digits.get_proxy(); }

  private:
    Glib::RefPtr<Gtk::Adjustment> m_adjustment;

#if GTK_CHECK_VERSION(4, 0, 0)
#else
    Gtk::Box*      m_box   = nullptr;
    Gtk::EventBox* m_event = nullptr;
#endif

    Gtk::Button*   m_minus = nullptr;
    Gtk::Label*    m_value = nullptr;
    Gtk::Button*   m_plus  = nullptr;
    Gtk::Entry*    m_entry = nullptr;

    Glib::RefPtr<Gdk::Cursor> m_old_cursor;

#if GTK_CHECK_VERSION(4, 0, 0)

    // ------------- CONTROLLERS -------------
    // Only Gestures are available in GTK3 (and not GestureClick).
    // We'll rely on signals for GTK3.

    Glib::RefPtr<Gtk::EventControllerMotion> m_motion;
    void on_motion_enter(double x, double y);
    void on_motion_leave();

    Glib::RefPtr<Gtk::EventControllerMotion> m_motion_value;
    void on_motion_enter_value(double x, double y);
    void on_motion_leave_value();

    Glib::RefPtr<Gtk::GestureDrag> m_drag_value;
    void on_drag_begin_value(Gdk::EventSequence *sequence);
    void on_drag_update_value(Gdk::EventSequence *sequence);
    void on_drag_end_value(Gdk::EventSequence *sequence);

    Glib::RefPtr<Gtk::EventControllerScroll> m_scroll;
    void on_scroll_begin(); // Not used with mouse wheel.
    bool on_scroll(double dx, double dy);
    void on_scroll_end(); // Not used with mouse wheel.

    // Use Gesture to get access to modifier keys (rather than "clicked" signal).
    Glib::RefPtr<Gtk::GestureClick> m_click_plus;
    void on_pressed_plus(int n_press, double x, double y);
    Glib::RefPtr<Gtk::GestureClick> m_click_minus;
    void on_pressed_minus(int n_press, double x, double y);

    // Glib::RefPtr<Gtk::EventControllerKey> m_key_entry;
    // bool on_key_pressed (guint keyval, guint keycode, Gdk::ModifierType state); // "pressed" vs GTK3 "press"
    // void on_key_released(guint keyval, guint keycode, Gdk::ModifierType state);

#else

    // ------------- CONTROLLERS -------------
    Glib::RefPtr<Gtk::GestureDrag> m_drag_value;
    void on_drag_begin_value(double start_x, double start_y);
    void on_drag_update_value(double offset_x, double offset_y);
    void on_drag_end_value(double offset_x, double offset_y);

    // --------------  EVENTS  ----------------

    // Hide/show +/- buttons. RESULTS IN INFINITE LOOP
    bool on_enter_notify(GdkEventCrossing* crossing_event);
    bool on_leave_notify(GdkEventCrossing* crossing_event);
    sigc::connection on_enter_notify_connection;
    sigc::connection on_leave_notify_connection;

    // This is mouse movement. Sets cursor. (Actually uses Gtk::EventBox wrapper around value.)
    bool on_enter_notify_value(GdkEventCrossing* crossing_event);
    bool on_leave_notify_value(GdkEventCrossing* crossing_event);

    // Scroll value
    bool on_scroll_event(GdkEventScroll* scroll_event);

    // Drag value (Can't be used as a controller steals mouse-button-one press events.)
    // bool on_button_press_event_drag(GdkEventButton* button_event);
    // bool on_motion_notify_event_drag(GdkEventMotion* motion_event);
    // bool on_button_release_event_drag(GdkEventButton* button_event);

    // Plus/minus buttons
    bool on_button_press_event(GdkEventButton* button_event, bool plus);

#endif

  public:
    void on_activate(); // Needs to be public for GTK4 work-around.

  private:
    void on_changed();
    void on_editing_done();

    // ---------------- DATA ------------------
    double m_initial_value = 0.0; // Value of adjustment at start of drag.
    double m_initial_x = 0.0; // Cursor position at start of drag (GTK3).
    bool m_dragged = false; // Hack to avoid enabling entry after drag. TODO Probably not needed now.

    // ----------- PROPERTIES ------------
    Glib::Property<int> prop_digits;
};
} // namespace Ink2
} // namespace Widget
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_WIDGET_SPINBUTTON_H

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
