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
#include <2geom/point.h>

#include "ui/tool/elliptical-arc-end-node.h"
#include "ui/tool/node.h"

namespace Inkscape::UI {

EllipticalManipulator::EllipticalManipulator(SPDesktop &desktop, Geom::EllipticalArc const &arc,
                                             NodeSharedData const &data)
    : _arc{arc}
    , _node_shared_data{&data}
{}

void EllipticalManipulator::updateDisplay()
{
    // TODO: implement the center node
}

void EllipticalManipulator::writeSegment(Geom::PathSink &output) const
{
    auto const [ray_x, ray_y] = _arc.rays();
    output.arcTo(ray_x, ray_y, _arc.rotationAngle(), _arc.largeArc(), _arc.sweep(), _arc.finalPoint());
}

void EllipticalManipulator::setVisible(bool visible)
{
    // TODO: implement the center node
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

    // TODO: fix portion() so that dynamic casts are unnecessary
    auto *first_arc = dynamic_cast<Geom::EllipticalArc *>(first_curve.get());
    auto *second_arc = dynamic_cast<Geom::EllipticalArc *>(second_curve.get());

    assert(first_arc && second_arc);

    first_arc->setInitial(_arc.initialPoint());
    first_arc->setFinal(subdivision_point);

    second_arc->setInitial(subdivision_point);
    second_arc->setFinal(_arc.finalPoint());

    _arc = *second_arc;
    updateDisplay();

    return std::make_unique<EllipticalArcEndNode>(*first_arc, *_node_shared_data);
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
