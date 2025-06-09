// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Authors:
 *   Rafał Siejakowski <rs@rs-math.net>
 *
 * @copyright
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 *
 * @file @brief Unit tests SPDocument::importDefs()
 */

#include <algorithm>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "document.h"
#include "inkscape.h"
#include "object/sp-defs.h"
#include "object/sp-object.h"
#include "object/sp-path.h"
#include "object/sp-use.h"
#include "style.h"

using namespace ::testing;

namespace {

/// Create a span not including the null-terminator.
constexpr std::span<char const> static_string_to_span(char const *compile_time_string)
{
    return {compile_time_string, compile_time_string + std::char_traits<char>::length(compile_time_string)};
}
} // namespace

struct ImportDefsTest : public Test
{
    ImportDefsTest()
    {
        if (!Inkscape::Application::exists()) {
            Inkscape::Application::create(false);
        }
    }

    static constexpr auto OUR_DOC = static_string_to_span(
        R"EOD(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
    <svg version="1.1">
        <defs>
            <circle cx="0" cy="0" r="10" id="our-circle" />
            <rect x="2" y="5" width="5" height="6" id="our-rect" />
        </defs>
    </svg>)EOD");
};

/// Check that external doc's defs are simply appended to ours when there are no ID clashes
TEST_F(ImportDefsTest, NoClashCase)
{
    auto our_doc = SPDocument::createNewDocFromMem(OUR_DOC, true);

    static constexpr auto EXTERNAL_DOC_NO_CLASH = static_string_to_span(
        R"EOD(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
    <svg version="1.1">
        <defs>
            <circle cx="3" cy="5" r="40" id="their-circle" />
            <rect x="2" y="5" width="10" height="12" id="their-rect" />
        </defs>
    </svg>)EOD");
    auto external_doc = SPDocument::createNewDocFromMem(EXTERNAL_DOC_NO_CLASH, true);

    our_doc->importDefs(*external_doc);

    auto const *defs_after_import = our_doc->getDefs();
    ASSERT_TRUE(defs_after_import);

    auto const &defs_children = defs_after_import->children;
    EXPECT_EQ(defs_children.size(), 4U);

    std::set<std::string> def_ids;
    std::transform(defs_children.begin(), defs_children.end(), std::inserter(def_ids, def_ids.begin()),
                   std::mem_fn(&SPObject::getId));
    std::set<std::string> const expected_ids{"our-circle", "our-rect", "their-circle", "their-rect"};
    EXPECT_EQ(def_ids, expected_ids);
}

/// Check that ID clashes are successfully resolved
TEST_F(ImportDefsTest, ClashResolution)
{
    auto our_doc = SPDocument::createNewDocFromMem(OUR_DOC, true);

    static constexpr auto EXTERNAL_DOC_CLASH = static_string_to_span(
        R"EOD(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
    <svg version="1.1">
        <defs>
            <path d="M 0,0 L 2,4" id="our-circle" /><!-- Clashes with our circle -->
            <rect x="2" y="5" width="10" height="12" id="some-rect" />
        </defs>
    </svg>)EOD");
    auto external_doc = SPDocument::createNewDocFromMem(EXTERNAL_DOC_CLASH, true);

    our_doc->importDefs(*external_doc);

    auto const *defs_after_import = our_doc->getDefs();
    ASSERT_TRUE(defs_after_import);

    auto const &defs_children = defs_after_import->children;
    EXPECT_EQ(defs_children.size(), 4U);

    std::set<std::string> def_ids;
    std::transform(defs_children.begin(), defs_children.end(), std::inserter(def_ids, def_ids.begin()),
                   std::mem_fn(&SPObject::getId));

    // We expect that the IDs haven't changed except to resolve the conflicts
    std::set<std::string> const expected_ids{"our-circle", "our-rect", "some-rect"};

    // Check that the <path> with a clashing ID has a new ID
    std::set<std::string> new_ids;
    std::set_difference(def_ids.cbegin(), def_ids.cend(), expected_ids.begin(), expected_ids.end(),
                        std::inserter(new_ids, new_ids.begin()));
    ASSERT_EQ(new_ids.size(), 1U);

    auto const *path = our_doc->getObjectById(new_ids.begin()->c_str());
    ASSERT_TRUE(path);
    EXPECT_TRUE(is<SPPath>(path));
}

/// Check that ID clash resolution triggers an update of hrefs in source document.
TEST_F(ImportDefsTest, ClashResolutionRelinks)
{
    auto our_doc = SPDocument::createNewDocFromMem(OUR_DOC, true);

    static constexpr auto EXTERNAL_DOC_REFERENCE_TO_CLASH = static_string_to_span(
        R"EOD(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
    <svg version="1.1" xmlns:xlink="http://www.w3.org/1999/xlink">
        <defs>
            <circle cx="0" cy="0" r="10" id="our-circle" /><!-- Clashes with our circle -->
            <rect x="2" y="5" width="5" height="6" id="our-rect" />
        </defs>
        <use xlink:href="#our-circle" x="42" y="69" id="use-element" /><!-- Ref to clashing element -->
    </svg>)EOD");
    auto external_doc = SPDocument::createNewDocFromMem(EXTERNAL_DOC_REFERENCE_TO_CLASH, true);

    our_doc->importDefs(*external_doc);

    auto const *defs_after_import = our_doc->getDefs();
    ASSERT_TRUE(defs_after_import);

    EXPECT_EQ(defs_after_import->children.size(), 4U);

    // Check that the use element is still correctly linking to something
    auto const *referencing_element = external_doc->getObjectById("use-element");
    ASSERT_TRUE(referencing_element);
    auto const *use_element = cast<SPUse>(referencing_element);
    ASSERT_TRUE(use_element);

    std::string const new_href = use_element->href;
    auto const *referenced_element = use_element->trueOriginal();
    ASSERT_TRUE(referenced_element);

    // Check that we have the new element with the same ID
    auto const expected_href = std::string("#") + referenced_element->getId();
    EXPECT_EQ(expected_href, new_href);
    EXPECT_TRUE(our_doc->getObjectById(referenced_element->getId()));
    EXPECT_TRUE(our_doc->getObjectByHref(expected_href));
}

/// Check that an identical swatch in an external document is reused
TEST_F(ImportDefsTest, ReuseSwatch)
{
    static constexpr auto DOC_WITH_SWATCH = static_string_to_span(
        R"EOD(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
    <svg version="1.1" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape">
        <defs>
            <linearGradient id="swatch" inkscape:swatch="solid">
                <stop style="stop-color: #ff5555; stop-opacity: 1;" offset="0"/>
            </linearGradient>
        </defs>
        <rect x="2" y="5" width="5" height="6" id="our-rect" style="fill: url(#swatch);"/>
    </svg>)EOD");
    auto our_doc = SPDocument::createNewDocFromMem(DOC_WITH_SWATCH, true);

    static constexpr auto DOC_WITH_SAME_SWATCH = static_string_to_span(
        R"EOD(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
    <svg version="1.1" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape">
        <defs>
            <linearGradient id="same-swatch" inkscape:swatch="solid">
                <stop style="stop-color: #ff5555; stop-opacity: 1;" offset="0"/>
            </linearGradient>
        </defs>
        <rect x="2" y="5" width="5" height="6" id="their-rect" style="fill: url(#same-swatch);"/>
    </svg>)EOD");
    auto external_doc = SPDocument::createNewDocFromMem(DOC_WITH_SAME_SWATCH, true);

    auto const *defs_before_import = our_doc->getDefs();
    ASSERT_TRUE(defs_before_import);
    EXPECT_EQ(defs_before_import->children.size(), 1U);

    our_doc->importDefs(*external_doc);

    auto const *defs_after_import = our_doc->getDefs();
    ASSERT_TRUE(defs_after_import);

    // Expect that the swatch is not duplicated
    EXPECT_EQ(defs_after_import->children.size(), 1U);
    EXPECT_STREQ(defs_after_import->firstChild()->getId(), "swatch");

    // Check that other document's rect is relinked to refer to "#swatch" as fill
    auto const *external_rect = external_doc->getObjectById("their-rect");
    ASSERT_TRUE(external_rect);
    ASSERT_TRUE(external_rect->style);
    ASSERT_TRUE(external_rect->style->fill.href);

    auto const *uri = external_rect->style->fill.href->getURI();
    ASSERT_TRUE(uri);

    EXPECT_STREQ(uri->getFragment(), "swatch"); // Was "same-swatch"
}

/// Check that a swatch is still imported if it is different
TEST_F(ImportDefsTest, CopySwatchIfDifferent)
{
    static constexpr auto DOC_WITH_SWATCH = static_string_to_span(
        R"EOD(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
    <svg version="1.1" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape">
        <defs>
            <linearGradient id="swatch" inkscape:swatch="solid">
                <stop style="stop-color: #ff5555; stop-opacity: 1;" offset="0"/>
            </linearGradient>
        </defs>
        <rect x="2" y="5" width="5" height="6" id="our-rect" style="fill: url(#swatch);"/>
    </svg>)EOD");
    auto our_doc = SPDocument::createNewDocFromMem(DOC_WITH_SWATCH, true);

    static constexpr auto DOC_WITH_DIFFERENT_SWATCH = static_string_to_span(
        R"EOD(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
    <svg version="1.1" xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape">
        <defs>
            <linearGradient id="different-color-swatch" inkscape:swatch="solid">
                <stop style="stop-color: #ff5556; stop-opacity: 1;" offset="0"/>
            </linearGradient>
        </defs>
        <rect x="2" y="5" width="5" height="6" id="their-rect" style="fill: url(#different-color-swatch);"/>
    </svg>)EOD");
    auto external_doc = SPDocument::createNewDocFromMem(DOC_WITH_DIFFERENT_SWATCH, true);

    auto const *defs_before_import = our_doc->getDefs();
    ASSERT_TRUE(defs_before_import);
    EXPECT_EQ(defs_before_import->children.size(), 1U);

    our_doc->importDefs(*external_doc);

    auto const *defs_after_import = our_doc->getDefs();
    ASSERT_TRUE(defs_after_import);

    // Expect that we have two swatches now
    auto const &defs_children = defs_after_import->children;
    EXPECT_EQ(defs_children.size(), 2U);

    std::set<std::string> const expected_swatch_ids{"swatch", "different-color-swatch"};
    std::set<std::string> actual_swatch_ids;
    std::transform(defs_children.begin(), defs_children.end(),
                   std::inserter(actual_swatch_ids, actual_swatch_ids.begin()), std::mem_fn(&SPObject::getId));

    EXPECT_EQ(actual_swatch_ids, expected_swatch_ids);
}
