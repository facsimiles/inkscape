// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SP_PAINT_SERVER_H
#define SEEN_SP_PAINT_SERVER_H

/*
 * Base class for gradients and patterns
 *
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 1999-2002 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2010 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <memory>
#include <cairo.h>
#include <2geom/rect.h>
#include <sigc++/slot.h>
#include "sp-object.h"

namespace Inkscape {
class Drawing;
class DrawingPattern;
class DrawingPaintServer;
} // namespace Inkscape

class SPPaintServer
    : public SPObject
{
public:
    SPPaintServer();
    ~SPPaintServer() override;
    int tag() const override { return tag_of<decltype(*this)>; }

    bool isSwatch() const;
    virtual bool isValid() const;

    /*
     * There are two ways to implement a paint server:
     *
     *  1. Simple paint servers (solid colors and gradients) implement the create_drawing_paintserver() method.
     *     This returns a DrawingPaintServer instance holding a copy of the paint server's resources which is
     *     used to produce a pattern on-demand using create_pattern().
     *
     *  2. The other paint servers (patterns and hatches) implement set_visible(true), set_visible(false) and setBBox().
     *     The drawing item subtree returned by set_visible(true) is attached as a fill/stroke child of the
     *     drawing item the paint server is applied to, and used directly when rendering.
     *
     *  Paint servers only need to implement one method. If both are implemented, then option 2 is used.
     */

    virtual std::unique_ptr<Inkscape::DrawingPaintServer> create_drawing_paintserver();

    virtual Inkscape::DrawingPattern *show(Inkscape::Drawing &drawing, unsigned key, Geom::OptRect const &bbox);
    virtual void hide(unsigned key);
    virtual void setBBox(unsigned key, Geom::OptRect const &bbox);

protected:
    bool swatch = false;
};

#endif // SEEN_SP_PAINT_SERVER_H
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
