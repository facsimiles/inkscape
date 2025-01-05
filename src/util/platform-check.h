// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Contains specific methods for platform-specific code: see if you're running GNOME, KDE...
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
