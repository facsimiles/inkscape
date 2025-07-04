// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * CommandPalette: Class providing Command Palette feature
 */
/* Authors:
 *     Abhay Raj Singh <abhayonlyone@gmail.com>
 *
 * Copyright (C) 2020 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_DIALOG_COMMAND_PALETTE_H
#define INKSCAPE_DIALOG_COMMAND_PALETTE_H

#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtk/gtk.h>     // GtkEventControllerKey
#include <gtkmm/enums.h> // Gtk::DirectionType
#include <optional>
#include <sigc++/connection.h>
#include <utility>

#include "cp-history-xml.h"

namespace Gdk {
enum class ModifierType;
}

namespace Gio {
class Action;
} // namespace Gio

namespace Gtk {
class Box;
class Builder;
class Label;
class ListBox;
class ListBoxRow;
class ScrolledWindow;
class SearchEntry2;
class Widget;
} // namespace Gtk

namespace Inkscape::UI::Dialog {

// Enables using switch case
enum class TypeOfVariant
{
    NONE,
    UNKNOWN,
    BOOL,
    INT,
    DOUBLE,
    STRING,
    TUPLE_DD
};

enum class CPMode
{
    SEARCH,
    INPUT, ///< Input arguments
    SHELL,
    HISTORY
};

class CommandPalette
{
public:
    CommandPalette();
    ~CommandPalette() = default;

    CommandPalette(CommandPalette const &) = delete;            // no copy
    CommandPalette &operator=(CommandPalette const &) = delete; // no assignment

    void open();
    void close();
    void toggle();

    Gtk::Box &get_base_widget() { return _CPBase; }

private:
    using ActionPtr = Glib::RefPtr<Gio::Action>;
    using ActionPtrName = std::pair<ActionPtr, Glib::ustring>;

    // Insert actions in _CPSuggestions
    void load_app_actions();
    void load_win_doc_actions();

    void append_recent_file_operation(Glib::ustring const &path, bool is_suggestion, bool is_import = true);
    bool generate_action_operation(ActionPtrName const &action_ptr_name, bool const is_suggestion);

    void on_search();

    int on_filter_general(Gtk::ListBoxRow *child);
    bool on_filter_full_action_name(Gtk::ListBoxRow *child);
    bool on_filter_recent_file(Gtk::ListBoxRow *child, bool const is_import);

    bool on_key_pressed(unsigned keyval, unsigned keycode, Gdk::ModifierType state);
    void on_window_focus(Gtk::Widget const *focus);

    void on_activate_cpfilter();
    bool on_entry_keypress(unsigned keyval, unsigned, Gdk::ModifierType);

    // when search bar is empty
    void hide_suggestions();
    // when search bar isn't empty
    void show_suggestions();

    void on_row_activated(Gtk::ListBoxRow *activated_row);
    void on_history_selection_changed(Gtk::ListBoxRow *lb);

    bool operate_recent_file(Glib::ustring const &uri, bool const import);

    void on_action_fullname_clicked(Glib::ustring const &action_fullname);

    /// Implements text matching logic
    static bool fuzzy_search(Glib::ustring const &subject, Glib::ustring const &search);
    static bool normal_search(Glib::ustring const &subject, Glib::ustring const &search);
    static bool fuzzy_tolerance_search(Glib::ustring const &subject, Glib::ustring const &search);
    static int fuzzy_points(Glib::ustring const &subject, Glib::ustring const &search);
    static int fuzzy_tolerance_points(Glib::ustring const &subject, Glib::ustring const &search);
    static int fuzzy_points_compare(int fuzzy_points_count_1, int fuzzy_points_count_2, int text_len_1, int text_len_2);

    static void test_sort();
    int on_sort(Gtk::ListBoxRow *row1, Gtk::ListBoxRow *row2);
    void set_mode(CPMode mode);

    // Color addition in searched character
    void add_color(Gtk::Label *label, Glib::ustring const &search, Glib::ustring const &subject, bool tooltip = false);
    void remove_color(Gtk::Label *label, Glib::ustring const &subject, bool tooltip = false);
    static void add_color_description(Gtk::Label *label, Glib::ustring const &search);

    // Executes Action
    bool ask_action_parameter(ActionPtrName const &action);
    static ActionPtrName get_action_ptr_name(Glib::ustring full_action_name);
    bool execute_action(ActionPtrName const &action, Glib::ustring const &value);

    static TypeOfVariant get_action_variant_type(ActionPtr const &action_ptr);

    static std::pair<Gtk::Label *, Gtk::Label *> get_name_desc(Gtk::ListBoxRow *child);
    Gtk::Label *get_full_action_name(Gtk::ListBoxRow *child);

    // Widgets
    Glib::RefPtr<Gtk::Builder> _builder;
    Gtk::Box &_CPBase;
    Gtk::Box &_CPListBase;
    Gtk::SearchEntry2 &_CPFilter;
    Gtk::ListBox &_CPSuggestions;
    Gtk::ListBox &_CPHistory;
    Gtk::ScrolledWindow &_CPSuggestionsScroll;
    Gtk::ScrolledWindow &_CPHistoryScroll;

    // Data
    int const _max_height_requestable = 360;
    Glib::ustring _search_text;

    // States
    bool _is_open = false;
    bool _win_doc_actions_loaded = false;

    /// History
    CPHistoryXML _history_xml;
    /**
     * Remember the mode we are in helps in unnecessary signal disconnection and reconnection
     * Used by set_mode()
     */
    CPMode _mode = CPMode::SHELL;
    // Default value other than SEARCH required
    // set_mode() switches between mode hence checks if it already in the target mode.
    // Constructed value is sometimes SEARCH being the first Item for now
    // set_mode() never attaches the on search listener then
    // This initialising value can be any thing other than the initial required mode
    // Example currently it's open in search mode

    /// Stores the search connection to deactivate when not needed
    sigc::connection _cpfilter_search_connection;

    /// Stores the most recent ask_action_name for when Entry::activate fires & we are in INPUT mode
    std::optional<ActionPtrName> _ask_action_ptr_name;
};

} // namespace Inkscape::UI::Dialog

#endif // INKSCAPE_DIALOG_COMMAND_PALETTE_H

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
