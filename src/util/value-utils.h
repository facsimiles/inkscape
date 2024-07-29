// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_UTIL_VALUE_UTILS_H
#define INKSCAPE_UTIL_VALUE_UTILS_H

/**
 * @file
 * Wrapper API for GValue. Improves upon Glib::Value by
 * - relaxing the constraint that custom types must be default-constructible,
 * - eliminating pointless copies forced by the Glib::Value API,
 * - single-line construction and testing of values,
 * - thread-safe type registration.
 */

#include <glibmm/utility.h>
#include <glibmm/value.h>

namespace Inkscape::Util::GlibValue {

template <class T>
GType type()
{
    static auto const type = [] {
        auto name = std::string{"inkscape_glibvalue_"};
        Glib::append_canonical_typename(name, typeid(T).name());

        return g_boxed_type_register_static(
            name.c_str(),
            +[] (void *p) -> void * {
                return new (std::nothrow) T{*reinterpret_cast<T *>(p)};
            },
            +[] (void *p) {
                delete reinterpret_cast<T *>(p);
            }
        );
    }();
    return type;
}

template <typename T>
bool holds(GValue const *value)
{
    return G_VALUE_HOLDS(value, type<T>());
}

template <typename T>
T *get(GValue const *value)
{
    if (holds<T>(value)) {
        return reinterpret_cast<T *>(g_value_get_boxed(value));
    }
    return nullptr;
}

template <typename T>
bool holds(Glib::ValueBase const &value)
{
    return holds<T>(value.gobj());
}

template <typename T>
T *get(Glib::ValueBase const &value)
{
    return get<T>(value.gobj());
}

template <typename T>
Glib::ValueBase own(T *t)
{
    Glib::ValueBase value;
    value.init(type<T>());
    g_value_set_boxed(value.gobj(), t);
    return value;
}

template <typename T, typename... Args>
Glib::ValueBase create(Args&&... args)
{
    return own(new T(std::forward<Args>(args)...));
}

inline GValue release(Glib::ValueBase &&value)
{
    return std::exchange(*value.gobj(), GValue(G_VALUE_INIT));
}

} // namespace Inkscape::Util::GlibValue

#endif // INKSCAPE_UTIL_VALUE_UTILS_H
