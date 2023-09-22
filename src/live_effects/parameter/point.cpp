// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "point.h"

#include <utility>
#include <glibmm/i18n.h>
#include <gtkmm/box.h>
#include <sigc++/functors/mem_fun.h>

#include "inkscape.h"
#include "live_effects/effect.h"
#include "svg/stringstream.h"
#include "svg/svg.h"
#include "ui/controller.h"
#include "ui/icon-names.h"
#include "ui/knot/knot-holder-entity.h"
#include "ui/knot/knot-holder.h"
#include "ui/pack.h"
#include "ui/widget/point.h"

namespace Inkscape::LivePathEffect {

PointParam::PointParam(const Glib::ustring& label, const Glib::ustring& tip,
                       const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                       Effect * const effect, std::optional<Glib::ustring> htip, Geom::Point default_value,
                       bool live_update )
    : Parameter(label, tip, key, wr, effect)
    , defvalue{std::move(default_value)}
    , liveupdate(live_update)
    , handle_tip{std::move(htip)}
{
}

PointParam::~PointParam() {
    if (_knotholder) {
        _knotholder->clear();
        _knotholder = nullptr;
    }
}

void
PointParam::param_set_default()
{
    param_setValue(defvalue,true);
}

void
PointParam::param_set_liveupdate( bool live_update)
{
    liveupdate = live_update;
}

Geom::Point 
PointParam::param_get_default() const{
    return defvalue;
}

void
PointParam::param_update_default(Geom::Point default_point)
{
    defvalue = default_point;
}

void
PointParam::param_update_default(char const * const default_point)
{
    gchar ** strarray = g_strsplit(default_point, ",", 2);
    double newx, newy;
    unsigned int success = sp_svg_number_read_d(strarray[0], &newx);
    success += sp_svg_number_read_d(strarray[1], &newy);
    g_strfreev (strarray);
    if (success == 2) {
        param_update_default( Geom::Point(newx, newy) );
    }
}

void 
PointParam::param_hide_knot(bool hide) {
    if (_knotholder && !_knotholder->entity.empty()) {
        bool update = false;
        if (hide && _knotholder->entity.front()->knot->flags & SP_KNOT_VISIBLE) {
            update = true;
            _knotholder->entity.front()->knot->hide();
        } else if(!hide && !(_knotholder->entity.front()->knot->flags & SP_KNOT_VISIBLE)) {
            update = true;
            _knotholder->entity.front()->knot->show();
        }
        if (update) {
            _knotholder->entity.front()->update_knot();
        }
    }
}

void
PointParam::param_setValue(Geom::Point newpoint, bool write)
{
    *dynamic_cast<Geom::Point *>( this ) = newpoint;
    if(write){
        Inkscape::SVGOStringStream os;
        os << newpoint;
        param_write_to_repr(os.str().c_str());
    }
    if(_knotholder && liveupdate){
        _knotholder->entity.front()->update_knot();
    }
}

bool
PointParam::param_readSVGValue(char const * const strvalue)
{
    gchar ** strarray = g_strsplit(strvalue, ",", 2);
    double newx, newy;
    unsigned int success = sp_svg_number_read_d(strarray[0], &newx);
    success += sp_svg_number_read_d(strarray[1], &newy);
    g_strfreev (strarray);
    if (success == 2) {
        param_setValue( Geom::Point(newx, newy) );
        return true;
    }
    return false;
}

Glib::ustring
PointParam::param_getSVGValue() const
{
    Inkscape::SVGOStringStream os;
    os << *dynamic_cast<Geom::Point const *>( this );
    return os.str();
}

Glib::ustring
PointParam::param_getDefaultSVGValue() const
{
    Inkscape::SVGOStringStream os;
    os << defvalue;
    return os.str();
}

void
PointParam::param_transform_multiply(Geom::Affine const& postmul, bool /*set*/)
{
    param_setValue( (*this) * postmul, true);
}

Gtk::Widget *
PointParam::param_newWidget()
{
    auto const pointwdg = Gtk::make_managed<UI::Widget::RegisteredTransformedPoint>( param_label,
                                                                                     param_tooltip,
                                                                                     param_key,
                                                                                    *param_wr,
                                                                                     param_effect->getRepr(),
                                                                                     param_effect->getSPDoc() );
    Geom::Affine transf = SP_ACTIVE_DESKTOP->doc2dt();
    pointwdg->setTransform(transf);
    pointwdg->setValue( *this );
    pointwdg->clearProgrammatically();
    pointwdg->set_undo_parameters(_("Change point parameter"), INKSCAPE_ICON("dialog-path-effects"));
    pointwdg->signal_x_value_changed().connect(sigc::mem_fun(*this, &PointParam::on_value_changed));
    pointwdg->signal_y_value_changed().connect(sigc::mem_fun(*this, &PointParam::on_value_changed));

    auto const hbox = Gtk::make_managed<Gtk::Box>(Gtk::ORIENTATION_HORIZONTAL);
    UI::pack_start(*hbox, *pointwdg, true, true);
    hbox->show_all_children();
    return hbox;
}

void PointParam::on_value_changed()
{
    param_effect->refresh_widgets = true;
}

void
PointParam::set_oncanvas_looks(Inkscape::CanvasItemCtrlShape shape,
                               Inkscape::CanvasItemCtrlMode mode,
                               std::uint32_t const color)
{
    knot_shape = shape;
    knot_mode  = mode;
    knot_color = color;
}

class PointParamKnotHolderEntity final : public KnotHolderEntity {
public:
    PointParamKnotHolderEntity(PointParam * const p) : pparam{p} {}
    ~PointParamKnotHolderEntity() final { this->pparam->_knotholder = nullptr;}

    void knot_set(Geom::Point const &p, Geom::Point const &origin, unsigned state) final;
    Geom::Point knot_get() const final;
    void knot_ungrabbed(Geom::Point const &p, Geom::Point const &origin, unsigned state) final;
    void knot_click(unsigned state) final;

private:
    PointParam *pparam = nullptr;
};

void
PointParamKnotHolderEntity::knot_set(Geom::Point const &p, Geom::Point const &origin,
                                     unsigned const state)
{
    Geom::Point s = snap_knot_position(p, state);
    if (state & GDK_CONTROL_MASK) {
        Geom::Point A(origin[Geom::X],p[Geom::Y]);
        Geom::Point B(p[Geom::X],origin[Geom::Y]);
        double distanceA = Geom::distance(A,p);
        double distanceB = Geom::distance(B,p);
        if(distanceA > distanceB){
            s = B;
        } else {
            s = A;
        }
    }
    if(this->pparam->liveupdate){
        pparam->param_setValue(s, true);
    } else {
        pparam->param_setValue(s);
    }
}

void
PointParamKnotHolderEntity::knot_ungrabbed(Geom::Point const &p, Geom::Point const &origin,
                                           unsigned const state)
{
    pparam->param_effect->makeUndoDone(_("Move handle"));
}

Geom::Point
PointParamKnotHolderEntity::knot_get() const
{
    return *pparam;
}

void
PointParamKnotHolderEntity::knot_click(unsigned const state)
{
    if (state & GDK_CONTROL_MASK) {
        if (state & GDK_MOD1_MASK) {
            this->pparam->param_set_default();
            pparam->param_setValue(*pparam,true);
        }
    }
}

void
PointParam::addKnotHolderEntities(KnotHolder *knotholder, SPItem *item)
{
    _knotholder = knotholder;
    KnotHolderEntity * knot_entity = new PointParamKnotHolderEntity(this);
    // TODO: can we ditch handleTip() etc. because we have access to handle_tip etc. itself???
    knot_entity->create(nullptr, item, knotholder, Inkscape::CANVAS_ITEM_CTRL_TYPE_LPE, "LPE:Point",
                         handleTip(), knot_color);
    _knotholder->add(knot_entity);
}

} // namespace Inkscape::LivePathEffect

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
