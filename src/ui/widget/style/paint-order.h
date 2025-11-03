// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Widget for paint order styles
 *//*
 * Copyright (C) 2025 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_UI_WIDGET_PAINT_ORDER_H
#define SEEN_UI_WIDGET_PAINT_ORDER_H

#include <glibmm/i18n.h>

#include "ui/widget/generic/reorderable-stack.h"

#include "style-internal.h"

namespace Inkscape::UI::Widget {

class PaintOrderWidget : public ReorderableStack
{
public:
    PaintOrderWidget()
        : ReorderableStack()
    {
        add_option(_("Marker"), "paint-order-markers", _("Arrows, markers and points"),       SP_CSS_PAINT_ORDER_MARKER);
        add_option(_("Stroke"), "paint-order-stroke",  _("The border line around the shape"), SP_CSS_PAINT_ORDER_STROKE);
        add_option(_("Fill"),   "paint-order-fill",    _("The content of the shape"),         SP_CSS_PAINT_ORDER_FILL);
        set_visible(true);
    }

    void setValue(SPIPaintOrder &po)
    {
        // array to vector
        auto values = po.get_layers();
        std::vector<int> vec = {values[0], values[1], values[2]};
        setValues(vec);
    }

    SPIPaintOrder getValue()
    {
        SPIPaintOrder po;
        auto values = getValues();
        for (auto i = 0; i < 3; i++) {
            po.layer[i] = (SPPaintOrderLayer)values[i];
            po.layer_set[i] = true;
        }
        po.set = true;
        return po;
    }
};

} // namespace Inkscape::UI::Widget

#endif /* !SEEN_UI_WIDGET_PAINT_ORDER_H */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim:filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99:
