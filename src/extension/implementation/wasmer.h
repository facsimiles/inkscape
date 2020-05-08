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
    Wasmer() = default;
    virtual ~Wasmer() = default;

    virtual std::shared_ptr<ImplementationDocumentCache> newDocCache(Inkscape::UI::View::View *doc) override;

    bool load(Inkscape::Extension::Extension * /*module*/) override;
    void unload(Inkscape::Extension::Extension * /*module*/) override;

    bool check(Inkscape::Extension::Extension * /*module*/) override;

    void effect(Inkscape::Extension::Effect *, std::shared_ptr<ImplementationDocumentCache>) override;

  private:
    std::string moduleContent;

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
