// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_LPE_CONNECTOR_AVOID_H
#define INKSCAPE_LPE_CONNECTOR_AVOID_H

/** \file
 * LPE <connector_avoid> implementation used by the connector tool
 * to avoid shapes in the document when drawing connector lines.
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
#include "3rdparty/adaptagrams/libavoid/router.h"
#include "3rdparty/adaptagrams/libavoid/shape.h"
#include "live_effects/effect.h"

namespace Inkscape::LivePathEffect {

bool isAvoided(SPObject const *obj);

class LPEConnectorAvoid : public Effect
{
public:
    LPEConnectorAvoid(LivePathEffectObject *lpeobject);
    ~LPEConnectorAvoid() override;

    void doAfterEffect(SPLPEItem const *lpe_item, SPCurve *curve) override;
    void doOnRemove(SPLPEItem const *lpeitem) override;

    Geom::PathVector doEffect_path(Geom::PathVector const &path_in) override;

    static LPEConnectorAvoid *get(SPItem *item);
    static bool toggleAvoid(SPItem *item, bool enable = true);

private:
    std::map<SPItem const *, Avoid::ShapeRef *> avoid_refs;

    void addRef(SPItem const *item, Avoid::ShapeRef *avoid_ref);
    void removeRef(SPItem const *item);
};

} // namespace Inkscape::LivePathEffect

#endif // INKSCAPE_LPE_CONNECTOR_AVOID_H

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
