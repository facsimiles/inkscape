// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors: see git history
 *
 * Copyright (C) 2025 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_COLORS_CMS_TRANSFORM_CAIRO_H
#define SEEN_COLORS_CMS_TRANSFORM_CAIRO_H

#include <cairomm/surface.h>
#include <memory>

#include "transform.h"

#include "colors/spaces/enum.h"

namespace Inkscape::Colors::CMS {

class Profile;
class TransformCairo : public Transform
{
public:
    TransformCairo(
                   std::shared_ptr<Profile> const &from,
                   std::shared_ptr<Profile> const &to,
                   std::shared_ptr<Profile> const &proof = nullptr,
                   RenderingIntent proof_intent = RenderingIntent::AUTO,
                   bool with_gamut_warn = false);

    void do_transform(cairo_surface_t *in, cairo_surface_t *out) const;
    void do_transform(Cairo::RefPtr<Cairo::ImageSurface> &in, Cairo::RefPtr<Cairo::ImageSurface> &out) const;

    void set_gamut_warn(std::vector<double> const &input);

    static std::vector<float> splice(std::vector<float *> inputs, int width, int height, int channels);
    static void premultiply(float *input, int width, int height, int channels = 3);
private:
    int _pixel_size_in;
    int _pixel_size_out;
};

} // namespace Inkscape::Colors::CMS

#endif // SEEN_COLORS_CMS_TRANSFORM_CAIRO_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
