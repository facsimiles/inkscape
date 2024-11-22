// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors:
 *     fred
 *     bulia byak <buliabyak@users.sf.net>
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"  // only include where actually required!
#endif

#include <iostream>
#include <iomanip>

#ifndef PANGO_ENABLE_ENGINE
#define PANGO_ENABLE_ENGINE
#endif

#include <ft2build.h>
#include FT_OUTLINE_H
#include FT_BBOX_H
#include FT_TRUETYPE_TAGS_H
#include FT_TRUETYPE_TABLES_H
#include FT_GLYPH_H
#include FT_MULTIPLE_MASTERS_H

#include <pango/pangoft2.h>
#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>
#include <harfbuzz/hb-ot.h>

#include <glibmm/regex.h>

#include <2geom/pathvector.h>
#include <2geom/path-sink.h>
#include "libnrtype/font-glyph.h"
#include "libnrtype/font-instance.h"

#include "display/cairo-utils.h"  // Inkscape::Pixbuf

/*
 * Outline extraction
 */

struct FT2GeomData
{
    FT2GeomData(Geom::PathBuilder &b, double s)
        : builder(b)
        , last(0, 0)
        , scale(s)
    {
    }

    Geom::PathBuilder &builder;
    Geom::Point last;
    double scale;
};

// outline as returned by freetype
static int ft2_move_to(FT_Vector const *to, void * i_user)
{
    FT2GeomData *user = (FT2GeomData*)i_user;
    Geom::Point p(to->x, to->y);
    //    printf("m  t=%f %f\n",p[0],p[1]);
    user->builder.moveTo(p * user->scale);
    user->last = p;
    return 0;
}

static int ft2_line_to(FT_Vector const *to, void *i_user)
{
    FT2GeomData *user = (FT2GeomData*)i_user;
    Geom::Point p(to->x, to->y);
    //    printf("l  t=%f %f\n",p[0],p[1]);
    user->builder.lineTo(p * user->scale);
    user->last = p;
    return 0;
}

static int ft2_conic_to(FT_Vector const *control, FT_Vector const *to, void *i_user)
{
    FT2GeomData *user = (FT2GeomData*)i_user;
    Geom::Point p(to->x, to->y), c(control->x, control->y);
    user->builder.quadTo(c * user->scale, p * user->scale);
    //    printf("b c=%f %f  t=%f %f\n",c[0],c[1],p[0],p[1]);
    user->last = p;
    return 0;
}

static int ft2_cubic_to(FT_Vector const *control1, FT_Vector const *control2, FT_Vector const *to, void *i_user)
{
    FT2GeomData *user = (FT2GeomData*)i_user;
    Geom::Point p(to->x, to->y);
    Geom::Point c1(control1->x, control1->y);
    Geom::Point c2(control2->x, control2->y);
    //    printf("c c1=%f %f  c2=%f %f   t=%f %f\n",c1[0],c1[1],c2[0],c2[1],p[0],p[1]);
    //user->theP->CubicTo(p,3*(c1-user->last),3*(p-c2));
    user->builder.curveTo(c1 * user->scale, c2 * user->scale, p * user->scale);
    user->last = p;
    return 0;
}

/*
 *
 */

FontInstance::FontInstance(PangoFont *p_font, PangoFontDescription *descr)
{
    acquire(p_font, descr);

    _ascent  = _ascent_max  = 0.8;
    _descent = _descent_max = 0.2;
    _xheight = 0.5;

    // Default baseline values, alphabetic is reference
    _baselines[ SP_CSS_BASELINE_AUTO             ] =  0.0;
    _baselines[ SP_CSS_BASELINE_ALPHABETIC       ] =  0.0;
    _baselines[ SP_CSS_BASELINE_IDEOGRAPHIC      ] = -_descent;
    _baselines[ SP_CSS_BASELINE_HANGING          ] =  0.8 * _ascent;
    _baselines[ SP_CSS_BASELINE_MATHEMATICAL     ] =  0.8 * _xheight;
    _baselines[ SP_CSS_BASELINE_CENTRAL          ] =  0.5 - _descent;
    _baselines[ SP_CSS_BASELINE_MIDDLE           ] =  0.5 * _xheight;
    _baselines[ SP_CSS_BASELINE_TEXT_BEFORE_EDGE ] = _ascent;
    _baselines[ SP_CSS_BASELINE_TEXT_AFTER_EDGE  ] = -_descent;

    init_face();

    find_font_metrics();
}

FontInstance::~FontInstance()
{
    release();
}

/*
 * The following two functions isolate all the C-style resource ownership logic.
 */

// Either acquires all the necessary pointers to resources, or acquires nothing and throws CtorException.
void FontInstance::acquire(PangoFont *p_font_, PangoFontDescription *descr_)
{
    p_font = p_font_;
    descr = descr_;
    hb_font_copy = nullptr;
    face = nullptr;
    hb_face = nullptr;

    hb_font = pango_font_get_hb_font(p_font); // Pango owns hb_font.
    if (!hb_font) {
        release();
        throw CtorException("Failed to get harfbuzz font");
    }
    hb_face = hb_font_get_face(hb_font);

#if HB_VERSION_ATLEAST(2,6,5)
    // hb_font is immutable, yet we need to act on it (with set_funcs) to extract the freetype face
    hb_font_copy = hb_font_create_sub_font(hb_font);
    hb_ft_font_set_funcs(hb_font_copy);
    face = hb_ft_font_lock_face(hb_font_copy);
#else
    if (face) {
        pango_fc_font_unlock_face(PANGO_FC_FONT(p_font));
    }
#endif
    if (!face) {
        release();
        throw CtorException("Failed to get freetype face");
    }
}

// Release the resources acquired by acquire().
void FontInstance::release()
{
#if HB_VERSION_ATLEAST(2,6,5)
    if (hb_font_copy) {
        if (face) {
            hb_ft_font_unlock_face(hb_font_copy);
        }
        hb_font_destroy(hb_font_copy);
    }
#else
    if (face) {
        pango_fc_font_unlock_face(PANGO_FC_FONT(p_font));
    }
#endif
    pango_font_description_free(descr);
    g_object_unref(p_font);
}

void FontInstance::init_face()
{
    auto hb_font = pango_font_get_hb_font(p_font); // Pango owns hb_font.
    assert(hb_font); // Guaranteed since already tested in acquire().

    has_svg = hb_ot_color_has_svg(hb_face); // SVG glyphs Since HB 2.1.0

    FT_Select_Charmap(face, ft_encoding_unicode);
    FT_Select_Charmap(face, ft_encoding_symbol);

    data = std::make_shared<Data>();
    readOpenTypeTableList(hb_font, openTypeTableList);
    readOpenTypeSVGTable(hb_font, data->openTypeSVGGlyphs, data->openTypeSVGData);
    readOpenTypeFvarAxes(face, data->openTypeVarAxes);

#if FREETYPE_MAJOR == 2 && FREETYPE_MINOR >= 8  // 2.8 does not seem to work even though it has some support.

    // 'font-variation-settings' support.
    //    The font returned from pango_fc_font_lock_face does not include variation settings. We must set them.

    // We need to:
    //   Extract axes with values from Pango font description.
    //   Replace default axis values with extracted values.

    if (auto var = pango_font_description_get_variations(descr)) {
        Glib::ustring variations = var;

        FT_MM_Var *mmvar = nullptr;
        FT_Multi_Master mmtype;
        if (FT_HAS_MULTIPLE_MASTERS(face)      &&    // Font has variables
            FT_Get_MM_Var(face, &mmvar) == 0   &&    // We found the data
            FT_Get_Multi_Master(face, &mmtype) != 0) {  // It's not an Adobe MM font

            // std::cout << "  Multiple Masters: variables: " << mmvar->num_axis
            //           << "  named styles: " << mmvar->num_namedstyles << std::endl;

            // Get the required values from Pango Font Description
            // Need to check format of values from Pango, for the moment accept any format.
            auto regex = Glib::Regex::create("(\\w{4})=([-+]?\\d*\\.?\\d+([eE][-+]?\\d+)?)");
            Glib::MatchInfo matchInfo;

            FT_UInt num_axis = data->openTypeVarAxes.size();
            std::vector<FT_Fixed> w(num_axis, 0);

            auto tokens = Glib::Regex::split_simple(",", variations);
            for (auto const &token : tokens) {

                regex->match(token, matchInfo);
                if (matchInfo.matches()) {

                    float value = std::stod(matchInfo.fetch(2).raw());  // Should clamp value

                    // Translate the "named" axes.
                    auto name = matchInfo.fetch(1);
                    if (name == "wdth") name = "Width"      ; // 'font-stretch'
                    if (name == "wght") name = "Weight"     ; // 'font-weight'
                    if (name == "opsz") name = "OpticalSize"; // 'font-optical-sizing' (indirectly)
                    if (name == "slnt") name = "Slant"      ; // 'font-style'
                    if (name == "ital") name = "Italic"     ; // 'font-style'

                    auto it = data->openTypeVarAxes.find(name);
                    if (it != data->openTypeVarAxes.end()) {
                        it->second.set_val = value;
                        w[it->second.index] = value * 65536;
                    }
                }
            }

            // Set design coordinates
            auto err = FT_Set_Var_Design_Coordinates(face, num_axis, w.data());
            if (err) {
                std::cerr << "FontInstance::FontInstance(): Error in call to FT_Set_Var_Design_Coordinates(): " << err << std::endl;
            }

            // FT_Done_MM_Var(mmlib, mmvar);
        }
    }

#endif // FreeType
}

// Internal function to find baselines
void FontInstance::find_font_metrics()
{
    // CSS2 recommends using the OS/2 values sTypoAscender and sTypoDescender for the Typographic ascender and descender values:
    //   http://www.w3.org/TR/CSS2/visudet.html#sTypoAscender
    // On Windows, the typographic ascender and descender are taken from the otmMacAscent and
    // otmMacDescent values:
    //   http://microsoft.public.win32.programmer.gdi.narkive.com/LV6k4BDh/msdn-documentation-outlinetextmetrics-clarification
    // The otmAscent and otmDescent values are the maximum ascent and maximum descent of all the glyphs in a font.
    if (face->units_per_EM != 0) {  // If zero then it's a bitmap font.

        auto os2 = (TT_OS2*)FT_Get_Sfnt_Table(face, ft_sfnt_os2);

        if (auto post = (TT_Postscript*)FT_Get_Sfnt_Table(face, ft_sfnt_post)) {
            _italic_angle = FTFixedToDouble(post->italicAngle);
            _fixed_width = post->isFixedPitch != 0;
            // fsSelection mask: oblique/italic = 0x201
            _oblique = post->italicAngle != 0 || (os2 && (os2->fsSelection & 0x201) != 0);
        }

        if (os2) {
            _family_class = os2->sFamilyClass;
            _ascent  = std::fabs((double)os2->sTypoAscender / face->units_per_EM);
            _descent = std::fabs((double)os2->sTypoDescender/ face->units_per_EM);
        } else {
            _ascent  = std::fabs((double)face->ascender  / face->units_per_EM);
            _descent = std::fabs((double)face->descender / face->units_per_EM);
        }
        _ascent_max  = std::fabs((double)face->ascender  / face->units_per_EM);
        _descent_max = std::fabs((double)face->descender / face->units_per_EM);
        _design_units = face->units_per_EM;

        // In CSS em size is ascent + descent... which should be 1. If not, adjust so it is.
        double em = _ascent + _descent;
        if (em > 0.0) {
            _ascent /= em;
            _descent /= em;
        }

        // x-height
        if (os2 && os2->version >= 0x0002 && os2->version != 0xffffu) {
            // Only os/2 version 2 and above have sxHeight, 0xffff marks "old Mac fonts" without table
            _xheight = std::fabs((double)os2->sxHeight / face->units_per_EM);
        } else {
            // Measure 'x' height in font. Recommended option by XSL standard if no sxHeight.
            FT_UInt index = FT_Get_Char_Index(face, 'x');
            if (index != 0) {
                FT_Load_Glyph(face, index, FT_LOAD_NO_SCALE);
                _xheight = std::fabs((double)face->glyph->metrics.height / face->units_per_EM);
            } else {
                // No 'x' in font!
                _xheight = 0.5;
            }
        }

        // Baselines defined relative to  alphabetic.
        _baselines[ SP_CSS_BASELINE_IDEOGRAPHIC      ] = -_descent;      // Recommendation
        _baselines[ SP_CSS_BASELINE_HANGING          ] = 0.8 * _ascent;  // Guess
        _baselines[ SP_CSS_BASELINE_MATHEMATICAL     ] = 0.8 * _xheight; // Guess
        _baselines[ SP_CSS_BASELINE_CENTRAL          ] = 0.5 - _descent; // Definition
        _baselines[ SP_CSS_BASELINE_MIDDLE           ] = 0.5 * _xheight; // Definition
        _baselines[ SP_CSS_BASELINE_TEXT_BEFORE_EDGE ] = _ascent;        // Definition
        _baselines[ SP_CSS_BASELINE_TEXT_AFTER_EDGE  ] = -_descent;      // Definition

        // Better math baseline:
        // Try center of minus sign
        FT_UInt index = FT_Get_Char_Index(face, 0x2212); //'−'
        // If no minus sign, try hyphen
        if (index == 0) {
            index = FT_Get_Char_Index(face, '-');
        }

        if (index != 0) {
            FT_Load_Glyph(face, index, FT_LOAD_NO_SCALE);
            FT_Glyph aglyph;
            FT_Get_Glyph(face->glyph, &aglyph);
            FT_BBox acbox;
            FT_Glyph_Get_CBox(aglyph, FT_GLYPH_BBOX_UNSCALED, &acbox);
            double math = (double)(acbox.yMin + acbox.yMax) / 2.0 / face->units_per_EM;
            _baselines[ SP_CSS_BASELINE_MATHEMATICAL ] = math;
            // std::cout << "Math baseline: - bbox: y_min: " << acbox.yMin
            //           << "  y_max: " << acbox.yMax
            //           << "  math: " << math << std::endl;
            FT_Done_Glyph(aglyph);
        }

        // Find hanging baseline... assume it is at top of 'म'.
        index = FT_Get_Char_Index(face, 0x092E); // 'म'
        if (index != 0) {
            FT_Load_Glyph(face, index, FT_LOAD_NO_SCALE);
            FT_Glyph aglyph;
            FT_Get_Glyph(face->glyph, &aglyph);
            FT_BBox acbox;
            FT_Glyph_Get_CBox(aglyph, FT_GLYPH_BBOX_UNSCALED, &acbox);
            double hanging = (double)acbox.yMax / face->units_per_EM;
            _baselines[ SP_CSS_BASELINE_HANGING ] = hanging;
            // std::cout << "Hanging baseline:  प: " << hanging << std::endl;
            FT_Done_Glyph(aglyph);
        }
    }

    // const gchar *family = pango_font_description_get_family(descr);
    // std::cout << "Font: " << (family?family:"null") << std::endl;
    // std::cout << "  ascent:      " << _ascent      << std::endl;
    // std::cout << "  descent:     " << _descent     << std::endl;
    // std::cout << "  x-height:    " << _xheight     << std::endl;
    // std::cout << "  max ascent:  " << _ascent_max  << std::endl;
    // std::cout << "  max descent: " << _descent_max << std::endl;
    // std::cout << " Baselines:" << std::endl;
    // std::cout << "  alphabetic:  " << _baselines[ SP_CSS_BASELINE_ALPHABETIC       ] << std::endl;
    // std::cout << "  ideographic: " << _baselines[ SP_CSS_BASELINE_IDEOGRAPHIC      ] << std::endl;
    // std::cout << "  hanging:     " << _baselines[ SP_CSS_BASELINE_HANGING          ] << std::endl;
    // std::cout << "  math:        " << _baselines[ SP_CSS_BASELINE_MATHEMATICAL     ] << std::endl;
    // std::cout << "  central:     " << _baselines[ SP_CSS_BASELINE_CENTRAL          ] << std::endl;
    // std::cout << "  middle:      " << _baselines[ SP_CSS_BASELINE_MIDDLE           ] << std::endl;
    // std::cout << "  text_before: " << _baselines[ SP_CSS_BASELINE_TEXT_BEFORE_EDGE ] << std::endl;
    // std::cout << "  text_after:  " << _baselines[ SP_CSS_BASELINE_TEXT_AFTER_EDGE  ] << std::endl;
}

unsigned int FontInstance::MapUnicodeChar(gunichar c) const
{
    unsigned int res = 0;
    if (c > 0x10ffff) {
        // >= 0xf0000 is out of range for assigned codepoints, above is for private use.
        std::cerr << "FontInstance::MapUnicodeChar: Unicode codepoint out of range: "
                  << std::hex << c << std::dec
                  << std::endl;
    } else {
        res = FT_Get_Char_Index(face, c);
    }
    return res;
}

FontGlyph const *FontInstance::LoadGlyph(unsigned int glyph_id)
{
    if (!FT_IS_SCALABLE(face)) {
        return nullptr; // bitmap font
    }

    if (auto it = data->glyphs.find(glyph_id); it != data->glyphs.end()) {
        return it->second.get(); // already loaded
    }

    Geom::PathBuilder path_builder;

    // Note: Bitmap only fonts (i.e. some color fonts) ignore FT_LOAD_NO_BITMAP.
    if (FT_Load_Glyph(face, glyph_id, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP)) {
        return nullptr; // error
    }

    // Find scale, used by both metrics and paths.
    int x_scale = 0;
    int y_scale = 0;
    hb_font_get_scale(hb_font, &x_scale, &y_scale);
    if (x_scale != y_scale) {
        std::cerr << "FontInstance::LoadGlyph: x scale not equal to y scale!" << std::endl;
    }

    auto n_g = std::make_unique<FontGlyph>();

    // Find metrics ----------------------------------

    // Use harfbuzz as freetype doesn't return proper values for bitmap fonts.

    n_g->h_advance = hb_font_get_glyph_h_advance (hb_font, glyph_id) / (double)x_scale; // Since HB 0.9.2
    if (openTypeTableList.contains("vmtx")) {
        n_g->v_advance = -hb_font_get_glyph_v_advance (hb_font, glyph_id) / (double)y_scale;
    } else {
        // Don't use harfbuzz synthisized vertical metrics, it's wrong (includes line gap?)!
        // CSS3 Writing modes dictates that if vertical font metrics are missing we must
        // synthisize them. No method is specified. The SVG 1.1 spec suggests using the em
        // height (which is not theFace->height as that includes leading). The em height
        // is ascender + descender (descender positive).  Note: The "Requirements for
        // Japanese Text Layout" W3C document says that Japanese kanji should be "set
        // solid" which implies that vertical (and horizontal) advance should be 1em.
        n_g->v_advance = 1.0;
    }

    hb_glyph_extents_t extents;
    bool success = hb_font_get_glyph_extents(hb_font, glyph_id, &extents); // Since HB 0.9.2
    if (success) {
        n_g->bbox_exact = Geom::Rect(extents.x_bearing / (double)x_scale,
                                     extents.y_bearing / (double)y_scale,
                                    (extents.x_bearing + extents.width) / (double)x_scale,
                                    (extents.y_bearing + extents.height) / (double)y_scale);
    } else {
        std::cerr << "FontInstance::LoadGlyph: Failed to get glyph extents for glyph: glyph_id!"
                  << "  (" << pango_font_description_to_string(descr) << ")"
                  << std::endl;
    }

    // Nominal design space of glyph.
    n_g->bbox_pick.setRight(n_g->h_advance);
    n_g->bbox_pick.setBottom(  _ascent_max);
    n_g->bbox_pick.setTop(   -_descent_max);

    // Any place that might be inked, including any text decoration.
    n_g->bbox_draw = n_g->bbox_pick;               // Design space for glyph
    n_g->bbox_draw.setBottom(  _ascent_max * 1.1); // Expand to allow for text decoration
    n_g->bbox_draw.setTop(   -_descent_max * 1.1);
    n_g->bbox_draw.unionWith(n_g->bbox_exact);     // Extend if glyph leaks outside of design space.

    // Find path vector ------------------------------

    if (face->glyph->format == ft_glyph_format_outline) {
        FT_Outline_Funcs ft2_outline_funcs = {
            ft2_move_to,
            ft2_line_to,
            ft2_conic_to,
            ft2_cubic_to,
            0, 0
        };
        FT2GeomData user(path_builder, 1.0 / face->units_per_EM);
        FT_Outline_Decompose(&face->glyph->outline, &ft2_outline_funcs, &user);
    }

    path_builder.flush();

    Geom::PathVector pv = path_builder.peek();

    // close all paths
    for (auto &i : pv) {
        i.close();
    }

    if (!pv.empty()) {
        n_g->pathvector = std::move(pv);
    }

    auto ret = data->glyphs.emplace(glyph_id, std::move(n_g));

    return ret.first->second.get();
}

bool FontInstance::FontMetrics(double &ascent, double &descent, double &xheight) const
{
    ascent = _ascent;
    descent = _descent;
    xheight = _xheight;

    return true;
}

bool FontInstance::FontDecoration(double &underline_position, double &underline_thickness, double &linethrough_position, double &linethrough_thickness) const
{
    if (face->units_per_EM == 0) {
        return false; // bitmap font
    }
    underline_position    = std::fabs((double)face->underline_position  / face->units_per_EM);
    underline_thickness   = std::fabs((double)face->underline_thickness / face->units_per_EM);
    // there is no specific linethrough information, mock it up from other font fields
    linethrough_position  = std::fabs((double)face->ascender / 3.0      / face->units_per_EM);
    linethrough_thickness = std::fabs((double)face->underline_thickness / face->units_per_EM);
    return true;
}

bool FontInstance::FontSlope(double &run, double &rise) const
{
    run = 0.0;
    rise = 1.0;

    if (!FT_IS_SCALABLE(face)) {
        return false; // bitmap font
    }

    auto hhea = reinterpret_cast<TT_HoriHeader*>(FT_Get_Sfnt_Table(face, ft_sfnt_hhea));
    if (!hhea) {
        return false;
    }
    run = hhea->caret_Slope_Run;
    rise = hhea->caret_Slope_Rise;

    return true;
}

Geom::Rect FontInstance::BBoxExact(unsigned int glyph_id)
{
    auto g = LoadGlyph(glyph_id);
    if (!g) {
        return {};
    }

    return g->bbox_exact;
}

Geom::Rect FontInstance::BBoxPick(unsigned int glyph_id)
{
    auto g = LoadGlyph(glyph_id);
    if (!g) {
        return {0, 0, 1, 1}; // em box
    }

    return g->bbox_pick;
}

Geom::Rect FontInstance::BBoxDraw(unsigned int glyph_id)
{
    auto g = LoadGlyph(glyph_id);
    if (!g) {
        return {};
    }

    return g->bbox_draw;
}

Geom::PathVector const *FontInstance::PathVector(unsigned int glyph_id)
{
    auto g = LoadGlyph(glyph_id);
    if (!g) {
        return nullptr;
    }

    return &g->pathvector;
}

Inkscape::Pixbuf const *FontInstance::PixBuf(unsigned int glyph_id)
{
    auto glyph_iter = data->openTypeSVGGlyphs.find(glyph_id);
    if (glyph_iter == data->openTypeSVGGlyphs.end()) {
        return nullptr; // out of range
    }

    // Glyphs are layed out in the +x, -y quadrant (assuming viewBox origin is 0,0).
    // We need to shift the viewBox by the height inorder to generate pixbuf!
    // To do: glyphs must draw overflow so we actually need larger pixbuf!
    // To do: Error handling.

    if (glyph_iter->second.pixbuf) {
        return glyph_iter->second.pixbuf.get(); // already loaded
    }

    Glib::ustring svg = data->openTypeSVGData[glyph_iter->second.entry_index];

    // Create new viewbox which determines pixbuf size.
    Glib::ustring viewbox("viewBox=\"0 ");
    viewbox += std::to_string(-_design_units);
    viewbox += " ";
    viewbox += std::to_string(_design_units*2); // Noto emoji leaks outside of em-box.
    viewbox += " ";
    viewbox += std::to_string(_design_units*2);
    viewbox += "\"";

    // Search for existing viewbox
    static auto regex = Glib::Regex::create("viewBox=\"\\s*(\\d*\\.?\\d+)\\s*,?\\s*(\\d*\\.?\\d+)\\s*,?\\s*(\\d+\\.?\\d+)\\s*,?\\s*(\\d+\\.?\\d+)\\s*\"", Glib::RegexCompileFlags::REGEX_OPTIMIZE);
    Glib::MatchInfo matchInfo;
    regex->match(svg, matchInfo);

    if (matchInfo.matches()) {
        // We have viewBox! We must transform so viewBox corresponds to design units.

        // Replace viewbox
        svg = regex->replace_literal(svg, 0, viewbox, static_cast<Glib::RegexMatchFlags>(0));

        // Insert group with required transform to map glyph to new viewbox.
        double x = std::stod(matchInfo.fetch(1).raw());
        double y = std::stod(matchInfo.fetch(2).raw());
        double w = std::stod(matchInfo.fetch(3).raw());
        double h = std::stod(matchInfo.fetch(4).raw());
        // std::cout << " x: " << x
        //           << " y: " << y
        //           << " w: " << w
        //           << " h: " << h << std::endl;

        if (w <= 0.0 || h <= 0.0) {
            std::cerr << "FontInstance::PixBuf: Invalid glyph width or height!" << std::endl;
        } else {

            double xscale = _design_units/w;
            double yscale = _design_units/h;
            double xtrans = _design_units/w * x;
            double ytrans = _design_units/h * y;

            if (xscale != 1.0 || yscale != 1.0) {
                Glib::ustring group = "<g transform=\"matrix(";
                group += std::to_string(xscale);
                group += ", 0, 0, ";
                group += std::to_string(yscale);
                group += std::to_string(-xtrans);
                group += ", ";
                group += std::to_string(-ytrans);
                group += ")\">";

                // Insert start group tag after initial <svg>
                Glib::RefPtr<Glib::Regex> regex = Glib::Regex::create("<\\s*svg.*?>");
                regex->match(svg, matchInfo);
                if (matchInfo.matches()) {
                    int start = -1;
                    int end   = -1;
                    matchInfo.fetch_pos(0, start, end);
                    svg.insert(end, group);
                } else {
                    std::cerr << "FontInstance::PixBuf: Could not find <svg> tag!" << std::endl;
                }

                // Insert end group tag before final </svg> (To do: make sure it is final </svg>)
                regex = Glib::Regex::create("<\\s*\\/\\s*svg.*?>");
                regex->match(svg, matchInfo);
                if (matchInfo.matches()) {
                    int start = -1;
                    int end   = -1;
                    matchInfo.fetch_pos(0, start, end);
                    svg.insert(start, "</g>");
                } else {
                    std::cerr << "FontInstance::PixBuf: Could not find </svg> tag!" << std::endl;
                }
            }
        }

    } else {
        // No viewBox! We insert one. (To do: Look at 'width' and 'height' to see if we must scale.)
        Glib::RefPtr<Glib::Regex> regex = Glib::Regex::create("<\\s*svg");
        viewbox.insert(0, "<svg ");
        svg = regex->replace_literal(svg, 0, viewbox, static_cast<Glib::RegexMatchFlags>(0));
    }

    // Make glyph visible.
    auto pattern = Glib::ustring::compose("(id=\"\\s*glyph%1\\s*\")\\s*visibility=\"hidden\"", glyph_id);
    auto regex2 = Glib::Regex::create(pattern, Glib::RegexCompileFlags::REGEX_OPTIMIZE);
    svg = regex2->replace(svg, 0, "\\1", static_cast<Glib::RegexMatchFlags>(0));

    // Finally create pixbuf!
    auto pixbuf = Inkscape::Pixbuf::create_from_buffer(svg.raw());

    // Ensure exists in cairo format before locking it down. (Rendering code requires cairo format.)
    pixbuf->ensurePixelFormat(Inkscape::Pixbuf::PF_CAIRO);

    // And cache it.
    glyph_iter->second.pixbuf.reset(pixbuf);

    return pixbuf;
}

double FontInstance::Advance(unsigned int glyph_id, bool vertical)
{
    auto g = LoadGlyph(glyph_id);
    if (!g) {
        return 0;
    }

    return vertical ? g->v_advance : g->h_advance;
}

std::map<Glib::ustring, OTSubstitution> const &FontInstance::get_opentype_tables()
{
    if (!data->openTypeTables) {
        auto hb_font = pango_font_get_hb_font(p_font);
        assert(hb_font);

        data->openTypeTables.emplace();
        readOpenTypeGsubTable(hb_font, *data->openTypeTables);
    }

    return *data->openTypeTables;
}

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
