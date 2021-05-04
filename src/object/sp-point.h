// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SPPoint -- an inkscape point
 *//*
 * Authors:
 *   Martin Owens 2021
 *
 * Copyright (C) 2021 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_SP_POINT_H
#define SEEN_SP_POINT_H

#include <2geom/point.h>

#include "sp-item.h"
#include "helper/auto-connection.h"
#include "svg/svg-length.h"

class SPPoint final : public SPItem
{
public:
    int tag() const override { return tag_of<decltype(*this)>; }

    // Used API for this SPItem
    Geom::Point *parentPoint() const;
    Geom::Point *itemPoint() const;
    void setParentPoint(Geom::Point const &parent_point);
    void setItemPoint(Geom::Point const *doc_point);

    std::string const &getOriginalPointName() const;
    void setOriginalPointName(std::string name);

    // Static API for consistancy
    static SPPoint *makePointAbsolute(SPItem *parent, Geom::Point const *item_point);
    static SPPoint *makePointRelative(SPItem *parent, Geom::Point const *parent_point, std::string const &name = "");

    static Geom::Point *getParentPoint(SPItem const *parent, Geom::Point const *item_point);
    static Geom::Point *getItemPoint(SPItem const *parent, Geom::Point const *parent_point);

protected:
    void build(SPDocument *doc, Inkscape::XML::Node *repr) override;
    void set(SPAttr key, char const *value) override;

    Inkscape::XML::Node *write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, unsigned flags) override;

    void parentModified(SPObject *parent, int flags);

private:
    // These lengths are intended to be relative percents, but don't have to be.
    SVGLength x;
    SVGLength y;

    // A link to the virtual-point that spawned this point
    // Allows us to connect the two for automatic adjustment later.
    std::string original_point;

    Inkscape::auto_connection _parent_modified;
};

#endif // SEEN_SP_POINT_H

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
