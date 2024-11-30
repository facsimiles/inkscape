// SPDX-License-Identifier: GPL-2.0-or-later
//
// Created by Michael Kowalski on 9/7/24.
//

#include "swatch-editor.h"

#include <glib/gi18n.h>
#include <glibmm/main.h>
#include <gtkmm/filterlistmodel.h>
#include <gtkmm/label.h>
#include <gtkmm/object.h>
#include <gtkmm/signallistitemfactory.h>

#include "color-preview.h"
#include "desktop.h"
#include "document-undo.h"
#include "document.h"
#include "gradient-chemistry.h"
#include "ink-spin-button.h"
#include "preferences.h"
#include "helper/choose-file.h"
#include "object/sp-defs.h"
#include "object/sp-stop.h"
#include "ui/util.h"
#include "ui/dialog/global-palettes.h"
#include "util/variant-visitor.h"

namespace Inkscape::UI::Widget {

namespace {

std::vector<Color> extract_palette_colors(const Dialog::PaletteFileData& palette) {
    std::vector<Color> colors;
    colors.reserve(palette.colors.size());

    for (auto& c : palette.colors) {
        std::visit(VariantVisitor {
            [&](const Color& c) { colors.push_back(c); },
            [](const Dialog::PaletteFileData::SpacerItem&) {},
            [](const Dialog::PaletteFileData::GroupStart&) {}
        }, c);
    }

    return colors;
}

void remove_unused_swatches(SPDocument* doc) {
    if (sp_cleanup_document_swatches(doc) > 0) {
        DocumentUndo::done(doc, _("Removed unused swaches"), "");
    }
}

struct ListItem : public Glib::Object {
    std::string id;
    Glib::ustring label;
    Color color{0};
    bool is_fill = false;
    bool is_stroke = false;

    static Glib::RefPtr<ListItem> create(
        const char* id,
        const Glib::ustring& label,
        const Color& color
    ) {
        auto item = Glib::make_refptr_for_instance<ListItem>(new ListItem());
        item->id = id ? id : "";
        item->label = label;
        item->color = color;
        return item;
    }

    bool operator == (const ListItem& item) const {
        return id == item.id &&
            label == item.label &&
            color == item.color &&
            is_fill == item.is_fill &&
            is_stroke == item.is_stroke;
    }

    ListItem& operator = (const ListItem& src) {
        id = src.id;
        label = src.label;
        color = src.color;
        is_fill = src.is_fill;
        is_stroke = src.is_stroke;
        return *this;
    }

private:
    ListItem() {}
};

Glib::RefPtr<ListItem> to_list_item(SPGradient* swatch) {
    Color color(0x000000ff);
    if (auto stop = swatch->getFirstStop()) {
        color = stop->getColor();
    }
    return ListItem::create(swatch->getId(), swatch->defaultLabel(), color);
}

// max height of the swatch list; if popover gets too high, gtk will forcibly close it
int get_max_gridview_height() {
    static int max_height = 300; // std::max(100, get_monitor_geometry_primary().get_height() - 750);
    return max_height;
}

} // namespace

SwatchEditor::SwatchEditor(Colors::Space::Type space, Glib::ustring prefs_path):
    Box(Gtk::Orientation::VERTICAL),
    _colors(new Colors::ColorSet()),
    _color_picker(ColorPickerPanel::create(space, get_plate_type_preference(prefs_path.c_str(), ColorPickerPanel::None), _colors)),
    _prefs_path(prefs_path)
{
    set_name("SwatchEditor");

    _colors->signal_changed.connect([this]() {
        auto swatch = get_selected_vector();
        if (!swatch) return;

        auto c = _colors->getAverage();
        _signal_color_changed.emit(swatch, c);
    });

    auto grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(4);
    grid->set_column_spacing(8); // 8 to align with color picker sliders
    grid->set_margin_bottom(6); // separate us from color picker
    grid->set_margin_top(4);
    auto col_1 = Gtk::make_managed<Gtk::Box>();
    auto col_3 = Gtk::make_managed<Gtk::Box>();
    _color_picker->get_first_column_size()->add_widget(*col_1);
    _color_picker->get_last_column_size()->add_widget(*col_3);
    _label.set_max_width_chars(20);
    grid->attach(*col_1, 0, 0);
    grid->attach(_label, 1, 0);
    grid->attach(*col_3, 2, 0);
    auto buttons = Gtk::make_managed<Gtk::Box>();
    grid->attach(*buttons, 1, 1);

    buttons->set_spacing(4);
    _new_btn.set_icon_name("plus");
    _new_btn.set_tooltip_text(_("Add new swatch"));
    buttons->append(_new_btn);
    _del_btn.set_icon_name("minus");
    _del_btn.set_tooltip_text(_("Delete current swatch"));
    _del_btn.set_halign(Gtk::Align::START);
    _del_btn.set_hexpand();
    buttons->append(_del_btn);
    _import_btn.set_icon_name("import");
    _import_btn.set_tooltip_text(_("Import color palette into document"));
    _import_btn.set_halign(Gtk::Align::END);
    buttons->append(_import_btn);
    _export_btn.set_icon_name("export");
    _export_btn.set_tooltip_text(_("Export swatches to color palette"));
    buttons->append(_export_btn);
    _clean_btn.set_icon_name("document-cleanup");
    _clean_btn.set_tooltip_text(_("Remove unused swatches"));
    buttons->append(_clean_btn);

    _new_btn.signal_clicked().connect([this]() {
        _signal_changed.emit(nullptr, EditOperation::New, nullptr);
    });
    _del_btn.signal_clicked().connect([this]() {
        auto swatch = get_selected_vector();
        if (sp_can_delete_swatch(swatch)) {
            if (auto replacement = sp_find_replacement_swatch(swatch->document, swatch)) {
                _signal_changed.emit(swatch, EditOperation::Delete, replacement);
            }
        }
    });
    _label.signal_changed().connect([this]() {
        if (_update.pending() || !_document) return;
        // edit swatch label
        if (auto swatch = get_selected_vector()) {
            _signal_label_changed.emit(swatch, _label.get_text());
        }
    });
    _import_btn.signal_clicked().connect([this]() {
        // read color palette and create swatches
        import_swatches();
    });
    _export_btn.signal_clicked().connect([this]() {
        // save document swatches into a color palette
        export_swatches();
    });
    _clean_btn.signal_clicked().connect([this]() {
        // remove all unused swatches from current document
        remove_unused_swatches(_document);
    });

    auto header = Gtk::make_managed<Box>();
    header->set_spacing(4);
    header->set_margin_top(4);
    header->set_margin_bottom(2);
    auto title = Gtk::make_managed<Gtk::Label>(_("Swatch fill"));
    title->set_hexpand();
    title->set_halign(Gtk::Align::START);
    header->append(*title);
    auto menu = Gtk::make_managed<Gtk::MenuButton>();
    menu->set_icon_name("gear");
    menu->set_has_frame(false);
    menu->set_popover(_settings);
    header->append(*menu);
    _search.set_max_width_chars(10);
    _search.signal_search_changed().connect([this]() {
        refilter();
    });
    header->append(_search);

    build_grid();
    build_settings();

    _separator.resize(&_scroll, get_max_gridview_height());
    _separator.get_signal_resized().connect([this](int size) {
        _list_height = size;
        Preferences::get()->setInt(_prefs_path + "/list-height", _list_height);
    });
    _scroll.set_size_request(-1, _list_height);

    append(*header);
    append(_scroll);
    append(_separator);
    append(*grid);
    append(*_color_picker);
}

void SwatchEditor::build_grid() {
    auto factory = Gtk::SignalListItemFactory::create();

    factory->signal_setup().connect([this](const Glib::RefPtr<Gtk::ListItem>& list_item) {
        auto box = Gtk::make_managed<Gtk::Box>();
        box->add_css_class("item-box");
        box->set_orientation(Gtk::Orientation::HORIZONTAL);
        box->set_spacing(4);

        auto color = Gtk::make_managed<ColorPreview>();
        color->set_size_request(_tile_size, _tile_size);
        color->setIndicator(ColorPreview::Swatch);
        color->set_frame(true);
        box->append(*color);

        if (_show_labels) {
            auto label = Gtk::make_managed<Gtk::Label>();
            label->set_hexpand();
            label->set_xalign(0);
            label->set_valign(Gtk::Align::CENTER);
            box->append(*label);
        }

        list_item->set_child(*box);
    });

    factory->signal_bind().connect([this](const Glib::RefPtr<Gtk::ListItem>& list_item) {
        auto obj = list_item->get_item();

        auto& box = dynamic_cast<Gtk::Box&>(*list_item->get_child());
        auto first = box.get_first_child();
        if (!first) throw std::runtime_error("Missing widget in swatch editor factory binding");
        auto& color = dynamic_cast<ColorPreview&>(*first);
        auto label = dynamic_cast<Gtk::Label*>(first->get_next_sibling());

        auto item = std::dynamic_pointer_cast<ListItem>(obj);

        color.set_size_request(_tile_size, _tile_size);
        color.setRgba32(item->color.toRGBA());
        color.set_fill(item->is_fill);
        color.set_stroke(item->is_stroke);
        color.set_tooltip_text(item->color.toString(false));
        if (label) {
            label->set_label(item->label);
        }
    });

    _store = Gio::ListStore<ListItem>::create();
    _filter = Gtk::BoolFilter::create({});
    auto filtered_model = Gtk::FilterListModel::create(_store, _filter);
    _selection_model = Gtk::SingleSelection::create(filtered_model);
    _selection_model->set_autoselect(false);
    _selection_model->signal_selection_changed().connect([this](auto, auto) {
        if (_update.pending() || !_document) return;
        // fire selection changed
        if (auto item = std::dynamic_pointer_cast<ListItem>(_selection_model->get_selected_item())) {
            if (auto swatch = cast<SPGradient>(_document->getObjectById(item->id))) {
                _signal_changed.emit(swatch, EditOperation::Change, nullptr);
            }
        }
    });
    _gridview.add_css_class("grid-view-no-margins");
    _gridview.set_factory(factory);
    _gridview.set_model(_selection_model);
    // max number of tiles horizontally; it impacts amount of virtual items created, so needs to be kept low
    _gridview.set_max_columns(16);
    _scroll.set_child(_gridview);
    _scroll.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    _scroll.set_vexpand();
    _scroll.set_has_frame();
    _scroll.set_margin_top(2);
    _scroll.set_margin_bottom(4);
}

void SwatchEditor::build_settings() {
    int row = 0;
    auto grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_column_spacing(4);

    _show_labels = Preferences::get()->getBool(_prefs_path + "/show-labels", _show_labels);
    auto labels = Gtk::make_managed<Gtk::CheckButton>(C_("Swatch names", "Show labels"));
    labels->set_active(_show_labels);
    labels->set_halign(Gtk::Align::START);
    labels->signal_toggled().connect([=,this]() {
        _show_labels = labels->get_active();
        rebuild();
        Preferences::get()->setBool(_prefs_path + "/show-labels", _show_labels);
    });
    grid->attach(*labels, 0, row++, 2);

    _tile_size = Preferences::get()->getIntLimited(_prefs_path + "/tile-size", _tile_size, 16, 32);
    {
        // tile size scale
        auto label = Gtk::make_managed<Gtk::Label>(_("Tile size"));
        label->set_halign(Gtk::Align::START);
        grid->attach(*label, 0, row);
        auto adj = Gtk::Adjustment::create(_tile_size, 16, 32, 1, 4);
        auto scale = Gtk::make_managed<Gtk::Scale>(adj);
        scale->set_size_request(140);
        scale->set_show_fill_level();
        scale->set_draw_value();
        scale->set_value_pos(Gtk::PositionType::RIGHT);
        scale->set_digits(0);
        adj->signal_value_changed().connect([=,this]() {
            auto size = static_cast<int>(adj->get_value());
            _tile_size = size;
            rebuild();
            Preferences::get()->setInt(_prefs_path + "/tile-size", _tile_size);
        });
        grid->attach(*scale, 1, row++);
    }

    _list_height = Preferences::get()->getIntLimited(_prefs_path + "/list-height", _list_height, 40, get_max_gridview_height());

    _settings.set_child(*grid);
}

void SwatchEditor::set_desktop(SPDesktop* desktop) {
    _desktop = desktop;
    _cur_swatch_id.clear();
}

void SwatchEditor::set_document(SPDocument* document) {
    if (_document == document) return;

    _document = document;
    _rsrc_changed.disconnect();
    _defs_changed.disconnect();

    if (!_document) return;

    _rsrc_changed = _document->connectResourcesChanged("gradient", [this]() {
        schedule_update();
    });

    if (_document->getDefs()) {
        _defs_changed = _document->getDefs()->connectModified([this](SPObject*, unsigned int flags) {
            if (flags & SP_OBJECT_CHILD_MODIFIED_FLAG) {
                schedule_update();
            }
        });
    }

    schedule_update();
}

void SwatchEditor::select_vector(SPGradient* vector) {
    _cur_swatch_id = vector && vector->getId() ? vector->getId() : "";

    Color color(0x000000ff);
    if (vector && vector->hasStops()) {
        color = vector->getFirstStop()->getColor();
    }
    _color_picker->set_color(color);

    if (vector) {
        // update if we are not actually editing the name
        if (!contains_focus(_label)) {
            _label.set_text(vector->defaultLabel());
            _label.set_sensitive();
        }
    }
    else {
        _label.set_text({});
        _label.set_sensitive(false);
    }

    // enable/disable buttons
    _del_btn.set_sensitive(sp_can_delete_swatch(vector));
    _label.set_sensitive(vector != nullptr);
    // todo

    if (vector && vector->getId()) {
        update_selection(vector->getId());
    }
}

void SwatchEditor::update_selection(const std::string& id) {
    int pos = -1;
    if (!id.empty()) {
        auto N = _selection_model->get_n_items();
        for (int i = 0; i < N; ++i) {
            auto item = std::dynamic_pointer_cast<ListItem>(_store->get_object(i));
            if (item->id == id) {
                pos = i;
                break;
            }
        }
    }
    if (pos >= 0) {
        _selection_model->set_selected(pos);
        _gridview.scroll_to(pos);
    }
    else {
        // unselect
        _selection_model->set_selected(GTK_INVALID_LIST_POSITION);
    }
}

SPGradient* SwatchEditor::get_selected_vector() const {
    std::string id;
    if (auto item = std::dynamic_pointer_cast<ListItem>(_selection_model->get_selected_item())) {
        id = item->id;
    }
    else {
        // due to filtering selection can be empty
        id = _cur_swatch_id;
    }

    if (_document && !id.empty()) {
        return cast<SPGradient>(_document->getObjectById(id));
    }

    return nullptr;
}

void SwatchEditor::set_color_picker_plate(ColorPickerPanel::PlateType type) {
    _color_picker->set_plate_type(type);
    set_plate_type_preference(_prefs_path.c_str(), type);
}

ColorPickerPanel::PlateType SwatchEditor::get_color_picker_plate() const {
    return _color_picker->get_plate_type();
}

void SwatchEditor::import_swatches() {
    if (!_document) return;

    auto window = dynamic_cast<Gtk::Window*>(get_root());
    auto path = Dialog::choose_palette_file(window);
    if (path.empty()) return;

    // load colors
    auto res = Dialog::load_palette(path);
    if (res.palette) {
        // import loaded palette
        auto colors = extract_palette_colors(*res.palette);
        if (colors.empty()) return;

        sp_create_document_swatches(_document, colors);
        DocumentUndo::done(_document, _("Import swatches"), "");
    }
    else if (_desktop) {
        _desktop->showNotice(res.error_message);
    }
}

std::string choose_file(Glib::ustring title, Gtk::Window* parent, Glib::ustring mime_type, Glib::ustring file_name) {
    static std::string current_folder;
    return Inkscape::choose_file_save(title, parent, mime_type, _("GIMP color palette"), file_name, current_folder);
}

void SwatchEditor::export_swatches() {
    auto N = _store->get_n_items();
    if (!_document || N == 0) return;

    auto window = dynamic_cast<Gtk::Window*>(get_root());
    //TODO: allow other, more capable formats, to record non-rgb colors
    auto fname = choose_file(_("Export Color Palette"), window, "application/color-palette", "swatch-palette.gpl");
    if (fname.empty()) return;

    std::vector<std::pair<int, std::string>> colors;
    colors.reserve(N);
    for (int i = 0; i < N; ++i) {
        auto item = std::dynamic_pointer_cast<ListItem>(_store->get_object(i));
        // save labels, but not auto generated IDs (starting with '#')
        colors.emplace_back(item->color.toARGB(1.0), item->label.raw()[0] != '#' ? item->label.raw() : std::string());
    }

    save_gimp_palette(fname, colors, _("Inkscape swatch list"));
}

void SwatchEditor::schedule_update() {
    if (_idle_update.connected()) return;

    _idle_update = Glib::signal_idle().connect([this]() {
        update_store();
        _idle_update.disconnect();
        return false;
    });
}

void SwatchEditor::update_store() {
    auto swatches = sp_collect_all_swatches(_document);
    auto scoped(_update.block());

    _store->freeze_notify();

    bool changed = false;
    auto N = _store->get_n_items();
    if (N == swatches.size()) {
        // update in-place
        for (int i = 0; i < N; ++i) {
            auto obj = _store->get_object(i);
            auto item = std::dynamic_pointer_cast<ListItem>(obj);
            auto upd = to_list_item(swatches[i]);
            if (*item != *upd) {
                *item = *upd;
                changed = true;
            }
        }
    }
    else {
        // rebuild
        _store->remove_all();
        changed = true;

        for (auto swatch : swatches) {
            _store->append(to_list_item(swatch));
        }
    }

    _store->thaw_notify();

    if (changed) {
        rebuild();
    }
}

bool SwatchEditor::is_item_visible(const Glib::RefPtr<Glib::ObjectBase>& item) const {
    auto ptr = std::dynamic_pointer_cast<ListItem>(item);
    if (!ptr) return false;

    const auto& swatch = *ptr;

    // filter by name
    auto str = _search.get_text().lowercase();
    if (str.empty()) return true;

    Glib::ustring text = swatch.label;
    return text.lowercase().find(str) != Glib::ustring::npos;
}

void SwatchEditor::refilter() {
    // When a new expression is set in the BoolFilter, it emits signal_changed(),
    // and the FilterListModel re-evaluates the filter.
    auto expression = Gtk::ClosureExpression<bool>::create([this](auto& item){ return is_item_visible(item); });
    // filter results
    _filter->set_expression(expression);
}

void SwatchEditor::rebuild() {
    // remove all
    auto none = Gtk::ClosureExpression<bool>::create([this](auto& item){ return false; });
    _filter->set_expression(none);
    // restore
    refilter();

    // selection gets cleared after refiltering;
    // also selection might have come before we updated our list of swatches;
    // try to select swatch now
    if (!_cur_swatch_id.empty() && !_selection_model->get_selected_item()) {
        update_selection(_cur_swatch_id);
    }
}

void SwatchEditor::set_view_list_mode(bool list) {
    if (_show_labels == list) return;

    _show_labels = list;
    refilter();
}

} // namespace
