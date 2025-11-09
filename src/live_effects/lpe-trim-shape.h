// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author(s):
 *     Jabiertxo Arraiza Cenoz <jabier.arraiza@marker.es>
 *
 * Copyright (C) 2014 Author(s)
 *
 * Jabiertxof:Thanks to all people help me
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_LPE_TRIM_SHAPE_H
#define INKSCAPE_LPE_TRIM_SHAPE_H

#include "live_effects/effect.h"
#include "live_effects/parameter/enum.h"
#include "live_effects/parameter/enumarray.h"
#include "live_effects/parameter/scalararray.h"
#include "live_effects/parameter/parameter.h"

namespace Inkscape {
namespace LivePathEffect {

namespace TrimShapeNS {
// we need a separate namespace to avoid clashes with other LPEs
// we use TrimShapeNS to follow similar usage in tapper stroke
class KnotHolderEntityAttach;
}

class LPETrimShape : public Effect {
public:
    LPETrimShape(LivePathEffectObject *lpeobject);
    ~LPETrimShape() override;

    void doOnApply(SPLPEItem const* lpeitem) override;
    void doBeforeEffect (SPLPEItem const* lpeitem) override;
    Geom::PathVector doEffect_path (Geom::PathVector const& path_in) override;
    Geom::Path doEffect_simplePath(Geom::Path const& path, double start, double end);

    void addKnotHolderEntities(KnotHolder * knotholder, SPItem * item) override;
protected:
    friend class TrimShapeNS::KnotHolderEntityAttach;
    ScalarArrayParam attach_start;
    ScalarArrayParam attach_end;
private:
    ScalarParam subpath;
    BoolParam linkall;
    BoolParam flexible;
    size_t previous_size = 0;
    bool prevflex = false;
    double prev_attach_start = -1;
    double prev_attach_end = -1;
    std::vector<Geom::Point> start_attach_point;
    std::vector<Geom::Point> end_attach_point;
    Geom::PathTime _factorTime(Geom::Path const path,Geom::Coord t) const;
    double timeAtLength(double const A, Geom::Piecewise<Geom::D2<Geom::SBasis>> pwd2);
    size_t prev_subpath = Glib::ustring::npos;
    Geom::PathVector _pathvector_before_effect;
    Geom::PathVector pathv_out;
    LPETrimShape(const LPETrimShape&) = delete;
    LPETrimShape& operator=(const LPETrimShape&) = delete;
};

} //namespace LivePathEffect
} //namespace Inkscape

#endif

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

