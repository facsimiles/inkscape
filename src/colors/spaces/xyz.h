// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLORS_SPACES_XYZ_H
#define SEEN_COLORS_SPACES_XYZ_H

#include "base.h"

namespace Inkscape::Colors::Space {

class XYZ : public AnySpace
{
public:
    XYZ(): AnySpace(Type::XYZ, 3, "XYZ", "XYZ", "color-selector-xyz", true) {
        _intent = RenderingIntent::RELATIVE_COLORIMETRIC_NOBPC;
        _intent_priority = 10;
    }
    ~XYZ() override = default;

    unsigned int getComponentCount() const override { return 3; }

protected:
    friend class Inkscape::Colors::Color;

    XYZ(Type type, int components, std::string name, std::string shortName, std::string icon, bool spaceIsUnbounded = false);

    std::shared_ptr<Inkscape::Colors::CMS::Profile> const getProfile() const override;
    std::string toString(std::vector<double> const &values, bool opacity = true) const override { return _toString(values, opacity, false); }
    std::string _toString(std::vector<double> const &values, bool opacity, bool d50) const;
};

class XYZ50 : public XYZ
{
public:
    XYZ50(): XYZ(Type::XYZ50, 3, "XYZ D50", "XYZ D50", "color-selector-xyz", true) {}
    ~XYZ50() override = default;

protected:
    friend class Inkscape::Colors::Color;

    XYZ50(Type type, int components, std::string name, std::string shortName, std::string icon, bool spaceIsUnbounded = false);

    std::shared_ptr<Inkscape::Colors::CMS::Profile> const getProfile() const override;
    std::string toString(std::vector<double> const &values, bool opacity = true) const override { return _toString(values, opacity, true); }
};

} // namespace Inkscape::Colors::Space

#endif // SEEN_COLORS_SPACES_XYZ_H
