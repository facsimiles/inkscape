// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Kees Cook <kees@outflux.net>
 *
 * Copyright (C) 2007 Kees Cook
 * Copyright (C) 2004 Bryce Harrington
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_RENDERING_OPTIONS_H
#define INKSCAPE_UI_WIDGET_RENDERING_OPTIONS_H

#include "scalar.h"

#include <gtkmm/box.h>
#include <gtkmm/frame.h>
#include <gtkmm/checkbutton.h>

namespace Inkscape::UI::Widget {

/**
 * A container for selecting rendering options.
 */
class RenderingOptions : public Gtk::Box
{
public:

    /**
     * Construct a Rendering Options widget.
     */
    RenderingOptions();

    bool as_bitmap();   // should we render as a bitmap?
    double bitmap_dpi();   // at what DPI should we render the bitmap?

protected:
    // Radio buttons to select desired rendering
    Gtk::Frame       _frame_backends;
    Gtk::CheckButton _radio_vector;
    Gtk::CheckButton _radio_bitmap;

    // Bitmap options
    Gtk::Frame       _frame_bitmap;
    Scalar           _dpi; // DPI of bitmap to render

    // callback for bitmap button
    void _toggled();
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_RENDERING_OPTIONS_H

/* 
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
