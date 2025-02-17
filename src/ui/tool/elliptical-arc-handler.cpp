// SPDX-License-Identifier: GPL-2.0-or-later
/* Authors:
 *   Rafał M. Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ui/tool/elliptical-arc-handler.h"

#include <2geom/ellipse.h>
#include <2geom/elliptical-arc.h>
#include <2geom/line.h>
#include <2geom/transforms.h>
#include <algorithm>
#include <glib/gi18n.h>
#include <numeric>

#include "ui/modifiers.h"
#include "ui/tool/elliptical-arc-end-node.h"
#include "ui/tool/elliptical-manipulator.h"
#include "ui/tool/node.h"
#include "ui/widget/events/canvas-event.h"

namespace {

/** Given three collinear points, construct the smallest degenerate arc with the given initial and final points
 *  containing the specified third point.
 *  The last argunment arc_center_hint is used as a loose suggestion for where the arc's enter should be placed.
 */
Geom::EllipticalArc compute_degenerate_arc(Geom::Point const &initial_point, Geom::Point const &final_point,
                                           Geom::Point const &point_on_arc, Geom::Point const &arc_center_hint)
{
    Geom::Line const line{initial_point, final_point};
    auto const arc_center = line.isDegenerate() ? initial_point : line.pointAt(line.nearestTime(arc_center_hint));

    double const ray =
        std::max(Geom::distance(arc_center, point_on_arc),
                 std::max(Geom::distance(arc_center, initial_point), Geom::distance(arc_center, final_point)));

    Geom::Ellipse degenerate_ellipse{arc_center, {ray, 0.0}, line.angle()}; // TODO const-correct Geom::Ellipse::arc()

    // This is needed only because lib2geom insists on heap allocation
    std::unique_ptr<Geom::EllipticalArc> result{degenerate_ellipse.arc(initial_point, point_on_arc, final_point)};
    return *result;
}

Geom::Line perpendicular_bisector(Geom::Point const &a, Geom::Point const &b)
{
    return Geom::Line::from_origin_and_vector(Geom::middle_point(a, b), (a - b).cw());
}

Geom::Point triple_intersection(Geom::Line const &a, Geom::Line const &b, Geom::Line const &c)
{
    auto intersections = a.intersect(b);
    auto const ac = c.intersect(a);
    auto const bc = b.intersect(c);

    intersections.insert(intersections.end(), ac.begin(), ac.end());
    intersections.insert(intersections.end(), bc.begin(), bc.end());
    if (intersections.empty()) {
        return {};
    }

    return std::accumulate(
        intersections.begin(), intersections.end(), Geom::Point{},
        [coefficient = 1.0 / static_cast<double>(intersections.size())](auto const &accumulated, auto const &incoming) {
            return accumulated + coefficient * incoming.point();
        });
}

/**
 * @brief Find an ellipse with the specified aspect ratio and rotation angle passing through three given points.
 *
 * @pre Points a, b, c do not lie on the same straight line.
 *
 * @param aspect_ratio A point whose ratio of coordinates expresses the desired ratio of ellipse's rays.
 * @pre Both of the coordinates of aspect_ratio are strictly positive.
 *
 * @param rotation     The angle of the rotation taking the X axis to the axis of aspect_ratio.x().
 * @return An ellipse passing through a, b, c whose rotation angle and aspect ratio match the input.
 */
Geom::Ellipse fit_ellipse_to_three_points(Geom::Point const &a, Geom::Point const &b, Geom::Point const &c,
                                          Geom::Point const &aspect_ratio, Geom::Angle rotation)
{
    auto const level = Geom::Rotate(-rotation);
    auto const circularize = Geom::Scale(aspect_ratio.y(), aspect_ratio.x());
    auto const transform = level * circularize;

    auto const p = a * transform;
    auto const q = b * transform;
    auto const r = c * transform;

    auto const center_transformed =
        triple_intersection(perpendicular_bisector(p, q), perpendicular_bisector(q, r), perpendicular_bisector(r, p));
    auto const radius_transformed = Geom::distance(center_transformed, p) / 3.0 +
                                    Geom::distance(center_transformed, q) / 3.0 +
                                    Geom::distance(center_transformed, r) / 3.0;
    auto const rays = Geom::Point{radius_transformed, radius_transformed} * circularize.inverse();

    return {center_transformed * transform.inverse(), rays, rotation};
}

bool is_modifier_active(Inkscape::Modifiers::Type type, Inkscape::CanvasEvent const &event)
{
    auto const *modifier = Inkscape::Modifiers::Modifier::get(type);
    return modifier && modifier->active(event.modifiers);
}
} // namespace

namespace Inkscape::UI {

EllipticalArcHandler::EllipticalArcHandler(EllipticalManipulator &manipulator)
    : _manipulator{&manipulator}
{}

bool EllipticalArcHandler::pointGrabbed(NodeList::iterator curve_start, NodeList::iterator curve_end)
{
    _rays_at_drag_start = _manipulator->arc().rays();
    return false;
}

bool EllipticalArcHandler::pointDragged(NodeList::iterator curve_start, NodeList::iterator curve_end, double curve_time,
                                        Geom::Point const &drag_origin, Geom::Point const &drag_destination,
                                        MotionEvent const &event)
{
    auto const &arc = _manipulator->arc();
    auto const initial_point = arc.initialPoint();
    auto const final_point = arc.finalPoint();

    if (Geom::are_collinear(initial_point, drag_destination, final_point)) {
        // The endpoints of the arc lie on the same straight line as the drag point; the arc will be degenerate.
        _manipulator->setArcGeometry(
            compute_degenerate_arc(initial_point, final_point, drag_destination, arc.center()));
        return true;
    }

    // The points are not on the same line, so we need a real arc. However, the original arc might have become
    // degenerate in case the user is in the midst of dragging the point from one side of the chord to the other.
    // So we make an arc of an ellipse with the same aspect ratio as the original ellipse at the start of the drag,
    // unless that ellipse itself was degenerate, in which case we create a circle.
    bool const was_degenerate = _rays_at_drag_start.x() < Geom::EPSILON || _rays_at_drag_start.y() < Geom::EPSILON;
    auto const &reference_rays = was_degenerate ? Geom::Point{1.0, 1.0} : _rays_at_drag_start;

    Geom::Angle arc_rotation = arc.rotationAngle();
    if (!is_modifier_active(Modifiers::Type::MOVE_CONFINE, event)) {
        auto const center = arc.center();
        arc_rotation += Geom::angle_between(drag_origin - center, drag_destination - center);
        // TODO: the expression (drag_destination - center) does not take into account the changed center
    }

    auto fitting_ellipse =
        fit_ellipse_to_three_points(initial_point, drag_destination, final_point, reference_rays, arc_rotation);
    std::unique_ptr<Geom::EllipticalArc> new_arc{fitting_ellipse.arc(initial_point, drag_destination, final_point)};

    _manipulator->setArcGeometry(*new_arc);
    return true;
}

Glib::ustring EllipticalArcHandler::getTooltip(unsigned event_state, NodeList::iterator /*curve_start*/)
{
    return C_("Path segment tip", "<b>Elliptical arc</b>: drag to shape the arc, doubleclick to insert node");
}
} // namespace Inkscape::UI

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
