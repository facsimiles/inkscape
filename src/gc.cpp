/*
 * Inkscape::GC - Wrapper for Boehm GC
 *
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *
 * Copyright (C) 2004 MenTaLguY
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "gc-core.h"
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <glib/gmessages.h>

namespace Inkscape {
namespace GC {

namespace {

void display_warning(char *msg, GC_word arg) {
    g_warning(msg, arg);
}

void *debug_malloc(std::size_t size) {
    return GC_debug_malloc(size, GC_EXTRAS);
}

void *debug_malloc_atomic(std::size_t size) {
    return GC_debug_malloc_atomic(size, GC_EXTRAS);
}

void *debug_malloc_uncollectable(std::size_t size) {
    return GC_debug_malloc_uncollectable(size, GC_EXTRAS);
}

std::ptrdiff_t compute_debug_base_fixup() {
    char *base=reinterpret_cast<char *>(GC_debug_malloc(1, GC_EXTRAS));
    char *real_base=reinterpret_cast<char *>(GC_base(base));
    GC_debug_free(base);
    return base - real_base;
}

inline std::ptrdiff_t const &debug_base_fixup() {
    static std::ptrdiff_t fixup=compute_debug_base_fixup();
    return fixup;
}

void *debug_base(void *ptr) {
    char *base=reinterpret_cast<char *>(GC_base(ptr));
    return base + debug_base_fixup();
}

int debug_general_register_disappearing_link(void **p_ptr, void *base) {
    char *real_base=reinterpret_cast<char *>(base) - debug_base_fixup();
    return GC_general_register_disappearing_link(p_ptr, real_base);
}

void *dummy_base(void *) { return NULL; }

void dummy_register_finalizer(void *, CleanupFunc, void *,
                                      CleanupFunc *old_func, void **old_data)
{
    if (old_func) {
        *old_func = NULL;
    }
    if (old_data) {
        *old_data = NULL;
    }
}

int dummy_general_register_disappearing_link(void **, void *) { return 0; }

int dummy_unregister_disappearing_link(void **link) { return 0; }

Ops enabled_ops = {
    &GC_malloc,
    &GC_malloc_atomic,
    &GC_malloc_uncollectable,
    &GC_base,
    &GC_register_finalizer_ignore_self,
    &GC_general_register_disappearing_link,
    &GC_unregister_disappearing_link,
    &GC_free
};

Ops debug_ops = {
    &debug_malloc,
    &debug_malloc_atomic,
    &debug_malloc_uncollectable,
    &debug_base,
    &GC_debug_register_finalizer_ignore_self,
    &debug_general_register_disappearing_link,
    &GC_unregister_disappearing_link,
    &GC_debug_free
};

Ops disabled_ops = {
    &std::malloc,
    &std::malloc,
    &std::malloc,
    &dummy_base,
    &dummy_register_finalizer,
    &dummy_general_register_disappearing_link,
    &dummy_unregister_disappearing_link,
    &std::free
};

class InvalidGCModeError : public std::runtime_error {
public:
    InvalidGCModeError(const char *mode)
    : runtime_error(std::string("Unknown GC mode \"") + mode + "\"")
    {}
};

Ops const &get_ops() throw (InvalidGCModeError) {
    char *mode_string=std::getenv("_INKSCAPE_GC");
    if (mode_string) {
        if (!std::strcmp(mode_string, "enable")) {
            return enabled_ops;
        } else if (!std::strcmp(mode_string, "debug")) {
            return debug_ops;
        } else if (!std::strcmp(mode_string, "disable")) {
            return disabled_ops;
        } else {
            throw InvalidGCModeError(mode_string);
        }
    } else {
        return enabled_ops;
    }
}

}

Ops ops = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

void init() {
    try {
        ops = get_ops();
    } catch (InvalidGCModeError &e) {
        g_warning("%s; enabling normal collection", e.what());
        ops = enabled_ops;
    }

    if ( ops.malloc != std::malloc ) {
        GC_no_dls = 1;
        GC_all_interior_pointers = 1;
        GC_finalize_on_demand = 0;

        GC_INIT();

        GC_set_free_space_divisor(8);
        GC_set_warn_proc(&display_warning);
    }
}

}
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=c++:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
