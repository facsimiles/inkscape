// SPDX-License-Identifier: GPL-2.0-or-later
//
// Created by Michael Kowalski on 12/12/25.
//

#include "transform-panel.h"

#include <glibmm/i18n.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/stack.h>

#include "document-undo.h"
#include "ink-property-grid.h"
#include "preferences.h"
#include "selection.h"
#include "generic/spin-button.h"
#include "generic/tab-strip.h"
#include "ui/builder-utils.h"
#include "util/transform-objects.h"

namespace Inkscape::UI::Widget {

constexpr int PageTransforms = 0;
constexpr int PageMatrix = 1;

TransformPanel::TransformPanel() :
    _builder(create_builder("transform-panel.ui")),
    _matrix_a(get_widget<InkSpinButton>(_builder, "matrix-a")),
    _matrix_b(get_widget<InkSpinButton>(_builder, "matrix-b")),
    _matrix_c(get_widget<InkSpinButton>(_builder, "matrix-c")),
    _matrix_d(get_widget<InkSpinButton>(_builder, "matrix-d")),
    _matrix_e(get_widget<InkSpinButton>(_builder, "matrix-e")),
    _matrix_f(get_widget<InkSpinButton>(_builder, "matrix-f")),
    _move_x(get_widget<InkSpinButton>(_builder, "move-x")),
    _move_y(get_widget<InkSpinButton>(_builder, "move-y")),
    _scale_x(get_widget<InkSpinButton>(_builder, "scale-x")),
    _scale_y(get_widget<InkSpinButton>(_builder, "scale-y")),
    _skew_x(get_widget<InkSpinButton>(_builder, "skew-x")),
    _skew_y(get_widget<InkSpinButton>(_builder, "skew-y")),
    _rotate(get_widget<InkSpinButton>(_builder, "rotate")),
    _relative_move(get_widget<Gtk::CheckButton>(_builder, "relative-move")),
    _current_matrix(get_widget<Gtk::CheckButton>(_builder, "current-matrix")),
    _replace_matrix(get_widget<Gtk::Stack>(_builder, "replace-matrix")),
    _separate_transform(get_widget<Gtk::CheckButton>(_builder, "obj-separately")),
    _link_scale_btn(get_widget<Gtk::Button>(_builder, "link-scale")),
    _apply_btn(get_widget<Gtk::Button>(_builder, "apply")),
    _duplicate_btn(get_widget<Gtk::Button>(_builder, "duplicate"))
{
}

TransformPanel::~TransformPanel() {
    // unparent checkbox child widgets manually; gtk is not going to do it automatically
    gtk_widget_unparent(GTK_WIDGET(_replace_matrix.gobj()));
    gtk_widget_unparent(GTK_WIDGET(get_widget<Gtk::Label>(_builder, "label-1").gobj()));
    gtk_widget_unparent(GTK_WIDGET(get_widget<Gtk::Label>(_builder, "label-2").gobj()));
}

void TransformPanel::add_to_grid(InkPropertyGrid& grid) {
    auto tabs = Gtk::make_managed<TabStrip>();
    _tab_strip = tabs;
    tabs->add_css_class("no-indent");
    tabs->set_hexpand();
    tabs->set_stretch_tabs(true);
    tabs->set_show_labels(TabStrip::ShowLabels::Never);
    tabs->set_show_close_button(false);
    tabs->set_show_plus_button(false);
    tabs->set_rearranging_tabs(TabStrip::Rearrange::Never);
    _tab_transform = tabs->add_tab(_("Move, scale, rotate, skew"), "dialog-transform");
    _tab_matrix = tabs->add_tab(_("Transformation matrix"), "matrix");
    tabs->signal_select_tab().connect([this](auto& tab) {
        auto page = &tab == _tab_transform ? PageTransforms : PageMatrix;
        set_page(page);
        Preferences::get()->setInt(_cur_page_pref.observed_path, page);
    });
    auto& box = get_widget<Gtk::Box>(_builder, "tab-box");
    box.prepend(*tabs);
    _tabs = grid.add_row("", &box, Gtk::make_managed<Gtk::Box>());

    //TEMP: until spinbutton MR is merged
    get_widget<InkSpinButton>(_builder, "matrix-a").set_min_size("12");
    get_widget<InkSpinButton>(_builder, "matrix-b").set_min_size("12");
    get_widget<InkSpinButton>(_builder, "matrix-c").set_min_size("12");
    get_widget<InkSpinButton>(_builder, "matrix-d").set_min_size("12");
    get_widget<InkSpinButton>(_builder, "matrix-e").set_min_size("12");
    get_widget<InkSpinButton>(_builder, "matrix-f").set_min_size("12");

    _transform_page = reparent_properties(get_widget<Gtk::Grid>(_builder, "grid-transform"), grid);
    _matrix_page = grid.add_row(&get_widget<Gtk::Grid>(_builder, "grid-matrix"), Gtk::make_managed<Gtk::Box>());
    _footer = reparent_properties(get_widget<Gtk::Grid>(_builder, "grid-footer"), grid);

    _current_matrix.set_active(_replace_matrix_pref);
    _current_matrix.signal_toggled().connect([this] {
        if (!_desktop) return;

        auto replace = _current_matrix.get_active();
        Preferences::get()->setBool(_replace_matrix_pref.observed_path, replace);
        if (replace) {
            update_ui(*_desktop->getSelection());
        }
        else {
            clear_matrix();
        }
    });
    _replace_matrix_pref.action = [this] {
        _current_matrix.set_active(_replace_matrix_pref);
    };

    get_widget<Gtk::Button>(_builder, "reset").signal_clicked().connect([this]{ reset_to_defaults(); });
    _apply_btn.signal_clicked().connect([this]{ on_apply(false); });
    _duplicate_btn.signal_clicked().connect([this]{ on_apply(true); });

    _scale_x.signal_value_changed().connect([this](auto value) {
        if (_linked_scale_pref) {
            _scale_y.set_value(value);
        }
    });
    _scale_y.signal_value_changed().connect([this](auto value) {
        if (_linked_scale_pref) {
            _scale_x.set_value(value);
        }
    });
    auto set_link_icon = [this] {
        _link_scale_btn.set_icon_name(_linked_scale_pref ? "entries-linked" : "entries-unlinked");
    };
    _link_scale_btn.signal_clicked().connect([this, set_link_icon] {
        bool link = !_linked_scale_pref;
        Preferences::get()->setBool(_linked_scale_pref.observed_path, link);
        if (link) {
            _scale_y.set_value(_scale_x.get_value());
        }
        set_link_icon();
    });
    _linked_scale_pref.action = [set_link_icon] {
        set_link_icon();
    };
    set_link_icon();

    reset_to_defaults();
    set_page(_cur_page_pref);
    _cur_page_pref.action = [this]{ set_page(_cur_page_pref); };
}

void TransformPanel::set_visible(bool show) {
    _tabs.set_visible(show);
    _footer.set_visible(show);

    if (show) {
        show_page(_cur_page_pref);
    }
    else {
        // hide all
        _transform_page.set_visible(false);
        _matrix_page.set_visible(false);
    }
}

void TransformPanel::set_page(int page) {
    show_page(page);
}

void TransformPanel::show_page(int page) {
    if (page == PageTransforms) {
        _matrix_page.set_visible(false);
        _matrix_page.set_sensitive(false);
        _transform_page.set_sensitive(true);
        _transform_page.set_visible(true);
    }
    else {
        _transform_page.set_visible(false);
        _transform_page.set_sensitive(false);
        _matrix_page.set_sensitive(true);
        _matrix_page.set_visible(true);
    }
    _tab_strip->select_tab(page == 0 ? *_tab_transform : *_tab_matrix);
}

void TransformPanel::reset_to_defaults() {
    _current_matrix.set_active(false);
    clear_matrix();

    _move_x.set_value(0);
    _move_y.set_value(0);
    _relative_move.set_active();
    _rotate.set_value(0);
    _scale_x.set_value(100);
    _scale_y.set_value(100);
    _skew_x.set_value(0);
    _skew_y.set_value(0);
}

void TransformPanel::set_desktop(SPDesktop* desktop) {
    _desktop = desktop;
}

void TransformPanel::update_ui(Selection& selection) {
    auto enable = !selection.isEmpty();

    if (enable && _current_matrix.get_active()) {
        // update matrix
        auto& matrix = selection.items().front()->transform;
        _matrix_a.set_value(matrix[0]);
        _matrix_b.set_value(matrix[1]);
        _matrix_c.set_value(matrix[2]);
        _matrix_d.set_value(matrix[3]);
        _matrix_e.set_value(matrix[4]);
        _matrix_f.set_value(matrix[5]);
    }

    // change check box label
    _replace_matrix.set_visible_child(selection.size() > 1 ? "multi" : "single");

    // "transform objects separately" only applies to multiple selection
    _separate_transform.set_visible(selection.size() > 1);

    _apply_btn.set_sensitive(enable);
    _duplicate_btn.set_sensitive(enable);
}

void TransformPanel::on_apply(bool duplicate) {
    if (!_desktop) return;

    auto selection = _desktop->getSelection();
    if (!selection || selection->isEmpty()) return;

    // is matrix page visible?
    if (_cur_page_pref == PageMatrix) {
        // get matrix values first; they can change when we duplicate selection
        auto a = _matrix_a.get_value();
        auto b = _matrix_b.get_value();
        auto c = _matrix_c.get_value();
        auto d = _matrix_d.get_value();
        auto e = _matrix_e.get_value();
        auto f = _matrix_f.get_value();
        Geom::Affine matrix(a, b, c, d, e, f);

        if (duplicate) {
            selection->duplicate();
        }

        bool replace = _current_matrix.get_active();
        transform_apply_matrix(selection, matrix, replace);
        DocumentUndo::done(_desktop->getDocument(), duplicate ? _("Duplicate selection and edit transformation matrix") : _("Edit transformation matrix"), "dialog-transform");
        return;
    }

    if (duplicate) {
        selection->duplicate();
    }

    if (_cur_page_pref == PageTransforms) {
        bool apply_separately = _separate_transform.get_active();
        bool changed = false;

        bool relative = _relative_move.get_active();
        auto mx = _move_x.get_value();
        auto my = _move_y.get_value();
        if (!relative || mx != 0 || my != 0) {
            transform_move(selection, mx, my, relative, apply_separately, _desktop->yaxisdir());
            changed = true;
        }

        auto angle = _rotate.get_value();
        if (angle != 0) {
            transform_rotate(selection, angle, apply_separately);
            changed = true;
        }

        auto sx = _scale_x.get_value();
        auto sy = _scale_y.get_value();
        if (sx != 100 || sy != 100) {
            bool transform_stroke = Preferences::get()->getBool("/options/transform/stroke", true);
            bool preserve = Preferences::get()->getBool("/options/preservetransform/value", false);
            transform_scale(selection, sx, sy, true, apply_separately, transform_stroke, preserve);
            changed = true;
        }

        auto hx = _skew_x.get_value();
        auto hy = _skew_y.get_value();
        if (hx != 0 || hy != 0) {
            transform_skew(selection, hx, hy, SkewUnits::Absolute, apply_separately, _desktop->yaxisdir());
            changed = true;
        }

        if (changed) {
            DocumentUndo::done(_desktop->getDocument(), duplicate ? _("Duplicate and transform selection") : _("Transform selection"), "dialog-transform");
        }
    }
}

void TransformPanel::clear_matrix() {
    _matrix_a.set_value(1);
    _matrix_b.set_value(0);
    _matrix_c.set_value(0);
    _matrix_d.set_value(1);
    _matrix_e.set_value(0);
    _matrix_f.set_value(0);
}

} // namespaces
