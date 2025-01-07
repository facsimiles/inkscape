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
