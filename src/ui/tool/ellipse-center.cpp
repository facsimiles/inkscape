// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Control point at the center of an elliptical arc in the Node Tool.
 */
/* Authors:
 *   Rafał M. Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ui/tool/ellipse-center.h"

#include <2geom/circle.h>
#include <2geom/point.h>
#include <cmath>
#include <glibmm/i18n.h>
#include <glibmm/ustring.h>
#include <memory>

#include "desktop.h"
#include "display/control/canvas-item-enums.h"
#include "enums.h"
#include "object/sp-namedview.h"
#include "snap.h"
#include "ui/modifiers.h"
#include "ui/tool/control-point.h"
#include "ui/tool/elliptical-manipulator.h"
#include "ui/tool/node.h"
#include "ui/widget/events/canvas-event.h"

namespace {

std::unique_ptr<Geom::EllipticalArc> change_arc_center(Geom::EllipticalArc const &arc, Geom::Point const &center)
{
    using namespace Geom;
    static auto constexpr COS = X;
    static auto constexpr SIN = Y;

    auto const initial_pt = arc.initialPoint();
    auto const final_pt = arc.finalPoint();
    auto const [ray_x, ray_y] = arc.rays();

    auto const [irx, iry] = initial_pt - center;
    auto const [frx, fry] = final_pt - center;

    /** \todo{Rafał}: this matrix could be degenerate if the old center is on the chord.
     * It's not entirely clear to me what the expected behavior is in this situation, but some kind of
     * "continuity" should be imposed: the new arc should be close to the old one.
     */
    Affine const point_matrix{irx, iry, frx, fry, center.x(), center.y()};

    if (are_near(ray_x, ray_y, std::max(ray_x, ray_y) * EPSILON)) {
        // The arc is nearly circular, so we have to "break the symmetry" and make it an eccentric ellipse.
        Ellipse new_ellipse{Circle{{}, 1.0}};
        new_ellipse *= point_matrix;
        return std::unique_ptr<EllipticalArc>{new_ellipse.arc(initial_pt, arc.pointAt(0.5), final_pt)};
    }

    // The ellipse is eccentric, so we try to preserve the eccentric anomalies at endpoints
    auto const initial_eccentric_anomaly = Point::polar(arc.initialAngle());
    auto const middle_eccentric_anomaly = Point::polar(arc.angleAt(0.5));
    auto const final_eccentric_anomaly = Point::polar(arc.finalAngle());

    Affine const transform{initial_eccentric_anomaly[COS],
                           initial_eccentric_anomaly[SIN],
                           final_eccentric_anomaly[COS],
                           final_eccentric_anomaly[SIN],
                           0.0,
                           0.0};

    if (are_near(transform.det(), 0.0)) {
        return {};
    }

    std::unique_ptr<EllipticalArc> new_arc{
        Circle{{}, 1.0}.arc(initial_eccentric_anomaly, middle_eccentric_anomaly, final_eccentric_anomaly)};
    *new_arc *= transform.inverse() * point_matrix;
    return new_arc;
}
} // namespace

namespace Inkscape::UI {

EllipseCenter::EllipseCenter(SPDesktop &desktop, NodeSharedData const &data, Geom::Point const &pos,
                             EllipticalManipulator &manipulator, SPItem const *path)
    : ControlPoint{&desktop, pos, SP_ANCHOR_CENTER, CANVAS_ITEM_CTRL_TYPE_ELLIPSE_CENTER, data.node_group}
    , _manipulator{&manipulator}
    , _path{path}
{}

void EllipseCenter::move(Geom::Point const &pos)
{
    auto const &old_arc = _arc_at_drag_start ? *_arc_at_drag_start : _manipulator->arc();

    if (auto const new_arc = change_arc_center(old_arc, pos)) {
        _manipulator->setArcGeometry(*new_arc);
        ControlPoint::move(new_arc->center());
    }
}

bool EllipseCenter::grabbed(MotionEvent const & /* event */)
{
    _arc_at_drag_start = _manipulator->arc();
    return false;
}

void EllipseCenter::dragged(Geom::Point &new_pos, MotionEvent const &event)
{
    auto const *snap_inhibitor = Modifiers::Modifier::get(Modifiers::Type::MOVE_SNAPPING);
    bool const should_snap = !snap_inhibitor || !snap_inhibitor->active(event.modifiers);
    if (should_snap) {
        auto &snap_manager = _desktop->getNamedView()->snap_manager;
        snap_manager.setup(_desktop, true, _path);
        new_pos = snap_manager.freeSnap(SnapCandidatePoint{new_pos, SNAPSOURCE_NODE_CATEGORY}).getPoint();
        snap_manager.unSetup();
    }

    // TODO: Handle constraint modifier (Ctrl) with constrained snapping (define what it should do).
    move(new_pos);
}

void EllipseCenter::ungrabbed(ButtonReleaseEvent const * /* event */)
{
    _manipulator->commitUndoEvent(COMMIT_MOUSE_MOVE);
}

Glib::ustring EllipseCenter::_getTip(unsigned) const
{
    return _("Arc center: drag to reposition");
}
}

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
