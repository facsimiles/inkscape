// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_LIVEPATHEFFECT_PARAMETER_ITEM_H
#define INKSCAPE_LIVEPATHEFFECT_PARAMETER_ITEM_H

/*
 * Inkscape::LivePathEffectParameters
 *
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glib.h>

#include "live_effects/parameter/parameter.h"
#include "live_effects/parameter/item-reference.h"
#include <sigc++/sigc++.h>

namespace Inkscape::LivePathEffect {

class ItemParam : public Parameter
{
public:
    ItemParam(Glib::ustring const &label,
              Glib::ustring const &tip,
              Glib::ustring const &key,
              Inkscape::UI::Widget::Registry *wr,
              Effect *effect,
              char const *default_value = "");
    ~ItemParam() override;

    Gtk::Widget *param_newWidget() override;

    ParamType paramType() const override { return ParamType::ITEM; };
    bool param_readSVGValue(char const *strvalue) override;
    Glib::ustring param_getSVGValue() const override;
    Glib::ustring param_getDefaultSVGValue() const override;
    void param_set_default() override;
    void param_update_default(char const *default_value) override;
    void param_set_and_write_default();
    SPObject *param_fork();
    void addCanvasIndicators(SPLPEItem const *lpeitem, std::vector<Geom::PathVector> &hp_vec) override;
    sigc::signal<void ()> signal_item_pasted;
    sigc::signal<void ()> signal_item_changed;
    void linkitem(Glib::ustring itemid);
    SPItem *getObject() const { return ref.getObject(); }

    friend class LPEBool;

    Geom::Affine last_transform;
    bool changed = true; /* this gets set whenever the path is changed (this is set to true, and then the signal_item_changed signal is emitted).
                          * the user must set it back to false if she wants to use it sensibly */

protected:
    char *href = nullptr; // contains link to other object, e.g. "#path2428", NULL if ItemParam contains pathdata itself
    ItemReference ref;
    sigc::connection ref_changed_connection;
    sigc::connection linked_delete_connection;
    sigc::connection linked_modified_connection;
    sigc::connection linked_transformed_connection;
    void ref_changed(SPObject *old_ref, SPObject *new_ref);
    void remove_link();
    void start_listening(SPObject *to);
    void quit_listening();
    void linked_delete(SPObject *deleted);
    void linked_modified(SPObject *linked_obj, unsigned flags);
    void linked_transformed(Geom::Affine const *rel_transf, SPItem *moved_item);
    virtual void linked_modified_callback(SPObject *linked_obj, unsigned flags);
    virtual void linked_transformed_callback(Geom::Affine const *rel_transf, SPItem *moved_item);
    void on_link_button_click();
    void emit_changed();

    char *defvalue;
};

} // namespace Inkscape::LivePathEffect

#endif // INKSCAPE_LIVEPATHEFFECT_PARAMETER_ITEM_H
