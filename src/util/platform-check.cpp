// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Inkscape platform-checking code
 */
/* Authors:
 *   Kaixoo
 *
 * Copyright (C) 2004-2024 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "platform-check.h"

#include <cstdlib>
#include <cstring>

namespace Inkscape::Util {

bool PlatformCheck::is_gnome() {
    auto desktop_session = getenv("DESKTOP_SESSION");
    return strcmp(desktop_session, "gnome") == 0 ||
           strcmp(desktop_session, "ubuntu-desktop") == 0 ||
           strcmp(desktop_session, "pantheon") == 0;
}

}
