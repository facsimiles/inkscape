// SPDX-License-Identifier: GPL-2.0-or-later
//
// Created by Michael Kowalski on 11/1/25.
//

#include "paint-enums.h"
#include "style-internal.h"

namespace Inkscape::UI::Widget {

std::optional<PaintInheritMode> get_inherited_paint_mode(const SPIPaint& paint) {
    if (!paint.isDerived()) {
        return {}; // not derived a derived paint
    }

    switch (paint.paintSource) {
    case SP_CSS_PAINT_ORIGIN_CONTEXT_FILL:
        return PaintInheritMode::ContextFill;
    case SP_CSS_PAINT_ORIGIN_CONTEXT_STROKE:
        return PaintInheritMode::ContextStroke;
    case SP_CSS_PAINT_ORIGIN_CURRENT_COLOR:
        return PaintInheritMode::CurrentColor;
    default:
        // check of 'inherit' keyword and "unset" paint
        if (paint.inheritSource) {
            return PaintInheritMode::Inherit;
        }
        if (!paint.set) {
            return PaintInheritMode::Unset;
        }
    }

    // all other combinations, if any
    g_warning("get_inherited_paint_mode - unrecognized paint combination.");
    return {};
}

std::string get_inherited_paint_css_mode(PaintInheritMode mode) {
    switch (mode) {
    case PaintInheritMode::Unset:
        return {};
    case PaintInheritMode::Inherit:
        return "inherit";
    case PaintInheritMode::ContextFill:
        return "context-fill";
    case PaintInheritMode::ContextStroke:
        return "context-stroke";
    case PaintInheritMode::CurrentColor:
        return "currentColor";
    default:
        g_warning("get_inherited_paint_css_mode(): unknown mode");
    }

    return {};
}

}
