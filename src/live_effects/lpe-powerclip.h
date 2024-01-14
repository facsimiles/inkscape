// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_LPE_POWERCLIP_H
#define INKSCAPE_LPE_POWERCLIP_H

/*
 * Inkscape::LPEPowerClip
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "live_effects/effect.h"
#include "live_effects/parameter/message.h"
#include "helper/auto-connection.h"

namespace Inkscape {
namespace LivePathEffect {

class LPEPowerClip : public Effect {
public:
    LPEPowerClip(LivePathEffectObject *lpeobject);
    ~LPEPowerClip() override;
    void doBeforeEffect (SPLPEItem const* lpeitem) override;
    Geom::PathVector doEffect_path (Geom::PathVector const & path_in) override;
    void doOnRemove(SPLPEItem const* /*lpeitem*/) override;
    void doOnVisibilityToggled(SPLPEItem const* lpeitem) override;
    void modified(auto, unsigned flags);
    Glib::ustring getId();
    void add();
    void upd();
    void del();
    Geom::PathVector getClipPathvector();

  private:
    BoolParam inverse;
    BoolParam flatten;
    BoolParam hide_clip;
    MessageParam message;
    auto_connection modified_connection;
    bool _updating;
    bool _legacy;
};

void sp_remove_powerclip(Inkscape::Selection *sel);
void sp_inverse_powerclip(Inkscape::Selection *sel);

} //namespace LivePathEffect
} //namespace Inkscape
#endif
