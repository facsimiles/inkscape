// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_LPE_ITEM_REFERENCE_H
#define SEEN_LPE_ITEM_REFERENCE_H

/*
 * Copyright (C) 2008-2012 Authors
 * Authors: Johan Engelen
 *          Abhishek Sharma
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "object/sp-item.h"
#include "object/uri-references.h"

class SPItem;
namespace Inkscape::XML { class Node; }

namespace Inkscape::LivePathEffect {

/**
 * The reference corresponding to href of LPE ItemParam.
 */
class ItemReference : public Inkscape::URIReference
{
public:
    ItemReference(SPObject *owner) : URIReference(owner) {}

    ItemReference(ItemReference const &) = delete;
    ItemReference &operator=(ItemReference const &) = delete;

    SPItem *getObject() const {
        return static_cast<SPItem *>(URIReference::getObject());
    }

protected:
    bool _acceptObject(SPObject *obj) const override;
};

} // namespace Inkscape::LivePathEffect

#endif // SEEN_LPE_ITEM_REFERENCE_H

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
