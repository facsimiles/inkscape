// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape SPGrid implementation
 *
 * Authors:
 * James Ferrarelli
 * Johan Engelen <johan@shouraizou.nl>
 * Lauris Kaplinski
 * Abhishek Sharma
 * Jon A. Cruz <jon@joncruz.org>
 * Tavmong Bah <tavmjong@free.fr>
 * see git history
 *
 * Copyright (C) 2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "sp-grid.h"
#include "sp-namedview.h"

#include "display/control/canvas-item-grid.h"

#include "attributes.h"
#include "desktop.h"
#include "document.h"
#include "grid-snapper.h"
#include "page-manager.h"
#include "snapper.h"
#include "svg/svg-color.h"
#include "util/units.h"

#include <glibmm/i18n.h>
#include <string>
#include <optional>

SPGrid::SPGrid()
    : _visible(true)
    , _enabled(true)
    , _dotted(false)
    , _snap_to_visible_only(true)
    , _legacy(false)
    , _pixel(true)
    , _major_color(GRID_DEFAULT_MAJOR_COLOR)
    , _minor_color(GRID_DEFAULT_MINOR_COLOR)
    , _grid_type(GridType::RECTANGULAR)
    , _major_opacity(0.38)
    , _minor_opacity(0.15)
    , _major_line_interval(5)
{
    _angle_x.unset(SVGAngle::Unit::DEG, 30, 30);
    _angle_z.unset(SVGAngle::Unit::DEG, 30, 30);
}

SPGrid::~SPGrid() = default;

void SPGrid::build(SPDocument *doc, Inkscape::XML::Node *repr)
{
    SPObject::build(doc, repr);

    readAttr(SPAttr::TYPE);
    readAttr(SPAttr::UNITS);
    readAttr(SPAttr::ORIGINX);
    readAttr(SPAttr::ORIGINY);
    readAttr(SPAttr::SPACINGX);
    readAttr(SPAttr::SPACINGY);
    readAttr(SPAttr::ANGLE_X);
    readAttr(SPAttr::ANGLE_Z);
    readAttr(SPAttr::COLOR);
    readAttr(SPAttr::EMPCOLOR);
    readAttr(SPAttr::VISIBLE);
    readAttr(SPAttr::ENABLED);
    readAttr(SPAttr::OPACITY);
    readAttr(SPAttr::EMPOPACITY);
    readAttr(SPAttr::MAJOR_LINE_INTERVAL);
    readAttr(SPAttr::DOTTED);
    readAttr(SPAttr::SNAP_TO_VISIBLE_ONLY);

    _checkOldGrid(doc, repr);

    _page_selected_connection = document->getPageManager().connectPageSelected([=](void *) { update(nullptr, 0); });
    _page_modified_connection = document->getPageManager().connectPageModified([=](void *) { update(nullptr, 0); });

    doc->addResource("grid", this);
}

void SPGrid::release()
{
    if (document) {
        document->removeResource("grid", this);
    }

    assert(views.empty());

    _page_selected_connection.disconnect();
    _page_modified_connection.disconnect();

    SPObject::release();
}

static std::optional<GridType> readGridType(char const *value)
{
    if (!value) {
        return {};
    } else if (!std::strcmp(value, "xygrid")) {
        return GridType::RECTANGULAR;
    } else if (!std::strcmp(value, "axonomgrid")) {
        return GridType::AXONOMETRIC;
    } else {
        return {};
    }
}

void SPGrid::set(SPAttr key, const gchar* value)
{
    switch (key) {
        case SPAttr::TYPE: {
            auto const grid_type = readGridType(value).value_or(GridType::RECTANGULAR); // default
            if (grid_type != _grid_type) {
                _grid_type = grid_type;
                _recreateViews();
            }
            break;
        }
        case SPAttr::ORIGINX:
            _origin_x.readOrUnset(value);
            requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::ORIGINY:
            _origin_y.readOrUnset(value);
            requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::SPACINGX:
            _spacing_x.readOrUnset(value, SVGLength::Unit::PX, 1.0, 1.0);
            requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::SPACINGY:
            _spacing_y.readOrUnset(value, SVGLength::Unit::PX, 1.0, 1.0);
            requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::ANGLE_X: // only meaningful for axonomgrid
            _angle_x.readOrUnset(value, SVGAngle::Unit::DEG, 30, 30);
            requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::ANGLE_Z: // only meaningful for axonomgrid
            _angle_z.readOrUnset(value, SVGAngle::Unit::DEG, 30, 30);
            requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::COLOR:
            _minor_color = (_minor_color & 0xff) | sp_svg_read_color(value, GRID_DEFAULT_MINOR_COLOR);
            requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::EMPCOLOR:
            _major_color = (_major_color & 0xff) | sp_svg_read_color(value, GRID_DEFAULT_MAJOR_COLOR);
            requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::VISIBLE:
            _visible.readOrUnset(value);
            requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::ENABLED:
            _enabled.readOrUnset(value);
            if (_snapper) _snapper->setEnabled(_enabled);
            requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::OPACITY:
            _minor_opacity = sp_ink_read_opacity(value, &_minor_color, GRID_DEFAULT_MINOR_COLOR);
            requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::EMPOPACITY:
            _major_opacity = sp_ink_read_opacity(value, &_major_color, GRID_DEFAULT_MAJOR_COLOR);
            requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::MAJOR_LINE_INTERVAL:
            _major_line_interval = value ? std::max(std::stoi(value), 1) : 5;
            requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::DOTTED:
            _dotted.readOrUnset(value);
            requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::SNAP_TO_VISIBLE_ONLY:
            _snap_to_visible_only.readOrUnset(value);
            if (_snapper) _snapper->setSnapVisibleOnly(_snap_to_visible_only);
            requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        default:
            SPObject::set(key, value);
            break;
    }
}

/**
 * checks for old grid attriubte keys from version 0.46, and 
 * sets the old defaults to the newer attribute keys
 */
void SPGrid::_checkOldGrid(SPDocument *doc, Inkscape::XML::Node *repr)
{
    // set old settings
    const char* gridspacingx    = "1px";
    const char* gridspacingy    = "1px";
    const char* gridoriginy     = "0px";
    const char* gridoriginx     = "0px";
    const char* gridempspacing  = "5";
    const char* gridcolor       = "#3f3fff";
    const char* gridempcolor    = "#3f3fff";
    const char* gridopacity     = "0.15";
    const char* gridempopacity  = "0.38";

    if (auto originx = repr->attribute("gridoriginx")) {
        gridoriginx = originx;
        _legacy = true;
    }
    if (auto originy = repr->attribute("gridoriginy")) {
        gridoriginy = originy;
        _legacy = true;
    }
    if (auto spacingx = repr->attribute("gridspacingx")) {
        gridspacingx = spacingx;
        _legacy = true;
    }
    if (auto spacingy = repr->attribute("gridspacingy")) {
        gridspacingy = spacingy;
        _legacy = true;
    }
    if (auto minorcolor = repr->attribute("gridcolor")) {
        gridcolor = minorcolor;
        _legacy = true;
    }
    if (auto majorcolor = repr->attribute("gridempcolor")) {
        gridempcolor = majorcolor;
        _legacy = true;
    }
    if (auto majorinterval = repr->attribute("gridempspacing")) {
        gridempspacing = majorinterval;
        _legacy = true;
    }
    if (auto minoropacity = repr->attribute("gridopacity")) {
        gridopacity = minoropacity;
        _legacy = true;
    }
    if (auto majoropacity = repr->attribute("gridempopacity")) {
        gridempopacity = majoropacity;
        _legacy = true;
    }

    if (_legacy) {
        // generate new xy grid with the correct settings
        // first create the child xml node, then hook it to repr. This order is important, to not set off listeners to repr before the new node is complete.
        Inkscape::XML::Document *xml_doc = document->getReprDoc();
        Inkscape::XML::Node *newnode = xml_doc->createElement("inkscape:grid");
        newnode->setAttribute("id", "GridFromPre046Settings");
        newnode->setAttribute("type", getSVGType());
        newnode->setAttribute("originx", gridoriginx);
        newnode->setAttribute("originy", gridoriginy);
        newnode->setAttribute("spacingx", gridspacingx);
        newnode->setAttribute("spacingy", gridspacingy);
        newnode->setAttribute("color", gridcolor);
        newnode->setAttribute("empcolor", gridempcolor);
        newnode->setAttribute("opacity", gridopacity);
        newnode->setAttribute("empopacity", gridempopacity);
        newnode->setAttribute("empspacing", gridempspacing);

        repr->appendChild(newnode);
        Inkscape::GC::release(newnode);

        // remove all old settings 
        repr->removeAttribute("gridoriginx");
        repr->removeAttribute("gridoriginy");
        repr->removeAttribute("gridspacingx");
        repr->removeAttribute("gridspacingy");
        repr->removeAttribute("gridcolor");
        repr->removeAttribute("gridempcolor");
        repr->removeAttribute("gridopacity");
        repr->removeAttribute("gridempopacity");
        repr->removeAttribute("gridempspacing");
    }
}

static std::unique_ptr<Inkscape::CanvasItemGrid> create_view(GridType grid_type, Inkscape::CanvasItemGroup *canvasgrids)
{
    switch (grid_type) {
        case GridType::RECTANGULAR: return std::make_unique<Inkscape::CanvasItemGridXY>    (canvasgrids); break;
        case GridType::AXONOMETRIC: return std::make_unique<Inkscape::CanvasItemGridAxonom>(canvasgrids); break;
        default: g_assert_not_reached(); return {};
    }
}

void SPGrid::_recreateViews()
{
    // handle change in grid type requiring all views to be recreated as a different type
    for (auto &view : views) {
        view = create_view(_grid_type, view->get_parent());
    }
}

// update internal state on XML change
void SPGrid::modified(unsigned int flags)
{
    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        updateRepr();
    }
}

// tell canvas to redraw grid
void SPGrid::update(SPCtx *ctx, unsigned int flags)
{
    auto [origin, spacing] = getEffectiveOriginAndSpacing();

    auto prefs = Inkscape::Preferences::get();
    if (prefs->getBool("/options/origincorrection/page", true))
        origin *= document->getPageManager().getSelectedPageAffine();

    for (auto &view : views) {
        view->set_visible(_visible && _enabled);
        if (_enabled) {
            view->set_origin(origin);
            view->set_spacing(spacing);
            view->set_major_color(_major_color);
            view->set_minor_color(_minor_color);
            view->set_dotted(_dotted);
            view->set_major_line_interval(_major_line_interval);

            if (auto axonom = dynamic_cast<Inkscape::CanvasItemGridAxonom *>(view.get())) {
                axonom->set_angle_x(_angle_x.computed);
                axonom->set_angle_z(_angle_z.computed);
            }
        }
    }
}

/**
 * creates a new grid canvasitem for the SPDesktop given as parameter. Keeps a link to this canvasitem in the views list.
 */
void SPGrid::show(SPDesktop *desktop)
{
    if (!desktop) return;

    // check if there is already a canvasitem on this desktop linking to this grid
    for (auto &view : views) {
        if (desktop->getCanvasGrids() == view->get_parent()) {
            return;
        }
    }

    // create designated canvasitem for this grid
    views.emplace_back(create_view(_grid_type, desktop->getCanvasGrids()));

    requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

void SPGrid::hide(SPDesktop const *desktop)
{
    if (!desktop) return;

    for (auto it = views.begin(); it != views.end(); ++it) {
        auto view = it->get();
        if (view->get_parent() == desktop->getCanvasGrids()) {
            views.erase(it);
            break;
        }
    }
}

void SPGrid::scale(const Geom::Scale &scale)
{
    setOrigin( getOrigin() * scale );
    setSpacing( getSpacing() * scale );
}

Inkscape::Snapper *SPGrid::snapper()
{
    if (!_snapper) {
        // lazily create
        _snapper = std::make_unique<Inkscape::GridSnapper>(this, &document->getNamedView()->snap_manager, 0);
        _snapper->setEnabled(_enabled);
        _snapper->setSnapVisibleOnly(_snap_to_visible_only);
    }
    return _snapper.get();
}

static auto ensure_min(double s)
{
    return std::max(s, 0.00001); // clamping, spacing must be > 0
}

static auto ensure_min(Geom::Point const &s)
{
    return Geom::Point(ensure_min(s.x()), ensure_min(s.y()));
}

std::pair<Geom::Point, Geom::Point> SPGrid::getEffectiveOriginAndSpacing() const
{
    auto const scale = document->getDocumentScale();
    return { getOrigin() * scale, ensure_min(getSpacing()) * scale };
}

const char *SPGrid::displayName() const
{
    switch (_grid_type) {
        case GridType::RECTANGULAR: return _("Rectangular Grid");
        case GridType::AXONOMETRIC: return _("Axonometric Grid");
        default: g_assert_not_reached();
    }
}

const char *SPGrid::getSVGType() const
{
    switch (_grid_type) {
        case GridType::RECTANGULAR: return "xygrid";
        case GridType::AXONOMETRIC: return "axonomgrid";
        default: g_assert_not_reached();
    }
}

void SPGrid::setSVGType(char const *svgtype)
{
    auto target_type = readGridType(svgtype);
    if (!target_type || *target_type == _grid_type) {
        return;
    }

    getRepr()->setAttribute("type", svgtype);

    requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

// finds the canvasitem active in the current active view
Inkscape::CanvasItemGrid *SPGrid::getAssociatedView(SPDesktop const *desktop)
{
    for (auto &view : views) {
        if (desktop->getCanvasGrids() == view->get_parent()) {
            return view.get();
        }
    }
    return nullptr;
}

void SPGrid::setVisible(bool v)
{
    getRepr()->setAttributeBoolean("visible", v);

    requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

bool SPGrid::isEnabled() const
{
    return _enabled;
}

void SPGrid::setEnabled(bool v)
{
    getRepr()->setAttributeBoolean("enabled", v);

    if (_snapper) _snapper->setEnabled(v);

    requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

Geom::Point SPGrid::getOrigin() const
{
    return Geom::Point(_origin_x ? _origin_x.computed : 0.0,
                       _origin_y ? _origin_y.computed : 0.0);
}

void SPGrid::setOrigin(Geom::Point const &new_origin)
{
    Inkscape::XML::Node *repr = getRepr();
    repr->setAttributeSvgDouble("originx", new_origin[Geom::X]);
    repr->setAttributeSvgDouble("originy", new_origin[Geom::Y]);

    requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

void SPGrid::setMajorColor(const guint32 color)
{
    char color_str[16];
    sp_svg_write_color(color_str, 16, color);

    getRepr()->setAttribute("empcolor", color_str);

    requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

void SPGrid::setMinorColor(const guint32 color)
{
    char color_str[16];
    sp_svg_write_color(color_str, 16, color);

    getRepr()->setAttribute("color", color_str);

    requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

Geom::Point SPGrid::getSpacing() const
{
    // the value for spacing is interpreted differently based on the type of grid
    // values are copied from XML in previous Inkscape version for rectangular
    double default_value = (_grid_type == GridType::RECTANGULAR) ? 0.26458333 : 1.0;

    return Geom::Point(_spacing_x ? _spacing_x.computed : default_value,
                       _spacing_y ? _spacing_y.computed : default_value);
}

void SPGrid::setSpacing(const Geom::Point &spacing)
{
    Inkscape::XML::Node *repr = getRepr();
    repr->setAttributeSvgDouble("spacingx", spacing[Geom::X]);
    repr->setAttributeSvgDouble("spacingy", spacing[Geom::Y]);

    requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

void SPGrid::setMajorLineInterval(const guint32 interval)
{
    getRepr()->setAttributeInt("empspacing", interval);

    requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

void SPGrid::setMinorOpacity(const float opacity)
{
    getRepr()->setAttributeSvgDouble("opacity", opacity);

    requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

void SPGrid::setMajorOpacity(const float opacity)
{
    getRepr()->setAttributeSvgDouble("empopacity", opacity);

    requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

void SPGrid::setDotted(bool v)
{
    getRepr()->setAttributeBoolean("dotted", v);

    requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

void SPGrid::setSnapToVisibleOnly(bool v)
{
    getRepr()->setAttributeBoolean("snapvisiblegridlinesonly", v);
    if (_snapper) _snapper->setSnapVisibleOnly(v);

    requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

void SPGrid::setAngleX(double deg)
{
    getRepr()->setAttributeSvgDouble("gridanglex", deg);

    requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

void SPGrid::setAngleZ(double deg)
{
    getRepr()->setAttributeSvgDouble("gridanglez", deg);

    requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

const char *SPGrid::typeName() const
{
    switch (_grid_type) {
        case GridType::RECTANGULAR: return "grid-rectangular";
        case GridType::AXONOMETRIC: return "grid-axonometric";
        default: g_assert_not_reached(); return "grid";
    }
}

const Inkscape::Util::Unit *SPGrid::getUnit()
{
    auto prefs = Inkscape::Preferences::get();
    switch (_grid_type) {
        case GridType::RECTANGULAR: return Inkscape::Util::unit_table.getUnit(prefs->getString("/options/grids/xy/units"));
        case GridType::AXONOMETRIC: return Inkscape::Util::unit_table.getUnit(prefs->getString("/options/grids/axonom/units"));
        default: g_assert_not_reached();
    }
}

void SPGrid::setUnits(const Glib::ustring units)
{
    getRepr()->setAttribute("units", units);

    requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}
