// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "OpenTypeUtil.h"

#define DEBUG_OPENTYPEUTIL
#ifdef DEBUG_OPENTYPEUTIL
#include <iostream>  // For debugging
#include <filesystem>
#include <fstream>
#endif

#include <memory>
#include <unordered_map>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdkmm/pixbuf.h>
#include <gdkmm/pixbufloader.h>
#include <glibmm/fileutils.h> // Glib::FileError

// FreeType
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_SFNT_NAMES_H

// Harfbuzz
#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>
#include <harfbuzz/hb-ot.h>

#include <glibmm/regex.h>

// SVG in OpenType
#include "io/stream/gzipstream.h"
#include "io/stream/bufferstream.h"

#include "display/cairo-utils.h"
#include "util/delete-with.h"

// Utilities used in this file

using HbSet = std::unique_ptr<hb_set_t, Inkscape::Util::Deleter<hb_set_destroy>>;

void dump_tag( guint32 *tag, Glib::ustring prefix = "", bool lf=true ) {
    std::cout << prefix
              << ((char)((*tag & 0xff000000)>>24))
              << ((char)((*tag & 0x00ff0000)>>16))
              << ((char)((*tag & 0x0000ff00)>> 8))
              << ((char)((*tag & 0x000000ff)    ));
    if( lf ) {
        std::cout << std::endl;
    }
}

Glib::ustring extract_tag( guint32 *tag ) {
    Glib::ustring tag_name;
    tag_name += ((char)((*tag & 0xff000000)>>24));
    tag_name += ((char)((*tag & 0x00ff0000)>>16));
    tag_name += ((char)((*tag & 0x0000ff00)>> 8));
    tag_name += ((char)((*tag & 0x000000ff)    ));
    return tag_name;
}

// Get font name from hb_face
std::string font_name(hb_face_t* hb_face) {
    const unsigned int MAX_NAME_CHAR = 100;
    unsigned int max_name_char = MAX_NAME_CHAR;
    char font_name_c[MAX_NAME_CHAR] = {};
    hb_ot_name_get_utf8 (hb_face, HB_OT_NAME_ID_FONT_FAMILY, hb_language_get_default(), &max_name_char, font_name_c);
    std::string font_name = "unknown";
    if (font_name_c) {
        font_name = font_name_c;
    }
    return font_name;
}

// Create and return a directory to dump a file file's SVGs or PNGs.
std::string font_directory(hb_face_t* hb_face) {

    const std::string font_directory = "font_dumps";
    if (!std::filesystem::is_directory(font_directory) || !std::filesystem::exists(font_directory)) {
        std::filesystem::create_directory(font_directory);
    }

    std::string font_name_directory = font_directory + "/" + font_name(hb_face);
    if (!std::filesystem::is_directory(font_name_directory) || !std::filesystem::exists(font_name_directory)) {
        std::filesystem::create_directory(font_name_directory);
    }

    return font_name_directory;
}

void readOpenTypeTableList(hb_font_t* hb_font, std::unordered_set<std::string>& list) {

    hb_face_t* hb_face = hb_font_get_face (hb_font);

    static const unsigned int MAX_TABLES = 100;
    unsigned int table_count = MAX_TABLES;
    hb_tag_t table_tags[MAX_TABLES];
    auto count = hb_face_get_table_tags(hb_face, 0, &table_count, table_tags);

    for (unsigned int i = 0; i < count; ++i) {
        char buf[5] = {}; // 4 characters plus null termination.
        hb_tag_to_string(table_tags[i], buf);
        list.emplace(buf);
    }
}

// There is now hb_face_collect_glyph_mappings() (since 7.0) that could be used.

// Later (see get_glyphs) we need to lookup the Unicode codepoint for a glyph
// but there's no direct API for that. So, we need a way to iterate over all
// glyph mappings and build a reverse map.
// FIXME: we should handle UVS at some point... or better, work with glyphs directly

// Allows looking up the lowest Unicode codepoint mapped to a given glyph.
// To do so, it lazily builds a reverse map.
class GlyphToUnicodeMap {
protected:
    hb_font_t* font;
    HbSet codepointSet;

    std::unordered_map<hb_codepoint_t, hb_codepoint_t> mappings;
    bool more = true; // false if we have finished iterating the set
    hb_codepoint_t codepoint = HB_SET_VALUE_INVALID; // current iteration
public:
    GlyphToUnicodeMap(hb_font_t* font): font(font), codepointSet(hb_set_create()) {
        hb_face_collect_unicodes(hb_font_get_face(font), codepointSet.get());
    }

    hb_codepoint_t lookup(hb_codepoint_t glyph) {
        // first, try to find it in the mappings we've seen so far
        if (auto it = mappings.find(glyph); it != mappings.end())
            return it->second;

        // populate more mappings from the set
        while ((more = (more && hb_set_next(codepointSet.get(), &codepoint)))) {
            // get the glyph that this codepoint is associated with, if any
            hb_codepoint_t tGlyph;
            if (!hb_font_get_nominal_glyph(font, codepoint, &tGlyph)) continue;

            // save the mapping, and return if this is the one we were looking for
            mappings.emplace(tGlyph, codepoint);
            if (tGlyph == glyph) return codepoint;
        }
        return 0;
    }
};

void get_glyphs(GlyphToUnicodeMap& glyphMap, HbSet& set, Glib::ustring& characters) {
    hb_codepoint_t glyph = -1;
    while (hb_set_next(set.get(), &glyph)) {
        if (auto codepoint = glyphMap.lookup(glyph))
            characters += codepoint;
    }
}

SVGGlyphEntry::~SVGGlyphEntry() = default;

// Make a list of all tables found in the GSUB
// This list includes all tables regardless of script or language.
// Use Harfbuzz, Pango's equivalent calls are deprecated.
void readOpenTypeGsubTable (hb_font_t* hb_font,
                            std::map<Glib::ustring, OTSubstitution>& tables)
{
    hb_face_t* hb_face = hb_font_get_face (hb_font);

    tables.clear();

    // First time to get size of array
    auto script_count = hb_ot_layout_table_get_script_tags(hb_face, HB_OT_TAG_GSUB, 0, nullptr, nullptr);
    auto const hb_scripts = g_new(hb_tag_t, script_count + 1);

    // Second time to fill array (this two step process was not necessary with Pango).
    hb_ot_layout_table_get_script_tags(hb_face, HB_OT_TAG_GSUB, 0, &script_count, hb_scripts);

    for(unsigned int i = 0; i < script_count; ++i) {
        // std::cout << " Script: " << extract_tag(&hb_scripts[i]) << std::endl;
        auto language_count = hb_ot_layout_script_get_language_tags(hb_face, HB_OT_TAG_GSUB, i, 0, nullptr, nullptr);

        if(language_count > 0) {
            auto const hb_languages = g_new(hb_tag_t, language_count + 1);
            hb_ot_layout_script_get_language_tags(hb_face, HB_OT_TAG_GSUB, i, 0, &language_count, hb_languages);

            for(unsigned int j = 0; j < language_count; ++j) {
                // std::cout << "  Language: " << extract_tag(&hb_languages[j]) << std::endl;
                auto feature_count = hb_ot_layout_language_get_feature_tags(hb_face, HB_OT_TAG_GSUB, i, j, 0, nullptr, nullptr);
                auto const hb_features = g_new(hb_tag_t, feature_count + 1);
                hb_ot_layout_language_get_feature_tags(hb_face, HB_OT_TAG_GSUB, i, j, 0, &feature_count, hb_features);

                for(unsigned int k = 0; k < feature_count; ++k) {
                    // std::cout << "   Feature: " << extract_tag(&hb_features[k]) << std::endl;
                    tables[ extract_tag(&hb_features[k])];
                }

                g_free(hb_features);
            }

            g_free(hb_languages);

        } else {

            // Even if no languages are present there is still the default.
            // std::cout << "  Language: " << " (dflt)" << std::endl;
            auto feature_count = hb_ot_layout_language_get_feature_tags(hb_face, HB_OT_TAG_GSUB, i,
                                                                        HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX,
                                                                        0, nullptr, nullptr);
            auto const hb_features = g_new(hb_tag_t, feature_count + 1); 
            hb_ot_layout_language_get_feature_tags(hb_face, HB_OT_TAG_GSUB, i,
                                                   HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX,
                                                   0, &feature_count, hb_features);

            for(unsigned int k = 0; k < feature_count; ++k) {
                // std::cout << "   Feature: " << extract_tag(&hb_features[k]) << std::endl;
                tables[ extract_tag(&hb_features[k])];
            }

            g_free(hb_features);
        }
    }

    // Find glyphs in OpenType substitution tables ('gsub').
    // Note that pango's functions are just dummies. Must use harfbuzz.

    GlyphToUnicodeMap glyphMap (hb_font);

    // Loop over all tables
    for (auto table: tables) {

        // Only look at style substitution tables ('salt', 'ss01', etc. but not 'ssty').
        // Also look at character substitution tables ('cv01', etc.).
        bool style    =
            table.first == "case"  /* Case-Sensitive Forms   */                          ||
            table.first == "salt"  /* Stylistic Alternatives */                          ||
            table.first == "swsh"  /* Swash                  */                          ||
            table.first == "cwsh"  /* Contextual Swash       */                          ||
            table.first == "ornm"  /* Ornaments              */                          ||
            table.first == "nalt"  /* Alternative Annotation */                          ||
            table.first == "hist"  /* Historical Forms       */                          ||
            (table.first[0] == 's' && table.first[1] == 's' && !(table.first[2] == 't')) ||
            (table.first[0] == 'c' && table.first[1] == 'v');

        bool ligature = ( table.first == "liga" ||  // Standard ligatures
                          table.first == "clig" ||  // Common ligatures
                          table.first == "dlig" ||  // Discretionary ligatures
                          table.first == "hlig" ||  // Historical ligatures
                          table.first == "calt" );  // Contextual alternatives

        bool numeric  = ( table.first == "lnum" ||  // Lining numerals
                          table.first == "onum" ||  // Old style
                          table.first == "pnum" ||  // Proportional
                          table.first == "tnum" ||  // Tabular
                          table.first == "frac" ||  // Diagonal fractions
                          table.first == "afrc" ||  // Stacked fractions
                          table.first == "ordn" ||  // Ordinal fractions
                          table.first == "zero" );  // Slashed zero

        if (style || ligature || numeric) {

            unsigned int feature_index;
            if (  hb_ot_layout_language_find_feature (hb_face, HB_OT_TAG_GSUB,
                                                      0,  // Assume one script exists with index 0
                                                      HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX,
                                                      HB_TAG(table.first[0],
                                                             table.first[1],
                                                             table.first[2],
                                                             table.first[3]),
                                                      &feature_index ) ) {

                // std::cout << "Table: " << table.first << std::endl;
                // std::cout << "  Found feature, number: " << feature_index << std::endl;

                unsigned start_offset = 0;

                while (true) {
                    unsigned lookup_indexes[32];
                    unsigned lookup_count = 32;
                    int count = hb_ot_layout_feature_get_lookups(hb_face, HB_OT_TAG_GSUB,
                                                                 feature_index,
                                                                 start_offset,
                                                                 &lookup_count,
                                                                 lookup_indexes);
                    // std::cout << "  Lookup count: " << lookup_count << " total: " << count << std::endl;

                    for (unsigned i = 0; i < lookup_count; i++) {
                        HbSet glyphs_before (hb_set_create());
                        HbSet glyphs_input  (hb_set_create());
                        HbSet glyphs_after  (hb_set_create());
                        HbSet glyphs_output (hb_set_create());

                        hb_ot_layout_lookup_collect_glyphs (hb_face, HB_OT_TAG_GSUB,
                                                            lookup_indexes[i],
                                                            glyphs_before.get(),
                                                            glyphs_input.get(),
                                                            glyphs_after.get(),
                                                            glyphs_output.get() );

                        // std::cout << "  Populations: "
                        //           << " " << hb_set_get_population (glyphs_before)
                        //           << " " << hb_set_get_population (glyphs_input)
                        //           << " " << hb_set_get_population (glyphs_after)
                        //           << " " << hb_set_get_population (glyphs_output)
                        //           << std::endl;

                        get_glyphs (glyphMap, glyphs_before, tables[table.first].before);
                        get_glyphs (glyphMap, glyphs_input,  tables[table.first].input );
                        get_glyphs (glyphMap, glyphs_after,  tables[table.first].after );
                        get_glyphs (glyphMap, glyphs_output, tables[table.first].output);

                        // std::cout << "  Before: " << tables[table.first].before.c_str() << std::endl;
                        // std::cout << "  Input:  " << tables[table.first].input.c_str() << std::endl;
                        // std::cout << "  After:  " << tables[table.first].after.c_str() << std::endl;
                        // std::cout << "  Output: " << tables[table.first].output.c_str() << std::endl;
                    }

                    start_offset += lookup_count;
                    if (start_offset >= count) {
                        break;
                    }
                }

            } else {
                // std::cout << "  Did not find '" << table.first << "'!" << std::endl;
            }
        }

    }

    g_free(hb_scripts);
}

// Harfbuzz now as API for variations (Version 2.2, Nov 29 2018).
// Make a list of all Variation axes with ranges.
void readOpenTypeFvarAxes(const FT_Face ft_face,
                          std::map<Glib::ustring, OTVarAxis>& axes) {

#if FREETYPE_MAJOR *10000 + FREETYPE_MINOR*100 + FREETYPE_MICRO >= 20701
    FT_MM_Var* mmvar = nullptr;
    FT_Multi_Master mmtype;
    if (FT_HAS_MULTIPLE_MASTERS( ft_face )    &&    // Font has variables
        FT_Get_MM_Var( ft_face, &mmvar) == 0   &&    // We found the data
        FT_Get_Multi_Master( ft_face, &mmtype) !=0) {  // It's not an Adobe MM font

        std::vector<FT_Fixed> coords(mmvar->num_axis);
        FT_Get_Var_Design_Coordinates(ft_face, mmvar->num_axis, coords.data());

        for (size_t i = 0; i < mmvar->num_axis; ++i) {
            FT_Var_Axis* axis = &mmvar->axis[i];
            char tag[5];
            for (int i = 0; i < 4; ++i) {
                tag[i] = (axis->tag >> (3 - i) * 8) & 0xff;
            }
            tag[4] = 0;
            axes[axis->name] =  OTVarAxis(FTFixedToDouble(axis->minimum),
                                          FTFixedToDouble(axis->def),
                                          FTFixedToDouble(axis->maximum),
                                          FTFixedToDouble(coords[i]),
                                          i, tag);
        }

        // for (auto a: axes) {
        //     std::cout << " " << a.first
        //               << " min: " << a.second.minimum
        //               << " max: " << a.second.maximum
        //               << " set: " << a.second.set_val << std::endl;
        // }

    }

#endif /* FREETYPE Version */
}


// Harfbuzz now as API for named variations (Version 2.2, Nov 29 2018).
// Make a list of all Named instances with axis values.
void readOpenTypeFvarNamed(const FT_Face ft_face,
                           std::map<Glib::ustring, OTVarInstance>& named) {

#if FREETYPE_MAJOR *10000 + FREETYPE_MINOR*100 + FREETYPE_MICRO >= 20701
    FT_MM_Var* mmvar = nullptr;
    FT_Multi_Master mmtype;
    if (FT_HAS_MULTIPLE_MASTERS( ft_face )    &&    // Font has variables
        FT_Get_MM_Var( ft_face, &mmvar) == 0   &&    // We found the data
        FT_Get_Multi_Master( ft_face, &mmtype) !=0) {  // It's not an Adobe MM font

        std::cout << "  Multiple Masters: variables: " << mmvar->num_axis
                  << "  named styles: " << mmvar->num_namedstyles << std::endl;

    //     const FT_UInt numNames = FT_Get_Sfnt_Name_Count(ft_face);
    //     std::cout << "  number of names: " << numNames << std::endl;
    //     FT_SfntName ft_name;
    //     for (FT_UInt i = 0; i < numNames; ++i) {

    //         if (FT_Get_Sfnt_Name(ft_face, i, &ft_name) != 0) {
    //             continue;
    //         }

    //         Glib::ustring name;
    //         for (size_t j = 0; j < ft_name.string_len; ++j) {
    //             name += (char)ft_name.string[j];
    //         }
    //         std::cout << " " << i << ": " << name << std::endl;
    //     }

    }

#endif /* FREETYPE Version */
}

#define HB_OT_TAG_SVG HB_TAG('S','V','G',' ')

// Get SVG glyphs out of an OpenType font.
void readOpenTypeSVGTable(hb_font_t* hb_font,
                          std::map<unsigned int, SVGGlyphEntry>& glyphs,
                          std::map<int, std::string>& svgs) {

    hb_face_t* hb_face = hb_font_get_face (hb_font);

#if 0
    // Test use of hb_ot_color_glyph_reference_svg()
    for (int i=0; i < hb_face_get_glyph_count(hb_face); ++i) {
        hb_blob_t* svg_glyph_blob = hb_ot_color_glyph_reference_svg(hb_face, i);
        if (svg_glyph_blob) {
            // Note: The blob can contain glyphs after the desired glyph. One must use the 'length' argument to truncate the blob.
            unsigned int length = 0;
            const char* data = hb_blob_get_data(svg_glyph_blob, &length);
            std::cout << "readOpenTypeSVGTable: " << font_name(hb_face) << " " << i <<  " length: " << length << std::endl;
            if (data) {
                std::ofstream output;
                std::string filename = font_directory(hb_face) + "/glyph_x_" + std::to_string(i) + ".svg";
                output.open (filename);
                for (unsigned int j = 0; j < length; j++) {
                    output << data[j];
                }
                output.close();
            }
            hb_blob_destroy(svg_glyph_blob);
        }
    }
#endif

    // Harfbuzz has some support for SVG fonts but it is not exposed until version 2.1 (Oct 30, 2018).
    // And, it turns out it is not very useful as it just returns the SVG that contains the glyph without
    // processing picking the glyph out of the SVG which can contain hundreds or thousands of glyphs.
    // We do it the hard way!
    hb_blob_t *hb_blob = hb_face_reference_table (hb_face, HB_OT_TAG_SVG);

    if (!hb_blob) {
        // No SVG table in font!
        return;
    }

    unsigned int svg_length = hb_blob_get_length (hb_blob);
    if (svg_length == 0) {
        // No SVG glyphs in table!
        hb_blob_destroy(hb_blob);
        return;
    }

    const char* data = hb_blob_get_data(hb_blob, &svg_length);
    hb_blob_destroy(hb_blob);
    if (!data) {
        std::cerr << "readOpenTypeSVGTable: Failed to get data! " << std::endl;
        return;
    }

    // OpenType fonts use Big Endian
    uint32_t offset  = ((data[2] & 0xff) << 24) + ((data[3] & 0xff) << 16) + ((data[4] & 0xff) << 8) + (data[5] & 0xff);

    // std::cout << "Offset: "  << offset << std::endl;
    // Bytes 6-9 are reserved.

    uint16_t entries = ((data[offset] & 0xff) << 8) + (data[offset+1] & 0xff);
    // std::cout << "Number of entries: " << entries << std::endl;

    for (int entry = 0; entry < entries; ++entry) {
        uint32_t base = offset + 2 + entry * 12;

        uint16_t startGlyphID = ((data[base  ] & 0xff) <<  8) + (data[base+1] & 0xff);
        uint16_t endGlyphID   = ((data[base+2] & 0xff) <<  8) + (data[base+3] & 0xff);
        uint32_t offsetGlyph  = ((data[base+4] & 0xff) << 24) + ((data[base+5] & 0xff) << 16) +((data[base+6]  & 0xff) << 8) + (data[base+7]  & 0xff);
        uint32_t lengthGlyph  = ((data[base+8] & 0xff) << 24) + ((data[base+9] & 0xff) << 16) +((data[base+10] & 0xff) << 8) + (data[base+11] & 0xff);

        // std::cout << "Entry " << entry << ": Start: " << startGlyphID << "  End: " << endGlyphID
        //           << "  Offset: " << offsetGlyph << " Length: " << lengthGlyph << std::endl;

        std::string svg;

        // static cast is needed as hb_blob_get_length returns char but we are comparing to a value greater than allowed by char.
        if (lengthGlyph > 1 && //
            static_cast<unsigned char>(data[offset + offsetGlyph + 0]) == 0x1f &&
            static_cast<unsigned char>(data[offset + offsetGlyph + 1]) == 0x8b) {
            // Glyph is gzipped

            std::vector<unsigned char> buffer;
            for (unsigned int c = offsetGlyph; c < offsetGlyph + lengthGlyph; ++c) {
                buffer.push_back(data[offset + c]);
            }

            Inkscape::IO::BufferInputStream zipped(buffer);
            Inkscape::IO::GzipInputStream gzin(zipped);
            for (int character = gzin.get(); character != -1; character = gzin.get()) {
               svg+= (char)character;
            }

        } else {
            // Glyph is not compressed

            for (unsigned int c = offsetGlyph; c < offsetGlyph + lengthGlyph; ++c) {
                svg += (unsigned char) data[offset + c];
            }
        }

        // Make all glyphs hidden (for SVG files with multiple glyphs, we'll need to pickout just one).
        static auto regex = Glib::Regex::create("(id=\"\\s*glyph\\d+\\s*\")", Glib::Regex::CompileFlags::OPTIMIZE);
        svg = regex->replace(Glib::UStringView(svg), 0, "\\1 visibility=\"hidden\"", static_cast<Glib::Regex::MatchFlags>(0));

#ifdef DEBUG_OPENTYPEUTIL
        std::ofstream output;
        std::string filename = font_directory(hb_face) + "/glyph_" + std::to_string(startGlyphID);
        if (startGlyphID != endGlyphID) {
            filename += "_" + std::to_string(endGlyphID);
        }
        filename += ".svg";
        output.open (filename);
        output << svg;
        output.close();

        std::cout << "Glyphs: " << startGlyphID << "-" << endGlyphID << " ";
        auto length = svg.length();
        if (length < 500) {
            std::cout << svg << std::endl;
        } else {
            std::cout << "svg length: " << length << std::endl;
        }
#endif
        svgs[entry] = svg;

        for (unsigned int i = startGlyphID; i < endGlyphID+1; ++i) {
            glyphs[i].entry_index = entry;
        }

    }
}

// Get PNG glyphs out of an OpenType font.
void readOpenTypePNG(hb_font_t* hb_font,
                     std::vector<Glib::RefPtr<Gdk::Pixbuf>>& pixbufs) {

    hb_face_t* hb_face = hb_font_get_face (hb_font);

    if (!hb_ot_color_has_png(hb_face)) {
        std::cout << "No PNG in font face!" << std::endl;
        return;
    }

    auto glyph_count = hb_face_get_glyph_count(hb_face);

    hb_set_t* hb_unicode_set = hb_set_create();
    hb_face_collect_unicodes(hb_face, hb_unicode_set);

    hb_map_t* hb_unicode_to_glyph_map = hb_map_create();
    hb_face_collect_nominal_glyph_mapping(hb_face, hb_unicode_to_glyph_map, hb_unicode_set);

    std::map<hb_codepoint_t, hb_codepoint_t> glyph_to_unicode_map;
    hb_codepoint_t current_unicode = HB_SET_VALUE_INVALID;
    while (hb_set_next(hb_unicode_set, &current_unicode)) {
        glyph_to_unicode_map[hb_map_get(hb_unicode_to_glyph_map, current_unicode)] = current_unicode; 
    }

    std::cout << "readOpenTypePNG: glyph count: " << glyph_count << std::endl;

    for (unsigned int i = 0; i < glyph_count; ++i) {
        hb_blob_t* png_data = hb_ot_color_glyph_reference_png(hb_font, i);

//        hb_codepoint_t unicode = hb_map_get(hb_glyph_map, i);
        auto unicode = glyph_to_unicode_map[i];
        if (i < 20) {
            std::cout << " glyph: " << i << " unicode: " << unicode << std::endl;
        }


        unsigned int length = 0;
        auto data = hb_blob_get_data(png_data, &length);
        hb_blob_destroy(png_data);
        if (!data) {
            std::cout << " No data! " << i << std::endl;
            continue;
        }

#ifdef DEBUG_OPENTYPEUTIL
        std::ofstream png_stream;
        std::string filename = font_directory(hb_face) + "/glyph_" + std::to_string(unicode) + ".png";
        png_stream.open(filename);
        if (!png_stream) {
            std::cerr << "Failed to open PNG stream" << std::endl;
            break;
        }
        for (unsigned int j = 0; j < length; ++j) {
            png_stream << data[j];
        }
        png_stream.close();
#endif

        GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
        if (gdk_pixbuf_loader_write(loader, (guchar*)data, length, nullptr)) {
            gdk_pixbuf_loader_close(loader, nullptr);
            GdkPixbuf *pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
        }
        g_object_unref(loader);

        // auto loader2 = Gdk::PixbufLoader::create();
        // try {
        //     loader2->write((guint8*)data, length);
        // }
        // catch (const Glib::FileError &e)
        // {
        //     std::cout << "readOpenTypePNG: " << e.what() << std::endl;
        // }
        // catch (const Gdk::PixbufError &e)
        // {
        //     std::cout << "readOpenTypePNG: " << e.what() << std::endl;
        // }
        // loader2->close();
        // auto pixbuf = loader2->get_pixbuf();

        // pixbufs.emplace_back(pixbuf);
    }
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
