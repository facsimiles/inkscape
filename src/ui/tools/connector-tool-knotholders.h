// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Inkscape::UI::Tools::
 * This is a container class which contains a knotholder for connector shapes.
 *
 *//*
 * Authors:
 *   Martin Owens <doctormo@gmail.com>
 *
 * Copyright (C) 2021 Martin Owens
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_CONNECTOR_TOOL_KNOTHOLDERS_H
#define SEEN_CONNECTOR_TOOL_KNOTHOLDERS_H

#include "desktop.h"
#include "live_effects/lpe-connector-line.h"
#include "object/sp-item.h"
#include "ui/knot/knot-holder-entity.h"
#include "ui/knot/knot-holder.h"
#include "ui/tools/connector-tool.h"
#include "ui/tools/tool-base.h"

using Inkscape::LivePathEffect::LPEConnectorLine;

namespace Inkscape {
class CanvasEvent;

namespace UI {
namespace Tools {

class ConnectorKnot : public KnotHolderEntity
{
public:
    ConnectorTool *tool() { return dynamic_cast<ConnectorTool *>(desktop->getTool()); };
};

class ConnectorObjectKnotHolder : public KnotHolder
{
public:
    ConnectorObjectKnotHolder(SPDesktop *desktop, SPItem *item);
};

class ConnectorKnotEditPoint : public ConnectorKnot
{
public:
    ConnectorKnotEditPoint(SPPoint *sub_point);
    Geom::Point knot_get() const override;
    void knot_set(Geom::Point const &p, Geom::Point const &origin, unsigned state) override;
    void knot_ungrabbed(Geom::Point const &p, Geom::Point const &origin, unsigned state) override{};
    bool knot_event(CanvasEvent const &event) override;

private:
    SPPoint *sub_point;
};

class ConnectorLineKnotHolder : public KnotHolder
{
public:
    ConnectorLineKnotHolder(SPDesktop *desktop, SPItem *item);
    ~ConnectorLineKnotHolder() override;

    Geom::Path getNewRoutePath();
    void updateAdvancedLine();
    void commitAdvancedLine();

    Geom::Path advanced_path;
private:
    Geom::Affine i2dt;
    Geom::Point advanced_start;
    Geom::Point advanced_end;
    CanvasItemPtr<CanvasItemBpath> advanced_line;
};

class ConnectorKnotMidpoint : public ConnectorKnot
{
public:
    ConnectorKnotMidpoint(int index);
    Geom::Point knot_get() const override;
    void knot_set(Geom::Point const &p, Geom::Point const &origin, unsigned state) override;
    void knot_ungrabbed(Geom::Point const &p, Geom::Point const &origin, unsigned state) override;
    void knot_click(guint state) override {};

private:
    Geom::Point moveOneAxis(bool vert, Geom::Point origin, Geom::Point raw);
    ConnectorLineKnotHolder *holder() const { return dynamic_cast<ConnectorLineKnotHolder *>(parent_holder); };
    int _index;
};

class ConnectorKnotCheckpoint : public ConnectorKnot
{
public:
    ConnectorKnotCheckpoint(int index, int dir, unsigned dynamic);
    Geom::Point knot_get() const override;
    void knot_set(Geom::Point const &p, Geom::Point const &origin, unsigned state) override;
    void knot_ungrabbed(Geom::Point const &p, Geom::Point const &origin, unsigned state) override{};
    void knot_click(guint state) override;
    bool knot_event(CanvasEvent const &event) override;

private:
    int index;
    int dir;
    unsigned dynamic;
};

class ConnectorKnotEnd : public ConnectorKnot
{
public:
    ConnectorKnotEnd(bool end);
    Geom::Point knot_get() const override;
    void knot_set(Geom::Point const &p, Geom::Point const &origin, unsigned state) override{};
    void knot_ungrabbed(Geom::Point const &p, Geom::Point const &origin, unsigned state) override{};
    bool knot_event(CanvasEvent const &event) override;

private:
    bool is_end;
};

class ConnectorPointsKnotHolder : public KnotHolder
{
public:
    ConnectorPointsKnotHolder(SPDesktop *desktop, SPItem *item);
};

class ConnectorPointKnot : public ConnectorKnot
{
public:
    void knot_set(Geom::Point const &p, Geom::Point const &origin, unsigned state) override{};
    void knot_ungrabbed(Geom::Point const &p, Geom::Point const &origin, unsigned state) override{};
    bool knot_event(CanvasEvent const &event) override;
};

class ConnectorKnotCenterPoint : public ConnectorPointKnot
{
public:
    Geom::Point knot_get() const override;
    void knot_enter(unsigned state) override;
};

class ConnectorKnotSubPoint : public ConnectorPointKnot
{
public:
    ConnectorKnotSubPoint(SPPoint *sub_point);
    Geom::Point knot_get() const override;
    void knot_enter(unsigned state) override;

private:
    SPPoint *sub_point;
};

class ConnectorKnotVirtualPoint : public ConnectorPointKnot
{
public:
    ConnectorKnotVirtualPoint(std::string point_name, Geom::Point coord);
    Geom::Point knot_get() const override;
    void knot_enter(unsigned state) override;
    std::string const &getName() const { return name; };

private:
    std::string name;
    Geom::Point coord;
};

} // namespace Tools
} // namespace UI
} // namespace Inkscape

#endif // SEEN_CONNECTOR_TOOL_KNOTHOLDERS_H

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
