#include "platform-check.h"

#include <cstdlib>
#include <cstring>

namespace Inkscape::Util {

bool PlatformCheck::is_gnome() {
    auto desktop_session = getenv("DESKTOP_SESSION");
    return strcmp(desktop_session, "gnome") ||
           strcmp(desktop_session, "ubuntu-desktop") ||
           strcmp(desktop_session, "pantheon");
}

}
