// SPDX-License-Identifier: GPL-2.0-or-later
/*
    Author:  Ted Gould <ted@gould.cx>
    Copyright (c) 2020

    Released under GNU GPL v2+, read the file 'COPYING' for more information.

    Plugs in WASM based extensions
*/

#include "wasmer.h"
#include "wasmer-wrap.h"

#include "extension/effect.h"
#include "extension/extension.h"
#include "object/sp-namedview.h"
#include "ui/view/view.h"
#include "xml/node.h"
#include "xml/repr.h"

#include <glibmm.h>
#include <numeric>

namespace Inkscape {
namespace Extension {
namespace Implementation {

/**
 * \brief Makes it so that we only turn the document into a string once
 */
class WasmerDocCache : public ImplementationDocumentCache {
    Glib::ustring xmldoc;

  public:
    WasmerDocCache(Inkscape::UI::View::View *view);
    Glib::ustring &doc() { return xmldoc; }
};

/**
 * \brief turns a View into a string
 */
WasmerDocCache::WasmerDocCache(Inkscape::UI::View::View *view)
    : ImplementationDocumentCache(view)
{
    xmldoc = sp_repr_save_buf(view->doc()->getReprDoc());
}

/**
 * \brief create an instance of WasmerDocCache
 */
std::shared_ptr<ImplementationDocumentCache> Wasmer::newDocCache(Inkscape::UI::View::View *doc)
{
    return std::make_shared<WasmerDocCache>(doc);
}

/**
 * \brief finds the module and creates an instance with that module
 */
bool Wasmer::load(Inkscape::Extension::Extension *module)
{
    if (!moduleContent.empty()) {
        return true;
    }

    if (!moduleDep) {
        moduleDep = build_dep(module);
    }

    if (!moduleDep) {
        return false;
    }

    if (!moduleDep->check()) {
        return false;
    }

    auto modulePath = moduleDep->get_path();

    moduleContent = Glib::file_get_contents(modulePath);
    if (moduleContent.empty()) {
        return false;
    }

    return true;
}

/**
 * \brief looks through the INX file to find the path to the WASM module
 */
std::shared_ptr<Inkscape::Extension::Dependency> Wasmer::build_dep(Inkscape::Extension::Extension *module)
{
    if (!module) {
        return {};
    }

    auto inx = module->get_repr();
    if (!inx) {
        return {};
    }

    auto moduleNode =
        inx->findChildPath(std::vector<std::string>{ INKSCAPE_EXTENSION_NS "wasm", INKSCAPE_EXTENSION_NS "module" });

    if (!moduleNode) {
        return {};
    }

    return std::make_shared<Inkscape::Extension::Dependency>(moduleNode, module,
                                                             Inkscape::Extension::Dependency::TYPE_FILE);
}

/**
 * \brief clears the instance causing it to be destroyed
 */
void Wasmer::unload(Inkscape::Extension::Extension * /*module*/) { this->moduleContent.clear(); }

/**
 * \brief ensures that there is a module we can find
 */
bool Wasmer::check(Inkscape::Extension::Extension *module)
{
    if (!moduleDep) {
        moduleDep = build_dep(module);
    }

    if (!moduleDep) {
        return false;
    }

    return moduleDep->check();
}

const std::string inkscape_effect{ "inkscape_effect" };

/**
 * \brief Calls the 'inkscape_effect' function in the module
 */
void Wasmer::effect(Inkscape::Extension::Effect *module, std::shared_ptr<ImplementationDocumentCache> docCache)
{
    auto dc = std::dynamic_pointer_cast<WasmerDocCache>(docCache);
    if (!dc) {
        g_warning("Wasmer::effect: Unable to create usable document cache");
        return;
    }

    try {
        auto premem = std::make_shared<wasmer::memory>(dc->doc().size());
        auto inst = std::make_shared<wasmer::instance>(moduleContent, premem);
        auto mem = inst->mem();

        auto [retstringaddr, retstringhandle] = inst->heapAllocate(sizeof(int32_t) * 2);
        auto [docaddr, dochandle] = inst->heapAllocate(dc->doc().size());

        memcpy(mem->ptr(docaddr), dc->doc().c_str(), dc->doc().size());

        /* Run script */
        inst->call<std::tuple<>>(inkscape_effect, retstringaddr, docaddr, int32_t(dc->doc().size()));

        auto addr = mem->data(retstringaddr);
        auto len = mem->data(retstringaddr + sizeof(int32_t));

        if (len == 0) {
            throw std::runtime_error{ "Returned zero length string" };
        }
        if (addr + len > mem->size()) {
            throw std::runtime_error{ "Memory out of range (addr: " + std::to_string(addr) +
                                      ", len: " + std::to_string(len) };
        }


        const std::string data{ (const char *)mem->ptr(addr), (size_t)len };

        auto newdoc = std::shared_ptr<SPDocument>(SPDocument::createNewDocFromMem(data.c_str(), data.size(), true),
                                                  [](void *doc) {
                                                      if (doc) {
                                                          static_cast<SPDocument *>(doc)->release();
                                                      }
                                                  });
        if (newdoc) {
            replace_document(dc->view(), newdoc.get());
        } else {
            throw std::runtime_error{ "Unable to build document" };
        }
    } catch (std::exception const &e) {
        g_warning("Wasmer Execution Failure: %s", e.what());
    } catch (wasmer::error const &e) {
        g_warning("Wasmer Execution Failure: %s", e.what());
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
