// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef INKSCAPE_UI_WIDGET_RECOLOR_ART_H
#define INKSCAPE_UI_WIDGET_RECOLOR_ART_H
/*
 * Authors:
 *   Fatma Omara <ftomara647@gmail.com>
 *
 * Copyright (C) 2025 authors
 */

#include <giomm/liststore.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/box.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/listview.h>
#include <gtkmm/notebook.h>
#include <gtkmm/signallistitemfactory.h>
#include <gtkmm/singleselection.h>
#include <gtkmm/popover.h>

#include "colors/color-set.h"
#include "object-colors.h"
#include "ui/operation-blocker.h"
#include "ui/widget/ink-color-wheel.h"

class SPMarker;

namespace Inkscape::Colors {
class Color;
class ColorSet;
} // namespace Inkscape::Colors

namespace Gtk {
class Builder;
class ListStore;
class Notebook;
} // namespace Gtk

class SPDesktop;

namespace Inkscape::UI::Widget {

class ColorNotebook;
class MultiMarkerColorPlate;

struct ColorItem : public Glib::Object
{
    uint32_t key;
    Colors::Color old_color{0};
    Colors::Color new_color{0};

    static Glib::RefPtr<ColorItem> create(uint32_t k, Colors::Color const &old_c, Colors::Color const &new_c)
    {
        auto item = Glib::make_refptr_for_instance<ColorItem>(new ColorItem());
        item->key = k;
        item->old_color = old_c;
        item->new_color = new_c;
        return item;
    }
};

class RecolorArt : public Gtk::Box
{
public:
    RecolorArt();

    void performUpdate();
    bool isInPreviewMode() const { return _is_preview; }
    void setDesktop(SPDesktop *desktop);
    void onResetClicked();
    void performMarkerUpdate(SPMarker *marker);

private:
    SPDesktop *_desktop = nullptr;
    Glib::RefPtr<Gtk::Builder> _builder;
    Gtk::Notebook &_notebook;
    Gtk::Box &_color_wheel_page;
    std::shared_ptr<Colors::ColorSet> _solid_colors = std::make_shared<Colors::ColorSet>();
    sigc::connection _solid_color_changed;
    Gtk::Box &_color_list;
    Gtk::Button &_reset;
    Gtk::CheckButton &_live_preview;
    Inkscape::UI::Widget::ColorNotebook *_color_picker_wdgt = nullptr;
    Gtk::ListView *_list_view = nullptr;
    uint32_t _current_color_id;
    bool _is_preview = true;

    Glib::RefPtr<Gio::ListStore<ColorItem>> _color_model;
    Glib::RefPtr<Gtk::SignalListItemFactory> _color_factory;
    Glib::RefPtr<Gtk::SingleSelection> _selection_model;

    ObjectColorSet _manager;

    MultiMarkerColorPlate *_color_wheel = nullptr;
    
    OperationBlocker _blocker;
    OperationBlocker _selection_blocker;

    void generateVisualList();
    void layoutColorPicker(std::shared_ptr<Colors::ColorSet> updated_color = nullptr);
    void colorButtons(Gtk::Box *button, Colors::Color color, bool is_original = false);

    // signal handlers
    void onOriginalColorClicked(uint32_t color_id);
    void onColorPickerChanged(Colors::Color color = Colors::Color{0}, bool wheel = false);
    void onLivePreviewToggled();
    void lpChecked(Colors::Color color = Colors::Color(0), bool wheel = false);
    void setUpTypeBox(Gtk::Box *box, Colors::Color const &color);
    void updateColorModel(std::vector<Colors::Color> const &new_colors = {});
    std::pair<Glib::RefPtr<ColorItem>,guint> findColorItemByKey(uint32_t key);
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_RECOLOR_ART_H
