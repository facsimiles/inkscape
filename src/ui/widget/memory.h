// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Memory statistics dialog
 */
/* Authors:
 *     MenTaLguY <mental@rydia.net>
 *
 * Copyright 2005 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */


#include <gtkmm/box.h>

namespace Inkscape::UI::Widget {

class Memory : public Gtk::Box
{
public:
    Memory();
    ~Memory();

protected:
    void apply();

private:
    struct Private;
    std::unique_ptr<Private> _private;
};

} // namespace Inkscape::UI::Widget

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
