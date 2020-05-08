// SPDX-License-Identifier: GPL-2.0-or-later
/*
    Author:  Ted Gould <ted@gould.cx>
    Copyright (c) 2020

    Released under GNU GPL v2+, read the file 'COPYING' for more information.

    Plugs in WASM based extensions
*/

#include "wasmer.h"

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

    auto modulePath = get_module_path(module);
    if (modulePath.empty()) {
        return false;
    }

    moduleContent = Glib::file_get_contents(modulePath);
    if (moduleContent.empty()) {
        return false;
    }

    return false;
}

/**
 * \brief looks through the INX file to find the path to the WASM module
 */
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

/**
 * \brief clears the instance causing it to be destroyed
 */
void Wasmer::unload(Inkscape::Extension::Extension * /*module*/) { this->moduleContent.clear(); }

/**
 * \brief ensures that there is a module we can find
 */
bool Wasmer::check(Inkscape::Extension::Extension *module)
{
    auto modulePath = get_module_path(module);
    if (modulePath.empty()) {
        return false;
    }
    return true;
}

std::shared_ptr<wasmer_memory_t> wasmerMemory(const void *data, unsigned int size)
{
    /* Allocate memory for our string */
    wasmer_memory_t *pmem = nullptr;
    auto memres = wasmer_memory_new(
        &pmem, wasmer_limits_t{ .min = size, .max = wasmer_limit_option_t{ .has_some = true, .some = size } });

    if (memres == wasmer_result_t::WASMER_ERROR) {
        g_warning("WASMER FAIL");
        return nullptr;
    }

    auto mdata = wasmer_memory_data(pmem);
    memcpy(mdata, data, size);

    return std::shared_ptr<wasmer_memory_t>(pmem, [](void *inptr) {
        if (inptr != nullptr) {
            wasmer_memory_destroy(static_cast<wasmer_memory_t *>(inptr));
        }
    });
}

const std::string moduleName{ "inkscape" };
const std::string importName{ "document" };

std::shared_ptr<wasmer_instance_t> wasmerInstance(std::string &moduleContent,
                                                  std::shared_ptr<wasmer_memory_t> &strmemory)
{

    std::array<wasmer_import_t, 1> imports = { wasmer_import_t{
        .module_name = wasmer_byte_array{ .bytes = (const unsigned char *)moduleName.c_str(),
                                          .bytes_len = (unsigned int)moduleName.length() },
        .import_name = wasmer_byte_array{ .bytes = (const unsigned char *)importName.c_str(),
                                          .bytes_len = (unsigned int)importName.length() },
        .tag = wasmer_import_export_kind::WASM_MEMORY,
        .value = wasmer_import_export_value{ .memory = &(*strmemory) } } };

    wasmer_instance_t *instancep = nullptr;
    auto result = wasmer_instantiate(&instancep, (uint8_t *)(moduleContent.c_str()), moduleContent.size(),
                                     imports.data(), imports.size());
    if (result != wasmer_result_t::WASMER_OK) {
        return nullptr;
    }

    auto instance = std::shared_ptr<wasmer_instance_t>(instancep, [](void *input) {
        if (input != nullptr) {
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
    if (dc == nullptr) {
        g_warning("Wasmer::effect: Unable to create usable document cache");
        return;
    }

    /* Allocate memory for our string */
    auto mem = wasmerMemory(dc->doc().c_str(), dc->doc().length() + 1);
    auto inst = wasmerInstance(moduleContent, mem);

    /* Run script */
    std::array<wasmer_value_t, 2> params{
        wasmer_value_t{ .tag = wasmer_value_tag::WASM_I32, .value = wasmer_value{ .I32 = 0 } },
        wasmer_value_t{ .tag = wasmer_value_tag::WASM_I32, .value = wasmer_value{ .I32 = (int)dc->doc().length() } }
    };
    std::array<wasmer_value_t, 2> returns{ 0 };

    auto result = wasmer_instance_call(inst.get(), "inkscape_effect", params.data(), params.size(), returns.data(),
                                       returns.size());
    if (result == wasmer_result_t::WASMER_ERROR) {
        g_warning("WASMER FAIL");
        return;
    }

    if (returns[0].tag != wasmer_value_tag::WASM_I32) {
        return;
    }
    if (returns[1].tag != wasmer_value_tag::WASM_I32) {
        return;
    }

    auto addr = returns[0].value.I32;
    auto len = returns[1].value.I32;

    auto ctx = wasmer_instance_context_get(inst.get());

    auto retmem = wasmer_instance_context_memory(ctx, 0);
    auto retdata = wasmer_memory_data(retmem);
    retdata += addr;

    auto newdoc = SPDocument::createNewDocFromMem((const char *)retdata, len, true);
    if (newdoc) {
        replace_document(dc->view(), newdoc);
        newdoc->release();
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
