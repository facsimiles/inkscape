// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef INKSCAPE_HELPER_SCOPED_BLOCK
#define INKSCAPE_HELPER_SCOPED_BLOCK

#include <sigc++/connection.h>
#include <sigc++/scoped_connection.h>

namespace Inkscape {

template <typename T>
    requires std::is_same_v<T, sigc::connection> ||
             std::is_same_v<T, sigc::scoped_connection>
class scoped_block
{
public:
    explicit scoped_block(T &connection)
        : _c{connection}
    {
        _c.block();
    }

    ~scoped_block()
    {
        _c.unblock();
    }

private:
    T &_c;
};

} // namespace Inkscape

#endif // INKSCAPE_HELPER_SCOPED_BLOCK

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
