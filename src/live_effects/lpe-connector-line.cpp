// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * LPE <connector_line> implementation used by the connector tool
 * to connect two points together using libavoid.
 */

/*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) Synopsys, Inc. 2021
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "live_effects/lpe-connector-line.h"
#include "live_effects/lpeobject.h"

#include <glibmm/i18n.h>

#include <2geom/circle.h>
#include <2geom/elliptical-arc.h>
#include <2geom/path-intersection.h>
#include "display/curve.h"
#include "helper/geom.h"
#include "object/sp-defs.h"
#include "object/sp-item.h"
#include "object/sp-point.h"
#include "object/sp-shape.h"
#include "svg/svg.h"

namespace Inkscape::LivePathEffect {

static bool updating_all;

/**
 * Returns true if item is a connector.
 */
bool isConnector(SPObject const *obj)
{
    auto lpeitem = cast<SPLPEItem>(obj);
    return lpeitem && lpeitem->hasPathEffectOfTypeRecursive(Inkscape::LivePathEffect::CONNECTOR_LINE);
}

/**
 * Gets the LPEConnectorLine for the given SPItem.
 */
LPEConnectorLine *LPEConnectorLine::get(SPItem *item)
{
    if (auto lpeitem = cast<SPLPEItem>(item)) {
        return dynamic_cast<LPEConnectorLine *>(lpeitem->getCurrentLPE());
    }
    return nullptr;
}

/**
 * A checkpoint position is usually the position of it's curve's initial point
 * but for automatically adjusting lines, it can become something else.
 *
 * @param previous - The previous curve with instructions.
 * @param curve - The curve of the checkpoint who's position is the default.
 * @param start - The calculated start of the line.
 * @param end - The calculated end of the line.
 */
Geom::Point LPEConnectorLine::getCheckpointPosition(Geom::Curve const *previous, Geom::Curve const *curve,
                                                    Geom::Point const &start, Geom::Point const &end)
{
    auto point = curve->initialPoint();
    if (auto dynamic = getCheckpointDynamic(previous)) {
        auto mid = Geom::middle_point(start, end);
        if (dynamic & DynamicX) {
            point.x() = mid.x();
        }
        if (dynamic & DynamicY) {
            point.y() = mid.y();
        }
    }
    return point;
}

/**
 * Return the automatic relocation dynamic for a given checkpoint curve.
 *
 * @param previous - The previous curve with instructions.
 */
int LPEConnectorLine::getCheckpointDynamic(Geom::Curve const *previous)
{
    int ret = DynamicNone;
    if (auto bezier = dynamic_cast<Geom::BezierCurve const *>(previous)) {
        if (bezier->size() == 4) {
            Geom::Point adj = bezier->controlPoint(3) - bezier->controlPoint(2);
            if (adj.y() != 0) {
                ret |= DynamicY;
            }
            if (adj.x() != 0) {
                ret |= DynamicX;
            }
        }
    }
    return ret;
}

/**
 * Get's the orientation of this checkpoint based on the bezier curve.
 *
 * @param curve - The curve/point to test.
 *
 * @returns ConnDirAll, ConnDirVert or ConnDirHorz
 */
int LPEConnectorLine::getCheckpointOrientation(Geom::Curve const *curve)
{
    if (auto bezier = dynamic_cast<Geom::BezierCurve const *>(curve)) {
        if (bezier->order() == 3) {
            Geom::Point adj = bezier->controlPoint(0) - bezier->controlPoint(1);
            if (adj.y() != 0) {
                return Avoid::ConnDirVert;
            }
            if (adj.x() != 0) {
                return Avoid::ConnDirHorz;
            }
        }
    }
    return Avoid::ConnDirAll;
}

/**
 * Detect what orientation this point would have if it's on the generated path.
 *
 * @param path - The routed/generated path (not the original one)
 * @param point - The point on the routed path used as a Checkpoint.
 *
 * @returns ConnDirVert or ConnDirHorz, default is ConnDirAll for no detection.
 */
int LPEConnectorLine::detectCheckpointOrientation(Geom::PathVector const &pathv, Geom::Point const &point)
{
    // Get the nearest section to this point.
    if (auto const pathvt = pathv.nearestTime(point)) {
        auto const &curve = pathv[pathvt->path_index].curveAt(pathvt->asPathTime());
        if (curve.isLineSegment()) {
            auto const cmp = curve.initialPoint() - curve.finalPoint();
            if (cmp.x() == 0 && cmp.y() != 0) {
                return Avoid::ConnDirVert;
            }
            if (cmp.y() == 0 && cmp.x() != 0) {
                return Avoid::ConnDirHorz;
            }
        }
    }
    return Avoid::ConnDirAll;
}

/**
 * Set the checkpoint curve to a specific direction
 */
void LPEConnectorLine::setCheckpointOrientation(Geom::BezierCurve &bezier, int dir)
{
    auto const p = bezier.controlPoint(0);
    if (dir == Avoid::ConnDirHorz) {
        bezier.setPoint(1, p + Geom::Point{10, 0});
    } else if (dir == Avoid::ConnDirVert) {
        bezier.setPoint(1, p + Geom::Point{0, 10});
    } else {
        bezier.setPoint(1, p);
    }
}

/**
 * Set the checkpoint dynamic, the automatic repositioning of this point based on the connectors.
 */
void LPEConnectorLine::setCheckpointDynamic(Geom::BezierCurve &bezier, unsigned dynamic)
{
    auto p = bezier.controlPoint(3);
    if (dynamic & DynamicX) {
        p += Geom::Point{10, 0};
    }
    if (dynamic & DynamicY) {
        p += Geom::Point{0, 10};
    }
    bezier.setPoint(2, p);
}

/**
 * Edit the original path and update the LPE routing.
 *
 * @param line - The line SPObject being written to (updates original-d attribute of this item)
 * @param *args - See rewriteLine(path, ...)
 */
void LPEConnectorLine::rewriteLine(SPShape *line, int index, Geom::Point p, int dir, unsigned dynamic, RewriteMode indel)
{
    if (auto lpe_item = cast<SPLPEItem>(line)) {
        auto original_pathv = line->curveForEdit()->get_pathvector();
        auto pathv = LPEConnectorLine::rewriteLine(original_pathv[0], index, p, dir, dynamic, indel);
        line->setAttribute("inkscape:original-d", sp_svg_write_path(pathv));
        sp_lpe_item_update_patheffect(lpe_item, false, true);
    }
}

/**
 * Edit an original path at the given index, inserting or moving the point.
 *
 * @param path - The Path being modified.
 * @param index - The index of the point in the path being modified.
 * @param point - The location the point should be moved to.
 * @param dir - The directionality of the point (NONE, VERT, HORZ, ALL)
 * @param dynamic - The dynamic setting (NONE, X, Y, BOTH)
 * @param indel - Insertion, deletion, or editing (default)
 */
Geom::PathVector LPEConnectorLine::rewriteLine(Geom::Path const &path, int index, Geom::Point const &p, int dir, unsigned dynamic, RewriteMode indel)
{
    Geom::PathVector pathv;
    pathv.push_back(Geom::Path{});
    int j = 0;
    for (int i = 0; i < path.size(); i++) {
        if (auto const bezier_orig = dynamic_cast<Geom::BezierCurve const *>(&path[i])) {
            Geom::BezierCurve *bezier;
            if (bezier_orig->order() == 1) {
                // Elevate lines to cubic bezier.
                bezier = new Geom::CubicBezier(bezier_orig->initialPoint(), bezier_orig->initialPoint(),
                                               bezier_orig->finalPoint(), bezier_orig->finalPoint());
            } else {
                bezier = new Geom::BezierCurve(*bezier_orig);
            }

            // Record previous point attributes.
            if (i + 1 == index) {
                if (indel == RewriteMode::Add) {
                    auto new_bezier = new Geom::CubicBezier(bezier->controlPoint(0), bezier->controlPoint(1), p, p);
                    setCheckpointDynamic(*new_bezier, dynamic);
                    pathv[0].append(new_bezier);
                    j = 1;
                } else if (indel == RewriteMode::Delete && index < path.size()) {
                    if (auto next_bezier = dynamic_cast<Geom::BezierCurve *>(path[i + 1].duplicate())) {
                        bezier->setPoint(3, next_bezier->controlPoint(3));
                        bezier->setPoint(2, next_bezier->controlPoint(2));
                    }
                } else {
                    bezier->setFinal(p);
                    setCheckpointDynamic(*bezier, dynamic);
                }
            }

            // Record next point attributes.
            if (i + j == index) {
                if (indel == RewriteMode::Delete) {
                    continue;
                } else {
                    bezier->setInitial(p);
                    setCheckpointOrientation(*bezier, dir);
                }
            }

            pathv[0].append(bezier);
        }
    }
    return pathv;
}

/**
 * Get's the orientation of this checkpoint based on the bezier curve.
 *
 * @param curve - The curve/point to test.
 *
 * @returns Avoid::ConnDir type (All, Left, Right, Up, Down)
 */
int LPEConnectorLine::getEndpointOrientation(Geom::Curve const *curve, bool is_end)
{
    if (auto bezier = dynamic_cast<Geom::BezierCurve const *>(curve)) {
        if (bezier->size() == 4) {
            // First or second QuadBezier control point depending on end.
            Geom::Point adj = bezier->controlPoint(is_end * 3) - bezier->controlPoint(1 + is_end);
            if (adj[Geom::X] > 0)
                return Avoid::ConnDirRight;
            if (adj[Geom::X] < 0)
                return Avoid::ConnDirLeft;
            if (adj[Geom::Y] > 0)
                return Avoid::ConnDirDown;
            if (adj[Geom::Y] < 0)
                return Avoid::ConnDirUp;
        }
    }
    return Avoid::ConnDirAll;
}

/**
 * Draw a Avoid::ConnRef into a Geom::PathVector object.
 *
 * @param connRef - The connector to draw.
 * @param curvature - The amount of curvature to use
 * @returns PathVector of routed shape
 */
static Geom::PathVector connref_to_pathvector(Avoid::ConnRef &connRef, double curvature)
{
    bool straight = curvature < 1e-3;

    Avoid::PolyLine route = connRef.displayRoute();
    if (!straight) {
        route = route.curvedPolyline(curvature);
    }
    connRef.calcRouteDist();

    if (route.ps.empty()) {
        g_warning("Route didn't generate any points!");
        return {};
    }

    Geom::PathVector pathv;
    pathv.push_back(Geom::Path(Geom::Point(route.ps[0].x, route.ps[0].y)));

    int pn = route.size();
    for (int i = 1; i < pn; ++i) {
        Geom::Point p(route.ps[i].x, route.ps[i].y);
        if (straight) {
            pathv.back().appendNew<Geom::LineSegment>(p);
        } else {
            switch (route.ts[i]) {
                case 'M':
                    // libavoid tells us to move to the same point at a checkpoint, this
                    // splits up our line and makes adjusting it very complicated.
                    // So we're going to ignore requests to split up the curve.
                    // pathv.push_back(Geom::Path(p));
                    break;
                case 'L':
                    pathv.back().appendNew<Geom::LineSegment>(p);
                    break;
                case 'C':
                    assert(i + 2 < pn);
                    Geom::Point p1(route.ps[i + 1].x, route.ps[i + 1].y);
                    Geom::Point p2(route.ps[i + 2].x, route.ps[i + 2].y);
                    pathv.back().appendNew<Geom::CubicBezier>(p, p1, p2);
                    i += 2;
                    break;
            }
        }
    }

    return pathv;
}

/**
 * Get the object adjustment, which is an amount by which the object
 * intersects with the given Path line so it can be shortened to meet
 * the edge of the object.
 *
 * @param line - The line object that this adjustment is transformed into
 * @param path - The path that crosses the edge of the object
 * @param obj - The object that sits at this end of the line
 *
 * @returns A double of how much to cut into the line segment
 */
double LPEConnectorLine::getObjectAdjustment(SPObject *line, Geom::Path const &path, SPItem *item)
{
    if (!item) {
        return 0.0;
    }

    auto outline = item->outline();
    if (outline.empty()) {
        return 0.0;
    }

    auto item_outline = outline * item->getRelativeTransform(line);

    Geom::CrossingSet cross = Geom::crossings(path, item_outline);

    double intersect_pos = 0.0;
    // iterate over all crossings
    for (auto const &cr : cross) {
        for (auto const &cr_pt : cr) {
            intersect_pos = std::max(intersect_pos, cr_pt.ta);
        }
    }
    return intersect_pos;
}

/**
 * Takes a list of time codes on the input path, and returns the gaps given
 * with the radius size
 *
 * @param input - The input path to cut holes into.
 * @param radius - The radius of the gaps to make.
 * @param tas - The time-code based locations in the input path to generate.
 * @returns A vector of LineGap pairs (start and end) timecodes.
 */
LineGaps LPEConnectorLine::calculateGaps(Geom::Path const &input, double radius, std::vector<double> tas)
{
    auto range = input.timeRange();

    // Path times must be in ascending order.
    std::sort(tas.begin(), tas.end());

    std::vector<LineGap> gaps;
    for (double ta : tas) {
        // To cut a circle out, we must now find a circle of the right radius
        // put it at the point in the line we want to cut and then find it's
        // own corssings.
        auto circle = Geom::Path(Geom::Circle(input.pointAt(ta), radius));
        Geom::Crossings cy = Geom::crossings(input, circle);
        Geom::delete_duplicates(cy);
        if (cy.empty()) {
            g_warning("Circle on line does not cross given line, magic.");
            continue;
        }

        // Image you have a line folding over and over, it's possible it could cross the
        // radius many times, but we're interested in the closest crossings to cx[i]
        // This also takes care of single crossings where the radius peeks out of the end
        // of a line allowing it to jump or cut the final section of the line.
        double t1 = range.min();
        double t2 = range.max();
        for (auto const &cross : cy) {
            if (cross.ta < ta && cross.ta > t1) {
                t1 = cross.ta;
            } else if (cross.ta > ta && cross.ta < t2) {
                t2 = cross.ta;
            }
        }

        if (!gaps.empty() && t1 < gaps.back().second) {
            // Found overlapping gap! combine them for a mega gap!
            t1 = gaps.back().first;
            gaps.pop_back();
        }
        gaps.emplace_back(t1, t2);
    }
    return gaps;
}

/**
 * Add jumps to a given Geom Path as it crosses other connector lines.
 *
 * @param line - The line object which the output will be saved.
 * @param input - The line so far generated before being saved.
 * @param type - The type of jump, GAP or ARC.
 * @param size - The size of the jump in document units.
 *
 * @returns the input path with the jumps added.
 */
Geom::PathVector LPEConnectorLine::addLineJumps(SPObject *line, Geom::Path const &input, JumpMode type, double size)
{
    Geom::Crossings cx;
    // Line comes before this one, modify this line with the crossing from the target.
    for (SPObject *obj = line->getPrev(); obj; obj = obj->getPrev()) {
        if (!isConnector(obj)) {
            continue;
        }
        if (auto shape = dynamic_cast<SPShape *>(obj)) {
            auto transform = shape->getRelativeTransform(line);
            auto lpe_curve = *shape->curve();
            lpe_curve.transform(transform);

            Geom::Path path_b = lpe_curve.get_pathvector()[0];
            Geom::Crossings cy = Geom::crossings(input, path_b);
            Geom::merge_crossings(cx, cy, 0);
            // TODO: Remove crossings which merge the lines together
        }
        // TODO: Could handle decendant groups here (somehow)
    }
    // TODO: Add self-crossings, as the line could jump over itself.

    // Remove dupes, and create a sorted list of timecodes
    Geom::delete_duplicates(cx);
    if (cx.empty() || size < 0.01)
        return input;

    std::vector<double> tas;
    for (auto const &cross : cx) {
        tas.push_back(cross.ta);
    }

    Geom::PathVector output;
    output.push_back(Geom::Path{});
    auto radius = size / 2;
    auto range = input.timeRange();
    double prev_pos = range.min();
    for (auto const &jump : calculateGaps(input, radius, tas)) {
        auto end_point = input.pointAt(jump.second);
        input.appendPortionTo(output.back(), prev_pos, jump.first);
        if (type == JumpMode::Gap) {
            if (!output.back().empty()) {
                output.push_back(Geom::Path(end_point));
            }
        } else if (type == JumpMode::Arc) {
            // NOTE: It's possible to control the arc flattness by adding a little to radius here.
            output.back().appendNew<Geom::EllipticalArc>(radius, radius, 0, false, true, end_point);
        } else {
            g_warning("Unknown jump type.");
        }
        prev_pos = jump.second;
    }
    input.appendPortionTo(output.back(), prev_pos, range.max());
    return output;
}

/**
 * Update all the objects next in line who jump over this lpe's line.
 *
 * @param line - The line for who's siblings should be updated.
 */
void LPEConnectorLine::updateSiblings(SPObject *line)
{
    // Line comes after this one, send request to update that line because we might have moved.
    for (SPObject *obj = line->getNext(); obj; obj = obj->getNext()) {
        if (isConnector(dynamic_cast<SPItem *>(obj))) {
            // XXX Check for crossing before calling update?
            sp_lpe_item_update_patheffect(cast<SPLPEItem>(obj), false, true);
            // It's very likely that the next item's update will check
            // all the items in the stack below it, so quit!
            break;
        }
    }
}

/**
 * Update all lines in the document (usually because an avoid object is moved)
 */
void LPEConnectorLine::updateAll(SPDocument *doc)
{
    updating_all = true;
    for (auto &child: doc->getDefs()->children) {
        if (auto lpe_obj = dynamic_cast<LivePathEffectObject *>(&child)) {
            if (auto line_lpe = dynamic_cast<LPEConnectorLine *>(lpe_obj->get_lpe())) {
                for (auto &lpe_item : line_lpe->getCurrrentLPEItems()) {
                    sp_lpe_item_update_patheffect(lpe_item, false, true);
                }
            }
        }
    }
    updating_all = false;
}

/**
 * Get the start or end point from the original path or the linked object.
 *
 * @param curve - The const iterator from the original path at the start or end
 * @param item - The linked item at the same end of the curve (nullptr if none)
 * @param sub_point - The relative location of the sub-point for this end.
 * @param target - The target object that will be modified (for transformation)
 *
 * @returns The visual Avoid::Point for that end corrected for transforms.
 */
Avoid::Point LPEConnectorLine::getConnectorPoint(Geom::Path::const_iterator curve, SPItem *item, Geom::Point *sub_point,
                                                 SPObject *target)
{
    Geom::Point point;
    if (item && target) {
        auto transform = item->getRelativeTransform(target);
        // Get connection points as their own items.
        if (sub_point) {
            point = *SPPoint::getItemPoint(item, sub_point) * transform;
        } else if (auto bbox = item->bbox(transform, SPItem::VISUAL_BBOX)) {
            point = bbox->midpoint();
        }
    } else {
        point = curve->initialPoint();
    }
    return {point.x(), point.y()};
}

/**
 * Get the start or end shape which might be useful for routing.
 *
 * @param item - The linked item at the same end of the curve (nullptr if none)
 * @param target - The target object that will be modified (for transformation)
 *
 * @returns The visual Avoid::Rectangle for that end corrected for transforms.
 */
Avoid::ShapeRef *LPEConnectorLine::getConnectorShape(Avoid::Router *router, Avoid::Point point, SPItem *item,
                                                     SPObject *target, int orientation)
{
    if (!item || !target) {
        return nullptr;
    }

    Avoid::ConnEnd conn = Avoid::ConnEnd(point);
    Avoid::ShapeRef *shape = nullptr;

    SPItem *parent = item;
    auto transform = item->getRelativeTransform(target);
    auto bbox = parent->bbox(transform, SPItem::VISUAL_BBOX);
    if (!bbox) {
        g_warning("Could not get visual box to connected object!");
        return nullptr;
    }

    auto rect =
        Avoid::Rectangle(Avoid::Point(bbox->left(), bbox->top()), Avoid::Point(bbox->right(), bbox->bottom()));

    shape = new Avoid::ShapeRef(router, rect);

    double x = point.x - rect.ps[3].x;
    double y = point.y - rect.ps[3].y;

    if (orientation == Avoid::ConnDirAll) {
        int detected = Avoid::ConnDirNone;
        auto x_s = x / bbox->width();
        auto y_s = y / bbox->height();
        if (x_s > 0.7) {
            detected |= Avoid::ConnDirRight;
        } else if (x_s < 0.3) {
            detected |= Avoid::ConnDirLeft;
        }
        if (y_s > 0.7) {
            detected |= Avoid::ConnDirDown;
        } else if (y_s < 0.3) {
            detected |= Avoid::ConnDirUp;
        }
        if (detected != Avoid::ConnDirNone) {
            orientation = detected;
        }
    }

    new Avoid::ShapeConnectionPin(shape, 1, x, y, false, 0.0, orientation); // owned by shape

    return shape;
}

LPEConnectorLine::LPEConnectorLine(LivePathEffectObject *lpeobject)
    : Effect(lpeobject)
    , connection_start(_("Start object:"), _("Object line is connected from"), "connection-start", &wr, this)
    , connection_end(_("End object:"), _("Object line is connected to"), "connection-end", &wr, this)
    , connector_type(_("Line type:"), _("Determines which line segment type to use."), "line-type", ConnectorType, &wr,
                     this, Avoid::ConnType_Orthogonal)
    , spacing(_("Spacing:"), _("Extra spacing from connectors"), "spacing", &wr, this, 0.0)
    , curvature(_("Curvature:"), _("The amount the route can curve around"), "curvature", &wr, this, 0.0)
    , jump_size(_("Jump size:"), _("The size of the jump when lines cross"), "jump-size", &wr, this, 1.0)
    , jump_type(_("Jump type:"), _("The type of jump made when lines corss."), "jump-type", JumpType, &wr, this, JumpMode::Arc)
    , adjust_for_obj(_("Adjust line to _outline"), _("Moves the line away from a connected object's outside curve."),
                     "adjust-for-obj", &wr, this, true)
    , adjust_for_marker(_("Adjust line to _marker"), _("Moves the line so it's marker touches the target coordinates."),
                        "adjust-for-marker", &wr, this, true)
{
    /* register all your parameters here, so Inkscape knows which parameters this effect has: */
    registerParameter(&connection_start);
    registerParameter(&connection_end);
    registerParameter(&connector_type);
    registerParameter(&spacing);
    registerParameter(&curvature);
    registerParameter(&jump_size);
    registerParameter(&jump_type);
    registerParameter(&adjust_for_obj);
    registerParameter(&adjust_for_marker);
}

void LPEConnectorLine::doAfterEffect(SPLPEItem const *lpe_item, SPCurve *curve)
{
    if (!updating_all) {
        // Make sure the the lines above this one are up to date.
        updateSiblings(const_cast<SPLPEItem *>(lpe_item));
    }
}

Geom::PathVector LPEConnectorLine::doEffect_path(Geom::PathVector const &path_in)
{
    // This is the standard router for this document, it keeps references to all the connectors.
    auto router = getSPDoc()->getRouter();

    // Step 1. Get the locations of the referenced start and end points for the line.
    auto conn_start = getConnStart();
    auto conn_end = getConnEnd();
    auto line = dynamic_cast<SPObject *>(sp_lpe_item);

    // Steps 2, call the static routing process with all the options.
    auto output = generate_path(path_in, router, line, conn_start, conn_end, connector_type, curvature);

    if (output.empty()) {
        // No path generated, likely because it's a zero size
        return output;
    }

    // Cache the pre-jumps path, so the tool can provide route editing.
    route_path = output;

    // Step 3. Make line end-adjustments
    auto range = output[0].timeRange();
    double adjust_start, adjust_end = 0.0;
    if (spacing > 0.01) {
        auto space_gaps = calculateGaps(output[0], spacing, {range.min(), range.max()});
        if (space_gaps.size() == 2) {
            adjust_start += (space_gaps[0].second - range.min());
            adjust_end += (range.max() - space_gaps[1].first);
        }
    }
    if (adjust_for_obj) {
        adjust_start += getObjectAdjustment(sp_lpe_item, output[0], conn_start);
        adjust_end += getObjectAdjustment(sp_lpe_item, output[0].reversed(), conn_end);
    }
    if (adjust_start > 0.0 || adjust_end > 0.0) {
        double offset_start = range.min() + adjust_start;
        double offset_end = range.max() - adjust_end;
        if (offset_start < offset_end) {
            Geom::PathVector adjusted_path;
            adjusted_path.push_back(output[0].portion(offset_start, offset_end));
            output = adjusted_path;
        }
    }

    // Step 4. Add any jumps or gaps to the resulting line
    return addLineJumps(sp_lpe_item, output[0], jump_type, jump_size);
}


/**
 * Generate a routed path based just on the information provided (static version)
 * this version calculates the parentPoints if conn ends are SPPoints.
 *
 * @param path_in - A path to use as the default path (including jogs)
 * @param router - The libavoid router objects (usually from the document)
 * @param line - The target line to transform the points towards
 * @param conn_start - The starting connector point, SPItem or SPPoint.
 * @param conn_end - The ending connector point, SPItem or SPPoint.
 * @param connector_type - Orthoganal or not option type (see libavoid)
 * @param curvature - Amount of router curvature to add
 */
Geom::PathVector LPEConnectorLine::generate_path(Geom::PathVector const &path_in, Avoid::Router *router, SPObject *line,
                                                 SPItem *conn_start, SPItem *conn_end, Avoid::ConnType connector_type,
                                                 double curvature)
{
    auto item_start = conn_start;
    auto point_start = new Geom::Point(0.5, 0.5);
    if (auto sp_point = dynamic_cast<SPPoint *>(conn_start)) {
        item_start = dynamic_cast<SPItem *>(sp_point->parent);
        point_start = sp_point->parentPoint();
    }

    auto item_end = conn_end;
    auto point_end = new Geom::Point(0.5, 0.5);
    if (auto sp_point = dynamic_cast<SPPoint *>(conn_end)) {
        item_end = dynamic_cast<SPItem *>(sp_point->parent);
        point_end = sp_point->parentPoint();
    }

    // Steps 2 & 3, call the static routing process with all the options.
    return generate_path(path_in, router, line, item_start, point_start, item_end, point_end, connector_type,
                         curvature);
}

/**
 * Static version of doEffect_path that can be used outside of the LPE
 *
 * @param path_in - A path to use as the default path (including jogs)
 * @param router - The libavoid router objects (usually from the document)
 * @param target - The target object to transform the points towards
 * @param item_start - Optional starting location object
 * @param point_start - Optional starting sub-point location (relative)
 * @param item_end - Optional ending location object. Shape or SPPoint.
 * @param point_end - Optional ending sub-point location (relative)
 * @param connector_type - Orthoganal or not option type (see libavoid)
 * @param curvature - Amount of router curvature to add
 *
 * @returns new routed path
 */
Geom::PathVector LPEConnectorLine::generate_path(Geom::PathVector const &path_in, Avoid::Router *router,
                                                 SPObject *target, SPItem *item_start, Geom::Point *point_start,
                                                 SPItem *item_end, Geom::Point *point_end,
                                                 Avoid::ConnType connector_type, double curvature)
{
    Geom::PathVector output;
    g_assert(!dynamic_cast<SPPoint *>(item_start));
    g_assert(!dynamic_cast<SPPoint *>(item_end));

    for (const auto &path_it : path_in) {
        // Ignore small node segments
        if (path_it.empty() || count_path_nodes(path_it) < 2) {
            continue;
        }

        // Create a new connection reference using the start and end of the line.
        Geom::Path::const_iterator curve_start = path_it.begin();
        Geom::Path::const_iterator curve_end = path_it.end_default();

        auto src_dir = getEndpointOrientation(&(*curve_start), false);
        auto src_point = getConnectorPoint(curve_start, item_start, point_start, target);
        auto src_shape = getConnectorShape(router, src_point, item_start, target, src_dir);
        Avoid::ConnEnd src = Avoid::ConnEnd(src_point);
        if (src_shape) {
            src = Avoid::ConnEnd(src_shape, 1);
        }

        auto dst_dir = getEndpointOrientation(&(*curve_end), true);
        auto dst_point = getConnectorPoint(curve_end, item_end, point_end, target);
        auto dst_shape = getConnectorShape(router, dst_point, item_end, target, dst_dir);
        Avoid::ConnEnd dst = Avoid::ConnEnd(dst_point);
        if (dst_shape) {
            dst = Avoid::ConnEnd(dst_shape, 1);
        }

        auto _connRef = new Avoid::ConnRef(router, src, dst);
        _connRef->setRoutingType(connector_type);

        auto previous = curve_start;
        ++curve_start;

        // Now we use all in-between points as "checkpoints" for the router.
        std::vector<Avoid::Checkpoint> checkpoints;

        while (curve_start != curve_end) {
            // Only process lines that are at least a little bit far apart from each other.
            if (!Geom::are_near((*curve_start).initialPoint(), (*curve_start).finalPoint())) {
                // This checkpoint code is most basic, it'd be better to use next/previous instead.
                Geom::Point src_checkpoint(src_point[Geom::X], src_point[Geom::Y]);
                Geom::Point dst_checkpoint(dst_point[Geom::X], dst_point[Geom::Y]);
                Geom::Point real_point = getCheckpointPosition(&(*previous), &(*curve_start), src_checkpoint, dst_checkpoint);
                Avoid::Point point_mid(real_point[Geom::X], real_point[Geom::Y]);
                // Checkpoint directionality is controlled by node type
                int dir = getCheckpointOrientation(&(*curve_start));
                checkpoints.emplace_back(point_mid, dir, dir);
            }
            previous = curve_start;
            ++curve_start;
        }

        if (!checkpoints.empty()) {
            _connRef->setRoutingCheckpoints(checkpoints);
        }

        // This does the calculation
        router->processTransaction();
        output = connref_to_pathvector(*_connRef, curvature);
        router->deleteConnector(_connRef);
        if (src_shape) {
            router->deleteShape(src_shape);
        }
        if (dst_shape) {
            router->deleteShape(dst_shape);
        }

        // We ONLY care about the first shape
        break;
    }
    return output;
}

/**
 * Returns the connection start object, if it exists.
 */
SPItem *LPEConnectorLine::getConnStart()
{
    return dynamic_cast<SPItem *>(connection_start.getObject());
}

SPItem *LPEConnectorLine::getConnEnd()
{
    return dynamic_cast<SPItem *>(connection_end.getObject());
}

} // Inkscape::LivePathEffect

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
