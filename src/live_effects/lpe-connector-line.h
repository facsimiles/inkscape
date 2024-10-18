// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_LPE_CONNECTOR_LINE_H
#define INKSCAPE_LPE_CONNECTOR_LINE_H

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

#include "3rdparty/adaptagrams/libavoid/connector.h"
#include "3rdparty/adaptagrams/libavoid/geomtypes.h"
#include "3rdparty/adaptagrams/libavoid/router.h"
#include "3rdparty/adaptagrams/libavoid/shape.h"
#include "live_effects/effect.h"
#include "live_effects/parameter/enum.h"
#include "live_effects/parameter/item.h"

namespace Avoid {
// Combination directons for checkpoints
inline constexpr int ConnDirHorz = Avoid::ConnDirLeft | Avoid::ConnDirRight;
inline constexpr int ConnDirVert = Avoid::ConnDirUp | Avoid::ConnDirDown;
} // namespace Avoid

namespace Inkscape::LivePathEffect {

using LineGap = std::pair<double, double>;
using LineGaps = std::vector<LineGap>;

inline auto const ConnectorTypeData = std::to_array<Util::EnumData<Avoid::ConnType>>({
    {Avoid::ConnType_None, N_("None"), "none"},
    {Avoid::ConnType_PolyLine, N_("Polyline"), "polyline"},
    {Avoid::ConnType_Orthogonal, N_("Orthogonal"), "orthogonal"},
});
inline auto const ConnectorType = Util::EnumDataConverter<Avoid::ConnType>{ConnectorTypeData.data(), ConnectorTypeData.size()};

enum class RewriteMode
{
    Delete,
    Edit,
    Add,
};

enum DynamicMode : unsigned
{
    DynamicNone = 0,
    DynamicX = 0x1,
    DynamicY = 0x2,
};

enum class JumpMode
{
    Arc,
    Gap,
};

inline auto const JumpTypeData = std::to_array<Util::EnumData<JumpMode>>({
    {JumpMode::Arc, N_("Arc"), "arc"},
    {JumpMode::Gap, N_("Gap"), "gap"},
});
inline auto const JumpType = Util::EnumDataConverter<JumpMode>{JumpTypeData.data(), JumpTypeData.size()};

bool isConnector(SPObject const *obj);

class LPEConnectorLine : public Effect
{
public:
    LPEConnectorLine(LivePathEffectObject *lpeobject);

    void doAfterEffect(SPLPEItem const *lpe_item, SPCurve *curve) override;

    Geom::PathVector doEffect_path(Geom::PathVector const &path_in) override;

    // Static middle part of doEffect_path used in connector-tool.cpp
    static Geom::PathVector generate_path(Geom::PathVector const &path_in, Avoid::Router *router, SPObject *target,
                                          SPItem *conn_start, SPItem *conn_end, Avoid::ConnType connector_type,
                                          double curvature);
    static Geom::PathVector generate_path(Geom::PathVector const &path_in, Avoid::Router *router, SPObject *target,
                                          SPItem *item_start, Geom::Point *point_start, SPItem *item_end,
                                          Geom::Point *point_end, Avoid::ConnType connector_type, double curvature);

    static LPEConnectorLine *get(SPItem *item);
    static Geom::Point getCheckpointPosition(Geom::Curve const *previous, Geom::Curve const *curve, Geom::Point const &start, Geom::Point const &end);
    static int getCheckpointDynamic(Geom::Curve const *previous);
    static int getCheckpointOrientation(Geom::Curve const *curve);
    static int detectCheckpointOrientation(Geom::PathVector const &pathv, Geom::Point const &on_path);
    static void setCheckpointOrientation(Geom::BezierCurve &curve, int dir);
    static void setCheckpointDynamic(Geom::BezierCurve &bezier, unsigned dynamic);
    static int getEndpointOrientation(Geom::Curve const *curve, bool is_end);
    static void rewriteLine(SPShape *item, int index, Geom::Point p, int dir, unsigned dynamic, RewriteMode indel = RewriteMode::Edit);
    static Geom::PathVector rewriteLine(Geom::Path const &path, int index, Geom::Point const &p, int dir, unsigned dynamic, RewriteMode indel = RewriteMode::Edit);
    static void updateAll(SPDocument *doc);

    bool providesOwnKnotholder() const override { return true; };

    SPItem *getConnStart();
    SPItem *getConnEnd();
    Avoid::ConnType getConnType() const { return connector_type; }
    double getCurvature() const { return curvature; }
    double getSpacing() const { return spacing; }
    double getJumpSize() const { return jump_size; }
    JumpMode getJumpType() const { return jump_type; }
    bool advancedEditor() { return connector_type == Avoid::ConnType_Orthogonal && curvature == 0.0; }
    Geom::PathVector const &getRoutePath() const { return route_path; }

private:
    static Avoid::Point getConnectorPoint(Geom::Path::const_iterator curve, SPItem *item, Geom::Point *sub_point, SPObject *target);
    static Avoid::ShapeRef *getConnectorShape(Avoid::Router *router, Avoid::Point point, SPItem *item, SPObject *target,
                                              int orientation = Avoid::ConnDirAll);
    static double getObjectAdjustment(SPObject *line, Geom::Path const &path, SPItem *item);
    static LineGaps calculateGaps(Geom::Path const &input, double radius, std::vector<double> tas);
    static Geom::PathVector addLineJumps(SPObject *line, Geom::Path const &path, JumpMode type, double size);
    static void updateSiblings(SPObject *line);

    ItemParam connection_start;
    ItemParam connection_end;
    EnumParam<Avoid::ConnType> connector_type;
    EnumParam<JumpMode> jump_type;
    ScalarParam jump_size;
    ScalarParam curvature;
    ScalarParam spacing;
    BoolParam adjust_for_obj;
    BoolParam adjust_for_marker;

    Geom::PathVector route_path;
};

} // namespace Inkscape::LivePathEffect

#endif // INKSCAPE_LPE_CONNECTOR_LINE_H

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
