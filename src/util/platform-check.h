// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Contains specific methods for platform-specific code: see if you're running GNOME, KDE...
 */
/* Authors:
 *   Kaixoo
 *
 * Copyright (C) 2004-2024 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef INKSCAPE_UTIL_PLATFORM_CHECK_H
#define INKSCAPE_UTIL_PLATFORM_CHECK_H

namespace Inkscape::Util {

class PlatformCheck {
public:
    static bool is_gnome();
};

} // namespace Inkscape::Util::PlatformCheck

#endif // INKSCAPE_UTIL_PLATFORM_CHECK_H
