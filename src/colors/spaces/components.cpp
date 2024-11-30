// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Meta data about color channels and how they are presented to users.
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2023 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "components.h"

#include <algorithm>
#include <cmath>
#include <libintl.h> // avoid glib include
#include <map>
// #include <utility>
#include <glib/gi18n.h>
#include <glibmm/value.h>

#include "enum.h"

namespace Inkscape::Colors::Space {

static const std::vector<Components> get_color_spaces() {

static const std::vector<Components> color_spaces = {
    {
        Type::RGB, Type::RGB, Traits::Picker,
        {
            { "r", _("_R"), _("Red"), 255, Component::Unit::None },
            { "g", _("_G"), _("Green"), 255, Component::Unit::None },
            { "b", _("_B"), _("Blue"), 255, Component::Unit::None }
        }
    },
    {
        Type::linearRGB, Type::NONE, Traits::Internal,
        {
            { "r", _("<sub>l</sub>_R"), _("Linear Red"), 255, Component::Unit::None },
            { "g", _("<sub>l</sub>_G"), _("Linear Green"), 255, Component::Unit::None },
            { "b", _("<sub>l</sub>_B"), _("Linear Blue"), 255, Component::Unit::None }
        }
    },
    {
        Type::HSL, Type::HSL, Traits::Picker,
        {
            { "h", _("_H"), _("Hue"), 360, Component::Unit::Degree },
            { "s", _("_S"), _("Saturation"), 100, Component::Unit::Percent },
            { "l", _("_L"), _("Lightness"), 100, Component::Unit::Percent }
        }
    },
    {
        Type::HSV, Type::HSV, Traits::Picker,
        {
            { "h", _("_H"), _("Hue"), 360, Component::Unit::Degree },
            { "s", _("_S"), _("Saturation"), 100, Component::Unit::Percent },
            { "v", _("_V"), _("Value"), 100, Component::Unit::Percent }
        }
    },
    {
        Type::CMYK, Type::NONE, Traits::Picker,
        {
            { "c", _("_C"), C_("CMYK", "Cyan"), 100, Component::Unit::Percent },
            { "m", _("_M"), C_("CMYK", "Magenta"), 100, Component::Unit::Percent },
            { "y", _("_Y"), C_("CMYK", "Yellow"), 100, Component::Unit::Percent },
            { "k", _("_K"), C_("CMYK", "Black"), 100, Component::Unit::Percent }
        }
    },
    {
        Type::CMY, Type::NONE, Traits::Picker,
        {
            { "c", _("_C"), C_("CMYK", "Cyan"), 100, Component::Unit::Percent },
            { "m", _("_M"), C_("CMYK", "Magenta"), 100, Component::Unit::Percent },
            { "y", _("_Y"), C_("CMYK", "Yellow"), 100, Component::Unit::Percent },
        }
    },
    {
        Type::HSLUV, Type::HSLUV, Traits::Picker,
        {
            { "h", _("_H*"), _("Hue"), 360, Component::Unit::Degree },
            { "s", _("_S*"), _("Saturation"), 100, Component::Unit::Percent },
            { "l", _("_L*"), _("Lightness"), 100, Component::Unit::Percent }
        }
    },
    {
        Type::OKHSL, Type::OKHSL, Traits::Picker,
        {
            { "h", _("_H<sub>ok</sub>"), _("Hue"), 360, Component::Unit::Degree },
            { "s", _("_S<sub>ok</sub>"), _("Saturation"), 100, Component::Unit::Percent },
            { "l", _("_L<sub>ok</sub>"), _("Lightness"), 100, Component::Unit::Percent }
        }
    },
    {
        Type::OKHSV, Type::OKHSV, Traits::Internal,
        {
            { "h", _("_H<sub>ok</sub>"), _("Hue"), 360, Component::Unit::Degree },
            { "s", _("_S<sub>ok</sub>"), _("Saturation"), 100, Component::Unit::Percent },
            { "v", _("_V<sub>ok</sub>"), _("Value"), 100, Component::Unit::Percent }
        }
    },
    {
        Type::LCH, Type::NONE, Traits::Internal,
        {
            { "l", _("_L"), _("Luminance"), 255, Component::Unit::None },
            { "c", _("_C"), _("Chroma"), 255, Component::Unit::None },
            { "h", _("_H"), _("Hue"), 360, Component::Unit::Degree },
        }
    },
    {
        Type::LUV, Type::NONE, Traits::Internal,
        {
            { "l", _("_L"), _("Luminance"), 100, Component::Unit::Percent },
            { "u", _("_U"), _("Chroma U"), 100, Component::Unit::Percent },
            { "v", _("_V"), _("Chroma V"), 100, Component::Unit::Percent },
        }
    },
    {
        Type::OKLAB, Type::NONE, Traits::Internal,
        {
            { "l", _("_L<sub>ok</sub>"), _("Lightness"), 100, Component::Unit::Percent },
            { "a", _("_A<sub>ok</sub>"), _("Component A"), 100, Component::Unit::Percent },
            { "b", _("_B<sub>ok</sub>"), _("Component B"), 100, Component::Unit::Percent }
        }
    },
    {
        Type::OKLCH, Type::OKHSL, Traits::Picker,
        {
            { "l", _("_L<sub>ok</sub>"), _("Lightness"), 100, Component::Unit::Percent },
            { "c", _("_C<sub>ok</sub>"), _("Chroma"), 40, Component::Unit::None }, //TODO: 100% is 0.4
            { "h", _("_H<sub>ok</sub>"), _("Hue"), 360, Component::Unit::Degree }
        }
    },
    {
        Type::LAB, Type::NONE, Traits::Internal,
        {
            { "l", _("_L"), _("Lightness"), 100, Component::Unit::Percent },
            { "a", _("_A"), _("Component A"), 255, Component::Unit::None },
            { "b", _("_B"), _("Component B"), 255, Component::Unit::None }
        }
    },
    {
        Type::YCbCr, Type::NONE, Traits::CMS,
        {
            { "y", _("_Y"), _("Y"), 255, Component::Unit::None },
            { "cb", _("C_r"), _("Cb"), 255, Component::Unit::None },
            { "cr", _("C_b"), _("Cr"), 255, Component::Unit::None }
        }
    },
    {
        Type::XYZ, Type::NONE, Traits::Internal,
        {
            { "x", "_X", "X", 255, Component::Unit::None },
            { "y", "_Y", "Y", 100, Component::Unit::None },
            { "z", "_Z", "Z", 255, Component::Unit::None }
        }
    },
    {
        Type::YXY, Type::NONE, Traits::Internal,
        {
            { "y1", "_Y", "Y", 255, Component::Unit::None },
            { "x", "_x", "x", 255, Component::Unit::None },
            { "y2", "y", "y", 255, Component::Unit::None }
        }
    },
    {
        Type::Gray, Type::NONE, Traits::Internal,
        {
            { "gray", _("G"), _("Gray"), 1024, Component::Unit::None }
        }
    }
};
    return color_spaces;
}


Component::Component(Type type, unsigned int index, std::string id, std::string name, std::string tip, unsigned scale, Component::Unit unit)
    : type(type)
    , index(index)
    , id(std::move(id))
    , name(std::move(name))
    , tip(std::move(tip))
    , scale(scale)
    , unit(unit)
{}

Component::Component(std::string id, std::string name, std::string tip, unsigned scale, Component::Unit unit)
    : type(Type::NONE)
    , index(-1)
    , id(std::move(id))
    , name(std::move(name))
    , tip(std::move(tip))
    , scale(scale)
    , unit(unit)
{}

/**
 * Clamp the value to between 0.0 and 1.0, except for hue which is wrapped around.
 */
double Component::normalize(double value) const
{
    if (scale == 360 && (value < 0.0 || value > 1.0)) {
        return value - std::floor(value);
    }
    return std::clamp(value, 0.0, 1.0);
}

void Components::add(std::string id, std::string name, std::string tip, unsigned scale, Component::Unit unit)
{
    _components.emplace_back(_type, _components.size(), std::move(id), std::move(name), std::move(tip), scale, unit);
}

std::map<Type, Components> _build(bool alpha)
{
    std::map<Type, Components> sets;
    for (auto& components : get_color_spaces()) {
        unsigned int index = 0;
        for (auto& component : components.getAll()) {
            // patch components
            auto& rw = const_cast<Component&>(component);
            rw.type = components.getType();
            rw.index = index++;
        }
        sets[components.getType()] = components;
    }

    if (alpha) {
        for (auto &[key, val] : sets) {
            // alpha component with unique ID, so it doesn't clash with "a" in Lab
            val.add("alpha", C_("Transparency (alpha)", "_A"), _("Alpha"), 100, Component::Unit::Percent);
        }
    }
    return sets;
}

Components const &Components::get(Type space, bool alpha)
{
    static std::map<Type, Components> sets_no_alpha = _build(false);
    static std::map<Type, Components> sets_with_alpha = _build(true);

    auto &lookup_set = alpha ? sets_with_alpha : sets_no_alpha;
    if (auto search = lookup_set.find(space); search != lookup_set.end()) {
        return search->second;
    }
    return lookup_set[Type::NONE];
}

Type Components::get_wheel_type() const {
    return _wheel_type;
}

}; // namespace Inkscape::Colors::Space
