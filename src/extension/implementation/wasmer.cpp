// SPDX-License-Identifier: GPL-2.0-or-later
/*
    Author:  Ted Gould <ted@gould.cx>
    Copyright (c) 2020

    Released under GNU GPL v2+, read the file 'COPYING' for more information.

    Plugs in WASM based extensions
*/

#include "wasmer.h"

#include "extension/extension.h"
#include "xml/node.h"

#include <glibmm.h>

namespace Inkscape {
namespace Extension {
namespace Implementation {

class WasmerDocCache : public ImplementationDocumentCache {
    std::string xmldoc;

  public:
    WasmerDocCache(Inkscape::UI::View::View *view);
    std::string &getDoc() { return xmldoc; }
};

WasmerDocCache::WasmerDocCache(Inkscape::UI::View::View *view)
    : ImplementationDocumentCache(view)
{
    xmldoc = "";
}

bool Wasmer::load(Inkscape::Extension::Extension *module)
{
    if (this->instance != nullptr) {
        return true;
    }

    auto modulePath = get_module_path(module);
    if (modulePath.empty()) {
        return false;
    }

    auto moduleContent = Glib::file_get_contents(modulePath);
    if (moduleContent.empty()) {
        return false;
    }

    wasmer_instance_t *instance = nullptr;
    auto result = wasmer_instantiate(&instance, (uint8_t *)(moduleContent.c_str()), moduleContent.size(), {}, 0);
    if (result != wasmer_result_t::WASMER_OK) {
        return false;
    }

    this->instance = std::shared_ptr<wasmer_instance_t>(instance, [](void *input) {
        if (input != nullptr) {
            wasmer_instance_destroy(static_cast<wasmer_instance_t *>(input));
        }
    });

    return false;
}

std::string Wasmer::get_module_path(Inkscape::Extension::Extension *module)
{
    if (module == nullptr) {
        return "";
    }

    auto inx = module->get_repr();
    if (inx == nullptr) {
        return "";
    }

    auto moduleNode =
        inx->findChildPath(std::vector<std::string>{ INKSCAPE_EXTENSION_NS "wasm", INKSCAPE_EXTENSION_NS "module" });

    if (moduleNode == nullptr) {
        return "";
    }

    auto modulePath = module->get_dependency_location(moduleNode->content());
    if (!Glib::file_test(modulePath, Glib::FILE_TEST_EXISTS)) {
        return "";
    }

    return modulePath;
}

void Wasmer::unload(Inkscape::Extension::Extension * /*module*/) { this->instance = nullptr; }

bool Wasmer::check(Inkscape::Extension::Extension *module)
{
    auto modulePath = get_module_path(module);
    if (modulePath.empty()) {
        return false;
    }
    return true;
}

void Wasmer::effect(Inkscape::Extension::Effect *module, Inkscape::UI::View::View *doc,
                    ImplementationDocumentCache *docCache)
{
    if (docCache == nullptr) {
        docCache = new WasmerDocCache(doc);
    }

    WasmerDocCache *dc = dynamic_cast<WasmerDocCache *>(docCache);
    if (dc == nullptr) {
        g_warning("Wasmer::effect: Unable to create usable document cache");
        return;
    }

    if (doc == nullptr) {
        g_warning("Wasmer::effect: View not defined");
        return;
    }
}

} // namespace Implementation
} // namespace Extension
} // namespace Inkscape


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
