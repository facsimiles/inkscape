// SPDX-License-Identifier: GPL-2.0-or-later
//
// Created by Michael Kowalski on 9/3/24.

#include "stroke-options.h"

#include <glib/gi18n.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>

#include "property-utils.h"
#include "style.h"

namespace Inkscape::UI::Widget {

struct Props {
    const char* label;
    sigc::signal<void (const char*)>* signal;
    struct {
        Gtk::ToggleButton* button = nullptr;
        const char* icon;
        const char* style;
        const char* tooltip;
    } buttons[7];
};

StrokeOptions::StrokeOptions() {
    set_column_spacing(4);
    set_row_spacing(8);

    Props properties[] = {
        // TRANSLATORS: The line join style specifies the shape to be used at the
        //  corners of paths. It can be "miter", "round" or "bevel".
        {_("Join"), &_join_changed, {
            {&_join_bevel, "stroke-join-bevel", "bevel", _("Bevel join")},
            {&_join_round, "stroke-join-round", "round", _("Round join")},
            {&_join_miter, "stroke-join-miter", "miter", _("Miter join")},
            {} // <- exit sentry
        }},
        // TRANSLATORS: cap type specifies the shape for the ends of lines
        {_("Cap"), &_cap_changed, {
            {&_cap_butt,   "stroke-cap-butt",   "butt",   _("Butt cap")},
            {&_cap_round,  "stroke-cap-round",  "round",  _("Round cap")},
            {&_cap_square, "stroke-cap-square", "square", _("Square cap")},
            {}
        }},
        // TRANSLATORS: Paint order determines the order the 'fill', 'stroke', and 'markers are painted.
        {_("Order"), &_order_changed, {
            {&_paint_order_fsm, "paint-order-fsm", "normal", _("Fill, Stroke, Markers")},
            {&_paint_order_sfm, "paint-order-sfm", "stroke fill markers", _("Stroke, Fill, Markers")},
            {&_paint_order_fms, "paint-order-fms", "fill markers stroke", _("Fill, Markers, Stroke")},
            {&_paint_order_mfs, "paint-order-mfs", "markers fill stroke", _("Markers, Fill, Stroke")},
            {&_paint_order_smf, "paint-order-smf", "stroke markers fill", _("Stroke, Markers, Fill")},
            {&_paint_order_msf, "paint-order-msf", "markers stroke fill", _("Markers, Stroke, Fill")},
            {}
        }}
    };

    Utils::SpinPropertyDef limit_prop = {&_miter_limit, { 0, 1e5, 0.1, 10, 3 }, nullptr, _("Maximum length of the miter (in units of stroke width)") };
    Utils::init_spin_button(limit_prop);

    int row = 0;
    for (auto& prop : properties) {
        auto& label = *Gtk::make_managed<Gtk::Label>(prop.label);
        label.set_xalign(0);
        attach(label, 0, row);
        auto box = Gtk::make_managed<Gtk::Box>();
        box->set_spacing(4);
        attach(*box, 1, row++);

        auto first = prop.buttons[0].button;
        for (auto btn : prop.buttons) {
            auto button = btn.button;
            if (!button) break;

            if (button != first) button->set_group(*first);
            button->set_icon_name(btn.icon);
            button->set_tooltip_text(btn.tooltip);
            button->signal_toggled().connect([this,btn,prop]() {
                if (_update.pending()) return;

                if (btn.button->get_active()) {
                    prop.signal->emit(btn.style);
                }
            });
            box->append(*button);
        }

        if (first == &_join_bevel) {
            box->append(_miter_limit);
        }
    }

    _miter_limit.signal_value_changed().connect([this](double value) {
        _miter_changed.emit(value);
    });
}

void StrokeOptions::update_widgets(SPStyle& style) {
    if (style.stroke.isNone()) return;

    auto scope(_update.block());

    auto limit = style.stroke_miterlimit.value;
    _miter_limit.set_value(limit);

    auto join = style.stroke_linejoin.value;
    if (join == SP_STROKE_LINEJOIN_BEVEL) {
        _join_bevel.set_active();
        _miter_limit.set_sensitive(false);
    }
    else if (join == SP_STROKE_LINEJOIN_ROUND) {
        _join_round.set_active();
        _miter_limit.set_sensitive(false);
    }
    else {
        _join_miter.set_active();
        _miter_limit.set_sensitive(!style.stroke_extensions.hairline);
    }

    auto cap = style.stroke_linecap.value;
    if (cap == SP_STROKE_LINECAP_SQUARE) {
        _cap_square.set_active();
    }
    else if (cap == SP_STROKE_LINECAP_ROUND) {
        _cap_round.set_active();
    }
    else {
        _cap_butt.set_active();
    }

    SPIPaintOrder order;
    order.read(style.paint_order.set ? style.paint_order.value : "normal");
    if (order.layer[0] != SP_CSS_PAINT_ORDER_NORMAL) {
        if (order.layer[0] == SP_CSS_PAINT_ORDER_FILL) {
            if (order.layer[1] == SP_CSS_PAINT_ORDER_STROKE) {
                _paint_order_fsm.set_active();
            }
            else {
                _paint_order_fms.set_active();
            }
        }
        else if (order.layer[0] == SP_CSS_PAINT_ORDER_STROKE) {
            if (order.layer[1] == SP_CSS_PAINT_ORDER_FILL) {
                _paint_order_sfm.set_active();
            }
            else {
                _paint_order_smf.set_active();
            }
        }
        else {
            if (order.layer[1] == SP_CSS_PAINT_ORDER_STROKE) {
                _paint_order_msf.set_active();
            }
            else {
                _paint_order_mfs.set_active();
            }
        }
    }
    else {
        // "normal" order
        _paint_order_fsm.set_active();
    }
}

} // namespace
