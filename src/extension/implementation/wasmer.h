// SPDX-License-Identifier: GPL-2.0-or-later
/*
    Author:  Ted Gould <ted@gould.cx>
    Copyright (c) 2020

    Released under GNU GPL v2+, read the file 'COPYING' for more information.

    Plugs in WASM based extensions
*/

#ifndef __INKSCAPE_EXTENSION_IMPLEMENTATION_WASMER_H__
#define __INKSCAPE_EXTENSION_IMPLEMENTATION_WASMER_H__ 1

#include "implementation.h"

#include <memory>
#include <wasmer.hh>

namespace Inkscape {
namespace Extension {
namespace Implementation {

class Wasmer : public Implementation {
  public:
    // ----- Constructor / destructor -----
    Wasmer() = default;

    virtual ~Wasmer() = default;

    bool load(Inkscape::Extension::Extension * /*module*/);
    void unload(Inkscape::Extension::Extension * /*module*/);

    bool check(Inkscape::Extension::Extension * /*module*/);

    bool cancelProcessing();
    void effect(Inkscape::Extension::Effect * /*module*/, Inkscape::UI::View::View * /*document*/,
                ImplementationDocumentCache * /*docCache*/);

  private:
    std::shared_ptr<wasmer_instance_t> instance;

    std::string get_module_path(Inkscape::Extension::Extension *module);
};

} // namespace Implementation
} // namespace Extension
} // namespace Inkscape

#endif // __INKSCAPE_EXTENSION_IMPLEMENTATION_WASMER_H__

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
