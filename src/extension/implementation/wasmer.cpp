// SPDX-License-Identifier: GPL-2.0-or-later
/*
    Author:  Ted Gould <ted@gould.cx>
    Copyright (c) 2020

    Released under GNU GPL v2+, read the file 'COPYING' for more information.

    Plugs in WASM based extensions
*/

#include "wasmer.h"

#include "extension/effect.h"
#include "extension/extension.h"
#include "object/sp-namedview.h"
#include "ui/view/view.h"
#include "xml/node.h"
#include "xml/repr.h"

#include <glibmm.h>

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

class wasmer_error : std::exception {
    std::string _error;

  public:
    wasmer_error(const std::string &explainer = "Wasmer error")
    {
        int error_len = wasmer_last_error_length();
        char *error_str = (char *)malloc(error_len);
        wasmer_last_error_message(error_str, error_len);
        _error = explainer + ": " + error_str;
        free(error_str);
    }
    virtual const char *what() const noexcept override { return _error.c_str(); }
    static void check(wasmer_result_t result, const std::string &text)
    {
        if (result != wasmer_result_t::WASMER_OK) {
            throw wasmer_error{ text };
        }
    }
};

std::shared_ptr<wasmer_instance_t> wasmerInstance(std::string &moduleContent)
{
    if (moduleContent.empty()) {
        throw std::runtime_error{ "Module content empty" };
    }

    std::array<wasmer_import_t, 0> imports;

    wasmer_instance_t *instancep;
    auto result = wasmer_instantiate(&instancep, (uint8_t *)(moduleContent.c_str()), moduleContent.size(),
                                     imports.data(), imports.size());
    wasmer_error::check(result, "Wasmer unable to create instance");

    auto instance = std::shared_ptr<wasmer_instance_t>(instancep, [](void *input) {
        if (input) {
            wasmer_instance_destroy(static_cast<wasmer_instance_t *>(input));
        }
    });

    return instance;
}

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
        auto inst = wasmerInstance(moduleContent);

        wasmer_exports_t *exports;
        wasmer_instance_exports(inst.get(), &exports);
        for (auto i = 0; i < wasmer_exports_len(exports); i++) {
            auto exp = wasmer_exports_get(exports, i);
            auto namebytes = wasmer_export_name(exp);
            const std::string name{ (const char *)namebytes.bytes, namebytes.bytes_len };
            printf("Export: %s\n", name.c_str());
        }

        /* Run script */
        std::array<wasmer_value_t, 3> params{
            wasmer_value_t{ .tag = wasmer_value_tag::WASM_I32, .value = wasmer_value{ .I32 = 8 } }, // no clue what this
                                                                                                    // is
            wasmer_value_t{ .tag = wasmer_value_tag::WASM_I32, .value = wasmer_value{ .I32 = 0 } },
            // wasmer_value_t{ .tag = wasmer_value_tag::WASM_I32, .value = wasmer_value{ .I32 = (int)dc->doc().length()
            // } }
            wasmer_value_t{ .tag = wasmer_value_tag::WASM_I32, .value = wasmer_value{ .I32 = 0 } }
        };
        std::array<wasmer_value_t, 0> returns;

        auto result = wasmer_instance_call(inst.get(), "inkscape_effect", params.data(), params.size(), returns.data(),
                                           returns.size());
        wasmer_error::check(result, "Wasmer instance execution error");


        auto ctx = wasmer_instance_context_get(inst.get());

        auto retmem = wasmer_instance_context_memory(ctx, 0);
        auto retdata = wasmer_memory_data(retmem);

        auto intdata = (unsigned int *)retdata;

        /* TODO: Why not 0, 1? */
        auto addr = intdata[2];
        auto len = intdata[3];

        if (len == 0) {
            throw std::runtime_error{ "Returned zero length string" };
        }
        if (addr + len > wasmer_memory_data_length((wasmer_memory_t *)retmem)) {
            throw std::runtime_error{ "Memory out of range (addr: " + std::to_string(addr) +
                                      ", len: " + std::to_string(len) };
        }

        printf("Address: %d\n", addr);
        printf("Length:  %d\n", len);

        const std::string data{ (const char *)&retdata[addr], len };
        printf("Data:    %s\n", data.c_str());

        auto newdoc = std::shared_ptr<SPDocument>(SPDocument::createNewDocFromMem(data.c_str(), data.size(), true),
                                                  [](void *doc) {
                                                      if (doc) {
                                                          static_cast<SPDocument *>(doc)->release();
                                                      }
                                                  });
        if (newdoc) {
            // replace_document(dc->view(), newdoc.get());
        } else {
            throw std::runtime_error{ "Unable to build document" };
        }
    } catch (std::exception const &e) {
        g_warning("Wasmer Execution Failure: %s", e.what());
    } catch (wasmer_error const &e) {
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
