// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Handles for the manipulation of elliptical arcs.
 */
/* Authors:
 *   Rafał M. Siejakowski <rs@rs-math.net> - Implementation
 *   Adam Belis <adam.belis@gmail.com> - Design and definition
 *
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ui/tool/elliptical-manipulator.h"

#include <2geom/elliptical-arc.h>
#include <2geom/line.h>
#include <2geom/path-sink.h>
#include <2geom/pathvector.h>
#include <2geom/point.h>

#include "display/control/canvas-item-bpath.h"
#include "display/control/canvas-item-enums.h"
#include "display/control/canvas-item-ptr.h"
#include "object/sp-item.h"
#include "ui/tool/elliptical-arc-end-node.h"
#include "ui/tool/multi-path-manipulator.h"
#include "ui/tool/node.h"
#include "ui/tool/path-manipulator.h"

namespace {

/// Given the arc of an ellipse, return the other arc making up the ellipse.
Geom::PathVector arc_complement(Geom::EllipticalArc const &arc)
{
    Geom::PathVector result;
    Geom::PathBuilder builder{result};

    auto const [rx, ry] = arc.rays();
    builder.moveTo(arc.finalPoint());
    builder.arcTo(rx, ry, arc.rotationAngle(), !arc.largeArc(), arc.sweep(), arc.initialPoint());
    builder.flush();
    return result;
}

double constexpr CONTOUR_WIDTH = 2.0;
double constexpr DASH_LENGTH = 2.0;
} // namespace

namespace Inkscape::UI {

EllipticalManipulator::EllipticalManipulator(SPDesktop &desktop, Geom::EllipticalArc const &arc,
                                             NodeSharedData const &data, PathManipulator &parent)
    : _arc{arc}
    , _node_shared_data{&data}
    , _contour{make_canvasitem<CanvasItemBpath>(data.handle_line_group)}
    , _parent{&parent}
{
    _contour->set_bpath(arc_complement(_arc));
    _contour->set_name("CanvasItemBPath:EllipseContour");
    _contour->set_stroke(CANVAS_ITEM_PRIMARY);
    _contour->lower_to_bottom();
    _contour->set_pickable(false);
    _contour->CanvasItem::set_fill(0U);
    _contour->set_stroke_width(CONTOUR_WIDTH);
    _contour->set_dashes({DASH_LENGTH, DASH_LENGTH});
}

void EllipticalManipulator::updateDisplay()
{
    // TODO: implement the arc controller
    // _center_node.setPosition(_arc.center());
    _contour->set_bpath(arc_complement(_arc));
    _parent->update();
}

void EllipticalManipulator::commitUndoEvent(CommitEvent event_type) const
{
    _parent->mpm().commit(event_type);
}

void EllipticalManipulator::writeSegment(Geom::PathSink &output) const
{
    auto const [ray_x, ray_y] = _arc.rays();
    output.arcTo(ray_x, ray_y, _arc.rotationAngle(), _arc.largeArc(), _arc.sweep(), _arc.finalPoint());
}

void EllipticalManipulator::setVisible(bool visible)
{
    // TODO: implement the halo node
    _contour->set_visible(visible);
}

void EllipticalManipulator::setArcGeometry(Geom::EllipticalArc const &new_arc)
{
    auto const old_initial_point = _arc.initialPoint();
    auto const old_final_point = _arc.finalPoint();

    _arc = new_arc;
    _arc.setInitial(old_initial_point);
    _arc.setFinal(old_final_point);

    updateDisplay();
}

void EllipticalManipulator::setArcFinalPoint(Geom::Point const &new_point)
{
    auto const old_initial_point = _arc.initialPoint();
    auto const old_chord_line = Geom::Line(_arc.chord());
    auto const new_chord_line = Geom::Line(old_initial_point, new_point);

    _arc.transform(old_chord_line.transformTo(new_chord_line));
    _arc.setInitial(old_initial_point);
    _arc.setFinal(new_point);

    updateDisplay();
}

void EllipticalManipulator::setArcInitialPoint(Geom::Point const &new_point)
{
    auto const old_final_point = _arc.finalPoint();
    auto const old_chord_line = Geom::Line(_arc.chord());
    auto const new_chord_line = Geom::Line(new_point, old_final_point);

    _arc.transform(old_chord_line.transformTo(new_chord_line));
    _arc.setInitial(new_point);
    _arc.setFinal(old_final_point);

    updateDisplay();
}

std::unique_ptr<Node> EllipticalManipulator::subdivideArc(double subdivision_time)
{
    double const t = std::clamp(subdivision_time, 0.0, 1.0);
    auto const subdivision_point = _arc.pointAt(t);

    std::unique_ptr<Geom::Curve> first_curve{_arc.portion(0.0, t)};
    std::unique_ptr<Geom::Curve> second_curve{_arc.portion(t, 1.0)};

    // TODO: fix portion() so that the dynamic cast is unnecessary
    auto *second_arc = dynamic_cast<Geom::EllipticalArc *>(second_curve.get());

    assert(second_arc);

    first_curve->setInitial(_arc.initialPoint());
    first_curve->setFinal(subdivision_point);

    second_arc->setInitial(subdivision_point);
    second_arc->setFinal(_arc.finalPoint());

    _arc = *second_arc;
    updateDisplay();

    return _parent->createNodeFactory().createArcEndpointNode(*first_curve);
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
