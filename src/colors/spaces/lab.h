// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_COLORS_SPACES_LAB_H
#define SEEN_COLORS_SPACES_LAB_H

#include "base.h"

namespace Inkscape::Colors::Space {

class Lab : public AnySpace
{
public:
    Lab(): AnySpace(Type::LAB, 3, "Lab", "Lab", "color-selector-lab", true) {
        _intent = RenderingIntent::ABSOLUTE_COLORIMETRIC;
        _intent_priority = 10;
    }
    ~Lab() override = default;

protected:
    friend class Inkscape::Colors::Color;

    std::shared_ptr<Inkscape::Colors::CMS::Profile> const getProfile() const override;
    std::string toString(std::vector<double> const &values, bool opacity) const override;

public:
    class Parser : public Colors::Parser
    {
    public:
        Parser()
            : Colors::Parser("lab", Type::LAB)
        {}
        bool parse(std::istringstream &input, std::vector<double> &output) const override;
    };

    static void toXYZ(std::vector<double> &output);
    static void fromXYZ(std::vector<double> &output);

    static void scaleDown(std::vector<double> &in_out);
    static void scaleUp(std::vector<double> &in_out);
};

} // namespace Inkscape::Colors::Space

#endif // SEEN_COLORS_SPACES_LAB_H
