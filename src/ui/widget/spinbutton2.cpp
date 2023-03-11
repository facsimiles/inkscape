// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Johan B. C. Engelen
 *
 * Copyright (C) 2011 Author
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "spinbutton2.h"
#include "util/expression-evaluator.h"
#include "ui/tools/tool-base.h"
#include "ui/util.h"

namespace Inkscape {
namespace UI {
namespace Widget {
namespace Ink2 {

#if GTK_CHECK_VERSION(4, 0, 0)
    SpinButton::SpinButton(Gtk::Orientation orientation)
      : Glib::ObjectBase("Ink2::SpinButton")
      , Gtk::Box(orientation)
      , prop_digits(*this, "digits")
#else
        SpinButton::SpinButton(Gtk::Orientation orientation)
      : Glib::ObjectBase("Ink2::SpinButton")
      , Gtk::EventBox()
      , prop_digits(*this, "digits")
#endif
    {
      set_name("Ink2::SpinButton");
//cursor(4, 4);
      m_minus = Gtk::manage(new Gtk::Button()); m_minus->set_name("Ink2::SpinButton::Minus");
      m_value = Gtk::manage(new Gtk::Label());  m_value->set_name("Ink2::SpinButton::Value");
      m_plus  = Gtk::manage(new Gtk::Button()); m_plus->set_name("Ink2::SpinButton::Plus");
      m_entry = Gtk::manage(new Gtk::Entry());  m_entry->set_name("Ink2::SpinButton::Entry");

      m_value->set_vexpand(true);
      m_value->set_hexpand(true);
      m_entry->set_vexpand(true);
      m_entry->set_hexpand(true);


#if GTK_CHECK_VERSION(4, 0, 0)

      m_minus->set_margin(0);
      m_value->set_margin(0);
      m_plus->set_margin(0);

      m_minus->set_icon_name("value-decrease-symbolic");
      m_plus->set_icon_name("value-increase-symbolic");

      if (orientation == Gtk::Orientation::HORIZONTAL) {
        append(*m_minus);
        append(*m_value);
        append(*m_entry);
        append(*m_plus);
      } else {
        append(*m_plus);
        append(*m_value);
        append(*m_entry);
        append(*m_minus);
      }

#else

      m_minus->set_image_from_icon_name("value-decrease-symbolic");
      m_plus->set_image_from_icon_name("value-increase-symbolic");

      m_box   = Gtk::manage(new Gtk::Box(orientation));
      m_event = Gtk::manage(new Gtk::EventBox());

      if (orientation == Gtk::ORIENTATION_HORIZONTAL) {
        m_box->pack_start(*m_minus);
        m_box->pack_start(*m_event);
        m_box->pack_start(*m_entry);
        m_box->pack_start(*m_plus);
      } else {
        m_box->pack_start(*m_plus);
        m_box->pack_start(*m_event);
        m_box->pack_start(*m_entry);
        m_box->pack_start(*m_minus);
      }
      m_event->add(*m_value);
      add(*m_box);
      m_event->show();
      m_box->show();
      show();

#endif

      static Glib::RefPtr<Gtk::CssProvider> provider;
      if (!provider) {
        provider = Gtk::CssProvider::create();
        Glib::ustring css = R"=====(
          box    { border: 1px solid lightgray; border-radius: 3px;}
          button { border: none;}
      )=====";
        provider->load_from_data(css);

        // auto const display = Gdk::Display::get_default();
        // Gtk::StyleContext::add_provider_for_display(display, provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

      }

      Glib::RefPtr<Gtk::StyleContext> style = get_style_context();
      Glib::RefPtr<Gtk::StyleContext> style_minus = m_minus->get_style_context();
      Glib::RefPtr<Gtk::StyleContext> style_value = m_value->get_style_context();
      Glib::RefPtr<Gtk::StyleContext> style_plus = m_plus->get_style_context();
      style->add_provider(      provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      style_minus->add_provider(provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      style_value->add_provider(provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      style_plus->add_provider( provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

#if GTK_CHECK_VERSION(4, 0, 0)
      //                  GTK4
      // ------------- CONTROLLERS -------------

      // This is mouse movement. Shows/hides +/- buttons.
      // Shows/hides +/- buttons.
      m_motion = Gtk::EventControllerMotion::create();
      m_motion->signal_enter().connect(sigc::mem_fun(*this, &SpinButton::on_motion_enter));
      m_motion->signal_leave().connect(sigc::mem_fun(*this, &SpinButton::on_motion_leave));
      add_controller(m_motion);

      // This is mouse movement. Sets cursor.
      m_motion_value = Gtk::EventControllerMotion::create();
      m_motion_value->signal_enter().connect(sigc::mem_fun(*this, &SpinButton::on_motion_enter_value));
      m_motion_value->signal_leave().connect(sigc::mem_fun(*this, &SpinButton::on_motion_leave_value));
      m_value->add_controller(m_motion_value);

      // This is mouse drag movement. Changes value.
      m_drag_value = Gtk::GestureDrag::create();
      m_drag_value->signal_begin().connect(sigc::mem_fun(*this, &SpinButton::on_drag_begin_value));
      m_drag_value->signal_update().connect(sigc::mem_fun(*this, &SpinButton::on_drag_update_value));
      m_drag_value->signal_end().connect(sigc::mem_fun(*this, &SpinButton::on_drag_end_value));
      m_drag_value->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
      m_value->add_controller(m_drag_value);

      // Changes value.
      m_scroll = Gtk::EventControllerScroll::create();
      m_scroll->signal_scroll_begin().connect(sigc::mem_fun(*this, &SpinButton::on_scroll_begin));
      m_scroll->signal_scroll().connect(      sigc::mem_fun(*this, &SpinButton::on_scroll      ), false);
      m_scroll->signal_scroll_end().connect(  sigc::mem_fun(*this, &SpinButton::on_scroll_end  ));
      m_scroll->set_flags(Gtk::EventControllerScroll::Flags::BOTH_AXES); // Mouse wheel is on y.
      add_controller(m_scroll);

      m_click_minus = Gtk::GestureClick::create();
      m_click_minus->signal_pressed().connect(sigc::mem_fun(*this, &SpinButton::on_pressed_minus));
      m_click_minus->set_propagation_phase(Gtk::PropagationPhase::CAPTURE); // Steal from default handler.
      m_minus->add_controller(m_click_minus);

      m_click_plus = Gtk::GestureClick::create();
      m_click_plus->signal_pressed().connect(sigc::mem_fun(*this, &SpinButton::on_pressed_plus));
      m_click_plus->set_propagation_phase(Gtk::PropagationPhase::CAPTURE); // Steal from default handler.
      m_plus->add_controller(m_click_plus);

      // m_key_entry = Gtk::EventControllerKey::create();
      // m_key_entry->signal_key_pressed().connect(sigc::mem_fun(*this, &SpinButton::on_key_pressed), true); // Before default handler.
      // m_entry->add_controller(m_key_entry);

      //                  GTK4
      // -------------   SIGNALS   -------------

      // GTKMM4 missing signal_activate()!
      g_signal_connect(G_OBJECT(m_entry->gobj()), "activate", G_CALLBACK(on_activate_c), this);

      // Value (button) NOT USED, Click handled by zero length drag.
      // m_value->signal_clicked().connect(sigc::mem_fun(*this, &SpinButton::on_value_clicked));

#else
      //                  GTK3
      // ------------- CONTROLLERS -------------
      // This is mouse drag movement. Changes value.
      m_drag_value = Gtk::GestureDrag::create(*m_event);
      m_drag_value->signal_drag_begin().connect(sigc::mem_fun(*this, &SpinButton::on_drag_begin_value), true);
      m_drag_value->signal_drag_update().connect(sigc::mem_fun(*this, &SpinButton::on_drag_update_value), true);
      m_drag_value->signal_drag_end().connect(sigc::mem_fun(*this, &SpinButton::on_drag_end_value), true);
      m_drag_value->set_propagation_phase(Gtk::PropagationPhase::PHASE_CAPTURE);

      //                  GTK3
      // -------------   SIGNALS   -------------

      m_entry->signal_activate().connect(sigc::mem_fun(*this, &SpinButton::on_activate));

      add_events(Gdk::SCROLL_MASK         |
                 Gdk::SMOOTH_SCROLL_MASK  |
                 Gdk::BUTTON_PRESS_MASK   |
                 Gdk::BUTTON_RELEASE_MASK |
                 Gdk::POINTER_MOTION_MASK);

      // Show/hide +/- buttons. Finish entry on leave.
      // on_enter_notify_connection =
      //   signal_enter_notify_event().connect(sigc::mem_fun(*this, &SpinButton::on_enter_notify), true);
      on_leave_notify_connection =
        signal_leave_notify_event().connect(sigc::mem_fun(*this, &SpinButton::on_leave_notify), true);

      m_event->signal_enter_notify_event().connect(sigc::mem_fun(*this, &SpinButton::on_enter_notify_value), true);
      m_event->signal_leave_notify_event().connect(sigc::mem_fun(*this, &SpinButton::on_leave_notify_value), true);

      signal_scroll_event().connect( sigc::mem_fun(*this, &SpinButton::on_scroll_event), false);

      // Gtk::Button GtkGestureMultiPress steals Button1 press event! (Checked with Debug window.)
      // m_value->signal_button_press_event().connect(sigc::mem_fun(*this, &SpinButton::on_button_press_event_drag), true);
      // m_value->signal_motion_notify_event().connect(sigc::mem_fun(*this, &SpinButton::on_motion_notify_event_drag));
      // m_value->signal_button_release_event().connect(sigc::mem_fun(*this, &SpinButton::on_button_release_event_drag));

      // Plus/Minus Buttons
      m_minus->signal_button_press_event().connect
        (sigc::bind(sigc::mem_fun(*this, &SpinButton::on_button_press_event), false), false);
      m_plus->signal_button_press_event().connect
        (sigc::bind(sigc::mem_fun(*this, &SpinButton::on_button_press_event), true),  false);

      // Entry
      m_entry->signal_changed().connect(sigc::mem_fun(*this, &SpinButton::on_changed));
      m_entry->signal_editing_done().connect(sigc::mem_fun(*this, &SpinButton::on_editing_done));

#endif

#if GTK_CHECK_VERSION(4, 0, 0)
      m_plus->hide();
      m_minus->hide();
      m_entry->hide();
#else
      m_plus->show();  // Show as we can't get enter_notify_event signal to work (infinite loop)!
      m_value->show(); // GTK3 hides by default.
      m_minus->show();
      m_entry->hide();
#endif   
    set_adjustment(Gtk::Adjustment::create(0.0, 0.0, 100.0, 1.0, 5.0, 0.0));   
    }

    void
    SpinButton::set_adjustment(const Glib::RefPtr<Gtk::Adjustment>& adjustment)
    {
      m_adjustment = adjustment;
      m_adjustment->signal_value_changed().connect(sigc::mem_fun(*this, &SpinButton::update));
      update();
    }

    void
    SpinButton::set_digits(int digits)
    {
      prop_digits.set_value(digits);
      update();
    }

    int
    SpinButton::get_digits()
    {
      return prop_digits.get_value();
    }
    
    void
    SpinButton::update()
    {
      if (m_adjustment) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(prop_digits.get_value()) << m_adjustment->get_value();

        m_value->set_label(ss.str());
        m_entry->set_text(ss.str());

        if (m_adjustment->get_value() <= m_adjustment->get_lower()) {
          m_minus->set_sensitive(false);
        } else {
          m_minus->set_sensitive(true);
        }

        if (m_adjustment->get_value() >= m_adjustment->get_upper()) {
          m_plus->set_sensitive(false);
        } else {
          m_plus->set_sensitive(true);
        }
      }
    }


#if GTK_CHECK_VERSION(4, 0, 0)

    // ---------------- CONTROLLERS -----------------

    // ------------------  MOTION  ------------------

    void SpinButton::on_motion_enter(double x, double y)
    {
      m_minus->show();
      m_plus->show();
    }

    void SpinButton::on_motion_leave()
    {
      m_minus->hide();
      m_plus->hide();

      if (m_entry->is_visible()) {
        // We left spinbutton, save value and update.
        on_activate();
      }
    }

    // ---------------  MOTION VALUE  ---------------

    void SpinButton::on_motion_enter_value(double x, double y)
    {
      m_old_cursor = get_cursor();
      auto new_cursor = Gdk::Cursor::create("ew-resize");
      set_cursor(new_cursor);
    }

    void SpinButton::on_motion_leave_value()
    {
      set_cursor(m_old_cursor);
    }

    // ---------------   DRAG VALUE  ----------------

    void SpinButton::on_drag_begin_value(Gdk::EventSequence *sequence)
    {
      m_initial_value = m_adjustment->get_value();
    }

    void SpinButton::on_drag_update_value(Gdk::EventSequence *sequence)
    {
      double dx = 0.0;
      double dy = 0.0;
      m_drag_value->get_offset(dx, dy);
      dx = std::round(dx/10.0);

      // If we don't move, then it probably was a button click.
      if (dx != 0.0) {
        double scale = 1.0;
        auto state = m_drag_value->get_current_event_state();
        if ((state & Gdk::ModifierType::CONTROL_MASK) == Gdk::ModifierType::CONTROL_MASK) {
          scale = 0.1;
        } else if ((state & Gdk::ModifierType::SHIFT_MASK) == Gdk::ModifierType::SHIFT_MASK) {
          scale = 10.0;
        }
        m_adjustment->set_value (m_initial_value + scale * dx);

        m_dragged = true;
      }
    }

    void SpinButton::on_drag_end_value(Gdk::EventSequence *sequence)
    {
      double dx = 0.0;
      double dy = 0.0;
      m_drag_value->get_offset(dx, dy);

      if (dx == 0 && !m_dragged) {
        // Must have been a click!
        m_minus->hide();
        m_value->hide();
        m_plus->hide();
        m_entry->select_region(0, m_entry->get_text_length());
        m_entry->show();
        m_entry->grab_focus();
      }
      m_dragged = false;
    }

    // ------------------  SCROLL  ------------------

    void SpinButton::on_scroll_begin()
    {
      // NOT USED.
    }

    bool SpinButton::on_scroll(double dx, double dy)
    {
      double scale = 1.0;
      auto state = m_scroll->get_current_event_state();
      if ((state & Gdk::ModifierType::CONTROL_MASK) == Gdk::ModifierType::CONTROL_MASK) {
        scale = 0.1;
      } else if ((state & Gdk::ModifierType::SHIFT_MASK) == Gdk::ModifierType::SHIFT_MASK) {
        scale = 10.0;
      }
      m_adjustment->set_value(m_adjustment->get_value() + m_adjustment->get_step_increment() * scale * dy);
    
      return true;
    }

    void SpinButton::on_scroll_end()
    {
      // NOT USED.
    }

    // ------------------   KEY    ------------------

    // bool SpinButton::on_key_pressed(guint keyval, guint keycode, Gdk::ModifierType state)
    // {
    //   switch (keyval) {
    //     // case GDK_KEY_Escape: // Defocus
    //     //   std::cout << "ESC" << std::endl;
    //     //   set_value(saved_value);
    //     //   defocus();
    //     //   return true;
    //     //   break;
    //   case GDK_KEY_Return:
    //     on_activate();
    //     return true;
    //     break;

    //   case GDK_KEY_z:
    //   case GDK_KEY_Z:
    //     if ((state & Gdk::ModifierType::CONTROL_MASK) == Gdk::ModifierType::CONTROL_MASK) {
    //       m_entry->hide();
    //       m_minus->show();
    //       m_value->show();
    //       m_plus->show();
    //       return true;
    //     }
    //     break;

    //   default:
    //     break;
    //   }

    //   return false;
    // }

    // ------------------  CLICK   ------------------

    void SpinButton::on_pressed_plus(int n_press, double x, double y)
    {
      double scale = 1.0;
      auto state = m_click_plus->get_current_event_state();
      if ((state & Gdk::ModifierType::CONTROL_MASK) == Gdk::ModifierType::CONTROL_MASK) {
        scale = 0.1;
      } else if ((state & Gdk::ModifierType::SHIFT_MASK) == Gdk::ModifierType::SHIFT_MASK) {
        scale = 10.0;
      }
      m_adjustment->set_value (m_adjustment->get_value() + m_adjustment->get_step_increment() * scale);
    }

    void SpinButton::on_pressed_minus(int n_press, double x, double y)
    {
      double scale = 1.0;
      auto state = m_click_minus->get_current_event_state();
      if ((state & Gdk::ModifierType::CONTROL_MASK) == Gdk::ModifierType::CONTROL_MASK) {
        scale = 0.1;
      } else if ((state & Gdk::ModifierType::SHIFT_MASK) == Gdk::ModifierType::SHIFT_MASK) {
        scale = 10.0;
      }
      m_adjustment->set_value (m_adjustment->get_value() - m_adjustment->get_step_increment() * scale);
    }

#else
    // ===================  GTK3 ==================

    //  -----------------   GTK3   ------------------
    //  ---------------- CONTROLLERS ----------------
    void SpinButton::on_drag_begin_value(double start_x, double start_y)
    {
      m_initial_value = m_adjustment->get_value();
    }

    void SpinButton::on_drag_update_value(double dx, double dy)
    {
      dx = std::round(dx/10.0);
      //cursor((int)dx, 200);
      // If we don't move, then it probably was a button click.
      if (dx != 0.0) {
        double scale = 1.0;
        // CAN'T GET STATE IN GTK3.
        // auto state = m_drag_value->get_current_event_state();
        // if ((state & Gdk::ModifierType::CONTROL_MASK) == Gdk::ModifierType::CONTROL_MASK) {
        //   scale = 0.1;
        // } else if ((state & Gdk::ModifierType::SHIFT_MASK) == Gdk::ModifierType::SHIFT_MASK) {
        //   scale = 10.0;
        // }
        m_adjustment->set_value (m_initial_value + scale * dx);

        m_dragged = true;
      }
    }

    void SpinButton::on_drag_end_value(double dx, double dy)
    {
      if (dx == 0 && !m_dragged) {
        // Must have been a click!
        m_minus->hide();
        m_value->hide();
        m_plus->hide();
        m_entry->select_region(0, m_entry->get_text_length());
        m_entry->show();
        m_entry->grab_focus();
      }
      m_dragged = false;
    }

    //  -----------------   GTK3   ------------------
    //  -----------------  SIGNALS ------------------


    bool
    SpinButton::on_enter_notify(GdkEventCrossing* crossing_event)
    {
      if (m_value->is_visible()) {
        // Showing/hiding these buttons creates an infinite loop, even if signal blocked.
        // m_minus->show();
        // m_plus->show();
      }
      return true;
    }

    bool
    SpinButton::on_leave_notify(GdkEventCrossing* crossing_event)
    {
      if (m_value->is_visible()) {
        // Showing/hiding these buttons creates an infinite loop, even if signal blocked.
        // m_minus->hide();
        // m_plus->hide();
      }

      if (m_entry->is_visible()) {
        // We left spinbutton, save value and update.
        on_activate();
      }

      return true;
    }

    bool
    SpinButton::on_enter_notify_value(GdkEventCrossing* crossing_event)
    {
      auto window = get_window();
      m_old_cursor = window->get_cursor();
      auto new_cursor = Gdk::Cursor::create(Gdk::Display::get_default(), "ew-resize");
      window->set_cursor(new_cursor);
      return true;
    }

    bool
    SpinButton::on_leave_notify_value(GdkEventCrossing* crossing_event)
    {
      auto window = get_window();
      window->set_cursor(m_old_cursor);
      return true;
    }

    bool
    SpinButton::on_scroll_event(GdkEventScroll* scroll_event)
    {
      double scale = 1.0;
      auto state = scroll_event->state;
      if ((state & Gdk::ModifierType::CONTROL_MASK) == Gdk::ModifierType::CONTROL_MASK) {
        scale = 0.1;
      } else if ((state & Gdk::ModifierType::SHIFT_MASK) == Gdk::ModifierType::SHIFT_MASK) {
        scale = 10.0;
      }
      m_adjustment->set_value(m_adjustment->get_value() + m_adjustment->get_step_increment() * scale * scroll_event->delta_y);
      return true;
    }

    // Doesn't work due to a controller stealing mouse-button-one press events.
    // bool
    // SpinButton::on_button_press_event_drag(GdkEventButton* button_event)
    // {
    //   std::cout << "Ink2::SpinButton::on_button_pressed_event_drag" << std::endl;
    //   m_initial_value = m_adjustment->get_value();
    //   // m_start_x = button_event->x;
    //   return true;
    // }

    // bool
    // SpinButton::on_motion_notify_event_drag(GdkEventMotion* motion_event)
    // {
    //   std::cout << "Ink2::SpinButton::on_motion_notify_event_drag" << std::endl;
    //   std::cout << "  " << motion_event->x << std::endl;
    //   auto state = motion_event->state;
    //   return true;
    // }
    // bool
    // SpinButton::on_button_release_event_drag(GdkEventButton* button_event)
    // {
    //   std::cout << "Ink2::SpinButton::on_button_release_event_drag" << std::endl;
    //   return true;
    // }


    //  ----------- +/- Buttons -----------

    // Can't use "clicked" signal as we can't get modifier key state.
    bool
    SpinButton::on_button_press_event(GdkEventButton* button_event, bool plus)
    {
      double scale = 1.0;
      auto state = button_event->state;
      if ((state & Gdk::ModifierType::CONTROL_MASK) == Gdk::ModifierType::CONTROL_MASK) {
        scale = 0.1;
      } else if ((state & Gdk::ModifierType::SHIFT_MASK) == Gdk::ModifierType::SHIFT_MASK) {
        scale = 10.0;
      }

      if (!plus) {
        scale = -scale;
      }

      m_adjustment->set_value(m_adjustment->get_value() + m_adjustment->get_step_increment() * scale);
      return true;
    }


#endif

    void
    SpinButton::on_activate()
    {
      double value = 0.0;
      try
        {
          value = std::stod(m_entry->get_text());
        }
      catch (const std::exception& e)
        {
          std::cerr << "SpinButton::on_activate: error: " << e.what() << std::endl;
          return;
        }

      m_adjustment->set_value(value);
      m_entry->hide();
      m_minus->show();
      m_value->show();
      m_plus->show();
    }

    void
    SpinButton::on_changed()
    {
      // NOT USED
    }

    void
    SpinButton::on_editing_done()
    {
      // NOT USED
    }

    // GTKMM4 bindings are missing signal_activate()!!
    void on_activate_c(GtkEntry* entry, gpointer user_data)
    {
      auto spinbutton = reinterpret_cast<Ink2::SpinButton *>(user_data);
      spinbutton->on_activate();
    }

} // namespace Ink2
} // namespace Widget
} // namespace UI
} // namespace Inkscape

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
