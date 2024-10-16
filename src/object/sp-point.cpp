// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape connector point implementation
 *
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2021 Martin Owens
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "object/sp-point.h"

#include <glibmm/i18n.h>

#include "attributes.h"
#include "document.h"

void SPPoint::build(SPDocument *document, Inkscape::XML::Node *repr)
{
    SPItem::build(document, repr);

    readAttr(SPAttr::X);
    readAttr(SPAttr::Y);
    readAttr(SPAttr::VIRTUAL_POINT_REF);

    // When the parent moves or changes, connected points need to know this is
    // passed to the LPE via the connectModified signal which is cascaded.
    _parent_modified = parent->connectModified(sigc::mem_fun(*this, &SPPoint::parentModified));
}

void SPPoint::set(SPAttr key, char const *value)
{
    switch (key) {
        case SPAttr::X:
            if (!x.read(value)) {
                x.read("50%");
            }
            break;
        case SPAttr::Y:
            if (!y.read(value)) {
                y.read("50%");
            }
            break;
        case SPAttr::VIRTUAL_POINT_REF:
            if (value) {
                original_point = value;
            } else {
                original_point = "";
            }
            break;
        default:
            SPItem::set(key, value);
            break;
    }
}

Inkscape::XML::Node *SPPoint::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, unsigned flags)
{
    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("inkscape:point");
    }
    repr->setAttributeSvgLength("x", x);
    repr->setAttributeSvgLength("y", y);
    repr->setAttributeOrRemoveIfEmpty("inkscape:original-point", original_point);

    SPItem::write(xml_doc, repr, flags);
    return repr;
}

/**
 * Create a new sp-point in the given parent object at the given location.
 *
 * @param parent - Item to make the point within
 * @param item_point - The point in item coordinates.
 *
 * @returns The new SPPoint object created, or nullptr if outside the bbox
 */
SPPoint *SPPoint::makePointAbsolute(SPItem *parent, Geom::Point const *item_point)
{
    return makePointRelative(parent, getParentPoint(parent, item_point));
}

SPPoint *SPPoint::makePointRelative(SPItem *parent, Geom::Point const *parent_point, std::string const &name)
{
    auto xml_doc = parent->document->getReprDoc();
    auto repr = xml_doc->createElement("inkscape:point");
    auto new_point = cast<SPPoint>(parent->appendChildRepr(repr));
    assert(new_point);
    if (!name.empty()) {
        new_point->setOriginalPointName(name);
    }
    new_point->setParentPoint(*parent_point);
    g_assert(new_point->getId());
    return new_point;
}

/**
 * Convert a set of item coordinates to a relative set inside the parent.
 *
 * @param parent - Item the relative points will be made relative to.
 * @param item_point - Coordinates in absolute item units.
 *
 * @returns The position of the point in relative units (%)
 */
Geom::Point *SPPoint::getParentPoint(SPItem const *parent, Geom::Point const *item_point)
{
    if (parent && item_point) {
        auto bbox = parent->bbox(Geom::identity(), SPItem::GEOMETRIC_BBOX);
        if (bbox) {
            return new Geom::Point{(*item_point - bbox->min()) / bbox->dimensions()};
        }
    }
    return nullptr;
}

/*
 * Returns the relative coordinates of this point.
 */
Geom::Point *SPPoint::parentPoint() const
{
    if (x.unit == SVGLength::PERCENT && y.unit == SVGLength::PERCENT) {
        return new Geom::Point{x.computed, y.computed};
    }
    // Recalculate non-relative coordinates according to the parent object.
    return getParentPoint(cast<SPItem>(parent), itemPoint());
}

/**
 * Convert a relative set of points to an absolute one inside the parent.
 *
 * @param parent - Item these relative points are relative to.
 * @param parent_point - Relative coordinates between 0.0 and 1.0
 *
 * @returns The position of the point in item units.
 */
Geom::Point *SPPoint::getItemPoint(SPItem const *parent, Geom::Point const *parent_point)
{
    if (parent && parent_point) {
        if (auto bbox = parent->bbox(Geom::identity(), SPItem::GEOMETRIC_BBOX)) {
            return new Geom::Point{bbox->min() + *parent_point * bbox->dimensions()};
        }
    }
    return nullptr;
}

/**
 * Returns the untransformed, absolute coordinates of this point.
 */
Geom::Point *SPPoint::itemPoint() const
{
    auto point = getItemPoint(cast<SPItem>(parent), new Geom::Point{x.computed, y.computed});
    if (!point) {
        return nullptr;
    }

    // Retrofit absolute units in, non-percent isn't expected, but
    // we want to support it as it will be parsed as a unit value.
    if (x.unit != SVGLength::PERCENT) {
        point->x() = x.computed;
    }
    if (y.unit != SVGLength::PERCENT) {
        point->y() = y.computed;
    }

    return point;
}

/**
 * Set the parent point and request display update.
 *
 * @param parent_point - Relative percentage point, must be between 0.0 and 1.0
 */
void SPPoint::setParentPoint(Geom::Point const &parent_point)
{
    x = std::clamp(parent_point.x(), 0.0, 1.0);
    y = std::clamp(parent_point.y(), 0.0, 1.0);
    x.unit = SVGLength::PERCENT;
    y.unit = SVGLength::PERCENT;

    auto repr = getRepr();
    write(repr->document(), repr, SP_OBJECT_MODIFIED_FLAG);
    requestModified(SP_OBJECT_MODIFIED_FLAG);
}

/**
 * Set the item point (absolute units), calls setParentPoint
 *
 * @param item_point - Abslute position of the point
 */
void SPPoint::setItemPoint(Geom::Point const *item_point)
{
    auto point = getParentPoint(cast<SPItem>(parent), item_point);
    if (point) {
        setParentPoint(*point);
    }
}

/**
 * Returns the original point name, if this point was created from one
 */
std::string const &SPPoint::getOriginalPointName() const
{
    return original_point;
}

/**
 * Set the original point's name (from the hint table)
 */
void SPPoint::setOriginalPointName(std::string name)
{
    original_point = std::move(name);
    auto repr = getRepr();
    write(repr->document(), repr, SP_OBJECT_MODIFIED_FLAG);
}

/**
 * Called when the parent owner of this point has been modified.
 */
void SPPoint::parentModified(SPObject * /*parent*/, int flags)
{
    emitModified(SP_OBJECT_MODIFIED_CASCADE);
}

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
