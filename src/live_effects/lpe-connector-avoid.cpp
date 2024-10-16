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

#include "live_effects/lpe-connector-avoid.h"
#include "live_effects/lpe-connector-line.h"

#include <glibmm/i18n.h>

#include <2geom/convex-hull.h>
#include "display/curve.h"
#include "object/sp-item.h"
#include "object/sp-item-group.h"
#include "object/sp-shape.h"

namespace Inkscape::LivePathEffect {

/**
 * Returns true if item is avoided in the router.
 */
bool isAvoided(SPObject const *obj)
{
    auto lpeitem = cast<SPLPEItem>(obj);
    return lpeitem && lpeitem->hasPathEffectOfTypeRecursive(Inkscape::LivePathEffect::CONNECTOR_AVOID);
}

/**
 * Gets the LPEConnectorAvoid for the given SPItem.
 */
LPEConnectorAvoid *LPEConnectorAvoid::get(SPItem *item)
{
    if (auto lpeitem = cast<SPLPEItem>(item)) {
        return dynamic_cast<LPEConnectorAvoid *>(lpeitem->getCurrentLPE());
    }
    return nullptr;
}

/**
 * Get all the points from the given SPCurve.
 *
 * @param curve - The curve to convert
 */
static std::vector<Geom::Point> curvePoints(SPCurve const *curve)
{
    // The number of segments to use for curve approximation
    constexpr int NUM_SEGS = 4;
    // The step size for sampling curves
    constexpr double seg_size = 1.0 / NUM_SEGS;

    std::vector<Geom::Point> result;

    // Iterate over all curves, adding the endpoints for linear curves and
    // sampling the other curves
    for (auto const &path : curve->get_pathvector()) {
        if (path.empty()) {
            continue;
        }
        result.push_back(path.initialPoint());
        for (auto const &curve : path) {
            if (!curve.isLineSegment()) {
                for (int i = 1; i < NUM_SEGS; i++) {
                    result.push_back(curve.pointAt(i * seg_size));
                }
            }
            result.push_back(curve.finalPoint());
        }
    }

    return result;
}

/**
 * Get a list of points based on the item type.
 *
 * @param item - The SPItem, which may be of many types.
 * @param affine - The item transformation.
 */
static std::vector<Geom::Point> itemPoints(SPItem const *item, Geom::Affine const &affine)
{
    std::vector<Geom::Point> output_points;
    SPCurve item_curve;

    auto item_mutable = const_cast<SPItem *>(item);

    if (auto group = cast<SPGroup>(item_mutable)) {
        // consider all first-order children
        for (auto child_item : group->item_list()) {
            auto child_points = itemPoints(child_item, affine * child_item->transform);
            output_points.insert(output_points.end(), child_points.begin(), child_points.end());
        }
    } else if (auto shape = cast<SPShape>(item_mutable)) {
        shape->set_shape();
        item_curve = *shape->curve();
        // apply transformations (up to common ancestor)
        item_curve.transform(affine);
    } else if (auto bbox = item->documentPreferredBounds()) {
        item_curve = SPCurve(*bbox);
    }

    std::vector<Geom::Point> curve_points = curvePoints(&item_curve);
    output_points.insert(output_points.end(), curve_points.begin(), curve_points.end());
    return output_points;
}

/**
 * Turn an SPItem into an libavoid shape which can be routed, avoided etc.
 *
 * @param points - The vector of all points in the shape.
 * @param spacing - The amount of space around the plotted item to pad.
 */
static Avoid::Polygon getPolygon(std::vector<Geom::Point> const &points, double spacing)
{
    // Create convex hull from the points.
    auto const hull = Geom::ConvexHull{points};
    if (hull.isDegenerate()) {
        return {};
    }

    Avoid::Polygon result;
    result.ps.resize(hull.size());

    auto prev_v = (hull.front() - hull.back()).normalized();

    for (int i = 0; i < hull.size(); i++) {
        auto const ip = (i + 1) % hull.size();
        auto const cur_v = (hull[ip] - hull[i]).normalized();
        auto const det = Geom::cross(cur_v, prev_v);
        auto const pt = hull[i] + (spacing / det) * (cur_v - prev_v);
        result.ps[i] = {pt.x(), pt.y()};
        prev_v = cur_v;
    }

    return result;
}

/**
 * Creates or destroys the avoid LPE on the item item.
 *
 * @returns true if the avoided status was changed.
 */
bool LPEConnectorAvoid::toggleAvoid(SPItem *item, bool enable)
{
    if (enable && !isAvoided(item)) {
        // Use a simple avoid lpe (there are no options).
        Inkscape::XML::Node *lpe_repr = nullptr;
        auto obj = item->document->getObjectById("standard_avoid");
        if (obj) {
            lpe_repr = obj->getRepr();
        }
        if (!lpe_repr) {
            // Create a new connector avoid effect
            lpe_repr = LivePathEffect::Effect::createEffect("connector_avoid", item->document);
            lpe_repr->setAttribute("id", "standard_avoid");
        }
        LivePathEffect::Effect::applyEffect(lpe_repr, item);
        return true;
    }
    if (!enable && isAvoided(item)) {
        if (auto lpe_item = cast<SPLPEItem>(item)) {
            lpe_item->removeCurrentPathEffect(false);
        }
        return true;
    }
    return false;
}

LPEConnectorAvoid::LPEConnectorAvoid(LivePathEffectObject *lpeobject)
    : Effect(lpeobject)
{}

LPEConnectorAvoid::~LPEConnectorAvoid() = default;

void LPEConnectorAvoid::doAfterEffect(SPLPEItem const *lpe_item, SPCurve *curve)
{
    // Remove any references to the past shape.
    removeRef(lpe_item);

    std::vector<Geom::Point> points;
    if (curve) {
        // Shapes, normal LPE items.
        points = curvePoints(curve);
    } else {
        // Groups, images, etc.
        points = itemPoints(lpe_item, lpe_item->i2doc_affine());
    }

    auto router = lpe_item->document->getRouter();
    Avoid::Polygon poly = getPolygon(points, 0.5);
    if (!poly.empty()) {
        addRef(lpe_item, new Avoid::ShapeRef(router, poly));
        router->processTransaction();
    }

    // Make sure the the lines above this one are up to date.
    LPEConnectorLine::updateAll(lpe_item->document);
}

Geom::PathVector LPEConnectorAvoid::doEffect_path(Geom::PathVector const &path_in)
{
    return path_in;
}

void LPEConnectorAvoid::doOnRemove(SPLPEItem const *lpeitem)
{
    removeRef(lpeitem);
}

void LPEConnectorAvoid::addRef(SPItem const *item, Avoid::ShapeRef *avoid_ref)
{
    avoid_refs[item] = avoid_ref;
}

void LPEConnectorAvoid::removeRef(SPItem const *item)
{
    auto router = getSPDoc()->getRouter();
    auto search = avoid_refs.find(item);
    if (search != avoid_refs.end()) {
        router->deleteShape(search->second);
        router->processTransaction();
        avoid_refs.erase(search);
    }
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
