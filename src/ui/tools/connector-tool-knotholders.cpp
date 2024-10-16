// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Node editing extension to connection tool
 *
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ui/tools/connector-tool-knotholders.h"

#include <glibmm/i18n.h>
#include <utility>

#include "ui/widget/events/canvas-event.h"
#include "live_effects/lpe-connector-line.h"
#include "display/curve.h"
#include "object/sp-point.h"
#include "object/sp-root.h"
#include "object/sp-shape.h"
#include "svg/svg.h"

using namespace Inkscape::LivePathEffect;

namespace Inkscape::UI::Tools {

ConnectorObjectKnotHolder::ConnectorObjectKnotHolder(SPDesktop *desktop, SPItem *item)
    : KnotHolder(desktop, item)
{
    // Adds point knotholders, editable this time.
    for (auto &sp_point : item->getConnectionPoints()) {
        ConnectorKnotEditPoint *point = new ConnectorKnotEditPoint(sp_point);
        point->create(desktop, item, this, Inkscape::CANVAS_ITEM_CTRL_TYPE_CONNECT_POINT, "point:edit",
                      _("Connection point in objct, drag to move this point."));
        entity.push_back(point);
    }
}

ConnectorKnotEditPoint::ConnectorKnotEditPoint(SPPoint *sub_point)
    : sub_point(sub_point)
{}

Geom::Point ConnectorKnotEditPoint::knot_get() const
{
    return *sub_point->itemPoint();
}

void ConnectorKnotEditPoint::knot_set(Geom::Point const &raw, Geom::Point const &origin, unsigned int state)
{
    Geom::Point p = snap_knot_position(raw, state);
    sub_point->setItemPoint(&p);
}

bool ConnectorKnotEditPoint::knot_event(Inkscape::CanvasEvent const &event)
{
    if (event.type() == EventType::BUTTON_PRESS) {
        auto &button_event = static_cast<ButtonPressEvent const &>(event);
        if (button_event.button == 1 && button_event.num_press == 2) {
            sub_point->deleteObject();
            tool()->selectionChange();
            return true;
        }
    }
    return false;
}

ConnectorLineKnotHolder::ConnectorLineKnotHolder(SPDesktop *desktop, SPItem *item)
    : KnotHolder(desktop, item)
    , i2dt(desktop->getDocument()->getRoot()->i2dt_affine())
{
    auto lpe = LPEConnectorLine::get(item);
    auto original_pathv = cast<SPShape>(item)->curveForEdit()->get_pathvector();
    auto const &route_pathv = lpe->getRoutePath();

    if (original_pathv.size() == 1) {
        ConnectorKnotEnd *src = new ConnectorKnotEnd(false);
        src->create(desktop, item, this, Inkscape::CANVAS_ITEM_CTRL_TYPE_CONNECTION, "end:src",
                    _("Move the connected line to a new connection."));
        entity.push_back(src);

        ConnectorKnotEnd *dst = new ConnectorKnotEnd(true);
        dst->create(desktop, item, this, Inkscape::CANVAS_ITEM_CTRL_TYPE_CONNECTION, "end:dst",
                    _("Move the connected line to a new connection."));
        entity.push_back(dst);

        if (lpe->advancedEditor()) {
            advanced_line = make_canvasitem<CanvasItemBpath>(desktop->getCanvasSketch());
            advanced_line->set_stroke(0x3465a479); // blue
            advanced_line->set_fill(0x0, SP_WIND_RULE_NONZERO);
            advanced_path = route_pathv[0];
            advanced_start = advanced_path.initialPoint();
            advanced_end = advanced_path.finalPoint();

            // we need to loop through all the segments (line segments) in the target line
            for (int i = 0; i < advanced_path.size(); i++) {
                if (advanced_path[i].length() < 0.01 || !advanced_path[i].isLineSegment()) {
                    continue;
                }

                // Each segment defines a mid point which always sits on that line's middle
                auto mid = new ConnectorKnotMidpoint(i);
                mid->create(desktop, item, this, Inkscape::CANVAS_ITEM_CTRL_TYPE_CONNECT_MIDPOINT, "midpoint",
                            _("Move the mid-point to a new location"), 0x3465a400);
                entity.push_back(mid);
            }
            updateAdvancedLine();
        } else {
            // Raw checkpoint editing, usually of directional lines.
            for (int i = 1; i < original_pathv[0].size(); i++) {
                int dir = LPEConnectorLine::getCheckpointOrientation(&original_pathv[0][i]);
                int dynamic = LPEConnectorLine::getCheckpointDynamic(&original_pathv[0][i-1]);
                auto check = new ConnectorKnotCheckpoint(i, dir, dynamic);
                check->create(desktop, item, this, Inkscape::CANVAS_ITEM_CTRL_TYPE_CONNECT_CHECKPOINT, "checkpoint",
                              _("Move the checkpoint to a new location"), 0xffffff00);
                entity.push_back(check);
            }
        }
    }
}

/**
 * Show a preview to the user of what their edit will do to the line.
 */
void ConnectorLineKnotHolder::updateAdvancedLine()
{
    Geom::PathVector pathv;
    pathv.push_back(getNewRoutePath());

    advanced_line->set_bpath(pathv * i2dt, true);
    advanced_line->set_visible(true);
}

Geom::Path ConnectorLineKnotHolder::getNewRoutePath()
{
    Geom::Path path;
    if (advanced_path.initialPoint() != advanced_start) {
        path = Geom::Path(advanced_start);
        path.appendNew<Geom::LineSegment>(advanced_path.initialPoint());
    }
    for (auto const &i : advanced_path) {
        path.append(i);
    }
    if (path.finalPoint() != advanced_end) {
        path.appendNew<Geom::LineSegment>(advanced_end);
    }
    return path;
}

/**
 * Actually change the line route configuration now.
 */
void ConnectorLineKnotHolder::commitAdvancedLine()
{
    // Remove unneeded elements from the path, healing and so forth.
    Geom::Path route_path = getNewRoutePath();
    Geom::Path path;
    bool last_vert = false;
    for (int i = 0; i < route_path.size(); i++) {
        auto bezier = dynamic_cast<Geom::BezierCurve *>(route_path[i].duplicate());
        bool vert = Geom::are_near(bezier->initialPoint()[Geom::X], bezier->finalPoint()[Geom::X]);
        bool horz = Geom::are_near(bezier->initialPoint()[Geom::Y], bezier->finalPoint()[Geom::Y]);

        // Line has been removed.
        if (horz && vert) {
            continue;
        }

        if (!path.empty()) {
            if (vert == last_vert) {
                // Skip lines that go in the same direction (healing)!
                path.setFinal(bezier->finalPoint());
                continue;
            }
            // Make sure the initial point is linked to the previous.
            bezier->setInitial(path[i-1].finalPoint());
        }

        last_vert = vert;
        path.append(bezier);
    }

    // Build all the checkmarks from all the mind points.
    Geom::PathVector pathv;
    pathv.push_back(Geom::Path(advanced_start));
    for (int i = 1; i < route_path.size() - 1; i++) {
        pathv.back().appendNew<Geom::LineSegment>(
            Geom::middle_point(path[i].initialPoint(), path[i].finalPoint())
        );
    }
    pathv.back().appendNew<Geom::LineSegment>(advanced_end);

    // Re-populate the directionality and dynamics (ineffecient method)
    for (int i = 1; i < pathv[0].size(); i++) {
        auto dir = LPEConnectorLine::detectCheckpointOrientation(route_path, pathv[0].nodes()[i]);

        int dynamic = DynamicNone;
        if (i == 1 || 1 == pathv[0].size() - 1) {
            if (dir & Avoid::ConnDirVert) {
                dynamic += DynamicY;
            }
            if (dir & Avoid::ConnDirHorz) {
                dynamic += DynamicX;
            }
        }
        pathv = LPEConnectorLine::rewriteLine(pathv[0], i, pathv[0].nodes()[i], dir, dynamic);
    }

    if (auto lpe_item = dynamic_cast<SPLPEItem *>(item)) {
        item->setAttribute("inkscape:original-d", sp_svg_write_path(pathv));
        sp_lpe_item_update_patheffect(lpe_item, false, true);
        DocumentUndo::done(desktop->getDocument(), _("Move orthoganal midpoint"), "");
    }

    if (auto tool = dynamic_cast<ConnectorTool *>(desktop->getTool())) {
        tool->selectionChange();
    }
}

ConnectorLineKnotHolder::~ConnectorLineKnotHolder() = default;

ConnectorKnotMidpoint::ConnectorKnotMidpoint(int index)
    : _index(index)
{}

// Always return the mid point of the target line
Geom::Point ConnectorKnotMidpoint::knot_get() const
{
    auto path = holder()->advanced_path;
    return Geom::middle_point(path[_index].initialPoint(), path[_index].finalPoint());
}

Geom::Point ConnectorKnotMidpoint::moveOneAxis(bool vert, Geom::Point origin, Geom::Point raw)
{
    if (vert) {
        return Geom::Point(raw.x(), origin.y());
    }
    return Geom::Point(origin.x(), raw.y());
}

// Update the route line in the parent as we re-draw it
void ConnectorKnotMidpoint::knot_set(Geom::Point const &raw, Geom::Point const &origin, unsigned state)
{
    auto snap_point = snap_knot_position(raw, state);
    auto path = holder()->advanced_path;
    bool vert = path[_index].initialPoint()[Geom::X] == path[_index].finalPoint()[Geom::X];

    Geom::Path new_path;
    for (int i = 0; i < path.size(); i++) {
        auto bezier = dynamic_cast<Geom::BezierCurve *>(path[i].duplicate());
        if (i == _index || i == _index - 1) {
            bezier->setFinal(moveOneAxis(vert, bezier->finalPoint(), snap_point));
        }
        if (i == _index || i == _index + 1) {
            bezier->setInitial(moveOneAxis(vert, bezier->initialPoint(), snap_point));
        }
        new_path.append(bezier);
    }

    holder()->advanced_path = new_path;
    holder()->updateAdvancedLine();
}

// Commit any changes to the midpoint and ALWAYS destroy the knotholder and re-request everything
void ConnectorKnotMidpoint::knot_ungrabbed(Geom::Point const &p, Geom::Point const &origin, unsigned state)
{
    holder()->commitAdvancedLine();
}

ConnectorKnotCheckpoint::ConnectorKnotCheckpoint(int index, int dir, unsigned dynamic)
    : ConnectorKnot()
    , index(index)
    , dir(dir)
    , dynamic(dynamic)
{}

Geom::Point ConnectorKnotCheckpoint::knot_get() const
{
    // Called by update_knot, so we sandwhich this in here.
    auto original_pathv = cast<SPShape>(item)->curveForEdit()->get_pathvector();
    if (dynamic) {
        auto route_pathv = LPEConnectorLine::get(item)->getRoutePath();
        return LPEConnectorLine::getCheckpointPosition(
               &(original_pathv[0][index-1]), &(original_pathv[0][index]),
               route_pathv.initialPoint(), route_pathv.finalPoint());
    }
    return original_pathv[0].nodes()[index];
}

void ConnectorKnotCheckpoint::knot_set(Geom::Point const &raw, Geom::Point const &origin, unsigned state)
{
    auto snap_point = snap_knot_position(raw, state);

    auto prefs = Inkscape::Preferences::get();
    int tolerance = prefs->getInt("/tools/connector/checkpoint/tolerance", 0);

    if (tolerance > 0) {
        // Remove dynamic if we stray far away from the original point.
        if (dynamic & DynamicX && std::abs(snap_point.x() - origin.x()) > tolerance) {
            dynamic -= DynamicX;
        }
        if (dynamic & DynamicY && std::abs(snap_point.y() - origin.y()) > tolerance) {
            dynamic -= DynamicY;
        }
    }
    // OptionalFuture: if we want a more dynamic update we could create a blue
    // routing line in connector-tool and only run this on mouse release.
    // Setting the dynamic here could be interesting. If we move it too far out for example.
    LPEConnectorLine::rewriteLine(cast<SPShape>(item), index, snap_point, dir, dynamic);
    DocumentUndo::done(desktop->getDocument(), _("Move connector checkpoint"), "");
}

bool ConnectorKnotCheckpoint::knot_event(Inkscape::CanvasEvent const &event)
{
    if (event.type() == EventType::BUTTON_PRESS) {
        auto &button_event = static_cast<ButtonPressEvent const &>(event);
        if (button_event.button == 1 && button_event.num_press == 2) {
            // Delete the checkpoint from the connector guide line
            LPEConnectorLine::rewriteLine(cast<SPShape>(item), index, Geom::Point(), 0, DynamicNone, RewriteMode::Delete);
            DocumentUndo::done(desktop->getDocument(), _("Delete connector checkpoint"), "");
            tool()->selectionChange();
            return true;
        }
    }
    return false;
}

void ConnectorKnotCheckpoint::knot_click(unsigned int state)
{
    if (state & GDK_SHIFT_MASK) {
        if (dir == Avoid::ConnDirHorz) {
            dir = Avoid::ConnDirVert;
        } else { // ConnDirVert or Avoid::ConnDirAll
            dir = Avoid::ConnDirHorz;
        }
        // Remove dynamic from point
        dynamic = DynamicNone;
        // Update direction and dynamic
        knot_set(knot_get(), Geom::Point(), GDK_SHIFT_MASK);
    }
}

ConnectorKnotEnd::ConnectorKnotEnd(bool end)
    : is_end(end)
{}

Geom::Point ConnectorKnotEnd::knot_get() const
{
    // Using the route_pathv here will confuse users putting center
    // connections in the center of objects far from their visible lines
    auto pathv = cast<SPShape>(item)->curve()->get_pathvector();
    if (is_end) {
        return pathv.finalPoint();
    }
    return pathv.initialPoint();
}

bool ConnectorKnotEnd::knot_event(Inkscape::CanvasEvent const &event)
{
    // This allows both click and drag re-connecting of lines.
    if (event.type() == EventType::BUTTON_PRESS) {
        auto &button_event = static_cast<ButtonPressEvent const &>(event);
        if (button_event.button == 1 && button_event.num_press == 1) {
            tool()->activateLine(cast<SPShape>(item), is_end);
            return true;
        }
    }
    // Fixed transparent point.
    return tool()->root_handler(event);
}

/**
 * ConnectorPointsKnotHolder is used when the mouse is hovering over an object, loading all
 * the possible locations where the user may like to connect a line.
 */
ConnectorPointsKnotHolder::ConnectorPointsKnotHolder(SPDesktop *desktop, SPItem *item)
    : KnotHolder(desktop, item)
{
    auto tool = dynamic_cast<ConnectorTool *>(desktop->getTool());

    // Add 'center' knot which is always shown as activated unless one of the other nodes is active
    if (item->bbox(Geom::identity(), SPItem::VISUAL_BBOX)) {
        ConnectorKnotCenterPoint *point = new ConnectorKnotCenterPoint();
        point->create(desktop, item, this, Inkscape::CANVAS_ITEM_CTRL_TYPE_CONNECT_ITEM, "point:center",
                      _("Center of the object, connect lines to this object."));
        entity.push_back(point);
    }

    // Add each defined point
    for (auto &sp_point : item->getConnectionPoints()) {
        // Find points that are already selected, so we avoid over-lapping knots
        auto p = tool->selected_points;
        if (std::find(p.begin(), p.end(), sp_point) != p.end()) {
            continue;
        }
        ConnectorKnotSubPoint *point = new ConnectorKnotSubPoint(sp_point);
        point->create(desktop, item, this, Inkscape::CANVAS_ITEM_CTRL_TYPE_CONNECT_POINT, "point:real",
                      _("Connection point in object, connect lines to this point."), 0xffffff00);
        entity.push_back(point);
    }

    for (auto &vt_point : item->getConnectionHints()) {
        // Don't add virtual points for the existing points, avoiding overlapping knots.
        bool exists = false;
        for (auto &sp_point : item->getConnectionPoints()) {
            if (sp_point->getOriginalPointName() == vt_point.first) {
                exists = true;
            }
        }
        if (!exists) {
            ConnectorKnotVirtualPoint *point = new ConnectorKnotVirtualPoint(vt_point.first, vt_point.second);
            point->create(desktop, item, this, Inkscape::CANVAS_ITEM_CTRL_TYPE_CONNECT_HINT, "point:hint",
                          _("Possible connection point in object, connect lines to this point."), 0xffffff88);
            entity.push_back(point);
        }
    }
}

/**
 * This makes these nodes a "hover" item only, but otherwise transparent to the tool events.
 */
bool ConnectorPointKnot::knot_event(Inkscape::CanvasEvent const &event)
{
    if (event.type() != EventType::MOTION) {
        return tool()->root_handler(event);
    }
    return false;
}

/**
 * Center point is the knot which prepr' connecting to the whole object.
 */
Geom::Point ConnectorKnotCenterPoint::knot_get() const
{
    auto bbox = item->bbox(Geom::identity(), SPItem::VISUAL_BBOX);
    g_assert(bbox);
    return bbox->midpoint();
}

void ConnectorKnotCenterPoint::knot_enter(unsigned int state)
{
    tool()->highlightPoint(item);
}

/**
 * A SubPoint is a sp-point which actually exists inside sp-item.
 */
ConnectorKnotSubPoint::ConnectorKnotSubPoint(SPPoint *sub_point)
    : sub_point(sub_point)
{}

Geom::Point ConnectorKnotSubPoint::knot_get() const
{
    return *sub_point->itemPoint();
}

void ConnectorKnotSubPoint::knot_enter(unsigned int state)
{
    tool()->highlightPoint(item, sub_point);
}

/**
 * A virtual point is a hint of where a point *might* like to be created on an object.
 */
ConnectorKnotVirtualPoint::ConnectorKnotVirtualPoint(std::string name, Geom::Point coord)
    : name(std::move(name))
    , coord(coord)
{}

Geom::Point ConnectorKnotVirtualPoint::knot_get() const
{
    return *SPPoint::getItemPoint(item, &coord);
}

void ConnectorKnotVirtualPoint::knot_enter(unsigned state)
{
    tool()->highlightPoint(item, name, coord);
}

} // namespace Inkscape::UI::Tools

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
