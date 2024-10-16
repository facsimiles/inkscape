// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Helper struct to handle libsigc++'s destroy_notify_callback API
 */
/*
 * Author:
 *   Liam P. White
 *
 * Copyright (C) 2024 Liam P. White
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_HELPER_SIGC_NOTIFIABLE_H
#define INKSCAPE_HELPER_SIGC_NOTIFIABLE_H

#include <sigc++/trackable.h>

template <typename T>
class notifiable_wrapper : public sigc::notifiable
{
public:
    notifiable_wrapper(T *t)
        : _t(t)
    {}

    T *get() const
    {
        return _t;
    }

private:
    T *const _t;
};

#endif // INKSCAPE_HELPER_SIGC_NOTIFIABLE_H

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