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
#include "ui/tool/multi-path-manipulator.h"
#include "ui/tool/node.h"
#include "ui/tool/path-manipulator.h"

namespace Inkscape::UI {

EllipticalManipulator::EllipticalManipulator(SPDesktop &desktop, Geom::EllipticalArc const &arc,
                                             NodeSharedData const &data, SPItem const *path, PathManipulator &parent)
    : _arc{arc}
    , _center_node{desktop, data, arc.center(), *this, path}
    , _node_shared_data{&data}
    , _path{path}
    , _parent{&parent}
{}

void EllipticalManipulator::update()
{
    _center_node.setPosition(_arc.center());
    _parent->update();
}

void EllipticalManipulator::commitUndoEvent(CommitEvent event_type)
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
    _center_node.setVisible(visible);
}

void EllipticalManipulator::setArcGeometry(Geom::EllipticalArc const &new_arc)
{
    auto const old_initial_point = _arc.initialPoint();
    auto const old_final_point = _arc.finalPoint();

    _arc = new_arc;
    _arc.setEndpoints(old_initial_point, old_final_point);

    update();
}

void EllipticalManipulator::setArcFinalPoint(Geom::Point const &new_point)
{
    auto const old_initial_point = _arc.initialPoint();
    auto const old_chord_line = Geom::Line(_arc.chord());
    auto const new_chord_line = Geom::Line(old_initial_point, new_point);

    _arc.transform(old_chord_line.transformTo(new_chord_line));
    _arc.setEndpoints(old_initial_point, new_point);

    update();
}

void EllipticalManipulator::setArcInitialPoint(Geom::Point const &new_point)
{
    auto const old_final_point = _arc.finalPoint();
    auto const old_chord_line = Geom::Line(_arc.chord());
    auto const new_chord_line = Geom::Line(new_point, old_final_point);

    _arc.transform(old_chord_line.transformTo(new_chord_line));
    _arc.setEndpoints(new_point, old_final_point);

    update();
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

    first_arc->setEndpoints(_arc.initialPoint(), subdivision_point);
    second_arc->setEndpoints(subdivision_point, _arc.finalPoint());

    _arc = *second_arc;
    update();

    return std::make_unique<EllipticalArcEndNode>(*first_arc, *_node_shared_data, _path, *_parent);
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
