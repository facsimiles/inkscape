// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author(s):
 *   Jabiertxo Arraiza Cenoz <jabier.arraiza@marker.es>
 *
 * Copyright (C) 2014 Author(s)
 *
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "lpe-trim-shape.h"
#include <glibmm/i18n.h>
#include "helper/geom.h"
#include "helper/geom-nodetype.h"
#include "object/sp-shape.h"
#include "object/sp-lpe-item.h"
#include "svg/svg.h"
#include "ui/knot/knot-holder.h"
#include "ui/knot/knot-holder-entity.h"


template<typename T>
inline bool withinRange(T value, T low, T high) {
    return (value > low && value < high);
}

namespace Inkscape {
namespace LivePathEffect {

namespace TrimShapeNS {
    class KnotHolderEntityAttach : public LPEKnotHolderEntity {
    public:
        KnotHolderEntityAttach(LPETrimShape * effect, size_t index, bool begin) 
        : LPEKnotHolderEntity(effect)
        , _effect(effect)
        , _index(index)
        , _begin(begin) {};
        void knot_set(Geom::Point const &p, Geom::Point const &origin, guint state) override;
        void knot_ungrabbed(Geom::Point const &p, Geom::Point const &origin, unsigned state) override;
        Geom::Point knot_get() const override;
        bool valid_index(unsigned int index) const {
            return (_effect->attach_start._vector.size() > index);
        };
    protected:
        size_t _index;
        bool _begin;
        LPETrimShape * _effect;
    };
} // TrimShapeNS

LPETrimShape::LPETrimShape(LivePathEffectObject *lpeobject)
    : Effect(lpeobject),
    subpath(_("Select subpath"), _("Select the subpath you want to modify"), "subpath", &wr, this, 1.),
    attach_start(_("Start offset"), _("Trim distance from path start"), "attach_start", &wr, this, 10),
    attach_end(_("End offset"), _("The ending position of the trim"), "end_offset", &wr, this, 10),
    flexible(_("Flexible position"), _("Flexible or absolute document units"), "flexible", &wr, this, true),
    linkall(_("Link all subpaths"), _("Link all subpaths"), "linkall", &wr, this, false)
{
    show_orig_path = true;
    _provides_knotholder_entities = true;
    attach_start.param_set_digits(2);
    attach_start.param_set_increments(1, 1);
    attach_end.param_set_digits(2);
    attach_end.param_set_increments(1, 1);
    subpath.param_set_range(1, 1);
    subpath.param_set_increments(1, 1);
    subpath.param_set_digits(0);
    registerParameter(&subpath);
    registerParameter(&attach_start);
    registerParameter(&attach_end);
    registerParameter(&flexible);
    registerParameter(&linkall);
}

LPETrimShape::~LPETrimShape() = default;

void LPETrimShape::doOnApply(SPLPEItem const *lpeItem)
{
    SPLPEItem *splpeitem = const_cast<SPLPEItem *>(lpeItem);
    auto shape = cast<SPShape>(splpeitem);
    if (!shape) {
        g_warning("LPE Slice Nodes can only be applied to shapes (not groups).");
        SPLPEItem *item = const_cast<SPLPEItem *>(lpeItem);
        item->removeCurrentPathEffect(false);
    }
}

// breaks time value into integral and fractional part
// must be better add PathVectorTimeAt and PathTimeAt to 2Geom adding this dupled function
Geom::PathTime LPETrimShape::_factorTime(Geom::Path const path, Geom::Coord t) const
{
    Geom::Path::size_type sz = path.size_default();
    if (t < 0 || t > sz) {
        g_warning("parameter t out of bounds");
        t = sz;
    }

    Geom::PathTime ret;
    Geom::Coord k;
    ret.t = modf(t, &k);
    ret.curve_index = k;
    if (ret.curve_index == sz) {
        --ret.curve_index;
        ret.t = 1;
    }
    return ret;
}

/**
 * @return Always returns a PathVector with three elements.
 *
 *  The positions of the effect knots are accessed to determine
 *  where exactly the input path should be split.
 */
Geom::Path LPETrimShape::doEffect_simplePath(Geom::Path const & path, double start, double end)
{
    bool cross_start = false;
    if (path.size() - start < end) { // this allow coninuing on closed paths like circles
        cross_start = path.closed();
    }
    if (path.size() - start == 0 && end == 0) { // this hide the path if start and end collapse at 0
        return Geom::Path();
    }
    return path.portion(_factorTime(path, start), _factorTime(path, path.size() - end), cross_start);
}

///Calculate the time in curve with a length
//TODO: find a better place to it dupled code better in 2GEOM
double LPETrimShape::timeAtLength(double const A, Geom::Piecewise<Geom::D2<Geom::SBasis>> pwd2)
{
    if ( A == 0 || pwd2.size() == 0) {
        return 0;
    }

    double t = pwd2.size();
    std::vector<double> t_roots = roots(Geom::arcLengthSb(pwd2) - A);
    if (!t_roots.empty()) {
        t = t_roots[0];
    }
    return t;
}

void LPETrimShape::doBeforeEffect(SPLPEItem const *lpeItem)
{
    using namespace Geom;
    // this allow multi stack LPE
    auto first =  dynamic_cast<LPETrimShape const *>(lpeItem->getFirstPathEffectOfType(TRIM_SHAPE));
    _pathvector_before_effect = pathvector_before_effect;
    if (this != first) {
        _pathvector_before_effect = first->_pathvector_before_effect;
    }
    // define ranges based on flexible value
    if (prevflex != flexible) {
        if (flexible) {
            attach_start.param_set_range(0, 100);
            attach_end.param_set_range(0, 100);
        } else {
            attach_start.param_set_range(0, std::numeric_limits<double>::max());
            attach_end.param_set_range(0, std::numeric_limits<double>::max());
        }
    }
    prevflex = flexible;
    Geom::PathVector pathv = pathv_to_linear_and_cubic_beziers(_pathvector_before_effect);
    size_t sicepv = pathv.size();
    bool write = false;
    // if number of subpaths change 
    if (previous_size != sicepv) {
        subpath.param_set_range(1, sicepv);
        // move to first one
        subpath.param_readSVGValue("1");
        if (!is_load) {
            attach_start._vector.clear();
            attach_end._vector.clear();
        }
        previous_size = sicepv;
        linkall.param_widget_is_enabled(sicepv > 1);
        refresh_widgets = true;
    }

    // if no begin data
    if (!attach_start._vector.size()) {
        // add 0 to all paths
        for (auto path : _pathvector_before_effect) {
            attach_start._vector.push_back(0);
            attach_end._vector.push_back(0);
        }
        attach_start.param_set_default();
        attach_end.param_set_default();
        write = true;
    }
    // if active subpath change 
    if (prev_subpath != subpath) {
        attach_start.param_setActive(subpath - 1);
        attach_end.param_setActive(subpath - 1);
        prev_subpath = subpath;
        refresh_widgets = true;
        write = true;
    }
    std::vector<double> attach_startv;
    Geom::Path::size_type index = 0;
    // for each data in begin
    for (auto & doub : attach_start.data()) {
        if (linkall) {
            // we need to sinc, add all the same to all
            attach_startv.push_back(attach_start._vector[subpath - 1]);
        } else {
            // if active subpath change and path is not current set value to negative to further ignore
            if (prev_subpath != subpath && index != (int)subpath) {
                attach_startv.push_back(-1);
            } else {
                attach_startv.push_back(doub);
            }
        }
        index += 1;
    }
    std::vector<double> attach_endv;
    index = 0;
    for (auto & doub : attach_end.data()) {
        if (linkall) {
            attach_endv.push_back(attach_end._vector[subpath - 1]);
        } else {
            // if active subpath change and path is not current set value to negative to further ignore
            if (prev_subpath != subpath && index != (int)subpath) {
                attach_endv.push_back(-1);
            } else {
                attach_endv.push_back(doub);
            }
        }
        index += 1;
    }
    // if write or start or end are moved
    if (write || (linkall && (
            prev_attach_start != attach_start._vector[subpath - 1] || 
            prev_attach_end != attach_end._vector[subpath - 1]
        ))) 
    {
        attach_start.param_set_and_write_new_value(attach_startv);
        attach_end.param_set_and_write_new_value(attach_endv);
    }
    prev_attach_start = attach_start._vector[subpath - 1];
    prev_attach_end = attach_end._vector[subpath - 1];
    pathv_out.clear();
    if (_pathvector_before_effect.empty()) {
        return;
    }
    
    index = 0;
    // clear points
    start_attach_point.clear();
    end_attach_point.clear();
    for (auto path : pathv) {
        Geom::Path first_cusp = path;
        Geom::Path last_cusp = path.reversed();
        Geom::Coord path_length = path.length();
        Geom::Piecewise< Geom::D2< Geom::SBasis > > first_cusp_pwd2 = first_cusp.toPwSb();
        Geom::Piecewise< Geom::D2< Geom::SBasis > > last_cusp_pwd2 = last_cusp.toPwSb();
        
        // there is a pretty good chance that people will try to drag the knots
        // on top of each other, so block it

        if (flexible) {
            if (double(unsigned(attach_startv[index])) > 100.0) {
                attach_startv[index] = 100.0;
            }
            if (double(unsigned(attach_endv[index])) > 100.0) {
                attach_endv[index] = 100.0;
            }
            unsigned allowed_start = 100.0;
            unsigned allowed_end = 100.0;

            // don't let the knots be farther than they are allowed to be
            {
                if ((unsigned)attach_startv[index] >= allowed_start) {
                    attach_startv[index] = ((double)allowed_start);
                }
                if ((unsigned)attach_endv[index] >= allowed_end) {
                    attach_endv[index] = ((double)allowed_end);
                }
            }
        }
        
        
        Geom::Path path_tmp;
        // Calculate length
        double start_path_length;
        double end_path_length;
        if (flexible) {
            start_path_length = (attach_startv[index] * path_length) /  100.0;
            end_path_length = (attach_endv[index] * path_length) /  100.0;
        } else  {
            start_path_length = std::min(path_length,attach_startv[index]);
            end_path_length = std::min(path_length,attach_endv[index]);
        }
        // get positions
        Geom::Coord new_pos_start = timeAtLength(start_path_length,first_cusp_pwd2);
        Geom::Coord new_pos_end = timeAtLength(end_path_length,last_cusp_pwd2);
        start_attach_point.push_back(first_cusp(new_pos_start));
        end_attach_point.push_back(last_cusp(new_pos_end));
        // do portioned
        path_tmp = doEffect_simplePath(path, new_pos_start, new_pos_end);
        pathv_out.push_back(path_tmp);
        index++;
    }
    if (this != first) { // if LPE is not the first one add it to allow staking
        for (auto path : pathvector_before_effect) {
            pathv_out.push_back(path);
        }
    }
    attach_startv.clear();
    attach_endv.clear();    
}

Geom::PathVector
LPETrimShape::doEffect_path(Geom::PathVector const &path_in)
{   
    return pathv_out;
}

void LPETrimShape::addKnotHolderEntities(KnotHolder *knotholder, SPItem *item)
{
    for (size_t i = 0 ; i < attach_start._vector.size(); i++) {
        KnotHolderEntity *e = new TrimShapeNS::KnotHolderEntityAttach(this, i, true);
        e->create(nullptr, item, knotholder, Inkscape::CANVAS_ITEM_CTRL_TYPE_LPE, "LPE:TrimShapeBegin",
                _("<b>Start point of the trim</b>: drag to alter the trim"));
        knotholder->add(e);

        KnotHolderEntity *f = new TrimShapeNS::KnotHolderEntityAttach(this, i, false);
        f->create(nullptr, item, knotholder, Inkscape::CANVAS_ITEM_CTRL_TYPE_LPE, "LPE:TrimShapeEnd",
                _("<b>End point of the trim</b>: drag to alter the trim"));
        knotholder->add(f);
    }
}

namespace TrimShapeNS {

void KnotHolderEntityAttach::knot_set(Geom::Point const &p, Geom::Point const&/*origin*/, guint state)
{
    using namespace Geom;
    // check is valid index
    if (_begin && (!valid_index(_index) || _effect->start_attach_point.size() <= _index)) {
        return;
    }

    if (!_begin && (!valid_index(_index) || _effect->end_attach_point.size() <= _index)) {
        return;
    }

    Geom::Point const s = snap_knot_position(p, state);

    if (!is<SPShape>(_effect->sp_lpe_item)) {
        g_warning("LPEItem is not a path!");
        return;
    }
    
    if (!cast_unsafe<SPShape>(_effect->sp_lpe_item)->curve()) {
        // oops
        return;
    }
    // in case you are wondering, the above are simply sanity checks. we never want to actually
    // use that object.
    
    Geom::PathVector pathv = _effect->_pathvector_before_effect;
    Piecewise<D2<SBasis> > pwd2;
    Geom::Path p_in;
    if (_begin) {
        p_in = pathv[_index];
    } else {
        p_in = pathv[_index].reversed();
    }
    // calculate lenght
    Geom::Coord path_length = p_in.length();
    pwd2.concat(p_in.toPwSb());
    double t0 = nearest_time(s, pwd2);
    // if the position is not very near to start or end
    if (!Geom::are_near(0,t0,0.01)) {
        // get the removed portion to measure
        Geom::Path p_in_port = p_in.portion(0,t0);
        Geom::Coord length = p_in_port.length();
        if (_effect->flexible) {
            t0 = (length * 100.0) / path_length;
        } else  {
            t0 = length;
        }
    } else {
        t0 = 0;
    }
    // if link all we use current subpath to
    size_t pos = _index;
    if (_effect->linkall) {
        pos = _effect->subpath - 1;
    }
    if (_begin) {
        _effect->attach_start._vector[pos] = t0;
        _effect->attach_start.write_to_SVG();
    } else {
        _effect->attach_end._vector[pos] = t0;
        _effect->attach_end.write_to_SVG();
    }
}

void KnotHolderEntityAttach::knot_ungrabbed(Geom::Point const &p, Geom::Point const &origin, unsigned state)
{
    LPETrimShape *lpe = dynamic_cast<LPETrimShape *>(_effect);
    lpe->makeUndoDone(_("Move handle"));
    sp_lpe_item_update_patheffect(cast<SPLPEItem>(item), false, false);
}

Geom::Point KnotHolderEntityAttach::knot_get() const
{
    if (!valid_index(_index)) {
        return Geom::Point();
    }
    if (_begin && _effect && _effect->start_attach_point.size() > _index) {
        return _effect->start_attach_point[_index];
    }
    if (!_begin && _effect && _effect->end_attach_point.size() > _index) {
        return _effect->end_attach_point[_index];
    }
    return Geom::Point();
}

} // namespace TrimShapeNS
} // namespace LivePathEffect
} // namespace Inkscape

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offset:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
