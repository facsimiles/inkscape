// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_RUBBERBAND_H
#define SEEN_RUBBERBAND_H
/*
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Carl Hetherington <inkscape@carlh.net>
 *
 * Copyright (C) 1999-2002 Lauris Kaplinski
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cairomm/pattern.h>
#include <cairomm/refptr.h>
#include <cstdint>
#include <2geom/point.h>
#include <2geom/path.h>
#include <2geom/rect.h>
#include <optional>
#include <vector>
#include "display/control/canvas-item-ptr.h"

/* fixme: do multidocument safe */

class SPCurve;
class SPDesktop;

namespace Inkscape {

class CanvasItemBpath;
class CanvasItemRect;

/**
 * Rubberbanding selector.
 */
class Rubberband
{
public:
    enum class Mode {
        RECT,
        TOUCHPATH,
        TOUCHRECT
    };

    void start(SPDesktop *desktop, Geom::Point const &p, bool tolerance = false);
    void move(Geom::Point const &p);
    Geom::OptRect getRectangle() const;
    void stop();
    bool is_started() { return _started; }
    bool is_moved() { return _moved; }

    Rubberband::Mode getMode() { return _mode; }
    std::vector<Geom::Point> getPoints() const;
    Geom::Path getPath() const;

    consteval static Rubberband::Mode get_default_mode() { return Rubberband::Mode::RECT; };
    void set_mode(Rubberband::Mode mode) { _mode = mode; };
    void set_default_mode() { _mode = get_default_mode(); };

    void resetColor() { _stroke.reset(); }

    static Rubberband* get(SPDesktop *desktop);

private:
    Rubberband(SPDesktop *desktop);
    static Rubberband* _instance;
    
    SPDesktop *_desktop;
    Geom::Point _start;
    Geom::Point _end;
    Geom::Path _path;

    CanvasItemPtr<CanvasItemRect> _rect;
    CanvasItemPtr<CanvasItemBpath> _touchpath;
    SPCurve *_touchpath_curve = nullptr;

    void delete_canvas_items();

    bool _started = false;
    bool _moved = false;
    Rubberband::Mode _mode = get_default_mode();
    double _tolerance = 0.0;

    std::optional<uint32_t> _fill;
    Cairo::RefPtr<Cairo::Pattern> _fill_pattern;
    std::optional<uint32_t> _stroke;
};

} // namespace Inkscape

#endif // SEEN_RUBBERBAND_H

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
