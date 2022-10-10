// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::Util::format - g_strdup_printf wrapper producing shared strings
 *
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *
 * Copyright (C) 2006 MenTaLguY
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_UTIL_FORMAT_H
#define SEEN_INKSCAPE_UTIL_FORMAT_H

#include <cstdarg>
#include <glib.h>

namespace Inkscape {

namespace Util {

inline int vsnformat(char *str, size_t n, char const *format, va_list args) {
    return g_vsnprintf(str, n, format, args);
}

// needed since G_GNUC_PRINTF can only be used on a declaration
int snformat(char *str, size_t n, char const *format, ...) G_GNUC_PRINTF(3,4);
inline int snformat(char *str, size_t n, char const *format, ...) {
    va_list args;

    va_start(args, format);
    auto result = vsnformat(str, n, format, args);
    va_end(args);

    return result;
}

}

}

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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
