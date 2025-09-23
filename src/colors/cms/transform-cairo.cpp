// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Convert cairo surfaces between various color spaces
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2024-2025 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "transform-cairo.h"
#include "profile.h"
#include "../color.h"

#include <boost/range/adaptor/reversed.hpp>
#include <cairo.h>
#include <string>
#include <iostream>

namespace Inkscape::Colors::CMS {

/**
 * Convert from cairo memory format to lcms2 memory format
 */
static int get_memory_format(cairo_surface_t *in)
{
    switch (cairo_image_surface_get_format(in)) {
        case CAIRO_FORMAT_ARGB32:
            return TYPE_ARGB_8_PREMUL;
        case CAIRO_FORMAT_RGB24:
            return TYPE_RGB_8;
        case CAIRO_FORMAT_A8:
            return TYPE_GRAY_8;
        case CAIRO_FORMAT_RGB96F:
            return TYPE_RGB_FLT;
        case CAIRO_FORMAT_RGBA128F:
            return TYPE_RGBA_FLT_PREMUL;
        default:
            return 0x0;
    }
}

/**
 * Construct a transformation suitable for display conversion in a cairo buffer
 *
 * @arg from  - The RGB CMS Profile the cairo data will start in.
 * @arg to    - The target RGB CMS Profile the cairo data needs to end up in.
 * @arg proof - A profile to apply a proofing step to, this can be CMYK for example.
 */
TransformCairo::TransformCairo(std::shared_ptr<Profile> const &from,
                               std::shared_ptr<Profile> const &to,
                               std::shared_ptr<Profile> const &proof,
                               RenderingIntent proof_intent, bool with_gamut_warn)
    : Transform(proof ?
        cmsCreateProofingTransformTHR(
            cmsCreateContext(nullptr, nullptr),
            from->getHandle(),
            lcms_color_format(from, true, Alpha::PREMULTIPLIED),
            to->getHandle(),
            lcms_color_format(from, true, Alpha::PRESENT),
            proof->getHandle(),
            INTENT_PERCEPTUAL,
            lcms_intent(proof_intent),
            cmsFLAGS_SOFTPROOFING | (with_gamut_warn ? cmsFLAGS_GAMUTCHECK : 0) | lcms_bpc(proof_intent))
      : cmsCreateTransformTHR(
            cmsCreateContext(nullptr, nullptr),
            from->getHandle(),
            lcms_color_format(from, true, Alpha::PREMULTIPLIED),
            to->getHandle(),
            lcms_color_format(from, true, Alpha::PRESENT),
            INTENT_PERCEPTUAL,
            0)
      , false)
    , _pixel_size_in((_channels_in + 1) * sizeof(float))
    , _pixel_size_out((_channels_out + 1) * sizeof(float))
{}

/**
 * Apply the CMS transform to the cairo surface and paint it into the output surface.
 *
 * @arg in - The source cairo surface with the pixels to transform.
 * @arg out - The destination cairo surface which may be the same as in.
 */
void TransformCairo::do_transform(cairo_surface_t *in, cairo_surface_t *out) const
{
    cairo_surface_flush(in);

    int width = cairo_image_surface_get_width(in);
    int height = cairo_image_surface_get_height(in);

    if (width != cairo_image_surface_get_width(out) ||
        height != cairo_image_surface_get_height(out)) {
        throw ColorError("Different image formats while applying CMS!");
    }

    auto px_in = cairo_image_surface_get_data(in);
    auto px_out = cairo_image_surface_get_data(out);

    cmsDoTransformLineStride(
         _handle,
         px_in,
         px_out,
         width,
         height,
         width * _pixel_size_in,
         width * _pixel_size_out,
         0, 0
    );

    cairo_surface_mark_dirty(out);
}

/**
 * Apply the CMS transform to the cairomm surface and paint it into the output surface.
 *
 * @arg in - The source cairomm surface with the pixels to transform.
 * @arg out - The destination cairomm surface which may be the same as in.
 */
void TransformCairo::do_transform(Cairo::RefPtr<Cairo::ImageSurface> &in, Cairo::RefPtr<Cairo::ImageSurface> &out) const
{
    do_transform(in->cobj(), out->cobj());
}

/**
 * Splice two Cairo RGBA128F formatted memory patches into one contigious
 * memory region suuitable for transformation in lcms2.
 *
 * @arg inputs   - A collection of raw data from a cairo image surface.
 *                 Each one should be 4 floats per pixel and each alpha should be the same.
 * @arg width    - The width of the surface in pixels.
 * @arg height   - The height of the surface in pixels.
 * @arg channels - The number of expected output channels not including alpha.
 *
 * @returns A newly allocated contigious region of floats. You should expect this region
 *          to contain alpha pre-multiplied channels so use accordingly.
 */
std::vector<float> TransformCairo::splice(std::vector<float *> inputs, int width, int height, int channels)
{
    std::vector<float> memory;
    memory.reserve((channels + 1) * width * height);

    for (int px = 0; px < (width * height); px++) {
        int c_out = 0;
        for (auto &input : inputs) {
            for (int c_in = 0; c_in < 3; c_in++) {
                if (c_out < channels) {
                    memory.emplace_back(*(input));
                    c_out++;
                }
                input++;
            }
            // alpha from the last surface
            if (c_out == channels) {
                memory.emplace_back(*(input));
                c_out++;
            }
            input++; // alpha
        }
    }

    return memory;
}

/**
 * Premultiply alpha in a Cairo RGBA128F memory region.
 *
 * Because lcms2 does not premultiply outputs but does allow them as inputs
 * we do this conversion after a cms transform to premultiply the color
 * channels in the way that cairo expects. Allowing for further processing
 * in a consistant way.
 *
 * @arg input    - The cairo memory data to be modified
 * @arg width    - The width in pixels of the data
 * @arg height   - The height in pixels of the data
 * @arg channels - The number of channels, this is always 3 unless
 *                 you are doing something special with spliced cairo.
 *
 */
void TransformCairo::premultiply(float *input, int width, int height, int channels)
{
     // Premultiply result back into cairo-like format for further operations
     for (int px = 0; px < (width * height); px++) {
         float a = input[channels];
         for (int c = 0; c < channels; c++) {
             input[c] *= a;
         }
         input += channels + 1;
     }
}



} // namespace Inkscape::Colors::CMS

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
