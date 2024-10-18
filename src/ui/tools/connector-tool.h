// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_UI_TOOLS_CONNECTOR_TOOL_H
#define INKSCAPE_UI_TOOLS_CONNECTOR_TOOL_H

/*
 * Connector creation tool
 *
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2021 Martin Owens
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <sigc++/connection.h>

#include "display/control/canvas-item-bpath.h"
#include "display/control/canvas-item-ptr.h"
#include "display/control/canvas-item-rect.h"
#include "helper/auto-connection.h"
#include "live_effects/lpe-connector-line.h"
#include "object/sp-point.h"
#include "object/sp-shape.h"
#include "selection.h"
#include "ui/tools/tool-base.h"
#include "ui/toolbar/connector-toolbar.h"

using Inkscape::LivePathEffect::LPEConnectorLine;

namespace Inkscape::UI::Tools {

class ConnectorObjectKnotHolder;
class ConnectorPointsKnotHolder;
class ConnectorLineKnotHolder;

enum ToolModes
{
    CONNECTOR_LINE_MODE,
    CONNECTOR_POINT_MODE,
};

class ConnectorTool : public ToolBase
{
public:
    ConnectorTool(SPDesktop *desktop);
    ~ConnectorTool() override;

    static LPEConnectorLine *getLPE(SPShape *line);

    void set(Preferences::Entry const &val) override;
    bool root_handler(CanvasEvent const &event) override;

    void setToolMode(int mode);

    // Public and called from knot-holder-entity.
    void setHighlightArea(CanvasItemPtr<CanvasItemRect> &rect, SPItem *item, Geom::Point *point = nullptr);

    void highlightPoint(SPItem *item, SPPoint *point = nullptr);
    void highlightPoint(SPItem *item, std::string name, Geom::Point parent_point);

    void unhighlightPoint();

    bool activateHoverPoint();
    void activatePoint(SPItem *item, SPPoint *point = nullptr);
    void activatePoint(SPItem *item, std::string name, Geom::Point parent_point);
    void activateLine(SPShape *line, bool is_end);

    std::vector<SPPoint *> selected_points;

    int tool_mode;

    bool addCheckpoint(SPShape *line, Geom::Point point);
    void selectionChange();

private:
    SPItem *findShapeAtPoint(Geom::Point last_post, bool ignore_lines = false);
    SPItem *findShapeNearPoint(Geom::Point dt_point, int distance = 3.0);

    Geom::PathVector drawSimpleLine(Geom::Point drawn_end);
    void moveDrawnLine(Geom::Point *point, Geom::Point *hover_sub_point = nullptr);
    void moveReconnectLine(Geom::Point *point, Geom::Point *hover_sub_point = nullptr);
    void finishDrawnLine(SPItem *end_item, SPPoint *end_point = nullptr);
    void finishReconnectLine(SPItem *end_item, SPPoint *end_point = nullptr);
    void deactivateLineDrawing();

    void selectionClear();
    void selectionChanged(Inkscape::Selection *selection);
    void selectObject(SPObject *object);

    auto_connection sel_changed_connection;

    // Toolbar settings
    Toolbar::ConnectorToolbar *toolbar = nullptr;
    int steps = 0;
    double spacing = 0.0;
    double curvature = 0.0;
    double jump_size = 0.0;
    Inkscape::LivePathEffect::JumpMode jump_type = Inkscape::LivePathEffect::JumpMode::Arc;
    Avoid::ConnType conn_type = Avoid::ConnType_PolyLine;

    // This holds knots of points when a user hovers over an item
    ConnectorPointsKnotHolder *hover_knots = nullptr;
    ConnectorObjectKnotHolder *point_editing_holder = nullptr;
    CanvasItemPtr<CanvasItemRect> point_editing_rect;
    // A knot holder for each selected line, or line connected to selected objects
    std::vector<ConnectorLineKnotHolder *> selected_line_holders;
    std::vector<SPShape *> selected_lines;
    // Active means that the user has clicked on the item and is drawing a line
    SPItem *active_item = nullptr;
    SPItem *hover_item = nullptr;
    SPPoint *active_point = nullptr;
    SPPoint *hover_point = nullptr;
    SPShape *modify_line = nullptr;
    bool modify_end = false;
    // The hint will be turned into an SPPoint once the line is drawn.
    std::string active_hint_name = "";
    Geom::Point *active_hint_point = nullptr;
    std::string hover_hint_name = "";
    Geom::Point *hover_hint_point = nullptr;
    // Store the "drawn-line" as the last calculated route
    Geom::PathVector last_route;

    // Highlighting and indication of connection process
    CanvasItemPtr<CanvasItemRect> highlight_a;
    CanvasItemPtr<CanvasItemRect> highlight_b;
    CanvasItemPtr<CanvasItemBpath> drawn_line;
    std::optional<Geom::Point> drawn_start;
};

} // namespace Inkscape::UI::Tools

#endif // INKSCAPE_UI_TOOLS_CONNECTOR_TOOL_H

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
