// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_HELPER_PATH_STROKE_H
#define INKSCAPE_HELPER_PATH_STROKE_H

/* Authors:
 *   Liam P. White
 *   Tavmjong Bah
 *
 * Copyright (C) 2014-2015 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <vector>
#include <functional>
#include <2geom/path.h>
#include <2geom/pathvector.h>

#include "livarot/LivarotDefs.h"

namespace Inkscape {

enum LineJoinType {
    JOIN_BEVEL,
    JOIN_ROUND,
    JOIN_MITER,
    JOIN_MITER_CLIP,
    JOIN_EXTRAPOLATE,
    JOIN_EXTRAPOLATE1,
    JOIN_EXTRAPOLATE2,
    JOIN_EXTRAPOLATE3,
};

enum LineCapType {
    BUTT_FLAT,
    BUTT_ROUND,
    BUTT_SQUARE,
    BUTT_PEAK, // This is not a line ending supported by the SVG standard.
};

/**
 * Strokes the path given by @a input.
 * Joins may behave oddly if the width is negative.
 *
 * @param[in] input Input path.
 * @param[in] width Stroke width.
 * @param[in] miter Miter limit. Only used when @a join is one of JOIN_MITER, JOIN_MITER_CLIP, and JOIN_EXTRAPOLATE.
 * @param[in] join  Line join type used during offset. Member of LineJoinType enum.
 * @param[in] cap   Line cap type used during stroking. Member of LineCapType enum.
 * @param[in] tolerance Tolerance, values smaller than 0 lead to automatic tolerance depending on width.
 *
 * @return Stroked path.
 *         If the input path is closed, the resultant vector will contain two paths.
 *         Otherwise, there should be only one in the output.
 */
Geom::PathVector outline(
        Geom::Path const& input,
        double width,
        double miter,
        LineJoinType join = JOIN_BEVEL,
        LineCapType cap = BUTT_FLAT,
        double tolerance = -1);

/**
 * Offset the input path by @a width.
 * Joins may behave oddly if the width is negative.
 *
 * @param[in] input Input path.
 * @param[in] width Amount to offset.
 * @param[in] miter Miter limit. Only used when @a join is one of JOIN_MITER, JOIN_MITER_CLIP, and JOIN_EXTRAPOLATE.
 * @param[in] join  Line join type used during offset. Member of LineJoinType enum.
 * @param[in] tolerance Tolerance, values smaller than 0 lead to automatic tolerance depending on width.
 *
 * @return Offsetted output.
 */
Geom::Path half_outline(
        Geom::Path const& input,
        double width,
        double miter,
        LineJoinType join = JOIN_BEVEL,
        double tolerance = -1);

/**
 * Builds a join on the provided path.
 * Joins may behave oddly if the width is negative.
 *
 * @param[inout] res      The path to build the join on. 
 *                        The outgoing path (or a portion thereof) will be appended after the join is created.
 *                        Previous segments may be modified as an optimization, beware!
 *
 * @param[in]    outgoing The segment to append on the outgoing portion of the join.
 * @param[in]    in_tang  The end tangent to consider on the input path.
 * @param[in]    out_tang The begin tangent to consider on the output path.
 * @param[in]    width
 * @param[in]    miter
 * @param[in]    join
 */
void outline_join(Geom::Path &res, Geom::Path const& outgoing, Geom::Point in_tang, Geom::Point out_tang, double width, double miter, LineJoinType join);

/**
 * Return the list of connected components of a graph described by an adjacency-test function.
 * \param size The number of nodes in the graph. (Nodes are labelled from 0 to size - 1.)
 * \param adj_test The adjacency-test function.
 */
std::vector<std::vector<int>> connected_components(int size, std::function<bool(int, int)> const &adj_test);

/**
 * Return true if the given path has close to zero area.
 */
bool is_path_empty(const Geom::Path &path);

/**
 * Split a collection of paths into connected components.
 * Two paths are viewed as connected if they overlap.
 */
std::vector<Geom::PathVector> split_non_intersecting_paths(Geom::PathVector &&paths, bool remove_empty = false);

/**
 * Create a user spected offset from a pathvector
 */
Geom::PathVector 
do_offset(Geom::PathVector const & path_in
        , double to_offset
        , double tolerance
        , double miter_limit
        , FillRule fillrule
        , Inkscape::LineJoinType join
        , Geom::Point point // knot on LPE
        , Geom::PathVector &helper_path
        , Geom::PathVector &mix_pathv_all);

Geom::PathVector 
do_offset(Geom::PathVector const & path_in
        , double to_offset
        , double tolerance
        , double miter_limit
        , FillRule fillrule
        , Inkscape::LineJoinType join);


} // namespace Inkscape

#endif // INKSCAPE_HELPER_PATH_STROKE_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8 :
