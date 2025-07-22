// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2023 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "rgb.h"

#include <sstream>

#include "colors/cms/profile.h"
#include "colors/color.h"
#include "colors/utils.h"

namespace Inkscape::Colors::Space {

/**
 * Return the RGB color profile, this is static for all RGB sub-types
 */
std::shared_ptr<Inkscape::Colors::CMS::Profile> const RGB::getProfile() const
{
    static std::shared_ptr<Colors::CMS::Profile> srgb_profile;
    if (!srgb_profile) {
        srgb_profile = Colors::CMS::Profile::create_srgb();
    }
    return srgb_profile;
}

RGB::RGB(Type type, int components, std::string name, std::string shortName, std::string icon, bool spaceIsUnbounded):
    AnySpace(type, components, std::move(name), std::move(shortName), std::move(icon), spaceIsUnbounded) {
}

/**
 * Print the RGB color to a CSS Hex code of 6 or 8 digits.
 *
 * @arg values - A vector of doubles for each channel in the RGB space
 * @arg opacity - True if the opacity should be included in the output.
 */
std::string RGB::toString(std::vector<double> const &values, bool opacity) const
{
    return rgba_to_hex(toRGBA(values), values.size() == 4 && opacity);
}

/**
 * Parse RGB css values
 */
bool RGB::Parser::parse(std::istringstream &ss, std::vector<double> &output) const
{
    bool end = false;
    return append_css_value(ss, output, end, ',', 255)                    // Red
           && append_css_value(ss, output, end, ',', 255)                 // Green
           && append_css_value(ss, output, end, !_alpha ? '/' : ',', 255) // Blue
           && (append_css_value(ss, output, end) || !_alpha)              // Opacity
           && end;
}

} // namespace Inkscape::Colors::Space
